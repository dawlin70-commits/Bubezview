/**
 * @file viewer_plugin_interface.h
 * @brief Qt plugin interface for modular viewer engines in BubezView.
 *
 * Implemented by standalone shared libraries (.so) residing in the plugins directory.
 */

#pragma once

#include <QtPlugin>
#include <QString>
#include <QStringList>
#include <QWidget>
#include "i_viewer_widget.h"
#include "plugin_manager.h"

/**
 * @class ViewerPluginInterface
 * @brief Interface implemented by BubezView engine plugins.
 */
class ViewerPluginInterface {
public:
    virtual ~ViewerPluginInterface() = default;

    /**
     * @brief Unique identifier of the engine (e.g. "pdf", "archive", "media").
     * @return String ID.
     */
    virtual QString pluginId() const = 0;

    /**
     * @brief Human-readable name for UI presentation.
     * @return Display name string.
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Detailed summary of engine capabilities.
     * @return Description string.
     */
    virtual QString description() const = 0;

    /**
     * @brief Category of files handled by this plugin.
     * @return FileCategory enum value.
     */
    virtual FileCategory category() const = 0;

    /**
     * @brief File extensions handled natively by this plugin (e.g. {"pdf", "ai"}).
     * @return List of lowercase extensions without leading dot.
     */
    virtual QStringList supportedExtensions() const = 0;

    /**
     * @brief Instantiates the viewer widget implementing IViewerWidget.
     * @param parent Parent widget container.
     * @return Newly created viewer widget instance.
     */
    virtual IViewerWidget *createViewer(QWidget *parent = nullptr) = 0;
};

#define ViewerPluginInterface_iid "org.bubezview.ViewerPluginInterface/1.0"
Q_DECLARE_INTERFACE(ViewerPluginInterface, ViewerPluginInterface_iid)
