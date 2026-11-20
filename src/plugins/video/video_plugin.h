/**
 * @file video_plugin.h
 * @brief Qt plugin entry point for the modular Video Studio engine (bubez_video.so).
 */

#pragma once

#include "core/interfaces/i_video_studio_plugin.h"
#include <QObject>

/**
 * @class VideoStudioPlugin
 * @brief Concrete Qt plugin factory exposing the Video Studio panel inside bubez_video.so.
 */
class VideoStudioPlugin : public QObject, public IVideoStudioPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IVideoStudioPlugin_iid)
    Q_INTERFACES(IVideoStudioPlugin)

public:
    explicit VideoStudioPlugin(QObject *parent = nullptr);
    ~VideoStudioPlugin() override = default;

    /** @copydoc IVideoStudioPlugin::pluginId */
    QString pluginId() const override;

    /** @copydoc IVideoStudioPlugin::displayName */
    QString displayName() const override;

    /** @copydoc IVideoStudioPlugin::description */
    QString description() const override;

    /** @copydoc IVideoStudioPlugin::createStudioWidget */
    IVideoStudioWidget *createStudioWidget(QWidget *parent = nullptr) override;
};
