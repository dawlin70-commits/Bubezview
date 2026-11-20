/**
 * @file folder_studio_widget.cpp
 * @brief Implementation of FolderStudioWidget: multi-format Duplicates, PhotoRec/TestDisk Recovery,
 *        Smart Tree Organizer, and Batch Rename / Cleanup tools with async scanning and Undo rollback.
 */

#include "folder_studio_widget.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLayout>
#include <QHeaderView>
#include <QListView>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QThreadPool>
#include <QPointer>
#include <QMetaObject>
#include <QRegularExpression>
#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QStyle>
#include <QSet>

#ifdef HAVE_POPPLER_QT6
#include <poppler/qt6/poppler-qt6.h>
#endif

// =============================================================================
// StudioFlowLayout — Responsive wrapping card grid (FolderViewer-like feel)
// =============================================================================
class StudioFlowLayout : public QLayout {
public:
    explicit StudioFlowLayout(QWidget *parent = nullptr, int margin = 0, int hSpacing = 12, int vSpacing = 12)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~StudioFlowLayout() override
    {
        QLayoutItem *item = nullptr;
        while ((item = takeAt(0)) != nullptr) {
            delete item;
        }
    }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem *takeAt(int index) override
    {
        return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
    }

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }

    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override { return minimumSize(); }

    QSize minimumSize() const override
    {
        QSize size;
        for (const QLayoutItem *item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins m = contentsMargins();
        size += QSize(m.left() + m.right(), m.top() + m.bottom());
        return size;
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        const QMargins m = contentsMargins();
        const QRect effective = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = effective.x();
        int y = effective.y();
        int lineHeight = 0;

        for (QLayoutItem *item : m_items) {
            const QSize hint = item->sizeHint();
            int nextX = x + hint.width() + m_hSpace;
            if (nextX - m_hSpace > effective.right() + 1 && lineHeight > 0) {
                x = effective.x();
                y += lineHeight + m_vSpace;
                nextX = x + hint.width() + m_hSpace;
                lineHeight = 0;
            }
            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), hint));
            }
            x = nextX;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + m.bottom();
    }

    QList<QLayoutItem *> m_items;
    int m_hSpace = 12;
    int m_vSpace = 12;
};

// =============================================================================
// StudioFileCard — Elevated thumbnail card mirroring FolderViewer aesthetics
// =============================================================================
class StudioFileCard : public QFrame {
public:
    StudioFileCard(const QString &filePath,
                   const QString &displayTitle,
                   const QString &badgeText,
                   const QString &badgeTone,
                   const QString &metaLine,
                   const QString &subLine,
                   bool initiallyChecked,
                   const QString &tooltipText,
                   std::function<void()> onDoubleClick,
                   QWidget *parent = nullptr)
        : QFrame(parent)
        , m_filePath(filePath)
        , m_onDoubleClick(std::move(onDoubleClick))
    {
        setObjectName(QStringLiteral("StudioFileCard"));
        setFixedSize(196, 240);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tooltipText);
        setProperty("cardTone", badgeTone);
        setProperty("cardChecked", initiallyChecked);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 8, 10, 8);
        lay->setSpacing(6);

        // Top bar: action checkbox on the left + status/recommendation pill on the right
        auto *topBar = new QHBoxLayout();
        topBar->setContentsMargins(0, 0, 0, 0);
        topBar->setSpacing(6);

        m_checkBox = new QCheckBox(this);
        m_checkBox->setChecked(initiallyChecked);
        m_checkBox->setCursor(Qt::PointingHandCursor);
        connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
            setProperty("cardChecked", checked);
            style()->unpolish(this);
            style()->polish(this);
            update();
        });
        topBar->addWidget(m_checkBox);
        topBar->addStretch(1);

        auto *badge = new QLabel(badgeText, this);
        badge->setObjectName(QStringLiteral("StudioBadgePill"));
        badge->setProperty("tone", badgeTone);
        badge->setAlignment(Qt::AlignCenter);
        topBar->addWidget(badge);
        lay->addLayout(topBar);

        // Thumbnail preview box (matches FolderViewer elevated preview frame)
        m_thumbLabel = new QLabel(this);
        m_thumbLabel->setObjectName(QStringLiteral("StudioThumbBox"));
        m_thumbLabel->setFixedSize(176, 116);
        m_thumbLabel->setAlignment(Qt::AlignCenter);
        const QString ext = QFileInfo(filePath).suffix().toUpper();
        m_thumbLabel->setText(ext.isEmpty() ? QStringLiteral("FILE") : ext);
        lay->addWidget(m_thumbLabel, 0, Qt::AlignHCenter);

        // Filename (2 lines max, clean eliding)
        auto *titleLbl = new QLabel(displayTitle, this);
        titleLbl->setObjectName(QStringLiteral("StudioCardTitle"));
        titleLbl->setWordWrap(true);
        titleLbl->setMaximumHeight(34);
        titleLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        lay->addWidget(titleLbl);

        // Size & quality/format details
        auto *metaLbl = new QLabel(metaLine, this);
        metaLbl->setObjectName(QStringLiteral("StudioCardMeta"));
        metaLbl->setWordWrap(false);
        const QFontMetrics fm(metaLbl->font());
        metaLbl->setText(fm.elidedText(metaLine, Qt::ElideRight, 176));
        lay->addWidget(metaLbl);

        if (!subLine.isEmpty()) {
            auto *subLbl = new QLabel(subLine, this);
            subLbl->setObjectName(QStringLiteral("StudioCardSub"));
            subLbl->setWordWrap(false);
            const QFontMetrics sfm(subLbl->font());
            subLbl->setText(sfm.elidedText(subLine, Qt::ElideMiddle, 176));
            lay->addWidget(subLbl);
        }

        lay->addStretch(1);
    }

    QString filePath() const { return m_filePath; }
    bool isChecked() const { return m_checkBox && m_checkBox->isChecked(); }
    QCheckBox *checkBox() const { return m_checkBox; }
    void setCheckedSilent(bool checked)
    {
        if (!m_checkBox) return;
        QSignalBlocker blocker(m_checkBox);
        m_checkBox->setChecked(checked);
    }

    void setThumbnailPixmap(const QPixmap &pix)
    {
        if (!m_thumbLabel || pix.isNull()) return;
        m_thumbLabel->setText(QString());
        m_thumbLabel->setPixmap(pix.scaled(m_thumbLabel->size() - QSize(6, 6),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_onDoubleClick) {
            // Avoid triggering preview if the user rapidly double-clicked the checkbox
            if (m_checkBox && m_checkBox->geometry().contains(event->pos())) {
                QFrame::mouseDoubleClickEvent(event);
                return;
            }
            m_onDoubleClick();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    QString m_filePath;
    QCheckBox *m_checkBox = nullptr;
    QLabel *m_thumbLabel = nullptr;
    std::function<void()> m_onDoubleClick;
};

namespace {

/**
 * @brief Renders a crisp visual thumbnail off the GUI thread for Folder Studio cards.
 *
 * Supports raster images (with EXIF auto-orientation), PDF first-page previews via Poppler,
 * micro-text paper previews for text/code files, and clean styled type badges for other formats.
 */
QImage renderStudioThumbnail(const QString &filePath, const QSize &targetSize)
{
    const QFileInfo fi(filePath);
    if (!fi.exists()) return QImage();

    // Directory card icon
    if (fi.isDir()) {
        QImage img(targetSize, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF tab(36, 22, 42, 16);
        const QRectF body(30, 34, 112, 64);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(56, 189, 248, 220));
        p.drawRoundedRect(tab, 5, 5);
        p.setBrush(QColor(2, 132, 199, 235));
        p.drawRoundedRect(body, 9, 9);
        p.setPen(Qt::white);
        QFont f;
        f.setBold(true);
        f.setPointSize(9);
        p.setFont(f);
        p.drawText(body, Qt::AlignCenter, QStringLiteral("FOLDER"));
        return img;
    }

    const QString ext = fi.suffix().toLower();

    // 1. Raster images via QImageReader (fast scaled decode + EXIF rotation)
    static const QSet<QString> imageExts = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp"),
        QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("ico"),
        QStringLiteral("svg")
    };
    if (imageExts.contains(ext)) {
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        if (reader.canRead()) {
            const QSize orig = reader.size();
            if (orig.isValid() && (orig.width() > targetSize.width() * 2 || orig.height() > targetSize.height() * 2)) {
                reader.setScaledSize(orig.scaled(targetSize * 2, Qt::KeepAspectRatio));
            }
            QImage img = reader.read();
            if (!img.isNull()) {
                return img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }

#ifdef HAVE_POPPLER_QT6
    // 2. PDF first-page render
    if (ext == QLatin1String("pdf")) {
        std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(filePath);
        if (doc && !doc->isLocked() && doc->numPages() > 0) {
            std::unique_ptr<Poppler::Page> page = doc->page(0);
            if (page) {
                const QSizeF pt = page->pageSizeF();
                if (pt.width() > 0 && pt.height() > 0) {
                    const double scaleX = (targetSize.width() * 72.0) / pt.width();
                    const double scaleY = (targetSize.height() * 72.0) / pt.height();
                    const double dpi = qBound(18.0, qMin(scaleX, scaleY), 72.0);
                    QImage pdfImg = page->renderToImage(dpi, dpi);
                    if (!pdfImg.isNull()) {
                        return pdfImg.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                }
            }
        }
    }
#endif

    // 3. Text / code micro-document sheet preview
    static const QSet<QString> textExts = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("json"), QStringLiteral("xml"),
        QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("ini"), QStringLiteral("cfg"),
        QStringLiteral("csv"), QStringLiteral("log"), QStringLiteral("cpp"), QStringLiteral("h"),
        QStringLiteral("hpp"), QStringLiteral("c"), QStringLiteral("py"), QStringLiteral("js"),
        QStringLiteral("ts"), QStringLiteral("html"), QStringLiteral("css"), QStringLiteral("sh"),
        QStringLiteral("rs"), QStringLiteral("go"), QStringLiteral("java"), QStringLiteral("cmake")
    };
    if (textExts.contains(ext)) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QStringList lines;
            QTextStream in(&f);
            while (!in.atEnd() && lines.size() < 20) {
                lines.append(in.readLine(100));
            }
            QImage sheet(targetSize, QImage::Format_ARGB32_Premultiplied);
            sheet.fill(Qt::transparent);
            QPainter p(&sheet);
            p.setRenderHint(QPainter::Antialiasing, true);
            const QRectF paper(34, 6, targetSize.width() - 68, targetSize.height() - 12);
            p.setBrush(QColor(250, 250, 252));
            p.setPen(QPen(QColor(203, 213, 225), 1.0));
            p.drawRoundedRect(paper, 6, 6);

            QFont mf(QStringLiteral("Monospace"));
            mf.setPixelSize(6);
            p.setFont(mf);
            p.setPen(QColor(51, 65, 85));
            int y = static_cast<int>(paper.top()) + 12;
            for (const QString &ln : lines) {
                if (y > paper.bottom() - 6) break;
                p.drawText(QRectF(paper.left() + 6, y - 6, paper.width() - 12, 8),
                           Qt::AlignLeft | Qt::AlignVCenter, ln.left(34));
                y += 6;
            }
            return sheet;
        }
    }

    // 4. Clean styled type badge for audio, video, archives, office, and other binaries
    QColor accent(100, 116, 139);
    QString label = ext.isEmpty() ? QStringLiteral("FILE") : ext.toUpper().left(6);
    static const QSet<QString> audioExts = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"), QStringLiteral("ogg"),
        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("opus"), QStringLiteral("wma")
    };
    static const QSet<QString> videoExts = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("mov"),
        QStringLiteral("webm"), QStringLiteral("wmv"), QStringLiteral("flv"), QStringLiteral("m4v")
    };
    static const QSet<QString> archiveExts = {
        QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("7z"),
        QStringLiteral("rar"), QStringLiteral("xz"), QStringLiteral("bz2"), QStringLiteral("zst")
    };
    if (audioExts.contains(ext)) accent = QColor(168, 85, 247);
    else if (videoExts.contains(ext)) accent = QColor(236, 72, 153);
    else if (archiveExts.contains(ext)) accent = QColor(245, 158, 11);
    else if (ext == QLatin1String("pdf")) accent = QColor(239, 68, 68);

    QImage badge(targetSize, QImage::Format_ARGB32_Premultiplied);
    badge.fill(Qt::transparent);
    QPainter p(&badge);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF pill(46, 30, targetSize.width() - 92, targetSize.height() - 60);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawRoundedRect(pill, 10, 10);
    p.setPen(Qt::white);
    QFont bf;
    bf.setBold(true);
    bf.setPointSize(11);
    p.setFont(bf);
    p.drawText(pill, Qt::AlignCenter, label);
    return badge;
}

} // namespace

FolderStudioWidget::FolderStudioWidget(const QString &folderPath, QWidget *parent)
    : IFolderStudioWidget(parent)
    , m_aliveFlag(std::make_shared<std::atomic<bool>>(true))
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
    , m_thumbGeneration(std::make_shared<std::atomic<uint64_t>>(0))
{
    setObjectName(QStringLiteral("FolderStudioWidget"));
    setupUi();
    setTargetFolder(folderPath);
    updateTheme();
}

FolderStudioWidget::~FolderStudioWidget()
{
    // Mark widget destroyed before signalling workers so queued GUI callbacks bail out safely
    if (m_aliveFlag) {
        m_aliveFlag->store(false, std::memory_order_release);
    }
    if (m_cancelFlag) {
        m_cancelFlag->store(true, std::memory_order_relaxed);
    }
    if (m_thumbGeneration) {
        m_thumbGeneration->fetch_add(1, std::memory_order_relaxed);
    }
}

void FolderStudioWidget::setTargetFolder(const QString &folderPath)
{
    QString canonical = QDir(folderPath).canonicalPath();
    if (canonical.isEmpty()) {
        canonical = folderPath;
    }

    // Preserve active scan results when returning from file preview to the exact same folder
    const bool sameFolder = (!m_currentFolder.isEmpty() && canonical == m_currentFolder);
    if (sameFolder) {
        if (m_lblFolderContext) {
            const QString name = QFileInfo(m_currentFolder).fileName();
            m_lblFolderContext->setText(tr("Target: %1").arg(name.isEmpty() ? m_currentFolder : name));
            m_lblFolderContext->setToolTip(m_currentFolder);
        }
        refreshUndoButtons();
        return;
    }

    cancelActiveOperations();
    m_currentFolder = canonical;
    m_thumbCache.clear();
    m_activeCards.clear();
    if (m_thumbGeneration) {
        m_thumbGeneration->fetch_add(1, std::memory_order_relaxed);
    }

    if (m_lblFolderContext) {
        const QString name = QFileInfo(m_currentFolder).fileName();
        m_lblFolderContext->setText(tr("Target: %1").arg(name.isEmpty() ? m_currentFolder : name));
        m_lblFolderContext->setToolTip(m_currentFolder);
    }

    // Reset stale results from any previously visited folder
    clearDuplicatesGallery();
    clearRecoveryGallery();
    if (m_tableOrganize) m_tableOrganize->setRowCount(0);
    if (m_tableRename) m_tableRename->setRowCount(0);
    if (m_tableCleanup) m_tableCleanup->setRowCount(0);
    m_duplicateGroups.clear();
    m_recoveryItems.clear();
    m_organizePlan.clear();
    m_renamePlan.clear();
    m_cleanupItems.clear();

    if (m_btnDupTrash) m_btnDupTrash->setEnabled(false);
    if (m_btnRecFix) m_btnRecFix->setEnabled(false);
    if (m_btnRecTrash) m_btnRecTrash->setEnabled(false);
    if (m_btnOrgApply) m_btnOrgApply->setEnabled(false);
    if (m_btnRenameApply) m_btnRenameApply->setEnabled(false);
    if (m_btnCleanTrash) m_btnCleanTrash->setEnabled(false);

    if (m_lblRecSummary) {
        m_lblRecSummary->setText(tr("Run analysis to inspect real file signatures (magic bytes), detect corrupted dumps, and recover names."));
    }
    if (m_lblOrgSummary) {
        m_lblOrgSummary->setText(tr("Preview a Dry-Run organization plan before moving any files on disk."));
    }
    if (m_lblStatus) {
        m_lblStatus->setText(tr("Ready"));
    }
    refreshUndoButtons();
}

void FolderStudioWidget::setActiveTool(FolderStudioTool tool)
{
    m_activeTool = tool;
    if (m_stack) {
        const int idx = qBound(0, static_cast<int>(tool), m_stack->count() - 1);
        m_stack->setCurrentIndex(idx);
    }
    refreshUndoButtons();
}

void FolderStudioWidget::cancelActiveOperations()
{
    if (m_cancelFlag) {
        m_cancelFlag->store(true, std::memory_order_relaxed);
    }
    if (m_isBusy) {
        setBusy(false, tr("Cancelled"));
    }
}

void FolderStudioWidget::setOnFolderModified(std::function<void()> cb)
{
    m_onFolderModified = std::move(cb);
}

void FolderStudioWidget::setOnPreviewFileRequested(std::function<void(const QString &, const QStringList &)> cb)
{
    m_onPreviewFileRequested = std::move(cb);
}

void FolderStudioWidget::notifyFileRemoved(const QString &filePath)
{
    const QString targetAbs = QFileInfo(filePath).absoluteFilePath();
    if (targetAbs.isEmpty()) return;

    m_thumbCache.remove(filePath);
    m_thumbCache.remove(targetAbs);

    // 1. Prune from cached Duplicates groups
    bool dupChanged = false;
    for (int g = m_duplicateGroups.size() - 1; g >= 0; --g) {
        auto &items = m_duplicateGroups[g].items;
        for (int i = items.size() - 1; i >= 0; --i) {
            if (QFileInfo(items.at(i).path).absoluteFilePath() == targetAbs) {
                items.removeAt(i);
                dupChanged = true;
            }
        }
        // Drop groups that no longer contain at least 2 competing items
        if (items.size() < 2) {
            m_duplicateGroups.removeAt(g);
            dupChanged = true;
        }
    }
    if (dupChanged) {
        populateDuplicatesGallery(m_duplicateGroups);
        if (m_lblStatus) {
            m_lblStatus->setText(m_duplicateGroups.isEmpty()
                ? tr("All duplicate groups resolved.")
                : tr("Updated: %1 duplicate/equivalent group(s) remaining.").arg(m_duplicateGroups.size()));
        }
    }

    // 2. Prune from cached Recovery items
    bool recChanged = false;
    for (int i = m_recoveryItems.size() - 1; i >= 0; --i) {
        if (QFileInfo(m_recoveryItems.at(i).path).absoluteFilePath() == targetAbs) {
            m_recoveryItems.removeAt(i);
            recChanged = true;
        }
    }
    if (recChanged) {
        populateRecoveryGallery(m_recoveryItems);
    }
}

void FolderStudioWidget::setupUi()
{
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(16, 10, 16, 14);
    rootLay->setSpacing(10);

    // Top shared status & async progress strip
    m_statusBanner = new QFrame(this);
    m_statusBanner->setObjectName(QStringLiteral("StudioStatusBanner"));
    m_statusBanner->setFixedHeight(38);
    auto *statusLay = new QHBoxLayout(m_statusBanner);
    statusLay->setContentsMargins(12, 4, 12, 4);
    statusLay->setSpacing(10);

    m_lblFolderContext = new QLabel(tr("Target: -"), m_statusBanner);
    m_lblFolderContext->setObjectName(QStringLiteral("StudioContextLabel"));
    statusLay->addWidget(m_lblFolderContext);

    m_lblStatus = new QLabel(tr("Ready"), m_statusBanner);
    m_lblStatus->setObjectName(QStringLiteral("StudioStatusLabel"));
    statusLay->addWidget(m_lblStatus, 1);

    m_progressBar = new QProgressBar(m_statusBanner);
    m_progressBar->setFixedWidth(180);
    m_progressBar->setFixedHeight(10);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    statusLay->addWidget(m_progressBar);

    m_btnCancelScan = new QPushButton(tr("Cancel"), m_statusBanner);
    m_btnCancelScan->setObjectName(QStringLiteral("StudioDangerBtn"));
    m_btnCancelScan->setCursor(Qt::PointingHandCursor);
    m_btnCancelScan->setFixedHeight(26);
    m_btnCancelScan->setVisible(false);
    connect(m_btnCancelScan, &QPushButton::clicked, this, &FolderStudioWidget::cancelActiveOperations);
    statusLay->addWidget(m_btnCancelScan);

    rootLay->addWidget(m_statusBanner);

    // Main 4-tool workspace stack
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildDuplicatesPage()); // 0: Duplicates
    m_stack->addWidget(buildRecoveryPage());   // 1: Recovery
    m_stack->addWidget(buildOrganizerPage());  // 2: TreeOrganizer
    m_stack->addWidget(buildBatchPage());      // 3: BatchTools
    rootLay->addWidget(m_stack, 1);
}

QWidget *FolderStudioWidget::buildDuplicatesPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    // Filter options & action controls card
    auto *ctrlFrame = new QFrame(page);
    ctrlFrame->setObjectName(QStringLiteral("StudioCard"));
    auto *ctrlLay = new QVBoxLayout(ctrlFrame);
    ctrlLay->setContentsMargins(14, 10, 14, 10);
    ctrlLay->setSpacing(8);

    auto *checksRow = new QHBoxLayout();
    checksRow->setSpacing(14);
    m_chkDupExact = new QCheckBox(tr("Exact (All Formats)"), ctrlFrame);
    m_chkDupExact->setChecked(true);
    m_chkDupDocs = new QCheckBox(tr("Documents (PDF / Text)"), ctrlFrame);
    m_chkDupDocs->setChecked(true);
    m_chkDupMedia = new QCheckBox(tr("Audio / Video"), ctrlFrame);
    m_chkDupMedia->setChecked(true);
    m_chkDupImages = new QCheckBox(tr("Similar Images"), ctrlFrame);
    m_chkDupImages->setChecked(true);
    m_chkDupArchive = new QCheckBox(tr("Archive vs Folder"), ctrlFrame);
    m_chkDupArchive->setChecked(true);
    m_chkDupRecursive = new QCheckBox(tr("Subfolders"), ctrlFrame);
    m_chkDupRecursive->setChecked(true);

    checksRow->addWidget(m_chkDupExact);
    checksRow->addWidget(m_chkDupDocs);
    checksRow->addWidget(m_chkDupMedia);
    checksRow->addWidget(m_chkDupImages);
    checksRow->addWidget(m_chkDupArchive);
    checksRow->addWidget(m_chkDupRecursive);
    checksRow->addStretch(1);
    ctrlLay->addLayout(checksRow);

    auto *actionsRow = new QHBoxLayout();
    actionsRow->setSpacing(10);

    auto *lblSim = new QLabel(tr("Image similarity:"), ctrlFrame);
    actionsRow->addWidget(lblSim);

    m_sliderImgSim = new QSlider(Qt::Horizontal, ctrlFrame);
    m_sliderImgSim->setRange(80, 98);
    m_sliderImgSim->setValue(90);
    m_sliderImgSim->setFixedWidth(130);
    actionsRow->addWidget(m_sliderImgSim);

    m_lblImgSimVal = new QLabel(QStringLiteral("90%"), ctrlFrame);
    m_lblImgSimVal->setFixedWidth(36);
    connect(m_sliderImgSim, &QSlider::valueChanged, this, [this](int v) {
        if (m_lblImgSimVal) m_lblImgSimVal->setText(QStringLiteral("%1%").arg(v));
    });
    actionsRow->addWidget(m_lblImgSimVal);

    actionsRow->addStretch(1);

    m_btnDupScan = new QPushButton(tr("Scan Duplicates & Equivalents"), ctrlFrame);
    m_btnDupScan->setObjectName(QStringLiteral("StudioPrimaryBtn"));
    m_btnDupScan->setCursor(Qt::PointingHandCursor);
    m_btnDupScan->setFixedHeight(32);
    connect(m_btnDupScan, &QPushButton::clicked, this, &FolderStudioWidget::startDuplicatesScan);
    actionsRow->addWidget(m_btnDupScan);

    m_btnDupTrash = new QPushButton(tr("Trash Unchecked Duplicates"), ctrlFrame);
    m_btnDupTrash->setObjectName(QStringLiteral("StudioDangerBtn"));
    m_btnDupTrash->setCursor(Qt::PointingHandCursor);
    m_btnDupTrash->setFixedHeight(32);
    m_btnDupTrash->setEnabled(false);
    connect(m_btnDupTrash, &QPushButton::clicked, this, &FolderStudioWidget::trashCheckedDuplicates);
    actionsRow->addWidget(m_btnDupTrash);

    ctrlLay->addLayout(actionsRow);
    lay->addWidget(ctrlFrame);

    // Visual grouped thumbnail gallery (FolderViewer-style cards grouped by duplicate cluster)
    m_dupScrollArea = new QScrollArea(page);
    m_dupScrollArea->setObjectName(QStringLiteral("StudioGalleryScroll"));
    m_dupScrollArea->setWidgetResizable(true);
    m_dupScrollArea->setFrameShape(QFrame::NoFrame);
    m_dupScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_dupContainer = new QWidget(m_dupScrollArea);
    m_dupContainer->setObjectName(QStringLiteral("StudioGalleryCanvas"));
    auto *dupContLay = new QVBoxLayout(m_dupContainer);
    dupContLay->setContentsMargins(6, 6, 6, 12);
    dupContLay->setSpacing(12);

    m_lblDupEmpty = new QLabel(
        tr("Click \"Scan Duplicates & Equivalents\" to visually compare exact copies, similar images, documents, media, and archives."),
        m_dupContainer);
    m_lblDupEmpty->setObjectName(QStringLiteral("StudioEmptyHint"));
    m_lblDupEmpty->setAlignment(Qt::AlignCenter);
    m_lblDupEmpty->setWordWrap(true);
    dupContLay->addWidget(m_lblDupEmpty, 1);

    m_dupScrollArea->setWidget(m_dupContainer);
    lay->addWidget(m_dupScrollArea, 1);
    return page;
}

QWidget *FolderStudioWidget::buildRecoveryPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    auto *ctrlFrame = new QFrame(page);
    ctrlFrame->setObjectName(QStringLiteral("StudioCard"));
    auto *ctrlLay = new QVBoxLayout(ctrlFrame);
    ctrlLay->setContentsMargins(14, 10, 14, 10);
    ctrlLay->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    m_chkRecRecursive = new QCheckBox(tr("Include Subfolders (e.g. recup_dir.*)"), ctrlFrame);
    m_chkRecRecursive->setChecked(true);
    topRow->addWidget(m_chkRecRecursive);

    auto *lblJunk = new QLabel(tr("Tiny junk threshold:"), ctrlFrame);
    topRow->addWidget(lblJunk);

    m_spinJunkKb = new QSpinBox(ctrlFrame);
    m_spinJunkKb->setRange(0, 4096);
    m_spinJunkKb->setValue(15);
    m_spinJunkKb->setSuffix(tr(" KB"));
    m_spinJunkKb->setFixedWidth(92);
    topRow->addWidget(m_spinJunkKb);

    topRow->addStretch(1);

    m_btnRecScan = new QPushButton(tr("Analyze Recovery Folder"), ctrlFrame);
    m_btnRecScan->setObjectName(QStringLiteral("StudioPrimaryBtn"));
    m_btnRecScan->setCursor(Qt::PointingHandCursor);
    m_btnRecScan->setFixedHeight(32);
    connect(m_btnRecScan, &QPushButton::clicked, this, &FolderStudioWidget::startRecoveryScan);
    topRow->addWidget(m_btnRecScan);

    m_btnRecFix = new QPushButton(tr("Fix Extensions && Smart Rename"), ctrlFrame);
    m_btnRecFix->setObjectName(QStringLiteral("StudioSecondaryBtn"));
    m_btnRecFix->setCursor(Qt::PointingHandCursor);
    m_btnRecFix->setFixedHeight(32);
    m_btnRecFix->setEnabled(false);
    connect(m_btnRecFix, &QPushButton::clicked, this, &FolderStudioWidget::applyRecoveryRenames);
    topRow->addWidget(m_btnRecFix);

    m_btnRecTrash = new QPushButton(tr("Trash Junk && Corrupt"), ctrlFrame);
    m_btnRecTrash->setObjectName(QStringLiteral("StudioDangerBtn"));
    m_btnRecTrash->setCursor(Qt::PointingHandCursor);
    m_btnRecTrash->setFixedHeight(32);
    m_btnRecTrash->setEnabled(false);
    connect(m_btnRecTrash, &QPushButton::clicked, this, &FolderStudioWidget::trashRecoveryJunkAndCorrupt);
    topRow->addWidget(m_btnRecTrash);

    m_btnRecUndo = new QPushButton(tr("Undo Last"), ctrlFrame);
    m_btnRecUndo->setObjectName(QStringLiteral("StudioSecondaryBtn"));
    m_btnRecUndo->setCursor(Qt::PointingHandCursor);
    m_btnRecUndo->setFixedHeight(32);
    connect(m_btnRecUndo, &QPushButton::clicked, this, &FolderStudioWidget::triggerUndoLastOperation);
    topRow->addWidget(m_btnRecUndo);

    ctrlLay->addLayout(topRow);

    m_lblRecSummary = new QLabel(
        tr("Run analysis to inspect real file signatures (magic bytes), detect corrupted dumps, and recover names."),
        ctrlFrame);
    m_lblRecSummary->setObjectName(QStringLiteral("StudioHintLabel"));
    m_lblRecSummary->setWordWrap(true);
    ctrlLay->addWidget(m_lblRecSummary);

    lay->addWidget(ctrlFrame);

    // Visual thumbnail gallery for recovered files (makes PhotoRec dumps immediately recognizable)
    m_recScrollArea = new QScrollArea(page);
    m_recScrollArea->setObjectName(QStringLiteral("StudioGalleryScroll"));
    m_recScrollArea->setWidgetResizable(true);
    m_recScrollArea->setFrameShape(QFrame::NoFrame);
    m_recScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_recContainer = new QWidget(m_recScrollArea);
    m_recContainer->setObjectName(QStringLiteral("StudioGalleryCanvas"));
    auto *recRootLay = new QVBoxLayout(m_recContainer);
    recRootLay->setContentsMargins(6, 6, 6, 12);
    recRootLay->setSpacing(10);

    m_lblRecEmpty = new QLabel(
        tr("Click \"Analyze Recovery Folder\" to visually inspect recovered files, verify signatures, and filter corrupt or junk dumps."),
        m_recContainer);
    m_lblRecEmpty->setObjectName(QStringLiteral("StudioEmptyHint"));
    m_lblRecEmpty->setAlignment(Qt::AlignCenter);
    m_lblRecEmpty->setWordWrap(true);
    recRootLay->addWidget(m_lblRecEmpty);

    auto *flowHost = new QWidget(m_recContainer);
    flowHost->setObjectName(QStringLiteral("StudioGalleryCanvas"));
    m_recFlowLayout = new StudioFlowLayout(flowHost, 4, 12, 12);
    recRootLay->addWidget(flowHost);
    recRootLay->addStretch(1);

    m_recScrollArea->setWidget(m_recContainer);
    lay->addWidget(m_recScrollArea, 1);
    return page;
}

QWidget *FolderStudioWidget::buildOrganizerPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    auto *ctrlFrame = new QFrame(page);
    ctrlFrame->setObjectName(QStringLiteral("StudioCard"));
    auto *ctrlLay = new QVBoxLayout(ctrlFrame);
    ctrlLay->setContentsMargins(14, 10, 14, 10);
    ctrlLay->setSpacing(8);

    auto *row = new QHBoxLayout();
    row->setSpacing(10);

    row->addWidget(new QLabel(tr("Strategy:"), ctrlFrame));

    m_comboOrgMode = new QComboBox(ctrlFrame);
    m_comboOrgMode->setView(new QListView(m_comboOrgMode));
    m_comboOrgMode->addItem(tr("By Category (Documents, Office, Images, Audio, Video, Archives, Code)"), static_cast<int>(OrganizeMode::ByCategory));
    m_comboOrgMode->addItem(tr("By Year / Month (YYYY/YYYY-MM from metadata or date)"), static_cast<int>(OrganizeMode::ByYearMonth));
    m_comboOrgMode->addItem(tr("By Category && Year (Category/YYYY)"), static_cast<int>(OrganizeMode::ByCategoryAndYear));
    m_comboOrgMode->addItem(tr("Flatten Nested Folders into Root (ideal for PhotoRec dumps)"), static_cast<int>(OrganizeMode::Flatten));
    m_comboOrgMode->setMinimumWidth(340);
    m_comboOrgMode->setFixedHeight(32);
    row->addWidget(m_comboOrgMode, 1);

    m_chkOrgRecursive = new QCheckBox(tr("Include Subfolders"), ctrlFrame);
    m_chkOrgRecursive->setChecked(true);
    row->addWidget(m_chkOrgRecursive);

    m_btnOrgPreview = new QPushButton(tr("Preview Plan (Dry-Run)"), ctrlFrame);
    m_btnOrgPreview->setObjectName(QStringLiteral("StudioPrimaryBtn"));
    m_btnOrgPreview->setCursor(Qt::PointingHandCursor);
    m_btnOrgPreview->setFixedHeight(32);
    connect(m_btnOrgPreview, &QPushButton::clicked, this, &FolderStudioWidget::startOrganizePreview);
    row->addWidget(m_btnOrgPreview);

    m_btnOrgApply = new QPushButton(tr("Apply Organization"), ctrlFrame);
    m_btnOrgApply->setObjectName(QStringLiteral("StudioSuccessBtn"));
    m_btnOrgApply->setCursor(Qt::PointingHandCursor);
    m_btnOrgApply->setFixedHeight(32);
    m_btnOrgApply->setEnabled(false);
    connect(m_btnOrgApply, &QPushButton::clicked, this, &FolderStudioWidget::applyOrganizePlan);
    row->addWidget(m_btnOrgApply);

    m_btnOrgUndo = new QPushButton(tr("Undo Last"), ctrlFrame);
    m_btnOrgUndo->setObjectName(QStringLiteral("StudioSecondaryBtn"));
    m_btnOrgUndo->setCursor(Qt::PointingHandCursor);
    m_btnOrgUndo->setFixedHeight(32);
    connect(m_btnOrgUndo, &QPushButton::clicked, this, &FolderStudioWidget::triggerUndoLastOperation);
    row->addWidget(m_btnOrgUndo);

    ctrlLay->addLayout(row);

    m_lblOrgSummary = new QLabel(
        tr("Preview a Dry-Run organization plan before moving any files on disk."),
        ctrlFrame);
    m_lblOrgSummary->setObjectName(QStringLiteral("StudioHintLabel"));
    ctrlLay->addWidget(m_lblOrgSummary);

    lay->addWidget(ctrlFrame);

    m_tableOrganize = new QTableWidget(0, 3, page);
    m_tableOrganize->setHorizontalHeaderLabels({
        tr("Current Location"), tr("Planned Destination"), tr("Rule / Reason")
    });
    m_tableOrganize->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableOrganize->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableOrganize->setAlternatingRowColors(true);
    m_tableOrganize->verticalHeader()->setVisible(false);
    m_tableOrganize->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableOrganize->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableOrganize->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(m_tableOrganize, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_organizePlan.size() && m_onPreviewFileRequested) {
            QStringList contextPaths;
            contextPaths.reserve(m_organizePlan.size());
            for (const MovePlanItem &m : m_organizePlan) contextPaths.append(m.sourcePath);
            m_onPreviewFileRequested(m_organizePlan.at(row).sourcePath, contextPaths);
        }
    });

    lay->addWidget(m_tableOrganize, 1);
    return page;
}

QWidget *FolderStudioWidget::buildBatchPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_batchTabs = new QTabWidget(page);

    // --- Sub-tab 1: Smart Batch Rename ---
    auto *renameTab = new QWidget(m_batchTabs);
    auto *renLay = new QVBoxLayout(renameTab);
    renLay->setContentsMargins(10, 10, 10, 10);
    renLay->setSpacing(10);

    auto *renCard = new QFrame(renameTab);
    renCard->setObjectName(QStringLiteral("StudioCard"));
    auto *renGrid = new QGridLayout(renCard);
    renGrid->setContentsMargins(14, 10, 14, 10);
    renGrid->setHorizontalSpacing(10);
    renGrid->setVerticalSpacing(8);

    renGrid->addWidget(new QLabel(tr("Pattern:"), renCard), 0, 0);
    m_editRenamePattern = new QLineEdit(renCard);
    m_editRenamePattern->setPlaceholderText(tr("e.g. {date}_{title} or {clean}_{counter:03} (leave empty to keep stem)"));
    m_editRenamePattern->setFixedHeight(30);
    renGrid->addWidget(m_editRenamePattern, 0, 1, 1, 4);

    // Clickable token chips to compose rename patterns effortlessly
    auto *chipRow = new QHBoxLayout();
    chipRow->setSpacing(6);
    chipRow->addWidget(new QLabel(tr("Insert token:"), renCard));
    const QStringList tokens = {
        QStringLiteral("{name}"), QStringLiteral("{clean}"), QStringLiteral("{title}"),
        QStringLiteral("{date}"), QStringLiteral("{type}"), QStringLiteral("{counter:03}")
    };
    for (const QString &tok : tokens) {
        auto *chip = new QPushButton(tok, renCard);
        chip->setObjectName(QStringLiteral("StudioChipBtn"));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFixedHeight(24);
        connect(chip, &QPushButton::clicked, this, [this, tok]() {
            if (m_editRenamePattern) {
                m_editRenamePattern->insert(tok);
                m_editRenamePattern->setFocus();
            }
        });
        chipRow->addWidget(chip);
    }
    chipRow->addStretch(1);
    renGrid->addLayout(chipRow, 1, 0, 1, 5);

    renGrid->addWidget(new QLabel(tr("Find:"), renCard), 2, 0);
    m_editRenameFind = new QLineEdit(renCard);
    m_editRenameFind->setPlaceholderText(tr("Optional text or regex to match..."));
    m_editRenameFind->setFixedHeight(30);
    renGrid->addWidget(m_editRenameFind, 2, 1);

    renGrid->addWidget(new QLabel(tr("Replace:"), renCard), 2, 2);
    m_editRenameReplace = new QLineEdit(renCard);
    m_editRenameReplace->setPlaceholderText(tr("Replacement text..."));
    m_editRenameReplace->setFixedHeight(30);
    renGrid->addWidget(m_editRenameReplace, 2, 3);

    m_chkRenameRegex = new QCheckBox(tr("Regex"), renCard);
    renGrid->addWidget(m_chkRenameRegex, 2, 4);

    auto *renBtnRow = new QHBoxLayout();
    renBtnRow->setSpacing(10);
    m_chkRenameRecursive = new QCheckBox(tr("Include Subfolders"), renCard);
    m_chkRenameRecursive->setChecked(false);
    renBtnRow->addWidget(m_chkRenameRecursive);
    renBtnRow->addStretch(1);

    m_btnRenamePreview = new QPushButton(tr("Preview Rename"), renCard);
    m_btnRenamePreview->setObjectName(QStringLiteral("StudioPrimaryBtn"));
    m_btnRenamePreview->setCursor(Qt::PointingHandCursor);
    m_btnRenamePreview->setFixedHeight(32);
    connect(m_btnRenamePreview, &QPushButton::clicked, this, &FolderStudioWidget::startBatchRenamePreview);
    renBtnRow->addWidget(m_btnRenamePreview);

    m_btnRenameApply = new QPushButton(tr("Apply Rename"), renCard);
    m_btnRenameApply->setObjectName(QStringLiteral("StudioSuccessBtn"));
    m_btnRenameApply->setCursor(Qt::PointingHandCursor);
    m_btnRenameApply->setFixedHeight(32);
    m_btnRenameApply->setEnabled(false);
    connect(m_btnRenameApply, &QPushButton::clicked, this, &FolderStudioWidget::applyBatchRenamePlan);
    renBtnRow->addWidget(m_btnRenameApply);

    m_btnRenameUndo = new QPushButton(tr("Undo Last"), renCard);
    m_btnRenameUndo->setObjectName(QStringLiteral("StudioSecondaryBtn"));
    m_btnRenameUndo->setCursor(Qt::PointingHandCursor);
    m_btnRenameUndo->setFixedHeight(32);
    connect(m_btnRenameUndo, &QPushButton::clicked, this, &FolderStudioWidget::triggerUndoLastOperation);
    renBtnRow->addWidget(m_btnRenameUndo);

    renGrid->addLayout(renBtnRow, 3, 0, 1, 5);
    renLay->addWidget(renCard);

    m_tableRename = new QTableWidget(0, 3, renameTab);
    m_tableRename->setHorizontalHeaderLabels({tr("Original File"), tr("New Filename"), tr("Reason")});
    m_tableRename->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableRename->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableRename->setAlternatingRowColors(true);
    m_tableRename->verticalHeader()->setVisible(false);
    m_tableRename->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableRename->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableRename->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(m_tableRename, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_renamePlan.size() && m_onPreviewFileRequested) {
            QStringList contextPaths;
            contextPaths.reserve(m_renamePlan.size());
            for (const MovePlanItem &m : m_renamePlan) contextPaths.append(m.sourcePath);
            m_onPreviewFileRequested(m_renamePlan.at(row).sourcePath, contextPaths);
        }
    });

    renLay->addWidget(m_tableRename, 1);

    m_batchTabs->addTab(renameTab, tr("Smart Batch Rename"));

    // --- Sub-tab 2: Folder Cleanup ---
    auto *cleanTab = new QWidget(m_batchTabs);
    auto *clnLay = new QVBoxLayout(cleanTab);
    clnLay->setContentsMargins(10, 10, 10, 10);
    clnLay->setSpacing(10);

    auto *clnCard = new QFrame(cleanTab);
    clnCard->setObjectName(QStringLiteral("StudioCard"));
    auto *clnRow = new QHBoxLayout(clnCard);
    clnRow->setContentsMargins(14, 10, 14, 10);
    clnRow->setSpacing(12);

    m_chkCleanRecursive = new QCheckBox(tr("Scan Subfolders"), clnCard);
    m_chkCleanRecursive->setChecked(true);
    clnRow->addWidget(m_chkCleanRecursive);
    clnRow->addStretch(1);

    m_btnCleanScan = new QPushButton(tr("Scan Empty Dirs, Broken Links && Junk"), clnCard);
    m_btnCleanScan->setObjectName(QStringLiteral("StudioPrimaryBtn"));
    m_btnCleanScan->setCursor(Qt::PointingHandCursor);
    m_btnCleanScan->setFixedHeight(32);
    connect(m_btnCleanScan, &QPushButton::clicked, this, &FolderStudioWidget::startCleanupScan);
    clnRow->addWidget(m_btnCleanScan);

    m_btnCleanTrash = new QPushButton(tr("Move Checked to Trash"), clnCard);
    m_btnCleanTrash->setObjectName(QStringLiteral("StudioDangerBtn"));
    m_btnCleanTrash->setCursor(Qt::PointingHandCursor);
    m_btnCleanTrash->setFixedHeight(32);
    m_btnCleanTrash->setEnabled(false);
    connect(m_btnCleanTrash, &QPushButton::clicked, this, &FolderStudioWidget::trashCheckedCleanupItems);
    clnRow->addWidget(m_btnCleanTrash);

    clnLay->addWidget(clnCard);

    m_tableCleanup = new QTableWidget(0, 3, cleanTab);
    m_tableCleanup->setHorizontalHeaderLabels({tr("Category"), tr("Path"), tr("Size")});
    m_tableCleanup->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableCleanup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableCleanup->setAlternatingRowColors(true);
    m_tableCleanup->verticalHeader()->setVisible(false);
    m_tableCleanup->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableCleanup->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableCleanup->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    clnLay->addWidget(m_tableCleanup, 1);

    m_batchTabs->addTab(cleanTab, tr("Empty Dirs && Junk Cleaner"));

    lay->addWidget(m_batchTabs, 1);
    return page;
}

// =============================================================================
// VISUAL THUMBNAIL PIPELINE
// =============================================================================

void FolderStudioWidget::requestThumbnailsForPaths(const QStringList &paths)
{
    if (paths.isEmpty()) return;

    QStringList missing;
    QSet<QString> seen;
    for (const QString &p : paths) {
        if (p.isEmpty() || seen.contains(p)) continue;
        seen.insert(p);
        auto it = m_thumbCache.constFind(p);
        if (it != m_thumbCache.constEnd() && !it.value().isNull()) {
            applyThumbnailToCards(p, it.value());
        } else {
            missing.append(p);
        }
    }

    if (missing.isEmpty()) return;

    const uint64_t gen = m_thumbGeneration->fetch_add(1, std::memory_order_relaxed) + 1;
    auto genPtr = m_thumbGeneration;
    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;

    QThreadPool::globalInstance()->start([rawSelf, alive, genPtr, gen, missing]() {
        constexpr int kMaxThumbCacheEntries = 400;
        const QSize thumbSize(172, 112);
        for (const QString &path : missing) {
            if (!alive->load(std::memory_order_acquire) || genPtr->load(std::memory_order_relaxed) != gen) return;
            QImage img = renderStudioThumbnail(path, thumbSize);
            if (img.isNull()) continue;
            if (QCoreApplication *app = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(app, [rawSelf, alive, genPtr, gen, path, img]() {
                    if (!alive->load(std::memory_order_acquire) || genPtr->load(std::memory_order_relaxed) != gen) return;
                    const QPixmap pix = QPixmap::fromImage(img);
                    // Cap thumbnail cache size so huge PhotoRec dumps do not exhaust RAM/VRAM
                    if (rawSelf->m_thumbCache.size() >= kMaxThumbCacheEntries) {
                        rawSelf->m_thumbCache.clear();
                    }
                    rawSelf->m_thumbCache.insert(path, pix);
                    rawSelf->applyThumbnailToCards(path, pix);
                }, Qt::QueuedConnection);
            }
        }
    });
}

void FolderStudioWidget::applyThumbnailToCards(const QString &filePath, const QPixmap &pixmap)
{
    const auto cards = m_activeCards.values(filePath);
    for (const QPointer<StudioFileCard> &card : cards) {
        if (card) {
            card->setThumbnailPixmap(pixmap);
        }
    }
}

// =============================================================================
// 1. DUPLICATES WORKFLOW
// =============================================================================

void FolderStudioWidget::clearDuplicatesGallery()
{
    for (const QPointer<StudioFileCard> &card : m_dupCards) {
        if (card) {
            m_activeCards.remove(card->filePath(), card);
        }
    }
    m_dupCards.clear();

    if (!m_dupContainer) return;
    if (auto *lay = qobject_cast<QVBoxLayout *>(m_dupContainer->layout())) {
        // Delete all dynamic group cards & trailing stretch, keeping m_lblDupEmpty intact
        for (int i = lay->count() - 1; i >= 0; --i) {
            QLayoutItem *it = lay->itemAt(i);
            if (it && it->widget() == m_lblDupEmpty) continue;
            QLayoutItem *taken = lay->takeAt(i);
            if (taken) {
                if (QWidget *w = taken->widget()) {
                    w->deleteLater();
                }
                delete taken;
            }
        }
    }
    if (m_lblDupEmpty) {
        m_lblDupEmpty->setText(tr("Click \"Scan Duplicates & Equivalents\" to visually compare exact copies, similar images, documents, media, and archives."));
        m_lblDupEmpty->show();
    }
}

void FolderStudioWidget::startDuplicatesScan()
{
    if (m_currentFolder.isEmpty() || m_isBusy) return;

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancelFlag;
    const QString folder = m_currentFolder;
    const bool recursive = m_chkDupRecursive->isChecked();
    const bool exact = m_chkDupExact->isChecked();
    const bool docs = m_chkDupDocs->isChecked();
    const bool media = m_chkDupMedia->isChecked();
    const bool imgs = m_chkDupImages->isChecked();
    const bool archives = m_chkDupArchive->isChecked();
    const int sim = m_sliderImgSim->value();

    m_duplicateGroups.clear();
    clearDuplicatesGallery();
    if (m_lblDupEmpty) m_lblDupEmpty->setText(tr("Scanning folder for duplicates and visual matches..."));
    m_btnDupTrash->setEnabled(false);
    setBusy(true, tr("Scanning folder for exact and multi-format duplicates..."));

    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;
    QThreadPool::globalInstance()->start([rawSelf, alive, cancel, folder, recursive, exact, docs, media, imgs, archives, sim]() {
        auto progressCb = [rawSelf, alive, cancel](int cur, int total, const QString &text) {
            if (cancel->load(std::memory_order_relaxed) || !alive->load(std::memory_order_acquire)) return;
            if (QCoreApplication *app = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, cur, total, text]() {
                    if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                    if (rawSelf->m_progressBar) {
                        rawSelf->m_progressBar->setRange(0, qMax(1, total));
                        rawSelf->m_progressBar->setValue(cur);
                    }
                    if (rawSelf->m_lblStatus) rawSelf->m_lblStatus->setText(text);
                }, Qt::QueuedConnection);
            }
        };

        QList<DuplicateGroup> groups = FolderStudioEngine::scanDuplicates(
            folder, recursive, exact, docs, media, imgs, archives, sim, *cancel, progressCb);

        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, groups]() {
                if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                if (cancel->load(std::memory_order_relaxed)) {
                    rawSelf->setBusy(false, tr("Scan cancelled."));
                    return;
                }
                rawSelf->populateDuplicatesGallery(groups);
                rawSelf->setBusy(false, groups.isEmpty()
                    ? tr("No duplicates or equivalents found.")
                    : tr("Found %1 duplicate/equivalent group(s). Double-click any card to preview and flip through the group.").arg(groups.size()));
            }, Qt::QueuedConnection);
        }
    });
}

void FolderStudioWidget::populateDuplicatesGallery(const QList<DuplicateGroup> &groups)
{
    m_duplicateGroups = groups;
    clearDuplicatesGallery();

    auto *rootLay = m_dupContainer ? qobject_cast<QVBoxLayout *>(m_dupContainer->layout()) : nullptr;
    if (!rootLay) return;

    if (groups.isEmpty()) {
        if (m_lblDupEmpty) {
            m_lblDupEmpty->setText(tr("No duplicates or equivalents found in this folder."));
            m_lblDupEmpty->show();
        }
        m_btnDupTrash->setEnabled(false);
        return;
    }

    if (m_lblDupEmpty) m_lblDupEmpty->hide();

    auto kindLabel = [this](DuplicateKind k) -> QString {
        switch (k) {
        case DuplicateKind::Exact:         return tr("Exact");
        case DuplicateKind::DocumentText:  return tr("Document");
        case DuplicateKind::AudioVideo:    return tr("Media");
        case DuplicateKind::SimilarImage:  return tr("Similar Image");
        case DuplicateKind::ArchiveFolder: return tr("Archive");
        }
        return QString();
    };

    QStringList thumbPaths;
    int checkableCount = 0;

    for (const DuplicateGroup &g : groups) {
        // Collect file paths in this specific group so previewing flips strictly across this group
        QStringList groupFilePaths;
        groupFilePaths.reserve(g.items.size());
        for (const DuplicateItem &d : g.items) {
            if (!QFileInfo(d.path).isDir()) {
                groupFilePaths.append(d.path);
            }
        }

        auto *groupCard = new QFrame(m_dupContainer);
        groupCard->setObjectName(QStringLiteral("StudioGroupCard"));
        auto *gLay = new QVBoxLayout(groupCard);
        gLay->setContentsMargins(14, 12, 14, 14);
        gLay->setSpacing(10);

        // Group header strip
        auto *hdrRow = new QHBoxLayout();
        hdrRow->setContentsMargins(0, 0, 0, 0);
        hdrRow->setSpacing(8);

        auto *kindPill = new QLabel(kindLabel(g.kind), groupCard);
        kindPill->setObjectName(QStringLiteral("StudioKindPill"));
        hdrRow->addWidget(kindPill);

        auto *titleLbl = new QLabel(g.title, groupCard);
        titleLbl->setObjectName(QStringLiteral("StudioGroupTitle"));
        hdrRow->addWidget(titleLbl);

        if (!g.detail.isEmpty()) {
            auto *detLbl = new QLabel(g.detail, groupCard);
            detLbl->setObjectName(QStringLiteral("StudioGroupMeta"));
            hdrRow->addWidget(detLbl, 1);
        } else {
            hdrRow->addStretch(1);
        }

        gLay->addLayout(hdrRow);

        // Visual thumbnail cards flow inside the group
        auto *cardsHost = new QWidget(groupCard);
        cardsHost->setObjectName(QStringLiteral("StudioGroupCardsHost"));
        auto *flow = new StudioFlowLayout(cardsHost, 0, 12, 12);

        // Option A (Safe Keep-Selection): Checked = KEEP this file, Unchecked = move to Trash.
        // At least 1 file in every duplicate group MUST always remain checked (kept).
        auto groupCards = std::make_shared<QVector<QPointer<StudioFileCard>>>();
        groupCards->reserve(g.items.size());

        for (const DuplicateItem &d : g.items) {
            const bool isKeep = d.recommendedKeep;
            const QString badgeText = isKeep ? tr("★ Keep") : tr("Duplicate");
            const QString badgeTone = isKeep ? QStringLiteral("keep") : QStringLiteral("danger");
            const QString sizeStr = FolderStudioEngine::formatBytes(d.sizeBytes);
            const QString metaLine = d.qualitySummary.isEmpty()
                ? sizeStr
                : QStringLiteral("%1 • %2").arg(sizeStr, d.qualitySummary);
            const QString relPath = relativeDisplayPath(d.path);
            const QString subLine = (relPath != d.fileName) ? relPath : QString();
            const QString tooltip = QStringLiteral("%1\n%2\n%3\n%4")
                .arg(d.path, metaLine,
                     tr("Checked = KEEP this file (unchecked files will be moved to Trash)."),
                     tr("Double-click to preview and compare this group"));

            const QString itemPath = d.path;
            auto *card = new StudioFileCard(
                itemPath,
                d.fileName,
                badgeText,
                badgeTone,
                metaLine,
                subLine,
                isKeep, // Checked = KEEP (auto-keep recommended best file; duplicates unchecked)
                tooltip,
                [this, itemPath, groupFilePaths]() {
                    if (!itemPath.isEmpty() && m_onPreviewFileRequested) {
                        m_onPreviewFileRequested(itemPath, groupFilePaths);
                    }
                },
                cardsHost);

            if (QCheckBox *chk = card->checkBox()) {
                chk->setToolTip(tr("Checked = KEEP this file (at least one file per group must stay kept)"));
                connect(chk, &QCheckBox::toggled, this, [this, card, groupCards](bool checked) {
                    if (!checked) {
                        // Enforce invariant: never allow 0 kept files in a duplicate group
                        int keptInGroup = 0;
                        for (const QPointer<StudioFileCard> &gc : *groupCards) {
                            if (gc && gc->isChecked()) ++keptInGroup;
                        }
                        if (keptInGroup == 0) {
                            card->setCheckedSilent(true);
                            if (m_lblStatus) {
                                m_lblStatus->setText(tr("At least one file per duplicate group must remain kept."));
                            }
                            return;
                        }
                    }
                    int toTrashCount = 0;
                    for (const QPointer<StudioFileCard> &c : m_dupCards) {
                        if (c && !c->isChecked()) ++toTrashCount;
                    }
                    if (m_btnDupTrash) {
                        m_btnDupTrash->setEnabled(toTrashCount > 0);
                    }
                });
            }

            flow->addWidget(card);
            groupCards->append(card);
            m_dupCards.append(card);
            m_activeCards.insert(itemPath, card);
            thumbPaths.append(itemPath);
            if (!isKeep) ++checkableCount;
        }

        gLay->addWidget(cardsHost);
        rootLay->addWidget(groupCard);
    }

    rootLay->addStretch(1);
    m_btnDupTrash->setEnabled(checkableCount > 0);
    requestThumbnailsForPaths(thumbPaths);
}

void FolderStudioWidget::trashCheckedDuplicates()
{
    // Safe Keep-Selection: move all UNCHECKED duplicates to Trash, double-checking that
    // every group still retains at least one checked (kept) file.
    QStringList toTrash;
    QSet<QString> keptPaths;
    for (const QPointer<StudioFileCard> &card : m_dupCards) {
        if (card && card->isChecked()) {
            keptPaths.insert(card->filePath());
        }
    }
    for (const DuplicateGroup &g : m_duplicateGroups) {
        int keptInGroup = 0;
        for (const DuplicateItem &d : g.items) {
            if (keptPaths.contains(d.path)) ++keptInGroup;
        }
        if (keptInGroup == 0) {
            // Safety backstop: skip any group with 0 kept items
            continue;
        }
        for (const DuplicateItem &d : g.items) {
            if (!keptPaths.contains(d.path) && !d.path.isEmpty() && !toTrash.contains(d.path)) {
                toTrash.append(d.path);
            }
        }
    }

    if (toTrash.isEmpty()) {
        QMessageBox::information(this, tr("Nothing to Trash"),
                                 tr("All duplicate items are currently marked as Keep. Uncheck the copies you want to move to Trash."));
        return;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Move Unchecked Duplicates to Trash"),
        tr("Move %1 unchecked duplicate item(s) to the system Trash?\n(All checked files will be safely kept.)").arg(toTrash.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int trashed = 0;
    QStringList errors;
    FolderStudioEngine::trashItems(toTrash, &trashed, &errors);

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Trash Completed with Warnings"),
                             tr("Moved %1 item(s) to Trash.\n\nErrors:\n%2")
                                 .arg(trashed)
                                 .arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Moved %1 duplicate item(s) to Trash.").arg(trashed));
    }

    if (m_onFolderModified) m_onFolderModified();
    startDuplicatesScan();
}

// =============================================================================
// 2. RECOVERY WORKFLOW (PhotoRec / TestDisk)
// =============================================================================

void FolderStudioWidget::clearRecoveryGallery()
{
    for (const QPointer<StudioFileCard> &card : m_recCards) {
        if (card) {
            m_activeCards.remove(card->filePath(), card);
        }
    }
    m_recCards.clear();

    if (m_recFlowLayout) {
        QLayoutItem *it = nullptr;
        while ((it = m_recFlowLayout->takeAt(0)) != nullptr) {
            if (QWidget *w = it->widget()) {
                w->deleteLater();
            }
            delete it;
        }
    }
    if (m_lblRecEmpty) {
        m_lblRecEmpty->setText(tr("Click \"Analyze Recovery Folder\" to visually inspect recovered files, verify signatures, and filter corrupt or junk dumps."));
        m_lblRecEmpty->show();
    }
}

void FolderStudioWidget::startRecoveryScan()
{
    if (m_currentFolder.isEmpty() || m_isBusy) return;

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancelFlag;
    const QString folder = m_currentFolder;
    const bool recursive = m_chkRecRecursive->isChecked();
    const int junkKb = m_spinJunkKb->value();

    m_recoveryItems.clear();
    clearRecoveryGallery();
    if (m_lblRecEmpty) m_lblRecEmpty->setText(tr("Inspecting file headers and generating visual previews..."));
    m_btnRecFix->setEnabled(false);
    m_btnRecTrash->setEnabled(false);
    setBusy(true, tr("Inspecting file headers (magic bytes), integrity, and internal metadata..."));

    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;
    QThreadPool::globalInstance()->start([rawSelf, alive, cancel, folder, recursive, junkKb]() {
        auto progressCb = [rawSelf, alive, cancel](int cur, int total, const QString &text) {
            if (cancel->load(std::memory_order_relaxed) || !alive->load(std::memory_order_acquire)) return;
            if (QCoreApplication *app = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, cur, total, text]() {
                    if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                    if (rawSelf->m_progressBar) {
                        rawSelf->m_progressBar->setRange(0, qMax(1, total));
                        rawSelf->m_progressBar->setValue(cur);
                    }
                    if (rawSelf->m_lblStatus) rawSelf->m_lblStatus->setText(text);
                }, Qt::QueuedConnection);
            }
        };

        QList<RecoveryItem> items = FolderStudioEngine::scanRecovery(folder, recursive, junkKb, *cancel, progressCb);

        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, items]() {
                if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                if (cancel->load(std::memory_order_relaxed)) {
                    rawSelf->setBusy(false, tr("Recovery analysis cancelled."));
                    return;
                }
                rawSelf->populateRecoveryGallery(items);
                rawSelf->setBusy(false, tr("Analyzed %1 file(s). Double-click any card to preview.").arg(items.size()));
            }, Qt::QueuedConnection);
        }
    });
}

void FolderStudioWidget::populateRecoveryGallery(const QList<RecoveryItem> &items)
{
    m_recoveryItems = items;
    clearRecoveryGallery();

    if (items.isEmpty()) {
        if (m_lblRecEmpty) {
            m_lblRecEmpty->setText(tr("No files found to analyze in this folder."));
            m_lblRecEmpty->show();
        }
        m_btnRecFix->setEnabled(false);
        m_btnRecTrash->setEnabled(false);
        refreshUndoButtons();
        return;
    }

    if (m_lblRecEmpty) m_lblRecEmpty->hide();

    QStringList allRecPaths;
    allRecPaths.reserve(items.size());
    for (const RecoveryItem &r : items) {
        allRecPaths.append(r.path);
    }

    int healthy = 0, wrongExt = 0, corrupt = 0, junk = 0, renameCandidates = 0;
    QWidget *flowParent = m_recFlowLayout ? m_recFlowLayout->parentWidget() : m_recContainer;

    for (const RecoveryItem &r : items) {
        const QString currentName = QFileInfo(r.path).fileName();

        QString statusText;
        QString tone;
        switch (r.status) {
        case RecoveryStatus::Healthy:
            statusText = tr("Healthy");
            tone = QStringLiteral("keep");
            ++healthy;
            break;
        case RecoveryStatus::WrongExtension:
            statusText = tr("Wrong Ext");
            tone = QStringLiteral("warn");
            ++wrongExt;
            break;
        case RecoveryStatus::Corrupt:
            statusText = tr("Corrupt");
            tone = QStringLiteral("danger");
            ++corrupt;
            break;
        case RecoveryStatus::Junk:
            statusText = tr("Junk");
            tone = QStringLiteral("neutral");
            ++junk;
            break;
        }

        const bool hasBetterName = (!r.suggestedName.isEmpty() && r.suggestedName != currentName
                                    && (r.status == RecoveryStatus::Healthy || r.status == RecoveryStatus::WrongExtension));
        if (hasBetterName) ++renameCandidates;

        const QString fmtStr = r.detectedExt.isEmpty()
            ? r.detectedMime
            : QStringLiteral(".%1").arg(r.detectedExt.toUpper());
        const QString metaLine = QStringLiteral("%1 • %2")
            .arg(FolderStudioEngine::formatBytes(r.sizeBytes), fmtStr);
        const QString subLine = hasBetterName
            ? QStringLiteral("→ %1").arg(r.suggestedName)
            : r.reason;
        const QString tooltip = QStringLiteral("%1\n%2: %3\n%4")
            .arg(r.path, statusText, r.reason,
                 hasBetterName ? tr("Suggested name: %1").arg(r.suggestedName) : tr("Double-click to preview"));

        const QString itemPath = r.path;
        auto *card = new StudioFileCard(
            itemPath,
            currentName,
            statusText,
            tone,
            metaLine,
            subLine,
            (r.status != RecoveryStatus::Healthy || hasBetterName),
            tooltip,
            [this, itemPath, allRecPaths]() {
                if (!itemPath.isEmpty() && m_onPreviewFileRequested) {
                    m_onPreviewFileRequested(itemPath, allRecPaths);
                }
            },
            flowParent);

        if (m_recFlowLayout) {
            m_recFlowLayout->addWidget(card);
        }
        m_recCards.append(card);
        m_activeCards.insert(itemPath, card);
    }

    m_lblRecSummary->setText(
        tr("Summary: %1 Healthy  •  %2 Wrong Extension  •  %3 Corrupt  •  %4 Junk  •  %5 Smart Rename Candidate(s)")
            .arg(healthy).arg(wrongExt).arg(corrupt).arg(junk).arg(renameCandidates));

    m_btnRecFix->setEnabled(renameCandidates > 0 || wrongExt > 0);
    m_btnRecTrash->setEnabled(corrupt > 0 || junk > 0);
    refreshUndoButtons();
    requestThumbnailsForPaths(allRecPaths);
}

void FolderStudioWidget::applyRecoveryRenames()
{
    QList<MovePlanItem> plan;
    QSet<QString> reserved;
    const int count = qMin(m_recoveryItems.size(), m_recCards.size());
    for (int i = 0; i < count; ++i) {
        const QPointer<StudioFileCard> &card = m_recCards.at(i);
        if (!card || !card->isChecked()) continue;
        const RecoveryItem &r = m_recoveryItems.at(i);
        if (r.status == RecoveryStatus::Corrupt || r.status == RecoveryStatus::Junk) continue;
        const QFileInfo fi(r.path);
        if (!r.suggestedName.isEmpty() && r.suggestedName != fi.fileName()) {
            // Disambiguate colliding suggested filenames within the same batch or existing files
            QString dest = fi.absolutePath() + QLatin1Char('/') + r.suggestedName;
            if (QFileInfo::exists(dest) || reserved.contains(dest)) {
                const QFileInfo destFi(dest);
                const QString stem = destFi.completeBaseName();
                const QString ext = destFi.suffix();
                for (int n = 1; n < 10000; ++n) {
                    const QString cand = ext.isEmpty()
                        ? QStringLiteral("%1/%2_%3").arg(fi.absolutePath(), stem).arg(n, 2, 10, QLatin1Char('0'))
                        : QStringLiteral("%1/%2_%3.%4").arg(fi.absolutePath(), stem).arg(n, 2, 10, QLatin1Char('0')).arg(ext);
                    if (!QFileInfo::exists(cand) && !reserved.contains(cand)) {
                        dest = cand;
                        break;
                    }
                }
            }
            reserved.insert(dest);
            MovePlanItem m;
            m.sourcePath = r.path;
            m.destPath = dest;
            m.reason = r.reason;
            plan.append(m);
        }
    }

    if (plan.isEmpty()) {
        QMessageBox::information(this, tr("Nothing to Rename"),
                                 tr("No checked healthy/wrong-extension files require renaming."));
        return;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Apply Recovery Renames"),
        tr("Rename / repair extensions for %1 file(s)?\nAn Undo rollback manifest will be saved automatically.").arg(plan.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    int applied = 0;
    QStringList errors;
    FolderStudioEngine::executeMovePlan(plan, m_currentFolder, false, true, &applied, &errors);
    refreshUndoButtons();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Completed with Warnings"),
                             tr("Renamed %1 file(s).\n\nErrors:\n%2").arg(applied).arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Renamed and repaired %1 file(s).").arg(applied));
    }

    if (m_onFolderModified) m_onFolderModified();
    startRecoveryScan();
}

void FolderStudioWidget::trashRecoveryJunkAndCorrupt()
{
    QStringList toTrash;
    const int count = qMin(m_recoveryItems.size(), m_recCards.size());
    for (int i = 0; i < count; ++i) {
        const QPointer<StudioFileCard> &card = m_recCards.at(i);
        if (!card || !card->isChecked()) continue;
        const RecoveryItem &r = m_recoveryItems.at(i);
        if (r.status == RecoveryStatus::Junk || r.status == RecoveryStatus::Corrupt) {
            toTrash.append(r.path);
        }
    }

    if (toTrash.isEmpty()) {
        QMessageBox::information(this, tr("No Junk or Corrupt Items"),
                                 tr("No checked Junk or Corrupt files to move to Trash."));
        return;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Trash Junk & Corrupt Files"),
        tr("Move %1 checked Junk / Corrupt file(s) to the system Trash?").arg(toTrash.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int trashed = 0;
    QStringList errors;
    FolderStudioEngine::trashItems(toTrash, &trashed, &errors);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Trash Completed with Warnings"),
                             tr("Moved %1 Junk/Corrupt item(s) to Trash.\n\nErrors:\n%2")
                                 .arg(trashed)
                                 .arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Moved %1 Junk/Corrupt item(s) to Trash.").arg(trashed));
    }
    if (m_onFolderModified) m_onFolderModified();
    startRecoveryScan();
}

// =============================================================================
// 3. SMART TREE ORGANIZER WORKFLOW
// =============================================================================

void FolderStudioWidget::startOrganizePreview()
{
    if (m_currentFolder.isEmpty() || m_isBusy) return;

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancelFlag;
    const QString folder = m_currentFolder;
    const OrganizeMode mode = static_cast<OrganizeMode>(m_comboOrgMode->currentData().toInt());
    const bool recursive = m_chkOrgRecursive->isChecked();

    m_tableOrganize->setRowCount(0);
    m_organizePlan.clear();
    m_btnOrgApply->setEnabled(false);
    setBusy(true, tr("Building Dry-Run tree organization plan..."));

    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;
    QThreadPool::globalInstance()->start([rawSelf, alive, cancel, folder, mode, recursive]() {
        QList<MovePlanItem> plan = FolderStudioEngine::planTreeOrganization(folder, mode, recursive, *cancel);
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, plan]() {
                if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                if (cancel->load(std::memory_order_relaxed)) {
                    rawSelf->setBusy(false, tr("Plan generation cancelled."));
                    return;
                }
                rawSelf->populateOrganizeTable(plan);
                rawSelf->setBusy(false, plan.isEmpty()
                    ? tr("Folder is already organized according to this strategy.")
                    : tr("Dry-Run ready: %1 file(s) planned to move.").arg(plan.size()));
            }, Qt::QueuedConnection);
        }
    });
}

void FolderStudioWidget::populateOrganizeTable(const QList<MovePlanItem> &plan)
{
    m_organizePlan = plan;
    m_tableOrganize->setRowCount(plan.size());
    for (int row = 0; row < plan.size(); ++row) {
        const MovePlanItem &m = plan.at(row);
        m_tableOrganize->setItem(row, 0, new QTableWidgetItem(relativeDisplayPath(m.sourcePath)));
        m_tableOrganize->setItem(row, 1, new QTableWidgetItem(relativeDisplayPath(m.destPath)));
        m_tableOrganize->setItem(row, 2, new QTableWidgetItem(m.reason));
    }
    m_lblOrgSummary->setText(plan.isEmpty()
        ? tr("No file moves needed for this strategy.")
        : tr("Dry-Run Preview: %1 file(s) will be moved. Empty source directories will be pruned and Undo rollback will be saved.").arg(plan.size()));
    m_btnOrgApply->setEnabled(!plan.isEmpty());
    refreshUndoButtons();
}

void FolderStudioWidget::applyOrganizePlan()
{
    if (m_organizePlan.isEmpty()) return;

    const auto reply = QMessageBox::question(
        this,
        tr("Apply Tree Organization"),
        tr("Move %1 file(s) according to the previewed plan?\nYou can revert this operation with 'Undo Last'.").arg(m_organizePlan.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    int applied = 0;
    QStringList errors;
    FolderStudioEngine::executeMovePlan(m_organizePlan, m_currentFolder, true, true, &applied, &errors);

    m_organizePlan.clear();
    m_tableOrganize->setRowCount(0);
    m_btnOrgApply->setEnabled(false);
    refreshUndoButtons();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Organization Completed with Warnings"),
                             tr("Moved %1 file(s).\n\nErrors:\n%2").arg(applied).arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Organized %1 file(s). Undo rollback saved.").arg(applied));
        m_lblOrgSummary->setText(tr("Organization complete (%1 file(s) moved).").arg(applied));
    }

    if (m_onFolderModified) m_onFolderModified();
}

// =============================================================================
// 4. BATCH RENAME & CLEANUP WORKFLOW
// =============================================================================

void FolderStudioWidget::startBatchRenamePreview()
{
    if (m_currentFolder.isEmpty() || m_isBusy) return;

    const QString findText = m_editRenameFind->text();
    const bool useRegex = m_chkRenameRegex->isChecked();
    if (useRegex && !findText.isEmpty()) {
        const QRegularExpression rx(findText);
        if (!rx.isValid()) {
            QMessageBox::warning(this, tr("Invalid Regular Expression"),
                                 tr("The Find regular expression is invalid:\n%1").arg(rx.errorString()));
            return;
        }
    }

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancelFlag;
    const QString folder = m_currentFolder;
    const bool recursive = m_chkRenameRecursive->isChecked();
    const QString pattern = m_editRenamePattern->text().trimmed();
    const QString replaceText = m_editRenameReplace->text();

    m_tableRename->setRowCount(0);
    m_renamePlan.clear();
    m_btnRenameApply->setEnabled(false);
    setBusy(true, tr("Computing batch rename preview..."));

    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;
    QThreadPool::globalInstance()->start([rawSelf, alive, cancel, folder, recursive, pattern, findText, replaceText, useRegex]() {
        QList<MovePlanItem> plan = FolderStudioEngine::planBatchRename(
            folder, recursive, pattern, findText, replaceText, useRegex, *cancel);
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, plan]() {
                if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                if (cancel->load(std::memory_order_relaxed)) {
                    rawSelf->setBusy(false, tr("Batch rename preview cancelled."));
                    return;
                }
                rawSelf->populateBatchRenameTable(plan);
                rawSelf->setBusy(false, plan.isEmpty()
                    ? tr("No filenames would change with the current pattern/filter.")
                    : tr("Preview ready: %1 file(s) will be renamed.").arg(plan.size()));
            }, Qt::QueuedConnection);
        }
    });
}

void FolderStudioWidget::populateBatchRenameTable(const QList<MovePlanItem> &plan)
{
    m_renamePlan = plan;
    m_tableRename->setRowCount(plan.size());
    for (int row = 0; row < plan.size(); ++row) {
        const MovePlanItem &m = plan.at(row);
        m_tableRename->setItem(row, 0, new QTableWidgetItem(relativeDisplayPath(m.sourcePath)));
        m_tableRename->setItem(row, 1, new QTableWidgetItem(QFileInfo(m.destPath).fileName()));
        m_tableRename->setItem(row, 2, new QTableWidgetItem(m.reason));
    }
    m_btnRenameApply->setEnabled(!plan.isEmpty());
    refreshUndoButtons();
}

void FolderStudioWidget::applyBatchRenamePlan()
{
    if (m_renamePlan.isEmpty()) return;

    const auto reply = QMessageBox::question(
        this,
        tr("Apply Batch Rename"),
        tr("Rename %1 file(s)?\nAn Undo rollback manifest will be saved automatically.").arg(m_renamePlan.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    int applied = 0;
    QStringList errors;
    FolderStudioEngine::executeMovePlan(m_renamePlan, m_currentFolder, false, true, &applied, &errors);

    m_renamePlan.clear();
    m_tableRename->setRowCount(0);
    m_btnRenameApply->setEnabled(false);
    refreshUndoButtons();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Rename Completed with Warnings"),
                             tr("Renamed %1 file(s).\n\nErrors:\n%2").arg(applied).arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Batch renamed %1 file(s).").arg(applied));
    }

    if (m_onFolderModified) m_onFolderModified();
}

void FolderStudioWidget::startCleanupScan()
{
    if (m_currentFolder.isEmpty() || m_isBusy) return;

    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancelFlag;
    const QString folder = m_currentFolder;
    const bool recursive = m_chkCleanRecursive->isChecked();

    m_tableCleanup->setRowCount(0);
    m_cleanupItems.clear();
    m_btnCleanTrash->setEnabled(false);
    setBusy(true, tr("Scanning for empty folders, broken symlinks, 0-byte files, and temp junk..."));

    auto alive = m_aliveFlag;
    FolderStudioWidget *rawSelf = this;
    QThreadPool::globalInstance()->start([rawSelf, alive, cancel, folder, recursive]() {
        QList<CleanupItem> items = FolderStudioEngine::scanCleanup(folder, recursive, *cancel);
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [rawSelf, alive, cancel, items]() {
                if (!alive->load(std::memory_order_acquire) || cancel != rawSelf->m_cancelFlag) return;
                if (cancel->load(std::memory_order_relaxed)) {
                    rawSelf->setBusy(false, tr("Cleanup scan cancelled."));
                    return;
                }
                rawSelf->populateCleanupTable(items);
                rawSelf->setBusy(false, items.isEmpty()
                    ? tr("Folder is clean — no empty directories or junk found.")
                    : tr("Found %1 cleanup candidate(s).").arg(items.size()));
            }, Qt::QueuedConnection);
        }
    });
}

void FolderStudioWidget::populateCleanupTable(const QList<CleanupItem> &items)
{
    m_cleanupItems = items;
    m_tableCleanup->setRowCount(items.size());

    auto kindStr = [this](CleanupKind k) -> QString {
        switch (k) {
        case CleanupKind::EmptyDir:      return tr("Empty Folder");
        case CleanupKind::BrokenSymlink: return tr("Broken Symlink");
        case CleanupKind::ZeroByteFile:  return tr("0-Byte File");
        case CleanupKind::TempJunk:      return tr("OS / Temp Junk");
        }
        return QString();
    };

    for (int row = 0; row < items.size(); ++row) {
        const CleanupItem &c = items.at(row);
        auto *kindItem = new QTableWidgetItem(kindStr(c.kind));
        kindItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        kindItem->setCheckState(Qt::Checked);
        m_tableCleanup->setItem(row, 0, kindItem);
        m_tableCleanup->setItem(row, 1, new QTableWidgetItem(relativeDisplayPath(c.path)));
        m_tableCleanup->setItem(row, 2, new QTableWidgetItem(c.kind == CleanupKind::EmptyDir ? QStringLiteral("—") : FolderStudioEngine::formatBytes(c.sizeBytes)));
    }
    m_btnCleanTrash->setEnabled(!items.isEmpty());
}

void FolderStudioWidget::trashCheckedCleanupItems()
{
    QStringList toTrash;
    for (int row = 0; row < m_cleanupItems.size(); ++row) {
        QTableWidgetItem *chk = m_tableCleanup->item(row, 0);
        if (chk && chk->checkState() == Qt::Checked) {
            toTrash.append(m_cleanupItems.at(row).path);
        }
    }

    if (toTrash.isEmpty()) return;

    const auto reply = QMessageBox::question(
        this,
        tr("Clean Checked Items"),
        tr("Move %1 checked cleanup item(s) to the system Trash?").arg(toTrash.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int trashed = 0;
    QStringList errors;
    FolderStudioEngine::trashItems(toTrash, &trashed, &errors);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Cleanup Completed with Warnings"),
                             tr("Cleaned %1 item(s) to Trash.\n\nErrors:\n%2")
                                 .arg(trashed)
                                 .arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Cleaned %1 item(s) to Trash.").arg(trashed));
    }
    if (m_onFolderModified) m_onFolderModified();
    startCleanupScan();
}

// =============================================================================
// SHARED UNDO, BUSY STATE & THEME
// =============================================================================

void FolderStudioWidget::triggerUndoLastOperation()
{
    if (!FolderStudioEngine::canUndoLastOperation()) return;

    const auto reply = QMessageBox::question(
        this,
        tr("Undo Last Folder Operation"),
        tr("Revert the last move / organize / rename operation and restore original paths?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    int restored = 0;
    QStringList errors;
    FolderStudioEngine::undoLastOperation(&restored, &errors);

    // Clear stale plans and galleries whose file paths may have just been moved back
    clearDuplicatesGallery();
    clearRecoveryGallery();
    if (m_tableOrganize) m_tableOrganize->setRowCount(0);
    if (m_tableRename) m_tableRename->setRowCount(0);
    if (m_tableCleanup) m_tableCleanup->setRowCount(0);
    m_duplicateGroups.clear();
    m_recoveryItems.clear();
    m_organizePlan.clear();
    m_renamePlan.clear();
    m_cleanupItems.clear();
    if (m_btnDupTrash) m_btnDupTrash->setEnabled(false);
    if (m_btnRecFix) m_btnRecFix->setEnabled(false);
    if (m_btnRecTrash) m_btnRecTrash->setEnabled(false);
    if (m_btnOrgApply) m_btnOrgApply->setEnabled(false);
    if (m_btnRenameApply) m_btnRenameApply->setEnabled(false);
    if (m_btnCleanTrash) m_btnCleanTrash->setEnabled(false);
    refreshUndoButtons();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Undo Completed with Warnings"),
                             tr("Restored %1 file(s).\n\nErrors:\n%2").arg(restored).arg(errors.mid(0, 10).join(QLatin1Char('\n'))));
    } else {
        m_lblStatus->setText(tr("Undo complete: restored %1 file(s).").arg(restored));
    }

    if (m_onFolderModified) m_onFolderModified();
}

void FolderStudioWidget::refreshUndoButtons()
{
    const bool canUndo = !m_isBusy && FolderStudioEngine::canUndoLastOperation();
    if (m_btnRecUndo) m_btnRecUndo->setEnabled(canUndo);
    if (m_btnOrgUndo) m_btnOrgUndo->setEnabled(canUndo);
    if (m_btnRenameUndo) m_btnRenameUndo->setEnabled(canUndo);
}

void FolderStudioWidget::setBusy(bool busy, const QString &statusText)
{
    m_isBusy = busy;
    if (m_progressBar) {
        m_progressBar->setVisible(busy);
        if (busy) m_progressBar->setRange(0, 0); // Indeterminate until first progress callback
    }
    if (m_btnCancelScan) m_btnCancelScan->setVisible(busy);
    if (m_lblStatus && !statusText.isEmpty()) m_lblStatus->setText(statusText);

    if (m_btnDupScan) m_btnDupScan->setEnabled(!busy);
    if (m_btnRecScan) m_btnRecScan->setEnabled(!busy);
    if (m_btnOrgPreview) m_btnOrgPreview->setEnabled(!busy);
    if (m_btnRenamePreview) m_btnRenamePreview->setEnabled(!busy);
    if (m_btnCleanScan) m_btnCleanScan->setEnabled(!busy);

    // Prevent destructive disk mutations while background workers are scanning
    if (busy) {
        if (m_btnDupTrash) m_btnDupTrash->setEnabled(false);
        if (m_btnRecFix) m_btnRecFix->setEnabled(false);
        if (m_btnRecTrash) m_btnRecTrash->setEnabled(false);
        if (m_btnOrgApply) m_btnOrgApply->setEnabled(false);
        if (m_btnRenameApply) m_btnRenameApply->setEnabled(false);
        if (m_btnCleanTrash) m_btnCleanTrash->setEnabled(false);
    } else {
        if (m_btnDupTrash) m_btnDupTrash->setEnabled(!m_duplicateGroups.isEmpty());
        if (m_btnOrgApply) m_btnOrgApply->setEnabled(!m_organizePlan.isEmpty());
        if (m_btnRenameApply) m_btnRenameApply->setEnabled(!m_renamePlan.isEmpty());
        if (m_btnCleanTrash) m_btnCleanTrash->setEnabled(!m_cleanupItems.isEmpty());
    }
    refreshUndoButtons();
}

QString FolderStudioWidget::relativeDisplayPath(const QString &fullPath) const
{
    if (m_currentFolder.isEmpty()) return fullPath;
    QDir root(m_currentFolder);
    const QString rel = root.relativeFilePath(fullPath);
    return rel.startsWith(QStringLiteral("..")) ? fullPath : rel;
}

void FolderStudioWidget::updateTheme()
{
    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = !ThemeManager::isDark(preset);
    const bool isZinc = (preset == ThemePreset::ZincGray);

    const QString bg = isLight ? QStringLiteral("#eef1f5")
                               : (isZinc ? QStringLiteral("#494a4f") : QStringLiteral("#181a1f"));
    const QString cardBg = isLight ? QStringLiteral("#ffffff")
                                   : (isZinc ? QStringLiteral("#3c3d42") : QStringLiteral("#1e232e"));
    const QString altBg = isLight ? QStringLiteral("#f8fafc")
                                  : (isZinc ? QStringLiteral("#44454a") : QStringLiteral("#232936"));
    const QString border = isLight ? QStringLiteral("rgba(15, 23, 42, 0.12)")
                                   : (isZinc ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(255, 255, 255, 0.09)"));
    const QString text = isLight ? QStringLiteral("#0f172a") : QStringLiteral("#f1f5f9");
    const QString subText = isLight ? QStringLiteral("#64748b") : QStringLiteral("#94a3b8");
    const QString inputBg = isLight ? QStringLiteral("#f1f5f9")
                                    : (isZinc ? QStringLiteral("#333438") : QStringLiteral("#141820"));
    const QString secBtnBg = isLight ? QStringLiteral("#e2e8f0")
                                     : (isZinc ? QStringLiteral("#56575e") : QStringLiteral("#283040"));
    const QString secBtnHover = isLight ? QStringLiteral("#cbd5e1")
                                        : (isZinc ? QStringLiteral("#63646c") : QStringLiteral("#334155"));

    const QString css = QStringLiteral(
        "QWidget#FolderStudioWidget { background-color: %1; color: %2; }"
        "QFrame#StudioStatusBanner, QFrame#StudioCard {"
        "   background-color: %3; border: 1px solid %4; border-radius: 12px;"
        "}"
        "QLabel { color: %2; background: transparent; border: none; font-size: 12px; }"
        "QLabel#StudioContextLabel { font-weight: 700; color: #38bdf8; }"
        "QLabel#StudioHintLabel { color: %5; font-size: 11.5px; }"
        "QCheckBox { color: %2; spacing: 6px; font-size: 12px; }"
        "QLineEdit, QSpinBox, QComboBox {"
        "   background-color: %6; color: %2; border: 1px solid %4; border-radius: 8px; padding: 3px 10px; font-size: 12px;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #38bdf8; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView {"
        "   background-color: %3; color: %2; border: 1px solid %4; selection-background-color: #0284c7; selection-color: #ffffff; outline: none;"
        "}"
        "QPushButton#StudioPrimaryBtn {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff; border: none; border-radius: 9px; padding: 0px 14px; font-weight: 600; font-size: 12px;"
        "}"
        "QPushButton#StudioPrimaryBtn:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6); }"
        "QPushButton#StudioPrimaryBtn:disabled { opacity: 0.4; }"
        "QPushButton#StudioSuccessBtn {"
        "   background-color: #16a34a; color: #ffffff; border: none; border-radius: 9px; padding: 0px 14px; font-weight: 600; font-size: 12px;"
        "}"
        "QPushButton#StudioSuccessBtn:hover { background-color: #22c55e; }"
        "QPushButton#StudioSuccessBtn:disabled { background-color: %7; color: %5; }"
        "QPushButton#StudioDangerBtn {"
        "   background-color: rgba(239, 68, 68, 0.16); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.35);"
        "   border-radius: 9px; padding: 0px 13px; font-weight: 600; font-size: 12px;"
        "}"
        "QPushButton#StudioDangerBtn:hover { background-color: #ef4444; color: #ffffff; }"
        "QPushButton#StudioDangerBtn:disabled { background-color: %7; color: %5; border-color: %4; }"
        "QPushButton#StudioSecondaryBtn, QPushButton#StudioChipBtn {"
        "   background-color: %7; color: %2; border: 1px solid %4; border-radius: 8px; padding: 0px 12px; font-weight: 600; font-size: 11.5px;"
        "}"
        "QPushButton#StudioSecondaryBtn:hover, QPushButton#StudioChipBtn:hover { background-color: %8; }"
        "QPushButton#StudioSecondaryBtn:disabled { color: %5; }"
        "QTreeWidget, QTableWidget {"
        "   background-color: %3; alternate-background-color: %9; color: %2;"
        "   border: 1px solid %4; border-radius: 12px; gridline-color: %4; outline: none; font-size: 12px;"
        "}"
        "QScrollArea#StudioGalleryScroll, QWidget#StudioGalleryCanvas, QWidget#StudioGroupCardsHost {"
        "   background: transparent; border: none;"
        "}"
        "QFrame#StudioGroupCard {"
        "   background-color: %3; border: 1px solid %4; border-radius: 14px;"
        "}"
        "QLabel#StudioKindPill {"
        "   background-color: rgba(56, 189, 248, 0.16); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.32);"
        "   border-radius: 6px; padding: 2px 8px; font-weight: 700; font-size: 11px;"
        "}"
        "QLabel#StudioGroupTitle { font-weight: 700; font-size: 13px; color: %2; }"
        "QLabel#StudioGroupMeta { font-size: 11.5px; color: %5; }"
        "QFrame#StudioFileCard {"
        "   background-color: %9; border: 1.5px solid %4; border-radius: 12px;"
        "}"
        "QFrame#StudioFileCard:hover {"
        "   border-color: #38bdf8;"
        "}"
        "QFrame#StudioFileCard[cardChecked=\"true\"] {"
        "   border-color: rgba(239, 68, 68, 0.75);"
        "}"
        "QFrame#StudioFileCard[cardTone=\"keep\"][cardChecked=\"false\"] {"
        "   border-color: rgba(34, 197, 94, 0.55);"
        "}"
        "QLabel#StudioThumbBox {"
        "   background-color: %6; border: 1px solid %4; border-radius: 8px; color: %5; font-weight: 700; font-size: 12px;"
        "}"
        "QLabel#StudioBadgePill {"
        "   border-radius: 6px; padding: 2px 7px; font-weight: 700; font-size: 10.5px;"
        "}"
        "QLabel#StudioBadgePill[tone=\"keep\"] {"
        "   background-color: rgba(34, 197, 94, 0.16); color: #22c55e; border: 1px solid rgba(34, 197, 94, 0.35);"
        "}"
        "QLabel#StudioBadgePill[tone=\"danger\"] {"
        "   background-color: rgba(239, 68, 68, 0.16); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.35);"
        "}"
        "QLabel#StudioBadgePill[tone=\"warn\"] {"
        "   background-color: rgba(245, 158, 11, 0.16); color: #f59e0b; border: 1px solid rgba(245, 158, 11, 0.35);"
        "}"
        "QLabel#StudioBadgePill[tone=\"neutral\"] {"
        "   background-color: %7; color: %5; border: 1px solid %4;"
        "}"
        "QLabel#StudioCardTitle { font-weight: 600; font-size: 11.5px; color: %2; }"
        "QLabel#StudioCardMeta { font-size: 10.5px; color: %5; }"
        "QLabel#StudioCardSub { font-size: 10.5px; color: #38bdf8; }"
        "QLabel#StudioEmptyHint { color: %5; font-size: 13px; padding: 36px 24px; }"
        "QHeaderView::section {"
        "   background-color: %6; color: %5; border: none; border-bottom: 1px solid %4; padding: 6px 8px; font-weight: 600; font-size: 11.5px;"
        "}"
        "QTabWidget::pane { border: 1px solid %4; border-radius: 12px; background-color: %1; top: -1px; }"
        "QTabBar::tab {"
        "   background-color: %7; color: %5; border: 1px solid %4; border-bottom: none;"
        "   border-top-left-radius: 8px; border-top-right-radius: 8px; padding: 6px 16px; margin-right: 4px; font-weight: 600; font-size: 12px;"
        "}"
        "QTabBar::tab:selected { background-color: %3; color: %2; }"
        "QProgressBar { background-color: %6; border: none; border-radius: 5px; }"
        "QProgressBar::chunk { background-color: #38bdf8; border-radius: 5px; }"
    ).arg(bg, text, cardBg, border, subText, inputBg, secBtnBg, secBtnHover, altBg);

    setStyleSheet(css + ThemeManager::scrollBarStyleSheet(preset));
}
