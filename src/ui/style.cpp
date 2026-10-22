/**
 * @file style.cpp
 * @brief Retro-futuristic flat styling: monospace type, chamfered corners, vector glyphs.
 */
#include "style.h"

#include <QFontDatabase>

namespace conv {
namespace {

Theme &theme_() {
    static Theme value = presetTheme("phosphor");
    return value;
}

}

const Theme &activeTheme() { return theme_(); }
void setActiveTheme(const Theme &theme) { theme_() = theme; }
QColor themeColor(const QString &key) { return theme_().color(key); }

QFont uiFont(qreal points, bool bold) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSizeF(points);
    font.setBold(bold);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QPainterPath chamfer(const QRectF &r, qreal cut) {
    QPainterPath path;
    path.moveTo(r.left() + cut, r.top());
    path.lineTo(r.right(), r.top());
    path.lineTo(r.right(), r.bottom() - cut);
    path.lineTo(r.right() - cut, r.bottom());
    path.lineTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + cut);
    path.closeSubpath();
    return path;
}

void paintGlyph(QPainter &painter, Glyph glyph, const QRectF &box, const QColor &color) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(box.topLeft());
    painter.scale(box.width() / 24.0, box.height() / 24.0);
    QPen pen(color, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Photo:
        painter.drawRect(QRectF(3, 4, 18, 16));
        painter.drawPolyline(QPolygonF{{3, 17}, {9, 11}, {13, 15}, {16, 12}, {21, 17}});
        painter.setBrush(color);
        painter.drawRect(QRectF(15, 7, 2.5, 2.5));
        break;
    case Glyph::Video:
        painter.drawRect(QRectF(3, 5, 18, 14));
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{{10, 9}, {16, 12}, {10, 15}});
        break;
    case Glyph::Audio:
        for (int i = 0; i < 5; ++i) {
            const double heights[] = {5, 11, 16, 9, 6};
            painter.drawLine(QPointF(4 + i * 4, 12 - heights[i] / 2), QPointF(4 + i * 4, 12 + heights[i] / 2));
        }
        break;
    case Glyph::Document:
        painter.drawPolyline(QPolygonF{{5, 3}, {15, 3}, {19, 7}, {19, 21}, {5, 21}, {5, 3}});
        painter.drawLine(8, 11, 16, 11);
        painter.drawLine(8, 15, 16, 15);
        painter.drawLine(8, 7, 11, 7);
        break;
    case Glyph::Add:
        painter.drawLine(12, 4, 12, 20);
        painter.drawLine(4, 12, 20, 12);
        break;
    case Glyph::Folder:
        painter.drawPolyline(QPolygonF{{3, 6}, {10, 6}, {12, 8}, {21, 8}, {21, 19}, {3, 19}, {3, 6}});
        break;
    case Glyph::Play:
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{{7, 4}, {19, 12}, {7, 20}});
        break;
    case Glyph::Stop:
        painter.setBrush(color);
        painter.drawRect(QRectF(6, 6, 12, 12));
        break;
    case Glyph::Close:
        painter.drawLine(6, 6, 18, 18);
        painter.drawLine(18, 6, 6, 18);
        break;
    case Glyph::Check:
        painter.drawPolyline(QPolygonF{{4, 12}, {10, 18}, {20, 6}});
        break;
    case Glyph::Warn:
        painter.drawPolygon(QPolygonF{{12, 3}, {22, 20}, {2, 20}});
        painter.drawLine(12, 9, 12, 14);
        painter.drawPoint(QPointF(12, 17));
        break;
    case Glyph::Drop:
        painter.drawLine(12, 3, 12, 15);
        painter.drawPolyline(QPolygonF{{6, 10}, {12, 16}, {18, 10}});
        painter.drawPolyline(QPolygonF{{4, 17}, {4, 21}, {20, 21}, {20, 17}});
        break;
    case Glyph::Clear:
        painter.drawLine(5, 7, 19, 7);
        painter.drawPolyline(QPolygonF{{8, 7}, {8, 4}, {16, 4}, {16, 7}});
        painter.drawPolyline(QPolygonF{{7, 7}, {8, 20}, {16, 20}, {17, 7}});
        break;
    case Glyph::Info:
        painter.drawRect(QRectF(3, 3, 18, 18));
        painter.drawLine(12, 11, 12, 17);
        painter.drawPoint(QPointF(12, 7.5));
        break;
    }
    painter.restore();
}

QString styleSheet(const Theme &t) {
    const auto text = t.hex("text"), muted = t.hex("muted"), line = t.hex("line"), accent = t.hex("accent"),
               border = t.hex("border"), surface = t.hex("surface_alt"), danger = t.dangerColor.name();
    return QString(R"(
QWidget { color: %1; selection-background-color: %3; selection-color: %7; }
QLabel { background: transparent; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %4; min-height: 28px; border: none; }
QScrollBar::handle:vertical:hover { background: %3; }
QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: transparent; height: 0; border: none; }
QSlider::groove:horizontal { height: 4px; background: %5; border: none; }
QSlider::sub-page:horizontal { background: %3; }
QSlider::handle:horizontal { background: %3; width: 12px; height: 20px; margin: -8px 0; border: 2px solid %6; }
QSlider::handle:horizontal:hover { background: %1; }
QSlider:focus { outline: none; }
QLabel[tone="muted"] { color: %2; }
QLabel[tone="accent"] { color: %3; }
QLabel[tone="danger"] { color: %8; }
)").arg(text, muted, accent, border, line, surface, t.hex("accent_text"), danger);
}

}
