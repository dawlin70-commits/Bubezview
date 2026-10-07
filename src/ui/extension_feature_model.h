/**
 * @file extension_feature_model.h
 * @brief Data structures and registry representing system extensions and their enabled features in BubezView.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QObject>

/**
 * @struct SubDependency
 * @brief Optional or supplementary package (e.g. language pack, auxiliary font data).
 */
struct SubDependency {
    QString name;               ///< Display name of the sub-dependency
    QString systemPackage;      ///< Package manager identifier (e.g. "tesseract-ocr-ita")
    bool isInstalled;           ///< True if present on host
    QString hint;               ///< Explanation of capability gained if installed
};

/**
 * @struct ExtensionFeature
 * @brief Represents an individual feature or capability enabled in BubezView by an extension.
 */
struct ExtensionFeature {
    QString title;              ///< Feature title
    QString description;        ///< Clear description of user benefit
    QString codeHook;           ///< Underlying C++ class or method hook
    QString menuLocation;       ///< User interface location / shortcut
    bool isCore;                ///< True if fundamental capability, false if specialized
};

/**
 * @struct ExtensionModuleInfo
 * @brief Complete metadata and feature set for a modular extension or external utility.
 */
struct ExtensionModuleInfo {
    QString id;                         ///< Unique key ("pdfimages", "qpdf", "tesseract", etc.)
    QString displayName;                ///< User-friendly display name
    QString systemPackage;              ///< Primary distro package name
    QString category;                   ///< Engine category ("PDF", "Archive", "Office", "Media")
    QString summary;                    ///< Brief overview of extension scope
    bool isInstalled;                   ///< Runtime presence on the host
    bool isEngine = false;              ///< True if this is a primary Core Engine, false if extension
    bool canCompile;                    ///< True if compilable/optimizable from source
    bool isStaticCompiled;              ///< True if user chose static/source compilation
    QString diagnosticCommand;          ///< Command used to verify installation and get version string
    QList<SubDependency> subDependencies; ///< Related secondary packages (e.g. language dictionaries)
    QList<ExtensionFeature> features;   ///< Exact list of features enabled in codebase
};

/**
 * @class ExtensionFeatureRegistry
 * @brief Central query provider for extension metadata, feature lists, and dynamic status.
 */
class ExtensionFeatureRegistry : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Retrieves all registered extensions with current runtime installation status.
     * @return List of ExtensionModuleInfo records.
     */
    static QList<ExtensionModuleInfo> allModules();

    /**
     * @brief Retrieves all extensions that belong to a specific engine category.
     * @param category Category filter (e.g. "PDF", "Archive", "Office").
     * @return Filtered list of ExtensionModuleInfo records.
     */
    static QList<ExtensionModuleInfo> modulesForCategory(const QString &category);

    /**
     * @brief Finds a single extension by its unique identifier.
     * @param id Extension identifier.
     * @return ExtensionModuleInfo record or default empty struct if not found.
     */
    static ExtensionModuleInfo moduleById(const QString &id);

    /**
     * @brief Refreshes dynamic runtime status (installed/uninstalled) for a given module.
     * @param module In-out reference to update.
     */
    static void refreshModuleStatus(ExtensionModuleInfo &module);
};
