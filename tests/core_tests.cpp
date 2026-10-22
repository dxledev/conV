#include "core/converter.h"
#include "core/icon.h"
#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace conv;

namespace {

QString writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qPrintable(path));
    file.write(bytes);
    return path;
}

QByteArray qml(const QString &background) {
    return ("pragma Singleton\nSingleton {\n"
            "    readonly property color background: " + background + "\n"
            "    readonly property color foreground: \"#f4b999\"\n"
            "    readonly property color primary: \"#9279aa\"\n}\n").toUtf8();
}

}

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void akaneHexKeepsRgbAndAlpha() {
        QTemporaryDir dir;
        const auto theme = importPalette(writeFile(dir.filePath("Colors.qml"), qml("\"#e00e1e36\"")));
        const auto bg = theme.color("background");
        QCOMPARE(opaque(bg).name(), QString("#0e1e36"));
        QCOMPARE(bg.alpha(), 0xe0);
        QCOMPARE(theme.hex("background"), QString("#e00e1e36"));
        QCOMPARE(theme.color("surface").alpha(), 255);
    }

    void qtRgbaKeepsRgbAndAlpha() {
        QTemporaryDir dir;
        const auto theme = importPalette(writeFile(dir.filePath("Colors.qml"), qml("Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)")));
        QCOMPARE(opaque(theme.color("background")).name(), QString("#0e1e36"));
        QCOMPARE(theme.color("background").alpha(), 224);
    }

    void opaqueFallbackNeverShiftsChannels() {
        const QColor translucent = QColor::fromString("#e00e1e36");
        QCOMPARE(opaque(translucent).red(), 0x0e);
        QCOMPARE(opaque(translucent).green(), 0x1e);
        QCOMPARE(opaque(translucent).blue(), 0x36);
    }

    void jsonRoundTripKeepsAlpha() {
        auto theme = setColors(presetTheme("phosphor"), {"background=#e00e1e36"});
        const auto again = parseTheme(theme.toJson());
        QCOMPARE(again, theme);
        QCOMPARE(again.color("background").alpha(), 0xe0);
    }

    void followedAlphaThemeSettlesWithoutRewrite() {
        QTemporaryDir dir;
        writeFile(dir.filePath("Colors.qml"), qml("Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)"));
        const auto output = dir.filePath("theme.json");
        auto first = followTheme(dir.path());
        writeJson(output, first.toJson());
        // A reload resolves the source again; it must equal what was saved.
        const auto saved = loadTheme(output);
        QCOMPARE(refreshSource(saved), saved);
        QVERIFY(!writeTextIfChanged(output, QJsonDocument(refreshSource(saved).toJson()).toJson()));
    }

    void rejectsMalformedColors() {
        QVERIFY_EXCEPTION_THROWN(setColors(presetTheme("phosphor"), {"accent=#12345"}), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(setColors(presetTheme("phosphor"), {"nope=#123456"}), std::invalid_argument);
    }

    void otherKeysDropAlpha() {
        const auto theme = setColors(presetTheme("phosphor"), {"accent=#80ff0000"});
        QCOMPARE(theme.color("accent").alpha(), 255);
        QCOMPARE(theme.color("accent").name(), QString("#ff0000"));
    }

    void directoryFollowFindsPalette() {
        QTemporaryDir dir;
        writeFile(dir.filePath("palette.json"), QByteArray("{\"colors\":{\"background\":\"#102030\",\"foreground\":\"#eeeeee\",\"primary\":\"#33aa77\"}}"));
        const auto theme = followTheme(dir.path());
        QCOMPARE(theme.color("accent").name(), QString("#33aa77"));
        QCOMPARE(theme.sourcePath, dir.path());
    }

    void iconUsesThemeColors() {
        const auto svg = QString::fromUtf8(iconSvg(presetTheme("amber")));
        QVERIFY(svg.contains("#ffb000"));
        QVERIFY(!svg.contains("Gradient"));
    }

    void outputNeverOverwritesInput() {
        QTemporaryDir dir;
        const auto input = writeFile(dir.filePath("a.png"), "x");
        QCOMPARE(outputPath(input, {}, "png"), dir.filePath("a-converted.png"));
        QCOMPARE(outputPath(input, {}, "jpg"), dir.filePath("a.jpg"));
        writeFile(dir.filePath("a.jpg"), "y");
        QCOMPARE(uniquePath(dir.filePath("a.jpg")), dir.filePath("a (1).jpg"));
    }

    void commandsAreArgumentLists() {
        const auto &all = categories();
        const auto video = buildCommand(all[1], "/tmp/in put.mov", "/tmp/out.mp4", 100);
        QCOMPARE(video.program, QString("ffmpeg"));
        QVERIFY(video.arguments.contains("/tmp/in put.mov"));
        QVERIFY(video.arguments.contains("18"));
        const auto doc = buildCommand(all[3], "/tmp/a.md", "/tmp/a.html", 50);
        QVERIFY(doc.arguments.contains("-s"));
        const auto audio = buildCommand(all[2], "/tmp/a.flac", "/tmp/a.mp3", 100);
        QVERIFY(audio.arguments.contains("320k"));
    }

    void everyOfferedOutputConvertsWithRealTools_data() {
        QTest::addColumn<int>("category");
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("format");
        const QStringList inputs{"in.png", "in.mp4", "in.wav", "in.md"};
        for (int index = 0; index < categories().size(); ++index) {
            for (const auto &format : categories()[index].outputs)
                QTest::newRow(qPrintable(categories()[index].id + "-" + format)) << index << inputs[index] << format;
        }
    }

    void everyOfferedOutputConvertsWithRealTools() {
        QFETCH(int, category);
        QFETCH(QString, input);
        QFETCH(QString, format);
        const auto &item = categories()[category];
        if (categoryTool(item).isEmpty())
            QSKIP("tool not installed");
        QTemporaryDir dir;
        const auto source = dir.filePath(input);
        if (input.endsWith(".png")) {
            QImage image(64, 48, QImage::Format_ARGB32);
            image.fill(QColor(200, 30, 60, 128));
            QVERIFY(image.save(source));
        } else if (input.endsWith(".md")) {
            writeFile(source, "# Title\n\nSome *text* and a [link](https://example.com).\n");
        } else {
            QProcess make;
            const auto lavfi = input.endsWith(".mp4") ? QStringList{"-f", "lavfi", "-i", "testsrc=d=1:s=160x120", "-f", "lavfi", "-i", "sine=d=1", "-shortest"}
                                                      : QStringList{"-f", "lavfi", "-i", "sine=d=1"};
            make.start(findTool("ffmpeg"), QStringList{"-hide_banner", "-loglevel", "error", "-y"} + lavfi + QStringList{source});
            QVERIFY(make.waitForFinished(30000));
            QCOMPARE(make.exitCode(), 0);
        }
        const auto output = dir.filePath("out." + format);
        const auto command = buildCommand(item, source, output, 70);
        QProcess process;
        process.start(command.program, command.arguments);
        QVERIFY(process.waitForFinished(60000));
        QVERIFY2(process.exitCode() == 0, qPrintable(QString::fromLocal8Bit(process.readAllStandardError())));
        QVERIFY(QFileInfo(output).size() > 0);
    }

    void progressLines() {
        QCOMPARE(progressSeconds("out_time_us=2500000"), 2.5);
        QCOMPARE(progressSeconds("frame=12"), -1.0);
    }
};

QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
