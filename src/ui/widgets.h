#pragma once

#include "style.h"
#include <QAbstractButton>
#include <QLayout>
#include <QTimer>
#include <QWidget>

namespace conv {

/** @brief Flat chamfered button; checkable instances act as format chips. */
class RetroButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit RetroButton(const QString &text, QWidget *parent = nullptr);
    /** @brief Draw a vector glyph before the label. */
    void setGlyph(Glyph glyph) { glyph_ = glyph; hasGlyph_ = true; update(); }
    /** @brief Primary buttons fill with the accent color. */
    void setPrimary(bool primary) { primary_ = primary; update(); }
    /** @brief Compact chips use a smaller height and no glyph slot. */
    void setChip(bool chip) { chip_ = chip; updateGeometry(); update(); }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
    void focusInEvent(QFocusEvent *e) override { QAbstractButton::focusInEvent(e); update(); }
    void focusOutEvent(QFocusEvent *e) override { QAbstractButton::focusOutEvent(e); update(); }

private:
    Glyph glyph_ = Glyph::Add;
    bool hasGlyph_ = false, primary_ = false, chip_ = false;
};

/** @brief Vertical navigation tab with an index, glyph, and label. */
class RailTab : public QAbstractButton {
    Q_OBJECT
public:
    RailTab(int index, const QString &label, Glyph glyph, QWidget *parent = nullptr);
    /** @brief Mark the tab's converter as unavailable (missing tool). */
    void setUnavailable(bool unavailable) { unavailable_ = unavailable; update(); }
    QSize sizeHint() const override { return {180, 52}; }

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
    void focusInEvent(QFocusEvent *e) override { QAbstractButton::focusInEvent(e); update(); }
    void focusOutEvent(QFocusEvent *e) override { QAbstractButton::focusOutEvent(e); update(); }

private:
    int index_;
    Glyph glyph_;
    bool unavailable_ = false;
};

/** @brief Chamfered container with an inline caption in the top border. */
class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(const QString &caption, QWidget *parent = nullptr);
    void setCaption(const QString &caption) { caption_ = caption; update(); }
    /** @brief Draw a dashed border, used for the empty drop zone. */
    void setDashed(bool dashed) { dashed_ = dashed; update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString caption_;
    bool dashed_ = false;
};

/** @brief Blocky progress strip; a negative value shows an indeterminate sweep. */
class ProgressStrip : public QWidget {
    Q_OBJECT
public:
    explicit ProgressStrip(QWidget *parent = nullptr);
    /** @brief Fraction 0–1, or negative for indeterminate. */
    void setValue(double value);
    /** @brief Colour role: "accent", "danger" or "muted". */
    void setRole(const QString &role) { role_ = role; update(); }
    QSize sizeHint() const override { return {120, 10}; }

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override { timer_.stop(); }

private:
    double value_ = 0;
    int phase_ = 0;
    QString role_ = "accent";
    QTimer timer_;
};

/** @brief Layout that wraps its items onto new rows. */
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent = nullptr, int spacing = 8) : QLayout(parent), spacing_(spacing) {}
    ~FlowLayout() override;
    void addItem(QLayoutItem *item) override { items_.append(item); }
    int count() const override { return items_.size(); }
    QLayoutItem *itemAt(int index) const override { return items_.value(index); }
    QLayoutItem *takeAt(int index) override { return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect &rect) override { QLayout::setGeometry(rect); arrange(rect, false); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override;

private:
    int arrange(const QRect &rect, bool measureOnly) const;
    QList<QLayoutItem *> items_;
    int spacing_;
};

}
