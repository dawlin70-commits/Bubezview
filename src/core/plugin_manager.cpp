#include "plugin_manager.h"
#include <QFileInfo>

const QStringList PluginManager::s_imageExtensions = {
    "png", "jpg", "jpeg", "webp", "svg", "bmp", "gif", "ico", "tiff", "tif",
    "avif", "heic", "heif", "pbm", "pgm", "ppm", "xbm", "xpm"
};

const QStringList PluginManager::s_textExtensions = {
    "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
    "json", "xml", "html", "htm", "css", "js", "ts", "sh", "bash", "zsh",
    "yaml", "yml", "toml", "ini", "conf", "cfg", "log", "desktop", "service",
    "qml", "qrc", "ui", "cmake", "diff", "patch", "csv", "tsv"
};

const QStringList PluginManager::s_pdfExtensions = {
    "pdf"
};

const QStringList PluginManager::s_archiveExtensions = {
    "zip", "tar", "gz", "tgz", "bz2", "tbz2", "xz", "txz", "7z", "iso", "deb", "rpm", "cpio"
};

const QStringList PluginManager::s_mediaExtensions = {
    "mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v", "3gp",
    "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus"
};

const QStringList PluginManager::s_officeExtensions = {
    "docx", "doc", "xlsx", "xls", "xltx", "xlsm", "pptx", "ppt", "odt", "ods", "odp", "epub", "rtf"
};

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
    return (cat != FileCategory::Unknown);
}
