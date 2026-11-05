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
#include <QMouseEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QApplication>
#include <QRandomGenerator>
#include <QSettings>
#include <QCollator>
#include <QSignalBlocker>
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

bool isAudioExtension(const QString &ext)
{
    static const QStringList audioExts = {
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
        QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("aac"),
        QStringLiteral("wma"), QStringLiteral("opus"), QStringLiteral("aiff"),
        QStringLiteral("ape"), QStringLiteral("mac"), QStringLiteral("wv"),
        QStringLiteral("dsf"), QStringLiteral("dff"), QStringLiteral("dsd"),
        QStringLiteral("mpc"), QStringLiteral("mod"), QStringLiteral("xm"),
        QStringLiteral("it"), QStringLiteral("s3m"), QStringLiteral("ac3"),
        QStringLiteral("eac3"), QStringLiteral("dts")
    };
    return audioExts.contains(ext.toLower());
}

} // namespace

SystemMediaViewer::SystemMediaViewer(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #0b0e14;");

    QSettings settings("Bubez", "BubezView");
    m_isAutoAdvance = settings.value("audio/auto_advance", true).toBool();

    // Grace timer after exact seek to suppress transient pre-flush positionChanged signals
    m_seekGraceTimer = new QTimer(this);
    m_seekGraceTimer->setSingleShot(true);
    connect(m_seekGraceTimer, &QTimer::timeout, this, [this]() {
        m_isSeeking = false;
        if (m_player) {
            m_postSeekObservedPos = m_player->position();
        }
    });

    // Post-seek watchdog timer: recovers if GStreamer's GstAudioBaseSink ringbuffer stalls after a seek
    m_seekWatchdogTimer = new QTimer(this);
    m_seekWatchdogTimer->setSingleShot(true);
    connect(m_seekWatchdogTimer, &QTimer::timeout, this, [this]() {
        if (!m_player || m_isSeeking || m_lastSeekTargetMs < 0) {
            return;
        }
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            const qint64 nowPos = m_player->position();
            if (m_durationMs > 0 && nowPos < m_durationMs - 1000 &&
                std::abs(nowPos - m_postSeekObservedPos) < 30) {
                const qint64 recoverTarget = m_lastSeekTargetMs;
                m_player->stop();
                m_player->setPosition(recoverTarget);
                m_player->play();
            }
        }
    });

    // Initialize Qt Multimedia player and audio output pipeline.
    // Note: Video output is intentionally left detached (nullptr) for audio files so
    // GStreamer does not spin up a video sink for embedded cover art or contend with audio seeks.
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0f);
    m_player->setAudioOutput(m_audioOutput);

    setupUi();

    connect(m_player, &QMediaPlayer::positionChanged, this, &SystemMediaViewer::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &SystemMediaViewer::onDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &SystemMediaViewer::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &SystemMediaViewer::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::hasVideoChanged, this, &SystemMediaViewer::onHasVideoChanged);

    installEventFilter(this);
}

SystemMediaViewer::~SystemMediaViewer()
{
    if (m_seekGraceTimer) {
        m_seekGraceTimer->stop();
    }
    if (m_seekWatchdogTimer) {
        m_seekWatchdogTimer->stop();
    }
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

    // Top Studio Master / Quality Pill Chip
    m_audioQualityBadge = new QLabel(m_metaCard);
    m_audioQualityBadge->setAlignment(Qt::AlignCenter);
    m_audioQualityBadge->setVisible(false);
    m_badgeLayout = new QHBoxLayout();
    m_badgeLayout->setContentsMargins(0, 0, 0, 6);
    m_badgeLayout->setAlignment(Qt::AlignLeft);
    m_badgeLayout->addWidget(m_audioQualityBadge, 0, Qt::AlignLeft);
    m_metaLayout->addLayout(m_badgeLayout);

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

    // Tag Inspector Badge Chips (Genre, Track #, Year, Audio Specs, Comment)
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
    m_badgeGenre->setMinimumWidth(0);

    m_badgeTrack = new QLabel(m_tagsBadgeContainer);
    m_badgeTrack->setStyleSheet(badgeStyle);
    m_badgeTrack->setVisible(false);
    m_badgeTrack->setMinimumWidth(0);

    m_badgeDate = new QLabel(m_tagsBadgeContainer);
    m_badgeDate->setStyleSheet(badgeStyle);
    m_badgeDate->setVisible(false);
    m_badgeDate->setMinimumWidth(0);

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
    m_badgeTech->setMinimumWidth(0);

    m_badgeComment = new QLabel(m_tagsBadgeContainer);
    m_badgeComment->setStyleSheet(badgeStyle);
    m_badgeComment->setVisible(false);
    m_badgeComment->setMinimumWidth(0);

    m_tagsBadgeLayout->addWidget(m_badgeGenre);
    m_tagsBadgeLayout->addWidget(m_badgeTrack);
    m_tagsBadgeLayout->addWidget(m_badgeDate);
    m_tagsBadgeLayout->addWidget(m_badgeTech);
    m_tagsBadgeLayout->addWidget(m_badgeComment);

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
    connect(m_waveformScrubber, &SmoothWaveformScrubber::seekRequested, this, &SystemMediaViewer::requestSeek);

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
    m_sliderPosition->installEventFilter(this);
    m_sliderPosition->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: rgba(100, 116, 139, 0.3); border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #0284c7; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #38bdf8; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #7dd3fc; }"
    );
    connect(m_sliderPosition, &QSlider::sliderMoved, this, [this](int val) {
        requestSeek(static_cast<qint64>(val), false);
    });
    connect(m_sliderPosition, &QSlider::sliderPressed, this, [this]() {
        m_isSeeking = true;
    });
    connect(m_sliderPosition, &QSlider::sliderReleased, this, [this]() {
        requestSeek(static_cast<qint64>(m_sliderPosition->value()), true);
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

    m_btnProMode = new QPushButton(tr("⚡ Pro Mode"), leftTrackWidget);
    m_btnProMode->setObjectName("btnProMode");
    m_btnProMode->setToolTip(tr("Activate Pro Mode (libmpv engine, precision seeking, cutter, filters & equalizer)"));
    m_btnProMode->setCursor(Qt::PointingHandCursor);
    m_btnProMode->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f59e0b, stop:1 #d97706);"
        "  border: 1px solid rgba(251, 191, 36, 0.5);"
        "  border-radius: 12px;"
        "  padding: 2px 10px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  color: #0b0e14;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #fbbf24, stop:1 #f59e0b);"
        "  border-color: #fde047;"
        "  color: #000000;"
        "}"
        "QPushButton:pressed {"
        "  background: #b45309;"
        "}"
    );
    connect(m_btnProMode, &QPushButton::clicked, this, [this]() {
        if (m_upgradeToProCallback) {
            m_upgradeToProCallback();
        }
    });

    m_bottomTitleLabel = new QLabel(leftTrackWidget);
    m_bottomTitleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #f8fafc; border: none; background: transparent;");
    m_bottomTitleLabel->setVisible(false);
    m_bottomTitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_bottomTitleLabel->setMinimumWidth(0);

    m_bottomFileLabel = new QLabel(leftTrackWidget);
    m_bottomFileLabel->setStyleSheet("font-size: 10px; font-weight: 600; color: #94a3b8; border: none; background: transparent;");
    m_bottomFileLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_bottomFileLabel->setMinimumWidth(0);

    leftTrackLayout->addWidget(m_btnSpeed);
    leftTrackLayout->addWidget(m_btnProMode);
    leftTrackLayout->addWidget(m_bottomTitleLabel);
    leftTrackLayout->addWidget(m_bottomFileLabel);
    bottomRow->addWidget(leftTrackWidget, 0, Qt::AlignVCenter);

    bottomRow->addStretch(1);

    // Zone 2 (Center): Playback Controls Cluster with Large Circular Play Button
    QHBoxLayout *centerCluster = new QHBoxLayout();
    centerCluster->setSpacing(4);
    centerCluster->setAlignment(Qt::AlignCenter);

    auto makeNavBtn = [this](const QIcon &icon, const QString &tooltip, int sz = 32) {
        QPushButton *btn = new QPushButton(m_controlsWidget);
        btn->setFixedSize(sz, sz);
        btn->setIcon(icon);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 16px; padding: 4px; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.08); }"
        );
        return btn;
    };

    m_btnShuffle = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, QColor("#94a3b8"), 16), tr("Shuffle"), 30);
    connect(m_btnShuffle, &QPushButton::clicked, this, &SystemMediaViewer::toggleShuffle);

    m_btnPrev = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Previous, QColor("#cbd5e1"), 18), tr("Previous track"), 32);
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
    m_btnStop->setVisible(false);

    m_btnNext = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Next, QColor("#cbd5e1"), 18), tr("Next track"), 32);
    connect(m_btnNext, &QPushButton::clicked, this, &SystemMediaViewer::playNextTrack);

    m_btnLoop = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, QColor("#94a3b8"), 16), tr("Loop playback"), 30);
    connect(m_btnLoop, &QPushButton::clicked, this, &SystemMediaViewer::toggleLoop);

    m_btnAutoAdvance = new QPushButton(m_controlsWidget);
    m_btnAutoAdvance->setFixedSize(30, 30);
    m_btnAutoAdvance->setCursor(Qt::PointingHandCursor);
    connect(m_btnAutoAdvance, &QPushButton::clicked, this, &SystemMediaViewer::toggleAutoAdvance);
    updateAutoAdvanceUi();

    m_btnTracklist = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Playlist, QColor("#94a3b8"), 16), tr("Show folder audio tracklist drawer"), 30);
    connect(m_btnTracklist, &QPushButton::clicked, this, &SystemMediaViewer::toggleTracklistDrawer);

    centerCluster->addWidget(m_btnShuffle);
    centerCluster->addWidget(m_btnPrev);
    centerCluster->addWidget(m_btnPlayPause);
    centerCluster->addWidget(m_btnStop);
    centerCluster->addWidget(m_btnNext);
    centerCluster->addWidget(m_btnLoop);
    centerCluster->addWidget(m_btnAutoAdvance);
    centerCluster->addWidget(m_btnTracklist);
    bottomRow->addLayout(centerCluster);

    bottomRow->addStretch(1);

    // Zone 3 (Right): Mute & Volume Slider
    QHBoxLayout *rightCluster = new QHBoxLayout();
    rightCluster->setSpacing(6);
    rightCluster->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_btnMute = makeNavBtn(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18), tr("Volume / Mute"), 30);
    connect(m_btnMute, &QPushButton::clicked, this, &SystemMediaViewer::toggleMute);

    m_sliderVolume = new QSlider(Qt::Horizontal, m_controlsWidget);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(100);
    m_sliderVolume->setMinimumWidth(40);
    m_sliderVolume->setMaximumWidth(82);
    m_sliderVolume->setToolTip(tr("Volume"));
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

    // Collapsible Folder Audio Tracklist Drawer
    m_tracklistDrawer = new QFrame(m_controlsWidget);
    m_tracklistDrawer->setObjectName("TracklistDrawer");
    m_tracklistDrawer->setFixedHeight(170);
    m_tracklistDrawer->setStyleSheet(
        "QFrame#TracklistDrawer {"
        "  background-color: rgba(17, 24, 39, 0.95);"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 10px;"
        "}"
    );
    QVBoxLayout *drawerLayout = new QVBoxLayout(m_tracklistDrawer);
    drawerLayout->setContentsMargins(12, 8, 12, 8);
    drawerLayout->setSpacing(6);

    QHBoxLayout *drawerHeader = new QHBoxLayout();
    drawerHeader->setContentsMargins(0, 0, 0, 0);
    drawerHeader->setSpacing(8);

    QLabel *lblDrawerTitle = new QLabel(tr("Folder Audio Tracklist"), m_tracklistDrawer);
    lblDrawerTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #38bdf8; background: transparent; border: none;");
    drawerHeader->addWidget(lblDrawerTitle);
    drawerHeader->addStretch(1);

    QPushButton *btnDrawerClose = new QPushButton(m_tracklistDrawer);
    btnDrawerClose->setFixedSize(20, 20);
    btnDrawerClose->setCursor(Qt::PointingHandCursor);
    btnDrawerClose->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Close, QColor("#94a3b8"), 12));
    btnDrawerClose->setToolTip(tr("Close Tracklist"));
    btnDrawerClose->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 10px; }"
                                  "QPushButton:hover { background: rgba(255, 255, 255, 0.1); }");
    connect(btnDrawerClose, &QPushButton::clicked, this, [this]() {
        if (m_tracklistDrawer) m_tracklistDrawer->setVisible(false);
    });
    drawerHeader->addWidget(btnDrawerClose);
    drawerLayout->addLayout(drawerHeader);

    m_tracklistWidget = new QListWidget(m_tracklistDrawer);
    m_tracklistWidget->setStyleSheet(
        "QListWidget {"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 11px;"
        "}"
        "QListWidget::item {"
        "  padding: 4px 8px;"
        "  border-radius: 4px;"
        "  color: #cbd5e1;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: rgba(56, 189, 248, 0.12);"
        "  color: #ffffff;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(56, 189, 248, 0.22);"
        "  color: #38bdf8;"
        "  font-weight: bold;"
        "}"
    );
    connect(m_tracklistWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty() && path != m_currentFilePath) {
            if (m_trackChangeCallback) {
                m_trackChangeCallback(path);
            } else {
                loadFile(path);
            }
        }
    });
    drawerLayout->addWidget(m_tracklistWidget, 1);
    m_tracklistDrawer->setVisible(false);

    ctrlRootLayout->addWidget(m_tracklistDrawer);
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

    if (m_seekGraceTimer) m_seekGraceTimer->stop();
    if (m_seekWatchdogTimer) m_seekWatchdogTimer->stop();
    m_isSeeking = false;
    m_lastSeekTargetMs = -1;
    m_postSeekObservedPos = -1;
    m_lastReportedSec = -1;
    m_currentPositionMs = 0;

    // Cleanly stop any previous media pipeline before switching sources
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }

    m_currentFilePath = filePath;
    if (m_siblingFiles.contains(filePath)) {
        m_currentIndex = m_siblingFiles.indexOf(filePath);
    }

    const QString ext = fi.suffix().toLower();
    const bool isVideoExt = isVideoExtension(ext);
    m_isAudioOnly = !isVideoExt;

    if (m_isAudioOnly) {
        // Detach video sink so GStreamer does not spin up a video pipeline for embedded cover art
        if (m_player) {
            m_player->setVideoOutput(nullptr);
        }
        m_videoWidget->hide();
        m_audioCoverWidget->show();
        m_waveformContainer->show();
        m_videoSliderWidget->hide();
        if (m_bottomTitleLabel) m_bottomTitleLabel->setVisible(false);
        if (m_btnTracklist) m_btnTracklist->setVisible(true);
        if (m_btnAutoAdvance) m_btnAutoAdvance->setVisible(true);

        // 1. Read metadata tags and initialize duration immediately
        AudioMetadata meta = AudioProcessor::readMetadata(filePath);
        m_durationMs = (meta.durationMs > 0) ? meta.durationMs : 0;

        QString title = meta.title.isEmpty() ? fi.completeBaseName() : meta.title;
        m_audioTitleLabel->setText(title);
        if (m_bottomTitleLabel) m_bottomTitleLabel->setText(title);

        m_audioArtistLabel->setText(meta.artist.isEmpty() ? tr("Unknown Artist") : meta.artist);
        m_audioArtistLabel->setVisible(true);

        m_audioAlbumLabel->setText(meta.album.isEmpty() ? tr("Unknown Album") : meta.album);
        m_audioAlbumLabel->setVisible(true);

        // Year & File Size
        QString yearStr = meta.year.isEmpty() ? (meta.date.isEmpty() ? QString() : meta.date.left(4)) : meta.year;
        double mb = static_cast<double>(fi.size()) / (1024.0 * 1024.0);
        QString sizeStr = QString("%1 MB").arg(QString::number(mb, 'f', 1));

        // Combined Summary Line ("Year 1973  •  FLAC 2569 kbps  •  21.6 MB")
        QStringList summaryParts;
        if (!yearStr.isEmpty()) {
            summaryParts << tr("Year %1").arg(yearStr);
        }
        QString codecStr = meta.codecName.isEmpty() ? ext.toUpper() : meta.codecName.toUpper();
        if (meta.bitRate > 0) {
            codecStr += QString(" %1 kbps").arg(meta.bitRate);
        }
        summaryParts << codecStr;
        summaryParts << sizeStr;
        m_audioSpecsLabel->setText(summaryParts.join(QStringLiteral("  •  ")));

        // Studio Master & Quality Pill Chip
        if (m_audioQualityBadge) {
            m_audioQualityBadge->setVisible(true);
            bool isLossless = (ext == "flac" || ext == "wav" || ext == "aiff" || ext == "alac" || ext == "ape" || ext == "wv" || ext == "dsf" || ext == "dff");
            bool isHiRes = (meta.bitsPerSample >= 24 || meta.sampleRate >= 48000);
            bool isVinylSource = m_currentFilePath.contains(QStringLiteral("vinyl"), Qt::CaseInsensitive) ||
                                 meta.album.contains(QStringLiteral("vinyl"), Qt::CaseInsensitive) ||
                                 meta.album.contains(QStringLiteral("quad"), Qt::CaseInsensitive) ||
                                 meta.comment.contains(QStringLiteral("vinyl"), Qt::CaseInsensitive);

            QString rateSuffix;
            if (meta.bitsPerSample > 0 && meta.sampleRate > 0) {
                rateSuffix = QString(" • %1-BIT / %2 kHz")
                                 .arg(meta.bitsPerSample)
                                 .arg(QString::number(meta.sampleRate / 1000.0, 'f', (meta.sampleRate % 1000 == 0) ? 0 : 1));
            } else if (meta.sampleRate > 0) {
                rateSuffix = QString(" • %1 kHz")
                                 .arg(QString::number(meta.sampleRate / 1000.0, 'f', (meta.sampleRate % 1000 == 0) ? 0 : 1));
            }

            const QString goldPillStyle =
                "QLabel {"
                "   background: rgba(245, 158, 11, 0.14);"
                "   border: 1px solid rgba(245, 158, 11, 0.45);"
                "   color: #fbbf24;"
                "   font-size: 9px;"
                "   font-weight: bold;"
                "   border-radius: 6px;"
                "   padding: 4px 10px;"
                "}";

            const QString cyanPillStyle =
                "QLabel {"
                "   background: rgba(56, 189, 248, 0.14);"
                "   border: 1px solid rgba(56, 189, 248, 0.42);"
                "   color: #38bdf8;"
                "   font-size: 9px;"
                "   font-weight: bold;"
                "   border-radius: 6px;"
                "   padding: 4px 10px;"
                "}";

            if (isHiRes && isLossless) {
                m_audioQualityBadge->setText((isVinylSource ? tr("VINYL MASTER") : tr("HI-RES MASTER")) + rateSuffix);
                m_audioQualityBadge->setStyleSheet(goldPillStyle);
            } else if (isLossless) {
                if (isVinylSource) {
                    m_audioQualityBadge->setText(tr("VINYL LOSSLESS") + rateSuffix);
                    m_audioQualityBadge->setStyleSheet(goldPillStyle);
                } else {
                    m_audioQualityBadge->setText(tr("LOSSLESS STUDIO") + rateSuffix);
                    m_audioQualityBadge->setStyleSheet(cyanPillStyle);
                }
            } else {
                m_audioQualityBadge->setText(QString("%1 AUDIO%2").arg(codecStr.split(' ').first(), rateSuffix));
                m_audioQualityBadge->setStyleSheet(cyanPillStyle);
            }
        }

        // Update Tag Inspector Badges
        if (!meta.genre.isEmpty()) {
            m_badgeGenre->setText(QString("🏷 %1").arg(meta.genre));
            m_badgeGenre->setVisible(true);
        } else {
            m_badgeGenre->clear();
            m_badgeGenre->setVisible(false);
        }

        if (!meta.trackNumber.isEmpty()) {
            m_badgeTrack->setText(QString("#%1").arg(meta.trackNumber));
            m_badgeTrack->setVisible(true);
        } else {
            m_badgeTrack->clear();
            m_badgeTrack->setVisible(false);
        }

        if (!yearStr.isEmpty()) {
            m_badgeDate->setText(QString("📅 %1").arg(yearStr));
            m_badgeDate->setVisible(true);
        } else {
            m_badgeDate->clear();
            m_badgeDate->setVisible(false);
        }

        QStringList techInfo;
        if (meta.channels > 0) {
            techInfo << (meta.channels == 1 ? tr("Mono") : (meta.channels == 2 ? tr("Stereo") : tr("%1 Ch").arg(meta.channels)));
        }
        if (meta.sampleRate > 0) {
            techInfo << QString("%1 kHz").arg(QString::number(meta.sampleRate / 1000.0, 'f', (meta.sampleRate % 1000 == 0) ? 0 : 1));
        }
        if (meta.bitsPerSample > 0) {
            techInfo << QString("%1-bit").arg(meta.bitsPerSample);
        }
        if (!techInfo.isEmpty()) {
            m_badgeTech->setText(techInfo.join(QStringLiteral(" • ")));
            m_badgeTech->setVisible(true);
        } else {
            m_badgeTech->clear();
            m_badgeTech->setVisible(false);
        }

        if (m_badgeComment) {
            if (!meta.comment.isEmpty()) {
                QString commentPreview = meta.comment;
                if (commentPreview.length() > 30) commentPreview = commentPreview.left(27) + "...";
                m_badgeComment->setText(QString("💬 %1").arg(commentPreview));
                m_badgeComment->setToolTip(meta.comment);
                m_badgeComment->setVisible(true);
            } else {
                m_badgeComment->clear();
                m_badgeComment->setVisible(false);
            }
        }

        // 2. Extract album cover art and vinyl label
        QPixmap sleeveCover = AudioProcessor::extractCoverArt(filePath, 260);
        QPixmap vinylLabel = AudioProcessor::extractVinylLabelArt(filePath, 160);
        m_vinylSleeveWidget->setArtwork(sleeveCover, vinylLabel);

        // 3. Load smooth waveform scrubber and prime initial duration so clicks work immediately
        m_waveformScrubber->loadAudioWaveform(filePath);
        m_waveformScrubber->setPlaybackPosition(0, m_durationMs);

        // 4. Scan folder for all audio tracks to power Next, Previous, Shuffle, and Tracklist Drawer
        if (m_audioSiblingFiles.isEmpty()) {
            scanAudioSiblings();
        }
        QString cleanCurrent = fi.canonicalFilePath();
        if (cleanCurrent.isEmpty()) cleanCurrent = fi.absoluteFilePath();
        int curIdx = m_audioSiblingFiles.indexOf(cleanCurrent);
        if (curIdx < 0) {
            QString curName = fi.fileName();
            for (int i = 0; i < m_audioSiblingFiles.size(); ++i) {
                if (QFileInfo(m_audioSiblingFiles.at(i)).fileName() == curName) {
                    curIdx = i;
                    break;
                }
            }
        }
        if (curIdx >= 0) {
            m_currentIndex = curIdx;
        }
        if (curIdx >= 0 && m_audioSiblingFiles.size() > 1) {
            if (m_bottomFileLabel) {
                m_bottomFileLabel->setText(QString("•  Hi-Fi  [%1/%2]").arg(curIdx + 1).arg(m_audioSiblingFiles.size()));
            }
            int nextIdx = (curIdx + 1) % m_audioSiblingFiles.size();
            int prevIdx = (curIdx - 1 + m_audioSiblingFiles.size()) % m_audioSiblingFiles.size();
            if (m_btnNext) m_btnNext->setToolTip(tr("Next: %1").arg(QFileInfo(m_audioSiblingFiles[nextIdx]).fileName()));
            if (m_btnPrev) m_btnPrev->setToolTip(tr("Previous: %1").arg(QFileInfo(m_audioSiblingFiles[prevIdx]).fileName()));
        } else {
            if (m_bottomFileLabel) m_bottomFileLabel->setText(QStringLiteral("•  Hi-Fi"));
            if (m_btnNext) m_btnNext->setToolTip(tr("Next track"));
            if (m_btnPrev) m_btnPrev->setToolTip(tr("Previous track"));
        }

        if (m_tracklistDrawer && m_tracklistDrawer->isVisible()) {
            updateTracklistDrawer();
        }
    } else {
        m_durationMs = 0;
        if (m_player) {
            m_player->setVideoOutput(m_videoWidget);
        }
        m_audioCoverWidget->hide();
        m_videoWidget->show();
        m_waveformContainer->hide();
        m_videoSliderWidget->show();
        if (m_tracklistDrawer) m_tracklistDrawer->setVisible(false);
        if (m_btnTracklist) m_btnTracklist->setVisible(false);
        if (m_btnAutoAdvance) m_btnAutoAdvance->setVisible(false);
        if (m_bottomTitleLabel) {
            m_bottomTitleLabel->setText(fi.completeBaseName());
            m_bottomTitleLabel->setVisible(true);
        }
        if (m_bottomFileLabel) {
            m_bottomFileLabel->setText(fi.fileName());
        }
    }

    if (m_sliderPosition) {
        QSignalBlocker blocker(m_sliderPosition);
        m_sliderPosition->setRange(0, static_cast<int>(m_durationMs));
        m_sliderPosition->setValue(0);
    }
    updateTimeLabel(0, m_durationMs);

    if (m_player) {
        m_player->setPlaybackRate(m_playbackRate);
        m_player->setSource(QUrl::fromLocalFile(filePath));
        m_player->play();
    }

    updatePlayPauseUi(true);
    updateAudioResponsiveLayout();
    return true;
}

void SystemMediaViewer::clear()
{
    stop();
    m_currentFilePath.clear();
    m_durationMs = 0;
    m_currentPositionMs = 0;
    m_lastReportedSec = -1;
    m_lastSeekTargetMs = -1;
    m_postSeekObservedPos = -1;
    m_vinylSleeveWidget->clearArtwork();
    m_waveformScrubber->reset();
    if (m_audioQualityBadge) m_audioQualityBadge->setVisible(false);
    m_audioTitleLabel->clear();
    m_audioArtistLabel->clear();
    m_audioAlbumLabel->clear();
    m_audioSpecsLabel->clear();
    m_badgeGenre->clear();
    m_badgeGenre->setVisible(false);
    m_badgeTrack->clear();
    m_badgeTrack->setVisible(false);
    m_badgeDate->clear();
    m_badgeDate->setVisible(false);
    m_badgeTech->clear();
    m_badgeTech->setVisible(false);
    if (m_badgeComment) {
        m_badgeComment->clear();
        m_badgeComment->setVisible(false);
    }
    if (m_sliderPosition) {
        QSignalBlocker blocker(m_sliderPosition);
        m_sliderPosition->setValue(0);
    }
    updateTimeLabel(0, 0);
}

void SystemMediaViewer::stop()
{
    if (m_seekGraceTimer) m_seekGraceTimer->stop();
    if (m_seekWatchdogTimer) m_seekWatchdogTimer->stop();
    m_isSeeking = false;
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
        if (m_player->mediaStatus() == QMediaPlayer::EndOfMedia) {
            m_player->setPosition(0);
        }
        m_player->play();
    }
}

void SystemMediaViewer::setSiblingFiles(const QStringList &files)
{
    m_siblingFiles = files;
    m_audioSiblingFiles.clear();
    for (const QString &f : files) {
        QFileInfo fi(f);
        if (isAudioExtension(fi.suffix())) {
            QString clean = fi.canonicalFilePath();
            if (clean.isEmpty()) clean = fi.absoluteFilePath();
            if (!m_audioSiblingFiles.contains(clean)) {
                m_audioSiblingFiles.append(clean);
            }
        }
    }
    if (!m_currentFilePath.isEmpty()) {
        const QStringList &activeList = (m_isAudioOnly && !m_audioSiblingFiles.isEmpty()) ? m_audioSiblingFiles : m_siblingFiles;
        int idx = activeList.indexOf(m_currentFilePath);
        if (idx < 0) {
            QString canon = QFileInfo(m_currentFilePath).canonicalFilePath();
            if (!canon.isEmpty()) idx = activeList.indexOf(canon);
        }
        if (idx >= 0) {
            m_currentIndex = idx;
        }
    }
}

void SystemMediaViewer::scanAudioSiblings()
{
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QDir dir = fi.dir();

    QStringList files = dir.entryList(QDir::Files, QDir::NoSort);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), collator);

    QStringList detected;
    for (const QString &f : files) {
        if (f.startsWith('.')) continue;
        QFileInfo entryFi(dir.absoluteFilePath(f));
        if (isAudioExtension(entryFi.suffix())) {
            QString clean = entryFi.canonicalFilePath();
            if (clean.isEmpty()) clean = entryFi.absoluteFilePath();
            detected.append(clean);
        }
    }

    if (!detected.isEmpty()) {
        m_audioSiblingFiles = detected;
    }
}

void SystemMediaViewer::toggleTracklistDrawer()
{
    if (!m_tracklistDrawer) return;
    bool willShow = !m_tracklistDrawer->isVisible();
    if (willShow) {
        updateTracklistDrawer();
    }
    m_tracklistDrawer->setVisible(willShow);
}

void SystemMediaViewer::updateTracklistDrawer()
{
    if (!m_tracklistWidget) return;
    if (m_audioSiblingFiles.isEmpty() && !m_currentFilePath.isEmpty()) {
        scanAudioSiblings();
    }

    m_tracklistWidget->clear();
    int currentIndex = -1;
    QString cleanCurrent = QFileInfo(m_currentFilePath).canonicalFilePath();
    if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(m_currentFilePath).absoluteFilePath();

    for (int i = 0; i < m_audioSiblingFiles.size(); ++i) {
        const QString &path = m_audioSiblingFiles.at(i);
        QFileInfo fi(path);
        auto *item = new QListWidgetItem(m_tracklistWidget);
        item->setData(Qt::UserRole, path);
        item->setText(QString("%1. %2").arg(i + 1, 2, 10, QChar('0')).arg(fi.fileName()));
        item->setToolTip(path);
        if (path == m_currentFilePath || path == cleanCurrent) {
            currentIndex = i;
            item->setForeground(QBrush(QColor("#38bdf8")));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        } else {
            item->setForeground(QBrush(QColor("#cbd5e1")));
        }
    }

    if (currentIndex >= 0) {
        m_tracklistWidget->setCurrentRow(currentIndex);
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
    updateLoopButtonStyle();
    updateAutoAdvanceUi();
    if (m_isAudioOnly) {
        updateAudioResponsiveLayout();
    }
}

void SystemMediaViewer::promptMpvRequired(const QString &toolName)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Multimedia Studio Module Required"));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(tr("<h3>%1 Requires Multimedia Studio (Pro)</h3>"
                      "<p>Advanced media playback, precision seeking, audio trimming, pitch shifting, "
                      "and equalizer tools require the <b>Multimedia Studio (MPV)</b> engine.</p>"
                      "<p>Would you like to activate Pro Mode now?</p>")
                       .arg(toolName));

    QPushButton *btnPro = msgBox.addButton(tr("⚡ Activate Pro Mode"), QMessageBox::AcceptRole);
    btnPro->setStyleSheet("font-weight: bold; color: #0284c7;");
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    Q_UNUSED(btnCancel);
    msgBox.setDefaultButton(btnPro);

    msgBox.exec();
    if (msgBox.clickedButton() == btnPro && m_upgradeToProCallback) {
        m_upgradeToProCallback();
    }
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
    if (watched == m_sliderPosition) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                int w = m_sliderPosition->width();
                if (w > 0 && m_durationMs > 0) {
                    double frac = static_cast<double>(me->pos().x()) / static_cast<double>(w);
                    frac = std::clamp(frac, 0.0, 1.0);
                    qint64 targetVal = static_cast<qint64>(frac * m_durationMs);
                    {
                        QSignalBlocker blocker(m_sliderPosition);
                        m_sliderPosition->setValue(static_cast<int>(targetVal));
                    }
                    requestSeek(targetVal, false);
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                requestSeek(static_cast<qint64>(m_sliderPosition->value()), true);
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Space) {
            togglePlayPause();
            return true;
        }
        if (ke->key() == Qt::Key_Left) {
            requestSeek(std::max<qint64>(0, m_currentPositionMs - 5000), true);
            return true;
        }
        if (ke->key() == Qt::Key_Right) {
            qint64 upper = m_durationMs > 0 ? m_durationMs : m_currentPositionMs + 5000;
            requestSeek(std::min<qint64>(upper, m_currentPositionMs + 5000), true);
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

        if (m_audioCoverWidget && m_audioCoverWidget->layout()) {
            m_audioCoverWidget->layout()->setContentsMargins(12, 4, 12, 2);
        }

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

        if (m_audioQualityBadge) {
            m_audioQualityBadge->setAlignment(Qt::AlignCenter);
        }
        if (m_badgeLayout) {
            m_badgeLayout->setAlignment(Qt::AlignCenter);
            m_badgeLayout->setContentsMargins(0, 0, 0, 4);
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
            m_tagsBadgeLayout->setContentsMargins(0, 2, 0, 0);
        }

        // Adaptive priority badges in compact height
        const bool lowHeight = (h > 0 && h < 420);
        if (m_tagsBadgeContainer) {
            m_tagsBadgeContainer->setVisible(!lowHeight);
        }
        if (m_badgeGenre) {
            m_badgeGenre->setVisible(!lowHeight && !m_badgeGenre->text().isEmpty());
        }
        if (m_badgeDate) {
            m_badgeDate->setVisible(!lowHeight && (h >= 460) && !m_badgeDate->text().isEmpty());
        }
        if (m_badgeTrack) m_badgeTrack->setVisible(false);
        if (m_badgeTech) m_badgeTech->setVisible(false);
        if (m_badgeComment) m_badgeComment->setVisible(false);
    } else {
        m_isCompactAudioLayout = false;
        m_audioTopLayout->setDirection(QBoxLayout::LeftToRight);
        m_audioTopLayout->setSpacing(14);

        if (m_audioCoverWidget && m_audioCoverWidget->layout()) {
            m_audioCoverWidget->layout()->setContentsMargins(16, 8, 16, 4);
        }

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

        if (m_audioQualityBadge) {
            m_audioQualityBadge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (m_badgeLayout) {
            m_badgeLayout->setAlignment(Qt::AlignLeft);
            m_badgeLayout->setContentsMargins(0, 0, 0, 6);
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
            m_audioSpecsLabel->setVisible(!m_audioSpecsLabel->text().isEmpty());
            m_audioSpecsLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }

        if (m_tagsBadgeContainer) {
            m_tagsBadgeContainer->setVisible(true);
        }
        if (m_tagsBadgeLayout) {
            m_tagsBadgeLayout->setAlignment(Qt::AlignLeft);
            m_tagsBadgeLayout->setContentsMargins(0, 4, 0, 0);
        }

        if (m_badgeGenre) m_badgeGenre->setVisible(!m_badgeGenre->text().isEmpty());
        if (m_badgeTrack) m_badgeTrack->setVisible(!m_badgeTrack->text().isEmpty());
        if (m_badgeDate) m_badgeDate->setVisible(!m_badgeDate->text().isEmpty());
        if (m_badgeTech) m_badgeTech->setVisible(!m_badgeTech->text().isEmpty());
        if (m_badgeComment) m_badgeComment->setVisible(!m_badgeComment->text().isEmpty());
    }

    if (m_audioCoverWidget) {
        m_audioCoverWidget->updateGeometry();
    }
    updateGeometry();
}

void SystemMediaViewer::onPositionChanged(qint64 position)
{
    if (m_isSeeking || (m_sliderPosition && m_sliderPosition->isSliderDown())) {
        return;
    }

    m_currentPositionMs = position;

    if (m_sliderPosition) {
        QSignalBlocker blocker(m_sliderPosition);
        m_sliderPosition->setValue(static_cast<int>(position));
    }
    if (m_waveformScrubber) {
        m_waveformScrubber->setPlaybackPosition(position, m_durationMs);
    }

    const qint64 currentSec = position / 1000;
    if (currentSec != m_lastReportedSec || position == 0) {
        m_lastReportedSec = currentSec;
        updateTimeLabel(position, m_durationMs);
    }
}

void SystemMediaViewer::onDurationChanged(qint64 duration)
{
    if (duration <= 0) return;
    m_durationMs = duration;
    if (m_sliderPosition) {
        QSignalBlocker blocker(m_sliderPosition);
        m_sliderPosition->setRange(0, static_cast<int>(duration));
    }
    if (m_waveformScrubber) {
        m_waveformScrubber->setPlaybackPosition(m_currentPositionMs, duration);
    }
    updateTimeLabel(m_currentPositionMs, duration);
}

void SystemMediaViewer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    updatePlayPauseUi(state == QMediaPlayer::PlayingState);
}

void SystemMediaViewer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        // Dispatch via queued single-shot timer to avoid mutating QMediaPlayer state
        // re-entrantly inside GStreamer's EndOfMedia signal handler.
        QTimer::singleShot(0, this, [this]() {
            if (!m_player) return;
            if (m_isLooping) {
                requestSeek(0, true);
                m_player->play();
            } else if (!m_isAudioOnly || m_isAutoAdvance) {
                playNextTrack();
            } else {
                stop();
                m_currentPositionMs = 0;
                if (m_sliderPosition) {
                    QSignalBlocker blocker(m_sliderPosition);
                    m_sliderPosition->setValue(0);
                }
                if (m_waveformScrubber) {
                    m_waveformScrubber->setPlaybackPosition(0, m_durationMs);
                }
                updateTimeLabel(0, m_durationMs);
            }
        });
    }
}

void SystemMediaViewer::onHasVideoChanged(bool hasVideo)
{
    // Ignore embedded cover art video streams (e.g. MJPEG album art in MP3/FLAC files)
    if (m_isAudioOnly) {
        return;
    }

    if (hasVideo) {
        m_audioCoverWidget->hide();
        m_videoWidget->show();
        m_waveformContainer->hide();
        m_videoSliderWidget->show();
    }
}

void SystemMediaViewer::requestSeek(qint64 targetMs, bool exact)
{
    if (!m_player) return;

    if (m_durationMs > 0) {
        targetMs = std::clamp<qint64>(targetMs, 0, m_durationMs);
    } else {
        targetMs = std::max<qint64>(0, targetMs);
    }

    // 1. Update UI timestamps and waveform playhead immediately with zero lag
    m_isSeeking = true;
    m_currentPositionMs = targetMs;
    m_lastReportedSec = targetMs / 1000;
    updateTimeLabel(targetMs, m_durationMs);

    if (m_waveformScrubber) {
        m_waveformScrubber->setPlaybackPosition(targetMs, m_durationMs);
    }
    if (m_sliderPosition && !m_sliderPosition->isSliderDown()) {
        QSignalBlocker blocker(m_sliderPosition);
        m_sliderPosition->setValue(static_cast<int>(targetMs));
    }

    // 2. Only commit the flushing pipeline seek on mouse release / exact seek!
    // Sending rapid back-to-back flushing seeks during mouse drag triggers a 64-bit
    // unsigned clock skew underflow in GStreamer 1.22's GstAudioBaseSink.
    if (exact) {
        m_lastSeekTargetMs = targetMs;
        m_postSeekObservedPos = targetMs;
        m_player->setPosition(targetMs);

        if (m_seekGraceTimer) {
            m_seekGraceTimer->start(150);
        } else {
            m_isSeeking = false;
        }
        if (m_seekWatchdogTimer && m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_seekWatchdogTimer->start(650);
        }
    }
}

void SystemMediaViewer::seekTo(qint64 positionMs)
{
    requestSeek(positionMs, true);
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
        QSignalBlocker blocker(m_sliderVolume);
        m_sliderVolume->setValue(volumePercent);
    }
    if (m_btnMute) {
        const bool muted = (volumePercent == 0) || (m_audioOutput && m_audioOutput->isMuted());
        m_btnMute->setIcon(VectorIcons::createUiIcon(
            muted ? VectorIcons::UiIcon::VolumeMute : VectorIcons::UiIcon::Volume,
            muted ? QColor("#ef4444") : QColor("#cbd5e1"),
            18
        ));
    }
}

void SystemMediaViewer::toggleMute()
{
    if (!m_audioOutput) return;
    bool isMuted = m_audioOutput->isMuted();
    m_audioOutput->setMuted(!isMuted);
    if (!isMuted) {
        m_previousVolume = m_audioOutput->volume();
        m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::VolumeMute, QColor("#ef4444"), 18));
    } else {
        m_btnMute->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
    }
}

void SystemMediaViewer::playNextTrack()
{
    if (m_isAudioOnly && m_audioSiblingFiles.isEmpty() && !m_currentFilePath.isEmpty()) {
        scanAudioSiblings();
    }
    const QStringList &activeList = (m_isAudioOnly && !m_audioSiblingFiles.isEmpty()) ? m_audioSiblingFiles : m_siblingFiles;
    if (activeList.isEmpty()) return;

    int curIdx = m_currentIndex;
    if (!m_currentFilePath.isEmpty()) {
        QString cleanCurrent = QFileInfo(m_currentFilePath).canonicalFilePath();
        if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(m_currentFilePath).absoluteFilePath();
        int found = activeList.indexOf(cleanCurrent);
        if (found < 0) {
            QString curName = QFileInfo(m_currentFilePath).fileName();
            for (int i = 0; i < activeList.size(); ++i) {
                if (QFileInfo(activeList.at(i)).fileName() == curName) {
                    found = i;
                    break;
                }
            }
        }
        if (found >= 0) {
            curIdx = found;
        }
    }

    int nextIdx = 0;
    if (m_isShuffle && activeList.size() > 1) {
        nextIdx = curIdx;
        while (nextIdx == curIdx) {
            nextIdx = QRandomGenerator::global()->bounded(activeList.size());
        }
    } else {
        nextIdx = (curIdx + 1) % activeList.size();
    }
    m_currentIndex = nextIdx;

    QString nextTrack = activeList.value(m_currentIndex);
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
    if (m_isAudioOnly && m_audioSiblingFiles.isEmpty() && !m_currentFilePath.isEmpty()) {
        scanAudioSiblings();
    }
    const QStringList &activeList = (m_isAudioOnly && !m_audioSiblingFiles.isEmpty()) ? m_audioSiblingFiles : m_siblingFiles;
    if (activeList.isEmpty()) return;

    // If already played for more than 3 seconds, rewind current track to start
    if (m_currentPositionMs > 3000) {
        requestSeek(0, true);
        return;
    }

    int curIdx = m_currentIndex;
    if (!m_currentFilePath.isEmpty()) {
        QString cleanCurrent = QFileInfo(m_currentFilePath).canonicalFilePath();
        if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(m_currentFilePath).absoluteFilePath();
        int found = activeList.indexOf(cleanCurrent);
        if (found < 0) {
            QString curName = QFileInfo(m_currentFilePath).fileName();
            for (int i = 0; i < activeList.size(); ++i) {
                if (QFileInfo(activeList.at(i)).fileName() == curName) {
                    found = i;
                    break;
                }
            }
        }
        if (found >= 0) {
            curIdx = found;
        }
    }

    int prevIdx = 0;
    if (m_isShuffle && activeList.size() > 1) {
        prevIdx = curIdx;
        while (prevIdx == curIdx) {
            prevIdx = QRandomGenerator::global()->bounded(activeList.size());
        }
    } else {
        if (curIdx > 0) {
            prevIdx = curIdx - 1;
        } else {
            prevIdx = activeList.size() - 1;
        }
    }
    m_currentIndex = prevIdx;

    QString prevTrack = activeList.value(m_currentIndex);
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
    updateLoopButtonStyle();
}

void SystemMediaViewer::updateLoopButtonStyle()
{
    const QColor subIconColor("#94a3b8");
    if (m_btnLoop) {
        m_btnLoop->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, m_isLooping ? QColor("#38bdf8") : subIconColor, 18));
        m_btnLoop->setStyleSheet(m_isLooping
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }");
    }
}

void SystemMediaViewer::toggleShuffle()
{
    m_isShuffle = !m_isShuffle;
    if (m_btnShuffle) {
        m_btnShuffle->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, m_isShuffle ? QColor("#38bdf8") : QColor("#94a3b8"), 16));
        m_btnShuffle->setToolTip(m_isShuffle ? tr("Shuffle: ON") : tr("Shuffle: OFF"));
        m_btnShuffle->setStyleSheet(m_isShuffle
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }");
    }
}

void SystemMediaViewer::toggleAutoAdvance()
{
    m_isAutoAdvance = !m_isAutoAdvance;
    QSettings settings("Bubez", "BubezView");
    settings.setValue("audio/auto_advance", m_isAutoAdvance);
    updateAutoAdvanceUi();
}

void SystemMediaViewer::updateAutoAdvanceUi()
{
    const QColor subIconColor("#94a3b8");
    if (m_btnAutoAdvance) {
        m_btnAutoAdvance->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::AutoAdvance, m_isAutoAdvance ? QColor("#38bdf8") : subIconColor, 18));
        m_btnAutoAdvance->setToolTip(m_isAutoAdvance
            ? tr("Continuous Playback (Auto-Advance): ON\nAutomatically plays the next track when the current one ends.")
            : tr("Continuous Playback (Auto-Advance): OFF\nPlayback stops at the end of the current track. Click to enable."));
        m_btnAutoAdvance->setStyleSheet(m_isAutoAdvance
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }");
    }
}

void SystemMediaViewer::showSpeedMenu()
{
    QMenu speedMenu(this);
    const QList<double> speeds = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
    for (double rate : speeds) {
        QString text = QString("%1x").arg(rate, 0, 'f', 2);
        QAction *act = speedMenu.addAction(text);
        act->setCheckable(true);
        act->setChecked(std::abs(m_playbackRate - rate) < 0.01);
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
        m_btnPlayPause->setToolTip(isPlaying ? tr("Pause") : tr("Play"));
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
