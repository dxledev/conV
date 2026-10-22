/**
 * @file convert_page.cpp
 * @brief Queue, settings, and process runner for one conversion category.
 *
 * Jobs run one at a time through QProcess. Each writes to a hidden temporary file
 * beside its destination and renames it only on success, so a failed or cancelled
 * run never damages an existing file, and the source is never an output target.
 */
#include "convert_page.h"
#include "desktop_file_picker.h"
#include "widgets.h"

#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSlider>
#include <QStackedLayout>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace conv {
namespace {

QString sizeText(qint64 bytes) {
    const char *units[] = {"B", "KB", "MB", "GB"};
    double value = bytes;
    int unit = 0;
    while (value >= 1024 && unit < 3) {
        value /= 1024;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : 1) + " " + units[unit];
}

void setTone(QLabel *label, const char *tone) {
    if (label->property("tone").toString() == tone)
        return;
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

Glyph glyphFor(const QString &id) {
    if (id == "photo") return Glyph::Photo;
    if (id == "video") return Glyph::Video;
    if (id == "audio") return Glyph::Audio;
    return Glyph::Document;
}

QString acceptedList(const Category &category) {
    return category.inputs.join(" · ").toUpper();
}

}

/** @brief A queue entry: type glyph, name, size, progress, status, and remove button. */
class FileRow : public QWidget {
    Q_OBJECT
public:
    FileRow(const QString &path, const Category &category, QWidget *parent = nullptr) : QWidget(parent), glyph_(glyphFor(category.id)) {
        setFixedHeight(66);
        const QFileInfo info(path);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(58, 8, 10, 8);
        layout->setSpacing(12);
        auto *text = new QVBoxLayout;
        text->setSpacing(3);
        name_ = new QLabel;
        name_->setFont(uiFont(10.5, true));
        name_->setMinimumWidth(40);
        fullName_ = info.fileName();
        meta_ = new QLabel(QString("%1 · %2").arg(info.suffix().toUpper(), sizeText(info.size())));
        meta_->setFont(uiFont(9));
        meta_->setProperty("tone", "muted");
        text->addWidget(name_);
        text->addWidget(meta_);
        layout->addLayout(text, 1);
        progress_ = new ProgressStrip;
        progress_->setFixedWidth(104);
        progress_->hide();
        layout->addWidget(progress_);
        status_ = new QLabel;
        status_->setFont(uiFont(9, true));
        status_->setMinimumWidth(78);
        status_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(status_);
        details = new RetroButton("Details");
        details->setChip(true);
        details->hide();
        layout->addWidget(details);
        remove = new RetroButton("");
        remove->setChip(true);
        remove->setGlyph(Glyph::Close);
        remove->setFixedWidth(36);
        remove->setAccessibleName("Remove from queue");
        layout->addWidget(remove);
        setState("QUEUED", "muted");
    }

    void setState(const QString &text, const char *tone, double progress = 0, bool showProgress = false) {
        tone_ = tone;
        status_->setText(text);
        setTone(status_, tone);
        progress_->setVisible(showProgress);
        progress_->setRole(QString(tone) == "danger" ? "danger" : "accent");
        progress_->setValue(progress);
        update();
    }
    void setProgress(double value) { progress_->setValue(value); }
    void setMeta(const QString &text) { meta_->setText(text); }

    RetroButton *details = nullptr, *remove = nullptr;

protected:
    void resizeEvent(QResizeEvent *) override {
        name_->setText(QFontMetrics(name_->font()).elidedText(fullName_, Qt::ElideMiddle, std::max(40, name_->width())));
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
        painter.fillPath(chamfer(box, 8), themeColor("surface_alt"));
        const QString tone = tone_;
        const QColor bar = tone == "danger" ? activeTheme().dangerColor : tone == "accent" ? themeColor("accent") : themeColor("border");
        painter.fillRect(QRectF(1, 9, 4, height() - 18), bar);
        paintGlyph(painter, glyph_, QRectF(20, height() / 2.0 - 12, 24, 24), tone == "muted" ? themeColor("muted") : bar);
    }

private:
    Glyph glyph_;
    QString fullName_, tone_ = "muted";
    QLabel *name_, *meta_, *status_;
    ProgressStrip *progress_;
};

ConvertPage::ConvertPage(const Category &category, QWidget *parent)
    : QWidget(parent), category_(category), tool_(categoryTool(category)), format_(category.outputs.first()) {
    setAcceptDrops(true);
    buildInterface();
    refreshControls();
}

ConvertPage::~ConvertPage() {
    if (process_) {
        process_->disconnect(this);
        process_->kill();
        process_->waitForFinished(1000);
    }
    for (const auto &job : jobs_) {
        if (!job.temp.isEmpty())
            QFile::remove(job.temp);
    }
}

void ConvertPage::buildInterface() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(16);

    // Header: title, accepted formats, and tool state.
    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    auto *title = new QLabel(category_.title.toUpper() + " // CONVERTER");
    QFont titleFont = uiFont(19, true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    title->setFont(titleFont);
    auto *accepts = new QLabel(QString("Reads %1").arg(acceptedList(category_)));
    accepts->setFont(uiFont(8.5));
    accepts->setProperty("tone", "muted");
    accepts->setWordWrap(true);
    titles->addWidget(title);
    titles->addWidget(accepts);
    header->addLayout(titles, 1);
    auto *toolState = new QLabel(tool_.isEmpty() ? QString("▲ %1 MISSING").arg(category_.tool.toUpper())
                                                 : QString("● %1 READY").arg(QFileInfo(tool_).fileName().toUpper()));
    toolState->setFont(uiFont(9, true));
    toolState->setProperty("tone", tool_.isEmpty() ? "danger" : "accent");
    header->addWidget(toolState, 0, Qt::AlignTop);
    root->addLayout(header);

    // Queue.
    queuePanel_ = new Panel("Queue");
    auto *queueLayout = new QVBoxLayout(queuePanel_);
    queueLayout->setContentsMargins(14, 22, 14, 14);
    queueStack_ = new QStackedLayout;
    queueLayout->addLayout(queueStack_);

    auto *hint = new QWidget;
    // The empty queue is itself a button: click it (or press Enter/Space) to open the file chooser.
    hint->setProperty("hintArea", true);
    hint->setFocusPolicy(Qt::StrongFocus);
    hint->installEventFilter(this);
    if (!tool_.isEmpty())
        hint->setCursor(Qt::PointingHandCursor);
    auto *hintLayout = new QVBoxLayout(hint);
    hintLayout->setAlignment(Qt::AlignCenter);
    hintLayout->setSpacing(10);
    auto *hintIcon = new QWidget;
    hintIcon->setFixedSize(46, 46);
    hintIcon->installEventFilter(this);
    hintIcon->setProperty("hintGlyph", true);
    hintLayout->addWidget(hintIcon, 0, Qt::AlignHCenter);
    auto *hintTitle = new QLabel(tool_.isEmpty() ? "TOOL NOT INSTALLED" : "DROP FILES HERE");
    QFont hintFont = uiFont(14, true);
    hintFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
    hintTitle->setFont(hintFont);
    hintTitle->setAlignment(Qt::AlignCenter);
    auto *hintBody = new QLabel(tool_.isEmpty()
        ? QString("'%1' was not found on your PATH.\nInstall it, then restart conV to enable %2 conversion.").arg(category_.tool, category_.title.toLower())
        : "click here, press ADD FILES, or Ctrl+O");
    hintBody->setFont(uiFont(10));
    hintBody->setProperty("tone", "muted");
    hintBody->setAlignment(Qt::AlignCenter);
    hintBody->setWordWrap(true);
    hintLayout->addWidget(hintTitle);
    hintLayout->addWidget(hintBody);
    queueStack_->addWidget(hint);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *list = new QWidget;
    rows_ = new QVBoxLayout(list);
    rows_->setContentsMargins(0, 0, 4, 0);
    rows_->setSpacing(8);
    rows_->addStretch(1);
    scroll->setWidget(list);
    queueStack_->addWidget(scroll);
    root->addWidget(queuePanel_, 1);

    // Settings.
    auto *settingsPanel = new Panel("Output");
    settings_ = settingsPanel;
    auto *settings = new QVBoxLayout(settingsPanel);
    settings->setContentsMargins(18, 26, 18, 16);
    settings->setSpacing(10);

    auto *formatRow = new QWidget;
    formatRow_ = formatRow;
    auto *formatLayout = new FlowLayout(formatRow, 8);
    flow_ = formatLayout;
    formats_ = new QButtonGroup(this);
    for (const auto &extension : category_.outputs) {
        auto *chip = new RetroButton(extension);
        chip->setChip(true);
        chip->setCheckable(true);
        chip->setChecked(extension == format_);
        formats_->addButton(chip);
        formatLayout->addWidget(chip);
        connect(chip, &QAbstractButton::clicked, this, [this, extension] {
            format_ = extension;
            emit changed();
        });
    }
    auto *formatLabel = new QLabel("FORMAT");
    formatLabel->setFont(uiFont(8.5, true));
    formatLabel->setProperty("tone", "muted");
    settings->addWidget(formatLabel);
    settings->addWidget(formatRow);

    if (category_.hasQuality) {
        auto *qualityRow = new QHBoxLayout;
        auto *qualityLabel = new QLabel(category_.qualityLabel.toUpper());
        qualityLabel->setFont(uiFont(8.5, true));
        qualityLabel->setProperty("tone", "muted");
        qualityLabel->setFixedWidth(84);
        quality_slider_ = new QSlider(Qt::Horizontal);
        quality_slider_->setRange(1, 100);
        quality_slider_->setValue(quality_);
        quality_slider_->setFocusPolicy(Qt::StrongFocus);
        qualityValue_ = new QLabel(QString::number(quality_));
        qualityValue_->setFont(uiFont(10, true));
        qualityValue_->setFixedWidth(34);
        qualityValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        qualityRow->addWidget(qualityLabel);
        qualityRow->addWidget(quality_slider_, 1);
        qualityRow->addWidget(qualityValue_);
        settings->addLayout(qualityRow);
        connect(quality_slider_, &QSlider::valueChanged, this, [this](int value) {
            quality_ = value;
            qualityValue_->setText(QString::number(value));
            emit changed();
        });
    }

    auto *saveRow = new QHBoxLayout;
    saveRow->setSpacing(10);
    auto *saveLabel = new QLabel("SAVE TO");
    saveLabel->setFont(uiFont(8.5, true));
    saveLabel->setProperty("tone", "muted");
    saveLabel->setFixedWidth(84);
    same_ = new RetroButton("Next to source");
    same_->setChip(true);
    same_->setCheckable(true);
    same_->setChecked(true);
    folder_ = new RetroButton("Folder…");
    folder_->setChip(true);
    folder_->setGlyph(Glyph::Folder);
    directoryLabel_ = new QLabel;
    directoryLabel_->setFont(uiFont(9));
    directoryLabel_->setProperty("tone", "muted");
    directoryLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    saveRow->addWidget(saveLabel);
    saveRow->addWidget(same_);
    saveRow->addWidget(folder_);
    saveRow->addWidget(directoryLabel_, 1);
    settings->addLayout(saveRow);
    root->addWidget(settingsPanel);
    settingsPanel->installEventFilter(this);

    connect(same_, &QAbstractButton::clicked, this, [this] {
        outputDirectory_.clear();
        refreshControls();
        emit changed();
    });
    connect(folder_, &QAbstractButton::clicked, this, &ConvertPage::chooseDirectory);

    // Actions.
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    add_ = new RetroButton("Add files");
    add_->setGlyph(Glyph::Add);
    clear_ = new RetroButton("Clear");
    clear_->setGlyph(Glyph::Clear);
    status_ = new QLabel;
    status_->setFont(uiFont(9.5));
    status_->setProperty("tone", "muted");
    status_->setMinimumWidth(80);
    convert_ = new RetroButton("Convert");
    convert_->setPrimary(true);
    convert_->setGlyph(Glyph::Play);
    actions->addWidget(add_);
    actions->addWidget(clear_);
    actions->addWidget(status_, 1);
    actions->addWidget(convert_);
    root->addLayout(actions);

    connect(add_, &QAbstractButton::clicked, this, &ConvertPage::browse);
    connect(clear_, &QAbstractButton::clicked, this, &ConvertPage::clearFinished);
    connect(convert_, &QAbstractButton::clicked, this, [this] { running_ ? cancel() : start(); });
}

bool ConvertPage::eventFilter(QObject *watched, QEvent *event) {
    if (watched->property("hintArea").toBool() && toolAvailable()) {
        const auto type = event->type();
        if (type == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            browse();
            return true;
        }
        if (type == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
                browse();
                return true;
            }
        }
        if (type == QEvent::Paint || type == QEvent::FocusIn || type == QEvent::FocusOut) {
            static_cast<QWidget *>(watched)->update();
        }
    }
    if (watched == settings_ && event->type() == QEvent::Resize) {
        // Layouts inside a stacked widget do not propagate height-for-width, so size the chip row here.
        const int height = flow_->heightForWidth(settings_->width() - 36);
        if (height != formatRow_->height() || formatRow_->minimumHeight() != height)
            formatRow_->setFixedHeight(height);
    }
    if (event->type() == QEvent::Paint && watched->property("hintGlyph").toBool()) {
        auto *widget = static_cast<QWidget *>(watched);
        QPainter painter(widget);
        paintGlyph(painter, toolAvailable() ? Glyph::Drop : Glyph::Warn, widget->rect(),
                   toolAvailable() ? themeColor("accent") : activeTheme().dangerColor);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ConvertPage::setStatus(const QString &text, const char *tone) {
    status_->setText(QFontMetrics(status_->font()).elidedText(text, Qt::ElideRight, std::max(80, status_->width())));
    status_->setToolTip({});
    setTone(status_, tone);
}

void ConvertPage::refreshControls() {
    const bool enabled = toolAvailable();
    add_->setEnabled(enabled && !running_);
    clear_->setEnabled(!running_ && !jobs_.isEmpty());
    convert_->setEnabled(enabled && !jobs_.isEmpty());
    convert_->setText(running_ ? "Cancel" : "Convert");
    convert_->setGlyph(running_ ? Glyph::Stop : Glyph::Play);
    convert_->setPrimary(!running_);
    settings_->setEnabled(enabled && !running_);
    for (auto *button : formats_->buttons())
        button->setChecked(button->text() == format_);
    same_->setChecked(outputDirectory_.isEmpty());
    directoryLabel_->setText(outputDirectory_.isEmpty()
        ? "Same folder."
        : QFontMetrics(directoryLabel_->font()).elidedText(outputDirectory_, Qt::ElideMiddle, std::max(120, directoryLabel_->width())));
    queueStack_->setCurrentIndex(jobs_.isEmpty() ? 0 : 1);
    queuePanel_->setDashed(jobs_.isEmpty() && enabled);
    queuePanel_->setCaption(jobs_.isEmpty() ? "Queue" : QString("Queue · %1").arg(jobs_.size()));
}

QString ConvertPage::selectedFormat() const {
    return format_;
}

int ConvertPage::addFiles(const QStringList &paths) {
    if (running_ || !toolAvailable())
        return 0;
    QSet<QString> known;
    for (const auto &job : jobs_)
        known.insert(job.input);
    int added = 0, skipped = 0;
    for (const auto &raw : paths) {
        const QFileInfo info(raw);
        const auto path = info.absoluteFilePath();
        if (!info.isFile() || !accepts(category_, path)) {
            ++skipped;
            continue;
        }
        if (known.contains(path))
            continue;
        known.insert(path);
        Job job;
        job.input = path;
        job.row = new FileRow(path, category_);
        rows_->insertWidget(rows_->count() - 1, job.row);
        connect(job.row->remove, &QAbstractButton::clicked, this, [this, row = job.row] { removeJob(row); });
        jobs_.append(job);
        ++added;
    }
    refreshControls();
    if (skipped)
        setStatus(QString("%1 added · %2 skipped (not a %3 file)").arg(added).arg(skipped).arg(category_.title.toLower()), added ? "muted" : "danger");
    else if (added)
        setStatus(QString("%1 file%2 queued").arg(added).arg(added == 1 ? "" : "s"));
    return added;
}

void ConvertPage::pick(bool directory) {
    if (!picker_) {
        picker_ = new DesktopFilePicker(this);
        connect(picker_, &DesktopFilePicker::failed, this, [this](const QString &message) {
            if (!showModal)
                return;
            ModalRequest request;
            request.title = "File picker unavailable";
            request.body = message;
            request.glyph = Glyph::Warn;
            request.choices = {{"Close", 0, true}};
            showModal(request, [](int) {});
        });
    }
    if (picker_->active())
        return;
    // Selections arrive from the system's configured file chooser through the XDG portal.
    disconnect(picker_, &DesktopFilePicker::selected, nullptr, nullptr);
    connect(picker_, &DesktopFilePicker::selected, this, [this, directory](const QStringList &paths) {
        if (directory) {
            outputDirectory_ = paths.first();
            refreshControls();
            emit changed();
        } else {
            addFiles(paths);
        }
    });
    picker_->open(directory ? "Choose output folder" : "Add " + category_.title.toLower() + " files", !directory, directory);
}

void ConvertPage::browse() {
    if (running_ || !toolAvailable())
        return;
    pick(false);
}

void ConvertPage::removeJob(FileRow *row) {
    if (running_)
        return;
    for (int index = 0; index < jobs_.size(); ++index) {
        if (jobs_[index].row == row) {
            jobs_.removeAt(index);
            break;
        }
    }
    row->deleteLater();
    row->hide();
    rows_->removeWidget(row);
    refreshControls();
}

void ConvertPage::clearFinished() {
    if (running_)
        return;
    for (auto &job : jobs_) {
        job.row->hide();
        rows_->removeWidget(job.row);
        job.row->deleteLater();
    }
    jobs_.clear();
    refreshControls();
    setStatus({});
}

void ConvertPage::chooseDirectory() {
    pick(true);
}

void ConvertPage::planOutputs() {
    QSet<QString> claimed;
    for (auto &job : jobs_) {
        QString output = outputPath(job.input, outputDirectory_, selectedFormat());
        // Two inputs that differ only by extension would otherwise target one file.
        int suffix = 1;
        const QFileInfo info(output);
        while (claimed.contains(output))
            output = QString("%1/%2 (%3).%4").arg(info.absolutePath(), info.completeBaseName()).arg(suffix++).arg(info.suffix());
        claimed.insert(output);
        job.output = output;
    }
}

void ConvertPage::start() {
    if (running_ || jobs_.isEmpty() || !toolAvailable())
        return;
    planOutputs();
    QStringList existing;
    for (const auto &job : jobs_) {
        if (QFileInfo::exists(job.output))
            existing.append(QFileInfo(job.output).fileName());
    }
    if (existing.isEmpty() || !showModal) {
        begin(true);
        return;
    }
    ModalRequest request;
    request.title = existing.size() == 1 ? "File already exists" : QString("%1 files already exist").arg(existing.size());
    request.body = existing.size() == 1
        ? QString("“%1” is already in the destination. Replace it, or keep both?").arg(existing.first())
        : "Some converted files already exist in the destination. Replace them, or keep both?";
    request.detail = existing.mid(0, 12).join('\n') + (existing.size() > 12 ? QString("\n… and %1 more").arg(existing.size() - 12) : QString());
    request.glyph = Glyph::Warn;
    request.choices = {{"Cancel", 0, false}, {"Keep both", 1, false}, {"Replace", 2, true}};
    showModal(request, [this](int id) {
        if (id == 1) {
            for (auto &job : jobs_)
                job.output = uniquePath(job.output);
            begin(false);
        } else if (id == 2) {
            begin(true);
        }
    });
}

void ConvertPage::begin(bool) {
    running_ = true;
    cancelRequested_ = false;
    current_ = -1;
    for (auto &job : jobs_) {
        job.state = State::Queued;
        job.log.clear();
        job.row->details->hide();
        job.row->setState("QUEUED", "muted");
        job.row->remove->setEnabled(false);
    }
    refreshControls();
    emit busyChanged(true);
    next();
}

void ConvertPage::next() {
    ++current_;
    if (cancelRequested_ || current_ >= jobs_.size()) {
        running_ = false;
        for (auto &job : jobs_) {
            if (job.state == State::Queued) {
                job.state = State::Cancelled;
                job.row->setState("CANCELLED", "muted");
            }
            job.row->remove->setEnabled(true);
        }
        updateSummary();
        refreshControls();
        emit busyChanged(false);
        emit changed();
        return;
    }
    auto &job = jobs_[current_];
    job.state = State::Running;
    job.duration = 0;
    const QFileInfo target(job.output);
    if (!QDir().mkpath(target.absolutePath())) {
        job.state = State::Failed;
        job.log = "Cannot create the output folder: " + target.absolutePath();
        job.row->setState("FAILED", "danger");
        job.row->details->show();
        connect(job.row->details, &QAbstractButton::clicked, this, [this, index = current_] { showFailure(index); }, Qt::UniqueConnection);
        next();
        return;
    }
    job.temp = target.absolutePath() + "/.conv-part-" + target.fileName();
    QFile::remove(job.temp);
    const auto command = buildCommand(category_, job.input, job.temp, quality_);
    job.row->setState("0%", "accent", category_.id == "photo" || category_.id == "document" ? -1 : 0, true);
    setStatus(QString("Converting %1 of %2 · %3").arg(current_ + 1).arg(jobs_.size()).arg(QFileInfo(job.input).fileName()), "accent");
    stdoutBuffer_.clear();
    process_ = new QProcess(this);
    process_->setProgram(command.program);
    process_->setArguments(command.arguments);
    connect(process_, &QProcess::readyReadStandardOutput, this, &ConvertPage::readOutput);
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        auto &job = jobs_[current_];
        job.log.append(QString::fromLocal8Bit(process_->readAllStandardError()));
        if (job.log.size() > 12000)
            job.log = job.log.right(8000);
        if (job.duration <= 0) {
            static const QRegularExpression pattern(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))");
            const auto match = pattern.match(job.log);
            if (match.hasMatch())
                job.duration = match.captured(1).toInt() * 3600 + match.captured(2).toInt() * 60 + match.captured(3).toDouble();
        }
    });
    connect(process_, &QProcess::finished, this, &ConvertPage::finishJob);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && process_) {
            jobs_[current_].log += "Could not start " + process_->program();
            finishJob(-1, QProcess::CrashExit);
        }
    });
    process_->start();
}

void ConvertPage::readOutput() {
    stdoutBuffer_.append(QString::fromLocal8Bit(process_->readAllStandardOutput()));
    auto &job = jobs_[current_];
    int newline;
    while ((newline = stdoutBuffer_.indexOf('\n')) >= 0) {
        const auto line = stdoutBuffer_.left(newline);
        stdoutBuffer_.remove(0, newline + 1);
        const double seconds = progressSeconds(line);
        if (seconds >= 0 && job.duration > 0) {
            const double fraction = std::clamp(seconds / job.duration, 0.0, 1.0);
            job.row->setState(QString("%1%").arg(qRound(fraction * 100)), "accent", fraction, true);
        }
    }
}

void ConvertPage::finishJob(int exitCode, QProcess::ExitStatus status) {
    if (!process_ || current_ < 0 || current_ >= jobs_.size())
        return;
    auto *finished = process_;
    process_ = nullptr;
    finished->disconnect(this);
    finished->deleteLater();
    auto &job = jobs_[current_];
    const bool success = !cancelRequested_ && status == QProcess::NormalExit && exitCode == 0 && QFileInfo(job.temp).size() > 0;
    bool placed = false;
    if (success) {
        QFile::remove(job.output);
        placed = QFile::rename(job.temp, job.output);
        if (!placed)
            job.log += "\nCould not move the result into place: " + job.output;
    }
    QFile::remove(job.temp);
    job.temp.clear();
    if (cancelRequested_) {
        job.state = State::Cancelled;
        job.row->setState("CANCELLED", "muted");
    } else if (placed) {
        job.state = State::Done;
        job.row->setState("DONE", "accent");
        job.row->setMeta(QString("→ %1 · %2").arg(QFileInfo(job.output).fileName(), sizeText(QFileInfo(job.output).size())));
    } else {
        job.state = State::Failed;
        job.row->setState("FAILED", "danger");
        job.row->details->show();
        connect(job.row->details, &QAbstractButton::clicked, this, [this, index = current_] { showFailure(index); }, Qt::UniqueConnection);
    }
    next();
}

void ConvertPage::cancel() {
    if (!running_)
        return;
    cancelRequested_ = true;
    setStatus("Cancelling…");
    if (process_)
        process_->kill();
    else
        next();
}

void ConvertPage::showFailure(int index) {
    if (index < 0 || index >= jobs_.size() || !showModal)
        return;
    ModalRequest request;
    request.title = "Conversion failed";
    request.body = QString("“%1” could not be converted to %2.").arg(QFileInfo(jobs_[index].input).fileName(), format_.toUpper());
    request.detail = jobs_[index].log.trimmed().right(2000);
    request.glyph = Glyph::Warn;
    request.choices = {{"Close", 0, true}};
    showModal(request, [](int) {});
}

void ConvertPage::updateSummary() {
    int done = 0, failed = 0;
    for (const auto &job : jobs_) {
        done += job.state == State::Done;
        failed += job.state == State::Failed;
    }
    if (cancelRequested_)
        setStatus(QString("Cancelled · %1 converted").arg(done));
    else if (failed)
        setStatus(QString("%1 converted · %2 failed").arg(done).arg(failed), "danger");
    else
        setStatus(QString("All %1 converted").arg(done), "accent");
}

QJsonObject ConvertPage::state() const {
    return {{"format", format_}, {"quality", quality_}, {"directory", outputDirectory_}};
}

void ConvertPage::restore(const QJsonObject &saved) {
    const auto format = saved.value("format").toString();
    if (category_.outputs.contains(format))
        format_ = format;
    quality_ = std::clamp(saved.value("quality").toInt(quality_), 1, 100);
    if (quality_slider_)
        quality_slider_->setValue(quality_);
    const auto directory = saved.value("directory").toString();
    outputDirectory_ = QFileInfo(directory).isDir() ? directory : QString();
    refreshControls();
}

void ConvertPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    refreshControls();
}

void ConvertPage::dragEnterEvent(QDragEnterEvent *event) {
    if (!running_ && toolAvailable() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        queuePanel_->setDashed(true);
    }
}

void ConvertPage::dragLeaveEvent(QDragLeaveEvent *) {
    refreshControls();
}

void ConvertPage::dropEvent(QDropEvent *event) {
    QStringList paths;
    for (const auto &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    event->acceptProposedAction();
    addFiles(paths);
    refreshControls();
}

}

#include "convert_page.moc"
