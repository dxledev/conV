#pragma once

#include "theme.h"

namespace conv {

/** @brief Import a palette file (QML, JSON), optionally assigning a display name. */
Theme importPalette(const QString &path, const QString &name = {});
/** @brief Bind to a palette file or directory and resolve its current colors. */
Theme followTheme(const QString &path);
/** @brief Re-read the source bound to a theme and return its resolved colors. */
Theme refreshSource(const Theme &theme);
/** @brief Return files and directories that must be watched for source changes. */
QStringList sourceWatchPaths(const Theme &theme);

}
