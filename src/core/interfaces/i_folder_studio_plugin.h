/**
 * @file i_folder_studio_plugin.h
 * @brief Qt plugin interface for the modular Folder Studio engine (bubez_folder.so).
 *
 * Keeps heavy multi-format folder analysis (exact & semantic deduplication,
 * PhotoRec/TestDisk recovery & magic-byte repair, smart tree organization,
 * and batch rename/cleanup) decoupled from the lightweight BubezView core.
 */

#pragma once

#include <QtPlugin>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

/**
 * @brief Active workspace tool inside Folder Studio Edit Mode.
 */
enum class FolderStudioTool {
    Duplicates    = 0, ///< Exact & multi-format equivalent finder (Documents, Archives, Audio/Video, Images)
    Recovery      = 1, ///< PhotoRec / TestDisk recovery, magic-byte repair, integrity check & smart naming
    TreeOrganizer = 2, ///< Smart folder tree organizer / flattener with dry-run preview & undo rollback
    BatchTools    = 3  ///< Smart batch rename & empty-dir / junk cleaner
};

/**
 * @class IFolderStudioWidget
 * @brief Abstract widget interface implemented by the Folder Studio UI inside bubez_folder.so.
 */
class IFolderStudioWidget : public QWidget {
public:
    using QWidget::QWidget;
    ~IFolderStudioWidget() override = default;

    /**
     * @brief Sets the target root directory to analyze and operate on.
     * @param folderPath Canonical directory path.
     */
    virtual void setTargetFolder(const QString &folderPath) = 0;

    /**
     * @brief Switches the visible studio workspace tab.
     * @param tool Target tool enum.
     */
    virtual void setActiveTool(FolderStudioTool tool) = 0;

    /**
     * @brief Synchronizes visual styling with the active BubezView theme preset.
     */
    virtual void updateTheme() = 0;

    /**
     * @brief Cancels any background scan worker safely before hiding or destroying.
     */
    virtual void cancelActiveOperations() = 0;

    /**
     * @brief Registers a callback invoked whenever files or folders are modified on disk.
     * @param cb Callback functor.
     */
    virtual void setOnFolderModified(std::function<void()> cb) = 0;

    /**
     * @brief Registers a callback invoked when the user requests previewing a specific file.
     * @param cb Callback taking the clicked file path and an ordered list of scoped context files
     *           (e.g. files in the same duplicate group or filtered result list) for Prev/Next navigation.
     */
    virtual void setOnPreviewFileRequested(std::function<void(const QString &, const QStringList &)> cb) = 0;

    /**
     * @brief Removes a file path from active in-memory Studio results after external deletion/trashing.
     * @param filePath Absolute path of the removed file.
     */
    virtual void notifyFileRemoved(const QString &filePath) = 0;
};

/**
 * @class IFolderStudioPlugin
 * @brief Qt plugin factory interface implemented by bubez_folder.so.
 */
class IFolderStudioPlugin {
public:
    virtual ~IFolderStudioPlugin() = default;

    /**
     * @brief Unique identifier of the plugin ("folder").
     * @return String plugin ID.
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
     * @brief Instantiates an embedded Folder Studio widget for a given directory.
     * @param folderPath Initial directory path.
     * @param parent Optional parent widget.
     * @return Newly created IFolderStudioWidget instance.
     */
    virtual IFolderStudioWidget *createStudioWidget(const QString &folderPath, QWidget *parent = nullptr) = 0;
};

#define IFolderStudioPlugin_iid "org.bubezview.IFolderStudioPlugin/1.0"
Q_DECLARE_INTERFACE(IFolderStudioPlugin, IFolderStudioPlugin_iid)
