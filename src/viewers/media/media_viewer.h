#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QPointer>
#include <QDialog>
#include <mpv/client.h>
#include "mpv_video_widget.h"
#include "ui/equalizer_widget.h"

/**
 * @brief Universal high-performance audio and video playback widget utilizing libmpv.
 *
 * Automatically detects whether media is audio-only or contains a video stream,
 * provides hardware-accelerated (GPU) video rendering via MpvVideoWidget (OpenGL),
 * high-fidelity 64-bit float audio playback with SoX resampling, and complete playback
 * controls (play/pause, seek slider, timestamp, volume, speed, loop, track and subtitle selection).
 * Also integrates quick Swiss-knife audio tools (Trim, Tag editor, Audio extraction).
 */
#include "core/interfaces/i_media_viewer.h"

class StereoOscilloscopeWidget;

class MediaViewer : public QWidget, public IMediaViewer
{
    Q_OBJECT

public:
    explicit MediaViewer(QWidget *parent = nullptr);
    ~MediaViewer() override;

    QWidget *widget() override { return this; }

    /**
     * @brief Loads and starts playback of a media file via libmpv.
     * @param filePath Absolute path to the media file.
     * @return True if media file exists and playback started; false otherwise.
     */
    bool loadFile(const QString &filePath);

    /**
     * @brief Stops playback, clears media source, and resets metadata.
     */
    void clear();

    /**
     * @brief Toggles between playing and paused states.
     */
    void togglePlayPause();

    /**
     * @brief Stops playback and rewinds to the beginning.
     */
    void stop();

    /**
     * @brief Toggles repeat/loop playback mode.
     */
    void toggleLoop();

    /**
     * @brief Updates widget and control stylesheets to match current theme preset.
     */
    void updateTheme();

    /**
     * @brief Sets playback speed rate.
     * @param rate Playback speed multiplier (e.g., 0.5, 1.0, 1.5, 2.0).
     */
    void setPlaybackSpeed(double rate);

    /**
     * @brief Checks if media is audio-only.
     * @return True if audio-only, false if video stream.
     */
    bool isAudioOnly() const { return m_isAudioOnly; }

    /**
     * @brief Checks if loop playback is enabled.
     * @return True if looping infinitely, false otherwise.
     */
    bool isLooping() const { return m_isLooping; }

    /**
     * @brief Checks if media is currently playing.
     * @return True if actively playing, false if paused or stopped.
     */
    bool isPlaying() const { return m_isPlaying; }

    /**
     * @brief Checks if audiophile Bit-Perfect mode is enabled.
     * @return True if Bit-Perfect mode is active.
     */
    bool isBitPerfect() const { return m_isBitPerfect; }

    /**
     * @brief Gets the current audio output device identifier.
     * @return Audio output endpoint string (e.g., "auto" or "alsa/...").
     */
    QString currentAudioDevice() const { return m_currentAudioDevice; }

    /**
     * @brief Restarts PipeWire/WirePlumber user services to immediately release ALSA endpoints and unblock mixer.
     */
    static void resetAudioMixer();
    void resetMixer() override { resetAudioMixer(); }

    /**
     * @brief Requests PipeWire / WirePlumber to release ALSA hardware endpoints via FreeDesktop ReserveDevice1 DBus protocol.
     * @param cardIndex Specific ALSA card index to release, or -1 for all sound cards.
     */
    static void requestAlsaDeviceRelease(int cardIndex = -1);

    /**
     * @brief Gets the audio title currently displayed in the audio cover widget.
     * @return Displayed title string.
     */
    QString audioTitle() const { return m_audioTitleLabel ? m_audioTitleLabel->text() : QString(); }

    /**
     * @brief Gets the artist name currently displayed in the audio cover widget.
     * @return Displayed artist string.
     */
    QString audioArtist() const { return m_audioArtistLabel ? m_audioArtistLabel->text() : QString(); }

    /**
     * @brief Gets current playback speed multiplier.
     * @return Playback speed rate.
     */
    double playbackSpeed() const { return m_playbackRate; }

    /**
     * @brief Gets current media duration in milliseconds.
     * @return Total duration in milliseconds.
     */
    qint64 duration() const { return m_durationMs; }

    /**
     * @brief Current media file path loaded in the viewer.
     */
    QString currentFilePath() const { return m_currentFilePath; }

public slots:
    /**
     * @brief Sets the list of sibling files in the current folder for batch tools.
     */
    void setSiblingFiles(const QStringList &files);

    /**
     * @brief Opens the Audio Cutter / Trim dialog for the current media.
     */
    void openAudioTrimDialog();

    /**
     * @brief Opens the Audio Tag & Metadata editor dialog.
     */
    void openAudioTagDialog();

    /**
     * @brief Extracts audio track from the current video file.
     */
    void extractAudioTrack();

    /**
     * @brief Toggles audiophile Bit-Perfect playback mode.
     */
    void toggleBitPerfect();

    /**
     * @brief Enables or disables Bit-Perfect mode.
     * @param enabled True to enable Bit-Perfect stream pass-through.
     */
    void setBitPerfect(bool enabled);

    /**
     * @brief Displays the audio output device selection popup menu (Direct ALSA / PipeWire).
     */
    void showAudioDeviceMenu();

    /**
     * @brief Selects the active audio output endpoint.
     * @param deviceName mpv audio-device string or "auto".
     */
    void selectAudioDevice(const QString &deviceName);

    /**
     * @brief Requests a seek to target position with optional throttling during drags.
     * @param targetMs Target position in milliseconds.
     * @param exact True if exact frame seek should be executed (on release/click).
     */
    void requestSeek(qint64 targetMs, bool exact = false);

signals:
    /**
     * @brief Emitted when media duration or status is updated.
     * @param info Status information description.
     */
    void mediaStatusChanged(const QString &info);

    /**
     * @brief Emitted when an audio track change is requested (Next, Prev, Shuffle, or Auto-Advance).
     * @param filePath Absolute path of the next track to play.
     */
    void trackChangeRequested(const QString &filePath);

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void setPosition(int position);
    void applyVolume(int sliderValue);
    void showSpeedMenu();
    void showAudioTracksMenu();
    void showSubtitlesMenu();
    void processMpvEvents();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    QString formatTime(qint64 ms) const;
    void executeSeek(qint64 targetMs, bool exact = false);
    QString detectDefaultAlsaDevice();
    void updateLoopButtonStyle();
    void updateAutoAdvanceUi();
    void updatePlaybackStateUi(bool playing);
    void updateBitPerfectUi();
    void applyBitPerfectSettings();
    void toggleEqualizerDialog();
    void applyEqualizerSettings(const EqualizerPreset &preset, bool bypassed);
    void updateAudioDeviceUi();
    void applyAudioDeviceSettings(bool forceReload = false);
    void handleMpvPropertyChange(mpv_event_property *prop);
    void updateAudioBadgeAndCover();
    void reloadCurrentTrack(int delayMs = -1);
    void scanAudioSiblings();

    void playNextTrack();
    void playPreviousTrack();
    void toggleShuffle();
    void toggleAutoAdvance();

    mpv_handle *m_mpv;
    MpvVideoWidget *m_videoWidget;

    QWidget *m_audioCoverWidget;
    QLabel *m_audioCoverImageLabel;
    QFrame *m_metaCard;
    QLabel *m_audioTitleLabel;
    QLabel *m_audioArtistLabel;
    QLabel *m_audioAlbumLabel;
    QLabel *m_audioYearLabel;
    QLabel *m_audioQualityBadge;
    QLabel *m_audioSpecsLabel;
    QLabel *m_audioFileDetailsLabel;
    StereoOscilloscopeWidget *m_audioOscilloscope;

    QWidget *m_controlsWidget;
    QSlider *m_sliderPosition;
    QLabel *m_timeLeftLabel;
    QLabel *m_timeRightLabel;
    QLabel *m_bottomTitleLabel;
    QLabel *m_bottomFileLabel;
    QPushButton *m_btnShuffle;
    QPushButton *m_btnPrev;
    QPushButton *m_btnPlayPause;
    QPushButton *m_btnNext;
    QPushButton *m_btnLoop;
    QPushButton *m_btnAutoAdvance;
    QLabel *m_volumePercentLabel;
    QPushButton *m_btnVolume;
    QSlider *m_sliderVolume;
    QPushButton *m_btnSpeed;
    QPushButton *m_btnBitPerfect;
    QPushButton *m_btnAudioDevice;
    QPushButton *m_btnAudioTracks;
    QPushButton *m_btnSubtitles;
    QPushButton *m_btnTrim;
    QPushButton *m_btnTags;
    QPushButton *m_btnEqualizer;
    QPushButton *m_btnExtractAudio;

    QPointer<QDialog> m_equalizerDialog;
    EqualizerWidget *m_equalizerWidget;

    QString m_currentFilePath;
    QStringList m_siblingFiles;
    QStringList m_audioSiblingFiles;
    double m_playbackRate;
    qint64 m_durationMs;
    qint64 m_positionMs;
    qint64 m_lastSliderPos;
    qint64 m_lastReportedSec;
    bool m_isAudioOnly;
    bool m_isLooping;
    bool m_isAutoAdvance;
    bool m_isShuffle;
    bool m_isPlaying;
    bool m_isBitPerfect;
    QString m_currentAudioDevice;
    QString m_lastBitPerfectDevice;
    bool m_isUserSeeking;
    qint64 m_pendingSeekMs;
    QTimer *m_seekThrottleTimer;
    QTimer *m_reloadTimer;
};
