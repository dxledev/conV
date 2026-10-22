/**
 * @file theme_source.cpp
 * @brief Import and resolve palettes from QML color files and JSON palettes.
 *
 * Supported color forms: "#RRGGBB", "#AARRGGBB" (Qt order, alpha first) and
 * Qt.rgba(r, g, b, a) with plain numbers or divisions. Only the background keeps
 * opacity; every derived color is opaque. A directory source is searched for a
 * known palette file each time, so retargeting a symlinked directory is picked up.
 */
#include "theme_source.h"
#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>

namespace conv {
namespace {

using Palette = QMap<QString, QColor>;

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

QColor parseHex(QString value) {
    static const QRegularExpression bare("^[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$");
    if (bare.match(value).hasMatch())
        value.prepend('#');
    const auto color = QColor::fromString(value);
    if (!color.isValid())
        fail("Unsupported palette color: " + value);
    return color;
}

double channel(const QString &expression) {
    const auto parts = expression.trimmed().split('/');
    if (parts.size() > 2)
        fail("Unsupported Qt.rgba channel: " + expression);
    bool valid = false;
    double value = parts[0].trimmed().toDouble(&valid);
    if (!valid)
        fail("Invalid Qt.rgba channel: " + expression);
    if (parts.size() == 2) {
        bool denominatorValid = false;
        const double denominator = parts[1].trimmed().toDouble(&denominatorValid);
        if (!denominatorValid || denominator <= 0)
            fail("Invalid Qt.rgba divisor: " + expression);
        value /= denominator;
    }
    if (!std::isfinite(value) || value < 0 || value > 1)
        fail("Qt.rgba channels must be between 0 and 1");
    return value;
}

Palette qmlPalette(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        fail("Cannot read palette: " + path);
    QString contents = QString::fromUtf8(file.readAll());
    contents.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    contents.remove(QRegularExpression("//[^\\n]*"));
    static const QRegularExpression pattern(
        R"(property\s+color\s+(\w+)\s*:\s*(?:["'](#[\da-fA-F]{6}(?:[\da-fA-F]{2})?)["']|Qt\.rgba\(([^)]*)\)))");
    Palette result;
    auto matches = pattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (!match.captured(2).isEmpty()) {
            result.insert(match.captured(1), parseHex(match.captured(2)));
            continue;
        }
        const auto channels = match.captured(3).split(',');
        if (channels.size() != 4)
            fail("Qt.rgba needs four channels");
        result.insert(match.captured(1), QColor::fromRgbF(channel(channels[0]), channel(channels[1]),
                                                          channel(channels[2]), channel(channels[3])));
    }
    return result;
}

Palette jsonPalette(QJsonObject data) {
    if (data.value("colours").isObject())
        data = data.value("colours").toObject();
    else if (data.value("colors").isObject())
        data = data.value("colors").toObject();
    else if (data.value("dark").isObject() || data.value("light").isObject()) {
        const auto mode = data.value("mode").toString("dark");
        data = data.value(mode).toObject();
    }
    Palette result;
    static const QRegularExpression color("^#?[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$");
    for (auto value = data.constBegin(); value != data.constEnd(); ++value) {
        if (value.value().isString() && color.match(value.value().toString()).hasMatch())
            result.insert(value.key(), parseHex(value.value().toString()));
    }
    return result;
}

QColor pick(const Palette &palette, const QStringList &keys, QColor fallback = {}) {
    for (const auto &key : keys) {
        if (palette.contains(key))
            return palette.value(key);
    }
    if (!fallback.isValid())
        fail("Palette is missing " + keys.join(" / "));
    return fallback;
}

QColor mix(const QColor &base, const QColor &tint, double amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                            base.greenF() * (1 - amount) + tint.greenF() * amount,
                            base.blueF() * (1 - amount) + tint.blueF() * amount);
}

Theme mapPalette(const Palette &palette, const QString &name) {
    // Opacity is carried by the background only; mixes start from its opaque RGB.
    const auto background = pick(palette, {"background", "base", "m3background", "mSurface", "surface"});
    const auto solid = opaque(background);
    auto rgb = [&](const QStringList &keys, QColor fallback = {}) { return opaque(pick(palette, keys, fallback)); };
    const auto text = rgb({"foreground", "onBackground", "on_background", "text", "mOnSurface", "onSurface", "on_surface"});
    const auto accent = rgb({"primary", "accent", "mPrimary", "m3primary"});
    const auto surface = rgb({"surface_container_low", "surfaceContainerLow", "backgroundAlt", "mantle"}, mix(solid, text, .035));
    const auto alternate = rgb({"surface_container", "surfaceContainer", "backgroundGray", "surface0", "mSurfaceVariant"}, mix(solid, text, .07));
    const auto outline = rgb({"outline", "border", "mOutline"}, accent);
    const QColor onAccent = luminance(accent) > .4 ? QColor("#151515") : QColor("#ffffff");
    const Theme theme{name, {
        {"background", background}, {"surface", surface}, {"surface_alt", alternate}, {"text", text},
        {"muted", rgb({"foregroundInactive", "on_surface_variant", "onSurfaceVariant", "mOnSurfaceVariant", "muted"}, mix(solid, text, .65))},
        {"accent", accent}, {"accent_text", rgb({"on_primary", "onPrimary", "mOnPrimary"}, onAccent)},
        {"border", mix(surface, outline, .35)}, {"line", mix(surface, outline, .18)},
        {"hover", mix(surface, accent, .09)}, {"active", mix(surface, accent, .24)},
    }, {}, {}};
    // Quantize through JSON so alpha and RGB are stable across save/reload cycles.
    return parseTheme(theme.toJson());
}

QString expandedPath(QString path) {
    if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    return QFileInfo(path).absoluteFilePath();
}

QString resolveSource(const QString &requested) {
    const auto path = expandedPath(requested);
    if (!QFileInfo(path).isDir())
        return path;
    for (const char *candidate : {"Colors.qml", "palette.json", "colors.json", "noctalia.json"}) {
        const auto file = path + "/" + candidate;
        if (QFileInfo::exists(file))
            return file;
    }
    fail("No palette file found in " + path);
}

}

Theme importPalette(const QString &path, const QString &name) {
    const QString paletteName = name.isEmpty() ? QFileInfo(path).completeBaseName() : name;
    if (path.endsWith(".qml", Qt::CaseInsensitive))
        return mapPalette(qmlPalette(path), paletteName);
    const auto data = readJson(path);
    if (data.value("version").toInt() == 1 && data.value("colors").isObject() && data.value("colors").toObject().contains("active"))
        return parseTheme(data);
    return mapPalette(jsonPalette(data), paletteName);
}

Theme followTheme(const QString &path) {
    if (path.isEmpty())
        fail("Expected a palette file or directory to follow");
    const auto resolved = resolveSource(path);
    const auto directory = QFileInfo(QFileInfo(resolved).canonicalFilePath()).dir().dirName();
    auto theme = importPalette(resolved, directory.isEmpty() ? QFileInfo(resolved).completeBaseName() : directory);
    theme.sourceKind = "file";
    // Keep the caller's logical path so a retargeted symlink is followed.
    theme.sourcePath = expandedPath(path);
    return theme;
}

Theme refreshSource(const Theme &theme) {
    if (theme.sourceKind.isEmpty())
        return theme;
    return followTheme(theme.sourcePath);
}

QStringList sourceWatchPaths(const Theme &theme) {
    if (theme.sourcePath.isEmpty())
        return {};
    QStringList paths;
    auto collect = [&paths](QString path) {
        while (!path.isEmpty()) {
            if (QFileInfo::exists(path))
                paths.append(path);
            const auto parent = QFileInfo(path).dir().absolutePath();
            if (parent == path)
                break;
            path = parent;
        }
    };
    // The logical path, its target, and the palette file inside each are watched so
    // symlink retargeting, atomic replacement, and in-place edits all reload.
    for (const QString &root : {theme.sourcePath, QFileInfo(theme.sourcePath).canonicalFilePath()}) {
        if (root.isEmpty())
            continue;
        collect(root);
        if (QFileInfo(root).isDir()) {
            try {
                collect(resolveSource(root));
            } catch (const std::exception &) {
            }
        }
    }
    paths.removeDuplicates();
    return paths;
}

}
