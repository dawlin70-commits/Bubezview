#include "media_viewer.h"
#include "stereo_oscilloscope_widget.h"
#include "audio_trim_dialog.h"
#include "audio_tag_dialog.h"
#include "core/audio_processor.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include <QFileInfo>
#include <QTime>
#include <QStyle>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QRandomGenerator>
#include <QSettings>
#include <QProcess>
#include <QDir>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QDebug>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnection>
#include <QCollator>
#include <algorithm>
#include <cstring>
#include <clocale>

MediaViewer::MediaViewer(QWidget *parent)
    : QWidget(parent)
    , m_mpv(nullptr)
    , m_videoWidget(nullptr)
    , m_audioCoverWidget(nullptr)
    , m_audioCoverImageLabel(nullptr)
    , m_metaCard(nullptr)
    , m_audioTitleLabel(nullptr)
    , m_audioArtistLabel(nullptr)
    , m_audioAlbumLabel(nullptr)
    , m_audioYearLabel(nullptr)
    , m_audioQualityBadge(nullptr)
    , m_audioSpecsLabel(nullptr)
    , m_audioFileDetailsLabel(nullptr)
    , m_audioOscilloscope(nullptr)
    , m_controlsWidget(nullptr)
    , m_sliderPosition(nullptr)
    , m_timeLeftLabel(nullptr)
    , m_timeRightLabel(nullptr)
    , m_bottomTitleLabel(nullptr)
    , m_bottomFileLabel(nullptr)
    , m_btnShuffle(nullptr)
    , m_btnPrev(nullptr)
    , m_btnPlayPause(nullptr)
    , m_btnNext(nullptr)
    , m_btnLoop(nullptr)
    , m_btnAutoAdvance(nullptr)
    , m_volumePercentLabel(nullptr)
    , m_btnVolume(nullptr)
    , m_sliderVolume(nullptr)
    , m_btnSpeed(nullptr)
    , m_btnBitPerfect(nullptr)
    , m_btnAudioDevice(nullptr)
    , m_btnAudioTracks(nullptr)
    , m_btnSubtitles(nullptr)
    , m_btnTrim(nullptr)
    , m_btnTags(nullptr)
    , m_btnEqualizer(nullptr)
    , m_btnExtractAudio(nullptr)
    , m_equalizerDialog(nullptr)
    , m_equalizerWidget(nullptr)
    , m_currentFilePath()
    , m_siblingFiles()
    , m_audioSiblingFiles()
    , m_playbackRate(1.0)
    , m_durationMs(0)
    , m_positionMs(0)
    , m_lastSliderPos(-1)
    , m_lastReportedSec(-1)
    , m_isAudioOnly(false)
    , m_isLooping(false)
    , m_isAutoAdvance(true)
    , m_isShuffle(false)
    , m_isPlaying(false)
    , m_isBitPerfect(false)
    , m_currentAudioDevice("auto")
    , m_lastBitPerfectDevice()
    , m_isUserSeeking(false)
    , m_pendingSeekMs(-1)
    , m_seekThrottleTimer(nullptr)
    , m_reloadTimer(nullptr)
{
    // libmpv requires LC_NUMERIC to be "C"
    std::setlocale(LC_NUMERIC, "C");

    // Seek throttle timer to avoid flooding mpv while user drags slider/waveform
    m_seekThrottleTimer = new QTimer(this);
    m_seekThrottleTimer->setSingleShot(true);
    connect(m_seekThrottleTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingSeekMs >= 0) {
            executeSeek(m_pendingSeekMs, false);
        }
    });

    // Load persistent Bit-Perfect audiophile setting and preferred output device
    QSettings settings("Bubez", "BubezView");
    m_isAutoAdvance = settings.value("audio/auto_advance", true).toBool();
    m_isBitPerfect = settings.value("audio/bit_perfect", false).toBool();
    m_lastBitPerfectDevice = settings.value("audio/last_bitperfect_device", "").toString();
    m_currentAudioDevice = settings.value("audio/device", "auto").toString();

    // Ensure audio device is consistently in sync with Bit-Perfect mode:
    if (m_isBitPerfect) {
        if (m_currentAudioDevice == "auto" || !m_currentAudioDevice.startsWith("alsa/")) {
            if (!m_lastBitPerfectDevice.isEmpty() && m_lastBitPerfectDevice.startsWith("alsa/")) {
                m_currentAudioDevice = m_lastBitPerfectDevice;
            } else {
                m_currentAudioDevice = detectDefaultAlsaDevice();
                m_lastBitPerfectDevice = m_currentAudioDevice;
            }
        }
    } else {
        m_currentAudioDevice = "auto";
    }

    // 1. Initialize libmpv core engine
    m_mpv = mpv_create();
    if (m_mpv) {
        mpv_set_option_string(m_mpv, "terminal", "yes");
        mpv_set_option_string(m_mpv, "msg-level", "ao=v,alsa=v,audio=v");
        mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
        mpv_set_option_string(m_mpv, "vo", "libmpv");
        if (m_isBitPerfect) {
            mpv_set_option_string(m_mpv, "audio-resample", "no");
            mpv_set_option_string(m_mpv, "audio-pitch-correction", "no");
            mpv_set_option_string(m_mpv, "gapless-audio", "yes");
        } else {
            mpv_set_option_string(m_mpv, "audio-pitch-correction", "yes");
            // Studio-grade audiophile resampling via libsoxr (28-bit linear phase, zero aliasing)
            mpv_set_option_string(m_mpv, "audio-swresample-o", "resampler=soxr,precision=28");
            mpv_set_option_string(m_mpv, "audio-resample-filter-size", "32");
            mpv_set_option_string(m_mpv, "audio-resample-linear", "yes");
        }
        // Prevent mpv from squashing stereo dynamic range
        mpv_set_option_string(m_mpv, "audio-normalize-downmix", "no");
        mpv_set_option_string(m_mpv, "audio-fallback-to-null", "no");

        if (m_currentAudioDevice.startsWith("alsa/")) {
            mpv_set_option_string(m_mpv, "ao", "alsa");
            mpv_set_option_string(m_mpv, "audio-exclusive", "yes");
            QByteArray devBytes = m_currentAudioDevice.toUtf8();
            mpv_set_option_string(m_mpv, "audio-device", devBytes.constData());
        } else {
            mpv_set_option_string(m_mpv, "ao", "pulse,pipewire,alsa,jack,null");
            mpv_set_option_string(m_mpv, "audio-exclusive", "no");
            mpv_set_option_string(m_mpv, "audio-device", "auto");
        }

        mpv_set_option_string(m_mpv, "keep-open", "yes");
        mpv_set_option_string(m_mpv, "idle", "yes");

        // Observe playback and metadata properties
        mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
        mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);
        mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
        mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
        mpv_observe_property(m_mpv, 0, "metadata", MPV_FORMAT_NODE);

        // Async event notification callback
        mpv_set_wakeup_callback(m_mpv, [](void *ctx) {
            QMetaObject::invokeMethod(static_cast<MediaViewer *>(ctx), "processMpvEvents", Qt::QueuedConnection);
        }, this);

        mpv_initialize(m_mpv);
    }

    setupUi();

    if (m_videoWidget && m_mpv) {
        m_videoWidget->setMpvHandle(m_mpv);
    }

    applyAudioDeviceSettings();
    applyBitPerfectSettings();
    applyVolume(100);
}

MediaViewer::~MediaViewer()
{
    stop();
    if (m_isBitPerfect || m_currentAudioDevice.startsWith("alsa/")) {
        resetAudioMixer();
    }
    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    }
    if (m_videoWidget) {
        m_videoWidget->cleanup();
    }
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MediaViewer::resetAudioMixer()
{
    // Safely release ALSA hardware device reservation via standard FreeDesktop DBus ReserveDevice1
    // protocol without abruptly restarting the desktop sound server (PipeWire/WirePlumber).
    requestAlsaDeviceRelease();
}

void MediaViewer::requestAlsaDeviceRelease(int cardIndex)
{
    // Request PipeWire / WirePlumber to release ALSA hardware endpoints via FreeDesktop ReserveDevice1 standard protocol
    for (int card = 0; card < 4; ++card) {
        if (cardIndex >= 0 && card != cardIndex) continue;
        QString service = QString("org.freedesktop.ReserveDevice1.Audio%1").arg(card);
        QString path = QString("/org/freedesktop/ReserveDevice1/Audio%1").arg(card);
        QDBusInterface iface(service, path, "org.freedesktop.ReserveDevice1", QDBusConnection::sessionBus());
        if (iface.isValid()) {
            QDBusReply<bool> reply = iface.call("RequestRelease", 50);
            if (reply.isValid() && reply.value()) {
                qDebug() << "[BubezView-Audio] Successfully released ALSA hardware device" << service << "via DBus ReserveDevice1";
            }
        }
    }

    // Distrobox host execution fallback for pw-reserve if available
    if (QFile::exists("/usr/bin/distrobox-host-exec")) {
        QProcess::startDetached("/usr/bin/distrobox-host-exec", {"pw-reserve", "-n", "Audio0", "-r", "-p", "50"});
        QProcess::startDetached("/usr/bin/distrobox-host-exec", {"pw-reserve", "-n", "Audio1", "-r", "-p", "50"});
    }
}

void MediaViewer::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. GPU OpenGL Video Widget (for video files)
    m_videoWidget = new MpvVideoWidget(this);
    m_videoWidget->setStyleSheet("background-color: #000000;");

    // 2. Audio Cover & Visualizer Widget (for audio-only files)
    m_audioCoverWidget = new QWidget(this);
    QVBoxLayout *audioRootLayout = new QVBoxLayout(m_audioCoverWidget);
    audioRootLayout->setContentsMargins(16, 8, 16, 6);
    audioRootLayout->setSpacing(8);

    // Top Row: Album Cover (Left) + Rich Metadata Card (Right)
    QHBoxLayout *topCardLayout = new QHBoxLayout();
    topCardLayout->setSpacing(24);
    topCardLayout->setAlignment(Qt::AlignCenter);

    // 1. Album Cover (220x220) with subtle drop shadow
    m_audioCoverImageLabel = new QLabel(m_audioCoverWidget);
    m_audioCoverImageLabel->setFixedSize(220, 220);
    m_audioCoverImageLabel->setAlignment(Qt::AlignCenter);
    m_audioCoverImageLabel->setStyleSheet(
        "background-color: #181b20; border: 1px solid #2a313d; border-radius: 12px; font-size: 48px; color: #64748b;"
    );
    m_audioCoverImageLabel->setText("🎵");

    QGraphicsDropShadowEffect *coverShadow = new QGraphicsDropShadowEffect(m_audioCoverImageLabel);
    coverShadow->setBlurRadius(20);
    coverShadow->setColor(QColor(0, 0, 0, 160));
    coverShadow->setOffset(0, 6);
    m_audioCoverImageLabel->setGraphicsEffect(coverShadow);
    topCardLayout->addWidget(m_audioCoverImageLabel, 0, Qt::AlignCenter);

    // 2. Rich Metadata Card (380x220 - exactly matches cover height)
    m_metaCard = new QFrame(m_audioCoverWidget);
    m_metaCard->setFixedSize(380, 220);
    m_metaCard->setStyleSheet(
        "QFrame { background-color: #1a1e26; border: 1px solid #28303d; border-radius: 12px; }"
    );
    QVBoxLayout *metaLayout = new QVBoxLayout(m_metaCard);
    metaLayout->setContentsMargins(16, 10, 16, 10);
    metaLayout->setSpacing(4);

    // Track Title Row
    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(1);
    m_audioTitleLabel = new QLabel(m_metaCard);
    m_audioTitleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;");
    m_audioTitleLabel->setMaximumHeight(36);
    m_audioTitleLabel->setWordWrap(true);
    QLabel *lblTitleSub = new QLabel(tr("Track Title"), m_metaCard);
    lblTitleSub->setStyleSheet("font-size: 8px; color: #64748b; font-weight: 500; border: none; background: transparent; padding: 0px;");
    titleLayout->addWidget(m_audioTitleLabel);
    titleLayout->addWidget(lblTitleSub);
    metaLayout->addLayout(titleLayout);

    // Helper to add structured icon rows
    auto addMetaRow = [&](VectorIcons::UiIcon iconType, QLabel *&valLabel, const QString &subText, const QString &valStyle) {
        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        QLabel *iconLabel = new QLabel(m_metaCard);
        iconLabel->setFixedSize(16, 16);
        iconLabel->setScaledContents(true);
        iconLabel->setPixmap(VectorIcons::createUiIcon(iconType, QColor("#94a3b8"), 16).pixmap(16, 16));
        iconLabel->setStyleSheet("border: none; background: transparent; padding: 0px; margin: 0px;");
        rowLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        valLabel = new QLabel(m_metaCard);
        valLabel->setStyleSheet(valStyle + "; border: none; background: transparent; padding: 0px;");
        valLabel->setWordWrap(false);
        QLabel *subLabel = new QLabel(subText, m_metaCard);
        subLabel->setStyleSheet("font-size: 8px; color: #64748b; font-weight: 500; border: none; background: transparent; padding: 0px;");
        textLayout->addWidget(valLabel);
        textLayout->addWidget(subLabel);

        rowLayout->addLayout(textLayout, 1);
        metaLayout->addLayout(rowLayout);
    };

    addMetaRow(VectorIcons::UiIcon::User, m_audioArtistLabel, tr("Artist"), "font-size: 12px; font-weight: bold; color: #38bdf8;");
    addMetaRow(VectorIcons::UiIcon::Disc, m_audioAlbumLabel, tr("Album"), "font-size: 12px; font-weight: 600; color: #e2e8f0;");
    addMetaRow(VectorIcons::UiIcon::Calendar, m_audioYearLabel, tr("Release Year"), "font-size: 11px; font-weight: 600; color: #cbd5e1;");
    addMetaRow(VectorIcons::UiIcon::Signal, m_audioSpecsLabel, tr("Format Details"), "font-size: 11px; font-weight: bold; color: #e2e8f0;");
    addMetaRow(VectorIcons::UiIcon::File, m_audioFileDetailsLabel, tr("File Size"), "font-size: 11px; color: #94a3b8;");

    m_audioQualityBadge = new QLabel(m_metaCard);
    m_audioQualityBadge->setAlignment(Qt::AlignCenter);
    m_audioQualityBadge->setStyleSheet("font-size: 10px; font-weight: bold; padding: 3px 8px; border-radius: 4px;");
    m_audioQualityBadge->setVisible(false);

    QHBoxLayout *badgeLayout = new QHBoxLayout();
    badgeLayout->setContentsMargins(0, 4, 0, 0);
    badgeLayout->addWidget(m_audioQualityBadge, 0, Qt::AlignLeft);
    badgeLayout->addStretch();
    metaLayout->addLayout(badgeLayout);

    metaLayout->addStretch();
    topCardLayout->addWidget(m_metaCard, 0, Qt::AlignCenter);
    audioRootLayout->addLayout(topCardLayout);

    // Bottom: Dual Stereo Waveform & Full-Width Oscilloscope (115px)
    m_audioOscilloscope = new StereoOscilloscopeWidget(m_audioCoverWidget);
    connect(m_audioOscilloscope, &StereoOscilloscopeWidget::seekRequested, this, &MediaViewer::requestSeek);
    connect(m_audioOscilloscope, &StereoOscilloscopeWidget::levelsUpdated, this, [this](float left, float right) {
        if (m_equalizerWidget && m_equalizerDialog && m_equalizerDialog->isVisible()) {
            float gain = 1.0f;
            if (!m_equalizerWidget->isBypassed()) {
                gain = std::pow(10.0f, m_equalizerWidget->currentPreset().preAmpDb / 20.0f);
            }
            m_equalizerWidget->updateVuMeter(left * gain, right * gain);
        }
    });
    audioRootLayout->addWidget(m_audioOscilloscope, 0);

    mainLayout->addWidget(m_videoWidget, 1);
    mainLayout->addWidget(m_audioCoverWidget, 1);

    // 3. Floating Modern Bottom Controls Bar
    m_controlsWidget = new QWidget(this);
    QVBoxLayout *ctrlRootLayout = new QVBoxLayout(m_controlsWidget);
    ctrlRootLayout->setContentsMargins(16, 3, 16, 5);
    ctrlRootLayout->setSpacing(2);

    // Top row of controls: Full-width time slider with left/right timestamps
    QHBoxLayout *sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(10);

    m_timeLeftLabel = new QLabel("00:00 / 00:00", m_controlsWidget);
    m_timeLeftLabel->setStyleSheet("font-size: 11px; font-family: monospace; color: #94a3b8; border: none; background: transparent;");

    m_sliderPosition = new QSlider(Qt::Horizontal, m_controlsWidget);
    m_sliderPosition->setRange(0, 0);
    m_sliderPosition->installEventFilter(this);
    connect(m_sliderPosition, &QSlider::sliderMoved, this, [this](int val) {
        requestSeek(static_cast<qint64>(val), false);
    });
    connect(m_sliderPosition, &QSlider::sliderReleased, this, [this]() {
        requestSeek(static_cast<qint64>(m_sliderPosition->value()), true);
    });

    m_timeRightLabel = new QLabel("-00:00", m_controlsWidget);
    m_timeRightLabel->setStyleSheet("font-size: 11px; font-family: monospace; color: #94a3b8; border: none; background: transparent;");

    sliderRow->addWidget(m_timeLeftLabel);
    sliderRow->addWidget(m_sliderPosition, 1);
    sliderRow->addWidget(m_timeRightLabel);
    ctrlRootLayout->addLayout(sliderRow);

    // Bottom row of controls: 3 distinct zones
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(10);

    // Zone 1 (Left): Currently playing song title and filename
    QWidget *leftTrackWidget = new QWidget(m_controlsWidget);
    QVBoxLayout *leftTrackLayout = new QVBoxLayout(leftTrackWidget);
    leftTrackLayout->setContentsMargins(0, 0, 0, 0);
    leftTrackLayout->setSpacing(1);
    m_bottomTitleLabel = new QLabel(leftTrackWidget);
    m_bottomTitleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #f8fafc; border: none; background: transparent;");
    m_bottomFileLabel = new QLabel(leftTrackWidget);
    m_bottomFileLabel->setStyleSheet("font-size: 10px; color: #94a3b8; border: none; background: transparent;");
    leftTrackLayout->addWidget(m_bottomTitleLabel);
    leftTrackLayout->addWidget(m_bottomFileLabel);
    bottomRow->addWidget(leftTrackWidget, 0, Qt::AlignVCenter);

    bottomRow->addStretch(1);

    // Zone 2 (Center): Playback controls cluster with large circular Play/Pause button
    QHBoxLayout *centerCluster = new QHBoxLayout();
    centerCluster->setSpacing(8);
    centerCluster->setAlignment(Qt::AlignCenter);

    m_btnShuffle = new QPushButton(m_controlsWidget);
    m_btnShuffle->setFixedSize(30, 30);
    m_btnShuffle->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, QColor("#94a3b8"), 16));
    m_btnShuffle->setToolTip(tr("Shuffle"));
    m_btnShuffle->setCursor(Qt::PointingHandCursor);
    connect(m_btnShuffle, &QPushButton::clicked, this, &MediaViewer::toggleShuffle);

    m_btnPrev = new QPushButton(m_controlsWidget);
    m_btnPrev->setFixedSize(32, 32);
    m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Previous, QColor("#cbd5e1"), 18));
    m_btnPrev->setToolTip(tr("Previous track"));
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    connect(m_btnPrev, &QPushButton::clicked, this, &MediaViewer::playPreviousTrack);

    m_btnPlayPause = new QPushButton(m_controlsWidget);
    m_btnPlayPause->setFixedSize(44, 44);
    m_btnPlayPause->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Play, QColor("#ffffff"), 22));
    m_btnPlayPause->setToolTip(tr("Play / Pause"));
    m_btnPlayPause->setCursor(Qt::PointingHandCursor);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MediaViewer::togglePlayPause);

    m_btnNext = new QPushButton(m_controlsWidget);
    m_btnNext->setFixedSize(32, 32);
    m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Next, QColor("#cbd5e1"), 18));
    m_btnNext->setToolTip(tr("Next track"));
    m_btnNext->setCursor(Qt::PointingHandCursor);
    connect(m_btnNext, &QPushButton::clicked, this, &MediaViewer::playNextTrack);

    m_btnLoop = new QPushButton(m_controlsWidget);
    m_btnLoop->setFixedSize(30, 30);
    m_btnLoop->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, QColor("#94a3b8"), 16));
    m_btnLoop->setToolTip(tr("Loop playback"));
    m_btnLoop->setCursor(Qt::PointingHandCursor);
    connect(m_btnLoop, &QPushButton::clicked, this, &MediaViewer::toggleLoop);

    m_btnAutoAdvance = new QPushButton(m_controlsWidget);
    m_btnAutoAdvance->setFixedSize(30, 30);
    m_btnAutoAdvance->setCursor(Qt::PointingHandCursor);
    connect(m_btnAutoAdvance, &QPushButton::clicked, this, &MediaViewer::toggleAutoAdvance);
    updateAutoAdvanceUi();

    centerCluster->addWidget(m_btnShuffle);
    centerCluster->addWidget(m_btnPrev);
    centerCluster->addWidget(m_btnPlayPause);
    centerCluster->addWidget(m_btnNext);
    centerCluster->addWidget(m_btnLoop);
    centerCluster->addWidget(m_btnAutoAdvance);
    bottomRow->addLayout(centerCluster);

    // Shortened gap between playback controls and volume / audiophile cluster
    bottomRow->addSpacing(16);

    // Zone 3 (Right): Volume, rate & Swiss-knife tools
    QHBoxLayout *rightCluster = new QHBoxLayout();
    rightCluster->setSpacing(5);
    rightCluster->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_volumePercentLabel = new QLabel("100%", m_controlsWidget);
    m_volumePercentLabel->setStyleSheet("font-size: 11px; color: #94a3b8; font-family: monospace; border: none; background: transparent;");

    m_btnVolume = new QPushButton(m_controlsWidget);
    m_btnVolume->setFixedSize(30, 30);
    m_btnVolume->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
    m_btnVolume->setToolTip(tr("Volume / Mute"));
    m_btnVolume->setCursor(Qt::PointingHandCursor);
    connect(m_btnVolume, &QPushButton::clicked, this, [this]() {
        if (!m_sliderVolume) return;
        m_sliderVolume->setValue(m_sliderVolume->value() > 0 ? 0 : 100);
    });

    m_sliderVolume = new QSlider(Qt::Horizontal, m_controlsWidget);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(100);
    m_sliderVolume->setFixedWidth(75);
    m_sliderVolume->setToolTip(tr("Volume"));
    connect(m_sliderVolume, &QSlider::valueChanged, this, &MediaViewer::applyVolume);

    m_btnSpeed = new QPushButton("1.0x", m_controlsWidget);
    m_btnSpeed->setToolTip(tr("Playback speed"));
    m_btnSpeed->setFixedWidth(44);
    connect(m_btnSpeed, &QPushButton::clicked, this, &MediaViewer::showSpeedMenu);

    m_btnBitPerfect = new QPushButton(m_controlsWidget);
    m_btnBitPerfect->setCursor(Qt::PointingHandCursor);
    connect(m_btnBitPerfect, &QPushButton::clicked, this, &MediaViewer::toggleBitPerfect);
    updateBitPerfectUi();

    m_btnAudioDevice = new QPushButton(m_controlsWidget);
    m_btnAudioDevice->setCursor(Qt::PointingHandCursor);
    connect(m_btnAudioDevice, &QPushButton::clicked, this, &MediaViewer::showAudioDeviceMenu);
    updateAudioDeviceUi();

    m_btnTags = new QPushButton(m_controlsWidget);
    m_btnTags->setFixedSize(30, 30);
    m_btnTags->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Tag, QColor("#cbd5e1"), 18));
    m_btnTags->setToolTip(tr("Edit audio tags & cover art"));
    connect(m_btnTags, &QPushButton::clicked, this, &MediaViewer::openAudioTagDialog);

    m_btnTrim = new QPushButton(m_controlsWidget);
    m_btnTrim->setFixedSize(30, 30);
    m_btnTrim->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Scissors, QColor("#cbd5e1"), 18));
    m_btnTrim->setToolTip(tr("Trim audio clip"));
    connect(m_btnTrim, &QPushButton::clicked, this, &MediaViewer::openAudioTrimDialog);

    m_btnEqualizer = new QPushButton(m_controlsWidget);
    m_btnEqualizer->setFixedSize(30, 30);
    m_btnEqualizer->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Equalizer, QColor("#cbd5e1"), 18));
    m_btnEqualizer->setToolTip(tr("Equalizzatore & DSP Audio"));
    m_btnEqualizer->setCursor(Qt::PointingHandCursor);
    connect(m_btnEqualizer, &QPushButton::clicked, this, &MediaViewer::toggleEqualizerDialog);

    m_btnExtractAudio = new QPushButton("🎵", m_controlsWidget);
    m_btnExtractAudio->setFixedWidth(30);
    m_btnExtractAudio->setVisible(false);
    connect(m_btnExtractAudio, &QPushButton::clicked, this, &MediaViewer::extractAudioTrack);

    m_btnAudioTracks = new QPushButton("🎧", m_controlsWidget);
    m_btnAudioTracks->setFixedWidth(30);
    m_btnAudioTracks->setVisible(false);
    connect(m_btnAudioTracks, &QPushButton::clicked, this, &MediaViewer::showAudioTracksMenu);

    m_btnSubtitles = new QPushButton("💬", m_controlsWidget);
    m_btnSubtitles->setFixedWidth(30);
    m_btnSubtitles->setVisible(false);
    connect(m_btnSubtitles, &QPushButton::clicked, this, &MediaViewer::showSubtitlesMenu);

    rightCluster->addWidget(m_volumePercentLabel);
    rightCluster->addWidget(m_btnVolume);
    rightCluster->addWidget(m_sliderVolume);
    rightCluster->addWidget(m_btnSpeed);
    rightCluster->addWidget(m_btnBitPerfect);
    rightCluster->addWidget(m_btnAudioDevice);
    rightCluster->addWidget(m_btnEqualizer);
    rightCluster->addWidget(m_btnTags);
    rightCluster->addWidget(m_btnTrim);
    rightCluster->addWidget(m_btnExtractAudio);
    rightCluster->addWidget(m_btnAudioTracks);
    rightCluster->addWidget(m_btnSubtitles);
    bottomRow->addLayout(rightCluster);

    ctrlRootLayout->addLayout(bottomRow);
    mainLayout->addWidget(m_controlsWidget);

    connect(this, &MediaViewer::trackChangeRequested, this, [this](const QString &path) {
        if (m_trackChangeCallback) m_trackChangeCallback(path);
    });

    updateTheme();
}

bool MediaViewer::loadFile(const QString &filePath)
{
    QString targetPath = filePath;
    clear();

    QFileInfo fi(targetPath);
    if (!fi.exists()) return false;

    m_currentFilePath = targetPath;
    m_lastSliderPos = -1;
    m_lastReportedSec = -1;

    QString ext = fi.suffix().toLower();
    QStringList audioExts = {"mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus", "aiff", "ape", "wv", "dsf", "dff", "mpc"};
    m_isAudioOnly = audioExts.contains(ext);

    if (m_isAudioOnly) {
        m_videoWidget->setVisible(false);
        m_audioCoverWidget->setVisible(true);
        m_audioTitleLabel->setText(fi.fileName());
        m_audioArtistLabel->setText(QString());
        m_audioCoverImageLabel->setText("🎵");
        m_audioCoverImageLabel->setPixmap(QPixmap());
        m_btnTags->setVisible(true);
        if (m_btnBitPerfect) m_btnBitPerfect->setVisible(true);
        if (m_btnAudioDevice) m_btnAudioDevice->setVisible(true);
        m_btnExtractAudio->setVisible(false);
        updateAudioBadgeAndCover();
        if (m_audioOscilloscope) {
            m_audioOscilloscope->loadAudioWaveform(targetPath);
        }

        // Scan folder for all audio tracks to power Next, Previous and Shuffle
        if (m_audioSiblingFiles.isEmpty()) {
            scanAudioSiblings();
        }
        QString cleanCurrent = QFileInfo(targetPath).canonicalFilePath();
        if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(targetPath).absoluteFilePath();
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
        if (curIdx >= 0 && m_audioSiblingFiles.size() > 1) {
            if (m_bottomFileLabel) {
                m_bottomFileLabel->setText(QString("[%1/%2] %3").arg(curIdx + 1).arg(m_audioSiblingFiles.size()).arg(fi.fileName()));
            }
            int nextIdx = (curIdx + 1) % m_audioSiblingFiles.size();
            int prevIdx = (curIdx - 1 + m_audioSiblingFiles.size()) % m_audioSiblingFiles.size();
            if (m_btnNext) m_btnNext->setToolTip(tr("Next: %1").arg(QFileInfo(m_audioSiblingFiles[nextIdx]).fileName()));
            if (m_btnPrev) m_btnPrev->setToolTip(tr("Previous: %1").arg(QFileInfo(m_audioSiblingFiles[prevIdx]).fileName()));
        } else {
            if (m_bottomFileLabel) m_bottomFileLabel->setText(fi.fileName());
            if (m_btnNext) m_btnNext->setToolTip(tr("Next track"));
            if (m_btnPrev) m_btnPrev->setToolTip(tr("Previous track"));
        }
    } else {
        m_videoWidget->setVisible(true);
        m_audioCoverWidget->setVisible(false);
        m_btnTags->setVisible(false);
        if (m_btnBitPerfect) m_btnBitPerfect->setVisible(false);
        if (m_btnAudioDevice) m_btnAudioDevice->setVisible(false);
        m_btnExtractAudio->setVisible(true);
        if (m_audioOscilloscope) {
            m_audioOscilloscope->loadAudioWaveform(targetPath);
        }
    }

    m_btnAudioTracks->setVisible(false);
    m_btnSubtitles->setVisible(false);

    if (!m_mpv) return false;

    // Apply speed, loop, volume, audio device and bit-perfect configuration before starting playback
    setPlaybackSpeed(m_playbackRate);
    applyVolume(m_sliderVolume ? m_sliderVolume->value() : 100);
    mpv_set_property_string(m_mpv, "loop-file", m_isLooping ? "inf" : "no");
    applyAudioDeviceSettings(false);
    applyBitPerfectSettings();
    if (!m_isBitPerfect && m_equalizerWidget && !m_equalizerWidget->isBypassed()) {
        applyEqualizerSettings(m_equalizerWidget->currentPreset(), false);
    }

    // Load media file via libmpv
    qDebug() << "[BubezView-Audio] loadFile executing for:" << targetPath << "BitPerfect:" << m_isBitPerfect << "Device:" << m_currentAudioDevice;
    QByteArray pathBytes = targetPath.toUtf8();
    const char *cmd[] = {"loadfile", pathBytes.constData(), "replace", nullptr};
    mpv_command(m_mpv, cmd);

    int pause = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause);
    updatePlaybackStateUi(true);

    return true;
}

void MediaViewer::updateAudioBadgeAndCover()
{
    if (!m_isAudioOnly || m_currentFilePath.isEmpty()) return;

    AudioMetadata meta = AudioProcessor::readMetadata(m_currentFilePath);
    QFileInfo fi(m_currentFilePath);

    // Title
    QString title = meta.title.isEmpty() ? fi.completeBaseName() : meta.title;
    m_audioTitleLabel->setText(title);
    if (m_bottomTitleLabel) m_bottomTitleLabel->setText(title);
    if (m_bottomFileLabel) m_bottomFileLabel->setText(fi.fileName());

    // Artist
    if (m_audioArtistLabel) {
        m_audioArtistLabel->setText(meta.artist.isEmpty() ? tr("Unknown Artist") : meta.artist);
    }

    // Album
    if (m_audioAlbumLabel) {
        m_audioAlbumLabel->setText(meta.album.isEmpty() ? tr("Unknown Album") : meta.album);
    }

    // Year
    if (m_audioYearLabel) {
        m_audioYearLabel->setText(meta.year.isEmpty() ? (meta.date.isEmpty() ? "—" : meta.date) : meta.year);
    }

    // Format Details
    QStringList specs;
    specs << (meta.codecName.isEmpty() ? fi.suffix().toUpper() : meta.codecName.toUpper());
    if (meta.bitRate > 0) specs << QString("%1 kbps").arg(meta.bitRate);
    if (meta.sampleRate > 0) specs << QString("%1 kHz").arg(QString::number(meta.sampleRate / 1000.0, 'f', 1));
    if (meta.channels > 0) specs << (meta.channels == 2 ? "Stereo" : (meta.channels == 1 ? "Mono" : QString("%1ch").arg(meta.channels)));
    if (meta.bitsPerSample > 0) specs << QString("%1-bit").arg(meta.bitsPerSample);
    if (m_audioSpecsLabel) {
        m_audioSpecsLabel->setText(specs.join(" | "));
    }

    // File Details (Size)
    if (m_audioFileDetailsLabel) {
        double mb = static_cast<double>(fi.size()) / (1024.0 * 1024.0);
        m_audioFileDetailsLabel->setText(QString("%1 MB").arg(QString::number(mb, 'f', 1)));
    }

    // High-Fidelity & Bit-Perfect Quality Badge
    if (m_audioQualityBadge) {
        QString ext = fi.suffix().toLower();
        bool isLossless = (ext == "flac" || ext == "wav" || ext == "aiff" || ext == "alac" || ext == "ape" || ext == "wv" || ext == "dsf" || ext == "dff");
        bool isHiRes = (meta.bitsPerSample >= 24 || meta.sampleRate >= 48000);

        if (m_isBitPerfect) {
            m_audioQualityBadge->setVisible(true);
            bool isDirectAlsa = m_currentAudioDevice.startsWith("alsa/");

            if (isDirectAlsa) {
                if (isHiRes && isLossless) {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV2 • DIRECT ALSA HI-RES"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #d97706, stop:1 #f59e0b);"
                        "   color: #ffffff;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                } else if (isLossless) {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV2 • DIRECT ALSA CD"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #b45309, stop:1 #d97706);"
                        "   color: #ffffff;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                } else {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV2 • DIRECT ALSA"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: #334155;"
                        "   border: 1px solid #f59e0b;"
                        "   color: #fbbf24;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                }
            } else {
                if (isHiRes && isLossless) {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV1 • HI-RES LOSSLESS"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369a1, stop:1 #0284c7);"
                        "   color: #ffffff;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                } else if (isLossless) {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV1 • LOSSLESS CD"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: #0284c7;"
                        "   color: #ffffff;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                } else {
                    m_audioQualityBadge->setText(tr("🎯 BIT-PERFECT LV1 • DIRECT STREAM"));
                    m_audioQualityBadge->setStyleSheet(
                        "QLabel {"
                        "   background: #1e293b;"
                        "   border: 1px solid #38bdf8;"
                        "   color: #38bdf8;"
                        "   font-size: 10px;"
                        "   font-weight: bold;"
                        "   border-radius: 4px;"
                        "   padding: 3px 8px;"
                        "}"
                    );
                }
            }
        } else {
            if (isHiRes && isLossless) {
                m_audioQualityBadge->setVisible(true);
                m_audioQualityBadge->setText(tr("HI-RES LOSSLESS"));
                m_audioQualityBadge->setStyleSheet(
                    "QLabel {"
                    "   background: rgba(56, 189, 248, 0.2);"
                    "   border: 1px solid #0284c7;"
                    "   color: #38bdf8;"
                    "   font-size: 10px;"
                    "   font-weight: bold;"
                    "   border-radius: 4px;"
                    "   padding: 3px 8px;"
                    "}"
                );
            } else if (isLossless) {
                m_audioQualityBadge->setVisible(true);
                m_audioQualityBadge->setText(tr("LOSSLESS AUDIO"));
                m_audioQualityBadge->setStyleSheet(
                    "QLabel {"
                    "   background: rgba(148, 163, 184, 0.2);"
                    "   border: 1px solid #64748b;"
                    "   color: #cbd5e1;"
                    "   font-size: 10px;"
                    "   font-weight: bold;"
                    "   border-radius: 4px;"
                    "   padding: 3px 8px;"
                    "}"
                );
            } else {
                m_audioQualityBadge->setVisible(false);
            }
        }
    }

    // Album Cover art (220x220) with rounded corners
    QPixmap cover = AudioProcessor::extractCoverArt(m_currentFilePath, 220);
    if (!cover.isNull()) {
        QPixmap rounded(220, 220);
        rounded.fill(Qt::transparent);
        QPainter p(&rounded);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(0, 0, 220, 220, 12, 12);
        p.setClipPath(clip);
        p.drawPixmap(0, 0, 220, 220, cover.scaled(220, 220, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        p.end();

        m_audioCoverImageLabel->setPixmap(rounded);
        m_audioCoverImageLabel->setText(QString());
    } else {
        m_audioCoverImageLabel->setPixmap(QPixmap());
        m_audioCoverImageLabel->setText("🎵");
    }
}

void MediaViewer::setSiblingFiles(const QStringList &files)
{
    m_siblingFiles = files;
    m_audioSiblingFiles.clear();
    static const QStringList audioExts = {
        "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus", "aiff", "ape", "wv", "dsf", "dff", "mpc"
    };
    for (const QString &f : files) {
        QFileInfo fi(f);
        if (audioExts.contains(fi.suffix().toLower())) {
            QString clean = fi.canonicalFilePath();
            if (clean.isEmpty()) clean = fi.absoluteFilePath();
            if (!m_audioSiblingFiles.contains(clean)) {
                m_audioSiblingFiles.append(clean);
            }
        }
    }
}

void MediaViewer::scanAudioSiblings()
{
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QDir dir = fi.dir();
    static const QStringList audioExts = {
        "mp3", "flac", "wav", "ogg", "m4a", "aac", "wma", "opus", "aiff", "ape", "wv", "dsf", "dff", "mpc"
    };

    // List all files in the folder with natural numeric collation
    QStringList files = dir.entryList(QDir::Files, QDir::NoSort);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), collator);

    QStringList detected;
    for (const QString &f : files) {
        if (f.startsWith('.')) continue;
        QFileInfo entryFi(dir.absoluteFilePath(f));
        if (audioExts.contains(entryFi.suffix().toLower())) {
            QString clean = entryFi.canonicalFilePath();
            if (clean.isEmpty()) clean = entryFi.absoluteFilePath();
            detected.append(clean);
        }
    }

    if (!detected.isEmpty()) {
        m_audioSiblingFiles = detected;
    }
}

void MediaViewer::reloadCurrentTrack(int delayMs)
{
    if (m_currentFilePath.isEmpty()) return;
    QString current = m_currentFilePath;

    if (delayMs < 0) {
        delayMs = 250;
    }

    qDebug() << "[BubezView-Audio] reloadCurrentTrack requested, delay:" << delayMs << "ms, file:" << current << "bitPerfect:" << m_isBitPerfect << "device:" << m_currentAudioDevice;

    // 1. Immediately stop playback and release current audio hardware / server lock
    stop();

    if (m_isBitPerfect && m_bottomFileLabel) {
        m_bottomFileLabel->setText(tr("🎯 Bit-Perfect Direct ALSA"));
    }

    if (!m_reloadTimer) {
        m_reloadTimer = new QTimer(this);
        m_reloadTimer->setSingleShot(true);
    } else {
        m_reloadTimer->stop();
        m_reloadTimer->disconnect();
    }

    connect(m_reloadTimer, &QTimer::timeout, this, [this, current]() {
        qDebug() << "[BubezView-Audio] reloadCurrentTrack timer FIRED, calling loadFile for:" << current;
        loadFile(current);
    });
    m_reloadTimer->start(delayMs);
}

void MediaViewer::playNextTrack()
{
    if (!m_isAudioOnly) return;
    if (m_audioSiblingFiles.isEmpty()) {
        scanAudioSiblings();
    }
    if (m_audioSiblingFiles.isEmpty()) return;

    QString cleanCurrent = QFileInfo(m_currentFilePath).canonicalFilePath();
    if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(m_currentFilePath).absoluteFilePath();
    int curIdx = m_audioSiblingFiles.indexOf(cleanCurrent);
    if (curIdx < 0) {
        QString currentName = QFileInfo(m_currentFilePath).fileName();
        for (int i = 0; i < m_audioSiblingFiles.size(); ++i) {
            if (QFileInfo(m_audioSiblingFiles.at(i)).fileName() == currentName) {
                curIdx = i;
                break;
            }
        }
    }

    int nextIdx = 0;
    if (m_isShuffle) {
        if (m_audioSiblingFiles.size() > 1) {
            do {
                nextIdx = QRandomGenerator::global()->bounded(m_audioSiblingFiles.size());
            } while (nextIdx == curIdx);
        } else {
            nextIdx = 0;
        }
    } else {
        if (curIdx >= 0 && curIdx + 1 < m_audioSiblingFiles.size()) {
            nextIdx = curIdx + 1;
        } else {
            nextIdx = 0; // Wrap around to first track
        }
    }

    QString nextPath = m_audioSiblingFiles.at(nextIdx);
    emit trackChangeRequested(nextPath);
    if (m_currentFilePath != nextPath) {
        loadFile(nextPath);
    }
}

void MediaViewer::playPreviousTrack()
{
    if (!m_isAudioOnly) return;
    if (m_audioSiblingFiles.isEmpty()) {
        scanAudioSiblings();
    }
    if (m_audioSiblingFiles.isEmpty()) return;

    // If already played for more than 3 seconds, replay current track from start
    if (m_positionMs > 3000) {
        requestSeek(0, true);
        return;
    }

    QString cleanCurrent = QFileInfo(m_currentFilePath).canonicalFilePath();
    if (cleanCurrent.isEmpty()) cleanCurrent = QFileInfo(m_currentFilePath).absoluteFilePath();
    int curIdx = m_audioSiblingFiles.indexOf(cleanCurrent);
    if (curIdx < 0) {
        QString currentName = QFileInfo(m_currentFilePath).fileName();
        for (int i = 0; i < m_audioSiblingFiles.size(); ++i) {
            if (QFileInfo(m_audioSiblingFiles.at(i)).fileName() == currentName) {
                curIdx = i;
                break;
            }
        }
    }

    int prevIdx = 0;
    if (m_isShuffle) {
        if (m_audioSiblingFiles.size() > 1) {
            do {
                prevIdx = QRandomGenerator::global()->bounded(m_audioSiblingFiles.size());
            } while (prevIdx == curIdx);
        } else {
            prevIdx = 0;
        }
    } else {
        if (curIdx > 0) {
            prevIdx = curIdx - 1;
        } else {
            prevIdx = m_audioSiblingFiles.size() - 1; // Wrap around to last track
        }
    }

    QString prevPath = m_audioSiblingFiles.at(prevIdx);
    emit trackChangeRequested(prevPath);
    if (m_currentFilePath != prevPath) {
        loadFile(prevPath);
    }
}

void MediaViewer::toggleShuffle()
{
    m_isShuffle = !m_isShuffle;
    if (m_btnShuffle) {
        m_btnShuffle->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, m_isShuffle ? QColor("#38bdf8") : QColor("#94a3b8"), 16));
        m_btnShuffle->setToolTip(m_isShuffle ? tr("Shuffle: ON") : tr("Shuffle: OFF"));
        m_btnShuffle->setStyleSheet(m_isShuffle
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; }");
    }
}

void MediaViewer::toggleAutoAdvance()
{
    m_isAutoAdvance = !m_isAutoAdvance;
    QSettings settings("Bubez", "BubezView");
    settings.setValue("audio/auto_advance", m_isAutoAdvance);
    updateAutoAdvanceUi();
}

void MediaViewer::updateAutoAdvanceUi()
{
    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);
    QColor subIconColor = isDark ? QColor("#94a3b8") : QColor("#64748b");
    if (m_btnAutoAdvance) {
        m_btnAutoAdvance->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::AutoAdvance, m_isAutoAdvance ? QColor("#38bdf8") : subIconColor, 18));
        m_btnAutoAdvance->setToolTip(m_isAutoAdvance
            ? tr("Continuous Playback (Auto-Advance): ON\nAutomatically plays the next track when the current one ends.")
            : tr("Continuous Playback (Auto-Advance): OFF\nPlayback stops at the end of the current track. Click to enable."));
        m_btnAutoAdvance->setStyleSheet(m_isAutoAdvance
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; }");
    }
}

void MediaViewer::clear()
{
    stop();
    m_currentFilePath.clear();
    m_durationMs = 0;
    m_positionMs = 0;
    m_lastSliderPos = -1;
    m_lastReportedSec = -1;

    m_sliderPosition->setValue(0);
    if (m_timeLeftLabel) m_timeLeftLabel->setText("00:00 / 00:00");
    if (m_timeRightLabel) m_timeRightLabel->setText("-00:00");
    if (m_bottomTitleLabel) m_bottomTitleLabel->setText(QString());
    if (m_bottomFileLabel) m_bottomFileLabel->setText(QString());
    m_audioTitleLabel->setText(QString());
    m_audioArtistLabel->setText(QString());
    if (m_audioAlbumLabel) m_audioAlbumLabel->setText(QString());
    if (m_audioYearLabel) m_audioYearLabel->setText(QString());
    m_audioCoverImageLabel->setText("🎵");
    m_audioCoverImageLabel->setPixmap(QPixmap());
    m_audioQualityBadge->setVisible(false);
    if (m_audioSpecsLabel) m_audioSpecsLabel->setText(QString());
    if (m_audioFileDetailsLabel) m_audioFileDetailsLabel->setText(QString());
    if (m_audioOscilloscope) m_audioOscilloscope->reset();
    m_btnAudioTracks->setVisible(false);
    m_btnSubtitles->setVisible(false);
    m_btnExtractAudio->setVisible(false);
}

void MediaViewer::stop()
{
    if (m_mpv) {
        int pause = 1;
        mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause);
        const char *cmd[] = {"stop", nullptr};
        mpv_command(m_mpv, cmd);

        // Immediately disengage silence streaming and exclusive hold so ALSA hardware is released
        mpv_set_property_string(m_mpv, "audio-stream-silence", "no");
        mpv_set_property_string(m_mpv, "audio-exclusive", "no");
    }
    m_isPlaying = false;
    updatePlaybackStateUi(false);
}

void MediaViewer::togglePlayPause()
{
    if (!m_mpv) return;

    // If player is idle or stopped, reload current file to resume playback
    int idle = 0;
    if (mpv_get_property(m_mpv, "idle-active", MPV_FORMAT_FLAG, &idle) >= 0 && idle) {
        if (!m_currentFilePath.isEmpty()) {
            loadFile(m_currentFilePath);
            return;
        }
    }

    mpv_command_string(m_mpv, "cycle pause");
}

void MediaViewer::toggleLoop()
{
    m_isLooping = !m_isLooping;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "loop-file", m_isLooping ? "inf" : "no");
    }
    updateLoopButtonStyle();
}

void MediaViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);

    QColor mainIconColor = isDark ? QColor("#cbd5e1") : QColor("#475569");
    QColor subIconColor = isDark ? QColor("#94a3b8") : QColor("#64748b");

    if (m_btnPrev) m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Previous, mainIconColor, 20));
    if (m_btnNext) m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Next, mainIconColor, 20));
    if (m_btnShuffle) m_btnShuffle->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, m_isShuffle ? QColor("#38bdf8") : subIconColor, 18));
    if (m_btnVolume) m_btnVolume->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, mainIconColor, 18));
    if (m_btnTags) m_btnTags->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Tag, mainIconColor, 18));
    if (m_btnTrim) m_btnTrim->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Scissors, mainIconColor, 18));

    if (m_btnPlayPause) {
        m_btnPlayPause->setStyleSheet(
            "QPushButton {"
            "   background-color: #0284c7;"
            "   border: 2px solid #38bdf8;"
            "   border-radius: 24px;"
            "   padding: 0px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #0ea5e9;"
            "   border: 2px solid #7dd3fc;"
            "}"
        );
        m_btnPlayPause->setIcon(VectorIcons::createUiIcon(m_isPlaying ? VectorIcons::UiIcon::Pause : VectorIcons::UiIcon::Play, QColor("#ffffff"), 24));
    }

    QString navBtnStyle = isDark
        ? "QPushButton { background: transparent; border: none; border-radius: 18px; padding: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.08); }"
        : "QPushButton { background: transparent; border: none; border-radius: 18px; padding: 4px; } QPushButton:hover { background: rgba(0, 0, 0, 0.06); }";
    if (m_btnPrev) m_btnPrev->setStyleSheet(navBtnStyle);
    if (m_btnNext) m_btnNext->setStyleSheet(navBtnStyle);
    if (m_btnShuffle) {
        m_btnShuffle->setStyleSheet(m_isShuffle
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : navBtnStyle);
    }

    // Unified High-Fidelity Multimedia Theme for Audio Player Canvas
    QColor bgColor = QColor("#14171d");
    QColor cardColor = QColor("#1a1e26");
    QColor borderColor = QColor("#28303d");
    QColor accentColor = QColor("#38bdf8");

    m_audioCoverWidget->setStyleSheet(QString("background-color: %1;").arg(bgColor.name()));
    m_audioCoverImageLabel->setStyleSheet(
        QString("background-color: #181b20; border: 1px solid %1; border-radius: 12px; font-size: 48px; color: #64748b;").arg(borderColor.name())
    );
    if (m_metaCard) {
        m_metaCard->setStyleSheet(
            QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 12px; }").arg(cardColor.name(), borderColor.name())
        );
    }
    m_audioTitleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #ffffff; border: none; background: transparent; padding: 0px;");
    m_audioArtistLabel->setStyleSheet(QString("font-size: 12px; font-weight: bold; color: %1; border: none; background: transparent; padding: 0px;").arg(accentColor.name()));
    if (m_audioAlbumLabel) m_audioAlbumLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #e2e8f0; border: none; background: transparent; padding: 0px;");
    if (m_audioYearLabel) m_audioYearLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #cbd5e1; border: none; background: transparent; padding: 0px;");
    if (m_audioSpecsLabel) m_audioSpecsLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #e2e8f0; border: none; background: transparent; padding: 0px;");
    if (m_audioFileDetailsLabel) m_audioFileDetailsLabel->setStyleSheet("font-size: 11px; color: #94a3b8; border: none; background: transparent; padding: 0px;");
    if (m_bottomTitleLabel) m_bottomTitleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #f8fafc; border: none; background: transparent;");
    if (m_bottomFileLabel) m_bottomFileLabel->setStyleSheet("font-size: 10px; color: #94a3b8; border: none; background: transparent;");

    if (m_audioOscilloscope) m_audioOscilloscope->update();

    m_controlsWidget->setStyleSheet(
        "QWidget {"
        "   background-color: #101318;"
        "   border-top: 1px solid #222832;"
        "}"
        "QPushButton {"
        "   background-color: transparent;"
        "   color: #cbd5e1;"
        "   font-size: 12px;"
        "   font-weight: bold;"
        "   border: none;"
        "   padding: 3px 5px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1e2430;"
        "   color: #ffffff;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: #252b36;"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: #0284c7;"
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #38bdf8;"
        "   width: 10px;"
        "   height: 10px;"
        "   margin: -3px 0;"
        "   border-radius: 5px;"
        "}"
        "QLabel {"
        "   color: #94a3b8;"
        "   font-size: 11px;"
        "}"
    );

    updateLoopButtonStyle();
    updateAutoAdvanceUi();
    updateBitPerfectUi();
    updateAudioDeviceUi();
}

void MediaViewer::updateLoopButtonStyle()
{
    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);
    QColor subIconColor = isDark ? QColor("#94a3b8") : QColor("#64748b");
    if (m_btnLoop) {
        m_btnLoop->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repeat, m_isLooping ? QColor("#38bdf8") : subIconColor, 18));
        m_btnLoop->setStyleSheet(m_isLooping
            ? "QPushButton { background: rgba(56, 189, 248, 0.2); border: 1px solid #38bdf8; border-radius: 6px; padding: 4px; }"
            : "QPushButton { background: transparent; border: none; border-radius: 6px; padding: 4px; }");
    }
}

void MediaViewer::updatePlaybackStateUi(bool playing)
{
    m_isPlaying = playing;
    if (m_mpv && m_isBitPerfect) {
        mpv_set_property_string(m_mpv, "audio-stream-silence", playing ? "yes" : "no");
    }
    if (m_btnPlayPause) {
        m_btnPlayPause->setText(QString());
        m_btnPlayPause->setIcon(VectorIcons::createUiIcon(playing ? VectorIcons::UiIcon::Pause : VectorIcons::UiIcon::Play, QColor("#ffffff"), 24));
        m_btnPlayPause->setToolTip(playing ? tr("Pause") : tr("Play"));
    }
    if (m_audioOscilloscope) {
        m_audioOscilloscope->setPlaying(playing);
    }
}

void MediaViewer::toggleBitPerfect()
{
    setBitPerfect(!m_isBitPerfect);
}

void MediaViewer::setBitPerfect(bool enabled)
{
    if (m_isBitPerfect == enabled) return;

    // 1. Immediately stop current playback before switching audio servers or hardware devices
    stop();

    m_isBitPerfect = enabled;
    QSettings settings("Bubez", "BubezView");
    settings.setValue("audio/bit_perfect", m_isBitPerfect);

    if (m_isBitPerfect) {
        // Automatically switch to last used ALSA hardware device or detect one
        if (m_currentAudioDevice == "auto" || !m_currentAudioDevice.startsWith("alsa/")) {
            if (!m_lastBitPerfectDevice.isEmpty() && m_lastBitPerfectDevice.startsWith("alsa/")) {
                m_currentAudioDevice = m_lastBitPerfectDevice;
            } else {
                m_currentAudioDevice = detectDefaultAlsaDevice();
                m_lastBitPerfectDevice = m_currentAudioDevice;
            }
            settings.setValue("audio/device", m_currentAudioDevice);
            settings.setValue("audio/last_bitperfect_device", m_lastBitPerfectDevice);
        }

        // Release ALSA hardware device from PipeWire / WirePlumber
        requestAlsaDeviceRelease();
    } else {
        // Disabling Bit-Perfect: save current device and switch back to system sound server (PipeWire)
        if (m_currentAudioDevice.startsWith("alsa/")) {
            m_lastBitPerfectDevice = m_currentAudioDevice;
            settings.setValue("audio/last_bitperfect_device", m_lastBitPerfectDevice);
        }
        m_currentAudioDevice = "auto";
        settings.setValue("audio/device", "auto");
        resetAudioMixer(); // Immediately unblock KDE mixer
    }

    updateBitPerfectUi();
    updateAudioDeviceUi();
    if (m_isAudioOnly) {
        updateAudioBadgeAndCover();
        reloadCurrentTrack(250);
    }
}

void MediaViewer::applyBitPerfectSettings()
{
    if (!m_mpv) return;

    if (m_isBitPerfect) {
        // Audiophile Bit-Perfect:
        // 1. Bypass all software resampling (pass native sample rate to DAC/sound server)
        mpv_set_property_string(m_mpv, "audio-resample", "no");
        // 2. Disable pitch correction / time stretching DSP algorithms
        mpv_set_property_string(m_mpv, "audio-pitch-correction", "no");
        // 3. Keep audio stream alive during playback to prevent DAC pop / click between tracks
        mpv_set_property_string(m_mpv, "audio-stream-silence", m_isPlaying ? "yes" : "no");
        // 4. Gapless audio for seamless audiophile album playback
        mpv_set_property_string(m_mpv, "gapless-audio", "yes");

        // 5. Exclusive hardware mode if on ALSA
        if (m_currentAudioDevice.startsWith("alsa/")) {
            mpv_set_property_string(m_mpv, "audio-exclusive", "yes");
        }

        // 6. Unity gain (100% volume, 0 dB digital attenuation for unmodified bitstream)
        if (m_sliderVolume && m_sliderVolume->value() != 100) {
            m_sliderVolume->setValue(100);
        }

        // 7. Clear mpv audio filters (bypassing equalizer/DSP for pure bitstream)
        mpv_set_property_string(m_mpv, "af", "");
    } else {
        // Standard playback mode: dynamic resampling and DSP pitch correction enabled
        mpv_set_property_string(m_mpv, "audio-resample", "yes");
        mpv_set_property_string(m_mpv, "audio-pitch-correction", "yes");
        mpv_set_property_string(m_mpv, "audio-stream-silence", "no");
        mpv_set_property_string(m_mpv, "gapless-audio", "weak");
        mpv_set_property_string(m_mpv, "audio-exclusive", "no");

        if (m_equalizerWidget && !m_equalizerWidget->isBypassed()) {
            applyEqualizerSettings(m_equalizerWidget->currentPreset(), false);
        }
    }
}

void MediaViewer::updateBitPerfectUi()
{
    if (!m_btnBitPerfect) return;

    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);

    if (m_btnEqualizer) {
        if (m_isBitPerfect) {
            m_btnEqualizer->setEnabled(false);
            m_btnEqualizer->setToolTip(tr("Equalizer is disabled in Bit-Perfect audiophile mode"));
            if (m_equalizerDialog && m_equalizerDialog->isVisible()) {
                m_equalizerDialog->hide();
            }
        } else {
            m_btnEqualizer->setEnabled(true);
            m_btnEqualizer->setToolTip(tr("Equalizzatore & DSP Audio"));
        }
    }

    if (m_isBitPerfect) {
        m_btnBitPerfect->setText(tr("🎯 Bit-Perfect"));
        m_btnBitPerfect->setToolTip(tr("Bit-Perfect Audiophile Mode: ACTIVE\n"
                                       "• Resampling: Bypassed (Native sample rate)\n"
                                       "• DSP / Pitch Correction: Disabled\n"
                                       "• Digital Gain: 0 dB Unity (Unmodified bitstream)\n"
                                       "Click to disable."));
        m_btnBitPerfect->setStyleSheet(
            "QPushButton {"
            "   background: rgba(245, 158, 11, 0.22);"
            "   border: 1px solid #f59e0b;"
            "   color: #fbbf24;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "   padding: 3px 8px;"
            "   border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "   background: rgba(245, 158, 11, 0.38);"
            "   border: 1px solid #fde047;"
            "   color: #ffffff;"
            "}"
        );
    } else {
        m_btnBitPerfect->setText(tr("🎯 BP"));
        m_btnBitPerfect->setToolTip(tr("Bit-Perfect Audiophile Mode: OFF\n"
                                       "Click to bypass all software resampling and DSP for bit-exact DAC output."));
        if (isDark) {
            m_btnBitPerfect->setStyleSheet(
                "QPushButton {"
                "   background: transparent;"
                "   border: 1px solid rgba(148, 163, 184, 0.35);"
                "   color: #94a3b8;"
                "   font-size: 11px;"
                "   font-weight: 600;"
                "   padding: 3px 6px;"
                "   border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "   background: rgba(255, 255, 255, 0.08);"
                "   border-color: #cbd5e1;"
                "   color: #f1f5f9;"
                "}"
            );
        } else {
            m_btnBitPerfect->setStyleSheet(
                "QPushButton {"
                "   background: transparent;"
                "   border: 1px solid rgba(100, 116, 139, 0.4);"
                "   color: #475569;"
                "   font-size: 11px;"
                "   font-weight: 600;"
                "   padding: 3px 6px;"
                "   border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "   background: rgba(0, 0, 0, 0.06);"
                "   border-color: #334155;"
                "   color: #0f172a;"
                "}"
            );
        }
    }
}

void MediaViewer::applyAudioDeviceSettings(bool forceReload)
{
    if (!m_mpv) return;

    if (m_currentAudioDevice.startsWith("alsa/")) {
        // Direct ALSA Hardware (Bit-Perfect Livello 2)
        mpv_set_property_string(m_mpv, "ao", "alsa");
        mpv_set_property_string(m_mpv, "audio-exclusive", "yes");
        QByteArray devBytes = m_currentAudioDevice.toUtf8();
        mpv_set_property_string(m_mpv, "audio-device", devBytes.constData());
    } else {
        // Standard System Sound Server (PipeWire / PulseAudio)
        mpv_set_property_string(m_mpv, "audio-exclusive", "no");
        mpv_set_property_string(m_mpv, "ao", "pulse,pipewire,alsa,jack,null");
        mpv_set_property_string(m_mpv, "audio-device", "auto");
    }

    if (forceReload) {
        // Only force reload when user explicitly switches device in menu, not on every track load
        const char *cmd[] = {"ao-reload", nullptr};
        mpv_command(m_mpv, cmd);
    }
}

void MediaViewer::updateAudioDeviceUi()
{
    if (!m_btnAudioDevice) return;

    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);

    if (m_currentAudioDevice.startsWith("alsa/")) {
        QString label = tr("🔊 ALSA Direct ▾");
        if (m_currentAudioDevice.contains("iec958", Qt::CaseInsensitive)) {
            label = tr("🔊 S/PDIF ▾");
        } else if (m_currentAudioDevice.contains("front", Qt::CaseInsensitive)) {
            label = tr("🔊 ALSA Analog ▾");
        } else if (m_currentAudioDevice.contains("hdmi", Qt::CaseInsensitive)) {
            label = tr("🔊 ALSA HDMI ▾");
        }

        m_btnAudioDevice->setText(label);
        m_btnAudioDevice->setToolTip(tr("Audio Output: Direct ALSA Hardware (Bit-Perfect Livello 2)\n"
                                        "Device: %1\n"
                                        "Bypasses PipeWire/PulseAudio completely.\n"
                                        "Click to change output device.").arg(m_currentAudioDevice));
        m_btnAudioDevice->setStyleSheet(
            "QPushButton {"
            "   background: rgba(245, 158, 11, 0.22);"
            "   border: 1px solid #f59e0b;"
            "   color: #fbbf24;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "   padding: 3px 8px;"
            "   border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "   background: rgba(245, 158, 11, 0.38);"
            "   border: 1px solid #fde047;"
            "   color: #ffffff;"
            "}"
        );
    } else {
        m_btnAudioDevice->setText(tr("🔊 Auto ▾"));
        m_btnAudioDevice->setToolTip(tr("Audio Output: System Sound Server (PipeWire / PulseAudio)\n"
                                        "Click to select Direct ALSA Hardware (Bit-Perfect Livello 2)."));
        if (isDark) {
            m_btnAudioDevice->setStyleSheet(
                "QPushButton {"
                "   background: transparent;"
                "   border: 1px solid rgba(148, 163, 184, 0.35);"
                "   color: #94a3b8;"
                "   font-size: 11px;"
                "   font-weight: 600;"
                "   padding: 3px 6px;"
                "   border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "   background: rgba(255, 255, 255, 0.08);"
                "   border-color: #cbd5e1;"
                "   color: #f1f5f9;"
                "}"
            );
        } else {
            m_btnAudioDevice->setStyleSheet(
                "QPushButton {"
                "   background: transparent;"
                "   border: 1px solid rgba(100, 116, 139, 0.4);"
                "   color: #475569;"
                "   font-size: 11px;"
                "   font-weight: 600;"
                "   padding: 3px 6px;"
                "   border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "   background: rgba(0, 0, 0, 0.06);"
                "   border-color: #334155;"
                "   color: #0f172a;"
                "}"
            );
        }
    }
}

void MediaViewer::selectAudioDevice(const QString &deviceName)
{
    QString target = deviceName.trimmed();
    if (target.isEmpty()) target = "auto";

    // 1. Immediately stop current playback before switching devices
    stop();

    QString previousDevice = m_currentAudioDevice;
    m_currentAudioDevice = target;

    QSettings settings("Bubez", "BubezView");
    settings.setValue("audio/device", m_currentAudioDevice);

    if (m_currentAudioDevice.startsWith("alsa/")) {
        m_lastBitPerfectDevice = m_currentAudioDevice;
        settings.setValue("audio/last_bitperfect_device", m_lastBitPerfectDevice);
        // Selecting a direct ALSA hardware channel automatically activates Bit-Perfect mode
        m_isBitPerfect = true;
        settings.setValue("audio/bit_perfect", true);
        requestAlsaDeviceRelease();
    } else {
        // Selecting Auto / PipeWire automatically turns Bit-Perfect mode OFF
        m_isBitPerfect = false;
        settings.setValue("audio/bit_perfect", false);
        if (previousDevice.startsWith("alsa/")) {
            resetAudioMixer();
        }
    }

    updateAudioDeviceUi();
    updateBitPerfectUi();

    if (m_isAudioOnly) {
        updateAudioBadgeAndCover();
        reloadCurrentTrack(250);
    }
}

void MediaViewer::showAudioDeviceMenu()
{
    if (!m_mpv) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: #1a1e26;"
        "   border: 1px solid #334155;"
        "   border-radius: 8px;"
        "   padding: 6px;"
        "   color: #f1f5f9;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px 6px 12px;"
        "   border-radius: 4px;"
        "   font-size: 12px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "}"
        "QMenu::separator {"
        "   height: 1px;"
        "   background: #334155;"
        "   margin: 4px 6px;"
        "}"
    );

    // Section 1: System Sound Server
    QAction *secSys = menu.addAction(tr("--- System Audio Server ---"));
    secSys->setEnabled(false);

    QAction *actAuto = menu.addAction(tr("🌐 Auto / Default (PipeWire / PulseAudio)"));
    actAuto->setCheckable(true);
    actAuto->setChecked(m_currentAudioDevice == "auto" || m_currentAudioDevice.isEmpty());
    connect(actAuto, &QAction::triggered, this, [this]() {
        selectAudioDevice("auto");
    });

    menu.addSeparator();

    // Section 2: Direct ALSA Hardware Endpoints
    QAction *secHw = menu.addAction(tr("--- Direct ALSA Hardware (Bit-Perfect Level 2) ---"));
    secHw->setEnabled(false);

    struct DeviceEntry {
        QString name;
        QString desc;
    };
    QList<DeviceEntry> alsaDevices;

    mpv_node result;
    if (mpv_get_property(m_mpv, "audio-device-list", MPV_FORMAT_NODE, &result) >= 0) {
        if (result.format == MPV_FORMAT_NODE_ARRAY && result.u.list) {
            for (int i = 0; i < result.u.list->num; ++i) {
                mpv_node *item = &result.u.list->values[i];
                if (item->format == MPV_FORMAT_NODE_MAP && item->u.list) {
                    QString name;
                    QString desc;
                    for (int k = 0; k < item->u.list->num; ++k) {
                        const char *key = item->u.list->keys[k];
                        if (strcmp(key, "name") == 0 && item->u.list->values[k].format == MPV_FORMAT_STRING) {
                            name = QString::fromUtf8(item->u.list->values[k].u.string);
                        } else if (strcmp(key, "description") == 0 && item->u.list->values[k].format == MPV_FORMAT_STRING) {
                            desc = QString::fromUtf8(item->u.list->values[k].u.string);
                        }
                    }

                    // Exclude software layers and plugins: dmix, plughw, sysdefault, surround, lavrate, speex, etc.
                    if (name.contains("dmix:") || name.contains("plughw:") || name.contains("sysdefault:") ||
                        name.contains("default:") || name.contains("surround") || name.contains("lavrate") ||
                        name.contains("speex") || name.contains("upmix") || name.contains("vdownmix") ||
                        name.contains("usbstream") || name.contains("samplerate")) {
                        continue;
                    }

                    if (name.startsWith("alsa/front:") || name.startsWith("alsa/iec958:")) {
                        alsaDevices.append({name, desc});
                    } else if (name.startsWith("alsa/hdmi:")) {
                        // Check if HDMI endpoint is physically connected to a monitor/TV
                        bool isConnectedHdmi = false;
                        static QRegularExpression genericHdmi(R"(HDMI\s*\d+)", QRegularExpression::CaseInsensitiveOption);

                        QRegularExpression devRegex(R"(DEV=(\d+))");
                        auto match = devRegex.match(name);
                        int devNum = match.hasMatch() ? match.captured(1).toInt() : -1;

                        QDir asoundDir("/proc/asound");
                        QStringList cardDirs = asoundDir.entryList({"card*"}, QDir::Dirs);
                        for (const QString &cDir : cardDirs) {
                            QDir cardDir(asoundDir.filePath(cDir));
                            QStringList eldFiles = cardDir.entryList({QString("eld#0.%1").arg(devNum), QString("eld#%1.0").arg(devNum)}, QDir::Files);
                            for (const QString &eld : eldFiles) {
                                QFile f(cardDir.filePath(eld));
                                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                                    QString txt = f.readAll();
                                    f.close();
                                    if (txt.contains(QRegularExpression(R"(monitor_present\s+1)"))) {
                                        isConnectedHdmi = true;
                                        break;
                                    }
                                }
                            }
                            if (isConnectedHdmi) break;
                        }

                        if (!isConnectedHdmi) {
                            QString cleanDesc = desc;
                            cleanDesc.remove(QRegularExpression(R"(\bHDMI Audio Output\b)", QRegularExpression::CaseInsensitiveOption));
                            cleanDesc.remove(QRegularExpression(R"(HDA\s+[^,]+,\s*)"));
                            cleanDesc = cleanDesc.trimmed();
                            if (!cleanDesc.isEmpty() && !genericHdmi.match(cleanDesc).hasMatch()) {
                                isConnectedHdmi = true;
                            }
                        }

                        if (isConnectedHdmi) {
                            alsaDevices.append({name, desc});
                        }
                    } else if (name.startsWith("alsa/hw:") && !name.contains("CARD=HDMI") && !name.contains("CARD=Generic")) {
                        // External USB DAC or custom sound card hardware endpoint
                        alsaDevices.append({name, desc});
                    }
                }
            }
        }
        mpv_free_node_contents(&result);
    }

    if (alsaDevices.isEmpty()) {
        // Fallback standard ALSA devices if empty
        alsaDevices.append({"alsa/front:CARD=Generic,DEV=0", tr("HD-Audio ALC1220 Analog (front:CARD=Generic,DEV=0)")});
        alsaDevices.append({"alsa/iec958:CARD=Generic,DEV=0", tr("HD-Audio S/PDIF Digital (iec958:CARD=Generic,DEV=0)")});
    }

    for (const auto &dev : alsaDevices) {
        QString friendlyTitle;
        if (dev.name.contains("iec958", Qt::CaseInsensitive)) {
            friendlyTitle = tr("🔊 Digital Output S/PDIF (Optical / Coaxial)");
        } else if (dev.name.contains("front:", Qt::CaseInsensitive)) {
            if (dev.desc.contains("Analog", Qt::CaseInsensitive)) {
                friendlyTitle = tr("🔊 Analog Output / Headphones (3.5mm Jack)");
            } else {
                friendlyTitle = QString("🔊 %1").arg(dev.desc);
            }
        } else if (dev.name.contains("hdmi:", Qt::CaseInsensitive)) {
            QString displayName = dev.desc;
            displayName.remove(QRegularExpression(R"(\bHDMI Audio Output\b)", QRegularExpression::CaseInsensitiveOption));
            displayName.remove(QRegularExpression(R"(HDA\s+[^,]+,\s*)"));
            displayName.remove('/');
            displayName = displayName.trimmed();
            if (displayName.isEmpty()) displayName = "Monitor/TV";
            friendlyTitle = tr("🔊 HDMI Output: %1").arg(displayName);
        } else {
            friendlyTitle = QString("🔊 %1").arg(dev.desc.isEmpty() ? dev.name : dev.desc);
        }

        // Add hardware identifier for clarity
        friendlyTitle += QString(" (%1)").arg(dev.name.mid(5)); // e.g. front:CARD=Generic,DEV=0

        QAction *act = menu.addAction(QString("🎯 %1").arg(friendlyTitle));
        act->setCheckable(true);
        act->setChecked(m_currentAudioDevice == dev.name);
        QString devName = dev.name;
        connect(act, &QAction::triggered, this, [this, devName]() {
            selectAudioDevice(devName);
        });
    }

    if (m_btnAudioDevice) {
        menu.exec(m_btnAudioDevice->mapToGlobal(QPoint(0, m_btnAudioDevice->height())));
    }
}

void MediaViewer::setPlaybackSpeed(double rate)
{
    m_playbackRate = rate;
    if (m_mpv) {
        mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &rate);
    }
    m_btnSpeed->setText(QString("%1x").arg(rate, 0, 'f', 1));
}

void MediaViewer::setPosition(int position)
{
    requestSeek(static_cast<qint64>(position), true);
}

void MediaViewer::requestSeek(qint64 targetMs, bool exact)
{
    if (!m_mpv || m_durationMs <= 0) return;

    targetMs = std::clamp<qint64>(targetMs, 0, m_durationMs);
    m_isUserSeeking = true;
    m_pendingSeekMs = targetMs;

    // 1. Instantly update UI timestamps and playhead positions with zero lag
    m_positionMs = targetMs;
    if (m_timeLeftLabel) {
        m_timeLeftLabel->setText(QString("%1 / %2")
                                     .arg(formatTime(targetMs))
                                     .arg(formatTime(m_durationMs)));
    }
    if (m_timeRightLabel) {
        qint64 remMs = qMax<qint64>(0, m_durationMs - targetMs);
        m_timeRightLabel->setText(QString("-%1").arg(formatTime(remMs)));
    }
    if (m_audioOscilloscope) {
        m_audioOscilloscope->setPlaybackPosition(targetMs, m_durationMs);
    }
    if (m_sliderPosition && !m_sliderPosition->isSliderDown()) {
        m_sliderPosition->setValue(static_cast<int>(targetMs));
    }

    // 2. Perform or schedule non-blocking seek to mpv
    if (exact) {
        if (m_seekThrottleTimer && m_seekThrottleTimer->isActive()) {
            m_seekThrottleTimer->stop();
        }
        executeSeek(targetMs, true);
        // Release seek lock after a small grace period so mpv pipeline catches up cleanly
        QTimer::singleShot(150, this, [this]() {
            m_isUserSeeking = false;
        });
    } else {
        if (m_seekThrottleTimer && !m_seekThrottleTimer->isActive()) {
            m_seekThrottleTimer->start(70); // At most once every 70ms while dragging
        }
    }
}

void MediaViewer::executeSeek(qint64 targetMs, bool exact)
{
    if (!m_mpv) return;

    double sec = static_cast<double>(targetMs) / 1000.0;
    QString secStr = QString::number(sec, 'f', 3);
    QByteArray secBytes = secStr.toUtf8();

    if (exact) {
        const char *cmd[] = {"seek", secBytes.constData(), "absolute+exact", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
    } else {
        const char *cmd[] = {"seek", secBytes.constData(), "absolute+keyframes", nullptr};
        mpv_command_async(m_mpv, 0, cmd);
    }
}

bool MediaViewer::eventFilter(QObject *watched, QEvent *event)
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
                    m_sliderPosition->setValue(static_cast<int>(targetVal));
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
    return QWidget::eventFilter(watched, event);
}

QString MediaViewer::detectDefaultAlsaDevice()
{
    if (!m_mpv) return "alsa/front:CARD=Generic,DEV=0";

    mpv_node result;
    QString chosenDevice;

    if (mpv_get_property(m_mpv, "audio-device-list", MPV_FORMAT_NODE, &result) >= 0) {
        if (result.format == MPV_FORMAT_NODE_ARRAY && result.u.list) {
            // First priority: Connected HDMI endpoint (e.g. SONY TV)
            for (int i = 0; i < result.u.list->num; ++i) {
                mpv_node *item = &result.u.list->values[i];
                if (item->format == MPV_FORMAT_NODE_MAP && item->u.list) {
                    QString name;
                    QString desc;
                    for (int k = 0; k < item->u.list->num; ++k) {
                        const char *key = item->u.list->keys[k];
                        if (strcmp(key, "name") == 0 && item->u.list->values[k].format == MPV_FORMAT_STRING) {
                            name = QString::fromUtf8(item->u.list->values[k].u.string);
                        } else if (strcmp(key, "description") == 0 && item->u.list->values[k].format == MPV_FORMAT_STRING) {
                            desc = QString::fromUtf8(item->u.list->values[k].u.string);
                        }
                    }

                    if (name.startsWith("alsa/hdmi:")) {
                        if (desc.contains("SONY", Qt::CaseInsensitive) || desc.contains("TV", Qt::CaseInsensitive)) {
                            chosenDevice = name;
                            break;
                        }
                    }
                }
            }

            // Second priority: S/PDIF (iec958) or Front Analog
            if (chosenDevice.isEmpty()) {
                for (int i = 0; i < result.u.list->num; ++i) {
                    mpv_node *item = &result.u.list->values[i];
                    if (item->format == MPV_FORMAT_NODE_MAP && item->u.list) {
                        QString name;
                        for (int k = 0; k < item->u.list->num; ++k) {
                            if (strcmp(item->u.list->keys[k], "name") == 0 && item->u.list->values[k].format == MPV_FORMAT_STRING) {
                                name = QString::fromUtf8(item->u.list->values[k].u.string);
                                break;
                            }
                        }
                        if (name.startsWith("alsa/iec958:") || name.startsWith("alsa/front:")) {
                            chosenDevice = name;
                            break;
                        }
                    }
                }
            }
        }
        mpv_free_node_contents(&result);
    }

    if (chosenDevice.isEmpty()) {
        chosenDevice = "alsa/front:CARD=Generic,DEV=0";
    }
    return chosenDevice;
}

void MediaViewer::applyVolume(int sliderValue)
{
    if (!m_mpv) return;
    double vol = static_cast<double>(sliderValue);
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
    if (m_audioOscilloscope) {
        m_audioOscilloscope->setVolume(vol / 100.0);
    }
    if (m_volumePercentLabel) {
        m_volumePercentLabel->setText(QString("%1%").arg(sliderValue));
    }
    if (m_btnVolume) {
        m_btnVolume->setIcon(VectorIcons::createUiIcon(sliderValue == 0 ? VectorIcons::UiIcon::VolumeMute : VectorIcons::UiIcon::Volume, QColor("#cbd5e1"), 18));
    }
}

void MediaViewer::showSpeedMenu()
{
    QMenu menu(this);
    const QList<double> speeds = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};

    for (double s : speeds) {
        QAction *act = menu.addAction(QString("%1x").arg(s, 0, 'f', 2));
        act->setCheckable(true);
        act->setChecked(qFuzzyCompare(m_playbackRate, s));
        connect(act, &QAction::triggered, this, [this, s]() {
            setPlaybackSpeed(s);
        });
    }

    QPoint pos = m_btnSpeed->mapToGlobal(QPoint(0, -menu.sizeHint().height()));
    menu.exec(pos);
}

void MediaViewer::showAudioTracksMenu()
{
    if (!m_mpv) return;

    mpv_node trackList;
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &trackList) < 0) return;

    if (trackList.format != MPV_FORMAT_NODE_ARRAY) {
        mpv_free_node_contents(&trackList);
        return;
    }

    QMenu menu(this);

    for (int i = 0; i < trackList.u.list->num; ++i) {
        mpv_node &item = trackList.u.list->values[i];
        if (item.format != MPV_FORMAT_NODE_MAP) continue;

        QString type, id, title, lang;
        bool selected = false;

        for (int k = 0; k < item.u.list->num; ++k) {
            const char *key = item.u.list->keys[k];
            mpv_node &val = item.u.list->values[k];
            if (std::strcmp(key, "type") == 0 && val.format == MPV_FORMAT_STRING) {
                type = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "id") == 0 && val.format == MPV_FORMAT_INT64) {
                id = QString::number(val.u.int64);
            } else if (std::strcmp(key, "title") == 0 && val.format == MPV_FORMAT_STRING) {
                title = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "lang") == 0 && val.format == MPV_FORMAT_STRING) {
                lang = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "selected") == 0 && val.format == MPV_FORMAT_FLAG) {
                selected = (val.u.flag != 0);
            }
        }

        if (type == "audio") {
            QString name;
            if (!title.isEmpty() && !lang.isEmpty()) {
                name = QString("%1 (%2)").arg(title, lang);
            } else if (!title.isEmpty()) {
                name = title;
            } else if (!lang.isEmpty()) {
                name = lang;
            } else {
                name = tr("Audio Track %1").arg(id);
            }

            QAction *act = menu.addAction(name);
            act->setCheckable(true);
            act->setChecked(selected);
            connect(act, &QAction::triggered, this, [this, id]() {
                mpv_set_property_string(m_mpv, "aid", id.toUtf8().constData());
            });
        }
    }

    mpv_free_node_contents(&trackList);

    QPoint pos = m_btnAudioTracks->mapToGlobal(QPoint(0, -menu.sizeHint().height()));
    menu.exec(pos);
}

void MediaViewer::showSubtitlesMenu()
{
    if (!m_mpv) return;

    mpv_node trackList;
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &trackList) < 0) return;

    if (trackList.format != MPV_FORMAT_NODE_ARRAY) {
        mpv_free_node_contents(&trackList);
        return;
    }

    QMenu menu(this);
    bool anySubSelected = false;

    struct SubInfo {
        QString id;
        QString name;
        bool selected;
    };
    QList<SubInfo> subTracks;

    for (int i = 0; i < trackList.u.list->num; ++i) {
        mpv_node &item = trackList.u.list->values[i];
        if (item.format != MPV_FORMAT_NODE_MAP) continue;

        QString type, id, title, lang;
        bool selected = false;

        for (int k = 0; k < item.u.list->num; ++k) {
            const char *key = item.u.list->keys[k];
            mpv_node &val = item.u.list->values[k];
            if (std::strcmp(key, "type") == 0 && val.format == MPV_FORMAT_STRING) {
                type = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "id") == 0 && val.format == MPV_FORMAT_INT64) {
                id = QString::number(val.u.int64);
            } else if (std::strcmp(key, "title") == 0 && val.format == MPV_FORMAT_STRING) {
                title = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "lang") == 0 && val.format == MPV_FORMAT_STRING) {
                lang = QString::fromUtf8(val.u.string);
            } else if (std::strcmp(key, "selected") == 0 && val.format == MPV_FORMAT_FLAG) {
                selected = (val.u.flag != 0);
            }
        }

        if (type == "sub") {
            if (selected) anySubSelected = true;
            QString name;
            if (!title.isEmpty() && !lang.isEmpty()) {
                name = QString("%1 (%2)").arg(title, lang);
            } else if (!title.isEmpty()) {
                name = title;
            } else if (!lang.isEmpty()) {
                name = lang;
            } else {
                name = tr("Subtitle %1").arg(id);
            }
            subTracks.append({id, name, selected});
        }
    }

    mpv_free_node_contents(&trackList);

    QAction *actOff = menu.addAction(tr("Off"));
    actOff->setCheckable(true);
    actOff->setChecked(!anySubSelected);
    connect(actOff, &QAction::triggered, this, [this]() {
        mpv_set_property_string(m_mpv, "sid", "no");
    });

    if (!subTracks.isEmpty()) {
        menu.addSeparator();
    }

    for (const auto &sub : subTracks) {
        QAction *act = menu.addAction(sub.name);
        act->setCheckable(true);
        act->setChecked(sub.selected);
        QString id = sub.id;
        connect(act, &QAction::triggered, this, [this, id]() {
            mpv_set_property_string(m_mpv, "sid", id.toUtf8().constData());
        });
    }

    QPoint pos = m_btnSubtitles->mapToGlobal(QPoint(0, -menu.sizeHint().height()));
    menu.exec(pos);
}

void MediaViewer::openAudioTrimDialog()
{
    if (m_currentFilePath.isEmpty()) return;
    int wasPlaying = m_isPlaying;
    if (wasPlaying) togglePlayPause();

    AudioTrimDialog dlg(m_currentFilePath, this);
    dlg.exec();
}

void MediaViewer::toggleEqualizerDialog()
{
    if (m_isBitPerfect) {
        QMessageBox::information(this, tr("Bit-Perfect Mode Active"),
            tr("Equalizer and DSP processing are disabled while Bit-Perfect audiophile mode is active. Turn off Bit-Perfect to enable equalizer."));
        return;
    }

    if (!m_equalizerDialog) {
        m_equalizerDialog = new QDialog(this, Qt::Tool | Qt::WindowCloseButtonHint);
        m_equalizerDialog->setWindowTitle(tr("BubezView - Audio Equalizer & DSP"));
        m_equalizerDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        m_equalizerDialog->resize(580, 280);

        QVBoxLayout *layout = new QVBoxLayout(m_equalizerDialog);
        layout->setContentsMargins(8, 8, 8, 8);
        m_equalizerWidget = new EqualizerWidget(m_equalizerDialog);
        connect(m_equalizerWidget, &EqualizerWidget::configurationChanged,
                this, &MediaViewer::applyEqualizerSettings);
        layout->addWidget(m_equalizerWidget);
    }

    if (m_equalizerDialog->isVisible()) {
        m_equalizerDialog->hide();
    } else {
        m_equalizerDialog->show();
        m_equalizerDialog->raise();
        m_equalizerDialog->activateWindow();
    }
}

void MediaViewer::applyEqualizerSettings(const EqualizerPreset &preset, bool bypassed)
{
    if (!m_mpv) return;

    if (m_isBitPerfect || bypassed) {
        mpv_set_property_string(m_mpv, "af", "");
    } else {
        QString afString = EqualizerManager::buildMpvAfString(preset);
        mpv_set_property_string(m_mpv, "af", afString.toUtf8().constData());
    }
}

void MediaViewer::openAudioTagDialog()
{
    if (m_currentFilePath.isEmpty()) return;
    AudioTagDialog dlg(m_currentFilePath, m_siblingFiles, this);
    if (dlg.exec() == QDialog::Accepted) {
        updateAudioBadgeAndCover();
    }
}

void MediaViewer::extractAudioTrack()
{
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QString defaultOut = fi.dir().filePath(QString("%1_extracted.mp3").arg(fi.baseName()));
    QString dest = QFileDialog::getSaveFileName(this, tr("Extract Audio Track"), defaultOut, tr("MP3 Audio (*.mp3);;FLAC Lossless (*.flac);;WAV (*.wav);;AAC (*.aac)"));
    if (dest.isEmpty()) return;

    QString fmt = QFileInfo(dest).suffix().toLower();
    bool ok = AudioProcessor::extractAudioFromVideo(m_currentFilePath, dest, fmt);
    if (ok) {
        QMessageBox::information(this, tr("Success"), tr("Audio track extracted successfully!\n%1").arg(dest));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to extract audio track."));
    }
}

void MediaViewer::processMpvEvents()
{
    if (!m_mpv) return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto *prop = static_cast<mpv_event_property *>(event->data);
            handleMpvPropertyChange(prop);
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            auto *endFile = static_cast<mpv_event_end_file *>(event->data);
            qDebug() << "[BubezView-Audio] MPV_EVENT_END_FILE reason:" << (endFile ? endFile->reason : -1) << "error:" << (endFile ? endFile->error : 0);
            if (endFile && endFile->reason == MPV_END_FILE_REASON_EOF) {
                if (m_isLooping) {
                    if (m_mpv) {
                        const char *seekCmd[] = {"seek", "0", "absolute", nullptr};
                        mpv_command(m_mpv, seekCmd);
                    }
                } else if (m_isAudioOnly && m_isAutoAdvance) {
                    playNextTrack();
                } else {
                    stop();
                    m_positionMs = 0;
                    if (m_sliderPosition) m_sliderPosition->setValue(0);
                    if (m_timeLeftLabel) {
                        m_timeLeftLabel->setText(QString("00:00 / %1").arg(formatTime(m_durationMs)));
                    }
                    if (m_timeRightLabel) {
                        m_timeRightLabel->setText(QString("-%1").arg(formatTime(m_durationMs)));
                    }
                    if (m_audioOscilloscope) {
                        m_audioOscilloscope->setPlaybackPosition(0, m_durationMs);
                        m_audioOscilloscope->setPlaying(false);
                    }
                }
            } else {
                updatePlaybackStateUi(false);
            }
        } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
            qDebug() << "[BubezView-Audio] MPV_EVENT_FILE_LOADED received!";
            int pause = 0;
            mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause);
            updatePlaybackStateUi(true);
        }
    }
}

void MediaViewer::handleMpvPropertyChange(mpv_event_property *prop)
{
    if (!prop || !prop->name) return;

    if (std::strcmp(prop->name, "time-pos") == 0) {
        if (prop->format == MPV_FORMAT_DOUBLE && prop->data) {
            double sec = *static_cast<double *>(prop->data);
            onPositionChanged(static_cast<qint64>(sec * 1000.0));
        }
    } else if (std::strcmp(prop->name, "duration") == 0) {
        if (prop->format == MPV_FORMAT_DOUBLE && prop->data) {
            double sec = *static_cast<double *>(prop->data);
            onDurationChanged(static_cast<qint64>(sec * 1000.0));
        }
    } else if (std::strcmp(prop->name, "pause") == 0) {
        if (prop->format == MPV_FORMAT_FLAG && prop->data) {
            int paused = *static_cast<int *>(prop->data);
            updatePlaybackStateUi(paused == 0);
        }
    } else if (std::strcmp(prop->name, "track-list") == 0) {
        if (prop->format == MPV_FORMAT_NODE && prop->data) {
            auto *listNode = static_cast<mpv_node *>(prop->data);
            if (listNode->format == MPV_FORMAT_NODE_ARRAY) {
                int audioCount = 0;
                int subCount = 0;
                for (int i = 0; i < listNode->u.list->num; ++i) {
                    mpv_node &item = listNode->u.list->values[i];
                    if (item.format != MPV_FORMAT_NODE_MAP) continue;
                    for (int k = 0; k < item.u.list->num; ++k) {
                        if (std::strcmp(item.u.list->keys[k], "type") == 0 &&
                            item.u.list->values[k].format == MPV_FORMAT_STRING) {
                            const char *t = item.u.list->values[k].u.string;
                            if (std::strcmp(t, "audio") == 0) audioCount++;
                            else if (std::strcmp(t, "sub") == 0) subCount++;
                        }
                    }
                }
                m_btnAudioTracks->setVisible(audioCount > 1);
                m_btnSubtitles->setVisible(!m_isAudioOnly && subCount > 0);
            }
        }
    } else if (std::strcmp(prop->name, "media-title") == 0) {
        if (m_isAudioOnly && prop->format == MPV_FORMAT_STRING && prop->data) {
            QString title = QString::fromUtf8(*static_cast<char **>(prop->data));
            if (!title.isEmpty() && m_audioTitleLabel) {
                m_audioTitleLabel->setText(title);
            }
        }
    } else if (std::strcmp(prop->name, "metadata") == 0) {
        if (m_isAudioOnly && prop->format == MPV_FORMAT_NODE && prop->data) {
            auto *node = static_cast<mpv_node *>(prop->data);
            if (node->format == MPV_FORMAT_NODE_MAP) {
                QString title, artist, album;
                for (int i = 0; i < node->u.list->num; ++i) {
                    const char *key = node->u.list->keys[i];
                    if (node->u.list->values[i].format != MPV_FORMAT_STRING) continue;
                    const char *val = node->u.list->values[i].u.string;
                    if (strcasecmp(key, "title") == 0) title = QString::fromUtf8(val);
                    else if (strcasecmp(key, "artist") == 0) artist = QString::fromUtf8(val);
                    else if (strcasecmp(key, "album") == 0) album = QString::fromUtf8(val);
                }
                if (!title.isEmpty()) {
                    m_audioTitleLabel->setText(title);
                }
                if (!artist.isEmpty() && !album.isEmpty()) {
                    m_audioArtistLabel->setText(QString("%1 — %2").arg(artist, album));
                } else if (!artist.isEmpty()) {
                    m_audioArtistLabel->setText(artist);
                } else if (!album.isEmpty()) {
                    m_audioArtistLabel->setText(album);
                }
            }
        }
    }
}

void MediaViewer::onPositionChanged(qint64 position)
{
    // Ignore external updates while user is seeking or dragging slider
    if (m_isUserSeeking || (m_sliderPosition && m_sliderPosition->isSliderDown())) {
        return;
    }

    m_positionMs = position;

    // 1. Throttle slider position updates to avoid flooding the GUI thread
    if (qAbs(position - m_lastSliderPos) >= 80 || position == 0) {
        m_lastSliderPos = position;
        m_sliderPosition->setValue(static_cast<int>(position));
    }

    // 2. Only format and repaint timestamp text when the second changes
    qint64 currentSec = position / 1000;
    if (currentSec != m_lastReportedSec) {
        m_lastReportedSec = currentSec;
        if (m_timeLeftLabel) {
            m_timeLeftLabel->setText(QString("%1 / %2")
                                         .arg(formatTime(position))
                                         .arg(formatTime(m_durationMs)));
        }
        if (m_timeRightLabel) {
            qint64 remMs = qMax<qint64>(0, m_durationMs - position);
            m_timeRightLabel->setText(QString("-%1").arg(formatTime(remMs)));
        }
    }

    // 3. Update oscilloscope real waveform progress
    if (m_audioOscilloscope) {
        m_audioOscilloscope->setPlaybackPosition(position, m_durationMs);
    }
}

void MediaViewer::onDurationChanged(qint64 duration)
{
    m_durationMs = duration;
    m_sliderPosition->setRange(0, static_cast<int>(duration));
    if (m_timeLeftLabel) {
        m_timeLeftLabel->setText(QString("%1 / %2")
                                     .arg(formatTime(m_positionMs))
                                     .arg(formatTime(duration)));
    }
    if (m_timeRightLabel) {
        qint64 remMs = qMax<qint64>(0, duration - m_positionMs);
        m_timeRightLabel->setText(QString("-%1").arg(formatTime(remMs)));
    }
}

QString MediaViewer::formatTime(qint64 ms) const
{
    qint64 totalSeconds = ms / 1000;
    qint64 minutes = totalSeconds / 60;
    qint64 seconds = totalSeconds % 60;

    if (minutes >= 60) {
        qint64 hours = minutes / 60;
        minutes = minutes % 60;
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
