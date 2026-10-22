/**
 * @file window.cpp
 * @brief Main window composition, theme application, modals, and settings.
 *
 * The window background is painted with the theme's background alpha when the
 * compositor provides an ARGB surface, and with its RGB alone otherwise.
 */
#include "window.h"
#include "convert_page.h"
#include "core/icon.h"
#include "core/storage.h"
#include "modal.h"
#include "widgets.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace conv {
namespace {

class Canvas : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), themeColor("background"));
    }
};

class Rail : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), themeColor("surface"));
        painter.fillRect(QRect(width() - 2, 0, 2, height()), themeColor("border"));
    }
};

Glyph glyphFor(const QString &id) {
    if (id == "photo") return Glyph::Photo;
    if (id == "video") return Glyph::Video;
    if (id == "audio") return Glyph::Audio;
    return Glyph::Document;
}

}

Window::Window(QString configDirectory, Theme theme, const QStringList &files, QWidget *parent)
    : QMainWindow(parent), configDirectory_(std::move(configDirectory)),
      themeWatcher_(new ThemeWatcher(configDirectory_ + "/theme.json", std::move(theme), this)) {
    setWindowTitle("conV");
    setObjectName("convWindow");
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(780, 620);
    resize(1120, 780);
    setActiveTheme(themeWatcher_->theme());
    buildInterface();
    applyTheme();
    connect(themeWatcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(500);
    connect(&saveTimer_, &QTimer::timeout, this, &Window::saveSettings);

    try {
        const auto saved = readJson(configDirectory_ + "/settings.json");
        const auto states = saved.value("pages").toObject();
        for (int index = 0; index < pages_->count(); ++index) {
            auto *converter = page(index);
            converter->restore(states.value(converter->category().id).toObject());
        }
        select(std::clamp(saved.value("tab").toInt(0), 0, pages_->count() - 1));
    } catch (const std::exception &) {
        select(0);
    }
    routeFiles(files);
}

ConvertPage *Window::page(int index) const {
    return static_cast<ConvertPage *>(pages_->widget(index));
}

void Window::buildInterface() {
    canvas_ = new Canvas;
    setCentralWidget(canvas_);
    // The modal covers the canvas and blurs body_, so both must share one geometry.
    auto *canvasLayout = new QVBoxLayout(canvas_);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    body_ = new QWidget;
    canvasLayout->addWidget(body_);
    auto *outer = new QHBoxLayout(body_);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *rail = new Rail;
    rail->setFixedWidth(206);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 26, 2, 18);
    railLayout->setSpacing(2);

    auto *brand = new QLabel("conV");
    QFont brandFont = uiFont(26, true);
    brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    brand->setFont(brandFont);
    brand->setProperty("tone", "accent");
    brand->setContentsMargins(22, 0, 0, 0);
    auto *tagline = new QLabel("FILE FORMAT CONVERTER");
    tagline->setFont(uiFont(7.5, true));
    tagline->setProperty("tone", "muted");
    tagline->setContentsMargins(22, 0, 0, 22);
    railLayout->addWidget(brand);
    railLayout->addWidget(tagline);

    pages_ = new QStackedWidget;
    int index = 0;
    for (const auto &category : categories()) {
        auto *converter = new ConvertPage(category);
        converter->showModal = [this](const ModalRequest &request, std::function<void(int)> done) { present(request, std::move(done)); };
        connect(converter, &ConvertPage::changed, &saveTimer_, qOverload<>(&QTimer::start));
        pages_->addWidget(converter);
        auto *tab = new RailTab(++index, category.title, glyphFor(category.id));
        tab->setUnavailable(!converter->toolAvailable());
        tabs_.append(tab);
        railLayout->addWidget(tab);
        const int target = index - 1;
        connect(tab, &QAbstractButton::clicked, this, [this, target] { select(target); });
        auto *shortcut = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(index)), this);
        connect(shortcut, &QShortcut::activated, this, [this, target] { select(target); });
    }
    railLayout->addStretch(1);
    themeLabel_ = new QLabel;
    themeLabel_->setFont(uiFont(8));
    themeLabel_->setProperty("tone", "muted");
    themeLabel_->setContentsMargins(22, 0, 0, 0);
    themeLabel_->setWordWrap(true);
    railLayout->addWidget(themeLabel_);

    outer->addWidget(rail);
    outer->addWidget(pages_, 1);

    connect(new QShortcut(QKeySequence("Ctrl+O"), this), &QShortcut::activated, this, [this] {
        if (!modal_)
            page(pages_->currentIndex())->browse();
    });
    connect(new QShortcut(QKeySequence("Ctrl+Return"), this), &QShortcut::activated, this, [this] {
        auto *current = page(pages_->currentIndex());
        if (!current->busy() && !modal_)
            current->start();
    });
}

void Window::select(int index) {
    if (modal_)
        return;
    pages_->setCurrentIndex(index);
    tabs_[index]->setChecked(true);
    saveTimer_.start();
}

void Window::present(const ModalRequest &request, std::function<void(int)> done) {
    if (modal_)
        return;
    modal_ = new ModalCard(canvas_, body_, request);
    connect(modal_, &ModalCard::chosen, this, [this, done = std::move(done)](int id) {
        auto *card = modal_;
        modal_ = nullptr;
        card->hide();
        card->deleteLater();
        done(id);
    });
    modal_->show();
    modal_->raise();
}

void Window::routeFiles(const QStringList &files) {
    if (files.isEmpty())
        return;
    for (int index = 0; index < pages_->count(); ++index) {
        QStringList mine;
        for (const auto &file : files) {
            if (accepts(page(index)->category(), file))
                mine.append(file);
        }
        if (!mine.isEmpty() && page(index)->toolAvailable()) {
            select(index);
            page(index)->addFiles(mine);
            return;
        }
    }
}

void Window::applyTheme() {
    const auto &theme = themeWatcher_->theme();
    setActiveTheme(theme);
    qApp->setStyleSheet(conv::styleSheet(theme));
    themeLabel_->setText("THEME · " + theme.name.toUpper());
    if (modal_)
        modal_->setTint(theme.color("background"));
    for (auto *widget : findChildren<QWidget *>())
        widget->update();
    update();
    try {
        const auto svg = iconSvg(theme);
        writeTextIfChanged(configDirectory_ + "/icon.svg", svg);
        QPixmap pixmap(128, 128);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPainterPath shape = chamfer(QRectF(6, 6, 116, 116), 16);
        painter.fillPath(shape, opaque(theme.color("background")));
        painter.setPen(QPen(theme.color("accent"), 6, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawPath(shape);
        painter.setPen(QPen(theme.color("text"), 10, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawPolyline(QPolygonF{{30, 44}, {52, 64}, {30, 84}});
        painter.setPen(QPen(theme.color("accent"), 10, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawPolyline(QPolygonF{{62, 44}, {84, 64}, {62, 84}});
        painter.fillRect(QRectF(30, 98, 68, 8), theme.color("accent"));
        painter.end();
        setWindowIcon(QIcon(pixmap));
    } catch (const std::exception &) {
    }
}

void Window::saveSettings() {
    QJsonObject pages;
    for (int index = 0; index < pages_->count(); ++index)
        pages.insert(page(index)->category().id, page(index)->state());
    try {
        writeJson(configDirectory_ + "/settings.json", {{"version", 1}, {"tab", pages_->currentIndex()}, {"pages", pages}});
    } catch (const std::exception &) {
    }
}

void Window::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

}
