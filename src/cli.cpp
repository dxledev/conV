/**
 * @file cli.cpp
 * @brief Command-line parsing and theme management subcommands.
 *
 * Theme mutations are validated before writing. --dry-run prints the proposed
 * JSON and touches nothing. A written theme also refreshes the generated icon.
 */
#include "cli.h"
#include "core/icon.h"
#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>
#include <stdexcept>

namespace conv {
namespace {

void requireCount(const QStringList &arguments, int count) {
    if (arguments.size() != count)
        throw std::invalid_argument("Invalid arguments. Run conV --help for usage.");
}

}

Options parseOptions(QStringList arguments) {
    Options result;
    bool positionalOnly = false;
    for (int index = 0; index < arguments.size(); ++index) {
        const QString argument = arguments[index];
        if (positionalOnly) {
            result.arguments.append(argument);
        } else if (argument == "--") {
            positionalOnly = true;
        } else if (argument == "--config-dir") {
            if (index + 1 >= arguments.size() || arguments[index + 1].startsWith("--"))
                throw std::invalid_argument("--config-dir requires a value");
            result.configDirectory = QDir(arguments[++index]).absolutePath();
        } else if (argument == "--dry-run") {
            result.dryRun = true;
        } else if (argument == "--help" || argument == "-h") {
            result.help = true;
        } else if (argument == "--version") {
            result.version = true;
        } else if (argument.startsWith('-') && argument.size() > 1) {
            throw std::invalid_argument(("Unknown option: " + argument).toStdString());
        } else {
            result.arguments.append(argument);
        }
    }
    if (result.configDirectory.isEmpty())
        result.configDirectory = defaultConfigDirectory();
    return result;
}

void printHelp() {
    QTextStream(stdout) << R"(conV — a fast, native file format converter

Usage:
  conV [FILE...]                         open the app, optionally with files queued
  conV theme list
  conV theme path
  conV theme show
  conV theme preset phosphor|amber|ice|paper [--dry-run]
  conV theme set 'accent=#4dff9a' 'background=#e00b120e' [--dry-run]
  conV theme follow PATH [--dry-run]
  conV theme import FILE [--dry-run]
  conV theme export FILE [--dry-run]

Tabs: Photo (ImageMagick), Video and Audio (FFmpeg), Docs (Pandoc). A tab whose
tool is not installed explains what is missing; the others keep working.

Theming:
  Changes are saved atomically to theme.json and applied to every open window
  immediately. Colors are #RRGGBB; the background may also be #AARRGGBB (alpha
  first, as in Qt) to make the window translucent. Run `theme show` for the keys.
  `theme follow PATH` tracks a palette and re-themes live whenever it changes.
  PATH is a QML color file (hex strings or Qt.rgba(...)), a JSON palette, or a
  directory holding one of Colors.qml, palette.json, colors.json.

Options:
  --config-dir DIRECTORY   settings and theme location (or CONV_CONFIG_DIR)
  --dry-run                print the proposed theme JSON without writing
  --version, --help
)";
}

int runThemeCommand(const Options &options) {
    auto arguments = options.arguments;
    arguments.removeFirst();
    if (arguments.isEmpty())
        throw std::invalid_argument("Expected a theme subcommand. Run conV --help.");
    const QString command = arguments.takeFirst();
    const QString path = options.configDirectory + "/theme.json";
    QTextStream output(stdout);
    if (command == "list") {
        requireCount(arguments, 0);
        output << presetNames().join('\n') << '\n';
        return 0;
    }
    if (command == "path") {
        requireCount(arguments, 0);
        output << path << '\n';
        return 0;
    }
    const auto current = [&] {
        return QFileInfo::exists(path) ? loadTheme(path) : presetTheme("phosphor");
    };
    if (command == "show") {
        requireCount(arguments, 0);
        output << QJsonDocument(current().toJson()).toJson();
        return 0;
    }
    Theme next;
    QString destination = path;
    if (command == "preset") {
        requireCount(arguments, 1);
        next = presetTheme(arguments[0]);
    } else if (command == "set") {
        next = setColors(current(), arguments);
    } else if (command == "follow") {
        requireCount(arguments, 1);
        next = followTheme(arguments[0]);
        const auto source = QFileInfo(next.sourcePath);
        if (source.canonicalFilePath() == QFileInfo(path).canonicalFilePath() && !source.canonicalFilePath().isEmpty())
            throw std::invalid_argument("A theme cannot follow its own output file");
    } else if (command == "import") {
        requireCount(arguments, 1);
        next = importPalette(QFileInfo(arguments[0]).absoluteFilePath());
        next.sourceKind.clear();
        next.sourcePath.clear();
    } else if (command == "export") {
        requireCount(arguments, 1);
        next = current();
        destination = QFileInfo(arguments[0]).absoluteFilePath();
    } else {
        throw std::invalid_argument(("Unknown theme command: " + command).toStdString());
    }
    if (options.dryRun) {
        output << QJsonDocument(next.toJson()).toJson();
        return 0;
    }
    writeJson(destination, next.toJson());
    if (destination == path)
        writeTextIfChanged(options.configDirectory + "/icon.svg", iconSvg(next));
    output << destination << '\n';
    return 0;
}

}
