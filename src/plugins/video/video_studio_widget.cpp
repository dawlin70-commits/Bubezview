/**
 * @file video_studio_widget.cpp
 * @brief Implementation of the modular Video Studio side panel inside bubez_video.so.
 */

#include "video_studio_widget.h"
#include "core/services/theme_manager.h"
#include "core/system/package_manager_helper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QListView>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <cmath>

namespace {
// Cap the rolling FFmpeg log buffer so long transcodes never accumulate unbounded RAM
constexpr qsizetype kMaxProcessLogChars = 4096;
} // namespace

VideoStudioWidget::VideoStudioWidget(QWidget *parent)
    : IVideoStudioWidget(parent)
{
    setObjectName(QStringLiteral("VideoStudioWidget"));
    setFixedWidth(292);
    setupUi();
    updateTheme();
}

VideoStudioWidget::~VideoStudioWidget()
{
    // Terminate any background FFmpeg job without touching child widgets during destruction
    stopActiveProcess(true);
}

void VideoStudioWidget::setTargetVideo(const QString &filePath, qint64 durationMs)
{
    m_videoPath = filePath;
    m_currentPosMs = 0;
    m_durationMs = qMax<qint64>(0, durationMs);
    m_endAutoFilled = true;

    if (m_editStart) m_editStart->setText(QStringLiteral("00:00:00.0"));
    if (m_editEnd) {
        m_editEnd->setText(m_durationMs > 0 ? formatTimestamp(m_durationMs) : QStringLiteral("00:00:10.0"));
    }
    if (m_lblStatus) {
        const QString name = QFileInfo(filePath).fileName();
        m_lblStatus->setText(name.isEmpty() ? tr("Ready") : tr("Ready: %1").arg(name));
    }
}

void VideoStudioWidget::setPlaybackPosition(qint64 positionMs, qint64 durationMs)
{
    m_currentPosMs = qMax<qint64>(0, positionMs);
    if (durationMs > 0 && m_durationMs != durationMs) {
        m_durationMs = durationMs;
        // Only auto-populate the end timestamp if the user has not customized it yet
        if (m_editEnd && m_endAutoFilled) {
            m_editEnd->setText(formatTimestamp(m_durationMs));
        }
    }
}

void VideoStudioWidget::setOnOutputCreated(std::function<void(const QString &outputPath)> cb)
{
    m_onOutputCreated = std::move(cb);
}

void VideoStudioWidget::stopActiveProcess(bool removePartialOutput)
{
    if (m_activeProcess) {
        m_activeProcess->disconnect(this);
        if (m_activeProcess->state() != QProcess::NotRunning) {
            m_activeProcess->terminate();
            if (!m_activeProcess->waitForFinished(1500)) {
                m_activeProcess->kill();
                m_activeProcess->waitForFinished(500);
            }
        }
        m_activeProcess->deleteLater();
        m_activeProcess = nullptr;
    }

    if (removePartialOutput && !m_pendingOutputPath.isEmpty()
        && !isSameFilePath(m_pendingOutputPath, m_videoPath)
        && QFileInfo::exists(m_pendingOutputPath)) {
        QFile::remove(m_pendingOutputPath);
    }
    m_pendingOutputPath.clear();
    m_processLog.clear();
}

void VideoStudioWidget::cancelActiveOperations()
{
    // Avoid overwriting the status label when no FFmpeg job is currently running
    if (!m_activeProcess) {
        return;
    }
    stopActiveProcess(true);
    setBusy(false, tr("Cancelled"));
}

QString VideoStudioWidget::formatTimestamp(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 totalTenths = ms / 100;
    const int tenths = static_cast<int>(totalTenths % 10);
    const qint64 totalSec = totalTenths / 10;
    const int sec = static_cast<int>(totalSec % 60);
    const int min = static_cast<int>((totalSec / 60) % 60);
    const int hrs = static_cast<int>(totalSec / 3600);
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hrs, 2, 10, QLatin1Char('0'))
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'))
        .arg(tenths);
}

bool VideoStudioWidget::parseTimestampMs(const QString &text, qint64 *outMs)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // Accept HH:MM:SS(.fff), MM:SS(.fff), or plain seconds SS(.fff)
    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() < 1 || parts.size() > 3) {
        return false;
    }

    bool ok = false;
    double seconds = parts.last().toDouble(&ok);
    if (!ok || !std::isfinite(seconds) || seconds < 0.0) {
        return false;
    }

    qint64 totalMinutes = 0;
    if (parts.size() >= 2) {
        const qint64 mins = parts[parts.size() - 2].toLongLong(&ok);
        if (!ok || mins < 0 || (parts.size() == 3 && mins >= 60) || seconds >= 60.0) {
            return false;
        }
        totalMinutes += mins;
    }
    if (parts.size() == 3) {
        const qint64 hrs = parts[0].toLongLong(&ok);
        if (!ok || hrs < 0) {
            return false;
        }
        totalMinutes += hrs * 60;
    }

    if (outMs) {
        *outMs = totalMinutes * 60000 + static_cast<qint64>(std::llround(seconds * 1000.0));
    }
    return true;
}

bool VideoStudioWidget::isSameFilePath(const QString &pathA, const QString &pathB)
{
    if (pathA.isEmpty() || pathB.isEmpty()) {
        return false;
    }
    const QFileInfo fiA(pathA);
    const QFileInfo fiB(pathB);
    const QString normA = fiA.canonicalFilePath().isEmpty() ? fiA.absoluteFilePath() : fiA.canonicalFilePath();
    const QString normB = fiB.canonicalFilePath().isEmpty() ? fiB.absoluteFilePath() : fiB.canonicalFilePath();
    return normA == normB;
}

void VideoStudioWidget::setupUi()
{
    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("VideoStudioScroll"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(m_scrollArea);
    content->setObjectName(QStringLiteral("VideoStudioCanvas"));
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(12, 12, 12, 16);
    lay->setSpacing(12);

    // Status & progress header
    auto *statusCard = new QFrame(content);
    statusCard->setObjectName(QStringLiteral("VideoStudioCard"));
    auto *stLay = new QVBoxLayout(statusCard);
    stLay->setContentsMargins(10, 8, 10, 8);
    stLay->setSpacing(6);

    m_lblStatus = new QLabel(tr("Ready"), statusCard);
    m_lblStatus->setWordWrap(true);
    stLay->addWidget(m_lblStatus);

    auto *progRow = new QHBoxLayout();
    progRow->setSpacing(6);
    m_progressBar = new QProgressBar(statusCard);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    progRow->addWidget(m_progressBar, 1);

    m_btnCancel = new QPushButton(tr("Stop"), statusCard);
    m_btnCancel->setObjectName(QStringLiteral("VideoStudioDangerBtn"));
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setFixedHeight(24);
    m_btnCancel->setVisible(false);
    connect(m_btnCancel, &QPushButton::clicked, this, &VideoStudioWidget::cancelActiveOperations);
    progRow->addWidget(m_btnCancel);
    stLay->addLayout(progRow);
    lay->addWidget(statusCard);

    auto makeSectionCard = [&](const QString &title) -> std::pair<QFrame *, QVBoxLayout *> {
        auto *card = new QFrame(content);
        card->setObjectName(QStringLiteral("VideoStudioCard"));
        auto *cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 12);
        cardLay->setSpacing(8);
        auto *hdr = new QLabel(title, card);
        hdr->setObjectName(QStringLiteral("VideoStudioSectionTitle"));
        cardLay->addWidget(hdr);
        lay->addWidget(card);
        return {card, cardLay};
    };

    // 1. Trim / Cut
    {
        auto [card, cardLay] = makeSectionCard(tr("Trim & Cut"));
        auto *grid = new QGridLayout();
        grid->setSpacing(6);

        grid->addWidget(new QLabel(tr("Start:"), card), 0, 0);
        m_editStart = new QLineEdit(QStringLiteral("00:00:00.0"), card);
        m_editStart->setFixedHeight(28);
        grid->addWidget(m_editStart, 0, 1);
        auto *btnMarkIn = new QPushButton(tr("Now"), card);
        btnMarkIn->setObjectName(QStringLiteral("VideoStudioSecBtn"));
        btnMarkIn->setCursor(Qt::PointingHandCursor);
        btnMarkIn->setFixedHeight(28);
        btnMarkIn->setToolTip(tr("Set start time to current playback position"));
        connect(btnMarkIn, &QPushButton::clicked, this, [this]() {
            if (m_editStart) m_editStart->setText(formatTimestamp(m_currentPosMs));
        });
        grid->addWidget(btnMarkIn, 0, 2);

        grid->addWidget(new QLabel(tr("End:"), card), 1, 0);
        m_editEnd = new QLineEdit(QStringLiteral("00:00:10.0"), card);
        m_editEnd->setFixedHeight(28);
        connect(m_editEnd, &QLineEdit::textEdited, this, [this]() {
            m_endAutoFilled = false;
        });
        grid->addWidget(m_editEnd, 1, 1);
        auto *btnMarkOut = new QPushButton(tr("Now"), card);
        btnMarkOut->setObjectName(QStringLiteral("VideoStudioSecBtn"));
        btnMarkOut->setCursor(Qt::PointingHandCursor);
        btnMarkOut->setMinimumHeight(28);
        btnMarkOut->setToolTip(tr("Set end time to current playback position"));
        connect(btnMarkOut, &QPushButton::clicked, this, [this]() {
            if (m_editEnd) {
                m_endAutoFilled = false;
                m_editEnd->setText(formatTimestamp(m_currentPosMs));
            }
        });
        grid->addWidget(btnMarkOut, 1, 2);

        cardLay->addLayout(grid);

        m_chkLosslessTrim = new QCheckBox(tr("Lossless stream copy (instant)"), card);
        m_chkLosslessTrim->setChecked(true);
        cardLay->addWidget(m_chkLosslessTrim);

        auto *btnTrim = new QPushButton(tr("Export Trimmed Clip"), card);
        btnTrim->setObjectName(QStringLiteral("VideoStudioPrimaryBtn"));
        btnTrim->setCursor(Qt::PointingHandCursor);
        btnTrim->setMinimumHeight(30);
        connect(btnTrim, &QPushButton::clicked, this, &VideoStudioWidget::exportTrimmedVideo);
        m_actionButtons.append(btnTrim);
        cardLay->addWidget(btnTrim);
    }

    // 2. Frame Snapshot
    {
        auto [card, cardLay] = makeSectionCard(tr("Frame Snapshot"));
        auto *hint = new QLabel(tr("Save the exact frame at the current playback timestamp as a crisp image."), card);
        hint->setObjectName(QStringLiteral("VideoStudioHintLabel"));
        hint->setWordWrap(true);
        cardLay->addWidget(hint);

        auto *btnSnap = new QPushButton(tr("Capture Current Frame (PNG)"), card);
        btnSnap->setObjectName(QStringLiteral("VideoStudioSecBtn"));
        btnSnap->setCursor(Qt::PointingHandCursor);
        btnSnap->setMinimumHeight(30);
        connect(btnSnap, &QPushButton::clicked, this, &VideoStudioWidget::captureFrameSnapshot);
        m_actionButtons.append(btnSnap);
        cardLay->addWidget(btnSnap);
    }

    // 3. Extract Audio Track
    {
        auto [card, cardLay] = makeSectionCard(tr("Extract Audio"));
        m_comboAudioFormat = new QComboBox(card);
        m_comboAudioFormat->setView(new QListView(m_comboAudioFormat));
        m_comboAudioFormat->addItem(tr("MP3 (High Quality 320k)"), QStringLiteral("mp3"));
        m_comboAudioFormat->addItem(tr("FLAC (Lossless)"), QStringLiteral("flac"));
        m_comboAudioFormat->addItem(tr("WAV (Uncompressed PCM)"), QStringLiteral("wav"));
        m_comboAudioFormat->addItem(tr("Opus (Compact Web Audio)"), QStringLiteral("opus"));
        m_comboAudioFormat->setMinimumHeight(30);
        cardLay->addWidget(m_comboAudioFormat);

        auto *btnExtAudio = new QPushButton(tr("Extract Audio Track"), card);
        btnExtAudio->setObjectName(QStringLiteral("VideoStudioPrimaryBtn"));
        btnExtAudio->setCursor(Qt::PointingHandCursor);
        btnExtAudio->setMinimumHeight(30);
        connect(btnExtAudio, &QPushButton::clicked, this, &VideoStudioWidget::extractAudioTrack);
        m_actionButtons.append(btnExtAudio);
        cardLay->addWidget(btnExtAudio);
    }

    // 4. Transform & Optimize
    {
        auto [card, cardLay] = makeSectionCard(tr("Transform & Optimize"));

        cardLay->addWidget(new QLabel(tr("Rotation / Flip:"), card));
        m_comboRotate = new QComboBox(card);
        m_comboRotate->setView(new QListView(m_comboRotate));
        m_comboRotate->addItem(tr("Keep Original"), QString());
        m_comboRotate->addItem(tr("Rotate 90° Clockwise"), QStringLiteral("transpose=1"));
        m_comboRotate->addItem(tr("Rotate 90° Counter-Clockwise"), QStringLiteral("transpose=2"));
        m_comboRotate->addItem(tr("Rotate 180°"), QStringLiteral("transpose=1,transpose=1"));
        m_comboRotate->addItem(tr("Flip Horizontal"), QStringLiteral("hflip"));
        m_comboRotate->addItem(tr("Flip Vertical"), QStringLiteral("vflip"));
        m_comboRotate->setMinimumHeight(28);
        cardLay->addWidget(m_comboRotate);

        cardLay->addWidget(new QLabel(tr("Resolution:"), card));
        m_comboScale = new QComboBox(card);
        m_comboScale->setView(new QListView(m_comboScale));
        m_comboScale->addItem(tr("Original Resolution"), QString());
        m_comboScale->addItem(tr("1080p (Full HD)"), QStringLiteral("scale=-2:1080"));
        m_comboScale->addItem(tr("720p (HD)"), QStringLiteral("scale=-2:720"));
        m_comboScale->addItem(tr("480p (Compact)"), QStringLiteral("scale=-2:480"));
        m_comboScale->setMinimumHeight(28);
        cardLay->addWidget(m_comboScale);

        cardLay->addWidget(new QLabel(tr("Playback Speed:"), card));
        m_comboSpeed = new QComboBox(card);
        m_comboSpeed->setView(new QListView(m_comboSpeed));
        m_comboSpeed->addItem(tr("1.0x (Normal)"), 1.0);
        m_comboSpeed->addItem(tr("0.5x (Slow Motion)"), 0.5);
        m_comboSpeed->addItem(tr("1.5x (Fast)"), 1.5);
        m_comboSpeed->addItem(tr("2.0x (Double Speed)"), 2.0);
        m_comboSpeed->setMinimumHeight(28);
        cardLay->addWidget(m_comboSpeed);

        m_chkMuteAudio = new QCheckBox(tr("Remove audio track (mute)"), card);
        cardLay->addWidget(m_chkMuteAudio);

        auto *btnTransform = new QPushButton(tr("Export Transformed Video"), card);
        btnTransform->setObjectName(QStringLiteral("VideoStudioPrimaryBtn"));
        btnTransform->setCursor(Qt::PointingHandCursor);
        btnTransform->setMinimumHeight(30);
        connect(btnTransform, &QPushButton::clicked, this, &VideoStudioWidget::exportTransformedVideo);
        m_actionButtons.append(btnTransform);
        cardLay->addWidget(btnTransform);
    }

    lay->addStretch(1);
    m_scrollArea->setWidget(content);
    outerLay->addWidget(m_scrollArea);
}

bool VideoStudioWidget::ensureFfmpegAvailable()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty()) {
        return true;
    }
    return PackageManagerHelper::installComponentInteractive(QStringLiteral("ffmpeg"), this);
}

void VideoStudioWidget::setBusy(bool busy, const QString &status)
{
    if (m_progressBar) {
        m_progressBar->setVisible(busy);
        if (busy) m_progressBar->setRange(0, 0);
    }
    if (m_btnCancel) m_btnCancel->setVisible(busy);
    for (QPushButton *btn : m_actionButtons) {
        if (btn) btn->setEnabled(!busy);
    }
    if (m_lblStatus && !status.isEmpty()) m_lblStatus->setText(status);
}

void VideoStudioWidget::runFfmpegJob(const QStringList &args, const QString &outputPath, const QString &busyMessage)
{
    if (m_activeProcess) {
        QMessageBox::information(this, tr("Operation in Progress"),
                                 tr("Please wait for the current video job to finish or stop it first."));
        return;
    }
    if (!ensureFfmpegAvailable()) {
        return;
    }

    m_pendingOutputPath = outputPath;
    m_processLog.clear();
    m_activeProcess = new QProcess(this);
    m_activeProcess->setProcessChannelMode(QProcess::MergedChannels);

    // Drain FFmpeg output incrementally and keep only a bounded tail for error diagnostics
    connect(m_activeProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        if (!m_activeProcess) return;
        m_processLog.append(QString::fromUtf8(m_activeProcess->readAllStandardOutput()));
        if (m_processLog.size() > kMaxProcessLogChars) {
            m_processLog = m_processLog.right(kMaxProcessLogChars);
        }
    });

    // Handle launch failures (e.g. FailedToStart) where QProcess::finished is never emitted
    connect(m_activeProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_activeProcess) {
            return;
        }
        const QString errText = m_activeProcess->errorString();
        stopActiveProcess(false);
        setBusy(false, tr("Failed to start FFmpeg."));
        QMessageBox::warning(this, tr("Video Studio Error"),
                             tr("Could not start FFmpeg.\n\n%1").arg(errText));
    });

    connect(m_activeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (!m_activeProcess) return;
        m_processLog.append(QString::fromUtf8(m_activeProcess->readAllStandardOutput()));
        if (m_processLog.size() > kMaxProcessLogChars) {
            m_processLog = m_processLog.right(kMaxProcessLogChars);
        }
        const QString outLog = m_processLog;
        const QString outPath = m_pendingOutputPath;
        const bool succeeded = (exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(outPath));
        stopActiveProcess(!succeeded);

        if (succeeded) {
            setBusy(false, tr("Saved: %1").arg(QFileInfo(outPath).fileName()));
            if (m_onOutputCreated) {
                m_onOutputCreated(outPath);
            }
        } else {
            setBusy(false, tr("FFmpeg operation failed."));
            QMessageBox::warning(this, tr("Video Studio Error"),
                                 tr("Video processing failed.\n\n%1").arg(outLog.right(700).trimmed()));
        }
    });

    setBusy(true, busyMessage);
    m_activeProcess->start(QStringLiteral("ffmpeg"), args);
}

void VideoStudioWidget::exportTrimmedVideo()
{
    if (m_videoPath.isEmpty() || !m_editStart || !m_editEnd) return;

    // Validate start/end timestamps before prompting for output path
    qint64 startMs = 0;
    qint64 endMs = 0;
    const QString startStr = m_editStart->text().trimmed();
    const QString endStr = m_editEnd->text().trimmed();
    if (!parseTimestampMs(startStr, &startMs) || !parseTimestampMs(endStr, &endMs) || startMs >= endMs) {
        QMessageBox::warning(this, tr("Invalid Trim Range"),
                             tr("Please enter valid Start and End timestamps (HH:MM:SS.d) with Start earlier than End."));
        return;
    }

    const QFileInfo fi(m_videoPath);
    const QString srcSuffix = fi.suffix().isEmpty() ? QStringLiteral("mp4") : fi.suffix();
    const QString defaultOut = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                               + QStringLiteral("_trimmed.") + srcSuffix;
    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Save Trimmed Video"), defaultOut,
        tr("Video Files (*.%1 *.mp4 *.mkv *.webm);;All Files (*)").arg(srcSuffix));
    if (outPath.isEmpty()) return;

    if (isSameFilePath(outPath, m_videoPath)) {
        QMessageBox::warning(this, tr("Invalid Destination"),
                             tr("Please choose a different output file to avoid overwriting the source video."));
        return;
    }

    // Place -ss/-to before -i for fast input seeking
    QStringList args = {QStringLiteral("-y"), QStringLiteral("-ss"), startStr,
                        QStringLiteral("-to"), endStr,
                        QStringLiteral("-i"), m_videoPath};
    const bool isWebmOut = (QFileInfo(outPath).suffix().toLower() == QLatin1String("webm"));
    if (m_chkLosslessTrim && m_chkLosslessTrim->isChecked()) {
        args << QStringLiteral("-c") << QStringLiteral("copy");
    } else if (isWebmOut) {
        // WebM requires VP8/VP9/AV1 video and Opus/Vorbis audio
        args << QStringLiteral("-c:v") << QStringLiteral("libvpx-vp9")
             << QStringLiteral("-crf") << QStringLiteral("30")
             << QStringLiteral("-b:v") << QStringLiteral("0")
             << QStringLiteral("-c:a") << QStringLiteral("libopus");
    } else {
        args << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-crf") << QStringLiteral("20")
             << QStringLiteral("-c:a") << QStringLiteral("aac");
    }
    args << outPath;
    runFfmpegJob(args, outPath, tr("Exporting trimmed clip..."));
}

void VideoStudioWidget::captureFrameSnapshot()
{
    if (m_videoPath.isEmpty()) return;
    const QFileInfo fi(m_videoPath);
    const QString defaultOut = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                               + QStringLiteral("_frame.png");
    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Save Frame Snapshot"), defaultOut,
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"));
    if (outPath.isEmpty()) return;

    const QStringList args = {
        QStringLiteral("-y"),
        QStringLiteral("-ss"), formatTimestamp(m_currentPosMs),
        QStringLiteral("-i"), m_videoPath,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-q:v"), QStringLiteral("2"),
        outPath
    };
    runFfmpegJob(args, outPath, tr("Capturing video frame..."));
}

void VideoStudioWidget::extractAudioTrack()
{
    if (m_videoPath.isEmpty() || !m_comboAudioFormat) return;
    const QString fmt = m_comboAudioFormat->currentData().toString();
    const QFileInfo fi(m_videoPath);
    const QString defaultOut = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                               + QLatin1Char('.') + fmt;
    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Extract Audio Track"), defaultOut,
        tr("Audio (*.%1);;All Files (*)").arg(fmt));
    if (outPath.isEmpty()) return;

    if (isSameFilePath(outPath, m_videoPath)) {
        QMessageBox::warning(this, tr("Invalid Destination"),
                             tr("Please choose a different output file to avoid overwriting the source video."));
        return;
    }

    QStringList args = {QStringLiteral("-y"), QStringLiteral("-i"), m_videoPath, QStringLiteral("-vn")};
    if (fmt == QLatin1String("mp3")) {
        args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame") << QStringLiteral("-b:a") << QStringLiteral("320k");
    } else if (fmt == QLatin1String("flac")) {
        args << QStringLiteral("-c:a") << QStringLiteral("flac");
    } else if (fmt == QLatin1String("wav")) {
        args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
    } else {
        args << QStringLiteral("-c:a") << QStringLiteral("libopus") << QStringLiteral("-b:a") << QStringLiteral("160k");
    }
    args << outPath;
    runFfmpegJob(args, outPath, tr("Extracting audio track (%1)...").arg(fmt.toUpper()));
}

void VideoStudioWidget::exportTransformedVideo()
{
    if (m_videoPath.isEmpty()) return;

    QStringList vFilters;
    if (m_comboRotate) {
        const QString rot = m_comboRotate->currentData().toString();
        if (!rot.isEmpty()) vFilters.append(rot);
    }
    if (m_comboScale) {
        const QString sc = m_comboScale->currentData().toString();
        if (!sc.isEmpty()) vFilters.append(sc);
    }
    const double speed = m_comboSpeed ? m_comboSpeed->currentData().toDouble() : 1.0;
    const bool mute = m_chkMuteAudio && m_chkMuteAudio->isChecked();
    if (qAbs(speed - 1.0) > 0.01) {
        vFilters.append(QStringLiteral("setpts=%1*PTS").arg(1.0 / speed, 0, 'f', 3));
    }

    if (vFilters.isEmpty() && !mute) {
        QMessageBox::information(this, tr("No Transform Selected"),
                                 tr("Select at least one rotation, resolution, speed, or mute option."));
        return;
    }

    const QFileInfo fi(m_videoPath);
    const QString defaultOut = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                               + QStringLiteral("_edited.mp4");
    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Export Transformed Video"), defaultOut,
        tr("MP4 Video (*.mp4);;MKV Video (*.mkv);;WebM Video (*.webm)"));
    if (outPath.isEmpty()) return;

    if (isSameFilePath(outPath, m_videoPath)) {
        QMessageBox::warning(this, tr("Invalid Destination"),
                             tr("Please choose a different output file to avoid overwriting the source video."));
        return;
    }

    // Choose container-compatible codecs (WebM does not accept H.264/AAC)
    const bool isWebmOut = (QFileInfo(outPath).suffix().toLower() == QLatin1String("webm"));
    const bool srcIsWebm = (fi.suffix().toLower() == QLatin1String("webm"));
    QStringList args = {QStringLiteral("-y"), QStringLiteral("-i"), m_videoPath};
    if (!vFilters.isEmpty() || (isWebmOut && !srcIsWebm)) {
        if (!vFilters.isEmpty()) {
            args << QStringLiteral("-vf") << vFilters.join(QLatin1Char(','));
        }
        if (isWebmOut) {
            args << QStringLiteral("-c:v") << QStringLiteral("libvpx-vp9")
                 << QStringLiteral("-crf") << QStringLiteral("30")
                 << QStringLiteral("-b:v") << QStringLiteral("0");
        } else {
            args << QStringLiteral("-c:v") << QStringLiteral("libx264")
                 << QStringLiteral("-crf") << QStringLiteral("20")
                 << QStringLiteral("-preset") << QStringLiteral("fast");
        }
    } else {
        args << QStringLiteral("-c:v") << QStringLiteral("copy");
    }

    if (mute) {
        args << QStringLiteral("-an");
    } else if (qAbs(speed - 1.0) > 0.01) {
        args << QStringLiteral("-af") << QStringLiteral("atempo=%1").arg(speed, 0, 'f', 2)
             << QStringLiteral("-c:a") << (isWebmOut ? QStringLiteral("libopus") : QStringLiteral("aac"));
    } else if (isWebmOut && !srcIsWebm) {
        args << QStringLiteral("-c:a") << QStringLiteral("libopus");
    } else {
        args << QStringLiteral("-c:a") << QStringLiteral("copy");
    }
    args << outPath;
    runFfmpegJob(args, outPath, tr("Processing video transform..."));
}

void VideoStudioWidget::updateTheme()
{
    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = !ThemeManager::isDark(preset);
    const bool isZinc = (preset == ThemePreset::ZincGray);

    const QString bg = isLight ? QStringLiteral("#eef1f5")
                               : (isZinc ? QStringLiteral("#494a4f") : QStringLiteral("#181a1f"));
    const QString cardBg = isLight ? QStringLiteral("#ffffff")
                                   : (isZinc ? QStringLiteral("#3c3d42") : QStringLiteral("#1e232e"));
    const QString border = isLight ? QStringLiteral("rgba(15, 23, 42, 0.12)")
                                   : (isZinc ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(255, 255, 255, 0.09)"));
    const QString text = isLight ? QStringLiteral("#0f172a") : QStringLiteral("#f1f5f9");
    const QString subText = isLight ? QStringLiteral("#64748b") : QStringLiteral("#94a3b8");
    const QString inputBg = isLight ? QStringLiteral("#f1f5f9")
                                    : (isZinc ? QStringLiteral("#333438") : QStringLiteral("#141820"));
    const QString secBtnBg = isLight ? QStringLiteral("#e2e8f0")
                                     : (isZinc ? QStringLiteral("#56575e") : QStringLiteral("#283040"));
    const QString secBtnHover = isLight ? QStringLiteral("#cbd5e1")
                                        : (isZinc ? QStringLiteral("#63646c") : QStringLiteral("#334155"));

    const QString css = QStringLiteral(
        "QWidget#VideoStudioWidget, QScrollArea#VideoStudioScroll, QWidget#VideoStudioCanvas {"
        "   background-color: %1; color: %2; border: none;"
        "}"
        "QWidget#VideoStudioWidget { border-left: 1px solid %4; }"
        "QFrame#VideoStudioCard {"
        "   background-color: %3; border: 1px solid %4; border-radius: 12px;"
        "}"
        "QLabel { color: %2; background: transparent; border: none; font-size: 11.5px; }"
        "QLabel#VideoStudioSectionTitle { font-weight: 700; font-size: 12px; color: #38bdf8; }"
        "QLabel#VideoStudioHintLabel { color: %5; font-size: 11px; }"
        "QCheckBox { color: %2; font-size: 11.5px; spacing: 6px; }"
        "QLineEdit, QComboBox {"
        "   background-color: %6; color: %2; border: 1px solid %4; border-radius: 8px; padding: 3px 8px; font-size: 11.5px;"
        "}"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox QAbstractItemView {"
        "   background-color: %3; color: %2; border: 1px solid %4; selection-background-color: #0284c7; selection-color: #ffffff; outline: none;"
        "}"
        "QPushButton#VideoStudioPrimaryBtn {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff; border: none; border-radius: 8px; padding: 0px 10px; font-weight: 600; font-size: 11.5px;"
        "}"
        "QPushButton#VideoStudioPrimaryBtn:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6); }"
        "QPushButton#VideoStudioSecBtn {"
        "   background-color: %7; color: %2; border: 1px solid %4; border-radius: 8px; padding: 0px 8px; font-weight: 600; font-size: 11px;"
        "}"
        "QPushButton#VideoStudioSecBtn:hover { background-color: %8; }"
        "QPushButton#VideoStudioDangerBtn {"
        "   background-color: rgba(239, 68, 68, 0.16); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.35);"
        "   border-radius: 7px; padding: 0px 8px; font-weight: 600; font-size: 11px;"
        "}"
        "QPushButton#VideoStudioDangerBtn:hover { background-color: #ef4444; color: #ffffff; }"
        "QProgressBar { background-color: %6; border: none; border-radius: 4px; }"
        "QProgressBar::chunk { background-color: #38bdf8; border-radius: 4px; }"
    ).arg(bg, text, cardBg, border, subText, inputBg, secBtnBg, secBtnHover);

    setStyleSheet(css + ThemeManager::scrollBarStyleSheet(preset));
}
