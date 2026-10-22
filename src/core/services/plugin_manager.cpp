/**
 * @file plugin_manager.cpp
 * @brief Implementation of PluginManager dynamic plugin discovery and format categorization.
 */

#include "plugin_manager.h"
#include "interfaces/viewer_plugin_interface.h"
#include "package_manager_helper.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPluginLoader>
#include <QStandardPaths>
#include <QDebug>
#include <QCryptographicHash>
#include <QUrl>
#include <QTemporaryDir>
#include <QProcess>
#include <QImage>

const QStringList PluginManager::s_imageExtensions = {
    // Standard & Web Formats
    "png", "jpg", "jpeg", "webp", "svg", "bmp", "gif", "ico", "tiff", "tif",
    "avif", "heic", "heif", "pbm", "pgm", "ppm", "xbm", "xpm",
    // Digital Camera RAW Formats
    "raw", "dng", "cr2", "cr3", "nef", "nrw", "arw", "srf", "sr2",
    "raf", "orf", "rw2", "pef", "ptx", "kdc", "dcr",
    // Next-Gen, Graphics & Production Formats
    "jxl", "psd", "xcf", "hdr", "exr", "tga", "dds", "qoi", "cur", "svgz"
};

const QStringList PluginManager::s_textExtensions = {
    "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
    "json", "xml", "html", "htm", "css", "js", "ts", "sh", "bash", "zsh",
    "yaml", "yml", "toml", "ini", "conf", "cfg", "log", "desktop", "service",
    "qml", "qrc", "ui", "cmake", "diff", "patch", "csv", "tsv"
};

const QStringList PluginManager::s_pdfExtensions = {
    "pdf", "p7m"
};

const QStringList PluginManager::s_archiveExtensions = {
    "zip", "tar", "gz", "tgz", "bz2", "tbz2", "xz", "txz", "zst", "tzst", "7z", "iso", "deb", "rpm", "cpio"
};

const QStringList PluginManager::s_mediaExtensions = {
    "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v", "3gp",
    "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus"
};

const QStringList PluginManager::s_officeExtensions = {
    "docx", "doc", "xlsx", "xls", "xltx", "xlsm", "pptx", "ppt", "odt", "ods", "odp", "epub", "rtf"
};

PluginManager &PluginManager::instance()
{
    static PluginManager s_instance;
    return s_instance;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it->loader) {
            it->loader->unload();
            delete it->loader;
            it->loader = nullptr;
        }
    }
}

FileCategory PluginManager::categorizeFile(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) {
        return FileCategory::Unknown;
    }
    if (info.isDir()) {
        return FileCategory::Folder;
    }

    QString ext = info.suffix().toLower();

    if (s_imageExtensions.contains(ext)) {
        return FileCategory::Image;
    }
    if (s_textExtensions.contains(ext)) {
        return FileCategory::Text;
    }
    if (s_pdfExtensions.contains(ext)) {
        return FileCategory::Pdf;
    }
    if (s_archiveExtensions.contains(ext)) {
        return FileCategory::Archive;
    }
    if (s_mediaExtensions.contains(ext)) {
        return FileCategory::Media;
    }
    if (s_officeExtensions.contains(ext)) {
        return FileCategory::Office;
    }
    
    // Check if the file is binary or plain text
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray sample = file.read(1024);
        file.close();
        if (sample.contains('\0')) {
            return FileCategory::Unknown;
        }
        // Small readable text file without null bytes can be treated as text
        if (info.size() < 5 * 1024 * 1024) {
            return FileCategory::Text;
        }
    }

    return FileCategory::Unknown;
}

bool PluginManager::isNativeSupported(const QString &filePath)
{
    FileCategory cat = categorizeFile(filePath);
    if (cat == FileCategory::Unknown) return false;
    if (cat == FileCategory::Image || cat == FileCategory::Text || cat == FileCategory::Folder || cat == FileCategory::Office || cat == FileCategory::Media) {
        return true;
    }
    // For modular categories (Pdf, Archive), check if engine plugin is available
    return instance().isEngineAvailable(cat);
}

void PluginManager::discoverPlugins()
{
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath() + "/plugins",
        QCoreApplication::applicationDirPath() + "/../plugins",
        QDir::currentPath() + "/plugins",
        QDir::currentPath() + "/build_deb/plugins",
        QDir::homePath() + "/.local/lib/bubezview/plugins",
        "/usr/local/lib/bubezview/plugins",
        "/usr/lib/bubezview/plugins"
    };

    QSet<QString> loadedFileNames;
    for (const QString &dirPath : searchDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        QStringList entries = dir.entryList({"*.so", "libbubez_*.so", "bubez_*.so"}, QDir::Files | QDir::Readable);
        for (const QString &fileName : entries) {
            if (loadedFileNames.contains(fileName)) {
                continue;
            }
            QString fullPath = dir.absoluteFilePath(fileName);
            loadPluginFromFile(fullPath);
            loadedFileNames.insert(fileName);
        }
    }

    emit pluginsChanged();
}

void PluginManager::loadPluginFromFile(const QString &filePath)
{
    QPluginLoader *loader = new QPluginLoader(filePath, this);
    QObject *instance = loader->instance();
    if (!instance) {
        delete loader;
        return;
    }

    auto *plugin = qobject_cast<ViewerPluginInterface*>(instance);
    if (!plugin) {
        // Try interface cast if qobject_cast not supported
        plugin = dynamic_cast<ViewerPluginInterface*>(instance);
    }

    if (!plugin) {
        loader->unload();
        delete loader;
        return;
    }

    if (m_plugins.contains(plugin->pluginId())) {
        loader->unload();
        delete loader;
        return;
    }

    PluginDescriptor desc;
    desc.id = plugin->pluginId();
    desc.filePath = filePath;
    desc.displayName = plugin->displayName();
    desc.description = plugin->description();
    desc.category = plugin->category();
    desc.extensions = plugin->supportedExtensions();
    desc.isLoaded = true;
    desc.interfaceInstance = plugin;
    desc.loader = loader;

    m_plugins[desc.id] = desc;
}

void PluginManager::registerPlugin(ViewerPluginInterface *plugin)
{
    if (!plugin) return;

    PluginDescriptor desc;
    desc.id = plugin->pluginId();
    desc.filePath = "builtin";
    desc.displayName = plugin->displayName();
    desc.description = plugin->description();
    desc.category = plugin->category();
    desc.extensions = plugin->supportedExtensions();
    desc.isLoaded = true;
    desc.interfaceInstance = plugin;
    desc.loader = nullptr;

    m_plugins[desc.id] = desc;
    emit pluginsChanged();
}

bool PluginManager::isEngineAvailable(FileCategory category) const
{
    for (const auto &desc : m_plugins) {
        if (desc.category == category && desc.isLoaded && desc.interfaceInstance) {
            return true;
        }
    }
    return false;
}

IViewerWidget *PluginManager::createViewer(FileCategory category, QWidget *parent)
{
    for (const auto &desc : m_plugins) {
        if (desc.category == category && desc.isLoaded && desc.interfaceInstance) {
            return desc.interfaceInstance->createViewer(parent);
        }
    }
    return nullptr;
}

QList<PluginDescriptor> PluginManager::discoveredPlugins() const
{
    return m_plugins.values();
}

QString PluginManager::pluginInstallPackage(const QString &pluginId) const
{
    if (pluginId == "pdf") {
        return PackageManagerHelper::packageNameForComponent("poppler-qt6");
    } else if (pluginId == "archive") {
        return PackageManagerHelper::packageNameForComponent("libarchive");
    } else if (pluginId == "media") {
        return PackageManagerHelper::packageNameForComponent("mpv");
    }
    return QString();
}

QImage PluginManager::renderPdfThumbnail(const QString &filePath, int maxDimension)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return QImage();
    }

    // 1. Try FreeDesktop Thumbnail Cache (~/.cache/thumbnails/normal, large, x-large)
    QUrl uri = QUrl::fromLocalFile(QFileInfo(filePath).canonicalFilePath());
    QByteArray hash = QCryptographicHash::hash(uri.toEncoded(), QCryptographicHash::Md5).toHex();
    QString home = QDir::homePath();
    QStringList cachePaths = {
        home + "/.cache/thumbnails/normal/" + hash + ".png",
        home + "/.cache/thumbnails/large/" + hash + ".png",
        home + "/.cache/thumbnails/x-large/" + hash + ".png"
    };

    for (const QString &cPath : cachePaths) {
        if (QFile::exists(cPath)) {
            QImage cached(cPath);
            if (!cached.isNull()) {
                return cached.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }

    // 2. Try CLI pdftoppm if available in system PATH
    QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (!pdftoppm.isEmpty()) {
        QTemporaryDir tempDir;
        if (tempDir.isValid()) {
            QString outPrefix = tempDir.path() + "/thumb";
            QStringList args = {
                "-png",
                "-singlefile",
                "-scale-to", QString::number(maxDimension),
                "-q",
                filePath,
                outPrefix
            };
            QProcess proc;
            proc.start(pdftoppm, args);
            if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
                QString outPng = outPrefix + ".png";
                if (QFile::exists(outPng)) {
                    QImage img(outPng);
                    if (!img.isNull()) {
                        return img;
                    }
                }
            }
        }
    }

    return QImage();
}
