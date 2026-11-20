/**
 * @file i_video_studio_plugin.h
 * @brief Qt plugin interface for the modular Video Studio engine (bubez_video.so).
 *
 * Keeps video editing tools (lossless trim/cut, audio extraction, frame snapshot,
 * rotation/flip, resolution scaling, speed, and compression) decoupled from the
 * lightweight BubezView core video player.
 */

#pragma once

#include <QtPlugin>
#include <QString>
#include <QWidget>
#include <functional>

/**
 * @class IVideoStudioWidget
 * @brief Abstract video studio side-panel interface implemented inside bubez_video.so.
 */
class IVideoStudioWidget : public QWidget {
public:
    using QWidget::QWidget;
    ~IVideoStudioWidget() override = default;

    /**
     * @brief Sets the active video file path and its total duration in milliseconds.
     * @param filePath Absolute path to the video file.
     * @param durationMs Total duration in milliseconds (0 if unknown yet).
     */
    virtual void setTargetVideo(const QString &filePath, qint64 durationMs) = 0;

    /**
     * @brief Updates the current playback position and total duration from the player.
     * @param positionMs Current playback position in milliseconds.
     * @param durationMs Total video duration in milliseconds.
     */
    virtual void setPlaybackPosition(qint64 positionMs, qint64 durationMs) = 0;

    /**
     * @brief Synchronizes visual styling with the active BubezView theme preset.
     */
    virtual void updateTheme() = 0;

    /**
     * @brief Cancels any active FFmpeg background processing safely.
     */
    virtual void cancelActiveOperations() = 0;

    /**
     * @brief Registers a callback invoked when a new/edited media file is exported on disk.
     * @param cb Callback receiving the exported file path.
     */
    virtual void setOnOutputCreated(std::function<void(const QString &outputPath)> cb) = 0;
};

/**
 * @class IVideoStudioPlugin
 * @brief Qt plugin factory interface implemented by bubez_video.so.
 */
class IVideoStudioPlugin {
public:
    virtual ~IVideoStudioPlugin() = default;

    /**
     * @brief Unique identifier of the plugin ("video").
     * @return Plugin ID string.
     */
    virtual QString pluginId() const = 0;

    /**
     * @brief Human-readable plugin name.
     * @return Display name string.
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Short description of plugin capabilities.
     * @return Description string.
     */
    virtual QString description() const = 0;

    /**
     * @brief Instantiates an embedded Video Studio side panel widget.
     * @param parent Optional parent widget.
     * @return Newly created IVideoStudioWidget instance.
     */
    virtual IVideoStudioWidget *createStudioWidget(QWidget *parent = nullptr) = 0;
};

#define IVideoStudioPlugin_iid "org.bubezview.IVideoStudioPlugin/1.0"
Q_DECLARE_INTERFACE(IVideoStudioPlugin, IVideoStudioPlugin_iid)
