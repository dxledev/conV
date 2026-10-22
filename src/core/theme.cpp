/**
 * @file theme.cpp
 * @brief Theme presets, palette validation, color derivation, and JSON format.
 *
 * Colors are written as #RRGGBB. The background alone may carry opacity, in
 * which case it is written as Qt's #AARRGGBB (alpha first). Alpha is
 * quantized to 8 bits on every load so repeated save/reload cycles settle.
 */
#include "theme.h"
#include "storage.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace conv {
namespace {

const QMap<QString, QStringList> &palettes() {
    static const QMap<QString, QStringList> value{
    {"phosphor", {"#0b120e", "#101b14", "#16251b", "#d5f5df", "#7fa88b", "#4dff9a", "#04140a", "#25402f", "#1d3326", "#14231a", "#1e3a2a"}},
    {"amber", {"#140e07", "#1d150b", "#281d0f", "#ffe6bd", "#b89a6a", "#ffb000", "#1a1000", "#4a3418", "#35250f", "#221808", "#3a2a12"}},
    {"ice", {"#0a1220", "#0f1a2e", "#162340", "#dbe9ff", "#7f97c2", "#5cc8ff", "#001526", "#26406b", "#1b2e50", "#122038", "#1d3560"}},
    {"paper", {"#ece8dc", "#f6f3ea", "#e2ddcc", "#1d2420", "#667068", "#0b6b4f", "#ffffff", "#b9b29a", "#cfc9b4", "#e8e3d3", "#d3dcc9"}},
    };
    return value;
}

[[noreturn]] void invalid(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

double contrast(const QColor &foreground, const QColor &background) {
    const double a = luminance(foreground), b = luminance(background);
    return (std::max(a, b) + .05) / (std::min(a, b) + .05);
}

QColor generateDanger(const Theme &theme) {
    const auto surface = theme.color("surface");
    const double saturation = .7;
    QColor best = QColor::fromHslF(0.0, saturation, .55);
    double bestContrast = 0;
    for (int step = 25; step <= 85; ++step) {
        const auto candidate = QColor::fromRgb(QColor::fromHslF(0.0, saturation, step / 100.0).rgb());
        const double value = contrast(candidate, surface);
        if ((value >= 4.5 && bestContrast < 4.5) || (value >= 4.5 && std::abs(step - 58) < std::abs(best.lightnessF() * 100 - 58))
            || (bestContrast < 4.5 && value > bestContrast)) {
            best = candidate;
            bestContrast = value;
        }
    }
    return best;
}

QColor parseColorValue(const QString &key, const QString &value) {
    static const QRegularExpression pattern("^#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$");
    if (!pattern.match(value).hasMatch())
        invalid(key + ": expected a color in #RRGGBB or #AARRGGBB format");
    // QColor reads eight digits as #AARRGGBB, so alpha comes first and RGB is never shifted.
    const auto color = QColor::fromString(value);
    return key == "background" ? color : opaque(color);
}

}

QColor opaque(const QColor &color) {
    return QColor::fromRgb(color.rgb());
}

double luminance(const QColor &color) {
    const auto linear = [](double component) {
        return component <= .04045 ? component / 12.92 : std::pow((component + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

QString Theme::hex(const QString &key) const {
    const auto value = color(key);
    return value.alpha() == 255 ? value.name(QColor::HexRgb) : value.name(QColor::HexArgb);
}

QStringList colorKeys() {
    return {"background", "surface", "surface_alt", "text", "muted", "accent", "accent_text",
            "border", "line", "hover", "active"};
}

QStringList presetNames() {
    return palettes().keys();
}

Theme presetTheme(const QString &name) {
    if (!palettes().contains(name))
        invalid("Unknown preset: " + name + ". Available: " + presetNames().join(", "));
    Theme theme{name, {}, {}, {}};
    const auto keys = colorKeys();
    const auto values = palettes().value(name);
    for (int index = 0; index < keys.size(); ++index)
        theme.colors.insert(keys[index], QColor(values[index]));
    theme.dangerColor = generateDanger(theme);
    return theme;
}

QJsonObject Theme::toJson() const {
    QJsonObject result;
    for (const auto &key : colorKeys())
        result.insert(key, hex(key));
    QJsonObject data{{"version", 1}, {"name", name}, {"colors", result}};
    if (!sourceKind.isEmpty())
        data.insert("source", QJsonObject{{"kind", sourceKind}, {"path", sourcePath}});
    return data;
}

Theme parseTheme(const QJsonObject &data) {
    if (data.value("version").toInt(-1) != 1)
        invalid("Theme version must be 1");
    const QString name = data.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 60)
        invalid("Theme name must contain 1–60 characters");
    if (!data.value("colors").isObject())
        invalid("Theme must contain a colors object");
    const auto colors = data.value("colors").toObject();
    const auto keys = colorKeys();
    for (auto color = colors.constBegin(); color != colors.constEnd(); ++color) {
        if (!keys.contains(color.key()))
            invalid("Unknown color: " + color.key());
    }
    Theme theme{name, {}, {}, {}};
    for (const auto &key : keys)
        theme.colors.insert(key, parseColorValue(key, colors.value(key).toString()));
    theme.dangerColor = generateDanger(theme);
    if (data.contains("source")) {
        if (!data.value("source").isObject())
            invalid("Theme source must be an object");
        const auto source = data.value("source").toObject();
        theme.sourceKind = source.value("kind").toString();
        theme.sourcePath = source.value("path").toString();
        if (theme.sourceKind != "file" || !QFileInfo(theme.sourcePath).isAbsolute())
            invalid("Theme source needs kind \"file\" and an absolute path");
    }
    return theme;
}

Theme loadTheme(const QString &path) {
    return parseTheme(readJson(path));
}

Theme ensureTheme(const QString &path) {
    if (QFileInfo::exists(path))
        return loadTheme(path);
    const auto theme = presetTheme("phosphor");
    writeJson(path, theme.toJson());
    return theme;
}

Theme setColors(Theme theme, const QStringList &assignments) {
    if (assignments.isEmpty())
        invalid("Provide at least one COLOR=#RRGGBB assignment");
    auto data = theme.toJson();
    auto colors = data.value("colors").toObject();
    for (const auto &assignment : assignments) {
        const auto separator = assignment.indexOf('=');
        const auto key = assignment.left(separator);
        if (separator < 1 || !colorKeys().contains(key))
            invalid("Expected COLOR=#RRGGBB. Colors: " + colorKeys().join(", "));
        colors.insert(key, assignment.mid(separator + 1));
    }
    data.insert("name", "custom");
    data.remove("source");
    data.insert("colors", colors);
    return parseTheme(data);
}

}
