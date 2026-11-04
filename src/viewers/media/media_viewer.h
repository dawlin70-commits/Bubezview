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
#include "equalizer_widget.h"
#include "core/audio/audio_processor.h"
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>

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
class AudioTrimDialog;
class AudioTagDialog;

/**
 * @class VinylSleeveWidget
 * @brief Analog studio artwork widget rendering the front album sleeve on the left
 * with a realistic grooved vinyl record and circular center label peeking out on the right.
 */
class VinylSleeveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VinylSleeveWidget(QWidget *parent = nullptr);

    /**
     * @brief Updates the front sleeve artwork and the center vinyl disc label artwork.
     * @param sleeveCover Primary album front cover pixmap.
     * @param vinylLabel Secondary disc/vinyl center label pixmap (falls back to sleeveCover if null).
     */
    void setArtwork(const QPixmap &sleeveCover, const QPixmap &vinylLabel = QPixmap());

    /**
     * @brief Clears current artwork and restores the default studio vinyl placeholder.
     */
    void clearArtwork();

    /**
     * @brief Toggles compact layout mode (Option 2B: centered square album cover without peeking vinyl disc).
     * @param compact True for compact mode, false for full vinyl sleeve + disc mode.
     */
    void setCompactMode(bool compact);
    bool isCompactMode() const { return m_compactMode; }

    QSize sizeHint() const override { return m_compactMode ? QSize(180, 180) : QSize(360, 240); }
    QSize minimumSizeHint() const override { return m_compactMode ? QSize(70, 70) : QSize(200, 130); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_sleeveCover;
    QPixmap m_vinylLabel;
    bool m_compactMode = false;
};

/**
 * @class SmoothWaveformScrubber
 * @brief Interactive smooth continuous spline waveform timeline scrubber for audio preview.
 */
class SmoothWaveformScrubber : public QWidget
{
    Q_OBJECT

public:
    explicit SmoothWaveformScrubber(QWidget *parent = nullptr);

    /**
     * @brief Asynchronously computes and loads smooth waveform amplitude peaks for the audio file.
     * @param filePath Absolute path to the audio file.
     */
    void loadAudioWaveform(const QString &filePath);

    /**
     * @brief Updates the current playback position and playhead progress.
     * @param currentMs Current playback position in milliseconds.
     * @param totalDurationMs Total track duration in milliseconds.
     */
    void setPlaybackPosition(qint64 currentMs, qint64 totalDurationMs);

    /**
     * @brief Resets the waveform and playhead to initial state.
     */
    void reset();

signals:
    /**
     * @brief Emitted when the user clicks or drags along the waveform to seek.
     * @param targetMs Target playback position in milliseconds.
     * @param exact True on mouse release for exact frame seek, false during drag.
     */
    void seekRequested(qint64 targetMs, bool exact);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 calculateSeekMs(int mouseX) const;
    static QVector<float> makeDefaultEnvelope(int count);

    QVector<float> m_peaks;
    QString m_currentFilePath;
    qint64 m_currentMs = 0;
    qint64 m_totalDurationMs = 0;
    double m_progress = 0.0;
    bool m_isDragging = false;
};

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
     * @brief Switches the active audio workspace page (0=Preview Player, 1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags).
     * @param targetIndex Target workspace index (0..5).
     */
    void switchAudioWorkspace(int targetIndex) override;

    /**
     * @brief Gets current active audio workspace page index (0..5).
     */
    int audioWorkspaceIndex() const override;

    /**
     * @brief Toggles editing capabilities on the media viewer.
     */
    void setEditMode(bool editMode) override;
    bool isEditMode() const override { return m_isEditMode; }

    /**
     * @brief Opens the Audio Cutter / Trim dialog for the current media.
     */
    void openAudioTrimDialog() override;

    /**
     * @brief Toggles between Vinyl Turntable Player and full in-place Audio Studio workspace.
     */
    void toggleAudioStudio() override;
    bool isAudioStudioActive() const override;

    /**
     * @brief Toggles the audio equalizer dialog or switches to Equalizer workspace.
     */
    void toggleEqualizerDialog() override;

    /**
     * @brief Opens the Audio Tag & Metadata editor dialog or switches to Tags workspace.
     */
    void openAudioTagDialog() override;

    /**
     * @brief Toggles the collapsible audio tracklist drawer.
     */
    void toggleTracklistDrawer();

    /**
     * @brief Refreshes the audio tracklist drawer with sibling files in the current folder.
     */
    void updateTracklistDrawer();

    /**
     * @brief Applies channel mode routing (Stereo, Mono, Swap, Karaoke).
     */
    void applyChannelMode(ChannelMode mode);

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

    /**
     * @brief Returns whether the audio player is currently in compact stacked responsive layout.
     */
    bool isCompactAudioLayout() const { return m_isCompactAudioLayout; }

    /**
     * @brief Returns the inner vinyl sleeve artwork widget.
     */
    VinylSleeveWidget *vinylSleeveWidget() const { return m_vinylSleeveWidget; }

    /**
     * @brief Returns the metadata container card widget.
     */
    QWidget *metaCardWidget() const { return m_metaCard; }

    /**
     * @brief Returns the title label widget.
     */
    QLabel *audioTitleLabelWidget() const { return m_audioTitleLabel; }

    /**
     * @brief Dynamically adjusts audio player elements between wide and compact modes.
     * @param forceW Optional width override (e.g. pre-edit target width).
     * @param forceH Optional height override.
     */
    void updateAudioResponsiveLayout(int forceW = 0, int forceH = 0) override;

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

    /**
     * @brief Emitted when active audio workspace page changes (0..6).
     * @param index Target workspace page index.
     */
    void audioWorkspaceChanged(int index);

    /**
     * @brief Emitted when an audio workspace's modification state changes.
     * @param workspaceIndex Target workspace index (1..5).
     * @param modified True if changes are active.
     */
    void audioWorkspaceModified(int workspaceIndex, bool modified);

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
    void resizeEvent(QResizeEvent *event) override;

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

    void loadWorkspaceMetadata(const QString &filePath);

    mpv_handle *m_mpv;
    MpvVideoWidget *m_videoWidget;

    class QStackedWidget *m_audioStackedWidget = nullptr;
    QLabel *m_workspaceOverlay = nullptr;
    class QPropertyAnimation *m_audioTransitionAnim = nullptr;
    int m_currentWorkspaceIndex = 0;

    // Page 0: Vinyl Player
    QWidget *m_audioCoverWidget = nullptr;
    QBoxLayout *m_audioTopLayout = nullptr;
    VinylSleeveWidget *m_vinylSleeveWidget = nullptr;
    QLabel *m_audioCoverImageLabel = nullptr;
    QFrame *m_metaCard = nullptr;
    QVBoxLayout *m_metaLayout = nullptr;
    QHBoxLayout *m_badgeLayout = nullptr;
    QLabel *m_audioTitleLabel = nullptr;
    QLabel *m_audioArtistLabel = nullptr;
    QLabel *m_audioAlbumLabel = nullptr;
    QLabel *m_audioYearLabel = nullptr;
    QLabel *m_audioQualityBadge = nullptr;
    QLabel *m_audioSpecsLabel = nullptr;
    QLabel *m_audioFileDetailsLabel = nullptr;
    QWidget *m_tagsBadgeContainer = nullptr;
    QHBoxLayout *m_tagsBadgeLayout = nullptr;
    QLabel *m_badgeGenre = nullptr;
    QLabel *m_badgeTrack = nullptr;
    QLabel *m_badgeDate = nullptr;
    QLabel *m_badgeTech = nullptr;
    QLabel *m_badgeComment = nullptr;
    StereoOscilloscopeWidget *m_audioOscilloscope = nullptr;
    bool m_isCompactAudioLayout = false;

    // Embedded Full Audio Studio (Page 1: Trim, Filters, Pitch & Ch., Equalizer) & Album Tag Editor (Page 2: Tags)
    AudioTrimDialog *m_embeddedTrimDialog = nullptr;
    AudioTagDialog *m_embeddedTagDialog = nullptr;
    EqualizerWidget *m_equalizerWidget = nullptr;
    ChannelMode m_currentChannelMode = ChannelMode::Original;

    QWidget *m_controlsWidget;
    QFrame *m_waveformContainer;
    SmoothWaveformScrubber *m_waveformScrubber;
    QLabel *m_waveTimeLeftLabel;
    QLabel *m_waveTimeRightLabel;
    QWidget *m_videoSliderWidget;
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
    QPushButton *m_btnTracklist = nullptr;
    QFrame *m_tracklistDrawer = nullptr;
    QListWidget *m_tracklistWidget = nullptr;

    QPointer<QDialog> m_equalizerDialog;

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
    bool m_isEditMode = false;
};
