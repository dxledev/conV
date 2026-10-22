#pragma once

#include "core/theme.h"
#include "theme_watcher.h"
#include <QMainWindow>
#include <QTimer>
#include <functional>

class QStackedWidget;
class QLabel;

namespace conv {

class ConvertPage;
struct ModalRequest;
class ModalCard;
class RailTab;

/** @brief Main window: tab rail, converter pages, in-window modals, live theme. */
class Window : public QMainWindow {
    Q_OBJECT
public:
    Window(QString configDirectory, Theme theme, const QStringList &files, QWidget *parent = nullptr);
    /** @brief Page index for a tab id, used by tests. */
    ConvertPage *page(int index) const;
    ThemeWatcher *watcher() const { return themeWatcher_; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QString configDirectory_;
    ThemeWatcher *themeWatcher_;
    QWidget *canvas_ = nullptr;
    QWidget *body_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QLabel *themeLabel_ = nullptr;
    QVector<RailTab *> tabs_;
    ModalCard *modal_ = nullptr;
    QTimer saveTimer_;

    void buildInterface();
    void applyTheme();
    void select(int index);
    void present(const ModalRequest &request, std::function<void(int)> done);
    void routeFiles(const QStringList &files);
    void saveSettings();
};

}
