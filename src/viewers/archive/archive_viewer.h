/**
 * @file archive_viewer.h
 * @brief Header definition for advanced archive browser using libarchive.
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QString>
#include <QIcon>
#include <QFileInfo>
#include <QThread>
#include <QDateTime>
#include <atomic>

/**
 * @brief Represents metadata for a single archive entry.
 */
struct ArchiveItemRecord {
    QString fullPath;    /**< Normalized relative path inside archive. */
    bool isDir;          /**< True if the entry represents a directory. */
    qint64 size;         /**< Uncompressed size in bytes. */
    QDateTime mtime;     /**< Modification timestamp. */
    bool isEncrypted;    /**< True if the entry is password protected or encrypted. */
};

/**
 * @brief Aggregated metadata statistics for archive directories.
 */
struct FolderStats {
    int count = 0;          /**< Number of descendant files. */
    qint64 totalSize = 0;   /**< Total uncompressed size in bytes. */
    QDateTime latestMtime;  /**< Latest modification timestamp of child entries. */
};

/**
 * @brief Asynchronous thread for parsing archive central directories without blocking the UI.
 */
class ArchiveWorker : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an ArchiveWorker for reading archive metadata.
     * @param filePath Path to the archive file.
     * @param passphrase Optional password for encrypted archives.
     * @param parent Optional parent QObject.
     */
    explicit ArchiveWorker(const QString &filePath, const QString &passphrase = QString(), QObject *parent = nullptr);
    ~ArchiveWorker() override;

    /**
     * @brief Requests cancellation of the ongoing archive parsing loop.
     */
    void cancel();

    /**
     * @brief Takes parsed and pre-sorted archive entries from the worker.
     * @return Moved list of archive item records.
     */
    QList<ArchiveItemRecord> takeEntries() { return std::move(m_entries); }

    /**
     * @brief Takes pre-calculated directory statistics.
     * @return Moved map of folder statistics.
     */
    QMap<QString, FolderStats> takeDirStats() { return std::move(m_dirStats); }

signals:
    /**
     * @brief Emitted when archive parsing finishes successfully.
     * @param filePath Absolute path to the archive file.
     * @param totalSize Total uncompressed size in bytes.
     * @param formatName Archive format identifier (e.g. ZIP, 7-Zip, ISO9660).
     * @param filterName Compression filter identifier (e.g. gzip, xz, zstd, none).
     */
    void parsed(const QString &filePath, qint64 totalSize,
                const QString &formatName, const QString &filterName);

    /**
     * @brief Emitted when archive parsing encounters a fatal error.
     * @param filePath Path to the archive.
     * @param errorMsg Error description string.
     */
    void error(const QString &filePath, const QString &errorMsg);

    /**
     * @brief Emitted when an archive is encrypted and requires a passphrase to be unlocked.
     * @param filePath Path to the encrypted archive.
     */
    void passphraseRequired(const QString &filePath);

protected:
    void run() override;

private:
    QString m_filePath;
    QString m_passphrase;
    std::atomic<bool> m_isCancelled;
    QList<ArchiveItemRecord> m_entries;
    QMap<QString, FolderStats> m_dirStats;
};

/**
 * @brief Modern archive viewer and extractor utilizing libarchive.
 *
 * Supports browsing ZIP, TAR, GZ, BZ2, XZ, 7Z, ISO, DEB, RPM, and CPIO files.
 * Features fast hierarchical tree navigation, on-demand file previewing, directory extraction,
 * password decryption handling, integrity testing, and live name filtering.
 */
class ArchiveViewer : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Icon types used to classify tree items.
     */
    enum class IconType {
        Folder,
        Code,
        Zip,
        Image,
        Media,
        Document,
        Locked
    };

    explicit ArchiveViewer(QWidget *parent = nullptr);
    ~ArchiveViewer() override;

    /**
     * @brief Opens an archive and parses its directory asynchronously.
     * @param filePath Absolute path to the archive file.
     * @param passphrase Optional password to decrypt archive headers/content.
     * @return True if file exists and worker started; false otherwise.
     */
    bool loadFile(const QString &filePath, const QString &passphrase = QString());

    /**
     * @brief Clears tree items, cancels active archive workers, and resets state.
     */
    void clear();

    /**
     * @brief Updates theme palette colors for the top toolbar and tree view.
     */
    void updateTheme();

    /**
     * @brief Extracts all files from the current archive into a target destination.
     * @param destDir Destination folder path. If empty, a folder picker dialog is shown.
     */
    void extractAll(const QString &destDir = QString());

    /**
     * @brief Extracts currently selected items from the archive into a target destination.
     * @param destDir Destination folder path. If empty, a folder picker dialog is shown.
     */
    void extractSelected(const QString &destDir = QString());

    /**
     * @brief Tests the CRC/checksum integrity of all entries in the current archive.
     */
    void testArchiveIntegrity();

    int entryCount() const { return m_entryCount; }
    qint64 totalUncompressedSize() const { return m_totalSize; }
    QString currentArchiveFilePath() const { return m_currentArchiveFilePath; }

    /**
     * @brief Returns an ordered list of normalized relative paths for all previewable file entries.
     * @return List of file paths inside the archive.
     */
    QStringList previewableEntries() const;

    /**
     * @brief Extracts a single targeted file from the specified archive to a temporary path.
     * @param archivePath Path to the archive.
     * @param internalName Normalized relative path inside the archive.
     * @return Path to extracted temporary file, or empty string on failure.
     */
    QString extractSingleFile(const QString &archivePath, const QString &internalName);

signals:
    /**
     * @brief Emitted when an archive entry is clicked and extracted for in-app preview.
     * @param extractedFilePath Temporary location of the extracted file.
     * @param archiveOriginPath Path of the parent archive containing the file.
     * @param internalEntryPath Normalized relative path of the entry inside the archive.
     */
    void filePreviewRequested(const QString &extractedFilePath, const QString &archiveOriginPath, const QString &internalEntryPath = QString());

    /**
     * @brief Emitted when archive parsing finishes.
     * @param filePath Path to the archive.
     * @param count Total number of files and directories in the archive.
     * @param totalSize Total uncompressed size in bytes.
     */
    void archiveLoaded(const QString &filePath, int count, qint64 totalSize);

private slots:
    void onArchiveParsed(const QString &filePath, qint64 totalSize,
                         const QString &formatName, const QString &filterName);
    void onArchiveError(const QString &filePath, const QString &errorMsg);
    void onPassphraseRequired(const QString &filePath);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onFilterTextChanged(const QString &pattern);
    void onContextMenuRequested(const QPoint &pos);

private:
    void setupUi();
    QString formatSize(qint64 bytes) const;
    QIcon createVectorIcon(IconType type) const;
    IconType detectIconType(const QString &fileName, bool isDir) const;
    void extractFiles(const QStringList &filterPaths, const QString &destDir);

    QWidget *m_topBar;
    QFrame *m_pillBadge;
    QLabel *m_pillText;
    QLabel *m_pillIcon;
    QLineEdit *m_searchEdit;
    QPushButton *m_extractBtn;
    QPushButton *m_testBtn;
    QTreeWidget *m_treeWidget;

    ArchiveWorker *m_worker;
    QString m_currentArchiveFilePath;
    QString m_currentPassphrase;
    QList<ArchiveItemRecord> m_entries;
    int m_entryCount;
    qint64 m_totalSize;
    QString m_formatName;
    QString m_filterName;
};
