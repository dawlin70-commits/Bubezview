/**
 * @file preload_cache_manager.h
 * @brief Thread-safe asynchronous pre-caching and memory budget management.
 */

#pragma once

#include <QObject>
#include <QImage>
#include <QCache>
#include <QMutex>
#include <QStringList>
#include <QThreadPool>
#include <QRunnable>
#include <memory>
#include <poppler/qt6/poppler-qt6.h>

/**
 * @class PreloadCacheManager
 * @brief Manages decoded image/document caching with strict RAM constraints and async prefetching.
 */
class PreloadCacheManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Singleton access to the global cache manager.
     * @return Reference to the singleton instance.
     */
    static PreloadCacheManager &instance();

    /**
     * @brief Configures maximum RAM budget allocated for pre-caching.
     * @param maxMegaBytes Megabytes allowed (0 to disable prefetching).
     */
    void setBudgetMb(int maxMegaBytes);

    /**
     * @brief Retrieves the currently active memory budget in MB.
     * @return Configured budget (0 if disabled).
     */
    int budgetMb() const;

    /**
     * @brief Configures how many sibling files to prefetch ahead and behind (0 to 3).
     * @param distance Number of files (0 = disabled).
     */
    void setPrefetchDistance(int distance);

    /**
     * @brief Retrieves current prefetch distance.
     * @return Distance (0..3).
     */
    int prefetchDistance() const;

    /**
     * @brief Checks whether caching and prefetching are enabled.
     * @return True if budget > 0; false if disabled.
     */
    bool isEnabled() const;

    /**
     * @brief Attempts to retrieve a pre-cached image from memory.
     * @param filePath Absolute path of the file.
     * @param outImage Destination QImage pointer if found.
     * @return True if cache hit; false if cache miss.
     */
    bool tryGet(const QString &filePath, QImage &outImage);

    /**
     * @brief Stores an already loaded image into the cache.
     * @param filePath Absolute path of the file.
     * @param image Decoded QImage to store.
     */
    void insert(const QString &filePath, const QImage &image);

    /**
     * @brief Takes a pre-parsed Poppler document if available.
     * @param filePath Absolute path of the PDF.
     * @return Unique pointer to Poppler::Document or nullptr if not cached.
     */
    std::unique_ptr<Poppler::Document> takeDocument(const QString &filePath);

    /**
     * @brief Checks if any PDF file is present in the sibling files list and warms up Poppler.
     * @param siblingFiles Ordered list of files in the current folder.
     */
    void inspectFolderForPdfs(const QStringList &siblingFiles);

    /**
     * @brief Prefetches adjacent files asynchronously in background threads.
     * @param siblingFiles Ordered list of files in the current folder.
     * @param currentIndex Index of currently viewed file.
     */
    void requestPrefetchNeighbors(const QStringList &siblingFiles, int currentIndex);

    /**
     * @brief Purges all cached items from memory immediately.
     */
    void clear();

signals:
    /**
     * @brief Emitted when a background prefetch completes for a given file.
     * @param filePath Path of the file that was prefetched.
     */
    void prefetchCompleted(const QString &filePath);

private:
    explicit PreloadCacheManager(QObject *parent = nullptr);
    ~PreloadCacheManager() override = default;

    Q_DISABLE_COPY_MOVE(PreloadCacheManager)

    mutable QMutex m_mutex;
    QCache<QString, QImage> m_cache;
    QMap<QString, std::shared_ptr<Poppler::Document>> m_docMap;
    int m_budgetMb;
    int m_prefetchDistance;
    bool m_popplerWarmedUp;
    QThreadPool m_threadPool;
};
