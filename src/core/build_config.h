/**
 * @file build_config.h
 * @brief Persistent build and compiler configuration manager for BubezView.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QObject>

struct ExtensionInfo {
    QString id;                 ///< Unique identifier ("pdfimages", "qpdf", "ocr", "openssl", "zstd", etc.)
    QString name;               ///< Display name
    QString description;        ///< Clear explanation of added capabilities
    bool isInstalled;           ///< True if present on host system
    QString installPackage;     ///< Suggested package name (e.g. "poppler-utils", "qpdf")
};

/**
 * @struct ModuleInfo
 * @brief Metadata and system dependency state for a BubezView functional engine.
 */
struct ModuleInfo {
    QString id;                 ///< Unique identifier ("pdf", "archive", "media", "office", "qr")
    QString name;               ///< Display name
    QString description;        ///< Clear human-readable description of functionality
    bool isCore;                ///< True if mandatory viewer; false if optional
    bool isEnabled;             ///< Active in compilation
    bool isStatic;              ///< True = Built-in / Compiled from source; False = System Library
    QString requiredPkg;        ///< pkg-config package name (e.g. "poppler-qt6")
    bool pkgAvailable;          ///< True if present on host system
    QList<ExtensionInfo> extensions; ///< Sub-extensions and utilities associated with this module
};

/**
 * @class BuildConfig
 * @brief Manages persistent compilation settings, CPU profiles, and module selections.
 * Stored at ~/.local/share/bubezview/build_config.json.
 */
class BuildConfig {
public:
    static BuildConfig &instance();

    /**
     * @brief Loads build settings from disk.
     * @return True if file was loaded successfully or initialized with defaults.
     */
    bool load();

    /**
     * @brief Saves current build settings to disk.
     * @return True if saved successfully.
     */
    bool save();

    /**
     * @brief Returns standard path to configuration file.
     */
    QString configFilePath() const;

    // Optimization Level (-O2 vs -O3)
    QString optimizationLevel() const { return m_optimizationLevel; }
    void setOptimizationLevel(const QString &level) { m_optimizationLevel = level; }
    bool isO3() const { return m_optimizationLevel == "O3"; }

    // CPU Tuning Profile
    QString cpuProfile() const { return m_cpuProfile; }
    void setCpuProfile(const QString &profile) { m_cpuProfile = profile; }
    bool isNativeCpu() const { return m_cpuProfile == "native"; }

    // Library strategy
    QString libraryStrategy() const { return m_libraryStrategy; }
    void setLibraryStrategy(const QString &strategy) { m_libraryStrategy = strategy; }

    // Granular Module Toggles
    bool staticPdf() const { return m_staticPdf; }
    void setStaticPdf(bool val) { m_staticPdf = val; }

    QString popplerVersion() const { return m_popplerVersion; }
    void setPopplerVersion(const QString &ver) { m_popplerVersion = ver; }

    bool staticArchive() const { return m_staticArchive; }
    void setStaticArchive(bool val) { m_staticArchive = val; }

    bool enableInternalOffice() const { return m_enableInternalOffice; }
    void setEnableInternalOffice(bool val) { m_enableInternalOffice = val; }

    bool staticOffice() const { return m_staticOffice; }
    void setStaticOffice(bool val) { m_staticOffice = val; }

    // Custom Compiler Wizard Flags
    QString customCompilerFlags() const { return m_customCompilerFlags; }
    void setCustomCompilerFlags(const QString &flags) { m_customCompilerFlags = flags; }

    /**
     * @brief Formats current settings into CMake command line definitions.
     * @return List of arguments (e.g. ["-DOPTIMIZE_FOR_NATIVE_CPU=ON", ...])
     */
    QStringList toCMakeArgs() const;

    /**
     * @brief Queries system package manager and pkg-config for library availability.
     * @param pkgName Package identifier (e.g. "poppler-qt6").
     * @return True if package was detected on host.
     */
    static bool isPkgInstalled(const QString &pkgName);
    static bool isExecutableAvailable(const QString &exeName);
    static bool isQpdfAvailable();
    static bool isTesseractAvailable();
    static bool isPdfimagesAvailable();
    static bool isPopplerDataInstalled();

    static int pdfMergeCount();
    static void incrementPdfMergeCount();
    static bool isQpdfTipDismissed();
    static void dismissQpdfTip();

    static bool showUninstalledFeatures();
    static void setShowUninstalledFeatures(bool show);
    static bool showUnsupportedFiles();
    static void setShowUnsupportedFiles(bool show);

    /**
     * @brief Returns a structured list of all supported engines and their status.
     */
    QList<ModuleInfo> availableModules() const;

private:
    BuildConfig();
    ~BuildConfig() = default;
    BuildConfig(const BuildConfig &) = delete;
    BuildConfig &operator=(const BuildConfig &) = delete;

    QString m_optimizationLevel;    ///< "O2" (balanced / battery) or "O3" (aggressive / heavy compute)
    QString m_cpuProfile;           ///< "native" or "standard"
    QString m_libraryStrategy;      ///< "dynamic", "static", or "custom"
    QString m_customCompilerFlags;  ///< Fine-grained custom compiler flags from Compiler Wizard
    bool m_staticPdf;
    QString m_popplerVersion;
    bool m_staticArchive;
    bool m_enableInternalOffice;
    bool m_staticOffice;
};
