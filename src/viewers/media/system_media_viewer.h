/**
 * @file system_media_viewer.h
 * @brief Native audio and video playback widget utilizing Qt6 Multimedia and operating system codecs.
 *
 * Serves as the lightweight, zero-dependency media engine for BubezView. When the high-performance
 * MPV GPU player is not installed or enabled, SystemMediaViewer seamlessly plays audio and video
 * using platform decoders (GStreamer, PipeWire, ALSA, FFmpeg).
 */

#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "core/interfaces/i_media_viewer.h"

/**
 * @class SystemMediaViewer
 * @brief Native media viewer providing audio and video playback via Qt6 Multimedia.
 */
class SystemMediaViewer : public QWidget, public IMediaViewer
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the SystemMediaViewer widget and initializes Qt6 Multimedia.
     * @param parent Optional parent widget.
     */
    explicit SystemMediaViewer(QWidget *parent = nullptr);

    /**
     * @brief Cleans up media player resources and stops playback.
     */
    ~SystemMediaViewer() override;

    /**
     * @brief Returns the underlying QWidget pointer for embedding into layouts.
     * @return Pointer to this QWidget.
     */
    QWidget *widget() override { return this; }

    /**
     * @brief Loads and plays the specified audio or video file using system codecs.
     * @param filePath Absolute path of the media file to load.
     * @return True if file exists and playback was dispatched; false otherwise.
     */
    bool loadFile(const QString &filePath) override;

    /**
     * @brief Clears current playback state, media source, and visual metadata.
     */
    void clear() override;

    /**
     * @brief Stops media playback and rewinds to the beginning.
     */
    void stop() override;

    /**
     * @brief Toggles between active playback and paused state.
     */
    void togglePlayPause() override;

    /**
     * @brief Determines whether the active track is purely audio (no video stream).
     * @return True if audio only, false if video stream is present.
     */
    bool isAudioOnly() const override;

    /**
     * @brief Sets the list of sibling media files in the current directory for playlist traversal.
     * @param files List of absolute file paths.
     */
    void setSiblingFiles(const QStringList &files) override;

    /**
     * @brief Audio trim placeholder for system playback mode.
     */
    void openAudioTrimDialog() override;

    /**
     * @brief Audio tag editor placeholder or dispatch for system playback mode.
     */
    void openAudioTagDialog() override;

    /**
     * @brief Audio extraction placeholder or dispatch for system playback mode.
     */
    void extractAudioTrack() override;

    /**
     * @brief Direct ALSA hardware mode is exclusive to the high-performance MPV player.
     * @return Always false in system playback mode.
     */
    bool isBitPerfect() const override { return false; }

    /**
     * @brief Returns active audio device description.
     * @return String identifying default system audio routing.
     */
    QString currentAudioDevice() const override { return QStringLiteral("OS Default Audio"); }

    /**
     * @brief Resets audio output state.
     */
    void resetMixer() override;

protected:
    /**
     * @brief Intercepts keyboard events for media control shortcuts (Space, Left/Right arrows, Up/Down).
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /** @brief Updates the timeline slider and timestamp label when playback position advances. */
    void onPositionChanged(qint64 position);
    /** @brief Updates the timeline slider range when total media duration is resolved. */
    void onDurationChanged(qint64 duration);
    /** @brief Updates the play/pause button icon when playback state transitions. */
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    /** @brief Handles end-of-media looping or automatic track advancement. */
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    /** @brief Switches between video canvas and audio metadata card based on video stream presence. */
    void onHasVideoChanged(bool hasVideo);
    /** @brief Seeks to the specified playback position in milliseconds. */
    void seekTo(int positionMs);
    /** @brief Adjusts audio output volume percentage (0..100). */
    void setVolume(int volumePercent);
    /** @brief Toggles audio output mute state. */
    void toggleMute();
    /** @brief Advances to the next media file in the current directory playlist. */
    void playNextTrack();
    /** @brief Returns to the previous media file in the current directory playlist. */
    void playPreviousTrack();
    /** @brief Toggles single-track repeat loop mode. */
    void toggleLoop();

private:
    /** @brief Constructs the video surface, audio card, and bottom transport bar. */
    void setupUi();
    /** @brief Updates the play/pause button icon to match current state. */
    void updatePlayPauseUi(bool isPlaying);
    /** @brief Updates the formatted current/total duration readout label. */
    void updateTimeLabel(qint64 currentMs, qint64 totalMs);
    /** @brief Formats a duration in milliseconds as MM:SS or HH:MM:SS. */
    static QString formatTime(qint64 ms);

    // Qt Multimedia Core
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;

    // UI Widgets
    QWidget *m_canvasContainer = nullptr;
    QWidget *m_audioCard = nullptr;
    QLabel *m_lblAudioCover = nullptr;
    QLabel *m_lblAudioTitle = nullptr;
    QLabel *m_lblAudioArtist = nullptr;
    QLabel *m_lblEngineBadge = nullptr;

    QWidget *m_controlsBar = nullptr;
    QPushButton *m_btnPrev = nullptr;
    QPushButton *m_btnPlayPause = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnNext = nullptr;
    QPushButton *m_btnLoop = nullptr;
    QPushButton *m_btnMute = nullptr;
    QSlider *m_sliderPosition = nullptr;
    QSlider *m_sliderVolume = nullptr;
    QLabel *m_lblTime = nullptr;

    // State Tracking
    QString m_currentFilePath;
    QStringList m_siblingFiles;
    int m_currentIndex = -1;
    bool m_isLooping = false;
    bool m_isSeeking = false;
    float m_previousVolume = 1.0f;
};
