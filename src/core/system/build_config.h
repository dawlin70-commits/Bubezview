/**
 * @file build_config.h
 * @brief Persistent build and compiler configuration manager for BubezView.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QObject>

/**
 * @struct CustomSourceConfig
 * @brief Holds user-defined Git source, tag, and build configuration for a component.
 */
struct CustomSourceConfig {
    QString preset = "stable";   ///< "stable", "git", or "custom"
    QString gitUrl;              ///< Git repository URL or mirror
    QString branchTag;           ///< Branch or git tag name
    QString buildFlags;          ///< Additional CMake or build arguments
};

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
     * @brief Gets custom source configuration for a given component.
     * @param componentId Component ID (e.g. "tesseract", "qpdf", "poppler")
     */
    CustomSourceConfig customSourceConfig(const QString &componentId) const;
    void setCustomSourceConfig(const QString &componentId, const CustomSourceConfig &config);

    /**
     * @brief Returns default source configuration for a known component.
     * @param componentId Component ID.
     */
    static CustomSourceConfig defaultSourceConfig(const QString &componentId);

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
    static void clearPackageCache();
    static bool isExecutableAvailable(const QString &exeName);
    static bool isQpdfAvailable();
    static QString qpdfExecutable();
    static bool isTesseractAvailable();
    static QString pdfimagesExecutable();
    static bool isPdfimagesAvailable();
    static QString pdfuniteExecutable();
    static bool isPdfuniteAvailable();
    static QString sevenZipExecutable();
    static bool isSevenZipAvailable();
    static bool isPopplerDataInstalled();

    /**
     * @brief Dynamically discovers the project development root directory (if running in dev mode).
     * Inspects BUBEZVIEW_DEV_ROOT or ascends from applicationDirPath() looking for project markers.
     * @return Absolute path to project root, or empty QString if running from a standard system install.
     */
    static QString projectDevRoot();
    
    /**
     * @enum HardwareProfile
     * @brief High-level hardware profiles governing source builds vs packaged libraries.
     */
    enum class HardwareProfile {
        HighPerformance, ///< Potente / Desktop: Everything compiled from source with extreme optimization
        Balanced,        ///< Bilanciato: Everything from system precompiled packages
        Custom           ///< Manual fine-grained override via Compiler Wizard
    };

    /**
     * @brief Applies hardware profile, updating compiler flags, execution modes, and runtime cache budgets.
     */
    void applyHardwareProfile(HardwareProfile profile);

    /**
     * @brief Retrieves the active hardware profile.
     */
    HardwareProfile activeHardwareProfile() const;

    /**
     * @enum ComponentSourceMode
     * @brief Indicates whether a component is running from OS system packages or local source builds.
     */
    enum class ComponentSourceMode {
        SystemBinary,   ///< Provided by distro package manager (/usr/bin)
        LocalSource     ///< Compiled and installed into ~/.local or local build tree
    };

    /**
     * @brief Checks if the OS system package / binary is present on host.
     * @param componentId Component identifier ("qpdf", "pdfimages", "tesseract", etc.)
     */
    static bool hasSystemBinary(const QString &componentId);

    /**
     * @brief Checks if a locally compiled build of this component exists in ~/.local/bin or build tree.
     * @param componentId Component identifier.
     */
    static bool hasSourceBinary(const QString &componentId);

    /**
     * @brief Checks if a component is available either via system package/binary or local source binary.
     * @param componentId Component identifier ("qpdf", "pdfimages", "tesseract", "poppler", etc.)
     * @return True if available on host.
     */
    static bool isComponentAvailable(const QString &componentId);

    /**
     * @brief Determines the active execution mode for a component.
     */
    ComponentSourceMode componentSourceMode(const QString &componentId) const;

    /**
     * @brief Sets and saves preferred execution source mode.
     */
    void setComponentSourceMode(const QString &componentId, ComponentSourceMode mode);

    /**
     * @brief Removes locally compiled files for component from ~/.local/bin and build cache.
     * @return True if successfully removed.
     */
    static bool removeLocalSourceBuild(const QString &componentId);

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
    QMap<QString, CustomSourceConfig> m_customSources;
};
