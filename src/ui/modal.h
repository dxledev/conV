#pragma once

#include "modal_backdrop.h"
#include "style.h"
#include <QVector>

namespace conv {

/** @brief One button in a modal. */
struct Choice {
    QString text;
    int id;
    bool primary = false;
};

/** @brief Everything needed to present an in-window dialog. */
struct ModalRequest {
    QString title;
    QString body;
    QString detail; ///< Optional monospace block, e.g. a tool's error output.
    QVector<Choice> choices;
    Glyph glyph = Glyph::Info;
};

/**
 * @brief Blurred in-window dialog. Emits chosen() once with a button id, or -1 for
 *        Escape and clicks outside the card. The owner deletes it afterwards.
 */
class ModalCard : public ModalBackdrop {
    Q_OBJECT
public:
    ModalCard(QWidget *parent, QWidget *background, const ModalRequest &request);

signals:
    void chosen(int id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QVector<QWidget *> buttons_;
    QWidget *defaultButton_ = nullptr;
    bool finished_ = false;
    void finish(int id);
    void placeCard();
};

}
