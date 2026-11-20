/**
 * @file folder_plugin.h
 * @brief Qt plugin entry point for the modular Folder Studio engine (bubez_folder.so).
 */

#pragma once

#include "core/interfaces/i_folder_studio_plugin.h"
#include <QObject>

/**
 * @class FolderStudioPlugin
 * @brief Qt plugin implementation exporting FolderStudioWidget via IFolderStudioPlugin.
 */
class FolderStudioPlugin : public QObject, public IFolderStudioPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IFolderStudioPlugin_iid)
    Q_INTERFACES(IFolderStudioPlugin)

public:
    explicit FolderStudioPlugin(QObject *parent = nullptr);
    ~FolderStudioPlugin() override = default;

    /** @copydoc IFolderStudioPlugin::pluginId */
    QString pluginId() const override { return QStringLiteral("folder"); }

    /** @copydoc IFolderStudioPlugin::displayName */
    QString displayName() const override { return tr("Folder Studio Engine"); }

    /** @copydoc IFolderStudioPlugin::description */
    QString description() const override {
        return tr("Multi-format duplicate finder, PhotoRec/TestDisk recovery analyzer, smart tree organizer, and batch rename/cleanup studio.");
    }

    /** @copydoc IFolderStudioPlugin::createStudioWidget */
    IFolderStudioWidget *createStudioWidget(const QString &folderPath, QWidget *parent = nullptr) override;
};
