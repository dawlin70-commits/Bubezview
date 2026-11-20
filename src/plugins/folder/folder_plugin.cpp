/**
 * @file folder_plugin.cpp
 * @brief Implementation of the modular Folder Studio Qt plugin (bubez_folder.so).
 */

#include "folder_plugin.h"
#include "folder_studio_widget.h"

FolderStudioPlugin::FolderStudioPlugin(QObject *parent)
    : QObject(parent)
{
}

IFolderStudioWidget *FolderStudioPlugin::createStudioWidget(const QString &folderPath, QWidget *parent)
{
    return new FolderStudioWidget(folderPath, parent);
}
