#pragma once

#include <QString>
#include <QList>
#include <QIcon>

/**
 * @brief Represents metadata of an installed desktop application parsed from a FreeDesktop .desktop file.
 */
struct AppInfo {
    QString id;          ///< Unique desktop file ID (e.g. "gimp.desktop" or "org.kde.gwenview.desktop")
    QString name;        ///< Localized display name (e.g. "GIMP" or "Gwenview")
    QString genericName; ///< Localized generic application category (e.g. "Image Editor", "Web Browser")
    QString comment;     ///< Localized summary/tooltip description of the application
    QString iconName;    ///< Theme icon name or absolute filesystem path to the application icon
    QString execCmd;     ///< Executable command line containing FreeDesktop format specifiers
    QString desktopPath; ///< Absolute path to the underlying .desktop file on disk
    QStringList mimeTypes; ///< Supported MIME types parsed from the desktop MimeType field

    /**
     * @brief Resolves and returns the application's QIcon.
     * Searches the active desktop theme first, then falls back to direct icon file path,
     * and defaults to "application-x-executable" if not found.
     * @return Resolved QIcon.
     */
    QIcon icon() const;
};

/**
 * @brief Utilities for discovering, querying MIME associations, and launching desktop applications.
 *
 * Implements FreeDesktop Desktop Entry and Shared MIME Info specifications.
 * Queries system directories (/usr/share/applications, ~/.local/share/applications)
 * and resolves mimeapps.list to determine default and recommended programs for files.
 */
class AppLauncher
{
public:
    /**
     * @brief Retrieves applications explicitly associated with the MIME type of a given file.
     * Looks up system and user mimeapps.list, as well as MimeType definitions in desktop files.
     * @param filePath Absolute path of the target file.
     * @return List of AppInfo records matching the file's MIME type.
     */
    static QList<AppInfo> getAssociatedApps(const QString &filePath);

    /**
     * @brief Discovers and returns all visible, launchable desktop applications on the system.
     * Filters out NoDisplay=true, Hidden=true, and Non-Application entries.
     * @return List of all installed AppInfo records sorted alphabetically by name.
     */
    static QList<AppInfo> getAllApps();

    /**
     * @brief Launches an application detached with the specified target file path.
     * Automatically handles FreeDesktop Exec field codes (%f, %F, %u, %U) by substituting
     * file paths and local file URLs appropriately, and stripping unsupported flags.
     * @param app Target application metadata.
     * @param filePath Absolute path to the file to open.
     * @return True if the process was started successfully; false otherwise.
     */
    static bool launchApp(const AppInfo &app, const QString &filePath);

    /**
     * @brief Parses a single FreeDesktop .desktop file into an AppInfo structure.
     * Respects the current system locale for localized Name, GenericName, and Comment keys.
     * @param desktopPath Absolute path to the .desktop file.
     * @param outApp Output AppInfo record populated on success.
     * @return True if the file is a valid, displayable desktop application entry; false otherwise.
     */
    static bool parseDesktopFile(const QString &desktopPath, AppInfo &outApp);
};
