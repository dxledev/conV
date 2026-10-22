/**
 * @file widgets.cpp
 * @brief Custom-painted flat controls. Colors are read from the active theme at paint time,
 *        so a live theme change only needs update().
 */
#include "widgets.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QTimer>
#include <algorithm>

namespace conv {
namespace {

QColor pickRole(const QString &role) {
    if (role == "danger")
        return activeTheme().dangerColor;
    return themeColor(role);
}

void focusRing(QPainter &painter, const QPainterPath &path, const QWidget *widget) {
    if (!widget->hasFocus() || (widget->focusPolicy() == Qt::NoFocus))
        return;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(themeColor("accent"), 2));
    painter.drawPath(path);
}

}

RetroButton::RetroButton(const QString &text, QWidget *parent) : QAbstractButton(parent) {
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFont(uiFont(10, true));
}

QSize RetroButton::sizeHint() const {
    QFont font = uiFont(chip_ ? 9.5 : 10, true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    const int textWidth = QFontMetrics(font).horizontalAdvance(text().toUpper()) + text().size();
    const int glyph = hasGlyph_ ? 26 : 0;
    return {textWidth + glyph + (chip_ ? 28 : 36), chip_ ? 34 : 42};
}

void RetroButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked() || (primary_ && isEnabled());
    const bool pressed = isDown();
    const bool hover = underMouse() && isEnabled();
    QColor fill = themeColor("surface_alt"), edge = themeColor("border"), ink = themeColor("text");
    if (on && isEnabled()) {
        fill = themeColor("accent");
        edge = fill;
        ink = themeColor("accent_text");
        if (pressed)
            fill = fill.darker(115);
        else if (hover)
            fill = fill.lighter(112);
    } else if (!isEnabled()) {
        fill = themeColor("surface");
        edge = themeColor("line");
        ink = themeColor("muted");
    } else if (pressed) {
        fill = themeColor("active");
        edge = themeColor("accent");
    } else if (hover) {
        fill = themeColor("hover");
        edge = themeColor("accent");
    }
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    const auto path = chamfer(box, chip_ ? 6 : 8);
    painter.fillPath(path, fill);
    painter.setPen(QPen(edge, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    QFont font = uiFont(chip_ ? 9.5 : 10, true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    painter.setFont(font);
    painter.setPen(ink);
    QRectF textArea = box;
    if (hasGlyph_) {
        const qreal side = 16;
        const int textWidth = QFontMetrics(font).horizontalAdvance(text().toUpper()) + text().size();
        const qreal total = side + 8 + textWidth;
        const qreal left = box.center().x() - total / 2;
        paintGlyph(painter, glyph_, QRectF(left, box.center().y() - side / 2, side, side), ink);
        textArea.setLeft(left + side + 8);
        painter.drawText(textArea, Qt::AlignVCenter | Qt::AlignLeft, text().toUpper());
    } else {
        painter.drawText(box, Qt::AlignCenter, text().toUpper());
    }
    if (hasFocus())
        focusRing(painter, chamfer(QRectF(rect()).adjusted(-0.5, -0.5, 0.5, 0.5), chip_ ? 6 : 8), this);
}

RailTab::RailTab(int index, const QString &label, Glyph glyph, QWidget *parent)
    : QAbstractButton(parent), index_(index), glyph_(glyph) {
    setText(label);
    setCheckable(true);
    setAutoExclusive(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void RailTab::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool selected = isChecked();
    QColor ink = selected ? themeColor("accent") : themeColor("muted");
    if (underMouse() && !selected)
        ink = themeColor("text");
    if (selected)
        painter.fillRect(rect(), themeColor("active"));
    else if (underMouse())
        painter.fillRect(rect(), themeColor("hover"));
    // Solid 4 px selection bar on the leading edge.
    if (selected)
        painter.fillRect(QRect(0, 0, 4, height()), themeColor("accent"));
    painter.setFont(uiFont(8.5));
    painter.setPen(themeColor(selected ? "accent" : "muted"));
    painter.drawText(QRect(16, 0, 26, height()), Qt::AlignVCenter | Qt::AlignLeft, QString("%1").arg(index_, 2, 10, QChar('0')));
    paintGlyph(painter, glyph_, QRectF(44, height() / 2.0 - 10, 20, 20), ink);
    QFont font = uiFont(11, true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    painter.setFont(font);
    painter.setPen(selected ? themeColor("text") : ink);
    painter.drawText(QRect(74, 0, width() - 100, height()), Qt::AlignVCenter | Qt::AlignLeft, text().toUpper());
    if (unavailable_)
        paintGlyph(painter, Glyph::Warn, QRectF(width() - 30, height() / 2.0 - 8, 16, 16), activeTheme().dangerColor);
    if (hasFocus()) {
        painter.setPen(QPen(themeColor("accent"), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(rect()).adjusted(2, 2, -2, -2));
    }
}

Panel::Panel(const QString &caption, QWidget *parent) : QWidget(parent), caption_(caption) {}

void Panel::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(1, 8, -1, -1);
    const auto path = chamfer(box, 14);
    painter.fillPath(path, themeColor("surface"));
    QPen pen(themeColor(dashed_ ? "border" : "line"), dashed_ ? 2 : 1.5);
    if (dashed_)
        pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    if (!caption_.isEmpty()) {
        QFont font = uiFont(8.5, true);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
        painter.setFont(font);
        const QString text = caption_.toUpper();
        const int width = QFontMetrics(font).horizontalAdvance(text) + text.size() * 2 + 16;
        // Knock the caption out of the border with a flat patch of the surface colour.
        painter.fillRect(QRectF(24, 0, width, 17), themeColor("surface"));
        painter.setPen(themeColor("accent"));
        painter.drawText(QRectF(32, 0, width, 17), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}

ProgressStrip::ProgressStrip(QWidget *parent) : QWidget(parent) {
    timer_.setInterval(90);
    connect(&timer_, &QTimer::timeout, this, [this] { phase_ = (phase_ + 1) % 24; update(); });
    setFixedHeight(10);
}

void ProgressStrip::setValue(double value) {
    value_ = value;
    if (isVisible()) {
        if (value_ < 0 && !timer_.isActive())
            timer_.start();
        else if (value_ >= 0)
            timer_.stop();
    }
    update();
}

void ProgressStrip::showEvent(QShowEvent *) {
    if (value_ < 0)
        timer_.start();
}

void ProgressStrip::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const int block = 8, gap = 2;
    const int count = std::max(1, (width() + gap) / (block + gap));
    const QColor on = pickRole(role_), off = themeColor("line");
    for (int index = 0; index < count; ++index) {
        bool lit;
        if (value_ < 0)
            lit = ((index + phase_) % 24) < 4;
        else
            lit = index < qRound(value_ * count);
        painter.fillRect(QRect(index * (block + gap), 0, block, height()), lit ? on : off);
    }
}

FlowLayout::~FlowLayout() {
    qDeleteAll(items_);
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (auto *item : items_)
        size = size.expandedTo(item->minimumSize());
    return size + QSize(contentsMargins().left() + contentsMargins().right(), contentsMargins().top() + contentsMargins().bottom());
}

int FlowLayout::arrange(const QRect &rect, bool measureOnly) const {
    const auto margins = contentsMargins();
    const QRect area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = area.x(), y = area.y(), rowHeight = 0;
    for (auto *item : items_) {
        const QSize hint = item->sizeHint();
        if (x + hint.width() > area.right() + 1 && rowHeight > 0) {
            x = area.x();
            y += rowHeight + spacing_;
            rowHeight = 0;
        }
        if (!measureOnly)
            item->setGeometry(QRect(QPoint(x, y), hint));
        x += hint.width() + spacing_;
        rowHeight = std::max(rowHeight, hint.height());
    }
    return y + rowHeight - rect.y() + margins.bottom();
}

}
