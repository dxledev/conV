#pragma once

#include "core/converter.h"
#include "modal.h"
#include <QJsonObject>
#include <QProcess>
#include <QVector>
#include <QWidget>
#include <functional>

class QLabel;
class QScrollArea;
class QSlider;
class QStackedLayout;
class QVBoxLayout;
class QButtonGroup;

namespace conv {

class DesktopFilePicker;
class FileRow;
class FlowLayout;
class Panel;
class RetroButton;

/** @brief One converter tab: queue, output settings, and sequential conversion. */
class ConvertPage : public QWidget {
    Q_OBJECT
public:
    explicit ConvertPage(const Category &category, QWidget *parent = nullptr);
    ~ConvertPage() override;

    /** @brief Present a modal over the whole window; installed by the window. */
    std::function<void(const ModalRequest &, std::function<void(int)>)> showModal;
    bool toolAvailable() const { return !tool_.isEmpty(); }
    bool busy() const { return running_; }
    const Category &category() const { return category_; }
    /** @brief Add files that this page's converter accepts; returns how many were queued. */
    int addFiles(const QStringList &paths);
    /** @brief Open the system file picker. */
    void browse();
    /** @brief Start, or ask to start, converting the queue. */
    void start();
    QJsonObject state() const;
    void restore(const QJsonObject &state);

signals:
    void changed();
    void busyChanged(bool busy);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class State { Queued, Running, Done, Failed, Cancelled };
    struct Job {
        QString input, output, temp, log;
        FileRow *row = nullptr;
        State state = State::Queued;
        double duration = 0;
    };

    Category category_;
    QString tool_;
    QVector<Job> jobs_;
    QString format_;
    QString outputDirectory_;
    int quality_ = 80;
    bool running_ = false, cancelRequested_ = false;
    int current_ = -1;
    QProcess *process_ = nullptr;
    DesktopFilePicker *picker_ = nullptr;
    QString stdoutBuffer_;

    QLabel *status_ = nullptr;
    QLabel *qualityValue_ = nullptr;
    QLabel *directoryLabel_ = nullptr;
    QSlider *quality_slider_ = nullptr;
    QButtonGroup *formats_ = nullptr;
    QStackedLayout *queueStack_ = nullptr;
    QVBoxLayout *rows_ = nullptr;
    Panel *queuePanel_ = nullptr;
    RetroButton *add_ = nullptr, *clear_ = nullptr, *convert_ = nullptr, *folder_ = nullptr, *same_ = nullptr;
    QWidget *settings_ = nullptr;
    QWidget *formatRow_ = nullptr;
    FlowLayout *flow_ = nullptr;

    void buildInterface();
    void setStatus(const QString &text, const char *tone = "muted");
    void refreshControls();
    void removeJob(FileRow *row);
    void clearFinished();
    void planOutputs();
    void begin(bool overwrite);
    void next();
    void finishJob(int exitCode, QProcess::ExitStatus status);
    void readOutput();
    void cancel();
    void chooseDirectory();
    void pick(bool directory);
    void showFailure(int index);
    void updateSummary();
    QString selectedFormat() const;
};

}
