/**
 * @file package_manager_helper.h
 * @brief Intelligent operating system detection and package manager integration.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

enum class DistroFamily {
    Debian,     ///< Ubuntu, Debian, Linux Mint, Pop!_OS, Zorin
    Arch,       ///< Arch Linux, Manjaro, EndeavourOS, Garuda, CachyOS
    Fedora,     ///< Fedora, RHEL, CentOS, Rocky, Alma, Nobara
    OpenSuse,   ///< openSUSE Tumbleweed, Leap
    Void,       ///< Void Linux
    Solus,      ///< Solus
    Alpine,     ///< Alpine Linux
    Gentoo,     ///< Gentoo Linux
    Immutable,  ///< SteamOS, Fedora Silverblue/Kinoite, openSUSE MicroOS, NixOS
    Unknown
};

class PackageManagerHelper {
public:
    /**
     * @brief Detects current Linux distribution family from /etc/os-release and system indicators.
     */
    static DistroFamily detectFamily();

    /**
     * @brief Returns a human-friendly string for the detected distribution (e.g. "Debian / Ubuntu").
     */
    static QString distroDisplayName();

    /**
     * @brief Checks if the operating system is immutable (OSTree, NixOS, SteamOS).
     */
    static bool isImmutableSystem();

    /**
     * @brief Checks if the operating system is Gentoo.
     */
    static bool isGentoo();

    /**
     * @brief Maps generic component/library ID to the exact package name for the active distribution.
     * @param componentId e.g. "pdfimages", "qpdf", "tesseract", "poppler-qt6", "libarchive", "librevenge", "openssl"
     */
    static QString packageNameForComponent(const QString &componentId);

    /**
     * @brief Formats the suggested terminal command for installing a component.
     */
    static QString getInstallCommand(const QString &componentId);

    /**
     * @brief Checks if a component can be built as a native compiled engine module.
     * @param componentId Generic component ID
     * @return True if source compilation is supported
     */
    static bool isCompilableEngine(const QString &componentId);

    /**
     * @brief Configures BuildConfig and launches the compilation console for an engine module.
     * @param componentId Generic component ID
     * @param parentWidget Parent widget for dialogs
     * @return True if compilation was initiated successfully
     */
    static bool triggerEngineCompilation(const QString &componentId, QWidget *parentWidget = nullptr);

    /**
     * @brief Prompts user and installs the specified component using pkexec or native package manager.
     * @param componentId Generic component ID
     * @param parentWidget Parent widget for dialogs
     * @return True if installation completed successfully
     */
    static bool installComponentInteractive(const QString &componentId, QWidget *parentWidget = nullptr);

    /**
     * @brief Runs an interactive package manager command with graphical privilege escalation (pkexec).
     * @param packageNames List of package names to install
     * @param parentWidget Parent widget for dialogs
     * @param outError Optional pointer to store error message on failure
     * @return True if command finished with exit code 0
     */
    static bool runPrivilegedInstall(const QStringList &packageNames, QWidget *parentWidget = nullptr, QString *outError = nullptr);

    /**
     * @brief Checks if a component is installed on the host system.
     */
    static bool isComponentInstalled(const QString &componentId);

    /**
     * @brief Prompts user and uninstalls the specified component safely.
     * @param componentId Generic component ID
     * @param parentWidget Parent widget for dialogs
     * @return True if uninstallation completed successfully
     */
    static bool uninstallComponentInteractive(const QString &componentId, QWidget *parentWidget = nullptr);

    /**
     * @brief Formats the suggested terminal command for uninstalling a component.
     */
    static QString getUninstallCommand(const QString &componentId);

    /**
     * @brief Verifies whether uninstalling a component would cascade and remove other system packages.
     * @param componentId Generic component ID or package name
     * @param outOtherAffectedPackages Populated with names of other packages that would be removed
     * @return True if safe (only the target package would be removed); false if other packages would be affected
     */
    static bool checkUninstallSafety(const QString &componentId, QStringList &outOtherAffectedPackages);

    /**
     * @brief Runs an interactive package removal command with privilege escalation.
     */
    static bool runPrivilegedUninstall(const QStringList &packageNames, QWidget *parentWidget = nullptr);

    /**
     * @brief Queries the system package manager database to discover candidate/upstream version of a package.
     * @param componentId Generic component ID, e.g. "poppler-qt6"
     * @return Version string (e.g. "22.12.0", "26.08.0") or empty string if not found or query fails.
     */
    static QString queryPackageCandidateVersion(const QString &componentId);

    /**
     * @brief Returns a curated list of known stable releases from recent years for a component.
     * @param componentId Generic component ID, e.g. "poppler-qt6"
     * @return List of version strings (e.g. "24.08.0", "24.02.0", "23.12.0", "22.12.0")
     */
    static QStringList knownStableVersions(const QString &componentId);
};
