#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace conv {

/** @brief One conversion tab: its tool, accepted inputs, and selectable outputs. */
struct Category {
    QString id;            ///< Stable identifier: photo, video, audio, document.
    QString title;         ///< Tab label.
    QString tool;          ///< Primary executable name.
    QStringList inputs;    ///< Lower-case input extensions.
    QStringList outputs;   ///< Lower-case output extensions offered in the UI.
    bool hasQuality;       ///< Whether the quality control applies.
    QString qualityLabel;  ///< Name of the quality control.
};

/** @brief A fully resolved process invocation. */
struct Command {
    QString program;
    QStringList arguments;
};

/** @brief All conversion categories in tab order. */
const QVector<Category> &categories();
/** @brief Locate an executable on PATH; empty when absent. */
QString findTool(const QString &name);
/** @brief Resolve the executable for a category (photo accepts magick or convert). */
QString categoryTool(const Category &category);
/** @brief Lower-case extension of a path, without the dot. */
QString extensionOf(const QString &path);
/** @brief Whether the category can read the file's extension. */
bool accepts(const Category &category, const QString &path);
/**
 * @brief Choose the destination for a conversion.
 * @param directory Output directory, or empty to use the source's directory.
 * @return A path that never equals the input; a same-extension, same-directory
 *         result gets a "-converted" suffix instead of replacing the source.
 */
QString outputPath(const QString &input, const QString &directory, const QString &extension);
/** @brief Append " (n)" until the path does not exist. */
QString uniquePath(const QString &path);
/** @brief Build the process invocation for one file. @p quality is 1–100. */
Command buildCommand(const Category &category, const QString &input, const QString &output, int quality);
/** @brief Parse an `out_time_us=` / `out_time_ms=` progress line to seconds, or -1. */
double progressSeconds(const QString &line);

}
