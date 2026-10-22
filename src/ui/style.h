#pragma once

#include "core/theme.h"
#include <QFont>
#include <QPainter>
#include <QPainterPath>

namespace conv {

/** @brief Glyphs drawn as vector paths so they always take the theme's colors. */
enum class Glyph { Photo, Video, Audio, Document, Add, Folder, Play, Stop, Close, Check, Warn, Drop, Clear, Info };

/** @brief The palette every custom widget paints with. */
const Theme &activeTheme();
void setActiveTheme(const Theme &theme);
/** @brief Shortcut for activeTheme().color(key). */
QColor themeColor(const QString &key);
/** @brief Monospace UI font at a point size; labels are upper-cased by callers. */
QFont uiFont(qreal points, bool bold = false);
/** @brief Polygon with the top-left and bottom-right corners cut by @p cut pixels. */
QPainterPath chamfer(const QRectF &rect, qreal cut);
/** @brief Draw @p glyph inside @p box with a 1.6 px square-capped stroke. */
void paintGlyph(QPainter &painter, Glyph glyph, const QRectF &box, const QColor &color);
/** @brief Application-wide stylesheet for the standard widgets (labels, scrollbars, sliders). */
QString styleSheet(const Theme &theme);

}
