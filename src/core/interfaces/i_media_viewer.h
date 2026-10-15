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

protected:
    std::function<void(const QString&)> m_trackChangeCallback;
};
