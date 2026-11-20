/**
 * @file image_plugin.h
 * @brief Qt plugin entry point for the modular Image Studio engine (bubez_image.so).
 */

#pragma once

#include "core/interfaces/i_image_studio_plugin.h"
#include <QObject>

/**
 * @class ImageStudioPlugin
 * @brief Concrete Qt plugin factory exposing the Image Studio panel inside bubez_image.so.
 */
class ImageStudioPlugin : public QObject, public IImageStudioPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IImageStudioPlugin_iid)
    Q_INTERFACES(IImageStudioPlugin)

public:
    explicit ImageStudioPlugin(QObject *parent = nullptr);
    ~ImageStudioPlugin() override = default;

    /** @copydoc IImageStudioPlugin::pluginId */
    QString pluginId() const override;

    /** @copydoc IImageStudioPlugin::displayName */
    QString displayName() const override;

    /** @copydoc IImageStudioPlugin::description */
    QString description() const override;

    /** @copydoc IImageStudioPlugin::createStudioWidget */
    IImageStudioWidget *createStudioWidget(QWidget *parent = nullptr) override;
};
