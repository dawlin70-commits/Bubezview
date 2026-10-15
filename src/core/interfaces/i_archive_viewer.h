/**
 * @file i_archive_viewer.h
 * @brief Abstract interface for the Compressed Archive Viewer engine in BubezView.
 *
 * Exposes archive listing, file extraction, and preview coordination
 * without coupling the host to libarchive C headers.
 */

#pragma once

#include "i_viewer_widget.h"
#include <QStringList>
#include <functional>

/**
 * @class IArchiveViewer
 * @brief Abstract interface implemented by the Archive viewer plugin.
 */
class IArchiveViewer : public IViewerWidget {
public:
    virtual ~IArchiveViewer() = default;

    /**
     * @brief Total number of entries (files and directories) inside archive.
     * @return Integer entry count.
     */
    virtual int entryCount() const = 0;

    /**
     * @brief Total cumulative uncompressed bytes of all files in archive.
     * @return Size in bytes.
     */
    virtual qint64 totalUncompressedSize() const = 0;

    /**
     * @brief List of inner file paths eligible for preview.
     * @return String list of file paths.
     */
    virtual QStringList previewableEntries() const = 0;

    /**
     * @brief Extracts a single entry to a temporary file on disk for instant viewing.
     * @param archivePath Path to the container archive.
     * @param entryName Inner relative file path to extract.
     * @return Absolute path to extracted temporary file, or empty string on failure.
     */
    virtual QString extractSingleFile(const QString &archivePath, const QString &entryName) = 0;

    /**
     * @brief Triggers full extraction of all entries in archive.
     */
    virtual void extractAll() {}

    /**
     * @brief Tests archive health and CRC/checksum integrity.
     */
    virtual void testArchiveIntegrity() {}

    /**
     * @brief Prompts the user to select files from disk and appends them to the currently open archive.
     */
    virtual void addFilesToArchive() {}

    /**
     * @brief Prompts the user to select a folder from disk and appends it recursively to the currently open archive.
     */
    virtual void addFolderToArchive() {}

    /**
     * @brief Removes the currently selected items from the open archive.
     */
    virtual void removeSelectedFromArchive() {}

    /**
     * @brief Determines whether the current archive format is safe to modify in-place.
     * @return True if format is writable and unencrypted; false otherwise.
     */
    virtual bool isArchiveEditable() const { return false; }

    /**
     * @brief Creates a new compressed archive from the given list of source files and directories.
     * @param sourcePaths List of absolute paths to files and/or directories to compress.
     * @param destArchivePath Output archive file path.
     * @param format Archive format identifier ("zip", "tar.gz", "tar.xz", "tar.zst", "7z").
     * @param passphrase Optional password for encryption (supported on ZIP).
     * @param parentWidget Optional parent widget for progress and error dialogs.
     * @return True if archive creation succeeded; false otherwise.
     */
    virtual bool createArchive(const QStringList &sourcePaths,
                               const QString &destArchivePath,
                               const QString &format = QStringLiteral("zip"),
                               const QString &passphrase = QString(),
                               QWidget *parentWidget = nullptr) {
        Q_UNUSED(sourcePaths);
        Q_UNUSED(destArchivePath);
        Q_UNUSED(format);
        Q_UNUSED(passphrase);
        Q_UNUSED(parentWidget);
        return false;
    }

    /**
     * @brief Opens the interactive Create / Compress Archive dialog (Case B).
     * @param currentFilePath Currently active file path (if any).
     * @param currentFolderPath Currently active directory path (if any).
     * @param parentWidget Parent widget for modal dialog.
     * @param outSendWithLocalSend Optional output boolean set to true if user clicked "Compress & Send with LocalSend".
     * @param outOpenWhenDone Optional output boolean set to true if user wants to open the created archive in BubezView.
     * @return Absolute path to the newly created archive, or empty string if cancelled/failed.
     */
    virtual QString openCreateArchiveDialog(const QString &currentFilePath,
                                            const QString &currentFolderPath,
                                            QWidget *parentWidget = nullptr,
                                            bool *outSendWithLocalSend = nullptr,
                                            bool *outOpenWhenDone = nullptr) {
        Q_UNUSED(currentFilePath);
        Q_UNUSED(currentFolderPath);
        Q_UNUSED(parentWidget);
        if (outSendWithLocalSend) *outSendWithLocalSend = false;
        if (outOpenWhenDone) *outOpenWhenDone = false;
        return QString();
    }

    /**
     * @brief Registers callback invoked when user clicks an inner file for preview.
     * @param callback Function receiving extracted temp path, archive path, and inner entry name.
     */
    virtual void setOnFilePreviewRequested(std::function<void(const QString &extractedPath, const QString &archivePath, const QString &entryName)> callback) {
        m_filePreviewCallback = callback;
    }

    /**
     * @brief Requests an in-app file preview for the given extracted archive entry.
     */
    virtual void requestFilePreview(const QString &extractedFilePath, const QString &archiveOriginPath, const QString &internalEntryPath = QString()) {
        if (m_filePreviewCallback) {
            m_filePreviewCallback(extractedFilePath, archiveOriginPath, internalEntryPath);
        }
    }

    /**
     * @brief Registers callback invoked when an archive completes background loading.
     * @param callback Function receiving archive path, entry count, and total size.
     */
    virtual void setOnArchiveLoaded(std::function<void(const QString &archivePath, int count, qint64 size)> callback) {
        m_archiveLoadedCallback = callback;
    }

protected:
    std::function<void(const QString&, const QString&, const QString&)> m_filePreviewCallback;
    std::function<void(const QString&, int, qint64)> m_archiveLoadedCallback;
};

