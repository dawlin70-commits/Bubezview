#pragma once

#include <QString>
#include <QIcon>
#include <QKeySequence>
#include <functional>
#include <QList>
#include "plugin_manager.h"

class BaseSkin;

/**
 * @brief Describes a contextual UI action item (button, menu entry, or toolbar item)
 * with its display label, icon, shortcut, group category, and execution callback.
 */
struct UIActionItem {
    QString id;                      ///< Unique identifier for the action (e.g. "rotate_right", "delete_file")
    QString label;                   ///< Localized human-readable label shown in tooltips/buttons
    QString iconName;                ///< System theme icon name (e.g. "object-rotate-right")
    QKeySequence shortcut;          ///< Primary keyboard shortcut triggering this action
    std::function<void()> trigger;   ///< Lambda or functor invoked when the action is triggered
    std::function<bool()> isEnabled; ///< Optional predicate evaluating whether the action is enabled
    std::function<bool()> isChecked; ///< Optional predicate evaluating toggle state
    bool isCheckable = false;        ///< Whether the action represents a toggleable button
    QString category;                ///< Action group category: "nav", "edit", "view", "tools", "file", "app"
};

/**
 * @brief Central registry that dynamically determines which toolbar actions to display
 * based on the active file category (Images, PDFs, Text, Office, Folders, etc.).
 */
class ActionRegistry {
public:
    /**
     * @brief Generates the list of contextual actions appropriate for a given file category.
     * @param category The active FileCategory of the opened file.
     * @param skin Pointer to the BaseSkin window executing the actions.
     * @return Ordered list of UIActionItem instances to be placed on toolbars or menus.
     */
    static QList<UIActionItem> getActionsForCategory(FileCategory category, BaseSkin *skin);
};
