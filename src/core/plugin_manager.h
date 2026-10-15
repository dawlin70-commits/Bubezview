/**
 * @file plugin_manager.h
 * @brief High-level file categorization and dynamic viewer plugin discovery and lifecycle manager.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QObject>

class IViewerWidget;
class ViewerPluginInterface;
class QPluginLoader;

/**
 * @brief High-level file categories supported by BubezView.
 * Used to route files to the appropriate native viewer widget and determine toolbar actions.
 */
enum class FileCategory {
    Image,   ///< Raster/vector images (PNG, JPG, WebP, SVG, GIF, BMP, TIFF, ICO, etc.)
    Text,    ///< Plain text, source code, Markdown, XML, JSON, logs, configs
    Pdf,     ///< PDF documents
    Archive, ///< Compressed archives (ZIP, TAR, GZ, BZ2, XZ, 7Z, ISO)
    Media,   ///< Video and audio files (MP4, MKV, AVI, MP3, WAV, FLAC, OGG, etc.)
    Office,  ///< Office documents (DOCX, ODT, ODS, XLSX, RTF, EPUB, PPTX)
    Folder,  ///< Filesystem directories (gallery/thumbnail grid view)
    Unknown  ///< Unrecognized or unsupported file type
};

/**
 * @struct PluginDescriptor
 * @brief Holds metadata and runtime state for a viewer plugin.
 */
struct PluginDescriptor {
    QString id;
    QString filePath;
    QString displayName;
    QString description;
    FileCategory category;
    QStringList extensions;
    bool isLoaded = false;
    ViewerPluginInterface *interfaceInstance = nullptr;
    QPluginLoader *loader = nullptr;
};

/**
 * @class PluginManager
 * @brief Manages file format categorization and viewer plugin discovery.
 */
class PluginManager : public QObject {
    Q_OBJECT

public:
    static PluginManager &instance();

    /**
     * @brief Categorizes a file path into a FileCategory enum based on extension and MIME type.
     * @param filePath Absolute path or filename to categorize.
     * @return Resolved FileCategory.
     */
    static FileCategory categorizeFile(const QString &filePath);

    /**
     * @brief Checks if a file has native in-app viewing support in BubezView.
     * @param filePath Target file path.
     * @return True if supported by an internal or active plugin viewer; false otherwise.
     */
    static bool isNativeSupported(const QString &filePath);

    /**
     * @brief Scans system and user plugin directories to locate and load valid viewer plugins (.so).
     */
    void discoverPlugins();

    /**
     * @brief Checks whether a viewer plugin is loaded and ready for a given category.
     * @param category Target file category.
     * @return True if engine is available.
     */
    bool isEngineAvailable(FileCategory category) const;

    /**
     * @brief Instantiates a viewer widget for the given category using the loaded plugin.
     * @param category Target file category.
     * @param parent Parent widget.
     * @return Newly created IViewerWidget or nullptr if engine is unavailable.
     */
    IViewerWidget *createViewer(FileCategory category, QWidget *parent = nullptr);

    /**
     * @brief Manually registers a built-in or statically linked plugin instance.
     * @param plugin Pointer to interface implementation.
     */
    void registerPlugin(ViewerPluginInterface *plugin);

    /**
     * @brief Returns descriptors for all discovered plugins.
     * @return List of PluginDescriptor records.
     */
    QList<PluginDescriptor> discoveredPlugins() const;

    /**
     * @brief Queries distro package name for a plugin's core dependency.
     * @param pluginId Engine identifier ("pdf", "archive", "media").
     * @return Package name string.
     */
    QString pluginInstallPackage(const QString &pluginId) const;

    /**
     * @brief Renders a thumbnail of the first page of a PDF using FreeDesktop cache or pdftoppm.
     * @param filePath Path to the PDF file.
     * @param maxDimension Maximum dimension in pixels.
     * @return Rendered thumbnail QImage, or null QImage if unavailable.
     */
    static QImage renderPdfThumbnail(const QString &filePath, int maxDimension);

signals:
    /**
     * @brief Emitted whenever the set of loaded plugins changes.
     */
    void pluginsChanged();

private:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    void loadPluginFromFile(const QString &filePath);

    static const QStringList s_imageExtensions;
    static const QStringList s_textExtensions;
    static const QStringList s_pdfExtensions;
    static const QStringList s_archiveExtensions;
    static const QStringList s_mediaExtensions;
    static const QStringList s_officeExtensions;

    QMap<QString, PluginDescriptor> m_plugins;
};
