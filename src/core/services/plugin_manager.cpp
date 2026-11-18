/**
 * @file plugin_manager.cpp
 * @brief Implementation of PluginManager dynamic plugin discovery and format categorization.
 */

#include "plugin_manager.h"
#include "interfaces/viewer_plugin_interface.h"
#include "package_manager_helper.h"
#include "build_config.h"
#ifdef HAVE_LIBARCHIVE
#include "viewers/archive/archive_viewer.h"
#endif
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
#include <QMimeDatabase>
#include <QMimeType>

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
    // Video Formats
    "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v", "3gp", "3g2",
    "mpg", "mpeg", "m2v", "ts", "m2ts", "mts", "m2t", "vob", "ogv", "rm",
    "rmvb", "asf", "divx", "f4v", "wtv", "dvr-ms", "mxf", "dv", "h264", "h265",
    "264", "265", "hevc", "avc", "av1", "y4m", "nut", "bik",
    // Audio Formats
    "mp3", "wav", "flac", "ogg", "oga", "m4a", "aac", "wma", "opus", "alac",
    "aiff", "aif", "aifc", "dsf", "dff", "dsd", "ape", "mac", "wv", "tta", "tak",
    "mpc", "mp+", "mpp", "caf", "amr", "awb", "voc", "qcp", "ra", "ram",
    "mod", "xm", "it", "s3m", "med", "okt", "669", "mtm", "mid", "midi",
    "ac3", "eac3", "dts", "dtshd", "thd", "mka"
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

    // MIME type based fallback detection for video/audio, images, and archives
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    const QString mimeName = mime.name();
    if (mimeName.startsWith(QStringLiteral("video/")) || mimeName.startsWith(QStringLiteral("audio/"))) {
        return FileCategory::Media;
    }
    if (mimeName.startsWith(QStringLiteral("image/"))) {
        return FileCategory::Image;
    }
    if (mimeName == QStringLiteral("application/pdf")) {
        return FileCategory::Pdf;
    }
    if (mime.inherits(QStringLiteral("application/zip")) ||
        mime.inherits(QStringLiteral("application/x-tar")) ||
        mime.inherits(QStringLiteral("application/x-compressed-tar")) ||
        mime.inherits(QStringLiteral("application/x-7z-compressed")) ||
        mime.inherits(QStringLiteral("application/x-archive"))) {
        return FileCategory::Archive;
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
    if (cat == FileCategory::Image || cat == FileCategory::Text || cat == FileCategory::Folder || cat == FileCategory::Office || cat == FileCategory::Media || cat == FileCategory::Pdf) {
        return true;
    }
    // For modular categories (Archive), check if engine plugin is available
    return instance().isEngineAvailable(cat);
}

void PluginManager::discoverPlugins()
{
    // First, verify whether existing loaded plugins still exist on disk
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ) {
        if (it->loader && !it->filePath.isEmpty() && it->filePath != "builtin") {
            if (!QFile::exists(it->filePath)) {
                it->loader->unload();
                delete it->loader;
                it->loader = nullptr;
                it = m_plugins.erase(it);
                continue;
            }
        }
        ++it;
    }

    QStringList searchDirs = {
        QCoreApplication::applicationDirPath() + "/plugins",
        QCoreApplication::applicationDirPath() + "/../plugins",
        QDir::homePath() + "/.local/lib/bubezview/plugins",
        QDir::homePath() + "/.local/lib/x86_64-linux-gnu/bubezview/plugins",
        QDir::homePath() + "/.local/lib64/bubezview/plugins",
        QDir::homePath() + "/.local/share/bubezview/plugins",
        "/usr/local/lib/bubezview/plugins",
        "/usr/lib/bubezview/plugins"
    };

    const QString devRoot = BuildConfig::projectDevRoot();
    if (!devRoot.isEmpty()) {
        searchDirs.append(devRoot + "/container/.local/lib/bubezview/plugins");
        searchDirs.append(devRoot + "/container/.local/share/bubezview/plugins");
        searchDirs.append(devRoot + "/build_deb/plugins/pdf");
        searchDirs.append(devRoot + "/build_deb/plugins/archive");
        searchDirs.append(devRoot + "/build_deb/plugins/media");
        searchDirs.append(devRoot + "/build_deb/plugins");
        searchDirs.append(devRoot + "/build_install/lib/bubezview/plugins");
    }

    QSet<QString> loadedFileNames;
    for (const auto &p : m_plugins) {
        if (!p.filePath.isEmpty()) {
            loadedFileNames.insert(QFileInfo(p.filePath).fileName());
        }
    }

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

#ifdef HAVE_LIBARCHIVE
    if (!m_plugins.contains("archive")) {
        PluginDescriptor desc;
        desc.id = "archive";
        desc.filePath = "builtin";
        desc.displayName = "LibArchive Engine (Built-in)";
        desc.description = "Direct internal compressed archive support (ZIP, TAR, 7Z, ISO, etc.)";
        desc.category = FileCategory::Archive;
        desc.extensions = s_archiveExtensions;
        desc.isLoaded = true;
        desc.interfaceInstance = nullptr;
        desc.loader = nullptr;
        m_plugins[desc.id] = desc;
    }
#endif

    emit pluginsChanged();
}

void PluginManager::loadPluginFromFile(const QString &filePath)
{
    QPluginLoader *loader = new QPluginLoader(filePath, this);
    QObject *instance = loader->instance();
    if (!instance) {
        qDebug() << "[PluginManager] Failed to load plugin from" << filePath << ":" << loader->errorString();
        delete loader;
        return;
    }
    qDebug() << "[PluginManager] Successfully loaded plugin from" << filePath;

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
        if (desc.category == category && desc.isLoaded && (desc.interfaceInstance || desc.filePath == "builtin")) {
            return true;
        }
    }
    return false;
}

bool PluginManager::unloadPlugin(const QString &pluginId)
{
    QString id = pluginId.trimmed().toLower();
    if (id == "engine-poppler" || id == "poppler" || id == "poppler-qt6") id = "pdf";
    else if (id == "engine-libarchive" || id == "libarchive") id = "archive";
    else if (id == "engine-mpv" || id == "mpv" || id == "libmpv") id = "media";

    auto it = m_plugins.find(id);
    if (it == m_plugins.end()) {
        return false;
    }

    if (it->loader) {
        it->loader->unload();
        delete it->loader;
        it->loader = nullptr;
    }
    m_plugins.erase(it);
    emit pluginsChanged();
    return true;
}

bool PluginManager::unloadPlugin(FileCategory category)
{
    bool anyUnloaded = false;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ) {
        if (it->category == category) {
            if (it->loader) {
                it->loader->unload();
                delete it->loader;
                it->loader = nullptr;
            }
            it = m_plugins.erase(it);
            anyUnloaded = true;
        } else {
            ++it;
        }
    }
    if (anyUnloaded) {
        emit pluginsChanged();
    }
    return anyUnloaded;
}

IViewerWidget *PluginManager::createViewer(FileCategory category, QWidget *parent)
{
    for (const auto &desc : m_plugins) {
        if (desc.category == category && desc.isLoaded) {
            if (desc.interfaceInstance) {
                return desc.interfaceInstance->createViewer(parent);
            }
#ifdef HAVE_LIBARCHIVE
            if (desc.filePath == "builtin" && category == FileCategory::Archive) {
                return new ArchiveViewer(parent);
            }
#endif
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
