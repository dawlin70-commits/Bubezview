/**
 * @file system_media_viewer.cpp
 * @brief Implementation of the native Qt6 Multimedia fallback player.
 */

#include "system_media_viewer.h"
#include "ui/vector_icons.h"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QSet>
#include <QSize>

namespace {

/**
 * @brief Checks if a lowercase file suffix corresponds to a known video container format.
 */
bool isVideoExtension(const QString &suffix)
{
    static const QSet<QString> videoExts = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
        QStringLiteral("wmv"), QStringLiteral("m4v"), QStringLiteral("ts"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("vob")
    };
    return videoExts.contains(suffix.toLower());
}

} // namespace

SystemMediaViewer::SystemMediaViewer(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    m_audioOutput->setVolume(1.0f);

    connect(m_player, &QMediaPlayer::positionChanged, this, &SystemMediaViewer::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &SystemMediaViewer::onDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &SystemMediaViewer::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &SystemMediaViewer::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::hasVideoChanged, this, &SystemMediaViewer::onHasVideoChanged);

    installEventFilter(this);
}

SystemMediaViewer::~SystemMediaViewer()
{
    if (m_player) {
        m_player->stop();
    }
}

void SystemMediaViewer::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Central Canvas Container
    m_canvasContainer = new QWidget(this);
    m_canvasContainer->setStyleSheet("background-color: #0b0f19;");
    QVBoxLayout *canvasLayout = new QVBoxLayout(m_canvasContainer);
    canvasLayout->setContentsMargins(0, 0, 0, 0);

    // Video Surface
    m_videoWidget = new QVideoWidget(m_canvasContainer);
    m_videoWidget->setStyleSheet("background-color: #000000;");
    canvasLayout->addWidget(m_videoWidget, 1);

    // Audio Visual Card (displayed when media has no video stream)
    m_audioCard = new QWidget(m_canvasContainer);
    m_audioCard->setStyleSheet("background-color: #0b0f19;");
    QVBoxLayout *audioCardLayout = new QVBoxLayout(m_audioCard);
    audioCardLayout->setAlignment(Qt::AlignCenter);
    audioCardLayout->setSpacing(16);

    QFrame *cardFrame = new QFrame(m_audioCard);
    cardFrame->setStyleSheet(
        "QFrame { background-color: rgba(30, 41, 59, 0.75); border: 1px solid rgba(148, 163, 184, 0.25); "
        "border-radius: 16px; padding: 24px; min-width: 440px; max-width: 520px; }"
    );
    QVBoxLayout *frameLayout = new QVBoxLayout(cardFrame);
    frameLayout->setAlignment(Qt::AlignCenter);
    frameLayout->setSpacing(12);

    m_lblAudioCover = new QLabel(cardFrame);
    m_lblAudioCover->setAlignment(Qt::AlignCenter);
    m_lblAudioCover->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Media).pixmap(72, 72));
    frameLayout->addWidget(m_lblAudioCover);

    m_lblAudioTitle = new QLabel(cardFrame);
    m_lblAudioTitle->setAlignment(Qt::AlignCenter);
    m_lblAudioTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #f8fafc;");
    m_lblAudioTitle->setWordWrap(true);
    frameLayout->addWidget(m_lblAudioTitle);

    m_lblAudioArtist = new QLabel(cardFrame);
    m_lblAudioArtist->setAlignment(Qt::AlignCenter);
    m_lblAudioArtist->setStyleSheet("font-size: 13px; color: #94a3b8;");
    frameLayout->addWidget(m_lblAudioArtist);

    m_lblEngineBadge = new QLabel(cardFrame);
    m_lblEngineBadge->setAlignment(Qt::AlignCenter);
    m_lblEngineBadge->setStyleSheet(
        "background-color: rgba(16, 185, 129, 0.15); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.35); "
        "padding: 4px 12px; border-radius: 10px; font-size: 11px; font-weight: bold;"
    );
    m_lblEngineBadge->setText(tr("⚡ Native System Audio (OS Codecs)"));
    frameLayout->addWidget(m_lblEngineBadge);

    audioCardLayout->addWidget(cardFrame);
    canvasLayout->addWidget(m_audioCard, 1);
    m_audioCard->hide();

    mainLayout->addWidget(m_canvasContainer, 1);

    // 2. Playback Control Bar
    m_controlsBar = new QWidget(this);
    m_controlsBar->setFixedHeight(80);
    m_controlsBar->setStyleSheet(
        "QWidget { background-color: rgba(15, 23, 42, 0.95); border-top: 1px solid rgba(148, 163, 184, 0.2); }"
    );
    QVBoxLayout *barLayout = new QVBoxLayout(m_controlsBar);
    barLayout->setContentsMargins(16, 6, 16, 8);
    barLayout->setSpacing(4);

    // Progress / Seek row
    QHBoxLayout *seekLayout = new QHBoxLayout();
    seekLayout->setSpacing(8);

    m_sliderPosition = new QSlider(Qt::Horizontal, m_controlsBar);
    m_sliderPosition->setRange(0, 0);
    m_sliderPosition->setCursor(Qt::PointingHandCursor);
    m_sliderPosition->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: rgba(100, 116, 139, 0.3); border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #0284c7; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #38bdf8; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #7dd3fc; }"
    );
    connect(m_sliderPosition, &QSlider::sliderMoved, this, &SystemMediaViewer::seekTo);
    connect(m_sliderPosition, &QSlider::sliderPressed, this, [this]() { m_isSeeking = true; });
    connect(m_sliderPosition, &QSlider::sliderReleased, this, [this]() {
        m_isSeeking = false;
        seekTo(m_sliderPosition->value());
    });

    m_lblTime = new QLabel(tr("00:00 / 00:00"), m_controlsBar);
    m_lblTime->setStyleSheet("color: #94a3b8; font-size: 11px; font-family: monospace;");

    seekLayout->addWidget(m_sliderPosition, 1);
    seekLayout->addWidget(m_lblTime);
    barLayout->addLayout(seekLayout);

    // Buttons and volume row
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto makeBtn = [this](const QIcon &icon, const QString &tooltip) {
        QPushButton *btn = new QPushButton(m_controlsBar);
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(34, 30);
        btn->setStyleSheet(
            "QPushButton { background: rgba(51, 65, 85, 0.6); color: #f8fafc; border: 1px solid rgba(148, 163, 184, 0.25); border-radius: 6px; }"
            "QPushButton:hover { background: rgba(71, 85, 105, 0.8); border-color: rgba(148, 163, 184, 0.5); }"
        );
        return btn;
    };

    m_btnPrev = makeBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Previous, QColor("#cbd5e1"), 18), tr("Previous track"));
    connect(m_btnPrev, &QPushButton::clicked, this, &SystemMediaViewer::playPreviousTrack);

    m_btnPlayPause = makeBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Play, QColor("#ffffff"), 20), tr("Play / Pause"));
    m_btnPlayPause->setIconSize(QSize(20, 20));
    m_btnPlayPause->setStyleSheet(
        "QPushButton { background: #0284c7; color: white; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: #0369a1; }"
    );
    connect(m_btnPlayPause, &QPushButton::clicked, this, &SystemMediaViewer::togglePlayPause);

    m_btnStop = new QPushButton(QStringLiteral("■"), m_controlsBar);
    m_btnStop->setToolTip(tr("Stop"));
    m_btnStop->setCursor(Qt::PointingHandCursor);
    m_btnStop->setFixedSize(34, 30);
    m_btnStop->setStyleSheet(
        "QPushButton { background: rgba(51, 65, 85, 0.6); color: #cbd5e1; border: 1px solid rgba(148, 163, 184, 0.25); border-radius: 6px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(71, 85, 105, 0.8); border-color: rgba(148, 163, 184, 0.5); }"
    );
    connect(m_btnStop, &QPushButton::clicked, this, &SystemMediaViewer::stop);

    m_btnNext = makeBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Next, QColor("#cbd5e1"), 18), tr("Next track"));
    connect(m_btnNext, &QPushButton::clicked, this, &SystemMediaViewer::playNextTrack);

    m_btnLoop = makeBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, QColor("#94a3b8"), 16), tr("Toggle repeat loop"));
    connect(m_btnLoop, &QPushButton::clicked, this, &SystemMediaViewer::toggleLoop);

    btnLayout->addWidget(m_btnPrev);
    btnLayout->addWidget(m_btnPlayPause);
    btnLayout->addWidget(m_btnStop);
    btnLayout->addWidget(m_btnNext);
    btnLayout->addWidget(m_btnLoop);

    btnLayout->addSpacing(16);

    QLabel *badgeSystem = new QLabel(tr("⚡ OS Native Codecs"), m_controlsBar);
    badgeSystem->setStyleSheet("color: #64748b; font-size: 11px; font-weight: 500;");
    btnLayout->addWidget(badgeSystem);

    btnLayout->addStretch(1);

    m_btnMute = makeBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18), tr("Mute / Unmute"));
    connect(m_btnMute, &QPushButton::clicked, this, &SystemMediaViewer::toggleMute);
    btnLayout->addWidget(m_btnMute);

    m_sliderVolume = new QSlider(Qt::Horizontal, m_controlsBar);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(100);
    m_sliderVolume->setFixedWidth(100);
    m_sliderVolume->setCursor(Qt::PointingHandCursor);
    m_sliderVolume->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: rgba(100, 116, 139, 0.3); border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #10b981; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #34d399; width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }"
    );
    connect(m_sliderVolume, &QSlider::valueChanged, this, &SystemMediaViewer::setVolume);
    btnLayout->addWidget(m_sliderVolume);

    barLayout->addLayout(btnLayout);

    mainLayout->addWidget(m_controlsBar, 0);
}

bool SystemMediaViewer::loadFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }

    m_currentFilePath = filePath;
    if (m_siblingFiles.contains(filePath)) {
        m_currentIndex = m_siblingFiles.indexOf(filePath);
    }

    // Determine initial visual mode based on file extension
    bool isVideoExt = isVideoExtension(fi.suffix());

    if (isVideoExt) {
        m_videoWidget->show();
        m_audioCard->hide();
    } else {
        m_videoWidget->hide();
        m_audioCard->show();
        m_lblAudioTitle->setText(fi.fileName());
        m_lblAudioArtist->setText(tr("Format: %1 | Size: %2 MB").arg(fi.suffix().toUpper()).arg(QString::number(fi.size() / (1024.0 * 1024.0), 'f', 1)));
    }

    m_sliderPosition->setValue(0);
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();

    updatePlayPauseUi(true);
    return true;
}

void SystemMediaViewer::clear()
{
    stop();
    m_currentFilePath.clear();
    m_sliderPosition->setRange(0, 0);
    m_lblTime->setText(tr("00:00 / 00:00"));
    m_lblAudioTitle->clear();
    m_lblAudioArtist->clear();
}

void SystemMediaViewer::stop()
{
    if (m_player) {
        m_player->stop();
    }
    updatePlayPauseUi(false);
}

void SystemMediaViewer::togglePlayPause()
{
    if (!m_player) return;

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

bool SystemMediaViewer::isAudioOnly() const
{
    if (m_player && m_player->hasVideo()) {
        return false;
    }
    QFileInfo fi(m_currentFilePath);
    return !isVideoExtension(fi.suffix());
}

void SystemMediaViewer::setSiblingFiles(const QStringList &files)
{
    m_siblingFiles = files;
    if (!m_currentFilePath.isEmpty()) {
        m_currentIndex = m_siblingFiles.indexOf(m_currentFilePath);
    }
}

void SystemMediaViewer::resetMixer()
{
    if (m_audioOutput) {
        m_audioOutput->setMuted(false);
        m_audioOutput->setVolume(1.0f);
        if (m_sliderVolume) m_sliderVolume->setValue(100);
        if (m_btnMute) {
            m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
        }
    }
}

void SystemMediaViewer::openAudioTrimDialog()
{
    QMessageBox::information(
        this,
        tr("Audio Trimmer"),
        tr("The interactive lossless audio trimmer requires the Complete Suite or MPV Multimedia Plugin.")
    );
}

void SystemMediaViewer::openAudioTagDialog()
{
    QMessageBox::information(
        this,
        tr("Audio Tag Editor"),
        tr("Audio metadata editing requires the TagLib plugin available in the Complete Suite.")
    );
}

void SystemMediaViewer::extractAudioTrack()
{
    if (m_currentFilePath.isEmpty()) return;

    QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Extract Audio"),
            tr("Audio stream extraction requires 'ffmpeg' installed on your system.")
        );
        return;
    }

    QFileInfo fi(m_currentFilePath);
    QString outPath = fi.dir().filePath(fi.baseName() + "_audio.mp3");

    QStringList args;
    args << "-y" << "-i" << m_currentFilePath << "-vn" << "-acodec" << "libmp3lame" << "-q:a" << "2" << outPath;

    bool ok = QProcess::startDetached(ffmpeg, args);
    if (ok) {
        QMessageBox::information(
            this,
            tr("Extract Audio"),
            tr("Audio extraction started successfully:\n%1").arg(outPath)
        );
    } else {
        QMessageBox::warning(
            this,
            tr("Extract Audio"),
            tr("Failed to start ffmpeg process.")
        );
    }
}

bool SystemMediaViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Space) {
            togglePlayPause();
            return true;
        } else if (ke->key() == Qt::Key_Left) {
            if (m_player) seekTo(qMax<qint64>(0, m_player->position() - 5000));
            return true;
        } else if (ke->key() == Qt::Key_Right) {
            if (m_player) seekTo(qMin<qint64>(m_player->duration(), m_player->position() + 5000));
            return true;
        } else if (ke->key() == Qt::Key_Up) {
            if (m_sliderVolume) m_sliderVolume->setValue(qMin(100, m_sliderVolume->value() + 5));
            return true;
        } else if (ke->key() == Qt::Key_Down) {
            if (m_sliderVolume) m_sliderVolume->setValue(qMax(0, m_sliderVolume->value() - 5));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SystemMediaViewer::onPositionChanged(qint64 position)
{
    if (!m_isSeeking && m_sliderPosition) {
        m_sliderPosition->setValue(static_cast<int>(position));
    }
    updateTimeLabel(position, m_player ? m_player->duration() : 0);
}

void SystemMediaViewer::onDurationChanged(qint64 duration)
{
    if (m_sliderPosition) {
        m_sliderPosition->setRange(0, static_cast<int>(duration));
    }
    updateTimeLabel(m_player ? m_player->position() : 0, duration);
}

void SystemMediaViewer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    updatePlayPauseUi(state == QMediaPlayer::PlayingState);
}

void SystemMediaViewer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_isLooping) {
            m_player->setPosition(0);
            m_player->play();
        } else {
            playNextTrack();
        }
    }
}

void SystemMediaViewer::onHasVideoChanged(bool hasVideo)
{
    if (hasVideo) {
        m_videoWidget->show();
        m_audioCard->hide();
    } else {
        m_videoWidget->hide();
        m_audioCard->show();
    }
}

void SystemMediaViewer::seekTo(int positionMs)
{
    if (m_player) {
        m_player->setPosition(positionMs);
    }
}

void SystemMediaViewer::setVolume(int volumePercent)
{
    if (m_audioOutput) {
        float vol = static_cast<float>(volumePercent) / 100.0f;
        m_audioOutput->setVolume(vol);
        if (m_btnMute) {
            m_btnMute->setIcon(VectorIcons::createUiIcon(
                volumePercent == 0 ? VectorIcons::UiIcon::VolumeMute : VectorIcons::UiIcon::Volume,
                QColor("#cbd5e1"),
                18
            ));
        }
    }
}

void SystemMediaViewer::toggleMute()
{
    if (!m_audioOutput) return;

    if (m_audioOutput->isMuted()) {
        m_audioOutput->setMuted(false);
        if (m_btnMute) {
            m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
        }
        if (m_sliderVolume) m_sliderVolume->setValue(static_cast<int>(m_previousVolume * 100));
    } else {
        m_previousVolume = m_audioOutput->volume();
        m_audioOutput->setMuted(true);
        if (m_btnMute) {
            m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::VolumeMute, QColor("#cbd5e1"), 18));
        }
        if (m_sliderVolume) m_sliderVolume->setValue(0);
    }
}

void SystemMediaViewer::playNextTrack()
{
    if (m_siblingFiles.isEmpty()) return;

    int nextIdx = (m_currentIndex + 1) % m_siblingFiles.size();
    QString nextFile = m_siblingFiles.at(nextIdx);

    if (m_trackChangeCallback) {
        m_trackChangeCallback(nextFile);
    } else {
        loadFile(nextFile);
    }
}

void SystemMediaViewer::playPreviousTrack()
{
    if (m_siblingFiles.isEmpty()) return;

    int prevIdx = (m_currentIndex <= 0) ? (m_siblingFiles.size() - 1) : (m_currentIndex - 1);
    QString prevFile = m_siblingFiles.at(prevIdx);

    if (m_trackChangeCallback) {
        m_trackChangeCallback(prevFile);
    } else {
        loadFile(prevFile);
    }
}

void SystemMediaViewer::toggleLoop()
{
    m_isLooping = !m_isLooping;
    if (m_btnLoop) {
        m_btnLoop->setIcon(VectorIcons::createUiIcon(
            VectorIcons::UiIcon::Repeat,
            m_isLooping ? QColor("#38bdf8") : QColor("#94a3b8"),
            16
        ));
        m_btnLoop->setStyleSheet(
            m_isLooping
                ? "QPushButton { background: rgba(2, 132, 199, 0.4); border: 1px solid #0284c7; border-radius: 6px; }"
                : "QPushButton { background: rgba(51, 65, 85, 0.6); border: 1px solid rgba(148, 163, 184, 0.25); border-radius: 6px; }"
        );
    }
}

void SystemMediaViewer::updatePlayPauseUi(bool isPlaying)
{
    if (m_btnPlayPause) {
        m_btnPlayPause->setIcon(VectorIcons::createUiIcon(
            isPlaying ? VectorIcons::UiIcon::Pause : VectorIcons::UiIcon::Play,
            QColor("#ffffff"),
            20
        ));
    }
}

void SystemMediaViewer::updateTimeLabel(qint64 currentMs, qint64 totalMs)
{
    if (m_lblTime) {
        m_lblTime->setText(QString("%1 / %2").arg(formatTime(currentMs), formatTime(totalMs)));
    }
}

QString SystemMediaViewer::formatTime(qint64 ms)
{
    qint64 totalSec = ms / 1000;
    qint64 sec = totalSec % 60;
    qint64 min = (totalSec / 60) % 60;
    qint64 hr = totalSec / 3600;

    if (hr > 0) {
        return QString("%1:%2:%3")
            .arg(hr, 2, 10, QLatin1Char('0'))
            .arg(min, 2, 10, QLatin1Char('0'))
            .arg(sec, 2, 10, QLatin1Char('0'));
    }
    return QString("%1:%2")
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'));
}
