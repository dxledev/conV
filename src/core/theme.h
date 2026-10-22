#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace conv {

/**
 * @brief Validated palette, optional source binding, and derived status colors.
 *
 * Only the background keeps an alpha channel (stored as Qt #AARRGGBB). Every
 * other color is opaque so derived mixes never shift RGB channels.
 */
struct Theme {
    QString name; ///< Display name or preset identifier.
    QMap<QString, QColor> colors; ///< Complete map of configurable palette keys.
    QString sourceKind; ///< External source type, empty when following no source.
    QString sourcePath; ///< Path used by the external source.
    QColor dangerColor = {}; ///< Cached readable error color.
    /** @brief Read a configured color by key. */
    QColor color(const QString &key) const { return colors.value(key); }
    /** @brief Return a configured color as #RRGGBB, or #AARRGGBB for translucent colors. */
    QString hex(const QString &key) const;
    /** @brief Serialize the configurable palette and source metadata. */
    QJsonObject toJson() const;
    /** @brief Compare all theme fields, including derived colors. */
    bool operator==(const Theme &) const = default;
};

/** @brief Return the required configurable palette keys in canonical order. */
QStringList colorKeys();
/** @brief Return the names of built-in themes. */
QStringList presetNames();
/** @brief Construct a built-in palette by name. */
Theme presetTheme(const QString &name);
/** @brief Parse and validate a complete theme object. */
Theme parseTheme(const QJsonObject &data);
/** @brief Load and validate a theme JSON file. */
Theme loadTheme(const QString &path);
/** @brief Load an existing theme or atomically create the default theme file. */
Theme ensureTheme(const QString &path);
/** @brief Apply key=value palette assignments to a copy of a theme. */
Theme setColors(Theme theme, const QStringList &assignments);
/** @brief Drop alpha without touching the RGB channels. */
QColor opaque(const QColor &color);
/** @brief Perceptual luminance used for contrast decisions. */
double luminance(const QColor &color);

}
