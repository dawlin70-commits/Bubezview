/**
 * @file archive_plugin.cpp
 * @brief Implementation of ArchivePlugin creating native ArchiveViewer instances.
 */

#include "archive_plugin.h"
#include "viewers/archive/archive_viewer.h"

ArchivePlugin::ArchivePlugin(QObject *parent)
    : QObject(parent)
{
}

IViewerWidget *ArchivePlugin::createViewer(QWidget *parent)
{
    return new ArchiveViewer(parent);
}
