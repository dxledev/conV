#pragma once

#include <QStringList>

namespace conv {

/** @brief Parsed command-line settings shared by startup and theme commands. */
struct Options {
    QString configDirectory; ///< Absolute directory used for persistent application data.
    QStringList arguments;   ///< Positional arguments: files to open, or a theme command.
    bool dryRun = false;     ///< Validate and print a theme change without writing it.
    bool help = false;
    bool version = false;
    /** @brief Whether the positional arguments are a theme command. */
    bool isThemeCommand() const { return !arguments.isEmpty() && arguments.first() == "theme"; }
};

/** @brief Parse options, throwing std::invalid_argument on bad input. */
Options parseOptions(QStringList arguments);
void printHelp();
/** @brief Execute a theme subcommand and return its exit code. */
int runThemeCommand(const Options &options);

}
