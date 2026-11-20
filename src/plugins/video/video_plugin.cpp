/**
 * @file video_plugin.cpp
 * @brief Implementation of the modular Video Studio Qt plugin (bubez_video.so).
 */

#include "video_plugin.h"
#include "video_studio_widget.h"

VideoStudioPlugin::VideoStudioPlugin(QObject *parent)
    : QObject(parent)
{
}

QString VideoStudioPlugin::pluginId() const
{
    return QStringLiteral("video");
}

QString VideoStudioPlugin::displayName() const
{
    return tr("Video Studio");
}

QString VideoStudioPlugin::description() const
{
    return tr("Modular video editing studio: lossless trim/cut, audio track extraction, frame snapshots, rotation/flip, resizing, mute, and speed.");
}

IVideoStudioWidget *VideoStudioPlugin::createStudioWidget(QWidget *parent)
{
    return new VideoStudioWidget(parent);
}
