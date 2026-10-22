/**
 * @file converter.cpp
 * @brief Format tables and external-tool command lines for each conversion tab.
 *
 * Commands are built as argument lists, never shell strings. Inputs are passed
 * after a "--"-safe prefix so file names beginning with a dash stay literal.
 */
#include "converter.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

namespace conv {

const QVector<Category> &categories() {
    static const QVector<Category> all{
        {"photo", "Photo", "magick",
         {"png", "jpg", "jpeg", "webp", "avif", "heic", "heif", "gif", "bmp", "tif", "tiff", "ico", "jxl", "svg"},
         {"png", "jpg", "webp", "avif", "gif", "bmp", "tiff", "ico", "jxl"}, true, "Quality"},
        {"video", "Video", "ffmpeg",
         {"mp4", "mkv", "webm", "mov", "avi", "flv", "wmv", "m4v", "mpg", "mpeg", "ts", "gif"},
         {"mp4", "mkv", "webm", "mov", "avi", "gif"}, true, "Quality"},
        {"audio", "Audio", "ffmpeg",
         {"mp3", "wav", "flac", "ogg", "opus", "m4a", "aac", "wma", "aiff", "mka"},
         {"mp3", "wav", "flac", "ogg", "opus", "m4a"}, true, "Bitrate"},
        {"document", "Docs", "pandoc",
         {"md", "markdown", "html", "htm", "docx", "odt", "epub", "rst", "tex", "txt", "org"},
         {"md", "html", "docx", "odt", "epub", "rst", "txt", "tex"}, false, {}},
    };
    return all;
}

QString findTool(const QString &name) {
    return QStandardPaths::findExecutable(name);
}

QString categoryTool(const Category &category) {
    if (category.id == "photo") {
        const auto magick = findTool("magick");
        return magick.isEmpty() ? findTool("convert") : magick;
    }
    return findTool(category.tool);
}

QString extensionOf(const QString &path) {
    return QFileInfo(path).suffix().toLower();
}

bool accepts(const Category &category, const QString &path) {
    return category.inputs.contains(extensionOf(path));
}

QString outputPath(const QString &input, const QString &directory, const QString &extension) {
    const QFileInfo info(input);
    const QString folder = directory.isEmpty() ? info.absolutePath() : QDir(directory).absolutePath();
    QString path = folder + "/" + info.completeBaseName() + "." + extension;
    if (QFileInfo(path).absoluteFilePath() == info.absoluteFilePath()
        || QFileInfo(path).canonicalFilePath() == info.canonicalFilePath())
        path = folder + "/" + info.completeBaseName() + "-converted." + extension;
    return path;
}

QString uniquePath(const QString &path) {
    if (!QFileInfo::exists(path))
        return path;
    const QFileInfo info(path);
    for (int index = 1; index < 10000; ++index) {
        const auto candidate = QString("%1/%2 (%3).%4").arg(info.absolutePath(), info.completeBaseName()).arg(index).arg(info.suffix());
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return path;
}

namespace {

int clampQuality(int quality) {
    return std::clamp(quality, 1, 100);
}

Command photo(const QString &program, const QString &input, const QString &output, int quality) {
    const auto format = extensionOf(output);
    // "[0]" keeps animated or multi-page inputs to their first frame unless writing a GIF.
    const bool animated = format == "gif";
    QStringList args{extensionOf(input) == "svg" ? QStringList{"-background", "none", "-density", "192"} : QStringList{}};
    args << (input + (animated ? "" : "[0]"));
    if (format == "jpg" || format == "bmp")
        args << "-background" << "white" << "-alpha" << "remove" << "-alpha" << "off";
    if (format == "ico")
        args << "-resize" << "256x256>";
    if (format == "jpg" || format == "webp" || format == "avif" || format == "jxl")
        args << "-quality" << QString::number(clampQuality(quality));
    args << output;
    // The legacy "convert" binary takes the same arguments as "magick".
    return {program, args};
}

Command video(const QString &input, const QString &output, int quality) {
    const auto format = extensionOf(output);
    const double q = clampQuality(quality) / 100.0;
    QStringList args{"-hide_banner", "-nostdin", "-y", "-progress", "pipe:1", "-nostats", "-i", input, "-map", "0:v:0", "-map", "0:a:0?"};
    if (format == "gif") {
        args = {"-hide_banner", "-nostdin", "-y", "-progress", "pipe:1", "-nostats", "-i", input,
                "-vf", "fps=15,scale='min(480,iw)':-2:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse", "-an"};
    } else if (format == "webm") {
        args << "-c:v" << "libvpx-vp9" << "-crf" << QString::number(qRound(45 - q * 20)) << "-b:v" << "0" << "-c:a" << "libopus";
    } else if (format == "avi") {
        args << "-c:v" << "mpeg4" << "-q:v" << QString::number(qRound(10 - q * 8)) << "-c:a" << "libmp3lame";
    } else {
        args << "-c:v" << "libx264" << "-preset" << "medium" << "-crf" << QString::number(qRound(35 - q * 17))
             << "-pix_fmt" << "yuv420p" << "-c:a" << "aac";
        if (format == "mp4" || format == "mov")
            args << "-movflags" << "+faststart";
    }
    args << output;
    return {"ffmpeg", args};
}

Command audio(const QString &input, const QString &output, int quality) {
    const auto format = extensionOf(output);
    const QString bitrate = QString::number(64 + qRound(clampQuality(quality) / 100.0 * 256)) + "k";
    QStringList args{"-hide_banner", "-nostdin", "-y", "-progress", "pipe:1", "-nostats", "-i", input, "-vn", "-map", "0:a:0"};
    if (format == "mp3")
        args << "-c:a" << "libmp3lame" << "-b:a" << bitrate;
    else if (format == "ogg")
        args << "-c:a" << "libvorbis" << "-q:a" << QString::number(qRound(clampQuality(quality) / 10.0));
    else if (format == "opus")
        args << "-c:a" << "libopus" << "-b:a" << bitrate;
    else if (format == "m4a")
        args << "-c:a" << "aac" << "-b:a" << bitrate;
    else if (format == "flac")
        args << "-c:a" << "flac";
    else
        args << "-c:a" << "pcm_s16le";
    args << output;
    return {"ffmpeg", args};
}

QString pandocFormat(const QString &extension, bool output) {
    static const QMap<QString, QString> names{
        {"md", "markdown"}, {"markdown", "markdown"}, {"html", "html"}, {"htm", "html"}, {"docx", "docx"},
        {"odt", "odt"}, {"epub", "epub"}, {"rst", "rst"}, {"tex", "latex"}, {"txt", "markdown"}, {"org", "org"}};
    if (output && extension == "txt")
        return "plain";
    if (output && extension == "md")
        return "gfm";
    return names.value(extension, "markdown");
}

Command document(const QString &input, const QString &output) {
    const auto format = extensionOf(output);
    QStringList args{"-f", pandocFormat(extensionOf(input), false), "-t", pandocFormat(format, true)};
    if (format == "html" || format == "tex")
        args << "-s";
    args << "-o" << output << input;
    return {"pandoc", args};
}

}

Command buildCommand(const Category &category, const QString &input, const QString &output, int quality) {
    // A leading "./" keeps names that start with "-" from being read as options.
    const auto safe = [](const QString &path) { return path.startsWith('-') ? "./" + path : path; };
    const auto in = safe(QFileInfo(input).absoluteFilePath());
    const auto out = safe(QFileInfo(output).absoluteFilePath());
    if (category.id == "photo")
        return photo(categoryTool(category), in, out, quality);
    if (category.id == "video")
        return video(in, out, quality);
    if (category.id == "audio")
        return audio(in, out, quality);
    return document(in, out);
}

double progressSeconds(const QString &line) {
    const auto parse = [&](const QString &prefix, double scale) -> double {
        if (!line.startsWith(prefix))
            return -1;
        bool valid = false;
        const double value = line.mid(prefix.size()).trimmed().toDouble(&valid);
        return valid && value >= 0 ? value / scale : -1;
    };
    const double microseconds = parse("out_time_us=", 1e6);
    return microseconds >= 0 ? microseconds : parse("out_time_ms=", 1e6);
}

}
