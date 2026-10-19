#pragma once

#include <QString>

/**
 * @brief Manages integration with Linux desktop environments (KDE, GNOME, Cinnamon, XFCE).
 *
 * Configures system file manager context menus (Dolphin service menus, Nautilus scripts,
 * Nemo actions), and registers the main BubezView .desktop application launcher entry.
 */
class DesktopIntegrator {
public:
    enum class DesktopEnv {
        KDE,      ///< KDE Plasma desktop environment
        GNOME,    ///< GNOME Shell
        Cinnamon, ///< Cinnamon Desktop
        XFCE,     ///< XFCE Desktop
        Other     ///< Generic / other desktop environment
    };

    /**
     * @brief Detects active desktop environment from XDG_CURRENT_DESKTOP and DESKTOP_SESSION.
     */
    static DesktopEnv currentDesktop();

    /**
     * @brief Formats human-readable name of active desktop environment.
     */
    static QString desktopName();

    // Dolphin / KDE Service Menus
    static bool isDolphinEnabled();
    static QString currentDolphinShortcut();
    static bool enableDolphin(const QString &shortcut = "Space");
    static bool disableDolphin();

    // GNOME (Nautilus) & Cinnamon (Nemo) Action Scripts
    static bool isGnomeNemoEnabled();
    static bool enableGnomeNemo();
    static bool disableGnomeNemo();

    // Context Menu (Right Click associations)
    static bool isContextMenuEnabled();
    static bool enableContextMenu();
    static bool disableContextMenu();

    // System Application Launcher (.desktop in ~/.local/share/applications)
    static bool isAppMenuInstalled();
    static bool installAppMenu();
    static bool uninstallAppMenu();
};
