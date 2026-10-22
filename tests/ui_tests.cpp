#include "ui/convert_page.h"
#include "ui/modal.h"
#include "ui/window.h"
#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QApplication>
#include <cstdio>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace conv;

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void convertsAndNeverTouchesSource() {
        QTemporaryDir dir;
        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto source = dir.filePath("pic.png");
        QVERIFY(image.save(source));
        const auto before = QFileInfo(source).size();
        Window window(dir.filePath("cfg"), presetTheme("phosphor"), {});
        auto *photo = window.page(0);
        if (!photo->toolAvailable())
            QSKIP("ImageMagick not installed");
        photo->restore({{"format", "jpg"}});
        QCOMPARE(photo->addFiles({source, dir.filePath("missing.png"), "/etc/hostname"}), 1);
        photo->start();
        QTRY_VERIFY_WITH_TIMEOUT(!photo->busy() && QFileInfo::exists(dir.filePath("pic.jpg")), 15000);
        QCOMPARE(QFileInfo(source).size(), before);
        QVERIFY(QFileInfo(dir.filePath("pic.jpg")).size() > 0);
    }

    void sameFormatGetsSuffixInsteadOfOverwrite() {
        QTemporaryDir dir;
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto source = dir.filePath("pic.png");
        QVERIFY(image.save(source));
        Window window(dir.filePath("cfg"), presetTheme("phosphor"), {});
        auto *photo = window.page(0);
        if (!photo->toolAvailable())
            QSKIP("ImageMagick not installed");
        photo->restore({{"format", "png"}});
        photo->addFiles({source});
        photo->start();
        QTRY_VERIFY_WITH_TIMEOUT(!photo->busy() && QFileInfo::exists(dir.filePath("pic-converted.png")), 15000);
    }

    void collisionAsksInsideWindow() {
        QTemporaryDir dir;
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::blue);
        const auto source = dir.filePath("pic.png");
        QVERIFY(image.save(source));
        QFile existing(dir.filePath("pic.jpg"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("old");
        existing.close();
        Window window(dir.filePath("cfg"), presetTheme("phosphor"), {});
        auto *photo = window.page(0);
        if (!photo->toolAvailable())
            QSKIP("ImageMagick not installed");
        photo->restore({{"format", "jpg"}});
        int asked = 0;
        photo->showModal = [&](const ModalRequest &request, std::function<void(int)> done) {
            ++asked;
            QVERIFY(request.choices.size() == 3);
            done(0); // cancel
        };
        photo->addFiles({source});
        photo->start();
        QCOMPARE(asked, 1);
        QCOMPARE(QFileInfo(dir.filePath("pic.jpg")).size(), 3);
    }

    void themeChangeReloadsLive() {
        QTemporaryDir dir;
        const auto path = dir.filePath("cfg/theme.json");
        writeJson(path, presetTheme("phosphor").toJson());
        Window window(dir.filePath("cfg"), loadTheme(path), {});
        window.show();
        writeJson(path, setColors(presetTheme("phosphor"), {"accent=#ff00aa", "background=#cc0e1e36"}).toJson());
        QTRY_COMPARE_WITH_TIMEOUT(window.watcher()->theme().color("accent").name(), QString("#ff00aa"), 3000);
        QCOMPARE(window.watcher()->theme().color("background").alpha(), 0xcc);
    }

    void followsSelectorRetargetAndDynamicRewrites() {
        QTemporaryDir dir;
        const auto themes = dir.filePath("themes");
        auto palette = [&](const QString &name, const QString &primary, const QString &background) {
            QDir().mkpath(themes + "/" + name);
            QFile file(themes + "/" + name + "/Colors.qml");
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write(("Singleton {\n readonly property color background: " + background + "\n"
                        " readonly property color foreground: \"#eeeeee\"\n readonly property color primary: \"" + primary + "\"\n}\n").toUtf8());
        };
        palette("static-a", "#aa0000", "\"#e00e1e36\"");
        palette("dynamic-x", "#00aa00", "\"#101010\"");
        auto relink = [&](const QString &link, const QString &target) {
            QFile::remove(link + ".tmp");
            QVERIFY(QFile::link(target, link + ".tmp"));
            QCOMPARE(::rename(QFile::encodeName(link + ".tmp"), QFile::encodeName(link)), 0);
        };
        relink(themes + "/selector", "static-a");
        const auto config = dir.filePath("cfg");
        writeJson(config + "/theme.json", followTheme(themes + "/selector").toJson());
        Window window(config, loadTheme(config + "/theme.json"), {});
        QCOMPARE(window.watcher()->theme().color("accent").name(), QString("#aa0000"));
        QCOMPARE(window.watcher()->theme().color("background").alpha(), 0xe0);
        // Static -> dynamic: the selector is retargeted.
        relink(themes + "/selector", "dynamic-x");
        QTRY_COMPARE_WITH_TIMEOUT(window.watcher()->theme().color("accent").name(), QString("#00aa00"), 3000);
        QCOMPARE(window.watcher()->theme().color("background").alpha(), 255);
        // A new wallpaper regenerates the dynamic palette in place.
        palette("dynamic-x", "#0000bb", "\"#101010\"");
        QTRY_COMPARE_WITH_TIMEOUT(window.watcher()->theme().color("accent").name(), QString("#0000bb"), 3000);
        // The dynamic provider renders into a staging directory and swaps it into place.
        QDir().mkpath(themes + "/.stage");
        QFile::copy(themes + "/dynamic-x/Colors.qml", themes + "/.stage/Colors.qml");
        {
            QFile file(themes + "/.stage/Colors.qml");
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write("Singleton {\n readonly property color background: \"#101010\"\n readonly property color foreground: \"#eeeeee\"\n readonly property color primary: \"#cc00cc\"\n}\n");
        }
        QCOMPARE(::rename(QFile::encodeName(themes + "/dynamic-x").constData(), QFile::encodeName(themes + "/dynamic-x.old").constData()), 0);
        QCOMPARE(::rename(QFile::encodeName(themes + "/.stage").constData(), QFile::encodeName(themes + "/dynamic-x").constData()), 0);
        QTRY_COMPARE_WITH_TIMEOUT(window.watcher()->theme().color("accent").name(), QString("#cc00cc"), 3000);
        // The saved file follows, and settles instead of rewriting forever.
        QTest::qWait(300);
        QCOMPARE(loadTheme(config + "/theme.json").color("accent").name(), QString("#cc00cc"));
    }

    void emptyQueueIsAFocusableClickTarget() {
        QTemporaryDir dir;
        Window window(dir.filePath("cfg"), presetTheme("phosphor"), {});
        window.show();
        auto *photo = window.page(0);
        if (!photo->toolAvailable())
            QSKIP("ImageMagick not installed");
        QWidget *hint = nullptr;
        for (auto *widget : photo->findChildren<QWidget *>()) {
            if (widget->property("hintArea").toBool())
                hint = widget;
        }
        QVERIFY(hint);
        QVERIFY(hint->focusPolicy() == Qt::StrongFocus);
        QVERIFY(hint->cursor().shape() == Qt::PointingHandCursor);
    }

    void screenshots() {
        const auto output = qEnvironmentVariable("CONV_SHOT_DIR");
        if (output.isEmpty())
            QSKIP("set CONV_SHOT_DIR to capture screenshots");
        QDir().mkpath(output);
        QTemporaryDir dir;
        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::green);
        QStringList files;
        for (const auto &name : {"holiday-photo-final-v3.png", "scan.png", "a-very-long-file-name-that-should-be-elided-in-the-middle.png"}) {
            files << dir.filePath(name);
            image.save(files.last());
        }
        for (const auto &theme : {QString("phosphor"), QString("amber"), QString("paper")}) {
            Window window(dir.filePath("cfg-" + theme), presetTheme(theme), {});
            for (const QSize size : {QSize(1120, 780), QSize(800, 620)}) {
                window.resize(size);
                window.show();
                QTest::qWait(60);
                window.grab().save(QString("%1/%2-empty-%3.png").arg(output, theme).arg(size.width()));
            }
            window.resize(1120, 780);
            window.page(0)->addFiles(files);
            window.grab().save(QString("%1/%2-queue.png").arg(output, theme));
            window.page(1)->setProperty("x", 1);
        }
        Window window(dir.filePath("cfg-modal"), setColors(presetTheme("ice"), {"background=#e00e1e36"}), {});
        window.resize(1120, 780);
        window.show();
        window.page(0)->restore({{"format", "jpg"}});
        window.page(0)->addFiles(files);
        QFile existing(dir.filePath("holiday-photo-final-v3.jpg"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();
        window.page(0)->start();
        QTest::qWait(300);
        window.grab().save(output + "/modal.png");
        QTest::keyClick(&window, Qt::Key_Escape);
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
