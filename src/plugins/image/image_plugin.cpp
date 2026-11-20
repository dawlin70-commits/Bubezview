/**
 * @file image_plugin.cpp
 * @brief Implementation of the modular Image Studio Qt plugin (bubez_image.so).
 */

#include "image_plugin.h"
#include "image_studio_widget.h"

ImageStudioPlugin::ImageStudioPlugin(QObject *parent)
    : QObject(parent)
{
}

QString ImageStudioPlugin::pluginId() const
{
    return QStringLiteral("image");
}

QString ImageStudioPlugin::displayName() const
{
    return tr("Image Studio");
}

QString ImageStudioPlugin::description() const
{
    return tr("Modular photo editing studio: auto-enhance, color & light adjustments, cinematic presets, sharpen/blur/vignette, and geometry tools.");
}

IImageStudioWidget *ImageStudioPlugin::createStudioWidget(QWidget *parent)
{
    return new ImageStudioWidget(parent);
}
