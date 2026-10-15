/**
 * @file media_plugin.cpp
 * @brief Implementation of MediaPlugin creating native MediaViewer instances.
 */

#include "media_plugin.h"
#include "viewers/media/media_viewer.h"

MediaPlugin::MediaPlugin(QObject *parent)
    : QObject(parent)
{
}

IViewerWidget *MediaPlugin::createViewer(QWidget *parent)
{
    return new MediaViewer(parent);
}
