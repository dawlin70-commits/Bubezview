#pragma once

#include <QString>
#include <QStringList>
#include <QFileInfo>

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
 * @brief Manages file format categorization and viewer dispatching.
 * Fast MIME and file extension matching used to determine viewer routing without heavy disk reads.
 */
class PluginManager {
public:
    /**
     * @brief Categorizes a file path into a FileCategory enum based on extension and MIME type.
     * @param filePath Absolute path or filename to categorize.
     * @return Resolved FileCategory.
     */
    static FileCategory categorizeFile(const QString &filePath);

    /**
     * @brief Checks if a file has native in-app viewing support in BubezView.
     * @param filePath Target file path.
     * @return True if supported by an internal viewer; false otherwise.
     */
    static bool isNativeSupported(const QString &filePath);

private:
    static const QStringList s_imageExtensions;
    static const QStringList s_textExtensions;
    static const QStringList s_pdfExtensions;
    static const QStringList s_archiveExtensions;
    static const QStringList s_mediaExtensions;
    static const QStringList s_officeExtensions;
};
