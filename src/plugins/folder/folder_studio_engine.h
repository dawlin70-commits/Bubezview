/**
 * @file folder_studio_engine.h
 * @brief Multi-format analysis, deduplication, PhotoRec/TestDisk recovery,
 *        smart tree organization, batch rename, and undo rollback engine.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QList>
#include <atomic>
#include <functional>

/**
 * @brief Category of a detected duplicate or equivalent group.
 */
enum class DuplicateKind {
    Exact,          ///< Bit-for-bit identical files (any format)
    DocumentText,   ///< Equivalent text/PDF/Office documents with same content
    AudioVideo,     ///< Same audio/video track in different formats or quality
    SimilarImage,   ///< Visually identical or burst-similar images
    ArchiveFolder   ///< Archive file sitting next to an already-extracted folder
};

/**
 * @brief Represents one candidate file inside a duplicate/equivalent cluster.
 */
struct DuplicateItem {
    QString path;
    QString fileName;
    qint64 sizeBytes = 0;
    int qualityScore = 0;       ///< Heuristic quality score (0..100)
    QString qualitySummary;     ///< Human-readable quality summary (e.g. "12 MP • Sharp • EXIF")
    bool recommendedKeep = false;
};

/**
 * @brief Cluster of exact or content-equivalent files.
 */
struct DuplicateGroup {
    DuplicateKind kind = DuplicateKind::Exact;
    QString title;
    QString detail;
    QList<DuplicateItem> items;
};

/**
 * @brief Integrity and classification status of a file inspected during Recovery scan.
 */
enum class RecoveryStatus {
    Healthy,        ///< Valid file with matching extension
    WrongExtension, ///< Valid file whose real magic-byte format differs from its extension
    Corrupt,        ///< Truncated or unreadable file header/structure
    Junk            ///< Recovery noise (0-byte, tiny cache icon/thumbnail, OS temp junk)
};

/**
 * @brief Analysis record for a single file in Recovery & Repair mode.
 */
struct RecoveryItem {
    QString path;
    QString detectedMime;
    QString detectedExt;
    RecoveryStatus status = RecoveryStatus::Healthy;
    QString suggestedName;
    QString internalTitle;
    QDateTime internalDate;
    qint64 sizeBytes = 0;
    QString reason;
};

/**
 * @brief Strategy for Smart Tree Organization.
 */
enum class OrganizeMode {
    ByCategory,        ///< Group into Documents_PDF/, Office/, Images/, Audio/, Video/, Archives/, Text_Code/
    ByYearMonth,       ///< Group into YYYY/YYYY-MM/ using internal metadata date or mtime
    ByCategoryAndYear, ///< Group into Category/YYYY/
    Flatten            ///< Flatten nested folders (e.g. recup_dir.*) into root and clean empty dirs
};

/**
 * @brief Planned filesystem move/rename operation for Dry-Run preview and execution.
 */
struct MovePlanItem {
    QString sourcePath;
    QString destPath;
    QString reason;
};

/**
 * @brief Classification of a cleanup candidate item.
 */
enum class CleanupKind {
    EmptyDir,
    BrokenSymlink,
    ZeroByteFile,
    TempJunk
};

/**
 * @brief Candidate item detected by Folder Cleanup scan.
 */
struct CleanupItem {
    QString path;
    CleanupKind kind = CleanupKind::TempJunk;
    qint64 sizeBytes = 0;
};

/**
 * @class FolderStudioEngine
 * @brief Stateless multi-format folder processing algorithms with cancellation and rollback support.
 */
class FolderStudioEngine {
public:
    using ProgressCallback = std::function<void(int current, int total, const QString &statusText)>;

    /**
     * @brief Scans a folder for exact duplicates and multi-format content equivalents.
     * @param rootFolder Target directory path.
     * @param recursive Whether to descend into subdirectories.
     * @param includeExact Enable bit-for-bit duplicate detection across all file types.
     * @param includeDocuments Enable PDF/Text content SimHash duplicate detection.
     * @param includeMedia Enable Audio/Video cross-format equivalent detection.
     * @param includeImages Enable perceptual dHash visual similarity & best-shot scoring.
     * @param includeArchiveFolders Enable redundant archive-vs-extracted-folder detection.
     * @param imageSimilarityPercent Similarity threshold for visual images (80..98).
     * @param cancelFlag Atomic cancellation flag.
     * @param progress Optional progress callback.
     * @return List of detected DuplicateGroup clusters.
     */
    static QList<DuplicateGroup> scanDuplicates(const QString &rootFolder,
                                                bool recursive,
                                                bool includeExact,
                                                bool includeDocuments,
                                                bool includeMedia,
                                                bool includeImages,
                                                bool includeArchiveFolders,
                                                int imageSimilarityPercent,
                                                const std::atomic<bool> &cancelFlag,
                                                const ProgressCallback &progress = nullptr);

    /**
     * @brief Analyzes a folder (e.g. PhotoRec/TestDisk dump) via magic bytes, integrity checks, and internal metadata.
     * @param rootFolder Target directory path.
     * @param recursive Whether to descend into subdirectories (e.g. recup_dir.*).
     * @param junkMaxKb Maximum size in KB below which tiny recovered images/fragments are flagged as junk.
     * @param cancelFlag Atomic cancellation flag.
     * @param progress Optional progress callback.
     * @return List of RecoveryItem analysis records.
     */
    static QList<RecoveryItem> scanRecovery(const QString &rootFolder,
                                            bool recursive,
                                            int junkMaxKb,
                                            const std::atomic<bool> &cancelFlag,
                                            const ProgressCallback &progress = nullptr);

    /**
     * @brief Generates a Dry-Run move plan to reorganize or flatten a folder tree.
     * @param rootFolder Target root directory.
     * @param mode Reorganization strategy.
     * @param recursive Whether to include files inside subdirectories.
     * @param cancelFlag Atomic cancellation flag.
     * @return Planned list of MovePlanItem entries (no disk writes performed).
     */
    static QList<MovePlanItem> planTreeOrganization(const QString &rootFolder,
                                                    OrganizeMode mode,
                                                    bool recursive,
                                                    const std::atomic<bool> &cancelFlag);

    /**
     * @brief Generates a Dry-Run batch rename plan using tokens and optional find/replace.
     * Supported tokens: {name}, {clean}, {title}, {date}, {type}, {counter}, {counter:03}, {ext}.
     * @param rootFolder Target root directory.
     * @param recursive Whether to include subdirectories.
     * @param pattern Target filename pattern (empty keeps original stem).
     * @param findText Optional substring or regex to replace.
     * @param replaceText Replacement text.
     * @param useRegex Treat findText as a regular expression.
     * @param cancelFlag Atomic cancellation flag.
     * @return Planned list of MovePlanItem rename operations.
     */
    static QList<MovePlanItem> planBatchRename(const QString &rootFolder,
                                               bool recursive,
                                               const QString &pattern,
                                               const QString &findText,
                                               const QString &replaceText,
                                               bool useRegex,
                                               const std::atomic<bool> &cancelFlag);

    /**
     * @brief Scans a folder for empty directories, broken symlinks, 0-byte files, and OS temp junk.
     * @param rootFolder Target directory.
     * @param recursive Whether to scan recursively.
     * @param cancelFlag Atomic cancellation flag.
     * @return List of CleanupItem candidates.
     */
    static QList<CleanupItem> scanCleanup(const QString &rootFolder,
                                          bool recursive,
                                          const std::atomic<bool> &cancelFlag);

    /**
     * @brief Executes a Dry-Run move/rename plan safely, resolving collisions and persisting an Undo manifest.
     * @param plan List of move/rename operations to apply.
     * @param rootFolder Root directory (used for empty-dir pruning when flattening/organizing).
     * @param pruneEmptyDirs If true, removes directories left empty after moving files.
     * @param saveUndoManifest If true, writes a rollback manifest for undoLastOperation().
     * @param outAppliedCount Populated with number of successfully moved/renamed items.
     * @param outErrors Populated with human-readable error messages, if any.
     * @return True if at least one operation succeeded with zero fatal errors.
     */
    static bool executeMovePlan(const QList<MovePlanItem> &plan,
                                const QString &rootFolder,
                                bool pruneEmptyDirs,
                                bool saveUndoManifest,
                                int *outAppliedCount = nullptr,
                                QStringList *outErrors = nullptr);

    /**
     * @brief Moves a list of files or directories safely to the system Trash.
     * @param paths Absolute paths to trash.
     * @param outTrashedCount Populated with count of trashed items.
     * @param outErrors Populated with failure details.
     * @return True if all requested items were trashed.
     */
    static bool trashItems(const QStringList &paths, int *outTrashedCount = nullptr, QStringList *outErrors = nullptr);

    /**
     * @brief Checks whether a saved rollback manifest exists from a recent organize/rename operation.
     * @return True if undoLastOperation() can revert a previous transaction.
     */
    static bool canUndoLastOperation();

    /**
     * @brief Reverts the last recorded organize/rename/recovery move transaction.
     * @param outRestoredCount Populated with number of files moved back to their original paths.
     * @param outErrors Populated with any paths that could not be restored.
     * @return True if rollback succeeded.
     */
    static bool undoLastOperation(int *outRestoredCount = nullptr, QStringList *outErrors = nullptr);

    /**
     * @brief Extracts a meaningful internal title and real creation date from inside a file
     *        (PDF metadata/page 1, Office XML, Archive root folder, EXIF, Audio tags, or Text heading).
     * @param filePath Target file path.
     * @param outTitle Populated with sanitized human-readable title (empty if unavailable).
     * @param outDate Populated with internal creation/capture date (invalid if unavailable).
     */
    static void extractInternalMetadata(const QString &filePath, QString &outTitle, QDateTime &outDate);

    /**
     * @brief Formats a byte count into human-readable units (B, KB, MB, GB).
     */
    static QString formatBytes(qint64 bytes);

    /**
     * @brief Sanitizes a string so it is safe to use as a cross-platform filename stem.
     */
    static QString sanitizeFilenameStem(const QString &raw);
};
