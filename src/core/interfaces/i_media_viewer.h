/**
 * @file i_media_viewer.h
 * @brief Abstract interface for the Audio/Video Playback engine in BubezView.
 *
 * Exposes media playback, audio trimming, tag editing, and hardware device routing
 * without coupling the host to libmpv headers.
 */

#pragma once

#include "i_viewer_widget.h"
#include <QStringList>
#include <functional>

/**
 * @class IMediaViewer
 * @brief Abstract interface implemented by the Media viewer plugin.
 */
class IMediaViewer : public IViewerWidget {
public:
    virtual ~IMediaViewer() = default;

    /**
     * @brief Stops media playback and releases hardware audio/video resources.
     */
    virtual void stop() = 0;

    /**
     * @brief Toggles between playback and pause states.
     */
    virtual void togglePlayPause() = 0;

    /**
     * @brief Determines whether current track is purely audio (no video stream).
     * @return True if audio-only.
     */
    virtual bool isAudioOnly() const = 0;

    /**
     * @brief Sets playlist of sibling media files in current folder.
     * @param files List of absolute file paths.
     */
    virtual void setSiblingFiles(const QStringList &files) = 0;

    /**
     * @brief Opens interactive lossless Audio Trimmer and waveform cutter dialog.
     */
    virtual void openAudioTrimDialog() = 0;

    /**
     * @brief Switches the active audio workspace page (0=Player, 1=Equalizer, 2=Tags, 3=Trim, 4=Restore).
     * @param index Target 0-based workspace page index.
     */
    virtual void switchAudioWorkspace(int index) { Q_UNUSED(index); }
    virtual int audioWorkspaceIndex() const { return 0; }

    /**
     * @brief Toggles between the Vinyl Turntable Player and the in-place Audio Studio workspace.
     */
    virtual void toggleAudioStudio() {}
    virtual bool isAudioStudioActive() const { return false; }

    /**
     * @brief Toggles the audio equalizer dialog.
     */
    virtual void toggleEqualizerDialog() {}

    /**
     * @brief Opens ID3 metadata, album cover, and tag editing dialog.
     */
    virtual void openAudioTagDialog() = 0;

    /**
     * @brief Extracts audio stream from currently playing video into standalone audio file.
     */
    virtual void extractAudioTrack() = 0;

    /**
     * @brief Checks if bit-perfect direct ALSA hardware stream is active.
     * @return True if bit-perfect.
     */
    virtual bool isBitPerfect() const = 0;

    /**
     * @brief Returns current active audio output device string identifier.
     * @return Output device string (e.g. "alsa/hw:0,0").
     */
    virtual QString currentAudioDevice() const = 0;

    /**
     * @brief Resets audio mixer stream routing and releases exclusive hardware lock.
     */
    virtual void resetMixer() {}

    /**
     * @brief Registers callback invoked when track change is requested.
     * @param callback Function receiving new track path.
     */
    virtual void setOnTrackChangeRequested(std::function<void(const QString &trackPath)> callback) {
        m_trackChangeCallback = callback;
    }

    /**
     * @brief Registers callback invoked when active audio workspace page changes.
     * @param callback Function receiving new 0-based workspace index.
     */
    virtual void setOnAudioWorkspaceChanged(std::function<void(int workspaceIndex)> callback) {
        m_audioWorkspaceChangedCallback = callback;
    }

    /**
     * @brief Registers callback invoked when an audio workspace's modification state changes.
     * @param callback Function receiving workspace index and modified bool.
     */
    virtual void setOnAudioWorkspaceModified(std::function<void(int workspaceIndex, bool modified)> callback) {
        m_audioWorkspaceModifiedCallback = callback;
    }

    /**
     * @brief Updates the audio player responsive layout to match target dimensions.
     * @param forceW Target width override or 0 to use current width.
     * @param forceH Target height override or 0 to use current height.
     */
    virtual void updateAudioResponsiveLayout(int forceW = 0, int forceH = 0) {
        Q_UNUSED(forceW);
        Q_UNUSED(forceH);
    }

protected:
    std::function<void(const QString&)> m_trackChangeCallback;
    std::function<void(int)> m_audioWorkspaceChangedCallback;
    std::function<void(int, bool)> m_audioWorkspaceModifiedCallback;
};
