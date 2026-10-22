/**
 * @file icon.cpp
 * @brief Flat, theme-colored application icon (no gradients).
 */
#include "icon.h"

namespace conv {

QByteArray iconSvg(const Theme &theme) {
    // Opaque colors only: launchers draw the icon on arbitrary backgrounds.
    const auto fill = opaque(theme.color("background")).name();
    const auto frame = theme.hex("accent");
    const auto text = theme.hex("text");
    const auto accent = theme.hex("accent");
    return QString(R"(<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <polygon points="22,6 122,6 122,106 106,122 6,122 6,22" fill="%1"/>
  <polygon points="22,6 122,6 122,106 106,122 6,122 6,22" fill="none" stroke="%2" stroke-width="6" stroke-linejoin="miter"/>
  <polyline points="30,44 52,64 30,84" fill="none" stroke="%3" stroke-width="10" stroke-linecap="square" stroke-linejoin="miter"/>
  <polyline points="62,44 84,64 62,84" fill="none" stroke="%4" stroke-width="10" stroke-linecap="square" stroke-linejoin="miter"/>
  <rect x="30" y="98" width="68" height="8" fill="%4"/>
</svg>
)").arg(fill, frame, text, accent).toUtf8();
}

}
