/**
 * @file system_media_viewer.h
 * @brief Unified audio and video playback widget utilizing Qt6 Multimedia.
 *
 * Provides the identical, high-fidelity BubezView media player user interface
 * (vinyl record sleeve, centered album art, typography, waveform scrubber, and transport controls)
 * powered by Qt6 Multimedia when libmpv is unavailable.
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
#include <QBoxLayout>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QListWidget>
#include <QTimer>
#include "core/interfaces/i_media_viewer.h"
#include "vinyl_sleeve_widget.h"
#include "smooth_waveform_scrubber.h"

class IVideoStudioWidget;

/**
 * @class SystemMediaViewer
 * @brief Native media viewer providing audio and video playback via Qt6 Multimedia
 * while sharing the exact same visual design system as the primary MediaViewer.
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
    bool isAudioOnly() const override { return m_isAudioOnly; }

    /**
     * @brief Sets the list of sibling media files in the current directory for playlist traversal.
     * @param files List of absolute file paths.
     */
    void setSiblingFiles(const QStringList &files) override;

    /**
     * @brief Sets or checks edit mode state.
     */
    void setEditMode(bool editMode) override;
    bool isEditMode() const override { return m_isEditMode; }

    /**
     * @brief Attaches a modular Video Studio side panel loaded from bubez_video.so.
     * @param studioWidget Newly created IVideoStudioWidget instance (owned by SystemMediaViewer).
     */
    void attachVideoStudioWidget(IVideoStudioWidget *studioWidget);

    /**
     * @brief Synchronously detaches and destroys the Video Studio side panel (safe before plugin unload).
     */
    void detachVideoStudioWidget();

    /**
     * @brief Returns whether a modular Video Studio panel is currently attached.
     */
    bool hasVideoStudioWidget() const { return m_videoStudioWidget != nullptr; }

    /**
     * @brief Switches the active audio workspace page.
     * In system mode, workspaces > 0 require the MPV multimedia engine.
     */
    void switchAudioWorkspace(int index) override;
    int audioWorkspaceIndex() const override { return 0; }

    /**
     * @brief Studio tool actions prompt for MPV module installation.
     */
    void openAudioTrimDialog() override;
    void toggleAudioStudio() override { openAudioTrimDialog(); }
    bool isAudioStudioActive() const override { return false; }
    void openAudioTagDialog() override;
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

    /**
     * @brief Updates widget and control stylesheets to match current theme preset.
     */
    void updateTheme() override;

    /**
     * @brief Returns whether the audio player is currently in compact stacked layout.
     */
    bool isCompactAudioLayout() const { return m_isCompactAudioLayout; }

    /**
     * @brief Dynamically adjusts audio player elements between wide and compact modes.
     * @param forceW Optional width override.
     * @param forceH Optional height override.
     */
    void updateAudioResponsiveLayout(int forceW = 0, int forceH = 0) override;

    /**
     * @brief Returns a pointer to the Pro Mode activation button.
     * @return Pointer to the QPushButton triggering Pro Mode upgrade.
     */
    QPushButton *proModeButton() const { return m_btnProMode; }

public slots:
    /**
     * @brief Requests a seek to target position, updating UI immediately during drag
     * and committing the pipeline seek on release/click (exact == true).
     * @param targetMs Target position in milliseconds.
     * @param exact True when the user releases the mouse or clicks to commit the seek.
     */
    void requestSeek(qint64 targetMs, bool exact = false);

    /**
     * @brief Toggles the collapsible audio tracklist drawer.
     */
    void toggleTracklistDrawer();

    /**
     * @brief Refreshes the audio tracklist drawer with sibling audio files in the current folder.
     */
    void updateTracklistDrawer();

protected:
    /**
     * @brief Intercepts keyboard events and slider clicks for media control shortcuts.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /**
     * @brief Handles window resize to adjust between horizontal and compact responsive layouts.
     */
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onHasVideoChanged(bool hasVideo);
    void seekTo(qint64 positionMs);
    void setVolume(int volumePercent);
    void toggleMute();
    void playNextTrack();
    void playPreviousTrack();
    void toggleLoop();
    void toggleShuffle();
    void toggleAutoAdvance();
    void showSpeedMenu();
    void setPlaybackSpeed(double rate);

private:
    void setupUi();
    void updatePlayPauseUi(bool isPlaying);
    void updateLoopButtonStyle();
    void updateAutoAdvanceUi();
    void updateTimeLabel(qint64 currentMs, qint64 totalMs);
    void scanAudioSiblings();
    void promptMpvRequired(const QString &toolName);
    static QString formatTime(qint64 ms);

    // Qt Multimedia Core
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;

    // Visual Canvas Container
    QWidget *m_canvasContainer = nullptr;

    // Audio Player Mode (Vinyl Sleeve + Metadata)
    QWidget *m_audioCoverWidget = nullptr;
    QBoxLayout *m_audioTopLayout = nullptr;
    VinylSleeveWidget *m_vinylSleeveWidget = nullptr;
    QFrame *m_metaCard = nullptr;
    QVBoxLayout *m_metaLayout = nullptr;
    QHBoxLayout *m_badgeLayout = nullptr;
    QLabel *m_audioQualityBadge = nullptr;
    QLabel *m_audioTitleLabel = nullptr;
    QLabel *m_audioArtistLabel = nullptr;
    QLabel *m_audioAlbumLabel = nullptr;
    QLabel *m_audioSpecsLabel = nullptr;

    // Badges Container
    QWidget *m_tagsBadgeContainer = nullptr;
    QHBoxLayout *m_tagsBadgeLayout = nullptr;
    QLabel *m_badgeGenre = nullptr;
    QLabel *m_badgeTrack = nullptr;
    QLabel *m_badgeDate = nullptr;
    QLabel *m_badgeTech = nullptr;
    QLabel *m_badgeComment = nullptr;

    // Timeline Scrubber
    QWidget *m_controlsWidget = nullptr;
    QFrame *m_waveformContainer = nullptr;
    QLabel *m_waveTimeLeftLabel = nullptr;
    SmoothWaveformScrubber *m_waveformScrubber = nullptr;
    QLabel *m_waveTimeRightLabel = nullptr;

    // Video Slider Row
    QWidget *m_videoSliderWidget = nullptr;
    QLabel *m_timeLeftLabel = nullptr;
    QSlider *m_sliderPosition = nullptr;
    QLabel *m_timeRightLabel = nullptr;

    // Transport Controls
    QPushButton *m_btnSpeed = nullptr;
    QPushButton *m_btnProMode = nullptr;
    QLabel *m_bottomTitleLabel = nullptr;
    QLabel *m_bottomFileLabel = nullptr;
    QPushButton *m_btnShuffle = nullptr;
    QPushButton *m_btnPrev = nullptr;
    QPushButton *m_btnPlayPause = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnNext = nullptr;
    QPushButton *m_btnLoop = nullptr;
    QPushButton *m_btnAutoAdvance = nullptr;
    QPushButton *m_btnTracklist = nullptr;
    QFrame *m_tracklistDrawer = nullptr;
    QListWidget *m_tracklistWidget = nullptr;
    QPushButton *m_btnMute = nullptr;
    QSlider *m_sliderVolume = nullptr;

    // State Tracking
    QString m_currentFilePath;
    QStringList m_siblingFiles;
    QStringList m_audioSiblingFiles;
    int m_currentIndex = -1;
    qint64 m_durationMs = 0;
    qint64 m_currentPositionMs = 0;
    qint64 m_lastReportedSec = -1;
    qint64 m_lastSeekTargetMs = -1;
    qint64 m_postSeekObservedPos = -1;
    QTimer *m_seekGraceTimer = nullptr;
    QTimer *m_seekWatchdogTimer = nullptr;
    bool m_isAudioOnly = true;
    bool m_isLooping = false;
    bool m_isAutoAdvance = true;
    bool m_isShuffle = false;
    bool m_isSeeking = false;
    bool m_isEditMode = false;
    bool m_isCompactAudioLayout = false;
    float m_previousVolume = 1.0f;
    double m_playbackRate = 1.0;

    QHBoxLayout *m_rootSplitLayout = nullptr;
    IVideoStudioWidget *m_videoStudioWidget = nullptr;
};
