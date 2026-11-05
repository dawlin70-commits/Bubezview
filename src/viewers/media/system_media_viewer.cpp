/**
 * @file system_media_viewer.cpp
 * @brief Implementation of the unified Qt6 Multimedia media player.
 */

#include "system_media_viewer.h"
#include "ui/vector_icons.h"
#include "core/audio/audio_processor.h"
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QApplication>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

namespace {

bool isVideoExtension(const QString &ext)
{
    static const QStringList videoExts = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
        QStringLiteral("wmv"), QStringLiteral("m4v"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("ts"), QStringLiteral("3gp")
    };
    return videoExts.contains(ext.toLower());
}

} // namespace

SystemMediaViewer::SystemMediaViewer(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #0b0e14;");

    // Initialize Qt Multimedia player and audio output pipeline
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0f);
    m_player->setAudioOutput(m_audioOutput);

    setupUi();

    m_player->setVideoOutput(m_videoWidget);

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
    canvasLayout->setSpacing(0);

    // 1A. Video Surface (QVideoWidget)
    m_videoWidget = new QVideoWidget(m_canvasContainer);
    m_videoWidget->setStyleSheet("background-color: #000000;");
    canvasLayout->addWidget(m_videoWidget, 1);

    // 1B. Audio Player Widget (Vinyl Record Sleeve + Studio Typography)
    m_audioCoverWidget = new QWidget(m_canvasContainer);
    m_audioCoverWidget->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #111621, stop:1 #0d1118);");
    QVBoxLayout *audioRootLayout = new QVBoxLayout(m_audioCoverWidget);
    audioRootLayout->setContentsMargins(16, 8, 16, 4);
    audioRootLayout->setSpacing(6);

    // Top Row: Vinyl Sleeve (Left) + Borderless Typography (Right)
    m_audioTopLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_audioTopLayout->setContentsMargins(0, 0, 0, 0);
    m_audioTopLayout->setSpacing(14);

    // 1. Analog Vinyl Sleeve with Peeking Grooved Record
    m_vinylSleeveWidget = new VinylSleeveWidget(m_audioCoverWidget);
    m_audioTopLayout->addWidget(m_vinylSleeveWidget, 0, Qt::AlignVCenter);

    // 2. Clean Borderless Metadata Column on the Right
    m_metaCard = new QFrame(m_audioCoverWidget);
    m_metaCard->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_metaCard->setMinimumWidth(0);
    m_metaCard->setStyleSheet("QFrame { background: transparent; border: none; }");
    m_metaLayout = new QVBoxLayout(m_metaCard);
    m_metaLayout->setContentsMargins(4, 8, 4, 8);
    m_metaLayout->setSpacing(6);
    m_metaLayout->setAlignment(Qt::AlignVCenter);

    // Track Title (21px bold white)
    m_audioTitleLabel = new QLabel(m_metaCard);
    m_audioTitleLabel->setStyleSheet("font-size: 21px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;");
    m_audioTitleLabel->setWordWrap(true);
    m_audioTitleLabel->setMaximumHeight(58);
    m_audioTitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_audioTitleLabel->setMinimumWidth(0);
    m_metaLayout->addWidget(m_audioTitleLabel);

    // Artist (14px demibold cyan)
    m_audioArtistLabel = new QLabel(m_metaCard);
    m_audioArtistLabel->setStyleSheet("font-size: 14px; font-weight: 600; color: #38bdf8; border: none; background: transparent; padding: 0px;");
    m_audioArtistLabel->setWordWrap(false);
    m_audioArtistLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_audioArtistLabel->setMinimumWidth(0);
    m_metaLayout->addWidget(m_audioArtistLabel);

    // Album (11.5px light slate)
    m_audioAlbumLabel = new QLabel(m_metaCard);
    m_audioAlbumLabel->setStyleSheet("font-size: 11px; font-weight: normal; color: #cbd5e1; border: none; background: transparent; padding: 0px;");
    m_audioAlbumLabel->setWordWrap(false);
    m_audioAlbumLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_audioAlbumLabel->setMinimumWidth(0);
    m_metaLayout->addWidget(m_audioAlbumLabel);

    // Specs Line (10px slate)
    m_audioSpecsLabel = new QLabel(m_metaCard);
    m_audioSpecsLabel->setStyleSheet("font-size: 10px; font-weight: normal; color: #64748b; border: none; background: transparent; padding-top: 2px;");
    m_audioSpecsLabel->setWordWrap(false);
    m_audioSpecsLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_audioSpecsLabel->setMinimumWidth(0);
    m_metaLayout->addWidget(m_audioSpecsLabel);

    // Tag Inspector Badge Chips (Genre, Track #, Year, Audio Specs)
    m_tagsBadgeContainer = new QWidget(m_metaCard);
    m_tagsBadgeContainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_tagsBadgeContainer->setMinimumWidth(0);
    m_tagsBadgeLayout = new QHBoxLayout(m_tagsBadgeContainer);
    m_tagsBadgeLayout->setContentsMargins(0, 4, 0, 0);
    m_tagsBadgeLayout->setSpacing(6);
    m_tagsBadgeLayout->setAlignment(Qt::AlignLeft);

    const QString badgeStyle = "QLabel {"
                               "  background-color: rgba(255, 255, 255, 0.08);"
                               "  border: 1px solid rgba(255, 255, 255, 0.12);"
                               "  border-radius: 9px;"
                               "  padding: 2px 8px;"
                               "  font-size: 11px;"
                               "  color: #cbd5e1;"
                               "  font-weight: 500;"
                               "}";

    m_badgeGenre = new QLabel(m_tagsBadgeContainer);
    m_badgeGenre->setStyleSheet(badgeStyle);
    m_badgeGenre->setVisible(false);

    m_badgeTrack = new QLabel(m_tagsBadgeContainer);
    m_badgeTrack->setStyleSheet(badgeStyle);
    m_badgeTrack->setVisible(false);

    m_badgeDate = new QLabel(m_tagsBadgeContainer);
    m_badgeDate->setStyleSheet(badgeStyle);
    m_badgeDate->setVisible(false);

    m_badgeTech = new QLabel(m_tagsBadgeContainer);
    m_badgeTech->setStyleSheet("QLabel {"
                               "  background-color: rgba(56, 189, 248, 0.12);"
                               "  border: 1px solid rgba(56, 189, 248, 0.25);"
                               "  border-radius: 9px;"
                               "  padding: 2px 8px;"
                               "  font-size: 11px;"
                               "  color: #38bdf8;"
                               "  font-weight: 600;"
                               "}");
    m_badgeTech->setVisible(false);

    m_tagsBadgeLayout->addWidget(m_badgeGenre);
    m_tagsBadgeLayout->addWidget(m_badgeTrack);
    m_tagsBadgeLayout->addWidget(m_badgeDate);
    m_tagsBadgeLayout->addWidget(m_badgeTech);

    m_metaLayout->addWidget(m_tagsBadgeContainer);
    m_audioTopLayout->addWidget(m_metaCard, 1, Qt::AlignVCenter);
    audioRootLayout->addLayout(m_audioTopLayout, 1);

    canvasLayout->addWidget(m_audioCoverWidget, 1);
    mainLayout->addWidget(m_canvasContainer, 1);

    // 2. Bottom Timeline & Transport Controls Bar
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_controlsWidget->setMinimumWidth(0);
    m_controlsWidget->setStyleSheet(
        "QWidget {"
        "   background-color: #0b0e14;"
        "   border: none;"
        "}"
        "QFrame#WaveformContainer {"
        "   background-color: rgba(255, 255, 255, 0.025);"
        "   border: 1px solid rgba(255, 255, 255, 0.075);"
        "   border-radius: 14px;"
        "}"
    );
    QVBoxLayout *ctrlRootLayout = new QVBoxLayout(m_controlsWidget);
    ctrlRootLayout->setContentsMargins(12, 2, 12, 6);
    ctrlRootLayout->setSpacing(8);

    // 2A. Smooth Spline Waveform Scrubber (for Audio Mode)
    m_waveformContainer = new QFrame(m_controlsWidget);
    m_waveformContainer->setObjectName("WaveformContainer");
    m_waveformContainer->setFixedHeight(64);
    QHBoxLayout *waveLayout = new QHBoxLayout(m_waveformContainer);
    waveLayout->setContentsMargins(14, 8, 14, 8);
    waveLayout->setSpacing(12);

    m_waveTimeLeftLabel = new QLabel("00:00", m_waveformContainer);
    m_waveTimeLeftLabel->setFixedWidth(48);
    m_waveTimeLeftLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_waveTimeLeftLabel->setStyleSheet("font-size: 10px; font-weight: bold; font-family: monospace; color: #38bdf8; border: none; background: transparent;");

    m_waveformScrubber = new SmoothWaveformScrubber(m_waveformContainer);
    connect(m_waveformScrubber, &SmoothWaveformScrubber::seekRequested, this, [this](qint64 targetMs, bool /*exact*/) {
        seekTo(targetMs);
    });

    m_waveTimeRightLabel = new QLabel("-00:00", m_waveformContainer);
    m_waveTimeRightLabel->setFixedWidth(52);
    m_waveTimeRightLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_waveTimeRightLabel->setStyleSheet("font-size: 10px; font-weight: bold; font-family: monospace; color: #64748b; border: none; background: transparent;");

    waveLayout->addWidget(m_waveTimeLeftLabel);
    waveLayout->addWidget(m_waveformScrubber, 1);
    waveLayout->addWidget(m_waveTimeRightLabel);
    ctrlRootLayout->addWidget(m_waveformContainer);

    // 2B. Standard Time Slider Row (for Video Mode)
    m_videoSliderWidget = new QWidget(m_controlsWidget);
    m_videoSliderWidget->setStyleSheet("background: transparent; border: none;");
    QHBoxLayout *sliderRow = new QHBoxLayout(m_videoSliderWidget);
    sliderRow->setContentsMargins(0, 0, 0, 0);
    sliderRow->setSpacing(10);

    m_timeLeftLabel = new QLabel("00:00 / 00:00", m_videoSliderWidget);
    m_timeLeftLabel->setStyleSheet("font-size: 11px; font-family: monospace; color: #94a3b8; border: none; background: transparent;");

    m_sliderPosition = new QSlider(Qt::Horizontal, m_videoSliderWidget);
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

    m_timeRightLabel = new QLabel("-00:00", m_videoSliderWidget);
    m_timeRightLabel->setStyleSheet("font-size: 11px; font-family: monospace; color: #94a3b8; border: none; background: transparent;");

    sliderRow->addWidget(m_timeLeftLabel);
    sliderRow->addWidget(m_sliderPosition, 1);
    sliderRow->addWidget(m_timeRightLabel);
    ctrlRootLayout->addWidget(m_videoSliderWidget);

    // 2C. Bottom Row of Transport Controls
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(10);

    // Zone 1 (Left): Playback speed pill + Hi-Fi track indicator
    QWidget *leftTrackWidget = new QWidget(m_controlsWidget);
    leftTrackWidget->setStyleSheet("background: transparent; border: none;");
    leftTrackWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    leftTrackWidget->setMinimumWidth(0);
    QHBoxLayout *leftTrackLayout = new QHBoxLayout(leftTrackWidget);
    leftTrackLayout->setContentsMargins(0, 0, 0, 0);
    leftTrackLayout->setSpacing(4);

    m_btnSpeed = new QPushButton("1.0x", leftTrackWidget);
    m_btnSpeed->setToolTip(tr("Playback speed"));
    m_btnSpeed->setCursor(Qt::PointingHandCursor);
    m_btnSpeed->setStyleSheet(
        "QPushButton { background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); "
        "border-radius: 12px; padding: 2px 10px; font-size: 11px; font-weight: bold; color: #cbd5e1; }"
        "QPushButton:hover { background: rgba(56, 189, 248, 0.15); border-color: #38bdf8; color: #38bdf8; }"
    );
    connect(m_btnSpeed, &QPushButton::clicked, this, &SystemMediaViewer::showSpeedMenu);

    m_bottomTitleLabel = new QLabel(leftTrackWidget);
    m_bottomTitleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #f8fafc; border: none; background: transparent;");
    m_bottomTitleLabel->setVisible(false);

    m_bottomFileLabel = new QLabel(leftTrackWidget);
    m_bottomFileLabel->setStyleSheet("font-size: 10px; font-weight: 600; color: #94a3b8; border: none; background: transparent;");

    leftTrackLayout->addWidget(m_btnSpeed);
    leftTrackLayout->addWidget(m_bottomTitleLabel);
    leftTrackLayout->addWidget(m_bottomFileLabel);
    bottomRow->addWidget(leftTrackWidget, 0, Qt::AlignVCenter);

    bottomRow->addStretch(1);

    // Zone 2 (Center): Playback Controls Cluster with Large Circular Play Button
    QHBoxLayout *centerCluster = new QHBoxLayout();
    centerCluster->setSpacing(4);
    centerCluster->setAlignment(Qt::AlignCenter);

    auto makeNavBtn = [this](const QIcon &icon, const QString &tooltip) {
        QPushButton *btn = new QPushButton(m_controlsWidget);
        btn->setFixedSize(32, 32);
        btn->setIcon(icon);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 16px; padding: 4px; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.08); }"
        );
        return btn;
    };

    m_btnShuffle = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, QColor("#94a3b8"), 16), tr("Shuffle"));
    connect(m_btnShuffle, &QPushButton::clicked, this, &SystemMediaViewer::toggleShuffle);

    m_btnPrev = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Previous, QColor("#cbd5e1"), 18), tr("Previous track"));
    connect(m_btnPrev, &QPushButton::clicked, this, &SystemMediaViewer::playPreviousTrack);

    // Large circular 42x42 Play/Pause button
    m_btnPlayPause = new QPushButton(m_controlsWidget);
    m_btnPlayPause->setFixedSize(42, 42);
    m_btnPlayPause->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Play, QColor("#ffffff"), 20));
    m_btnPlayPause->setToolTip(tr("Play / Pause"));
    m_btnPlayPause->setCursor(Qt::PointingHandCursor);
    m_btnPlayPause->setStyleSheet(
        "QPushButton {"
        "   background: qradialgradient(cx:0.5, cy:0.35, radius:0.8, fx:0.5, fy:0.35, stop:0 #38bdf8, stop:1 #0284c7);"
        "   border: 1px solid rgba(255, 255, 255, 0.25);"
        "   border-radius: 21px;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background: qradialgradient(cx:0.5, cy:0.35, radius:0.8, fx:0.5, fy:0.35, stop:0 #7dd3fc, stop:1 #0ea5e9);"
        "   border: 1px solid rgba(255, 255, 255, 0.45);"
        "}"
        "QPushButton:pressed {"
        "   background: #0284c7;"
        "}"
    );
    connect(m_btnPlayPause, &QPushButton::clicked, this, &SystemMediaViewer::togglePlayPause);

    m_btnStop = new QPushButton(QStringLiteral("■"), m_controlsWidget);
    m_btnStop->setToolTip(tr("Stop"));
    m_btnStop->setCursor(Qt::PointingHandCursor);
    m_btnStop->setFixedSize(30, 30);
    m_btnStop->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 15px; color: #94a3b8; font-size: 13px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.08); color: #cbd5e1; }"
    );
    connect(m_btnStop, &QPushButton::clicked, this, &SystemMediaViewer::stop);

    m_btnNext = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Next, QColor("#cbd5e1"), 18), tr("Next track"));
    connect(m_btnNext, &QPushButton::clicked, this, &SystemMediaViewer::playNextTrack);

    m_btnLoop = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, QColor("#94a3b8"), 16), tr("Loop playback"));
    connect(m_btnLoop, &QPushButton::clicked, this, &SystemMediaViewer::toggleLoop);

    centerCluster->addWidget(m_btnShuffle);
    centerCluster->addWidget(m_btnPrev);
    centerCluster->addWidget(m_btnPlayPause);
    centerCluster->addWidget(m_btnStop);
    centerCluster->addWidget(m_btnNext);
    centerCluster->addWidget(m_btnLoop);
    bottomRow->addLayout(centerCluster);

    bottomRow->addStretch(1);

    // Zone 3 (Right): Mute & Volume Slider
    QHBoxLayout *rightCluster = new QHBoxLayout();
    rightCluster->setSpacing(6);
    rightCluster->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_btnMute = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18), tr("Volume / Mute"));
    connect(m_btnMute, &QPushButton::clicked, this, &SystemMediaViewer::toggleMute);

    m_sliderVolume = new QSlider(Qt::Horizontal, m_controlsWidget);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(100);
    m_sliderVolume->setFixedWidth(90);
    m_sliderVolume->setCursor(Qt::PointingHandCursor);
    m_sliderVolume->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: rgba(100, 116, 139, 0.3); border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #38bdf8; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #f8fafc; width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }"
    );
    connect(m_sliderVolume, &QSlider::valueChanged, this, &SystemMediaViewer::setVolume);

    rightCluster->addWidget(m_btnMute);
    rightCluster->addWidget(m_sliderVolume);
    bottomRow->addLayout(rightCluster);

    ctrlRootLayout->addLayout(bottomRow);
    mainLayout->addWidget(m_controlsWidget, 0);

    // Initial visual state
    m_videoWidget->hide();
    m_audioCoverWidget->show();
    m_waveformContainer->show();
    m_videoSliderWidget->hide();
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

    bool isVideoExt = isVideoExtension(fi.suffix());
    m_isAudioOnly = !isVideoExt;

    if (m_isAudioOnly) {
        m_videoWidget->hide();
        m_audioCoverWidget->show();
        m_waveformContainer->show();
        m_videoSliderWidget->hide();

        // 1. Read metadata tags
        AudioMetadata meta = AudioProcessor::readMetadata(filePath);
        if (!meta.title.isEmpty()) {
            m_audioTitleLabel->setText(meta.title);
        } else {
            m_audioTitleLabel->setText(fi.completeBaseName());
        }

        if (!meta.artist.isEmpty()) {
            m_audioArtistLabel->setText(meta.artist);
            m_audioArtistLabel->setVisible(true);
        } else {
            m_audioArtistLabel->setText(tr("Unknown Artist"));
            m_audioArtistLabel->setVisible(true);
        }

        if (!meta.album.isEmpty()) {
            m_audioAlbumLabel->setText(meta.album);
            m_audioAlbumLabel->setVisible(true);
        } else {
            m_audioAlbumLabel->setVisible(false);
        }

        // Combined Specs Summary Line
        QString specs;
        if (!meta.year.isEmpty()) specs += meta.year + QStringLiteral(" • ");
        specs += fi.suffix().toUpper();
        if (meta.bitRate > 0) specs += QStringLiteral(" %1 kbps").arg(meta.bitRate);
        specs += QStringLiteral(" • %1 MB").arg(QString::number(fi.size() / (1024.0 * 1024.0), 'f', 1));
        m_audioSpecsLabel->setText(specs);

        // Update Tag Badges
        if (!meta.genre.isEmpty()) {
            m_badgeGenre->setText(meta.genre);
            m_badgeGenre->setVisible(true);
        } else {
            m_badgeGenre->setVisible(false);
        }

        if (!meta.trackNumber.isEmpty()) {
            m_badgeTrack->setText(tr("Track %1").arg(meta.trackNumber));
            m_badgeTrack->setVisible(true);
        } else {
            m_badgeTrack->setVisible(false);
        }

        if (!meta.year.isEmpty()) {
            m_badgeDate->setText(meta.year);
            m_badgeDate->setVisible(true);
        } else {
            m_badgeDate->setVisible(false);
        }

        if (meta.sampleRate > 0) {
            m_badgeTech->setText(QStringLiteral("%1 kHz").arg(meta.sampleRate / 1000.0, 0, 'f', 1));
            m_badgeTech->setVisible(true);
        } else {
            m_badgeTech->setVisible(false);
        }

        // 2. Extract album cover art and vinyl label
        QPixmap sleeveCover = AudioProcessor::extractCoverArt(filePath, 260);
        QPixmap vinylLabel = AudioProcessor::extractVinylLabelArt(filePath, 160);
        m_vinylSleeveWidget->setArtwork(sleeveCover, vinylLabel);

        // 3. Load smooth waveform scrubber
        m_waveformScrubber->loadAudioWaveform(filePath);
    } else {
        m_audioCoverWidget->hide();
        m_videoWidget->show();
        m_waveformContainer->hide();
        m_videoSliderWidget->show();
    }

    if (m_bottomFileLabel) {
        m_bottomFileLabel->setText(fi.fileName());
    }

    m_sliderPosition->setValue(0);
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();

    updatePlayPauseUi(true);
    updateAudioResponsiveLayout();
    return true;
}

void SystemMediaViewer::clear()
{
    stop();
    m_currentFilePath.clear();
    m_vinylSleeveWidget->clearArtwork();
    m_waveformScrubber->reset();
    m_audioTitleLabel->clear();
    m_audioArtistLabel->clear();
    m_audioAlbumLabel->clear();
    m_audioSpecsLabel->clear();
    m_badgeGenre->setVisible(false);
    m_badgeTrack->setVisible(false);
    m_badgeDate->setVisible(false);
    m_badgeTech->setVisible(false);
    m_sliderPosition->setValue(0);
    updateTimeLabel(0, 0);
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
    }
}

void SystemMediaViewer::updateTheme()
{
    if (m_audioCoverWidget) {
        m_audioCoverWidget->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #111621, stop:1 #0d1118);");
    }
    if (m_metaCard) {
        m_metaCard->setStyleSheet("QFrame { background: transparent; border: none; }");
    }
    if (m_audioTitleLabel) {
        m_audioTitleLabel->setStyleSheet("font-size: 21px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;");
    }
    if (m_audioArtistLabel) {
        m_audioArtistLabel->setStyleSheet("font-size: 14px; font-weight: 600; color: #38bdf8; border: none; background: transparent; padding: 0px;");
    }
    if (m_vinylSleeveWidget) {
        m_vinylSleeveWidget->update();
    }
    if (m_waveformScrubber) {
        m_waveformScrubber->update();
    }
}

void SystemMediaViewer::promptMpvRequired(const QString &toolName)
{
    QMessageBox::information(
        this,
        tr("Multimedia Studio Module Required"),
        tr("<h3>%1 Requires Multimedia Studio</h3>"
           "<p>Audio editing tools, pitch shifting, real-time equalizer, and studio filters "
           "require the <b>Multimedia Studio (MPV)</b> engine.</p>"
           "<p>Please install <code>libmpv-dev</code> / <code>mpv</code> to unlock full studio features.</p>")
           .arg(toolName)
    );
}

void SystemMediaViewer::switchAudioWorkspace(int index)
{
    if (index > 0) {
        promptMpvRequired(tr("Audio Studio"));
    }
}

void SystemMediaViewer::openAudioTrimDialog()
{
    promptMpvRequired(tr("Audio Cutter / Trim"));
}

void SystemMediaViewer::openAudioTagDialog()
{
    promptMpvRequired(tr("Audio Tag Editor"));
}

void SystemMediaViewer::extractAudioTrack()
{
    promptMpvRequired(tr("Audio Track Extraction"));
}

bool SystemMediaViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Space) {
            togglePlayPause();
            return true;
        }
        if (ke->key() == Qt::Key_Left) {
            seekTo(std::max<qint64>(0, m_currentPositionMs - 5000));
            return true;
        }
        if (ke->key() == Qt::Key_Right) {
            seekTo(std::min<qint64>(m_durationMs, m_currentPositionMs + 5000));
            return true;
        }
        if (ke->key() == Qt::Key_Up) {
            setVolume(std::min(100, m_sliderVolume->value() + 5));
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            setVolume(std::max(0, m_sliderVolume->value() - 5));
            return true;
        }
        if (ke->key() == Qt::Key_M) {
            toggleMute();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SystemMediaViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_isAudioOnly) {
        updateAudioResponsiveLayout();
    }
}

void SystemMediaViewer::updateAudioResponsiveLayout(int forceW, int forceH)
{
    if (!m_isAudioOnly || !m_audioTopLayout || !m_metaCard || !m_vinylSleeveWidget) {
        return;
    }

    const int w = (forceW > 0) ? forceW : width();
    const int h = (forceH > 0) ? forceH : height();
    if (w <= 0) return;

    // Responsive Breakpoint: below 660px, switch from horizontal to centered vertical layout
    const bool isCompact = (w < 660);

    if (isCompact) {
        m_isCompactAudioLayout = true;
        m_audioTopLayout->setDirection(QBoxLayout::TopToBottom);
        m_audioTopLayout->setSpacing(4);

        m_vinylSleeveWidget->setCompactMode(true);
        m_vinylSleeveWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        const int maxCoverH = (h > 0 && h < 460) ? 120 : 150;
        m_vinylSleeveWidget->setMaximumSize(280, maxCoverH);
        m_vinylSleeveWidget->setMinimumSize(60, 60);
        m_audioTopLayout->setAlignment(m_vinylSleeveWidget, Qt::AlignHCenter);
        m_audioTopLayout->setAlignment(m_metaCard, Qt::Alignment());

        if (m_metaCard) {
            m_metaCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }
        if (m_metaLayout) {
            m_metaLayout->setAlignment(Qt::AlignCenter);
            m_metaLayout->setContentsMargins(0, 2, 0, 2);
            m_metaLayout->setSpacing(3);
        }

        if (m_audioTitleLabel) {
            m_audioTitleLabel->setAlignment(Qt::AlignCenter);
            m_audioTitleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;"));
            m_audioTitleLabel->setMaximumHeight(48);
            m_audioTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }

        if (m_audioArtistLabel) {
            m_audioArtistLabel->setAlignment(Qt::AlignCenter);
            m_audioArtistLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #38bdf8; border: none; background: transparent; padding: 0px;"));
            m_audioArtistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }

        if (m_audioAlbumLabel) {
            m_audioAlbumLabel->setAlignment(Qt::AlignCenter);
            m_audioAlbumLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: normal; color: #cbd5e1; border: none; background: transparent; padding: 0px;"));
            m_audioAlbumLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }

        if (m_audioSpecsLabel) {
            m_audioSpecsLabel->setAlignment(Qt::AlignCenter);
            m_audioSpecsLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: normal; color: #64748b; border: none; background: transparent; padding-top: 1px;"));
            m_audioSpecsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }

        if (m_tagsBadgeLayout) {
            m_tagsBadgeLayout->setAlignment(Qt::AlignCenter);
        }
    } else {
        m_isCompactAudioLayout = false;
        m_audioTopLayout->setDirection(QBoxLayout::LeftToRight);
        m_audioTopLayout->setSpacing(14);

        m_vinylSleeveWidget->setCompactMode(false);
        m_vinylSleeveWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        m_vinylSleeveWidget->setMinimumSize(200, 130);
        m_vinylSleeveWidget->setMaximumSize(400, 280);
        m_audioTopLayout->setAlignment(m_vinylSleeveWidget, Qt::AlignVCenter);
        m_audioTopLayout->setAlignment(m_metaCard, Qt::AlignVCenter);

        if (m_metaCard) {
            m_metaCard->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
        if (m_metaLayout) {
            m_metaLayout->setAlignment(Qt::AlignVCenter);
            m_metaLayout->setContentsMargins(4, 8, 4, 8);
            m_metaLayout->setSpacing(6);
        }

        if (m_audioTitleLabel) {
            m_audioTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_audioTitleLabel->setStyleSheet(QStringLiteral("font-size: 21px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;"));
            m_audioTitleLabel->setMaximumHeight(58);
            m_audioTitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        if (m_audioArtistLabel) {
            m_audioArtistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_audioArtistLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600; color: #38bdf8; border: none; background: transparent; padding: 0px;"));
            m_audioArtistLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        if (m_audioAlbumLabel) {
            m_audioAlbumLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_audioAlbumLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: normal; color: #cbd5e1; border: none; background: transparent; padding: 0px;"));
            m_audioAlbumLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        if (m_audioSpecsLabel) {
            m_audioSpecsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_audioSpecsLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: normal; color: #64748b; border: none; background: transparent; padding-top: 2px;"));
            m_audioSpecsLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        if (m_tagsBadgeLayout) {
            m_tagsBadgeLayout->setAlignment(Qt::AlignLeft);
        }
    }
}

void SystemMediaViewer::onPositionChanged(qint64 position)
{
    m_currentPositionMs = position;
    if (!m_isSeeking) {
        m_sliderPosition->setValue(static_cast<int>(position));
        m_waveformScrubber->setPlaybackPosition(position, m_durationMs);
    }
    updateTimeLabel(position, m_durationMs);
}

void SystemMediaViewer::onDurationChanged(qint64 duration)
{
    m_durationMs = duration;
    m_sliderPosition->setRange(0, static_cast<int>(duration));
    m_waveformScrubber->setPlaybackPosition(m_currentPositionMs, duration);
    updateTimeLabel(m_currentPositionMs, duration);
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
    m_isAudioOnly = !hasVideo;
    if (hasVideo) {
        m_audioCoverWidget->hide();
        m_videoWidget->show();
        m_waveformContainer->hide();
        m_videoSliderWidget->show();
    } else {
        m_videoWidget->hide();
        m_audioCoverWidget->show();
        m_waveformContainer->show();
        m_videoSliderWidget->hide();
    }
}

void SystemMediaViewer::seekTo(qint64 positionMs)
{
    if (m_player) {
        m_player->setPosition(positionMs);
    }
}

void SystemMediaViewer::setVolume(int volumePercent)
{
    if (m_audioOutput) {
        float norm = std::clamp(volumePercent / 100.0f, 0.0f, 1.0f);
        m_audioOutput->setVolume(norm);
        if (norm > 0.0f && m_audioOutput->isMuted()) {
            m_audioOutput->setMuted(false);
        }
    }
    if (m_sliderVolume && m_sliderVolume->value() != volumePercent) {
        m_sliderVolume->setValue(volumePercent);
    }
}

void SystemMediaViewer::toggleMute()
{
    if (!m_audioOutput) return;
    bool isMuted = m_audioOutput->isMuted();
    m_audioOutput->setMuted(!isMuted);
    if (!isMuted) {
        m_previousVolume = m_audioOutput->volume();
        m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#ef4444"), 18));
    } else {
        m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
    }
}

void SystemMediaViewer::playNextTrack()
{
    if (m_siblingFiles.isEmpty()) return;
    if (m_isShuffle && m_siblingFiles.size() > 1) {
        int nextIdx = m_currentIndex;
        while (nextIdx == m_currentIndex) {
            nextIdx = QRandomGenerator::global()->bounded(m_siblingFiles.size());
        }
        m_currentIndex = nextIdx;
    } else {
        m_currentIndex = (m_currentIndex + 1) % m_siblingFiles.size();
    }
    QString nextTrack = m_siblingFiles.value(m_currentIndex);
    if (!nextTrack.isEmpty()) {
        if (m_trackChangeCallback) {
            m_trackChangeCallback(nextTrack);
        } else {
            loadFile(nextTrack);
        }
    }
}

void SystemMediaViewer::playPreviousTrack()
{
    if (m_siblingFiles.isEmpty()) return;
    m_currentIndex = (m_currentIndex - 1 + m_siblingFiles.size()) % m_siblingFiles.size();
    QString prevTrack = m_siblingFiles.value(m_currentIndex);
    if (!prevTrack.isEmpty()) {
        if (m_trackChangeCallback) {
            m_trackChangeCallback(prevTrack);
        } else {
            loadFile(prevTrack);
        }
    }
}

void SystemMediaViewer::toggleLoop()
{
    m_isLooping = !m_isLooping;
    if (m_btnLoop) {
        m_btnLoop->setStyleSheet(m_isLooping
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 16px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 16px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }");
    }
}

void SystemMediaViewer::toggleShuffle()
{
    m_isShuffle = !m_isShuffle;
    if (m_btnShuffle) {
        m_btnShuffle->setStyleSheet(m_isShuffle
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 16px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 16px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }");
    }
}

void SystemMediaViewer::showSpeedMenu()
{
    QMenu speedMenu(this);
    const QList<double> speeds = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    for (double rate : speeds) {
        QString text = QString("%1x").arg(rate, 0, 'f', (rate == 1.0 || rate == 2.0 || rate == 0.5) ? 1 : 2);
        QAction *act = speedMenu.addAction(text);
        if (std::abs(m_playbackRate - rate) < 0.01) {
            act->setCheckable(true);
            act->setChecked(true);
        }
        connect(act, &QAction::triggered, this, [this, rate]() {
            setPlaybackSpeed(rate);
        });
    }
    speedMenu.exec(m_btnSpeed->mapToGlobal(QPoint(0, -speedMenu.sizeHint().height())));
}

void SystemMediaViewer::setPlaybackSpeed(double rate)
{
    m_playbackRate = rate;
    if (m_player) {
        m_player->setPlaybackRate(rate);
    }
    if (m_btnSpeed) {
        m_btnSpeed->setText(QString("%1x").arg(rate, 0, 'f', 1));
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
    QString curStr = formatTime(currentMs);
    QString totStr = formatTime(totalMs);
    qint64 remaining = std::max<qint64>(0, totalMs - currentMs);
    QString remStr = "-" + formatTime(remaining);

    if (m_waveTimeLeftLabel) m_waveTimeLeftLabel->setText(curStr);
    if (m_waveTimeRightLabel) m_waveTimeRightLabel->setText(remStr);
    if (m_timeLeftLabel) m_timeLeftLabel->setText(curStr + " / " + totStr);
    if (m_timeRightLabel) m_timeRightLabel->setText(remStr);
}

QString SystemMediaViewer::formatTime(qint64 ms)
{
    qint64 totalSec = ms / 1000;
    qint64 h = totalSec / 3600;
    qint64 m = (totalSec % 3600) / 60;
    qint64 s = totalSec % 60;
    if (h > 0) {
        return QString("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
