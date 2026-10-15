/**
 * @file preload_cache_manager.cpp
 * @brief Implementation of PreloadCacheManager.
 */

#include "preload_cache_manager.h"
#include "plugin_manager.h"
#include <QImageReader>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>

namespace {
/**
 * @class PreloadWorker
 * @brief Background task decodes candidate adjacent files and injects into cache.
 */
class PreloadWorker : public QRunnable {
public:
    explicit PreloadWorker(const QString &filePath)
        : m_filePath(filePath)
    {
        setAutoDelete(true);
    }

    void run() override {
        if (m_filePath.isEmpty()) return;

        FileCategory cat = PluginManager::categorizeFile(m_filePath);
        QImage loadedImage;

        if (cat == FileCategory::Image) {
            QImageReader reader(m_filePath);
            reader.setAutoTransform(true);
            loadedImage = reader.read();
        }

        if (!loadedImage.isNull()) {
            PreloadCacheManager::instance().insert(m_filePath, loadedImage);
        }
    }

private:
    QString m_filePath;
};
}

PreloadCacheManager &PreloadCacheManager::instance()
{
    static PreloadCacheManager s_instance;
    return s_instance;
}

PreloadCacheManager::PreloadCacheManager(QObject *parent)
    : QObject(parent)
    , m_budgetMb(256)
    , m_prefetchDistance(1)
    , m_popplerWarmedUp(false)
{
    // Limit pool concurrency to prevent background thread starvation
    m_threadPool.setMaxThreadCount(2);

    QSettings settings("Bubez", "BubezView");
    // Default cache budget: 256 MB (recommended for modern Linux workstations)
    int savedBudget = settings.value("cache_budget_mb", 256).toInt();
    setBudgetMb(savedBudget);

    m_prefetchDistance = settings.value("prefetch_distance", 1).toInt();
}

void PreloadCacheManager::setBudgetMb(int maxMegaBytes)
{
    QMutexLocker locker(&m_mutex);
    m_budgetMb = maxMegaBytes;
    if (m_budgetMb <= 0) {
        m_cache.clear();
        m_cache.setMaxCost(0);
    } else {
        // Convert MB to bytes for precise RAM accounting
        qsizetype maxBytes = static_cast<qsizetype>(m_budgetMb) * 1024 * 1024;
        m_cache.setMaxCost(maxBytes);
    }
}

int PreloadCacheManager::budgetMb() const
{
    QMutexLocker locker(&m_mutex);
    return m_budgetMb;
}

void PreloadCacheManager::setPrefetchDistance(int distance)
{
    QMutexLocker locker(&m_mutex);
    m_prefetchDistance = qBound(0, distance, 3);
    QSettings settings("Bubez", "BubezView");
    settings.setValue("prefetch_distance", m_prefetchDistance);
}

int PreloadCacheManager::prefetchDistance() const
{
    QMutexLocker locker(&m_mutex);
    return m_prefetchDistance;
}

bool PreloadCacheManager::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_budgetMb > 0 && m_prefetchDistance > 0;
}

bool PreloadCacheManager::tryGet(const QString &filePath, QImage &outImage)
{
    QMutexLocker locker(&m_mutex);
    if (m_budgetMb <= 0) {
        return false;
    }

    QImage *cached = m_cache.object(filePath);
    if (cached && !cached->isNull()) {
        outImage = *cached;
        return true;
    }
    return false;
}

void PreloadCacheManager::insert(const QString &filePath, const QImage &image)
{
    if (image.isNull()) return;

    QMutexLocker locker(&m_mutex);
    if (m_budgetMb <= 0) return;

    qsizetype costBytes = image.sizeInBytes();
    // Insert heap-allocated copy into QCache with actual memory size as cost
    m_cache.insert(filePath, new QImage(image), costBytes);
}

void PreloadCacheManager::inspectFolderForPdfs(const QStringList &siblingFiles)
{
    Q_UNUSED(siblingFiles);
}

void PreloadCacheManager::requestPrefetchNeighbors(const QStringList &siblingFiles, int currentIndex)
{
    if (!isEnabled() || siblingFiles.isEmpty() || m_prefetchDistance <= 0) return;

    QStringList candidates;
    for (int step = 1; step <= m_prefetchDistance; ++step) {
        // Next file (N + step)
        if (currentIndex + step < siblingFiles.size()) {
            candidates.append(siblingFiles.at(currentIndex + step));
        }
        // Previous file (N - step)
        if (currentIndex - step >= 0) {
            candidates.append(siblingFiles.at(currentIndex - step));
        }
    }

    for (const QString &path : candidates) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_cache.contains(path)) {
                continue; // Already prefetched
            }
        }

        FileCategory cat = PluginManager::categorizeFile(path);
        if (cat == FileCategory::Image || cat == FileCategory::Pdf) {
            m_threadPool.start(new PreloadWorker(path), QThread::LowPriority);
        }
    }
}

void PreloadCacheManager::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}
