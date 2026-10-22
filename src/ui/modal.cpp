/**
 * @file modal.cpp
 * @brief In-window dialog card over the shared blurred backdrop.
 *
 * Focus is trapped on the card's buttons, so keyboard users cannot reach the
 * blurred page behind it.
 */
#include "modal.h"
#include "widgets.h"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <algorithm>

namespace conv {
namespace {

class Card : public QWidget {
public:
    explicit Card(QWidget *parent) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(1, 1, -7, -7);
        // Flat hard offset shadow: a solid second copy of the outline, no blur.
        painter.fillPath(chamfer(box.translated(6, 6), 16), themeColor("line"));
        painter.fillPath(chamfer(box, 16), themeColor("surface"));
        painter.setPen(QPen(themeColor("accent"), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(chamfer(box, 16));
    }
};

}

ModalCard::ModalCard(QWidget *parent, QWidget *background, const ModalRequest &request)
    : ModalBackdrop(parent, background) {
    setTint(themeColor("background"));
    parent->installEventFilter(this);
    setGeometry(parent->rect());
    setFocusPolicy(Qt::StrongFocus);
    auto *card = new Card(this);
    card_ = card;
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 24, 34, 30);
    layout->setSpacing(14);

    auto *head = new QHBoxLayout;
    head->setSpacing(10);
    auto *icon = new QWidget;
    icon->setFixedSize(22, 22);
    icon->installEventFilter(this);
    icon->setProperty("glyph", int(request.glyph));
    head->addWidget(icon);
    auto *title = new QLabel(request.title.toUpper());
    QFont titleFont = uiFont(13, true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    title->setFont(titleFont);
    title->setProperty("tone", "accent");
    head->addWidget(title, 1);
    layout->addLayout(head);

    auto *body = new QLabel(request.body);
    body->setWordWrap(true);
    body->setFont(uiFont(10.5));
    body->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(body);

    if (!request.detail.isEmpty()) {
        auto *detail = new QPlainTextEdit(request.detail);
        detail->setReadOnly(true);
        detail->setFont(uiFont(9));
        detail->setFixedHeight(std::clamp(int(request.detail.count('\n')) * 18 + 46, 64, 150));
        detail->setFrameShape(QFrame::NoFrame);
        detail->setFocusPolicy(Qt::NoFocus);
        detail->setStyleSheet(QString("QPlainTextEdit { background: %1; border: 1.5px solid %2; padding: 6px; }")
                                  .arg(activeTheme().hex("surface_alt"), activeTheme().hex("line")));
        layout->addWidget(detail);
    }

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addStretch(1);
    for (const auto &choice : request.choices) {
        auto *button = new RetroButton(choice.text);
        button->setPrimary(choice.primary);
        connect(button, &QAbstractButton::clicked, this, [this, id = choice.id] { finish(id); });
        button->installEventFilter(this);
        buttons_.append(button);
        if (choice.primary || !defaultButton_)
            defaultButton_ = button;
        row->addWidget(button);
    }
    layout->addLayout(row);

    auto *opacity = new QGraphicsOpacityEffect(card);
    card->setGraphicsEffect(opacity);
    auto *fade = new QPropertyAnimation(opacity, "opacity", card);
    fade->setDuration(140);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    connect(fade, &QPropertyAnimation::finished, card, [card] { card->setGraphicsEffect(nullptr); });
    connect(this, &ModalBackdrop::outsideClicked, this, [this] { finish(-1); });
    QMetaObject::invokeMethod(fade, "start", Qt::QueuedConnection);
}

void ModalCard::finish(int id) {
    if (finished_)
        return;
    finished_ = true;
    emit chosen(id);
}

void ModalCard::placeCard() {
    const int width = std::min(520, std::max(280, this->width() - 48));
    card_->setFixedWidth(width);
    card_->adjustSize();
    card_->move((this->width() - card_->width()) / 2, std::max(12, (this->height() - card_->height()) / 2));
}

void ModalCard::resizeEvent(QResizeEvent *event) {
    ModalBackdrop::resizeEvent(event);
    placeCard();
}

void ModalCard::showEvent(QShowEvent *event) {
    ModalBackdrop::showEvent(event);
    placeCard();
    if (defaultButton_)
        defaultButton_->setFocus();
    else
        setFocus();
}

void ModalCard::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        finish(-1);
    else
        event->accept();
}

bool ModalCard::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parent() && event->type() == QEvent::Resize) {
        setGeometry(static_cast<QWidget *>(parent())->rect());
        return false;
    }
    if (event->type() == QEvent::Paint && watched->property("glyph").isValid()) {
        auto *widget = static_cast<QWidget *>(watched);
        QPainter painter(widget);
        paintGlyph(painter, Glyph(widget->property("glyph").toInt()), widget->rect(), themeColor("accent"));
        return true;
    }
    if (event->type() == QEvent::KeyPress && buttons_.contains(static_cast<QWidget *>(watched))) {
        auto *key = static_cast<QKeyEvent *>(event);
        const int count = buttons_.size();
        const int at = buttons_.indexOf(static_cast<QWidget *>(watched));
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab || key->key() == Qt::Key_Left || key->key() == Qt::Key_Right) {
            const bool backwards = key->key() == Qt::Key_Backtab || key->key() == Qt::Key_Left;
            buttons_[(at + (backwards ? count - 1 : 1)) % count]->setFocus();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            finish(-1);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space) {
            static_cast<QAbstractButton *>(watched)->click();
            return true;
        }
    }
    return ModalBackdrop::eventFilter(watched, event);
}

}
