/**
 * @file main.cpp
 * @brief Entry point: theme commands run headless; otherwise the Wayland-first window opens.
 */
#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("conV");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("conV");

    try {
        QStringList arguments;
        for (int index = 1; index < argc; ++index)
            arguments.append(QString::fromLocal8Bit(argv[index]));
        const auto options = conv::parseOptions(arguments);
        if (options.help || options.version || options.isThemeCommand()) {
            QCoreApplication app(argc, argv);
            if (options.help) {
                conv::printHelp();
                return 0;
            }
            if (options.version) {
                QTextStream(stdout) << "conV 1.0.0\n";
                return 0;
            }
            return conv::runThemeCommand(options);
        }
        if (options.dryRun)
            throw std::invalid_argument("--dry-run applies to theme commands");
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        // This raster-only UI does not need a GPU driver or EGL buffers.
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication app(argc, argv);
        app.setStyle("Fusion");
        app.setDesktopFileName("io.github.conv");
        QStringList files;
        for (const auto &argument : options.arguments)
            files.append(QFileInfo(argument).absoluteFilePath());
        auto theme = conv::ensureTheme(options.configDirectory + "/theme.json");
        conv::Window window(options.configDirectory, theme, files);
        window.show();
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "conV: " << error.what() << '\n';
        return 1;
    }
}
