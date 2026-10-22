#pragma once

#include "theme.h"
#include <QByteArray>

namespace conv {

/** @brief Render the application mark as a flat SVG in the theme's colors. */
QByteArray iconSvg(const Theme &theme);

}
