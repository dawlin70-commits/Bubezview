/**
 * @file folder_studio_engine.cpp
 * @brief Implementation of FolderStudioEngine multi-format algorithms.
 */

#include "folder_studio_engine.h"
#include "core/audio/audio_processor.h"
#include "core/services/exif_parser.h"
#include "core/services/plugin_manager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>
#include <QMutex>
#include <QMutexLocker>

#ifdef HAVE_POPPLER_QT6
#include <poppler/qt6/poppler-qt6.h>
#endif

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace {

// Serializes Poppler::Document::load/page calls across worker threads to avoid
// rare Fontconfig/Poppler internal races while keeping all hashing, image decoding,
// archive inspection, and text processing 100% lock-free across all CPU cores.
QMutex g_popplerMutex;

// Dynamic work-stealing parallel-for using all available hardware CPU threads.
template <typename Fn>
void parallelForIndices(int count, const std::atomic<bool> &cancelFlag, Fn &&fn)
{
    if (count <= 0) return;
    const unsigned int hw = std::thread::hardware_concurrency();
    const int numWorkers = qBound(1, hw > 0 ? static_cast<int>(hw) : 4, count);
    if (numWorkers <= 1) {
        for (int i = 0; i < count; ++i) {
            if (cancelFlag.load(std::memory_order_relaxed)) break;
            fn(i);
        }
        return;
    }

    std::atomic<int> nextIdx{0};
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(numWorkers));
    for (int t = 0; t < numWorkers; ++t) {
        pool.emplace_back([&nextIdx, count, &cancelFlag, &fn]() {
            while (!cancelFlag.load(std::memory_order_relaxed)) {
                const int idx = nextIdx.fetch_add(1, std::memory_order_relaxed);
                if (idx >= count) break;
                fn(idx);
            }
        });
    }
    for (std::thread &th : pool) {
        if (th.joinable()) th.join();
    }
}

QString undoManifestPath()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::homePath() + QStringLiteral("/.cache/BubezView");
    }
    QDir().mkpath(cacheDir);
    return cacheDir + QStringLiteral("/folder_studio_undo.json");
}

QStringList collectFiles(const QString &rootFolder, bool recursive, const std::atomic<bool> &cancelFlag)
{
    QStringList files;
    if (rootFolder.isEmpty() || !QDir(rootFolder).exists()) return files;

    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(rootFolder, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        if (cancelFlag.load(std::memory_order_relaxed)) break;
        const QString p = it.next();
        const QFileInfo fi(p);
        if (fi.isSymLink()) continue;
        // Skip hidden dot-files/folders inside the tree
        const QString rel = QDir(rootFolder).relativeFilePath(p);
        if (rel.startsWith(QLatin1Char('.')) || rel.contains(QStringLiteral("/."))) continue;
        files.append(p);
    }
    return files;
}

QByteArray computePartialHash(const QString &path, qint64 size)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    constexpr qint64 kEdge = 8192;
    if (size <= kEdge * 2) {
        h.addData(f.readAll());
    } else {
        h.addData(f.read(kEdge));
        f.seek(size - kEdge);
        h.addData(f.read(kEdge));
    }
    return h.result();
}

QByteArray computeFullHash(const QString &path, const std::atomic<bool> &cancelFlag)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    char buf[65536];
    while (!f.atEnd()) {
        if (cancelFlag.load(std::memory_order_relaxed)) return {};
        const qint64 n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        h.addData(QByteArray::fromRawData(buf, static_cast<int>(n)));
    }
    return h.result();
}

// Computes a 64-bit difference hash (dHash) on a 9x8 grayscale thumbnail for visual similarity
struct ImageSignature {
    quint64 dhash = 0;
    quint8 avgR = 0;
    quint8 avgG = 0;
    quint8 avgB = 0;
    int width = 0;
    int height = 0;
    double sharpness = 0.0;
    bool hasExif = false;
    bool valid = false;
};

ImageSignature analyzeImage(const QString &path)
{
    ImageSignature sig;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize origSz = reader.size();
    if (origSz.isValid()) {
        sig.width = origSz.width();
        sig.height = origSz.height();
    }
    // Decode a fast 160x160 working copy for sharpness + dHash without eating RAM
    if (origSz.isValid() && origSz.width() > 0 && origSz.height() > 0) {
        reader.setScaledSize(origSz.scaled(160, 160, Qt::KeepAspectRatio));
    }
    QImage img = reader.read();
    if (img.isNull()) return sig;

    if (sig.width <= 0 || sig.height <= 0) {
        sig.width = img.width();
        sig.height = img.height();
    }

    // 1. Compact RGB mean for color-coherence guard against false dHash matches
    QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    qint64 sumR = 0, sumG = 0, sumB = 0;
    const int totalPx = qMax(1, rgb.width() * rgb.height());
    for (int y = 0; y < rgb.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            sumR += qRed(line[x]);
            sumG += qGreen(line[x]);
            sumB += qBlue(line[x]);
        }
    }
    sig.avgR = static_cast<quint8>(sumR / totalPx);
    sig.avgG = static_cast<quint8>(sumG / totalPx);
    sig.avgB = static_cast<quint8>(sumB / totalPx);

    // 2. Focus / Sharpness via discrete 4-neighbor Laplacian variance on grayscale copy
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.width() >= 4 && gray.height() >= 4) {
        double sum = 0.0;
        double sumSq = 0.0;
        int count = 0;
        for (int y = 1; y < gray.height() - 1; ++y) {
            const uchar *prev = gray.constScanLine(y - 1);
            const uchar *curr = gray.constScanLine(y);
            const uchar *next = gray.constScanLine(y + 1);
            for (int x = 1; x < gray.width() - 1; ++x) {
                const double lap = -4.0 * curr[x] + prev[x] + next[x] + curr[x - 1] + curr[x + 1];
                sum += lap;
                sumSq += lap * lap;
                ++count;
            }
        }
        if (count > 0) {
            const double mean = sum / count;
            sig.sharpness = qMax(0.0, (sumSq / count) - mean * mean);
        }
    }

    // 3. 9x8 dHash (64 bits: compare horizontal adjacent luminance)
    QImage tiny = gray.scaled(9, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    quint64 hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        const uchar *row = tiny.constScanLine(y);
        for (int x = 0; x < 8; ++x) {
            if (row[x] > row[x + 1]) {
                hash |= (1ULL << bit);
            }
            ++bit;
        }
    }
    sig.dhash = hash;

    const ExifData exif = ExifParser::parseFile(path);
    sig.hasExif = exif.hasExif && (!exif.model.isEmpty() || !exif.dateTimeOriginal.isEmpty());
    sig.valid = true;
    return sig;
}

int hammingDistance64(quint64 a, quint64 b)
{
    quint64 x = a ^ b;
    int dist = 0;
    while (x) {
        x &= (x - 1);
        ++dist;
    }
    return dist;
}

// 64-bit SimHash over normalized alphanumeric tokens for near-duplicate document/text matching
quint64 computeTextSimHash(const QString &text, int *outWordCount = nullptr)
{
    int acc[64] = {0};
    static const QRegularExpression tokenRx(QStringLiteral(R"([a-zA-Z0-9\p{L}]{3,})"));
    auto it = tokenRx.globalMatch(text.toLower());
    int words = 0;
    while (it.hasNext() && words < 4000) {
        const QString tok = it.next().captured(0);
        // FNV-1a 64-bit hash of token
        quint64 h = 14695981039346656037ULL;
        const QByteArray utf8 = tok.toUtf8();
        for (unsigned char c : utf8) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        for (int b = 0; b < 64; ++b) {
            acc[b] += ((h >> b) & 1ULL) ? 1 : -1;
        }
        ++words;
    }
    if (outWordCount) *outWordCount = words;
    quint64 fp = 0;
    for (int b = 0; b < 64; ++b) {
        if (acc[b] > 0) fp |= (1ULL << b);
    }
    return fp;
}

QString extractDocumentTextSnippet(const QString &path, FileCategory cat, bool *outHasVectorText = nullptr)
{
    if (outHasVectorText) *outHasVectorText = false;
    if (cat == FileCategory::Text) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (outHasVectorText) *outHasVectorText = true;
            return QString::fromUtf8(f.read(65536));
        }
    }
#ifdef HAVE_POPPLER_QT6
    if (cat == FileCategory::Pdf) {
        QMutexLocker lock(&g_popplerMutex);
        auto doc = Poppler::Document::load(path);
        if (doc && !doc->isLocked()) {
            QString combined;
            const int pages = qMin(doc->numPages(), 3);
            for (int p = 0; p < pages; ++p) {
                auto page = doc->page(p);
                if (page) {
                    combined += page->text(QRectF()) + QLatin1Char('\n');
                }
            }
            if (!combined.trimmed().isEmpty() && outHasVectorText) {
                *outHasVectorText = true;
            }
            return combined;
        }
    }
#endif
    return QString();
}

// Peeks inside a ZIP-based container via libarchive to refine Office/EPUB magic type or find root folder name
struct ArchivePeekInfo {
    bool readable = false;
    int entryCount = 0;
    QString commonRootFolder;
    QString refinedOfficeExt; // "docx", "xlsx", "pptx", "odt", "ods", "odp", "epub"
    QString embeddedDocTitle;
};

ArchivePeekInfo peekArchive(const QString &path)
{
    ArchivePeekInfo info;
#ifdef HAVE_LIBARCHIVE
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (archive_read_open_filename(a, path.toLocal8Bit().constData(), 16384) == ARCHIVE_OK) {
        info.readable = true;
        struct archive_entry *entry = nullptr;
        QSet<QString> topRoots;
        bool hasWord = false, hasXl = false, hasPpt = false, hasContentXml = false, hasMimetype = false;
        QString mimetypeVal;

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK && info.entryCount < 500) {
            const char *rawName = archive_entry_pathname(entry);
            if (!rawName) continue;
            const QString name = QString::fromUtf8(rawName);
            ++info.entryCount;

            const QString clean = name.startsWith(QLatin1String("./")) ? name.mid(2) : name;
            const int slash = clean.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                topRoots.insert(clean.left(slash));
            } else if (!clean.isEmpty()) {
                topRoots.insert(clean);
            }

            if (clean == QLatin1String("word/document.xml")) hasWord = true;
            else if (clean == QLatin1String("xl/workbook.xml")) hasXl = true;
            else if (clean == QLatin1String("ppt/presentation.xml")) hasPpt = true;
            else if (clean == QLatin1String("content.xml")) hasContentXml = true;
            else if (clean == QLatin1String("mimetype")) {
                hasMimetype = true;
                char mbuf[128] = {0};
                const la_ssize_t n = archive_read_data(a, mbuf, sizeof(mbuf) - 1);
                if (n > 0) mimetypeVal = QString::fromUtf8(mbuf, static_cast<int>(n)).trimmed();
            } else if (clean == QLatin1String("docProps/core.xml") || clean == QLatin1String("meta.xml")) {
                QByteArray xmlBuf;
                char chunk[4096];
                la_ssize_t n = 0;
                while ((n = archive_read_data(a, chunk, sizeof(chunk))) > 0 && xmlBuf.size() < 32768) {
                    xmlBuf.append(chunk, static_cast<int>(n));
                }
                static const QRegularExpression titleRx(QStringLiteral(R"(<dc:title[^>]*>([^<]+)</dc:title>)"));
                const auto m = titleRx.match(QString::fromUtf8(xmlBuf));
                if (m.hasMatch()) {
                    info.embeddedDocTitle = m.captured(1).trimmed();
                }
            }
        }

        if (hasWord) info.refinedOfficeExt = QStringLiteral("docx");
        else if (hasXl) info.refinedOfficeExt = QStringLiteral("xlsx");
        else if (hasPpt) info.refinedOfficeExt = QStringLiteral("pptx");
        else if (hasMimetype && mimetypeVal.contains(QLatin1String("epub"))) info.refinedOfficeExt = QStringLiteral("epub");
        else if (hasMimetype && mimetypeVal.contains(QLatin1String("opendocument.text"))) info.refinedOfficeExt = QStringLiteral("odt");
        else if (hasMimetype && mimetypeVal.contains(QLatin1String("opendocument.spreadsheet"))) info.refinedOfficeExt = QStringLiteral("ods");
        else if (hasMimetype && mimetypeVal.contains(QLatin1String("opendocument.presentation"))) info.refinedOfficeExt = QStringLiteral("odp");
        else if (hasContentXml) info.refinedOfficeExt = QStringLiteral("odt");

        if (info.refinedOfficeExt.isEmpty() && topRoots.size() == 1) {
            const QString sole = *topRoots.begin();
            if (!sole.contains(QLatin1Char('.'))) {
                info.commonRootFolder = sole;
            }
        }
    }
    archive_read_free(a);
#else
    Q_UNUSED(path);
#endif
    return info;
}

QString categoryFolderName(FileCategory cat)
{
    switch (cat) {
    case FileCategory::Pdf:     return QStringLiteral("Documents_PDF");
    case FileCategory::Office:  return QStringLiteral("Office_Documents");
    case FileCategory::Image:   return QStringLiteral("Images");
    case FileCategory::Text:    return QStringLiteral("Text_and_Code");
    case FileCategory::Archive: return QStringLiteral("Archives");
    case FileCategory::Media:   return QStringLiteral("Audio_and_Video");
    default:                    return QStringLiteral("Other_Files");
    }
}

QString normalizeMediaStem(const QString &stem)
{
    QString s = stem.toLower();
    static const QRegularExpression noiseRx(QStringLiteral(R"(\b(1080p|720p|480p|4k|2160p|320kbps|128kbps|flac|mp3|copy|copia|final|v\d+)\b)"));
    s.remove(noiseRx);
    static const QRegularExpression sepRx(QStringLiteral(R"([^a-z0-9\p{L}]+)"));
    s.replace(sepRx, QStringLiteral(" "));
    return s.trimmed();
}

int mediaFormatQualityScore(const QString &ext, qint64 sizeBytes)
{
    int base = 50;
    if (ext == QLatin1String("flac") || ext == QLatin1String("wav") || ext == QLatin1String("alac")) base = 95;
    else if (ext == QLatin1String("mkv") || ext == QLatin1String("mov")) base = 85;
    else if (ext == QLatin1String("mp4") || ext == QLatin1String("webm") || ext == QLatin1String("opus")) base = 78;
    else if (ext == QLatin1String("m4a") || ext == QLatin1String("ogg")) base = 70;
    else if (ext == QLatin1String("mp3") || ext == QLatin1String("avi")) base = 60;
    // Slightly favor larger bitrate file when comparing identical formats
    const int sizeBonus = static_cast<int>(qMin<qint64>(10, sizeBytes / (2 * 1024 * 1024)));
    return qBound(0, base + sizeBonus, 100);
}

QString uniqueDestinationPath(const QString &desiredPath, QSet<QString> &reservedPaths)
{
    if (!QFileInfo::exists(desiredPath) && !reservedPaths.contains(desiredPath)) {
        reservedPaths.insert(desiredPath);
        return desiredPath;
    }
    const QFileInfo fi(desiredPath);
    const QString dir = fi.absolutePath();
    const QString stem = fi.completeBaseName();
    const QString ext = fi.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = ext.isEmpty()
            ? QStringLiteral("%1/%2_%3").arg(dir, stem).arg(i, 2, 10, QLatin1Char('0'))
            : QStringLiteral("%1/%2_%3.%4").arg(dir, stem).arg(i, 2, 10, QLatin1Char('0')).arg(ext);
        if (!QFileInfo::exists(candidate) && !reservedPaths.contains(candidate)) {
            reservedPaths.insert(candidate);
            return candidate;
        }
    }
    return desiredPath;
}

} // namespace

QString FolderStudioEngine::formatBytes(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024LL * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString FolderStudioEngine::sanitizeFilenameStem(const QString &raw)
{
    QString s = raw.trimmed();
    static const QRegularExpression badChars(QStringLiteral(R"([\\/:*?"<>|\r\n\t]+)"));
    s.replace(badChars, QStringLiteral("_"));
    static const QRegularExpression multiSpace(QStringLiteral(R"(\s+)"));
    s.replace(multiSpace, QStringLiteral("_"));
    static const QRegularExpression multiUnder(QStringLiteral(R"(_{2,})"));
    s.replace(multiUnder, QStringLiteral("_"));
    while (s.startsWith(QLatin1Char('_')) || s.startsWith(QLatin1Char('.'))) s.remove(0, 1);
    while (s.endsWith(QLatin1Char('_')) || s.endsWith(QLatin1Char('.'))) s.chop(1);
    if (s.length() > 64) s = s.left(64).trimmed();
    return s;
}

void FolderStudioEngine::extractInternalMetadata(const QString &filePath, QString &outTitle, QDateTime &outDate)
{
    outTitle.clear();
    outDate = QDateTime();

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) return;
    const FileCategory cat = PluginManager::categorizeFile(filePath);

    // 1. Images: EXIF capture date + camera model
    if (cat == FileCategory::Image) {
        const ExifData exif = ExifParser::parseFile(filePath);
        if (exif.hasExif) {
            if (!exif.dateTimeOriginal.isEmpty()) {
                QDateTime dt = QDateTime::fromString(exif.dateTimeOriginal.left(19), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
                if (!dt.isValid()) {
                    dt = QDateTime::fromString(exif.dateTimeOriginal.left(19), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                }
                if (dt.isValid()) outDate = dt;
            }
            if (!exif.model.isEmpty()) {
                outTitle = sanitizeFilenameStem(exif.model);
            }
        }
        return;
    }

    // 2. PDF: Poppler metadata title/creationDate or first clean heading line of page 1
#ifdef HAVE_POPPLER_QT6
    if (cat == FileCategory::Pdf) {
        QMutexLocker lock(&g_popplerMutex);
        auto doc = Poppler::Document::load(filePath);
        if (doc && !doc->isLocked()) {
            const QString metaTitle = doc->info(QStringLiteral("Title")).trimmed();
            if (metaTitle.length() >= 3 && metaTitle.length() <= 80 && !metaTitle.startsWith(QLatin1String("Microsoft Word"))) {
                outTitle = sanitizeFilenameStem(metaTitle);
            }
            const QDateTime cDate = doc->date(QStringLiteral("CreationDate"));
            if (cDate.isValid()) outDate = cDate;

            if (outTitle.isEmpty() && doc->numPages() > 0) {
                auto page = doc->page(0);
                if (page) {
                    const QStringList lines = page->text(QRectF()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    for (const QString &line : lines) {
                        const QString t = line.trimmed();
                        if (t.length() >= 4 && t.length() <= 72) {
                            outTitle = sanitizeFilenameStem(t);
                            break;
                        }
                    }
                }
            }
        }
        return;
    }
#endif

    // 3. Archives & Office ZIP containers (docx/xlsx/pptx/odt/epub)
    if (cat == FileCategory::Archive || cat == FileCategory::Office) {
        const ArchivePeekInfo peek = peekArchive(filePath);
        if (!peek.embeddedDocTitle.isEmpty()) {
            outTitle = sanitizeFilenameStem(peek.embeddedDocTitle);
        } else if (!peek.commonRootFolder.isEmpty()) {
            outTitle = sanitizeFilenameStem(peek.commonRootFolder);
        }
        return;
    }

    // 4. Text / Markdown / Code: extract first heading or descriptive line
    if (cat == FileCategory::Text) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            for (int i = 0; i < 12 && !in.atEnd(); ++i) {
                QString line = in.readLine(140).trimmed();
                if (line.startsWith(QLatin1String("#!")) || line.isEmpty()) continue;
                while (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('/')) || line.startsWith(QLatin1Char('*'))) {
                    line.remove(0, 1);
                }
                line = line.trimmed();
                if (line.length() >= 4) {
                    outTitle = sanitizeFilenameStem(line);
                    break;
                }
            }
        }
        return;
    }
}

QList<DuplicateGroup> FolderStudioEngine::scanDuplicates(const QString &rootFolder,
                                                         bool recursive,
                                                         bool includeExact,
                                                         bool includeDocuments,
                                                         bool includeMedia,
                                                         bool includeImages,
                                                         bool includeArchiveFolders,
                                                         int imageSimilarityPercent,
                                                         const std::atomic<bool> &cancelFlag,
                                                         const ProgressCallback &progress)
{
    QList<DuplicateGroup> groups;
    const QStringList allFiles = collectFiles(rootFolder, recursive, cancelFlag);
    if (allFiles.isEmpty() || cancelFlag.load(std::memory_order_relaxed)) {
        return groups;
    }

    const int totalFiles = allFiles.size();
    QSet<QString> alreadyGroupedExact;

    // -------------------------------------------------------------------------
    // STAGE 1: Exact Bit-for-Bit Duplicates (All File Types)
    // -------------------------------------------------------------------------
    if (includeExact) {
        if (progress) progress(0, totalFiles, QObject::tr("Grouping files by exact size..."));
        QHash<qint64, QStringList> bySize;
        for (const QString &p : allFiles) {
            if (cancelFlag.load(std::memory_order_relaxed)) return groups;
            const qint64 sz = QFileInfo(p).size();
            if (sz > 0) {
                bySize[sz].append(p);
            }
        }

        // Narrow down with 8KB head+tail hash across all CPU cores before full SHA-256
        struct SizeCandidate {
            QString path;
            qint64 size = 0;
        };
        QList<SizeCandidate> partialCandidates;
        for (auto it = bySize.constBegin(); it != bySize.constEnd(); ++it) {
            if (it.value().size() < 2) continue;
            for (const QString &p : it.value()) {
                partialCandidates.append({p, it.key()});
            }
        }

        QVector<QByteArray> partialHashes(partialCandidates.size());
        parallelForIndices(partialCandidates.size(), cancelFlag, [&](int i) {
            partialHashes[i] = computePartialHash(partialCandidates[i].path, partialCandidates[i].size);
        });
        if (cancelFlag.load(std::memory_order_relaxed)) return groups;

        QHash<QByteArray, QStringList> byPartial;
        for (int i = 0; i < partialCandidates.size(); ++i) {
            if (!partialHashes[i].isEmpty()) {
                // Include file size in key so partial hashes from different sizes never collide
                QByteArray key = QByteArray::number(partialCandidates[i].size) + ':' + partialHashes[i];
                byPartial[key].append(partialCandidates[i].path);
            }
        }

        QStringList fullCandidates;
        for (auto pit = byPartial.constBegin(); pit != byPartial.constEnd(); ++pit) {
            if (pit.value().size() >= 2) {
                fullCandidates.append(pit.value());
            }
        }

        QVector<QByteArray> fullHashes(fullCandidates.size());
        std::atomic<int> hashedCount{0};
        const int totalToHash = qMax(1, fullCandidates.size());
        QMutex progMutex;
        parallelForIndices(fullCandidates.size(), cancelFlag, [&](int i) {
            fullHashes[i] = computeFullHash(fullCandidates[i], cancelFlag);
            const int done = hashedCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress && (done % 4 == 0 || done == fullCandidates.size())) {
                QMutexLocker lock(&progMutex);
                progress(done, totalToHash, QObject::tr("Hashing exact duplicates..."));
            }
        });
        if (cancelFlag.load(std::memory_order_relaxed)) return groups;

        QHash<QByteArray, QStringList> byFullHash;
        for (int i = 0; i < fullCandidates.size(); ++i) {
            if (!fullHashes[i].isEmpty()) {
                byFullHash[fullHashes[i]].append(fullCandidates[i]);
            }
        }

        for (auto it = byFullHash.constBegin(); it != byFullHash.constEnd(); ++it) {
            if (it.value().size() < 2) continue;
            DuplicateGroup grp;
            grp.kind = DuplicateKind::Exact;
            const qint64 sz = QFileInfo(it.value().first()).size();
            grp.title = QObject::tr("Exact Duplicate (%1 files • %2 each)").arg(it.value().size()).arg(formatBytes(sz));
            grp.detail = QObject::tr("100% identical binary content (SHA-256 verified)");

            // Prefer the oldest file (earliest creation time) with a clean path (not copy/backup/recup)
            QVector<QDateTime> createdTimes;
            createdTimes.reserve(it.value().size());
            QDateTime earliestTime;
            for (int i = 0; i < it.value().size(); ++i) {
                const QFileInfo fi(it.value().at(i));
                QDateTime ct = fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();
                createdTimes.append(ct);
                if (ct.isValid() && (!earliestTime.isValid() || ct < earliestTime)) {
                    earliestTime = ct;
                }
            }

            int bestIdx = 0;
            int bestScore = -1;
            for (int i = 0; i < it.value().size(); ++i) {
                const QString &p = it.value().at(i);
                alreadyGroupedExact.insert(p);
                const QFileInfo fi(p);
                const QDateTime &ct = createdTimes.at(i);
                const QString dateStr = ct.isValid() ? ct.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString();

                int score = 80;
                if (p.contains(QLatin1String("recup_dir"), Qt::CaseInsensitive) ||
                    fi.fileName().contains(QLatin1String("copy"), Qt::CaseInsensitive) ||
                    fi.fileName().contains(QLatin1String("copia"), Qt::CaseInsensitive)) {
                    score -= 25;
                }
                if (ct.isValid() && earliestTime.isValid() && ct == earliestTime) {
                    // Reward earliest creation timestamp as the likely original file
                    score += 20;
                }
                score -= qMin(25, p.length() / 10);

                DuplicateItem item;
                item.path = p;
                item.fileName = fi.fileName();
                item.sizeBytes = sz;
                item.qualityScore = qBound(10, score, 100);
                item.qualitySummary = dateStr.isEmpty()
                    ? QObject::tr("Exact copy")
                    : QObject::tr("Created %1").arg(dateStr);
                grp.items.append(item);
                if (score > bestScore) {
                    bestScore = score;
                    bestIdx = i;
                }
            }
            grp.items[bestIdx].recommendedKeep = true;
            const QDateTime &bestCt = createdTimes.at(bestIdx);
            grp.items[bestIdx].qualitySummary = bestCt.isValid()
                ? QObject::tr("★ Original (Created %1)").arg(bestCt.toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                : QObject::tr("★ Recommended Keep");
            groups.append(grp);
        }
    }

    // -------------------------------------------------------------------------
    // STAGE 2: Document & Text Equivalents (PDF / Text / Code via SimHash)
    // -------------------------------------------------------------------------
    if (includeDocuments && !cancelFlag.load(std::memory_order_relaxed)) {
        if (progress) progress(0, totalFiles, QObject::tr("Analyzing document & text similarity..."));
        struct DocEntry {
            QString path;
            FileCategory cat;
            quint64 simhash = 0;
            int wordCount = 0;
            bool hasVectorText = false;
            qint64 sizeBytes = 0;
        };
        struct DocCandidate {
            QString path;
            FileCategory cat;
        };
        QList<DocCandidate> docCandidates;
        for (const QString &p : allFiles) {
            if (cancelFlag.load(std::memory_order_relaxed)) return groups;
            if (alreadyGroupedExact.contains(p)) continue;
            const FileCategory cat = PluginManager::categorizeFile(p);
            if (cat == FileCategory::Pdf || cat == FileCategory::Text) {
                docCandidates.append({p, cat});
            }
        }

        QVector<DocEntry> docRaw(docCandidates.size());
        parallelForIndices(docCandidates.size(), cancelFlag, [&](int i) {
            const DocCandidate &c = docCandidates[i];
            bool hasVec = false;
            const QString txt = extractDocumentTextSnippet(c.path, c.cat, &hasVec);
            int wc = 0;
            const quint64 sh = computeTextSimHash(txt, &wc);
            if (wc >= 12) {
                docRaw[i] = {c.path, c.cat, sh, wc, hasVec, QFileInfo(c.path).size()};
            }
        });
        if (cancelFlag.load(std::memory_order_relaxed)) return groups;

        QList<DocEntry> docs;
        for (const DocEntry &d : docRaw) {
            if (d.wordCount >= 12) docs.append(d);
        }

        QVector<bool> visited(docs.size(), false);
        for (int i = 0; i < docs.size(); ++i) {
            if (visited[i]) continue;
            QList<int> cluster;
            cluster.append(i);
            for (int j = i + 1; j < docs.size(); ++j) {
                if (visited[j]) continue;
                if (hammingDistance64(docs[i].simhash, docs[j].simhash) <= 5) {
                    visited[j] = true;
                    cluster.append(j);
                }
            }
            if (cluster.size() >= 2) {
                visited[i] = true;
                DuplicateGroup grp;
                grp.kind = DuplicateKind::DocumentText;
                grp.title = QObject::tr("Equivalent Documents (%1 files)").arg(cluster.size());
                grp.detail = QObject::tr("Same internal text content across different filenames or formats");

                int bestIdx = 0;
                int bestScore = -1;
                for (int k = 0; k < cluster.size(); ++k) {
                    const DocEntry &d = docs[cluster[k]];
                    const QFileInfo fi(d.path);
                    int score = 60 + (d.cat == FileCategory::Pdf ? 15 : 0) + (d.hasVectorText ? 15 : 0);
                    score += qMin(10, d.wordCount / 100);
                    DuplicateItem item;
                    item.path = d.path;
                    item.fileName = fi.fileName();
                    item.sizeBytes = d.sizeBytes;
                    item.qualityScore = qBound(0, score, 100);
                    item.qualitySummary = QStringLiteral("%1 • %2 words")
                        .arg(fi.suffix().toUpper())
                        .arg(d.wordCount);
                    grp.items.append(item);
                    if (score > bestScore) {
                        bestScore = score;
                        bestIdx = k;
                    }
                }
                grp.items[bestIdx].recommendedKeep = true;
                grp.items[bestIdx].qualitySummary = QObject::tr("★ Best (%1)").arg(grp.items[bestIdx].qualitySummary);
                groups.append(grp);
            }
        }
    }

    // -------------------------------------------------------------------------
    // STAGE 3: Audio & Video Equivalents Across Formats/Bitrates
    // -------------------------------------------------------------------------
    if (includeMedia && !cancelFlag.load(std::memory_order_relaxed)) {
        if (progress) progress(0, totalFiles, QObject::tr("Checking audio & video equivalents..."));
        QHash<QString, QStringList> mediaByStem;
        for (const QString &p : allFiles) {
            if (alreadyGroupedExact.contains(p)) continue;
            if (PluginManager::categorizeFile(p) != FileCategory::Media) continue;

            // For music/audio files, prefer embedded tags (artist + song title) when present so
            // differently named rips of the same song are still detected as duplicates.
            QString matchKey;
            const QFileInfo fi(p);
            const QString ext = fi.suffix().toLower();
            static const QSet<QString> audioExts = {
                QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"),
                QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
                QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("wma"),
                QStringLiteral("alac"), QStringLiteral("aiff"), QStringLiteral("wv"),
                QStringLiteral("ape")
            };
            if (audioExts.contains(ext)) {
                const AudioMetadata meta = AudioProcessor::readMetadata(p);
                const QString normTitle = normalizeMediaStem(meta.title);
                const QString normArtist = normalizeMediaStem(meta.artist);
                if (normTitle.length() >= 3) {
                    matchKey = normArtist.isEmpty() ? normTitle : (normArtist + QStringLiteral(" - ") + normTitle);
                }
            }
            if (matchKey.isEmpty()) {
                matchKey = normalizeMediaStem(fi.completeBaseName());
            }
            if (matchKey.length() >= 4) {
                mediaByStem[matchKey].append(p);
            }
        }
        for (auto it = mediaByStem.constBegin(); it != mediaByStem.constEnd(); ++it) {
            if (it.value().size() < 2) continue;
            DuplicateGroup grp;
            grp.kind = DuplicateKind::AudioVideo;
            grp.title = QObject::tr("Media Equivalents: %1 (%2 files)").arg(it.key()).arg(it.value().size());
            grp.detail = QObject::tr("Same track/video in multiple formats or bitrates");

            int bestIdx = 0;
            int bestScore = -1;
            for (int i = 0; i < it.value().size(); ++i) {
                const QFileInfo fi(it.value().at(i));
                const int score = mediaFormatQualityScore(fi.suffix().toLower(), fi.size());
                DuplicateItem item;
                item.path = fi.absoluteFilePath();
                item.fileName = fi.fileName();
                item.sizeBytes = fi.size();
                item.qualityScore = score;
                item.qualitySummary = QStringLiteral("%1 • %2").arg(fi.suffix().toUpper(), formatBytes(fi.size()));
                grp.items.append(item);
                if (score > bestScore) {
                    bestScore = score;
                    bestIdx = i;
                }
            }
            grp.items[bestIdx].recommendedKeep = true;
            grp.items[bestIdx].qualitySummary = QObject::tr("★ Highest Quality (%1)").arg(grp.items[bestIdx].qualitySummary);
            groups.append(grp);
        }
    }

    // -------------------------------------------------------------------------
    // STAGE 4: Redundant Archives Next to Extracted Folders
    // -------------------------------------------------------------------------
    if (includeArchiveFolders && !cancelFlag.load(std::memory_order_relaxed)) {
        for (const QString &p : allFiles) {
            if (PluginManager::categorizeFile(p) != FileCategory::Archive) continue;
            const QFileInfo fi(p);
            QString stem = fi.fileName();
            static const QStringList arcExts = {".tar.gz", ".tar.xz", ".tar.zst", ".tar.bz2", ".zip", ".7z", ".tar"};
            for (const QString &e : arcExts) {
                if (stem.endsWith(e, Qt::CaseInsensitive)) {
                    stem.chop(e.length());
                    break;
                }
            }
            const QString siblingDir = fi.absolutePath() + QLatin1Char('/') + stem;
            if (!stem.isEmpty() && QDir(siblingDir).exists()) {
                DuplicateGroup grp;
                grp.kind = DuplicateKind::ArchiveFolder;
                grp.title = QObject::tr("Archive Already Extracted: %1").arg(fi.fileName());
                grp.detail = QObject::tr("Both the compressed archive and its extracted folder exist side by side");

                DuplicateItem dirItem;
                dirItem.path = siblingDir;
                dirItem.fileName = stem + QLatin1Char('/');
                dirItem.sizeBytes = 0;
                dirItem.qualityScore = 90;
                dirItem.recommendedKeep = true;
                dirItem.qualitySummary = QObject::tr("★ Extracted Folder (ready to use)");

                DuplicateItem arcItem;
                arcItem.path = p;
                arcItem.fileName = fi.fileName();
                arcItem.sizeBytes = fi.size();
                arcItem.qualityScore = 70;
                arcItem.recommendedKeep = false;
                arcItem.qualitySummary = QObject::tr("Compressed Archive • %1").arg(formatBytes(fi.size()));

                grp.items << dirItem << arcItem;
                groups.append(grp);
            }
        }
    }

    // -------------------------------------------------------------------------
    // STAGE 5: Visually Similar Images + Best-Shot Sharpness/Resolution Scorer
    // -------------------------------------------------------------------------
    if (includeImages && !cancelFlag.load(std::memory_order_relaxed)) {
        QStringList imgPaths;
        for (const QString &p : allFiles) {
            if (alreadyGroupedExact.contains(p)) continue;
            if (PluginManager::categorizeFile(p) == FileCategory::Image) {
                imgPaths.append(p);
            }
        }

        if (imgPaths.size() >= 2) {
            struct ImgItem {
                QString path;
                ImageSignature sig;
                qint64 sizeBytes = 0;
            };
            QVector<ImgItem> imgRaw(imgPaths.size());
            std::atomic<int> imgDone{0};
            QMutex imgProgMutex;
            parallelForIndices(imgPaths.size(), cancelFlag, [&](int i) {
                const ImageSignature s = analyzeImage(imgPaths[i]);
                if (s.valid) {
                    imgRaw[i] = {imgPaths[i], s, QFileInfo(imgPaths[i]).size()};
                }
                const int done = imgDone.fetch_add(1, std::memory_order_relaxed) + 1;
                if (progress && (done % 4 == 0 || done == imgPaths.size())) {
                    QMutexLocker lock(&imgProgMutex);
                    progress(done, imgPaths.size(), QObject::tr("Computing visual fingerprints (%1/%2)...").arg(done).arg(imgPaths.size()));
                }
            });
            if (cancelFlag.load(std::memory_order_relaxed)) return groups;

            QList<ImgItem> imgs;
            imgs.reserve(imgPaths.size());
            for (const ImgItem &im : imgRaw) {
                if (im.sig.valid) imgs.append(im);
            }

            // Map similarity % (80..98) -> Hamming distance threshold (12..1)
            const int maxDist = qBound(1, qRound((100 - qBound(80, imageSimilarityPercent, 98)) * 0.64), 13);
            QVector<bool> visited(imgs.size(), false);
            for (int i = 0; i < imgs.size(); ++i) {
                if (visited[i]) continue;
                QList<int> cluster;
                cluster.append(i);
                for (int j = i + 1; j < imgs.size(); ++j) {
                    if (visited[j]) continue;
                    const int dist = hammingDistance64(imgs[i].sig.dhash, imgs[j].sig.dhash);
                    const int colorDiff = qAbs(int(imgs[i].sig.avgR) - int(imgs[j].sig.avgR)) +
                                          qAbs(int(imgs[i].sig.avgG) - int(imgs[j].sig.avgG)) +
                                          qAbs(int(imgs[i].sig.avgB) - int(imgs[j].sig.avgB));
                    if (dist <= maxDist && colorDiff <= 75) {
                        visited[j] = true;
                        cluster.append(j);
                    }
                }
                if (cluster.size() >= 2) {
                    visited[i] = true;
                    DuplicateGroup grp;
                    grp.kind = DuplicateKind::SimilarImage;
                    grp.title = QObject::tr("Similar Images (%1 shots)").arg(cluster.size());
                    grp.detail = QObject::tr("Visual similarity match — ranked by resolution, focus sharpness & EXIF");

                    // Normalize resolution and sharpness within this cluster
                    double maxPx = 1.0, maxSharp = 1.0;
                    for (int idx : cluster) {
                        maxPx = qMax(maxPx, double(imgs[idx].sig.width) * imgs[idx].sig.height);
                        maxSharp = qMax(maxSharp, imgs[idx].sig.sharpness);
                    }

                    int bestIdx = 0;
                    int bestScore = -1;
                    for (int k = 0; k < cluster.size(); ++k) {
                        const ImgItem &im = imgs[cluster[k]];
                        const double resNorm = (double(im.sig.width) * im.sig.height) / maxPx;
                        const double sharpNorm = im.sig.sharpness / maxSharp;
                        const QString ext = QFileInfo(im.path).suffix().toLower();
                        const int fmtBonus = (ext == QLatin1String("png") || ext == QLatin1String("tiff") || ext == QLatin1String("dng")) ? 10 : 0;
                        const int exifBonus = im.sig.hasExif ? 8 : 0;
                        const int score = qBound(1, qRound(resNorm * 42.0 + sharpNorm * 40.0 + fmtBonus + exifBonus), 100);

                        DuplicateItem item;
                        item.path = im.path;
                        item.fileName = QFileInfo(im.path).fileName();
                        item.sizeBytes = im.sizeBytes;
                        item.qualityScore = score;
                        const double mp = (double(im.sig.width) * im.sig.height) / 1000000.0;
                        QString notes = QStringLiteral("%1×%2 (%3 MP)").arg(im.sig.width).arg(im.sig.height).arg(mp, 0, 'f', 1);
                        if (sharpNorm < 0.65) notes += QObject::tr(" • Softer/Blurry");
                        if (im.sig.hasExif) notes += QStringLiteral(" • EXIF");
                        item.qualitySummary = notes;
                        grp.items.append(item);

                        if (score > bestScore) {
                            bestScore = score;
                            bestIdx = k;
                        }
                    }
                    grp.items[bestIdx].recommendedKeep = true;
                    grp.items[bestIdx].qualitySummary = QObject::tr("★ Best Shot • %1").arg(grp.items[bestIdx].qualitySummary);
                    groups.append(grp);
                }
            }
        }
    }

    return groups;
}

QList<RecoveryItem> FolderStudioEngine::scanRecovery(const QString &rootFolder,
                                                     bool recursive,
                                                     int junkMaxKb,
                                                     const std::atomic<bool> &cancelFlag,
                                                     const ProgressCallback &progress)
{
    QList<RecoveryItem> results;
    const QStringList files = collectFiles(rootFolder, recursive, cancelFlag);
    if (files.isEmpty() || cancelFlag.load(std::memory_order_relaxed)) {
        return results;
    }

    const qint64 junkMaxBytes = qMax<qint64>(1, junkMaxKb) * 1024;

    static const QSet<QString> junkExts = {
        QStringLiteral("thm"), QStringLiteral("tmp"), QStringLiteral("bak"),
        QStringLiteral("ini"), QStringLiteral("db"), QStringLiteral("ds_store")
    };
    static const QRegularExpression anonRx(QStringLiteral(R"(^[ft]\d{5,}$)"), QRegularExpression::CaseInsensitiveOption);

    QVector<RecoveryItem> out(files.size());
    QVector<bool> validMask(files.size(), false);
    std::atomic<int> inspectedCount{0};
    QMutex progMutex;

    parallelForIndices(files.size(), cancelFlag, [&](int i) {
        // Thread-local QMimeDatabase instance avoids cross-thread contention
        thread_local QMimeDatabase mimeDb;

        const QString &path = files[i];
        const QFileInfo fi(path);
        RecoveryItem rec;
        rec.path = path;
        rec.sizeBytes = fi.size();
        const QString curExt = fi.suffix().toLower();

        auto finishItem = [&]() {
            out[i] = std::move(rec);
            validMask[i] = true;
            const int done = inspectedCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress && (done % 10 == 0 || done == files.size())) {
                QMutexLocker lock(&progMutex);
                progress(done, files.size(), QObject::tr("Inspecting file headers & integrity (%1/%2)...").arg(done).arg(files.size()));
            }
        };

        // 1. 0-byte or obvious OS/recovery temp noise
        if (rec.sizeBytes == 0) {
            rec.status = RecoveryStatus::Junk;
            rec.reason = QObject::tr("Empty 0-byte file");
            finishItem();
            return;
        }
        if (junkExts.contains(curExt) || fi.fileName().compare(QLatin1String("Thumbs.db"), Qt::CaseInsensitive) == 0) {
            rec.status = RecoveryStatus::Junk;
            rec.reason = QObject::tr("Temporary / cache fragment");
            finishItem();
            return;
        }

        // 2. True Magic-Byte Content Detection
        const QMimeType contentMime = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchContent);
        rec.detectedMime = contentMime.name();
        rec.detectedExt = contentMime.preferredSuffix().toLower();

        // Refine ZIP magic into real Office/EPUB container + check archive integrity
        if (rec.detectedMime == QLatin1String("application/zip") || curExt == QLatin1String("zip") ||
            curExt == QLatin1String("docx") || curExt == QLatin1String("xlsx") || curExt == QLatin1String("odt") ||
            curExt == QLatin1String("7z") || curExt == QLatin1String("tar")) {
            const ArchivePeekInfo peek = peekArchive(path);
            if (!peek.readable) {
                rec.status = RecoveryStatus::Corrupt;
                rec.reason = QObject::tr("Corrupt or truncated archive header");
                finishItem();
                return;
            }
            if (!peek.refinedOfficeExt.isEmpty()) {
                rec.detectedExt = peek.refinedOfficeExt;
            }
        }

        // 3. Integrity verification by real content type
        if (rec.detectedMime.startsWith(QLatin1String("image/"))) {
            QImageReader reader(path);
            if (!reader.canRead() || !reader.size().isValid() || reader.size().width() <= 0) {
                rec.status = RecoveryStatus::Corrupt;
                rec.reason = QObject::tr("Corrupt or unreadable image data");
                finishItem();
                return;
            }
            if (rec.sizeBytes <= junkMaxBytes || (reader.size().width() < 96 && reader.size().height() < 96)) {
                rec.status = RecoveryStatus::Junk;
                rec.reason = QObject::tr("Tiny web/cache thumbnail (%1×%2, %3)")
                    .arg(reader.size().width()).arg(reader.size().height()).arg(formatBytes(rec.sizeBytes));
                finishItem();
                return;
            }
        }
#ifdef HAVE_POPPLER_QT6
        else if (rec.detectedMime == QLatin1String("application/pdf") || curExt == QLatin1String("pdf")) {
            QMutexLocker lock(&g_popplerMutex);
            auto doc = Poppler::Document::load(path);
            if (!doc || doc->numPages() <= 0) {
                rec.status = RecoveryStatus::Corrupt;
                rec.reason = QObject::tr("Damaged or unreadable PDF structure");
                finishItem();
                return;
            }
        }
#endif

        // 4. Extension mismatch check (normalize jpg/jpeg, tif/tiff, htm/html)
        auto normExt = [](const QString &e) {
            if (e == QLatin1String("jpeg")) return QStringLiteral("jpg");
            if (e == QLatin1String("tiff")) return QStringLiteral("tif");
            if (e == QLatin1String("htm"))  return QStringLiteral("html");
            return e;
        };
        const QString finalExt = !rec.detectedExt.isEmpty() ? normExt(rec.detectedExt) : normExt(curExt);
        if (!rec.detectedExt.isEmpty() && normExt(curExt) != finalExt &&
            rec.detectedMime != QLatin1String("application/octet-stream") &&
            rec.detectedMime != QLatin1String("text/plain")) {
            rec.status = RecoveryStatus::WrongExtension;
            rec.reason = QObject::tr("Detected as .%1 (currently .%2)").arg(finalExt, curExt.isEmpty() ? QStringLiteral("none") : curExt);
        } else {
            rec.status = RecoveryStatus::Healthy;
            rec.reason = QObject::tr("Verified intact");
        }

        // 5. Smart Name Reconstruction from internal content/metadata
        extractInternalMetadata(path, rec.internalTitle, rec.internalDate);
        const QString datePrefix = rec.internalDate.isValid()
            ? rec.internalDate.toString(QStringLiteral("yyyy-MM-dd"))
            : QString();

        // Detect PhotoRec-like anonymous names (e.g. f1234567.pdf, t009182.jpg)
        const bool isAnon = anonRx.match(fi.completeBaseName()).hasMatch();

        if (!rec.internalTitle.isEmpty() || (isAnon && !datePrefix.isEmpty()) || rec.status == RecoveryStatus::WrongExtension) {
            QString stem;
            if (!datePrefix.isEmpty() && !rec.internalTitle.isEmpty()) {
                stem = datePrefix + QLatin1Char('_') + rec.internalTitle;
            } else if (!rec.internalTitle.isEmpty()) {
                stem = rec.internalTitle;
            } else if (!datePrefix.isEmpty() && isAnon) {
                stem = datePrefix + QLatin1Char('_') + fi.completeBaseName();
            } else {
                stem = fi.completeBaseName();
            }
            rec.suggestedName = finalExt.isEmpty() ? stem : (stem + QLatin1Char('.') + finalExt);
        } else {
            rec.suggestedName = fi.fileName();
        }

        finishItem();
    });

    results.reserve(files.size());
    for (int i = 0; i < files.size(); ++i) {
        if (validMask[i]) results.append(std::move(out[i]));
    }
    return results;
}

QList<MovePlanItem> FolderStudioEngine::planTreeOrganization(const QString &rootFolder,
                                                             OrganizeMode mode,
                                                             bool recursive,
                                                             const std::atomic<bool> &cancelFlag)
{
    QList<MovePlanItem> plan;
    const QStringList files = collectFiles(rootFolder, recursive, cancelFlag);
    QSet<QString> reserved;

    for (const QString &src : files) {
        if (cancelFlag.load(std::memory_order_relaxed)) break;
        const QFileInfo fi(src);
        QString targetSubDir;
        QString reason;

        if (mode == OrganizeMode::Flatten) {
            targetSubDir = QString();
            reason = QObject::tr("Flatten to root folder");
        } else {
            const FileCategory cat = PluginManager::categorizeFile(src);
            const QString catName = categoryFolderName(cat);
            QString title;
            QDateTime dt;
            if (mode == OrganizeMode::ByYearMonth || mode == OrganizeMode::ByCategoryAndYear) {
                extractInternalMetadata(src, title, dt);
                if (!dt.isValid()) dt = fi.lastModified();
            }

            if (mode == OrganizeMode::ByCategory) {
                targetSubDir = catName;
                reason = QObject::tr("Category: %1").arg(catName);
            } else if (mode == OrganizeMode::ByYearMonth) {
                targetSubDir = dt.toString(QStringLiteral("yyyy/yyyy-MM"));
                reason = QObject::tr("Date: %1").arg(targetSubDir);
            } else { // ByCategoryAndYear
                targetSubDir = catName + QLatin1Char('/') + dt.toString(QStringLiteral("yyyy"));
                reason = QObject::tr("%1 (%2)").arg(catName, dt.toString(QStringLiteral("yyyy")));
            }
        }

        const QString destDir = targetSubDir.isEmpty() ? rootFolder : (rootFolder + QLatin1Char('/') + targetSubDir);
        const QString desiredDest = destDir + QLatin1Char('/') + fi.fileName();
        if (QDir::cleanPath(desiredDest) == QDir::cleanPath(src)) {
            continue; // Already in exact destination
        }

        const QString uniqueDest = uniqueDestinationPath(desiredDest, reserved);
        plan.append({src, uniqueDest, reason});
    }

    return plan;
}

QList<MovePlanItem> FolderStudioEngine::planBatchRename(const QString &rootFolder,
                                                        bool recursive,
                                                        const QString &pattern,
                                                        const QString &findText,
                                                        const QString &replaceText,
                                                        bool useRegex,
                                                        const std::atomic<bool> &cancelFlag)
{
    QList<MovePlanItem> plan;
    QStringList files = collectFiles(rootFolder, recursive, cancelFlag);
    files.sort(Qt::CaseInsensitive);
    QSet<QString> reserved;

    QRegularExpression rx;
    if (useRegex && !findText.isEmpty()) {
        rx = QRegularExpression(findText);
    }

    int counter = 1;
    for (const QString &src : files) {
        if (cancelFlag.load(std::memory_order_relaxed)) break;
        const QFileInfo fi(src);
        const QString origStem = fi.completeBaseName();
        const QString ext = fi.suffix();

        QString newStem = origStem;
        if (!pattern.trimmed().isEmpty()) {
            newStem = pattern;
            QString internalTitle;
            QDateTime internalDate;
            if (newStem.contains(QLatin1String("{title}")) || newStem.contains(QLatin1String("{date}"))) {
                extractInternalMetadata(src, internalTitle, internalDate);
            }
            if (!internalDate.isValid()) internalDate = fi.lastModified();

            const QString cleanStem = sanitizeFilenameStem(origStem);
            const QString catName = categoryFolderName(PluginManager::categorizeFile(src));

            newStem.replace(QLatin1String("{name}"), origStem);
            newStem.replace(QLatin1String("{clean}"), cleanStem);
            newStem.replace(QLatin1String("{title}"), internalTitle.isEmpty() ? cleanStem : internalTitle);
            newStem.replace(QLatin1String("{date}"), internalDate.toString(QStringLiteral("yyyy-MM-dd")));
            newStem.replace(QLatin1String("{type}"), catName);
            newStem.replace(QLatin1String("{counter:03}"), QStringLiteral("%1").arg(counter, 3, 10, QLatin1Char('0')));
            newStem.replace(QLatin1String("{counter:02}"), QStringLiteral("%1").arg(counter, 2, 10, QLatin1Char('0')));
            newStem.replace(QLatin1String("{counter}"), QString::number(counter));
            newStem.replace(QLatin1String("{ext}"), ext);
        }

        if (!findText.isEmpty()) {
            if (useRegex) {
                if (rx.isValid()) {
                    newStem.replace(rx, replaceText);
                }
            } else {
                newStem.replace(findText, replaceText);
            }
        }

        newStem = sanitizeFilenameStem(newStem);
        if (newStem.isEmpty()) newStem = origStem;

        const QString newFileName = ext.isEmpty() ? newStem : (newStem + QLatin1Char('.') + ext);
        const QString desiredDest = fi.absolutePath() + QLatin1Char('/') + newFileName;
        ++counter;

        if (QDir::cleanPath(desiredDest) == QDir::cleanPath(src)) {
            continue;
        }
        const QString uniqueDest = uniqueDestinationPath(desiredDest, reserved);
        plan.append({src, uniqueDest, QObject::tr("Batch rename")});
    }

    return plan;
}

QList<CleanupItem> FolderStudioEngine::scanCleanup(const QString &rootFolder,
                                                   bool recursive,
                                                   const std::atomic<bool> &cancelFlag)
{
    QList<CleanupItem> items;
    if (rootFolder.isEmpty() || !QDir(rootFolder).exists()) return items;

    static const QSet<QString> junkNames = {
        QStringLiteral("thumbs.db"), QStringLiteral(".ds_store"), QStringLiteral("desktop.ini")
    };
    static const QSet<QString> junkSuffixes = {
        QStringLiteral("tmp"), QStringLiteral("bak"), QStringLiteral("swp"), QStringLiteral("swo")
    };

    const QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(rootFolder, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, flags);
    QStringList subDirs;
    while (it.hasNext()) {
        if (cancelFlag.load(std::memory_order_relaxed)) return items;
        const QString p = it.next();
        const QFileInfo fi(p);

        if (fi.isSymLink() && !QFileInfo::exists(fi.symLinkTarget())) {
            items.append({p, CleanupKind::BrokenSymlink, 0});
            continue;
        }
        if (fi.isDir()) {
            subDirs.append(p);
            continue;
        }
        const QString lowerName = fi.fileName().toLower();
        const QString lowerExt = fi.suffix().toLower();
        if (fi.size() == 0) {
            items.append({p, CleanupKind::ZeroByteFile, 0});
        } else if (junkNames.contains(lowerName) || junkSuffixes.contains(lowerExt) || lowerName.endsWith(QLatin1Char('~'))) {
            items.append({p, CleanupKind::TempJunk, fi.size()});
        }
    }

    // Check empty directories deepest-first
    std::sort(subDirs.begin(), subDirs.end(), [](const QString &a, const QString &b) {
        return a.length() > b.length();
    });
    for (const QString &d : subDirs) {
        if (QDir(d).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty()) {
            items.append({d, CleanupKind::EmptyDir, 0});
        }
    }

    return items;
}

bool FolderStudioEngine::executeMovePlan(const QList<MovePlanItem> &plan,
                                         const QString &rootFolder,
                                         bool pruneEmptyDirs,
                                         bool saveUndoManifest,
                                         int *outAppliedCount,
                                         QStringList *outErrors)
{
    int applied = 0;
    QJsonArray undoArray;
    QSet<QString> reserved;

    for (const MovePlanItem &item : plan) {
        if (item.sourcePath.isEmpty() || item.destPath.isEmpty()) continue;
        if (QDir::cleanPath(item.sourcePath) == QDir::cleanPath(item.destPath)) continue;

        // Guard against runtime collisions (e.g. duplicate recovery names or disk changes after Dry-Run)
        const QString safeDest = uniqueDestinationPath(item.destPath, reserved);
        const QFileInfo destFi(safeDest);
        QDir().mkpath(destFi.absolutePath());

        if (QFile::rename(item.sourcePath, safeDest)) {
            ++applied;
            if (saveUndoManifest) {
                QJsonObject obj;
                obj.insert(QStringLiteral("from"), item.sourcePath);
                obj.insert(QStringLiteral("to"), safeDest);
                undoArray.append(obj);
            }
        } else if (outErrors) {
            outErrors->append(QObject::tr("Failed to move '%1' -> '%2'").arg(item.sourcePath, safeDest));
        }
    }

    if (saveUndoManifest && !undoArray.isEmpty()) {
        QFile f(undoManifestPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(undoArray).toJson(QJsonDocument::Compact));
        }
    }

    if (pruneEmptyDirs && !rootFolder.isEmpty()) {
        QStringList dirs;
        QDirIterator it(rootFolder, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) dirs.append(it.next());
        std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
            return a.length() > b.length();
        });
        for (const QString &d : dirs) {
            QDir dir(d);
            if (dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty()) {
                dir.rmdir(d);
            }
        }
    }

    if (outAppliedCount) *outAppliedCount = applied;
    return applied > 0;
}

bool FolderStudioEngine::trashItems(const QStringList &paths, int *outTrashedCount, QStringList *outErrors)
{
    int count = 0;
    for (const QString &p : paths) {
        if (p.isEmpty()) continue;
        const QFileInfo fi(p);
        if (QFile::moveToTrash(p)) {
            ++count;
        } else if (fi.isSymLink() && QFile::remove(p)) {
            // Broken symlinks may fail moveToTrash() on some platforms; unlink directly as fallback
            ++count;
        } else if (fi.isDir() && QDir(p).rmdir(p)) {
            ++count;
        } else if (outErrors) {
            outErrors->append(QObject::tr("Could not trash '%1'").arg(p));
        }
    }
    if (outTrashedCount) *outTrashedCount = count;
    return count == paths.size();
}

bool FolderStudioEngine::canUndoLastOperation()
{
    const QFileInfo fi(undoManifestPath());
    return fi.exists() && fi.size() > 2;
}

bool FolderStudioEngine::undoLastOperation(int *outRestoredCount, QStringList *outErrors)
{
    QFile f(undoManifestPath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return false;

    const QJsonArray arr = doc.array();
    int restored = 0;
    QJsonArray remaining;

    // Restore in reverse order; preserve any failed entries in the manifest so the user can retry
    for (int i = arr.size() - 1; i >= 0; --i) {
        const QJsonObject obj = arr.at(i).toObject();
        const QString origFrom = obj.value(QStringLiteral("from")).toString();
        const QString movedTo = obj.value(QStringLiteral("to")).toString();
        if (origFrom.isEmpty() || movedTo.isEmpty() || !QFileInfo::exists(movedTo)) continue;

        QDir().mkpath(QFileInfo(origFrom).absolutePath());
        if (QFile::rename(movedTo, origFrom)) {
            ++restored;
        } else {
            remaining.prepend(obj);
            if (outErrors) {
                outErrors->append(QObject::tr("Could not restore '%1'").arg(origFrom));
            }
        }
    }

    if (remaining.isEmpty()) {
        QFile::remove(undoManifestPath());
    } else {
        QFile outF(undoManifestPath());
        if (outF.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            outF.write(QJsonDocument(remaining).toJson(QJsonDocument::Compact));
        }
    }
    if (outRestoredCount) *outRestoredCount = restored;
    return restored > 0;
}
