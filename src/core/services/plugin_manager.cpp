/**
 * @file plugin_manager.cpp
 * @brief Implementation of PluginManager dynamic plugin discovery and format categorization.
 */

#include "plugin_manager.h"
#include "interfaces/viewer_plugin_interface.h"
#include "interfaces/i_folder_studio_plugin.h"
#include "interfaces/i_image_studio_plugin.h"
#include "interfaces/i_video_studio_plugin.h"
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
#include <QMessageBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QApplication>
#include <QEventLoop>
#include <QThread>

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
    if (m_folderStudioLoader) {
        m_folderStudioLoader->unload();
        delete m_folderStudioLoader;
        m_folderStudioLoader = nullptr;
        m_folderStudioPlugin = nullptr;
    }
    if (m_imageStudioLoader) {
        m_imageStudioLoader->unload();
        delete m_imageStudioLoader;
        m_imageStudioLoader = nullptr;
        m_imageStudioPlugin = nullptr;
    }
    if (m_videoStudioLoader) {
        m_videoStudioLoader->unload();
        delete m_videoStudioLoader;
        m_videoStudioLoader = nullptr;
        m_videoStudioPlugin = nullptr;
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

    if (m_folderStudioLoader && !m_folderStudioPath.isEmpty() && !QFile::exists(m_folderStudioPath)) {
        m_folderStudioLoader->unload();
        delete m_folderStudioLoader;
        m_folderStudioLoader = nullptr;
        m_folderStudioPlugin = nullptr;
        m_folderStudioPath.clear();
    }
    if (m_imageStudioLoader && !m_imageStudioPath.isEmpty() && !QFile::exists(m_imageStudioPath)) {
        m_imageStudioLoader->unload();
        delete m_imageStudioLoader;
        m_imageStudioLoader = nullptr;
        m_imageStudioPlugin = nullptr;
        m_imageStudioPath.clear();
    }
    if (m_videoStudioLoader && !m_videoStudioPath.isEmpty() && !QFile::exists(m_videoStudioPath)) {
        m_videoStudioLoader->unload();
        delete m_videoStudioLoader;
        m_videoStudioLoader = nullptr;
        m_videoStudioPlugin = nullptr;
        m_videoStudioPath.clear();
    }

    QStringList searchDirs = {
        QCoreApplication::applicationDirPath() + "/plugins",
        QCoreApplication::applicationDirPath() + "/../plugins",
        QDir::homePath() + "/.local/share/bubezview/plugins",
        QDir::homePath() + "/.local/lib/bubezview/plugins",
        QDir::homePath() + "/.local/lib/x86_64-linux-gnu/bubezview/plugins",
        QDir::homePath() + "/.local/lib64/bubezview/plugins",
        "/usr/local/lib/bubezview/plugins",
        "/usr/lib/bubezview/plugins"
    };

    QSet<QString> loadedFileNames;
    for (const auto &p : m_plugins) {
        if (!p.filePath.isEmpty()) {
            loadedFileNames.insert(QFileInfo(p.filePath).fileName());
        }
    }
    if (!m_folderStudioPath.isEmpty()) {
        loadedFileNames.insert(QFileInfo(m_folderStudioPath).fileName());
    }
    if (!m_imageStudioPath.isEmpty()) {
        loadedFileNames.insert(QFileInfo(m_imageStudioPath).fileName());
    }
    if (!m_videoStudioPath.isEmpty()) {
        loadedFileNames.insert(QFileInfo(m_videoStudioPath).fileName());
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
        // Check if this module implements the modular Folder Studio plugin interface (bubez_folder.so)
        auto *folderPlugin = qobject_cast<IFolderStudioPlugin*>(instance);
        if (!folderPlugin) {
            folderPlugin = dynamic_cast<IFolderStudioPlugin*>(instance);
        }
        if (folderPlugin && !m_folderStudioPlugin) {
            m_folderStudioPlugin = folderPlugin;
            m_folderStudioLoader = loader;
            m_folderStudioPath = filePath;
            qDebug() << "[PluginManager] Loaded modular Folder Studio plugin:" << folderPlugin->displayName();
            return;
        }
        // Check if this module implements the modular Image Studio plugin interface (bubez_image.so)
        auto *imagePlugin = qobject_cast<IImageStudioPlugin*>(instance);
        if (!imagePlugin) {
            imagePlugin = dynamic_cast<IImageStudioPlugin*>(instance);
        }
        if (imagePlugin && !m_imageStudioPlugin) {
            m_imageStudioPlugin = imagePlugin;
            m_imageStudioLoader = loader;
            m_imageStudioPath = filePath;
            qDebug() << "[PluginManager] Loaded modular Image Studio plugin:" << imagePlugin->displayName();
            return;
        }
        // Check if this module implements the modular Video Studio plugin interface (bubez_video.so)
        auto *videoPlugin = qobject_cast<IVideoStudioPlugin*>(instance);
        if (!videoPlugin) {
            videoPlugin = dynamic_cast<IVideoStudioPlugin*>(instance);
        }
        if (videoPlugin && !m_videoStudioPlugin) {
            m_videoStudioPlugin = videoPlugin;
            m_videoStudioLoader = loader;
            m_videoStudioPath = filePath;
            qDebug() << "[PluginManager] Loaded modular Video Studio plugin:" << videoPlugin->displayName();
            return;
        }
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
    else if (id == "engine-mpv" || id == "mpv" || id == "libmpv" || id == "media" || id == "engine-audio" || id == "audio-studio") id = "audio";
    else if (id == "engine-folder" || id == "folder-studio") id = "folder";
    else if (id == "engine-image" || id == "image-studio") id = "image";
    else if (id == "engine-video" || id == "video-studio") id = "video";

    if (id == "video") {
        if (!m_videoStudioLoader && !m_videoStudioPlugin) {
            return false;
        }
        emit aboutToUnloadPlugin(QStringLiteral("video"));
        if (m_videoStudioLoader) {
            m_videoStudioLoader->unload();
            delete m_videoStudioLoader;
            m_videoStudioLoader = nullptr;
        }
        m_videoStudioPlugin = nullptr;
        m_videoStudioPath.clear();
        emit pluginsChanged();
        return true;
    }

    if (id == "folder") {
        if (!m_folderStudioLoader && !m_folderStudioPlugin) {
            return false;
        }
        emit aboutToUnloadPlugin(QStringLiteral("folder"));
        if (m_folderStudioLoader) {
            m_folderStudioLoader->unload();
            delete m_folderStudioLoader;
            m_folderStudioLoader = nullptr;
        }
        m_folderStudioPlugin = nullptr;
        m_folderStudioPath.clear();
        emit pluginsChanged();
        return true;
    }

    if (id == "image") {
        if (!m_imageStudioLoader && !m_imageStudioPlugin) {
            return false;
        }
        emit aboutToUnloadPlugin(QStringLiteral("image"));
        if (m_imageStudioLoader) {
            m_imageStudioLoader->unload();
            delete m_imageStudioLoader;
            m_imageStudioLoader = nullptr;
        }
        m_imageStudioPlugin = nullptr;
        m_imageStudioPath.clear();
        emit pluginsChanged();
        return true;
    }

    if (id == "audio") {
        // Unload both "audio" (bubez_audio.so) and any legacy "media" (bubez_media.so)
        return unloadPlugin(FileCategory::Media);
    }
    if (id == "pdf") {
        return unloadPlugin(FileCategory::Pdf);
    }

    auto it = m_plugins.find(id);
    if (it == m_plugins.end()) {
        return false;
    }

    emit aboutToUnloadPlugin(id);
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
            emit aboutToUnloadPlugin(it->id);
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

bool PluginManager::isStudioModuleLoaded(const QString &moduleId) const
{
    const QString id = moduleId.trimmed().toLower();
    if (id == QLatin1String("folder")) {
        return m_folderStudioPlugin != nullptr;
    }
    if (id == QLatin1String("image")) {
        return m_imageStudioPlugin != nullptr;
    }
    if (id == QLatin1String("video")) {
        return m_videoStudioPlugin != nullptr;
    }
    if (id == QLatin1String("pdf")) {
        return isEngineAvailable(FileCategory::Pdf);
    }
    if (id == QLatin1String("audio") || id == QLatin1String("media")) {
        return isEngineAvailable(FileCategory::Media);
    }
    return false;
}

bool PluginManager::activateStudioModuleInteractive(const QString &moduleId, QWidget *parent)
{
    QString id = moduleId.trimmed().toLower();
    if (id == QLatin1String("media")) {
        id = QStringLiteral("audio");
    }
    if (id != QLatin1String("folder") && id != QLatin1String("pdf") &&
        id != QLatin1String("audio") && id != QLatin1String("video") &&
        id != QLatin1String("image")) {
        return false;
    }

    if (isStudioModuleLoaded(id)) {
        return true;
    }

    // Build clear, user-facing description of the features unlocked by this Edit Studio module
    QString title;
    QString header;
    QString featuresHtml;
    QString cmakeFlag;
    QString targetName;
    QString systemDep;

    if (id == QLatin1String("folder")) {
        title = tr("Activate Folder Studio");
        header = tr("Folder Studio Module (<code>bubez_folder.so</code>)");
        featuresHtml = tr(
            "<ul>"
            "<li><b>Visual Duplicate Finder</b> — exact SHA-256, similar images, documents (PDF/text), audio/video, and archives vs folders.</li>"
            "<li><b>PhotoRec / TestDisk Recovery</b> — magic-byte verification, corrupt/junk filtering, and smart renaming.</li>"
            "<li><b>Smart Tree Organizer</b> — automatic sorting by category/date or flattening with Dry-Run preview and Undo.</li>"
            "<li><b>Batch Rename &amp; Cleanup</b> — token-based renaming and empty-folder / broken-link cleaner.</li>"
            "</ul>");
        cmakeFlag = QStringLiteral("-DBUILD_PLUGIN_FOLDER=ON");
        targetName = QStringLiteral("bubez_folder");
    } else if (id == QLatin1String("pdf")) {
        title = tr("Activate PDF Studio");
        header = tr("PDF Studio Module (<code>bubez_pdf.so</code>)");
        featuresHtml = tr(
            "<ul>"
            "<li><b>Annotations &amp; Drawing</b> — highlights, freehand ink, shapes, and typewriter text overlays.</li>"
            "<li><b>Signatures &amp; Watermarks</b> — handwritten/image signatures, cryptographic signing, and custom watermarks.</li>"
            "<li><b>Page Management</b> — visual page organizer, reorder, rotate, delete, merge, and split PDFs.</li>"
            "<li><b>Document Tools</b> — embedded image extraction and PDF repair utilities.</li>"
            "</ul>");
        cmakeFlag = QStringLiteral("-DBUILD_PLUGIN_PDF=ON");
        targetName = QStringLiteral("bubez_pdf");
    } else if (id == QLatin1String("image")) {
        title = tr("Activate Image Studio");
        header = tr("Image Studio Module (<code>bubez_image.so</code>)");
        featuresHtml = tr(
            "<ul>"
            "<li><b>Auto Enhance</b> — one-click Natural, Vivid, Backlight Shadows, and Dehaze enhancement profiles.</li>"
            "<li><b>Light &amp; Color Grading</b> — precision brightness, contrast, saturation, temperature, and hue controls.</li>"
            "<li><b>Cinematic Presets</b> — Teal &amp; Orange, Vintage Sepia, Film Noir, Golden Hour, Cyberpunk, Document &amp; more.</li>"
            "<li><b>Details &amp; Transform</b> — sharpen, blur, vignette, invert, interactive crop, rotate, flip, and alpha flatten.</li>"
            "</ul>");
        cmakeFlag = QStringLiteral("-DBUILD_PLUGIN_IMAGE=ON");
        targetName = QStringLiteral("bubez_image");
    } else if (id == QLatin1String("video")) {
        title = tr("Activate Video Studio");
        header = tr("Video Studio Module (<code>bubez_video.so</code>)");
        featuresHtml = tr(
            "<ul>"
            "<li><b>Lossless Trim &amp; Cut</b> — export video clips with exact start/end timestamps from playback.</li>"
            "<li><b>Frame Snapshot</b> — capture full-resolution PNG/JPEG frames at any timestamp.</li>"
            "<li><b>Audio Extraction</b> — extract video soundtracks to MP3, FLAC, WAV, or Opus.</li>"
            "<li><b>Transform &amp; Optimize</b> — rotate, flip, downscale (1080p/720p/480p), change speed, or mute audio.</li>"
            "</ul>");
        cmakeFlag = QStringLiteral("-DBUILD_PLUGIN_VIDEO=ON");
        targetName = QStringLiteral("bubez_video");
    } else {
        title = tr("Activate Audio Studio");
        header = tr("Audio Studio Module (<code>bubez_audio.so</code>)");
        featuresHtml = tr(
            "<ul>"
            "<li><b>High-Fidelity MPV Audio Engine</b> — precision seeking and 64-bit float audio playback.</li>"
            "<li><b>Audio Cutter &amp; Restoration</b> — waveform trim/cut, noise filters, pitch &amp; channel controls.</li>"
            "<li><b>Metadata &amp; Tag Editor</b> — edit ID3/Vorbis tags and embed album cover artwork.</li>"
            "<li><b>Live Visualizers</b> — 10-band graphic equalizer and stereo oscilloscope.</li>"
            "</ul>");
        cmakeFlag = QStringLiteral("-DBUILD_PLUGIN_AUDIO=ON");
        targetName = QStringLiteral("bubez_audio");
        systemDep = QStringLiteral("mpv");
    }

    QMessageBox prompt(parent);
    prompt.setWindowTitle(title);
    prompt.setIcon(QMessageBox::Information);
    prompt.setTextFormat(Qt::RichText);
    prompt.setText(tr("<h3>%1</h3>"
                      "<p>This optional edit module is not installed yet. Activating it will compile and load only this module on demand:</p>"
                      "%2"
                      "<p>Do you want to activate it now?</p>")
                       .arg(header, featuresHtml));

    QPushButton *btnOk = prompt.addButton(tr("OK"), QMessageBox::AcceptRole);
    prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
    prompt.setDefaultButton(btnOk);
    prompt.exec();

    if (prompt.clickedButton() != btnOk) {
        return false;
    }

    // Install optional system library dependency on demand (e.g. libmpv for Multimedia Studio)
    if (!systemDep.isEmpty()) {
        QProcess pkgCheck;
        pkgCheck.start(QStringLiteral("pkg-config"), {QStringLiteral("--exists"), systemDep});
        pkgCheck.waitForFinished(3000);
        if (pkgCheck.exitCode() != 0) {
            if (!PackageManagerHelper::installComponentInteractive(systemDep, parent)) {
                return false;
            }
        }
    }

    QProgressDialog progress(tr("Compiling and activating %1...").arg(targetName + QStringLiteral(".so")),
                             QString(), 0, 0, parent);
    progress.setWindowTitle(title);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    auto runProcessAsync = [&](const QString &program, const QStringList &args, QString *outLog) -> bool {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        QEventLoop loop;
        QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &loop, &QEventLoop::quit);
        proc.start(program, args);
        if (!proc.waitForStarted(5000)) {
            if (outLog) *outLog = tr("Failed to start %1").arg(program);
            return false;
        }
        loop.exec();
        if (outLog) *outLog = QString::fromUtf8(proc.readAllStandardOutput());
        return (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
    };

    const QString home = QDir::homePath();
    const QString userPluginsDir = home + QStringLiteral("/.local/share/bubezview/plugins");
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString devRoot = BuildConfig::projectDevRoot();
    const QString soFileName = targetName + QStringLiteral(".so");

    bool buildOk = false;
    QString lastLog;

    // Strategy 1: Local source tree available (devRoot or parent of build dir) -> fast direct single-target build
    QString sourceDir;
    if (!devRoot.isEmpty() && QFile::exists(devRoot + QStringLiteral("/CMakeLists.txt"))) {
        sourceDir = devRoot;
    } else if (QFile::exists(appDir + QStringLiteral("/../CMakeLists.txt"))) {
        sourceDir = QDir(appDir + QStringLiteral("/..")).canonicalPath();
    }

    if (!sourceDir.isEmpty()) {
        QString buildDir;
        if (QFile::exists(appDir + QStringLiteral("/CMakeCache.txt"))) {
            buildDir = appDir;
        } else if (QFile::exists(sourceDir + QStringLiteral("/build_install/CMakeCache.txt"))) {
            buildDir = sourceDir + QStringLiteral("/build_install");
        } else if (QFile::exists(sourceDir + QStringLiteral("/build_deb/CMakeCache.txt"))) {
            buildDir = sourceDir + QStringLiteral("/build_deb");
        } else {
            buildDir = sourceDir + QStringLiteral("/build_install");
        }
        QDir().mkpath(buildDir);

        const int jobs = qMax(1, QThread::idealThreadCount());
        if (runProcessAsync(QStringLiteral("cmake"),
                            {QStringLiteral("-S"), sourceDir, QStringLiteral("-B"), buildDir,
                             QStringLiteral("-DCMAKE_BUILD_TYPE=Release"),
                             QStringLiteral("-DBUILD_TESTS=OFF"), cmakeFlag},
                            &lastLog)) {
            if (runProcessAsync(QStringLiteral("cmake"),
                                {QStringLiteral("--build"), buildDir,
                                 QStringLiteral("--target"), targetName,
                                 QStringLiteral("--parallel"), QString::number(jobs)},
                                &lastLog)) {
                const QString builtSo = buildDir + QStringLiteral("/plugins/") + soFileName;
                if (QFile::exists(builtSo)) {
                    QDir().mkpath(userPluginsDir);
                    const QString destPath = userPluginsDir + QLatin1Char('/') + soFileName;
                    QFile::remove(destPath);
                    buildOk = QFile::copy(builtSo, destPath);
                }
            }
        }
    } else {
        // Strategy 2: Installed standalone user binary -> delegate to saved Install.sh --build-plugin <id>
        const QString installerPath = home + QStringLiteral("/.local/share/bubezview/Install.sh");
        if (QFile::exists(installerPath)) {
            buildOk = runProcessAsync(QStringLiteral("bash"),
                                      {installerPath, QStringLiteral("--build-plugin"), id},
                                      &lastLog);
        } else {
            lastLog = tr("No local source directory or installer script found (%1).").arg(installerPath);
        }
    }

    progress.close();

    if (buildOk) {
        discoverPlugins();
    }

    if (!isStudioModuleLoaded(id)) {
        const QString tail = lastLog.right(900).trimmed();
        QMessageBox::warning(
            parent,
            tr("Module Activation Failed"),
            tr("Could not compile or load %1.\n\n%2")
                .arg(soFileName, tail.isEmpty() ? tr("Check compiler and development dependencies.") : tail));
        return false;
    }

    return true;
}

bool PluginManager::uninstallStudioModule(const QString &moduleId)
{
    const QString id = moduleId.trimmed().toLower();
    QStringList soNames;
    QString loadedPath;

    if (id == QLatin1String("folder")) {
        soNames << QStringLiteral("bubez_folder.so");
        loadedPath = m_folderStudioPath;
    } else if (id == QLatin1String("image")) {
        soNames << QStringLiteral("bubez_image.so");
        loadedPath = m_imageStudioPath;
    } else if (id == QLatin1String("video")) {
        soNames << QStringLiteral("bubez_video.so");
        loadedPath = m_videoStudioPath;
    } else if (id == QLatin1String("pdf")) {
        soNames << QStringLiteral("bubez_pdf.so");
        for (const auto &desc : m_plugins) {
            if (desc.category == FileCategory::Pdf && desc.filePath != QLatin1String("builtin")) {
                loadedPath = desc.filePath;
                break;
            }
        }
    } else if (id == QLatin1String("audio") || id == QLatin1String("media")) {
        soNames << QStringLiteral("bubez_audio.so") << QStringLiteral("bubez_media.so");
        for (const auto &desc : m_plugins) {
            if (desc.category == FileCategory::Media && desc.filePath != QLatin1String("builtin")) {
                loadedPath = desc.filePath;
                break;
            }
        }
    } else {
        return false;
    }

    bool removed = unloadPlugin(id);

    if (!loadedPath.isEmpty() && loadedPath != QLatin1String("builtin") && QFile::exists(loadedPath)) {
        if (QFile::remove(loadedPath)) {
            removed = true;
        }
    }

    const QString home = QDir::homePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString devRoot = BuildConfig::projectDevRoot();

    QStringList dirs = {
        appDir + QStringLiteral("/plugins"),
        home + QStringLiteral("/.local/share/bubezview/plugins"),
        home + QStringLiteral("/.local/lib/bubezview/plugins"),
        home + QStringLiteral("/.local/lib/x86_64-linux-gnu/bubezview/plugins"),
        home + QStringLiteral("/.local/lib64/bubezview/plugins")
    };
    if (!devRoot.isEmpty()) {
        dirs << (devRoot + QStringLiteral("/build_install/plugins"))
             << (devRoot + QStringLiteral("/build_deb/plugins"))
             << (devRoot + QStringLiteral("/container/.local/lib/bubezview/plugins"))
             << (devRoot + QStringLiteral("/container/.local/share/bubezview/plugins"));
    }

    for (const QString &d : dirs) {
        for (const QString &soName : soNames) {
            const QString p = d + QLatin1Char('/') + soName;
            if (QFile::exists(p) && QFile::remove(p)) {
                removed = true;
            }
        }
    }

    emit pluginsChanged();
    return removed;
}
