#include "archive_viewer.h"
#include "theme_manager.h"
#include "build_config.h"
#include "package_manager_helper.h"
#include <archive.h>
#include <archive_entry.h>
#include <QLabel>
#include <QHeaderView>
#include <QDateTime>
#include <QScrollBar>
#include <QProxyStyle>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QClipboard>
#include <QDirIterator>
#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QSet>
#include <functional>
#include <QCollator>
#include <algorithm>
#include <QDebug>

namespace {
class CleanBranchStyle : public QProxyStyle {
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (element == PE_IndicatorBranch) {
            if (option->state & State_Children) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                QPen pen(QColor("#787f87"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter->setPen(pen);
                QRect r = option->rect;
                int cx = r.center().x();
                int cy = r.center().y();
                QPainterPath path;
                if (option->state & State_Open) {
                    path.moveTo(cx - 4, cy - 2);
                    path.lineTo(cx, cy + 2);
                    path.lineTo(cx + 4, cy - 2);
                } else {
                    path.moveTo(cx - 2, cy - 4);
                    path.lineTo(cx + 2, cy);
                    path.lineTo(cx - 2, cy + 4);
                }
                painter->strokePath(path, pen);
                painter->restore();
            }
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

/**
 * @brief Custom QTreeWidgetItem enabling intelligent numeric size/date sorting while keeping folders grouped.
 */
class ArchiveTreeWidgetItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override {
        int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        bool thisIsDir = data(0, Qt::UserRole + 1).toBool();
        bool otherIsDir = other.data(0, Qt::UserRole + 1).toBool();

        // Always keep directories grouped together on top
        if (thisIsDir != otherIsDir) {
            Qt::SortOrder order = treeWidget() ? treeWidget()->header()->sortIndicatorOrder() : Qt::AscendingOrder;
            return (order == Qt::AscendingOrder) ? thisIsDir : !thisIsDir;
        }

        if (col == 2) { // Size column (bytes)
            qint64 s1 = data(2, Qt::UserRole).toLongLong();
            qint64 s2 = other.data(2, Qt::UserRole).toLongLong();
            return s1 < s2;
        } else if (col == 3) { // Date column
            QDateTime d1 = data(3, Qt::UserRole).toDateTime();
            QDateTime d2 = other.data(3, Qt::UserRole).toDateTime();
            return d1 < d2;
        } else if (col == 1) { // Type / Item count column
            int c1 = data(1, Qt::UserRole).toInt();
            int c2 = other.data(1, Qt::UserRole).toInt();
            if (c1 != c2) return c1 < c2;
        }

        // Natural numeric collation for filenames
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        return collator.compare(text(0), other.text(0)) < 0;
    }
};
} // namespace

// ==========================================
// ArchiveWorker (Asynchronous Background Thread)
// ==========================================
ArchiveWorker::ArchiveWorker(const QString &filePath, const QString &passphrase, QObject *parent)
    : QThread(parent)
    , m_filePath(filePath)
    , m_passphrase(passphrase)
    , m_isCancelled(false)
{
}

ArchiveWorker::~ArchiveWorker()
{
    cancel();
    wait(1000);
}

void ArchiveWorker::cancel()
{
    m_isCancelled.store(true, std::memory_order_relaxed);
}

void ArchiveWorker::run()
{
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (!m_passphrase.isEmpty()) {
        archive_read_add_passphrase(a, m_passphrase.toUtf8().constData());
    }

    int r = archive_read_open_filename(a, m_filePath.toUtf8().constData(), 65536);
    if (r != ARCHIVE_OK) {
        QString errStr = QString::fromUtf8(archive_error_string(a));
        archive_read_free(a);
        if (!m_isCancelled.load(std::memory_order_relaxed)) {
            if (errStr.contains("passphrase", Qt::CaseInsensitive) ||
                errStr.contains("password", Qt::CaseInsensitive) ||
                errStr.contains("encrypted", Qt::CaseInsensitive)) {
                emit passphraseRequired(m_filePath);
            } else {
                emit error(m_filePath, errStr.isEmpty() ? tr("Unable to open archive") : errStr);
            }
        }
        return;
    }

    struct archive_entry *entry;
    m_entries.clear();
    m_dirStats.clear();
    qint64 totalUncompressed = 0;
    QString formatName;
    QString filterName;
    bool hadEncryptedError = false;

    while (!m_isCancelled.load(std::memory_order_relaxed)) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
            QString err = QString::fromUtf8(archive_error_string(a));
            if (err.contains("passphrase", Qt::CaseInsensitive) ||
                err.contains("password", Qt::CaseInsensitive) ||
                err.contains("encrypted", Qt::CaseInsensitive)) {
                hadEncryptedError = true;
            }
            break;
        }

        if (formatName.isEmpty()) {
            const char *fn = archive_format_name(a);
            if (fn) formatName = QString::fromUtf8(fn);
            const char *flt = archive_filter_name(a, 0);
            if (flt) filterName = QString::fromUtf8(flt);
        }

        QString entryPath = QString::fromUtf8(archive_entry_pathname_utf8(entry));
        if (entryPath.isEmpty()) {
            entryPath = QString::fromLocal8Bit(archive_entry_pathname(entry));
        }

        entryPath.replace('\\', '/');
        while (entryPath.startsWith('/')) entryPath.remove(0, 1);
        while (entryPath.startsWith("./")) entryPath.remove(0, 2);
        if (entryPath.isEmpty() || entryPath == ".") continue;

        mode_t fileType = archive_entry_filetype(entry);
        bool isDir = S_ISDIR(fileType) || entryPath.endsWith('/');

        qint64 size = archive_entry_size(entry);
        if (!isDir) {
            totalUncompressed += size;
        }

        time_t mtime = archive_entry_mtime(entry);
        QDateTime dt = QDateTime::fromSecsSinceEpoch(mtime);

        bool isEncrypted = (archive_entry_is_encrypted(entry) != 0);

        m_entries.append({entryPath, isDir, size, dt, isEncrypted});

        // Pre-calculate directory sizes & item counts off the GUI thread
        if (!isDir) {
            QString clean = entryPath;
            if (clean.endsWith('/')) clean.chop(1);
            int lastSlash = clean.lastIndexOf('/');
            while (lastSlash != -1) {
                QString dirPart = clean.left(lastSlash);
                auto &st = m_dirStats[dirPart];
                st.count++;
                st.totalSize += size;
                if (dt.isValid() && (!st.latestMtime.isValid() || dt > st.latestMtime)) {
                    st.latestMtime = dt;
                }
                lastSlash = dirPart.lastIndexOf('/');
            }
        }
    }

    if (formatName.isEmpty()) {
        const char *fn = archive_format_name(a);
        if (fn) formatName = QString::fromUtf8(fn);
        const char *flt = archive_filter_name(a, 0);
        if (flt) filterName = QString::fromUtf8(flt);
    }

    archive_read_close(a);
    archive_read_free(a);

    if (m_isCancelled.load(std::memory_order_relaxed)) {
        return;
    }

    if (hadEncryptedError && m_entries.isEmpty() && m_passphrase.isEmpty()) {
        emit passphraseRequired(m_filePath);
        return;
    }

    // Sort entries in background thread so directories come first, then files with natural numeric collation
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_entries.begin(), m_entries.end(), [&collator](const ArchiveItemRecord &a, const ArchiveItemRecord &b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return collator.compare(a.fullPath, b.fullPath) < 0;
    });

    emit parsed(m_filePath, totalUncompressed, formatName, filterName);
}

// ==========================================
// ArchiveViewer
// ==========================================
ArchiveViewer::ArchiveViewer(QWidget *parent)
    : QWidget(parent)
    , m_topBar(nullptr)
    , m_pillBadge(nullptr)
    , m_pillText(nullptr)
    , m_pillIcon(nullptr)
    , m_searchEdit(nullptr)
    , m_addBtn(nullptr)
    , m_extractBtn(nullptr)
    , m_testBtn(nullptr)
    , m_treeWidget(nullptr)
    , m_worker(nullptr)
    , m_entryCount(0)
    , m_totalSize(0)
{
    setupUi();
}

ArchiveViewer::~ArchiveViewer()
{
    if (m_worker) {
        m_worker->disconnect(this);
        m_worker->cancel();
        m_worker->wait(1000);
        m_worker = nullptr;
    }
    cleanupPreviewCache();
}

void ArchiveViewer::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top Bar with Pill Badge, Filter Box, and Action Controls
    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(46);

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(12, 6, 12, 6);
    topLayout->setSpacing(8);

    m_pillBadge = new QFrame(m_topBar);
    m_pillBadge->setObjectName("ArchivePillBadge");
    m_pillBadge->setFixedHeight(26);

    QHBoxLayout *pillInnerLayout = new QHBoxLayout(m_pillBadge);
    pillInnerLayout->setContentsMargins(12, 0, 14, 0);
    pillInnerLayout->setSpacing(6);
    pillInnerLayout->setAlignment(Qt::AlignVCenter);

    m_pillIcon = new QLabel(m_pillBadge);
    m_pillIcon->setPixmap(createVectorIcon(IconType::Zip).pixmap(14, 14));
    m_pillIcon->setStyleSheet("border: none; background: transparent;");
    pillInnerLayout->addWidget(m_pillIcon);

    m_pillText = new QLabel(m_pillBadge);
    pillInnerLayout->addWidget(m_pillText);

    topLayout->addWidget(m_pillBadge);
    topLayout->addStretch(1);

    // Live search filter
    m_searchEdit = new QLineEdit(m_topBar);
    m_searchEdit->setPlaceholderText(tr("Search in archive..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(180);
    m_searchEdit->setFixedHeight(26);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ArchiveViewer::onFilterTextChanged);
    topLayout->addWidget(m_searchEdit);

    // Add / Modify dropdown action button
    m_addBtn = new QPushButton(tr("+ Add / Edit..."), m_topBar);
    m_addBtn->setFixedHeight(26);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    QMenu *addMenu = new QMenu(m_addBtn);
    QAction *actAddFiles = addMenu->addAction(QIcon::fromTheme("list-add"), tr("Add Files to Archive..."));
    QAction *actAddFolder = addMenu->addAction(QIcon::fromTheme("folder-new"), tr("Add Folder to Archive..."));
    addMenu->addSeparator();
    QAction *actRemSel = addMenu->addAction(QIcon::fromTheme("edit-delete"), tr("Remove Selected from Archive"));
    connect(actAddFiles, &QAction::triggered, this, &ArchiveViewer::addFilesToArchive);
    connect(actAddFolder, &QAction::triggered, this, &ArchiveViewer::addFolderToArchive);
    connect(actRemSel, &QAction::triggered, this, &ArchiveViewer::removeSelectedFromArchive);
    m_addBtn->setMenu(addMenu);
    topLayout->addWidget(m_addBtn);

    // Extraction dropdown action button
    m_extractBtn = new QPushButton(tr("Extract..."), m_topBar);
    m_extractBtn->setFixedHeight(26);
    m_extractBtn->setCursor(Qt::PointingHandCursor);
    QMenu *extractMenu = new QMenu(m_extractBtn);
    QAction *actAll = extractMenu->addAction(tr("Extract All..."));
    QAction *actSel = extractMenu->addAction(tr("Extract Selected..."));
    connect(actAll, &QAction::triggered, this, [this]() { extractAll(); });
    connect(actSel, &QAction::triggered, this, [this]() { extractSelected(); });
    m_extractBtn->setMenu(extractMenu);
    topLayout->addWidget(m_extractBtn);

    // Integrity check action button
    m_testBtn = new QPushButton(tr("Test"), m_topBar);
    m_testBtn->setFixedHeight(26);
    m_testBtn->setToolTip(tr("Test archive integrity"));
    m_testBtn->setCursor(Qt::PointingHandCursor);
    connect(m_testBtn, &QPushButton::clicked, this, &ArchiveViewer::testArchiveIntegrity);
    topLayout->addWidget(m_testBtn);

    mainLayout->addWidget(m_topBar);

    // Tree Widget (hierarchical clean display)
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setStyle(new CleanBranchStyle());
    m_treeWidget->setColumnCount(4);
    m_treeWidget->setHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Modified")});
    m_treeWidget->setIndentation(18);
    m_treeWidget->setAnimated(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setAlternatingRowColors(false);
    m_treeWidget->setFrameShape(QFrame::NoFrame);
    m_treeWidget->setFocusPolicy(Qt::NoFocus);
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, &ArchiveViewer::onContextMenuRequested);

    QHeaderView *header = m_treeWidget->header();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setStretchLastSection(false);
    header->setSortIndicatorShown(true);
    header->setSortIndicator(0, Qt::AscendingOrder);
    header->setHighlightSections(false);
    header->setSectionsClickable(true);
    connect(header, &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        m_treeWidget->setSortingEnabled(true);
        m_treeWidget->sortByColumn(logicalIndex, m_treeWidget->header()->sortIndicatorOrder());
    });

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &ArchiveViewer::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemActivated, this, &ArchiveViewer::onItemDoubleClicked);

    mainLayout->addWidget(m_treeWidget);

    connect(this, &ArchiveViewer::filePreviewRequested, this, [this](const QString &t, const QString &a, const QString &e) {
        if (m_filePreviewCallback) m_filePreviewCallback(t, a, e);
    });
    connect(this, &ArchiveViewer::archiveLoaded, this, [this](const QString &a, int c, qint64 s) {
        if (m_archiveLoadedCallback) m_archiveLoadedCallback(a, c, s);
    });

    updateTheme();
}

QIcon ArchiveViewer::createVectorIcon(IconType type) const
{
    static QMap<IconType, QIcon> s_iconCache;
    auto it = s_iconCache.find(type);
    if (it != s_iconCache.end()) {
        return it.value();
    }

    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QColor iconColor("#9da4af");
    QPen pen(iconColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);

    switch (type) {
    case IconType::Folder: {
        QPainterPath path;
        path.moveTo(3, 8);
        path.lineTo(3, 20);
        path.lineTo(21, 20);
        path.lineTo(21, 9);
        path.lineTo(12, 9);
        path.lineTo(10, 6);
        path.lineTo(3, 6);
        path.closeSubpath();
        p.strokePath(path, pen);
        break;
    }
    case IconType::Code: {
        // <
        QPainterPath p1;
        p1.moveTo(8, 8);
        p1.lineTo(4, 12);
        p1.lineTo(8, 16);
        p.strokePath(p1, pen);
        // /
        p.drawLine(14, 7, 10, 17);
        // >
        QPainterPath p2;
        p2.moveTo(16, 8);
        p2.lineTo(20, 12);
        p2.lineTo(16, 16);
        p.strokePath(p2, pen);
        break;
    }
    case IconType::Zip: {
        QPainterPath path;
        path.moveTo(5, 4);
        path.lineTo(19, 4);
        path.lineTo(19, 20);
        path.lineTo(5, 20);
        path.closeSubpath();
        p.strokePath(path, pen);
        p.drawLine(12, 4, 12, 14);
        for (int y = 6; y <= 12; y += 2) {
            p.drawLine(10, y, 14, y);
        }
        p.drawRect(10, 14, 4, 4);
        break;
    }
    case IconType::Image: {
        QPainterPath path;
        path.addRoundedRect(QRectF(4, 5, 16, 14), 2, 2);
        p.strokePath(path, pen);
        p.drawEllipse(7, 8, 2, 2);
        QPainterPath m;
        m.moveTo(6, 17);
        m.lineTo(10, 12);
        m.lineTo(13, 15);
        m.lineTo(16, 11);
        m.lineTo(18, 15);
        p.strokePath(m, pen);
        break;
    }
    case IconType::Media: {
        p.drawEllipse(6, 13, 5, 4);
        p.drawLine(11, 15, 11, 6);
        p.drawLine(11, 6, 17, 4);
        p.drawLine(17, 4, 17, 13);
        p.drawEllipse(12, 11, 5, 4);
        break;
    }
    case IconType::Document:
    default: {
        QPainterPath path;
        path.moveTo(5, 4);
        path.lineTo(14, 4);
        path.lineTo(19, 9);
        path.lineTo(19, 20);
        path.lineTo(5, 20);
        path.closeSubpath();
        p.strokePath(path, pen);
        p.drawLine(14, 4, 14, 9);
        p.drawLine(14, 9, 19, 9);
        p.drawLine(8, 13, 16, 13);
        p.drawLine(8, 16, 14, 16);
        break;
    }
    case IconType::Locked: {
        p.drawRoundedRect(6, 10, 12, 10, 2, 2);
        QPainterPath shackle;
        shackle.moveTo(8, 10);
        shackle.lineTo(8, 7);
        shackle.arcTo(8, 3, 8, 8, 180, -180);
        shackle.lineTo(16, 10);
        p.strokePath(shackle, pen);
        p.drawLine(12, 13, 12, 16);
        break;
    }
    }

    p.end();
    QIcon icon(pix);
    s_iconCache.insert(type, icon);
    return icon;
}

ArchiveViewer::IconType ArchiveViewer::detectIconType(const QString &fileName, bool isDir) const
{
    if (isDir) return IconType::Folder;

    QString ext = QFileInfo(fileName).suffix().toLower();

    static const QStringList codeExts = {
        "py", "cpp", "c", "h", "hpp", "cc", "cxx", "js", "ts", "html", "htm",
        "css", "scss", "json", "xml", "sh", "bash", "php", "rb", "rs", "go",
        "java", "cs", "lua", "yaml", "yml", "sql", "cmake", "ui", "qml"
    };
    if (codeExts.contains(ext)) return IconType::Code;

    static const QStringList zipExts = {
        "zip", "tar", "gz", "bz2", "xz", "7z", "rar", "zst", "tgz", "iso", "deb", "rpm", "cpio"
    };
    if (zipExts.contains(ext)) return IconType::Zip;

    static const QStringList imageExts = {
        "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "ico", "tiff", "kra", "psd", "xcf"
    };
    if (imageExts.contains(ext)) return IconType::Image;

    static const QStringList mediaExts = {
        "mp3", "wav", "flac", "ogg", "m4a", "aac", "opus", "mp4", "mkv", "avi", "mov", "webm"
    };
    if (mediaExts.contains(ext)) return IconType::Media;

    return IconType::Document;
}

void ArchiveViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::SystemNative) {
        // Native System Styling: use QPalette values and system desktop styling
        m_topBar->setStyleSheet("background-color: palette(window); border-bottom: 1px solid palette(mid);");
        m_pillBadge->setStyleSheet(
            "#ArchivePillBadge {"
            "   background-color: palette(button);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: palette(window-text); font-size: 11px; font-weight: 500;");

        m_searchEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: palette(base);"
            "   color: palette(text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 13px;"
            "   padding: 2px 8px;"
            "   font-size: 11px;"
            "}"
        );

        QString btnStyleNative =
            "QPushButton {"
            "   background-color: palette(button);"
            "   color: palette(button-text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 13px;"
            "   padding: 2px 10px;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: palette(highlight);"
            "   color: palette(highlighted-text);"
            "}";
        if (m_addBtn) m_addBtn->setStyleSheet(btnStyleNative);
        m_extractBtn->setStyleSheet(btnStyleNative);
        m_testBtn->setStyleSheet(btnStyleNative);

        m_treeWidget->setStyleSheet(
            "QTreeWidget {"
            "   background-color: palette(base);"
            "   color: palette(text);"
            "   border: none;"
            "   font-size: 13px;"
            "}"
            "QTreeWidget::item {"
            "   height: 36px;"
            "   padding-left: 6px;"
            "   padding-right: 12px;"
            "   border-bottom: 1px solid palette(alternate-base);"
            "}"
            "QTreeWidget::item:hover {"
            "   background-color: palette(alternate-base);"
            "}"
            "QTreeWidget::item:selected {"
            "   background-color: palette(highlight);"
            "   color: palette(highlighted-text);"
            "}"
            "QHeaderView::section {"
            "   background-color: palette(button);"
            "   color: palette(button-text);"
            "   padding-left: 12px;"
            "   padding-right: 12px;"
            "   padding-top: 6px;"
            "   padding-bottom: 6px;"
            "   border: none;"
            "   border-right: 1px solid palette(mid);"
            "   border-bottom: 1px solid palette(mid);"
            "   font-weight: 500;"
            "   font-size: 12px;"
            "}"
            "QHeaderView::section:last {"
            "   border-right: none;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );
    } else if (preset == ThemePreset::DarkSlate) {
        // Dark Slate Theme
        m_topBar->setStyleSheet("background-color: #22262b; border-bottom: 1px solid #2a2f35;");
        m_pillBadge->setStyleSheet(
            "#ArchivePillBadge {"
            "   background-color: #3b3e44;"
            "   border: 1px solid #4a4f56;"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #e2e6eb; font-size: 11px; font-weight: 500;");

        m_searchEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #2b3036;"
            "   color: #e2e6eb;"
            "   border: 1px solid #3b4149;"
            "   border-radius: 13px;"
            "   padding: 2px 8px;"
            "   font-size: 11px;"
            "}"
        );

        QString btnStyleDarkSlate =
            "QPushButton {"
            "   background-color: #3b3e44;"
            "   color: #e2e6eb;"
            "   border: 1px solid #4a4f56;"
            "   border-radius: 13px;"
            "   padding: 2px 10px;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #4b515b;"
            "}";
        if (m_addBtn) m_addBtn->setStyleSheet(btnStyleDarkSlate);
        m_extractBtn->setStyleSheet(btnStyleDarkSlate);
        m_testBtn->setStyleSheet(btnStyleDarkSlate);

        m_treeWidget->setStyleSheet(
            "QTreeWidget {"
            "   background-color: #22262b;"
            "   color: #eff0f1;"
            "   border: none;"
            "   font-size: 13px;"
            "}"
            "QTreeWidget::item {"
            "   height: 38px;"
            "   padding-left: 6px;"
            "   padding-right: 12px;"
            "   border-bottom: 1px solid #292d34;"
            "}"
            "QTreeWidget::item:hover {"
            "   background-color: #2a2f36;"
            "}"
            "QTreeWidget::item:selected {"
            "   background-color: #383c42;"
            "   color: #ffffff;"
            "}"
            "QHeaderView::section {"
            "   background-color: #22262b;"
            "   color: #a1a8b2;"
            "   padding-left: 12px;"
            "   padding-right: 12px;"
            "   padding-top: 6px;"
            "   padding-bottom: 6px;"
            "   border: none;"
            "   border-right: 1px solid #2a2f35;"
            "   border-bottom: 1px solid #2e333a;"
            "   font-weight: 500;"
            "   font-size: 12px;"
            "}"
            "QHeaderView::section:last {"
            "   border-right: none;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );
    } else if (preset == ThemePreset::BreezeDark) {
        // Breeze Dark Theme
        m_topBar->setStyleSheet("background-color: #181b1d; border-bottom: 1px solid #23272b;");
        m_pillBadge->setStyleSheet(
            "#ArchivePillBadge {"
            "   background-color: #282e34;"
            "   border: 1px solid #363d45;"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #eff0f1; font-size: 11px; font-weight: 500;");

        m_searchEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #23272b;"
            "   color: #eff0f1;"
            "   border: 1px solid #363d45;"
            "   border-radius: 13px;"
            "   padding: 2px 8px;"
            "   font-size: 11px;"
            "}"
        );

        QString btnStyleBreezeDark =
            "QPushButton {"
            "   background-color: #282e34;"
            "   color: #eff0f1;"
            "   border: 1px solid #363d45;"
            "   border-radius: 13px;"
            "   padding: 2px 10px;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #38414b;"
            "}";
        if (m_addBtn) m_addBtn->setStyleSheet(btnStyleBreezeDark);
        m_extractBtn->setStyleSheet(btnStyleBreezeDark);
        m_testBtn->setStyleSheet(btnStyleBreezeDark);

        m_treeWidget->setStyleSheet(
            "QTreeWidget {"
            "   background-color: #181b1d;"
            "   color: #eff0f1;"
            "   border: none;"
            "   font-size: 13px;"
            "}"
            "QTreeWidget::item {"
            "   height: 38px;"
            "   padding-left: 6px;"
            "   padding-right: 12px;"
            "   border-bottom: 1px solid #21262b;"
            "}"
            "QTreeWidget::item:hover {"
            "   background-color: #22282f;"
            "}"
            "QTreeWidget::item:selected {"
            "   background-color: #2a3037;"
            "   color: #ffffff;"
            "}"
            "QHeaderView::section {"
            "   background-color: #181b1d;"
            "   color: #98a0aa;"
            "   padding-left: 12px;"
            "   padding-right: 12px;"
            "   padding-top: 6px;"
            "   padding-bottom: 6px;"
            "   border: none;"
            "   border-right: 1px solid #23272b;"
            "   border-bottom: 1px solid #262c32;"
            "   font-weight: 500;"
            "   font-size: 12px;"
            "}"
            "QHeaderView::section:last {"
            "   border-right: none;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );
    }
}

bool ArchiveViewer::loadFile(const QString &filePath, const QString &passphrase)
{
    clear();
    m_currentArchiveFilePath = filePath;
    m_currentPassphrase = passphrase;

    // Show immediate non-blocking progress state
    m_pillText->setText(tr("⏳ Reading archive..."));

    // Safely detach any previous worker
    if (m_worker) {
        m_worker->disconnect(this);
        m_worker->cancel();
        m_worker = nullptr;
    }

    m_worker = new ArchiveWorker(m_currentArchiveFilePath, m_currentPassphrase, nullptr);
    connect(m_worker, &ArchiveWorker::parsed, this, &ArchiveViewer::onArchiveParsed);
    connect(m_worker, &ArchiveWorker::error, this, &ArchiveViewer::onArchiveError);
    connect(m_worker, &ArchiveWorker::passphraseRequired, this, &ArchiveViewer::onPassphraseRequired);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();

    return true;
}

void ArchiveViewer::onPassphraseRequired(const QString &filePath)
{
    if (filePath != m_currentArchiveFilePath) return;

    bool ok = false;
    QString pass = QInputDialog::getText(
        this,
        tr("Encrypted Archive"),
        tr("Enter password to unlock %1:").arg(QFileInfo(filePath).fileName()),
        QLineEdit::Password,
        QString(),
        &ok
    );

    if (ok && !pass.isEmpty()) {
        loadFile(filePath, pass);
    } else {
        m_pillText->setText(tr("🔒 Password required"));
    }
}

void ArchiveViewer::onArchiveParsed(const QString &filePath, qint64 totalSize,
                                   const QString &formatName, const QString &filterName)
{
    ArchiveWorker *worker = qobject_cast<ArchiveWorker*>(sender());
    if (!worker || worker != m_worker || filePath != m_currentArchiveFilePath) {
        return; // Stale worker result from previous file
    }

    QList<ArchiveItemRecord> entries = worker->takeEntries();
    QMap<QString, FolderStats> dirStats = worker->takeDirStats();
    m_entries = entries;

    m_treeWidget->setUpdatesEnabled(false);
    m_treeWidget->setSortingEnabled(false);
    m_treeWidget->clear();
    m_entryCount = entries.size();
    m_totalSize = totalSize;
    m_formatName = formatName;
    m_filterName = filterName;

    QMap<QString, QTreeWidgetItem*> dirNodes;
    QList<QTreeWidgetItem*> topLevelItems;

    for (const auto &rec : entries) {
        QString clean = rec.fullPath;
        if (clean.endsWith('/')) clean.chop(1);
        QStringList parts = clean.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        QString curPath = "";
        QTreeWidgetItem *parentItem = nullptr;

        for (int i = 0; i < parts.size(); ++i) {
            if (i > 0) curPath += "/";
            curPath += parts[i];

            bool isLeaf = (i == parts.size() - 1);

            if (isLeaf && !rec.isDir) {
                // File item
                ArchiveTreeWidgetItem *fileItem = nullptr;
                if (parentItem) {
                    fileItem = new ArchiveTreeWidgetItem(parentItem);
                } else {
                    fileItem = new ArchiveTreeWidgetItem();
                    topLevelItems.append(fileItem);
                }

                fileItem->setText(0, parts[i]);
                if (rec.isEncrypted) {
                    fileItem->setIcon(0, createVectorIcon(IconType::Locked));
                    fileItem->setToolTip(0, tr("Encrypted entry"));
                } else {
                    fileItem->setIcon(0, createVectorIcon(detectIconType(parts[i], false)));
                }
                fileItem->setText(1, "");
                fileItem->setText(2, formatSize(rec.size));
                fileItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
                fileItem->setText(3, rec.mtime.isValid() ? rec.mtime.toString("yyyy-MM-dd") : "-");

                // Store metadata for custom sorting and operations
                fileItem->setData(0, Qt::UserRole, rec.fullPath);
                fileItem->setData(0, Qt::UserRole + 1, false);
                fileItem->setData(0, Qt::UserRole + 2, rec.isEncrypted);
                fileItem->setData(1, Qt::UserRole, 0);
                fileItem->setData(2, Qt::UserRole, rec.size);
                fileItem->setData(3, Qt::UserRole, rec.mtime);
            } else {
                // Directory node
                if (!dirNodes.contains(curPath)) {
                    ArchiveTreeWidgetItem *dirItem = nullptr;
                    if (parentItem) {
                        dirItem = new ArchiveTreeWidgetItem(parentItem);
                    } else {
                        dirItem = new ArchiveTreeWidgetItem();
                        topLevelItems.append(dirItem);
                    }

                    dirItem->setText(0, parts[i]);
                    dirItem->setIcon(0, createVectorIcon(IconType::Folder));

                    FolderStats stats = dirStats.value(curPath);
                    if (stats.count > 0) {
                        dirItem->setText(1, tr("%1 items").arg(stats.count));
                        dirItem->setText(2, formatSize(stats.totalSize));
                    } else {
                        dirItem->setText(1, tr("0 items"));
                        dirItem->setText(2, "-");
                    }
                    dirItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);

                    QDateTime folderDt = stats.latestMtime.isValid() ? stats.latestMtime : rec.mtime;
                    dirItem->setText(3, folderDt.isValid() ? folderDt.toString("yyyy-MM-dd") : "-");

                    // Store metadata for custom sorting and operations
                    dirItem->setData(0, Qt::UserRole, curPath);
                    dirItem->setData(0, Qt::UserRole + 1, true);
                    dirItem->setData(1, Qt::UserRole, stats.count);
                    dirItem->setData(2, Qt::UserRole, stats.totalSize);
                    dirItem->setData(3, Qt::UserRole, folderDt);

                    dirNodes[curPath] = dirItem;
                }
                parentItem = dirNodes[curPath];
            }
        }
    }

    // Fast batch insertion into tree widget
    m_treeWidget->addTopLevelItems(topLevelItems);

    // Smart expansion: if archive is large (>50 items), expand only top level to avoid layout freezes
    if (m_entryCount <= 50) {
        m_treeWidget->expandAll();
    } else {
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            m_treeWidget->topLevelItem(i)->setExpanded(true);
        }
    }
    m_treeWidget->setUpdatesEnabled(true);

    // Update Pill Badge with format and compression algorithm details
    QString formatDisplay = m_formatName;
    if (!m_filterName.isEmpty() && m_filterName != "none") {
        formatDisplay += QString(" (%1)").arg(m_filterName);
    }
    if (formatDisplay.isEmpty()) {
        m_pillText->setText(tr("%1 Items (%2 Total)").arg(m_entryCount).arg(formatSize(m_totalSize)));
    } else {
        m_pillText->setText(tr("%1 • %2 Items (%3 Total)").arg(formatDisplay).arg(m_entryCount).arg(formatSize(m_totalSize)));
    }

    m_pillBadge->setToolTip(
        QString(tr("Archive: %1\nFormat: %2\nCompression: %3\nItems: %4\nTotal Size: %5"))
            .arg(QFileInfo(m_currentArchiveFilePath).fileName())
            .arg(m_formatName.isEmpty() ? "-" : m_formatName)
            .arg(m_filterName.isEmpty() ? "-" : m_filterName)
            .arg(m_entryCount)
            .arg(formatSize(m_totalSize))
    );

    // Update editability of action buttons based on archive format capabilities
    const bool canEdit = isArchiveEditable();
    if (m_addBtn) {
        m_addBtn->setEnabled(canEdit);
        m_addBtn->setToolTip(canEdit ? tr("Add or remove files in this archive")
                                     : tr("This archive format is read-only and cannot be modified in-place"));
    }

    emit archiveLoaded(m_currentArchiveFilePath, m_entryCount, m_totalSize);
}

void ArchiveViewer::onArchiveError(const QString &filePath, const QString &errorMsg)
{
    if (filePath != m_currentArchiveFilePath) return;
    m_pillText->setText(QString("⚠️ %1").arg(errorMsg));
}

void ArchiveViewer::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    bool isDir = item->data(0, Qt::UserRole + 1).toBool();
    if (isDir) {
        // Toggle folder expansion
        item->setExpanded(!item->isExpanded());
        return;
    }

    bool isEncrypted = item->data(0, Qt::UserRole + 2).toBool();
    if (isEncrypted && m_currentPassphrase.isEmpty()) {
        bool ok = false;
        QString pass = QInputDialog::getText(
            this,
            tr("Encrypted File"),
            tr("Enter password to view this file:"),
            QLineEdit::Password,
            QString(),
            &ok
        );
        if (!ok || pass.isEmpty()) {
            return;
        }
        m_currentPassphrase = pass;
    }

    QString internalName = item->data(0, Qt::UserRole).toString();
    if (internalName.isEmpty() || m_currentArchiveFilePath.isEmpty()) return;

    // Extract single targeted file
    QString extracted = extractSingleFile(m_currentArchiveFilePath, internalName);
    if (!extracted.isEmpty() && QFile::exists(extracted)) {
        emit filePreviewRequested(extracted, m_currentArchiveFilePath, internalName);
    }
}

void ArchiveViewer::onFilterTextChanged(const QString &pattern)
{
    QString trimmed = pattern.trimmed();
    std::function<bool(QTreeWidgetItem*)> filterItem = [&](QTreeWidgetItem *item) -> bool {
        bool match = trimmed.isEmpty() || item->text(0).contains(trimmed, Qt::CaseInsensitive);
        bool anyChildVisible = false;
        for (int i = 0; i < item->childCount(); ++i) {
            if (filterItem(item->child(i))) {
                anyChildVisible = true;
            }
        }
        bool visible = match || anyChildVisible;
        item->setHidden(!visible);
        if (!trimmed.isEmpty() && visible && anyChildVisible) {
            item->setExpanded(true);
        }
        return visible;
    };

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        filterItem(m_treeWidget->topLevelItem(i));
    }
}

void ArchiveViewer::onContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    QMenu menu(this);

    if (item) {
        bool isDir = item->data(0, Qt::UserRole + 1).toBool();
        QString internalPath = item->data(0, Qt::UserRole).toString();

        if (!isDir) {
            QAction *previewAct = menu.addAction(QIcon::fromTheme("document-open"), tr("Preview"));
            connect(previewAct, &QAction::triggered, this, [this, item]() {
                onItemDoubleClicked(item, 0);
            });
        }

        QAction *extractAct = menu.addAction(QIcon::fromTheme("archive-extract"), tr("Extract Selected..."));
        connect(extractAct, &QAction::triggered, this, [this]() {
            extractSelected();
        });

        menu.addSeparator();

        QAction *copyPathAct = menu.addAction(QIcon::fromTheme("edit-copy-path"), tr("Copy Path"));
        connect(copyPathAct, &QAction::triggered, this, [internalPath]() {
            QApplication::clipboard()->setText(internalPath);
        });

        menu.addSeparator();
    }

    const bool canEdit = isArchiveEditable();
    QAction *addFilesAct = menu.addAction(QIcon::fromTheme("list-add"), tr("Add Files to Archive..."));
    addFilesAct->setEnabled(canEdit);
    connect(addFilesAct, &QAction::triggered, this, &ArchiveViewer::addFilesToArchive);

    QAction *addFolderAct = menu.addAction(QIcon::fromTheme("folder-new"), tr("Add Folder to Archive..."));
    addFolderAct->setEnabled(canEdit);
    connect(addFolderAct, &QAction::triggered, this, &ArchiveViewer::addFolderToArchive);

    if (item || !m_treeWidget->selectedItems().isEmpty()) {
        QAction *removeAct = menu.addAction(QIcon::fromTheme("edit-delete"), tr("Remove Selected from Archive"));
        removeAct->setEnabled(canEdit);
        connect(removeAct, &QAction::triggered, this, &ArchiveViewer::removeSelectedFromArchive);
    }

    menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));
}

namespace {
struct DiskSourceItem {
    QString diskPath;
    QString archiveRelPath;
    bool isDir = false;
    qint64 fileSize = 0;
};

QString formatByteSpeed(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

/**
 * @brief Expands a list of user-selected file/folder paths into flat archive entry records.
 */
QList<DiskSourceItem> collectDiskItems(const QStringList &sourcePaths, const QString &targetPrefix = QString(), qint64 *outTotalBytes = nullptr)
{
    QList<DiskSourceItem> items;
    qint64 totalBytes = 0;
    QString prefix = targetPrefix;
    while (prefix.startsWith('/')) prefix.remove(0, 1);
    if (!prefix.isEmpty() && !prefix.endsWith('/')) prefix += '/';

    for (const QString &src : sourcePaths) {
        QFileInfo fi(src);
        if (!fi.exists()) continue;

        if (fi.isFile()) {
            qint64 sz = fi.size();
            totalBytes += sz;
            items.append({fi.absoluteFilePath(), prefix + fi.fileName(), false, sz});
        } else if (fi.isDir()) {
            QDir baseDir(fi.absoluteFilePath());
            QString folderName = fi.fileName();
            if (folderName.isEmpty()) folderName = QStringLiteral("folder");
            QString rootRel = prefix + folderName;
            items.append({fi.absoluteFilePath(), rootRel + "/", true, 0});

            QDirIterator it(fi.absoluteFilePath(),
                            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString childPath = it.next();
                QFileInfo cfi(childPath);
                QString rel = rootRel + "/" + baseDir.relativeFilePath(childPath);
                rel.replace('\\', '/');
                if (cfi.isDir() && !rel.endsWith('/')) {
                    rel += '/';
                }
                qint64 sz = cfi.isDir() ? 0 : cfi.size();
                totalBytes += sz;
                items.append({cfi.absoluteFilePath(), rel, cfi.isDir(), sz});
            }
        }
    }
    if (outTotalBytes) *outTotalBytes = totalBytes;
    return items;
}

/**
 * @brief Checks if a file is already compressed (e.g. video, audio, compressed image, or archive).
 *        Recompressing these files with Deflate wastes CPU cycles without meaningful size reduction.
 * @param fileName Path or filename to check.
 * @return True if the file has an already-compressed media or archive extension.
 */
bool isAlreadyCompressedMediaOrArchive(const QString &fileName)
{
    static const QSet<QString> compressedExtensions = {
        // Video formats
        QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mov"),
        QStringLiteral("wmv"), QStringLiteral("flv"), QStringLiteral("webm"), QStringLiteral("ts"),
        QStringLiteral("m4v"), QStringLiteral("3gp"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
        QStringLiteral("vob"), QStringLiteral("ogv"), QStringLiteral("m2ts"),

        // Audio formats (lossy and compressed lossless)
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("aac"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("m4a"), QStringLiteral("wma"), QStringLiteral("alac"),

        // Compressed images
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp"), QStringLiteral("png"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("jxl"),

        // Compressed archives and packages
        QStringLiteral("zip"), QStringLiteral("7z"), QStringLiteral("rar"), QStringLiteral("gz"),
        QStringLiteral("xz"), QStringLiteral("zst"), QStringLiteral("bz2"), QStringLiteral("tgz"),
        QStringLiteral("txz"), QStringLiteral("tzst"), QStringLiteral("tar.gz"), QStringLiteral("tar.xz"),
        QStringLiteral("apk"), QStringLiteral("jar"), QStringLiteral("docx"), QStringLiteral("xlsx"),
        QStringLiteral("pptx"), QStringLiteral("epub"), QStringLiteral("cbz"), QStringLiteral("cbr")
    };

    int dotIdx = fileName.lastIndexOf('.');
    if (dotIdx < 0) return false;
    QString ext = fileName.mid(dotIdx + 1).toLower();
    return compressedExtensions.contains(ext);
}

/**
 * @brief Locates the multi-threaded 7-Zip CLI binary (7zz, 7z, or 7za) if available.
 */
QString find7zBinary()
{
    return BuildConfig::sevenZipExecutable();
}

/**
 * @brief Checks if the target format or path is .7z or .zip.
 */
bool is7zOrZipFormat(const QString &formatOrPath, bool *outIs7z = nullptr)
{
    QString lower = formatOrPath.toLower();
    if (lower.endsWith(QStringLiteral(".7z")) || lower == QStringLiteral("7z")) {
        if (outIs7z) *outIs7z = true;
        return true;
    }
    if (lower.endsWith(QStringLiteral(".zip")) || lower == QStringLiteral("zip")) {
        if (outIs7z) *outIs7z = false;
        return true;
    }
    return false;
}

/**
 * @brief Helper to execute a 7-Zip process pass and monitor its output for live progress reporting.
 */
bool execute7zProcessPass(const QString &sevenZBin,
                         const QStringList &args,
                         const QString &workingDir,
                         const QString &defaultItemName,
                         qint64 safeTotalBytes,
                         std::atomic<bool> &cancelled,
                         const std::function<void(qint64, const QString &)> &onByteProgress,
                         QString *outError)
{
    QProcess proc;
    if (!workingDir.isEmpty()) {
        proc.setWorkingDirectory(workingDir);
    }
    proc.start(sevenZBin, args);
    if (!proc.waitForStarted(5000)) {
        if (outError) *outError = QObject::tr("Failed to start multi-core 7z engine.");
        return false;
    }

    QString currentItemName = defaultItemName;
    int lastPct = 0;

    auto parseProgressChunk = [&](const QByteArray &raw) {
        if (raw.isEmpty() || !onByteProgress) return;
        QString text = QString::fromUtf8(raw);
        text.replace('\r', '\n');
        text.replace('\b', ' ');
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            int pctIdx = line.indexOf('%');
            if (pctIdx > 0) {
                int startIdx = pctIdx - 1;
                while (startIdx >= 0 && line[startIdx].isDigit()) {
                    startIdx--;
                }
                bool ok = false;
                int pct = line.mid(startIdx + 1, pctIdx - startIdx - 1).toInt(&ok);
                if (ok && pct >= 0 && pct <= 100) {
                    lastPct = qMax(lastPct, pct);
                }
                int signIdx = line.indexOf(QStringLiteral("+ "), pctIdx);
                if (signIdx < 0) {
                    signIdx = line.indexOf(QStringLiteral("- "), pctIdx);
                }
                if (signIdx > 0) {
                    QString candidate = line.mid(signIdx + 2).trimmed();
                    if (!candidate.isEmpty()) {
                        currentItemName = QFileInfo(candidate).fileName();
                    }
                }
                qint64 estBytes = (static_cast<qint64>(lastPct) * safeTotalBytes) / 100;
                onByteProgress(estBytes, currentItemName);
            }
        }
    };

    while (proc.state() != QProcess::NotRunning) {
        if (cancelled.load(std::memory_order_relaxed)) {
            proc.kill();
            proc.waitForFinished(2000);
            return false;
        }
        if (proc.waitForReadyRead(80)) {
            parseProgressChunk(proc.readAllStandardOutput());
        }
    }

    parseProgressChunk(proc.readAllStandardOutput());

    if (cancelled.load(std::memory_order_relaxed)) {
        return false;
    }

    // 7-Zip return codes: 0 = Normal, 1 = Non-fatal warning (e.g. locked file or symlink), >=2 = Fatal error
    if (proc.exitStatus() != QProcess::NormalExit || (proc.exitCode() != 0 && proc.exitCode() != 1)) {
        if (outError) {
            QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            *outError = err.isEmpty() ? QObject::tr("7-Zip operation exited with code %1").arg(proc.exitCode()) : err;
        }
        return false;
    }

    return true;
}

/**
 * @brief Runs multi-threaded 7-Zip extraction (-mmt=on) across all CPU cores with live progress parsing.
 * @param sevenZBin Absolute or system path to the 7-Zip binary (7zz, 7z, 7za).
 * @param archivePath Path of the archive to extract.
 * @param destDir Target directory where contents will be extracted.
 * @param filterPaths Optional list of specific internal archive entries to extract. If empty, all entries are extracted.
 * @param passphrase Optional password for encrypted archives.
 * @param totalEstimatedBytes Estimated uncompressed size of target items for progress calculation.
 * @param cancelled Flag allowing the user to abort extraction.
 * @param onByteProgress Progress callback reporting processed bytes and the currently extracting file.
 * @param outError Output parameter receiving error description in case of failure.
 * @return True if extraction completed successfully, false otherwise.
 */
bool runMultiCore7zExtraction(const QString &sevenZBin,
                             const QString &archivePath,
                             const QString &destDir,
                             const QStringList &filterPaths,
                             const QString &passphrase,
                             qint64 totalEstimatedBytes,
                             std::atomic<bool> &cancelled,
                             const std::function<void(qint64, const QString &)> &onByteProgress,
                             QString *outError)
{
    QStringList args;
    args << QStringLiteral("x")
         << QStringLiteral("-y")
         << QStringLiteral("-bsp1")
         << QStringLiteral("-mmt=on")
         << QStringLiteral("-o%1").arg(destDir);

    if (!passphrase.isEmpty()) {
        args << QStringLiteral("-p%1").arg(passphrase);
    }

    args << archivePath;

    QTemporaryFile listFile;
    if (!filterPaths.isEmpty()) {
        if (listFile.open()) {
            QTextStream ts(&listFile);
            for (const QString &fp : filterPaths) {
                ts << fp << QStringLiteral("\n");
            }
            listFile.close();
            args << QStringLiteral("-r-")
                 << QStringLiteral("@%1").arg(listFile.fileName());
        }
    }

    QStringList safeArgs = args;
    for (QString &arg : safeArgs) {
        if (arg.startsWith(QStringLiteral("-p")) && arg.length() > 2) {
            arg = QStringLiteral("-p***");
        }
    }
    qDebug() << "[ArchiveViewer] Launching 7-Zip multi-core extraction:" << sevenZBin << safeArgs;

    return execute7zProcessPass(sevenZBin, args, QString(), QStringLiteral("extracting"),
                                totalEstimatedBytes, cancelled, onByteProgress, outError);
}

/**
 * @brief Checks if an archive is a native Linux package or filesystem structure (.tar, .deb, .rpm, .cpio, or Linux ISO).
 *        These archives rely heavily on exact POSIX permissions, ownership, and symlinks, which libarchive handles natively.
 * @param filePath Path to the archive.
 * @param formatName Name of the format reported by libarchive.
 * @param entries List of archive entries.
 * @return True if native Linux archive that must be handled by libarchive.
 */
bool isNativeLinuxArchive(const QString &filePath, const QString &formatName, const QList<ArchiveItemRecord> &entries)
{
    QString lower = filePath.toLower();
    if (lower.endsWith(QStringLiteral(".tar")) || lower.endsWith(QStringLiteral(".tar.gz")) ||
        lower.endsWith(QStringLiteral(".tgz")) || lower.endsWith(QStringLiteral(".tar.xz")) ||
        lower.endsWith(QStringLiteral(".txz")) || lower.endsWith(QStringLiteral(".tar.zst")) ||
        lower.endsWith(QStringLiteral(".tzst")) || lower.endsWith(QStringLiteral(".tar.bz2")) ||
        lower.endsWith(QStringLiteral(".tbz2")) || lower.endsWith(QStringLiteral(".deb")) ||
        lower.endsWith(QStringLiteral(".rpm")) || lower.endsWith(QStringLiteral(".cpio"))) {
        return true;
    }

    if (lower.endsWith(QStringLiteral(".iso"))) {
        if (formatName.contains(QStringLiteral("Rockridge"), Qt::CaseInsensitive)) {
            return true;
        }
        for (const auto &rec : entries) {
            const QString &p = rec.fullPath;
            if (p.startsWith(QStringLiteral("isolinux/"), Qt::CaseInsensitive) ||
                p.startsWith(QStringLiteral("syslinux/"), Qt::CaseInsensitive) ||
                p.startsWith(QStringLiteral("casper/"), Qt::CaseInsensitive) ||
                p.startsWith(QStringLiteral("arch/"), Qt::CaseInsensitive) ||
                p.startsWith(QStringLiteral("live/"), Qt::CaseInsensitive) ||
                p.contains(QStringLiteral("vmlinuz"), Qt::CaseInsensitive) ||
                p.contains(QStringLiteral("initrd"), Qt::CaseInsensitive) ||
                p.endsWith(QStringLiteral(".squashfs"), Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    return false;
}
} // namespace

void ArchiveViewer::extractAll(const QString &destDir)
{
    extractFiles({}, destDir);
}

void ArchiveViewer::extractSelected(const QString &destDir)
{
    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    if (selected.isEmpty()) {
        extractAll(destDir);
        return;
    }
    QStringList paths;
    for (auto *item : selected) {
        QString p = item->data(0, Qt::UserRole).toString();
        if (!p.isEmpty()) {
            paths.append(p);
        }
    }
    extractFiles(paths, destDir);
}

void ArchiveViewer::extractFiles(const QStringList &filterPaths, const QString &destDir)
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        return;
    }

    QString target = destDir;
    if (target.isEmpty()) {
        target = QFileDialog::getExistingDirectory(this, tr("Select Destination Folder"), QDir::homePath());
        if (target.isEmpty()) {
            return;
        }
    }

    const bool isLinuxArchive = isNativeLinuxArchive(m_currentArchiveFilePath, m_formatName, m_entries);
    QString sevenZBin = find7zBinary();

    if (!isLinuxArchive && sevenZBin.isEmpty()) {
        QMessageBox prompt(this);
        prompt.setWindowTitle(tr("7-Zip Engine Recommended"));
        prompt.setIcon(QMessageBox::Information);

        const QString distro = PackageManagerHelper::distroDisplayName();
        const QString installCmd = PackageManagerHelper::getInstallCommand(QStringLiteral("7zip"));
        QFileInfo fi(m_currentArchiveFilePath);
        QString ext = fi.suffix().toUpper();
        if (ext.isEmpty()) ext = tr("this");

        prompt.setText(tr("<h3>7-Zip Engine Recommended</h3>"
                          "<p>For maximum speed, multi-core extraction, and full compatibility with <b>%1</b> archives, <b>7-Zip</b> (<code>7zz</code>) is recommended.</p>"
                          "<p>Detected distribution: <b>%2</b></p>"
                          "<p>Manual installation command:<br><code>%3</code></p>")
                          .arg(ext, distro, installCmd));

        QPushButton *btnInstall = prompt.addButton(tr("Install 7-Zip"), QMessageBox::AcceptRole);
        QPushButton *btnContinue = prompt.addButton(tr("Continue with libarchive"), QMessageBox::ActionRole);
        QPushButton *btnCancel = prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
        Q_UNUSED(btnContinue);
        prompt.setDefaultButton(btnInstall);

        prompt.exec();

        if (prompt.clickedButton() == btnCancel) {
            return;
        }

        if (prompt.clickedButton() == btnInstall) {
            if (PackageManagerHelper::installComponentInteractive(QStringLiteral("7zip"), this)) {
                sevenZBin = find7zBinary();
            }
        }
    }

    if (!isLinuxArchive && !sevenZBin.isEmpty()) {
        QStringList selectedEntryPaths;
        qint64 targetEstimatedBytes = 0;
        int targetEntryCount = 0;

        if (!filterPaths.isEmpty()) {
            for (const auto &rec : m_entries) {
                for (const QString &fp : filterPaths) {
                    QString cleanFp = fp;
                    while (cleanFp.endsWith('/')) cleanFp.chop(1);
                    if (rec.fullPath == fp || rec.fullPath.startsWith(cleanFp + "/")) {
                        selectedEntryPaths.append(rec.fullPath);
                        if (!rec.isDir) {
                            targetEstimatedBytes += rec.size;
                            targetEntryCount++;
                        }
                        break;
                    }
                }
            }
        } else {
            targetEstimatedBytes = m_totalSize;
            targetEntryCount = m_entryCount;
        }

        const qint64 safeTotalBytes = qMax<qint64>(1, targetEstimatedBytes);

        QDialog progDlg(this);
        progDlg.setWindowTitle(tr("7-Zip Multi-Core Extraction"));
        progDlg.setModal(true);
        progDlg.resize(480, 150);

        QVBoxLayout *progLay = new QVBoxLayout(&progDlg);
        progLay->setContentsMargins(20, 18, 20, 18);
        progLay->setSpacing(12);

        QLabel *lblTitle = new QLabel(tr("⚡ 7-Zip Multi-Core Extracting: %1").arg(QFileInfo(m_currentArchiveFilePath).fileName()), &progDlg);
        lblTitle->setStyleSheet("font-weight: bold; font-size: 14px;");

        QLabel *lblStatus = new QLabel(tr("Initializing multi-core 7-Zip engine..."), &progDlg);
        lblStatus->setStyleSheet("color: #94a3b8; font-size: 12px;");

        QProgressBar *bar = new QProgressBar(&progDlg);
        bar->setRange(0, 1000);
        bar->setValue(0);
        bar->setTextVisible(true);
        bar->setFormat("%p%");
        bar->setStyleSheet(
            "QProgressBar { background-color: rgba(100, 116, 139, 0.2); border: 1px solid rgba(100, 116, 139, 0.35); border-radius: 6px; height: 22px; text-align: center; font-weight: bold; }"
            "QProgressBar::chunk { background-color: #0284c7; border-radius: 5px; }"
        );

        QHBoxLayout *btnLay = new QHBoxLayout();
        btnLay->addStretch();
        QPushButton *btnCancel = new QPushButton(tr("Cancel"), &progDlg);
        btnLay->addWidget(btnCancel);

        progLay->addWidget(lblTitle);
        progLay->addWidget(bar);
        progLay->addWidget(lblStatus);
        progLay->addLayout(btnLay);

        std::atomic<bool> cancelled{false};
        bool workerOk = true;
        QString workerError;
        QElapsedTimer timer;
        timer.start();

        connect(btnCancel, &QPushButton::clicked, &progDlg, [&]() {
            cancelled.store(true, std::memory_order_relaxed);
            btnCancel->setEnabled(false);
            lblStatus->setText(tr("Cancelling..."));
        });
        connect(&progDlg, &QDialog::rejected, &progDlg, [&]() {
            cancelled.store(true, std::memory_order_relaxed);
        });

        auto reportProgress = [&](qint64 doneBytes, const QString &currentName) {
            qint64 elapsedMs = qMax<qint64>(1, timer.elapsed());
            double mbPerSec = (doneBytes / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);
            int permille = static_cast<int>(std::clamp<qint64>((doneBytes * 1000) / safeTotalBytes, 0, 995));
            QMetaObject::invokeMethod(&progDlg, [bar, lblStatus, permille, doneBytes, safeTotalBytes, mbPerSec, currentName]() {
                bar->setValue(permille);
                lblStatus->setText(
                    QStringLiteral("⚡ %1 — %2 / %3 (%4 MB/s)")
                        .arg(currentName, formatByteSpeed(doneBytes), formatByteSpeed(safeTotalBytes))
                        .arg(mbPerSec, 0, 'f', 1)
                );
            }, Qt::QueuedConnection);
        };

        QEventLoop loop;
        QThread *worker = QThread::create([&]() {
            workerOk = runMultiCore7zExtraction(
                sevenZBin, m_currentArchiveFilePath, target, selectedEntryPaths,
                m_currentPassphrase, safeTotalBytes, cancelled, reportProgress, &workerError
            );
        });

        connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        connect(worker, &QThread::finished, worker, &QThread::deleteLater);

        worker->start();
        progDlg.show();
        loop.exec();

        if (cancelled.load(std::memory_order_relaxed)) {
            return;
        }

        if (workerOk) {
            if (destDir.isEmpty()) {
                QMessageBox::information(this, tr("Extraction Complete"),
                                         tr("Successfully extracted %1 items using the 7-Zip engine to:\n%2")
                                             .arg(targetEntryCount > 0 ? targetEntryCount : m_entryCount)
                                             .arg(target));
            }
            return;
        }

        qWarning() << "7-Zip extraction encountered an issue, falling back to libarchive:" << workerError;
    }

    qDebug() << "[ArchiveViewer] Proceeding with libarchive extraction for:" << m_currentArchiveFilePath;
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (!m_currentPassphrase.isEmpty()) {
        archive_read_add_passphrase(a, m_currentPassphrase.toUtf8().constData());
    }

    int r = archive_read_open_filename(a, m_currentArchiveFilePath.toUtf8().constData(), 10240);
    if (r != ARCHIVE_OK) {
        QString err = QString::fromUtf8(archive_error_string(a));
        archive_read_free(a);
        QMessageBox::warning(this, tr("Extraction Failed"), tr("Could not open archive: %1").arg(err));
        return;
    }

    struct archive *ext = archive_write_disk_new();
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    int totalEstimate = m_entryCount > 0 ? m_entryCount : 100;
    QProgressDialog progress(tr("Extracting files..."), tr("Cancel"), 0, totalEstimate, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(200);

    struct archive_entry *entry;
    int processed = 0;
    int extractedCount = 0;

    QString lastError;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN) {
        processed++;
        progress.setValue(processed);
        if (progress.wasCanceled()) {
            break;
        }

        QString entryPath = QString::fromUtf8(archive_entry_pathname_utf8(entry));
        if (entryPath.isEmpty()) {
            entryPath = QString::fromLocal8Bit(archive_entry_pathname(entry));
        }
        entryPath.replace('\\', '/');
        while (entryPath.startsWith('/')) entryPath.remove(0, 1);
        while (entryPath.startsWith("./")) entryPath.remove(0, 2);

        // Security check: skip unsafe entries attempting path traversal
        QString cleanRel = QDir::cleanPath(entryPath);
        if (cleanRel.isEmpty() || cleanRel == "." || cleanRel.startsWith("../") || cleanRel.contains("/../") || cleanRel == "..") {
            qWarning() << "[ArchiveViewer] Security skipping entry with path traversal:" << entryPath;
            archive_read_data_skip(a);
            continue;
        }

        // Filter if selective extraction
        if (!filterPaths.isEmpty()) {
            bool matches = false;
            for (const QString &fp : filterPaths) {
                if (cleanRel == fp || cleanRel.startsWith(fp + "/")) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                archive_read_data_skip(a);
                continue;
            }
        }

        QString fullDest = QDir(target).filePath(cleanRel);
        archive_entry_set_pathname(entry, fullDest.toUtf8().constData());

        // Redirect hardlink targets safely inside the destination directory
        const char *hl = archive_entry_hardlink(entry);
        if (hl) {
            QString hardlinkTarget = QString::fromUtf8(hl);
            hardlinkTarget.replace('\\', '/');
            while (hardlinkTarget.startsWith('/')) hardlinkTarget.remove(0, 1);
            while (hardlinkTarget.startsWith("./")) hardlinkTarget.remove(0, 2);
            QString cleanHl = QDir::cleanPath(hardlinkTarget);
            QString fullHl = QDir(target).filePath(cleanHl);
            archive_entry_set_hardlink(entry, fullHl.toUtf8().constData());
        }

        progress.setLabelText(tr("Extracting: %1").arg(QFileInfo(fullDest).fileName()));
        QApplication::processEvents();

        r = archive_write_header(ext, entry);
        if (r >= ARCHIVE_OK) {
            const void *buff;
            size_t size;
            int64_t offset;
            int rBlock = ARCHIVE_OK;
            while ((rBlock = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
                archive_write_data_block(ext, buff, size, offset);
            }
            if (rBlock != ARCHIVE_EOF && rBlock != ARCHIVE_OK) {
                lastError = QString::fromUtf8(archive_error_string(a));
            }
            archive_write_finish_entry(ext);
            extractedCount++;
        } else {
            lastError = QString::fromUtf8(archive_error_string(ext));
        }
    }

    if (r != ARCHIVE_EOF && r != ARCHIVE_OK && r != ARCHIVE_WARN) {
        lastError = QString::fromUtf8(archive_error_string(a));
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    progress.setValue(progress.maximum());

    if (!progress.wasCanceled()) {
        if (!lastError.isEmpty() && extractedCount == 0) {
            QMessageBox::warning(this, tr("Extraction Failed"),
                                 tr("Could not extract files:\n%1").arg(lastError));
        } else if (destDir.isEmpty()) {
            if (!lastError.isEmpty()) {
                QMessageBox::warning(this, tr("Extraction Completed with Warnings"),
                                     tr("Extracted %1 items, but encountered errors:\n%2").arg(extractedCount).arg(lastError));
            } else {
                QMessageBox::information(this, tr("Extraction Complete"),
                                         tr("Successfully extracted %1 items using libarchive engine to:\n%2").arg(extractedCount).arg(target));
            }
        }
    }
}

void ArchiveViewer::testArchiveIntegrity()
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        return;
    }

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (!m_currentPassphrase.isEmpty()) {
        archive_read_add_passphrase(a, m_currentPassphrase.toUtf8().constData());
    }

    int r = archive_read_open_filename(a, m_currentArchiveFilePath.toUtf8().constData(), 10240);
    if (r != ARCHIVE_OK) {
        QString err = QString::fromUtf8(archive_error_string(a));
        archive_read_free(a);
        QMessageBox::warning(this, tr("Integrity Test"), tr("Could not open archive: %1").arg(err));
        return;
    }

    int totalEstimate = m_entryCount > 0 ? m_entryCount : 100;
    QProgressDialog progress(tr("Testing archive integrity..."), tr("Cancel"), 0, totalEstimate, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(200);

    struct archive_entry *entry;
    int count = 0;
    bool ok = true;
    QString errorMsg;
    int rHeader = ARCHIVE_OK;

    while ((rHeader = archive_read_next_header(a, &entry)) == ARCHIVE_OK || rHeader == ARCHIVE_WARN) {
        count++;
        progress.setValue(count);
        if (progress.wasCanceled()) {
            break;
        }

        QString entryPath = QString::fromUtf8(archive_entry_pathname_utf8(entry));
        progress.setLabelText(tr("Verifying: %1").arg(QFileInfo(entryPath).fileName()));
        QApplication::processEvents();

        const void *buff;
        size_t size;
        int64_t offset;
        int rBlock = ARCHIVE_OK;
        while ((rBlock = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
            // Read data blocks sequentially to verify checksums and decompression
        }
        if (rBlock != ARCHIVE_EOF && rBlock != ARCHIVE_OK) {
            ok = false;
            errorMsg = QString::fromUtf8(archive_error_string(a));
            break;
        }
    }

    if (rHeader != ARCHIVE_EOF && rHeader != ARCHIVE_OK && rHeader != ARCHIVE_WARN) {
        ok = false;
        if (errorMsg.isEmpty()) {
            errorMsg = QString::fromUtf8(archive_error_string(a));
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    progress.setValue(progress.maximum());

    if (progress.wasCanceled()) return;

    if (ok) {
        QMessageBox::information(this, tr("Integrity Test Passed"),
                                 tr("Archive is healthy! Verified %1 entries without errors.").arg(count));
    } else {
        QMessageBox::warning(this, tr("Integrity Test Failed"),
                             tr("Corruption detected: %1").arg(errorMsg.isEmpty() ? tr("Unknown error") : errorMsg));
    }
}

QStringList ArchiveViewer::previewableEntries() const
{
    QStringList result;
    for (const auto &rec : m_entries) {
        if (!rec.isDir) {
            if (!rec.fullPath.startsWith("__MACOSX/") && !rec.fullPath.contains("/.__MACOSX/")) {
                result.append(rec.fullPath);
            }
        }
    }
    return result;
}

QString ArchiveViewer::extractSingleFile(const QString &archivePath, const QString &internalName)
{
    QString rel = internalName;
    rel.replace('\\', '/');
    while (rel.startsWith('/')) rel.remove(0, 1);
    while (rel.startsWith("./")) rel.remove(0, 2);

    // Normalize and strictly block path traversal attempts
    QString cleanRel = QDir::cleanPath(rel);
    if (cleanRel.isEmpty() || cleanRel == "." || cleanRel.startsWith("../") || cleanRel.contains("/../") || cleanRel == "..") {
        qWarning() << "[ArchiveViewer] Security rejection: Path traversal detected in entry:" << internalName;
        return QString();
    }

    QString tempCacheDir = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/bubezview_archive_preview"));
    QDir().mkpath(tempCacheDir);

    QString outPath = QDir::cleanPath(tempCacheDir + "/" + cleanRel);
    if (!outPath.startsWith(tempCacheDir + "/") && outPath != tempCacheDir) {
        qWarning() << "[ArchiveViewer] Security rejection: Path escapes temporary cache directory:" << internalName;
        return QString();
    }
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (!m_currentPassphrase.isEmpty()) {
        archive_read_add_passphrase(a, m_currentPassphrase.toUtf8().constData());
    }

    int r = archive_read_open_filename(a, archivePath.toUtf8().constData(), 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return QString();
    }

    struct archive_entry *entry;
    bool found = false;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        QString p = QString::fromUtf8(archive_entry_pathname_utf8(entry));
        if (p.isEmpty()) p = QString::fromLocal8Bit(archive_entry_pathname(entry));
        p.replace('\\', '/');
        while (p.startsWith('/')) p.remove(0, 1);
        while (p.startsWith("./")) p.remove(0, 2);

        if (p == cleanRel || p == internalName) {
            found = true;
            QFile outFile(outPath);
            bool writeOk = false;
            if (outFile.open(QIODevice::WriteOnly)) {
                writeOk = true;
                const void *buff;
                size_t size;
                int64_t offset;
                int rBlock = ARCHIVE_OK;
                while ((rBlock = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
                    if (outFile.write(reinterpret_cast<const char*>(buff), static_cast<qint64>(size)) != static_cast<qint64>(size)) {
                        writeOk = false;
                        break;
                    }
                }
                if (rBlock != ARCHIVE_EOF && rBlock != ARCHIVE_OK) {
                    writeOk = false;
                }
                outFile.close();
                if (!writeOk) {
                    outFile.remove();
                }
            }
            if (writeOk) {
                archive_read_close(a);
                archive_read_free(a);
                return outPath;
            }
            break;
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    // Fallback to 7-Zip engine if libarchive could not decrypt or extract the item
    const QString sevenZBin = find7zBinary();
    if (!sevenZBin.isEmpty()) {
        QStringList args;
        args << QStringLiteral("x")
             << archivePath
             << QStringLiteral("-o%1").arg(tempCacheDir)
             << QStringLiteral("-y")
             << QStringLiteral("-r-")
             << internalName;
        if (!m_currentPassphrase.isEmpty()) {
            args << QStringLiteral("-p%1").arg(m_currentPassphrase);
        }
        QProcess proc;
        proc.start(sevenZBin, args);
        if (proc.waitForFinished(10000) && QFile::exists(outPath) && QFileInfo(outPath).size() > 0) {
            return outPath;
        }
    }

    return (found && QFile::exists(outPath) && QFileInfo(outPath).size() > 0) ? outPath : QString();
}

void ArchiveViewer::clear()
{
    if (m_worker) {
        m_worker->disconnect(this);
        m_worker->cancel();
        m_worker = nullptr;
    }
    m_treeWidget->clear();
    if (m_pillText) m_pillText->clear();
    if (m_searchEdit) m_searchEdit->clear();
    m_entries.clear();
    m_entryCount = 0;
    m_totalSize = 0;
    m_currentArchiveFilePath.clear();
    m_currentPassphrase.clear();
    m_formatName.clear();
    m_filterName.clear();
    if (m_addBtn) {
        m_addBtn->setEnabled(true);
        m_addBtn->setToolTip(QString());
    }
}

QString ArchiveViewer::formatSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

bool ArchiveViewer::isArchiveEditable() const
{
    if (m_currentArchiveFilePath.isEmpty()) return false;
    QString lower = m_currentArchiveFilePath.toLower();
    if (lower.endsWith(QStringLiteral(".rar")) ||
        lower.endsWith(QStringLiteral(".iso")) ||
        lower.endsWith(QStringLiteral(".deb")) ||
        lower.endsWith(QStringLiteral(".rpm")) ||
        lower.endsWith(QStringLiteral(".cpio")) ||
        lower.endsWith(QStringLiteral(".squashfs")) ||
        lower.endsWith(QStringLiteral(".img"))) {
        return false;
    }

    // If 7-Zip archive contains encrypted entries, libarchive cannot safely write encrypted 7z
    for (const auto &rec : m_entries) {
        if (rec.isEncrypted && (lower.endsWith(QStringLiteral(".7z")) || m_formatName.contains(QStringLiteral("7-Zip"), Qt::CaseInsensitive))) {
            return false;
        }
    }

    if (lower.endsWith(QStringLiteral(".zip")) ||
        lower.endsWith(QStringLiteral(".7z")) ||
        lower.endsWith(QStringLiteral(".tar")) ||
        lower.endsWith(QStringLiteral(".tar.gz")) ||
        lower.endsWith(QStringLiteral(".tgz")) ||
        lower.endsWith(QStringLiteral(".tar.bz2")) ||
        lower.endsWith(QStringLiteral(".tbz2")) ||
        lower.endsWith(QStringLiteral(".tar.xz")) ||
        lower.endsWith(QStringLiteral(".txz")) ||
        lower.endsWith(QStringLiteral(".tar.zst")) ||
        lower.endsWith(QStringLiteral(".tzst"))) {
        return true;
    }

    return false;
}

void ArchiveViewer::cleanupPreviewCache()
{
    QString tempCacheDir = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/bubezview_archive_preview"));
    QDir dir(tempCacheDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

namespace {
/**
 * @brief Runs multi-threaded 7-Zip / ZIP compression (-mmt=on) across all CPU cores with live progress parsing.
 *        For ZIP archives, uses fast Deflate level 2 for regular files and Smart Store (Method = Store)
 *        for pre-compressed media/archives (video, audio, etc.), keeping compression ultra-fast.
 */
bool runMultiCore7zCompression(const QString &sevenZBin,
                               bool is7z,
                               const QString &tmpArchivePath,
                               const QStringList &sourcePaths,
                               const QString &passphrase,
                               qint64 safeTotalBytes,
                               std::atomic<bool> &cancelled,
                               const std::function<void(qint64, const QString &)> &onByteProgress,
                               QString *outError)
{
    QString currentItemName = QFileInfo(sourcePaths.value(0)).fileName();

    // 1. .7z archives use fast multi-core LZMA2 compression (level 2)
    if (is7z) {
        QStringList args;
        args << QStringLiteral("a")
             << QStringLiteral("-t7z")
             << QStringLiteral("-mx=2")
             << QStringLiteral("-mmt=on")
             << QStringLiteral("-bsp1")
             << QStringLiteral("-bse2")
             << QStringLiteral("-y");

        if (!passphrase.isEmpty()) {
            args << (QStringLiteral("-p") + passphrase);
            args << QStringLiteral("-mhe=on");
        }

        args << QStringLiteral("--") << tmpArchivePath;
        for (const QString &src : sourcePaths) {
            QFileInfo fi(src);
            if (fi.exists()) {
                args << fi.absoluteFilePath();
            }
        }

        bool ok = execute7zProcessPass(sevenZBin, args, QString(), currentItemName,
                                       safeTotalBytes, cancelled, onByteProgress, outError);
        if (ok && onByteProgress) {
            onByteProgress(safeTotalBytes, currentItemName);
        }
        return ok;
    }

    // 2. ZIP archives: Smart Store + Level 2 Deflate
    // Scan items to partition them into Deflate items and Store items
    QList<DiskSourceItem> items = collectDiskItems(sourcePaths);
    if (items.isEmpty()) {
        if (outError) *outError = QObject::tr("No files or directories found to compress.");
        return false;
    }

    // Determine the common base directory so relative paths preserve user folder hierarchy
    QString commonBaseDir;
    if (sourcePaths.size() == 1) {
        QFileInfo fi(sourcePaths.first());
        commonBaseDir = fi.isDir() ? fi.dir().absolutePath() : fi.absolutePath();
    } else {
        QString firstParent = QFileInfo(sourcePaths.first()).absolutePath();
        bool allSameParent = true;
        for (const QString &src : sourcePaths) {
            if (QFileInfo(src).absolutePath() != firstParent) {
                allSameParent = false;
                break;
            }
        }
        commonBaseDir = allSameParent ? firstParent : QString();
    }

    // Identify directories that actually contain files so we do NOT pass non-empty directory paths to 7z.
    // If a non-empty directory is passed to 7z, 7z recursively compresses all files inside it (including media!).
    QSet<QString> parentDirPaths;
    for (const DiskSourceItem &item : items) {
        if (!item.isDir) {
            QString path = item.archiveRelPath;
            int lastSlash = path.lastIndexOf('/');
            while (lastSlash != -1) {
                QString parentPart = path.left(lastSlash + 1); // includes trailing '/'
                parentDirPaths.insert(parentPart);
                lastSlash = path.lastIndexOf('/', lastSlash - 1);
            }
        }
    }

    QStringList deflateItems;
    QStringList storeItems;

    for (const DiskSourceItem &item : items) {
        if (item.isDir) {
            // ONLY keep genuinely empty leaf directories so empty folder structures are preserved,
            // without causing 7z to recursively re-scan and Deflate non-empty media folders!
            QString dirRel = item.archiveRelPath;
            if (!dirRel.endsWith('/')) dirRel += '/';
            if (!parentDirPaths.contains(dirRel)) {
                deflateItems.append(item.archiveRelPath);
            }
        } else {
            if (isAlreadyCompressedMediaOrArchive(item.diskPath)) {
                storeItems.append(item.archiveRelPath);
            } else {
                deflateItems.append(item.archiveRelPath);
            }
        }
    }

    // If commonBaseDir is valid, use response listfiles for exact relative paths
    if (!commonBaseDir.isEmpty()) {
        QTemporaryFile listDeflateFile;
        QTemporaryFile listStoreFile;
        listDeflateFile.setAutoRemove(true);
        listStoreFile.setAutoRemove(true);

        if (!listDeflateFile.open() || !listStoreFile.open()) {
            if (outError) *outError = QObject::tr("Failed to create temporary list file for compression.");
            return false;
        }

        QTextStream outDeflate(&listDeflateFile);
        for (const QString &rel : deflateItems) {
            outDeflate << rel << "\n";
        }
        outDeflate.flush();

        QTextStream outStore(&listStoreFile);
        for (const QString &rel : storeItems) {
            outStore << rel << "\n";
        }
        outStore.flush();

        // Pass 1: Add regular files with Fast Deflate (-mx=2)
        if (!deflateItems.isEmpty()) {
            QStringList args1;
            args1 << QStringLiteral("a")
                  << QStringLiteral("-tzip")
                  << QStringLiteral("-mm=Deflate")
                  << QStringLiteral("-mx=2")
                  << QStringLiteral("-mmt=on")
                  << QStringLiteral("-r-")
                  << QStringLiteral("-bsp1")
                  << QStringLiteral("-bse2")
                  << QStringLiteral("-y");

            if (!passphrase.isEmpty()) {
                args1 << (QStringLiteral("-p") + passphrase);
                args1 << QStringLiteral("-mem=AES256");
            }

            // Note: 7zz syntax for listfile is <archive_name> @<listfile> WITHOUT '--' before '@',
            // because '--' turns off switch parsing and causes '@' to be treated as a literal filename!
            args1 << tmpArchivePath;
            args1 << (QStringLiteral("@") + listDeflateFile.fileName());

            if (!execute7zProcessPass(sevenZBin, args1, commonBaseDir, currentItemName,
                                      safeTotalBytes, cancelled, onByteProgress, outError)) {
                return false;
            }
        }

        // Pass 2: Add or update media files with Store (-mx=0, Method=Copy)
        if (!storeItems.isEmpty()) {
            QStringList args2;
            if (deflateItems.isEmpty()) {
                // Initial archive creation with store items only
                args2 << QStringLiteral("a");
            } else {
                // Update archive, keeping existing deflated entries intact (-ux2y2z2w2)
                args2 << QStringLiteral("u")
                      << QStringLiteral("-ux2y2z2w2");
            }

            args2 << QStringLiteral("-tzip")
                  << QStringLiteral("-mm=Copy")
                  << QStringLiteral("-mx=0")
                  << QStringLiteral("-mmt=on")
                  << QStringLiteral("-r-")
                  << QStringLiteral("-bsp1")
                  << QStringLiteral("-bse2")
                  << QStringLiteral("-y");

            if (!passphrase.isEmpty()) {
                args2 << (QStringLiteral("-p") + passphrase);
                args2 << QStringLiteral("-mem=AES256");
            }

            args2 << tmpArchivePath;
            args2 << (QStringLiteral("@") + listStoreFile.fileName());

            if (!execute7zProcessPass(sevenZBin, args2, commonBaseDir, currentItemName,
                                      safeTotalBytes, cancelled, onByteProgress, outError)) {
                return false;
            }
        }
    } else {
        // Fallback for paths across disparate roots: direct absolute paths with fast Deflate level 2
        QStringList args;
        args << QStringLiteral("a")
             << QStringLiteral("-tzip")
             << QStringLiteral("-mx=2")
             << QStringLiteral("-mmt=on")
             << QStringLiteral("-bsp1")
             << QStringLiteral("-bse2")
             << QStringLiteral("-y");

        if (!passphrase.isEmpty()) {
            args << (QStringLiteral("-p") + passphrase);
            args << QStringLiteral("-mem=AES256");
        }

        args << QStringLiteral("--") << tmpArchivePath;
        for (const QString &src : sourcePaths) {
            QFileInfo fi(src);
            if (fi.exists()) {
                args << fi.absoluteFilePath();
            }
        }

        if (!execute7zProcessPass(sevenZBin, args, QString(), currentItemName,
                                  safeTotalBytes, cancelled, onByteProgress, outError)) {
            return false;
        }
    }

    if (onByteProgress) {
        onByteProgress(safeTotalBytes, currentItemName);
    }
    return true;
}

/**
 * @brief Configures a libarchive write handle according to the requested format string or file extension.
 */
bool configureArchiveWriteFormat(struct archive *out, const QString &formatOrPath, const QString &passphrase)
{
    const int threads = qMax(2, QThread::idealThreadCount());
    QString lower = formatOrPath.toLower();
    if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz") || lower == "tar.gz") {
        archive_write_set_format_pax_restricted(out);
        if (!QStandardPaths::findExecutable(QStringLiteral("pigz")).isEmpty()) {
            // Multi-core parallel gzip compression
            archive_write_add_filter_program(out, "pigz");
        } else {
            archive_write_add_filter_gzip(out);
        }
    } else if (lower.endsWith(".tar.xz") || lower.endsWith(".txz") || lower == "tar.xz") {
        archive_write_set_format_pax_restricted(out);
        archive_write_add_filter_xz(out);
        const QByteArray opt = QStringLiteral("xz:threads=%1").arg(threads).toUtf8();
        archive_write_set_options(out, opt.constData());
    } else if (lower.endsWith(".tar.zst") || lower.endsWith(".tzst") || lower == "tar.zst") {
        archive_write_set_format_pax_restricted(out);
        archive_write_add_filter_zstd(out);
        // Note: In libzstd, threads=0 means single-threaded (0 worker threads). Pass explicit core count!
        const QByteArray opt = QStringLiteral("zstd:threads=%1").arg(threads).toUtf8();
        archive_write_set_options(out, opt.constData());
    } else if (lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2") || lower == "tar.bz2") {
        archive_write_set_format_pax_restricted(out);
        archive_write_add_filter_bzip2(out);
    } else if (lower.endsWith(".tar") || lower == "tar") {
        archive_write_set_format_pax_restricted(out);
        archive_write_add_filter_none(out);
    } else if (lower.endsWith(".7z") || lower == "7z") {
        archive_write_set_format_7zip(out);
        archive_write_set_options(out, "7zip:compression=lzma2");
    } else {
        // Default: universal ZIP format with fast multi-core Deflate level 2
        archive_write_set_format_zip(out);
        archive_write_set_options(out, "zip:compression=deflate");
        archive_write_set_options(out, "zip:compression-level=2");
        if (!passphrase.isEmpty()) {
            archive_write_set_options(out, "zip:encryption=aes256");
            archive_write_set_passphrase(out, passphrase.toUtf8().constData());
        }
    }
    return true;
}

/**
 * @brief Writes a single DiskSourceItem into an open libarchive write stream with chunk-level progress callbacks.
 */
bool writeDiskItemToArchive(struct archive *out,
                            const DiskSourceItem &item,
                            std::atomic<bool> &cancelled,
                            qint64 &cumulativeBytesWritten,
                            const std::function<void(qint64, const QString &)> &onByteProgress = nullptr)
{
    if (cancelled.load(std::memory_order_relaxed)) return false;

    QFileInfo fi(item.diskPath);
    QString baseName = fi.fileName();
    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname_utf8(entry, item.archiveRelPath.toUtf8().constData());
    archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);

    if (item.isDir) {
        archive_entry_set_filetype(entry, AE_IFDIR);
        archive_entry_set_perm(entry, 0755);
        archive_entry_set_size(entry, 0);
        int r = archive_write_header(out, entry);
        archive_entry_free(entry);
        return (r >= ARCHIVE_WARN);
    }

    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, fi.isExecutable() ? 0755 : 0644);
    archive_entry_set_size(entry, fi.size());

    int r = archive_write_header(out, entry);
    if (r < ARCHIVE_WARN) {
        archive_entry_free(entry);
        return false;
    }

    if (onByteProgress) {
        onByteProgress(cumulativeBytesWritten, baseName);
    }

    QFile inFile(item.diskPath);
    bool ok = true;
    if (inFile.open(QIODevice::ReadOnly)) {
        QByteArray buf(1024 * 1024, Qt::Uninitialized); // 1 MB chunk buffer for multi-core throughput
        qint64 bytesRead = 0;
        while ((bytesRead = inFile.read(buf.data(), buf.size())) > 0) {
            if (cancelled.load(std::memory_order_relaxed)) {
                ok = false;
                break;
            }
            if (archive_write_data(out, buf.constData(), static_cast<size_t>(bytesRead)) < 0) {
                ok = false;
                break;
            }
            cumulativeBytesWritten += bytesRead;
            if (onByteProgress) {
                onByteProgress(cumulativeBytesWritten, baseName);
            }
        }
        inFile.close();
        if (ok && onByteProgress) {
            onByteProgress(cumulativeBytesWritten, baseName);
        }
    } else {
        ok = false;
    }

    archive_entry_free(entry);
    return ok;
}
} // namespace

void ArchiveViewer::addFilesToArchive()
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        QMessageBox::information(this, tr("Add Files"), tr("No archive is currently open."));
        return;
    }

    if (!isArchiveEditable()) {
        QMessageBox::information(this, tr("Read-Only Archive"),
            tr("Modifying '%1' archives in-place is not supported to protect data integrity.")
                .arg(m_formatName.isEmpty() ? QFileInfo(m_currentArchiveFilePath).fileName() : m_formatName));
        return;
    }

    QString startDir = QFileInfo(m_currentArchiveFilePath).absolutePath();
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Files to Add to Archive"),
        startDir,
        tr("All Files (*)")
    );
    if (files.isEmpty()) return;

    modifyCurrentArchive(files, {});
}

void ArchiveViewer::addFolderToArchive()
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        QMessageBox::information(this, tr("Add Folder"), tr("No archive is currently open."));
        return;
    }

    if (!isArchiveEditable()) {
        QMessageBox::information(this, tr("Read-Only Archive"),
            tr("Modifying '%1' archives in-place is not supported to protect data integrity.")
                .arg(m_formatName.isEmpty() ? QFileInfo(m_currentArchiveFilePath).fileName() : m_formatName));
        return;
    }

    QString startDir = QFileInfo(m_currentArchiveFilePath).absolutePath();
    QString folder = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder to Add to Archive"),
        startDir
    );
    if (folder.isEmpty()) return;

    modifyCurrentArchive({folder}, {});
}

void ArchiveViewer::removeSelectedFromArchive()
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        return;
    }

    if (!isArchiveEditable()) {
        QMessageBox::information(this, tr("Read-Only Archive"),
            tr("Modifying '%1' archives in-place is not supported to protect data integrity.")
                .arg(m_formatName.isEmpty() ? QFileInfo(m_currentArchiveFilePath).fileName() : m_formatName));
        return;
    }

    QList<QTreeWidgetItem*> selected = m_treeWidget->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Remove from Archive"),
                                 tr("Please select one or more items in the archive tree to remove."));
        return;
    }

    QStringList pathsToRemove;
    for (QTreeWidgetItem *item : selected) {
        QString p = item->data(0, Qt::UserRole).toString();
        while (p.startsWith('/')) p.remove(0, 1);
        if (p.endsWith('/')) p.chop(1);
        if (!p.isEmpty()) {
            pathsToRemove.append(p);
        }
    }
    if (pathsToRemove.isEmpty()) return;

    auto reply = QMessageBox::question(
        this,
        tr("Remove Selected Items"),
        tr("Remove %1 selected item(s) from archive '%2'?").arg(pathsToRemove.size()).arg(QFileInfo(m_currentArchiveFilePath).fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;

    modifyCurrentArchive({}, pathsToRemove);
}

bool ArchiveViewer::modifyCurrentArchive(const QStringList &pathsToAdd, const QStringList &entriesToRemove)
{
    if (m_currentArchiveFilePath.isEmpty() || !QFile::exists(m_currentArchiveFilePath)) {
        return false;
    }

    if (!isArchiveEditable()) {
        QMessageBox::information(this, tr("Read-Only Archive"),
            tr("Modifying '%1' archives in-place is not supported to protect data integrity.")
                .arg(m_formatName.isEmpty() ? QFileInfo(m_currentArchiveFilePath).fileName() : m_formatName));
        return false;
    }

    // Determine target subfolder inside archive if user has a single directory selected when adding
    QString targetSubfolder;
    if (!pathsToAdd.isEmpty() && entriesToRemove.isEmpty()) {
        QList<QTreeWidgetItem*> sel = m_treeWidget->selectedItems();
        if (sel.size() == 1 && sel.first()->data(0, Qt::UserRole + 1).toBool()) {
            targetSubfolder = sel.first()->data(0, Qt::UserRole).toString();
        }
    }

    qint64 newBytesTotal = 0;
    QList<DiskSourceItem> newItems = collectDiskItems(pathsToAdd, targetSubfolder, &newBytesTotal);
    QSet<QString> newRelPaths;
    for (const auto &it : newItems) {
        QString clean = it.archiveRelPath;
        if (clean.endsWith('/')) clean.chop(1);
        newRelPaths.insert(clean);
    }

    QString archivePath = m_currentArchiveFilePath;
    QString passphrase = m_currentPassphrase;
    QString tmpArchivePath = archivePath + ".bubez_rebuild_tmp";
    QFile::remove(tmpArchivePath);

    qint64 totalBytesEstimate = qMax<qint64>(1, m_totalSize + newBytesTotal);

    QDialog progDlg(this);
    progDlg.setWindowTitle(tr("Updating Archive"));
    progDlg.setModal(true);
    progDlg.setMinimumWidth(440);
    QVBoxLayout *progLay = new QVBoxLayout(&progDlg);
    progLay->setContentsMargins(16, 16, 16, 16);
    progLay->setSpacing(10);

    QLabel *lblStatus = new QLabel(tr("⏳ Preparing archive update..."), &progDlg);
    QProgressBar *bar = new QProgressBar(&progDlg);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(true);
    bar->setFormat("%p%");
    bar->setStyleSheet(
        "QProgressBar { background-color: rgba(100, 116, 139, 0.2); border: 1px solid rgba(100, 116, 139, 0.35); border-radius: 6px; height: 20px; text-align: center; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #0284c7; border-radius: 5px; }"
    );
    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &progDlg);
    btnLay->addWidget(btnCancel);

    progLay->addWidget(lblStatus);
    progLay->addWidget(bar);
    progLay->addLayout(btnLay);

    std::atomic<bool> cancelled{false};
    bool workerOk = true;
    QString workerError;
    QElapsedTimer timer;
    timer.start();

    connect(btnCancel, &QPushButton::clicked, &progDlg, [&]() {
        cancelled.store(true, std::memory_order_relaxed);
        btnCancel->setEnabled(false);
        lblStatus->setText(tr("Cancelling..."));
    });
    connect(&progDlg, &QDialog::rejected, &progDlg, [&]() {
        cancelled.store(true, std::memory_order_relaxed);
    });

    auto reportProgress = [&](qint64 doneBytes, const QString &currentName) {
        qint64 elapsedMs = qMax<qint64>(1, timer.elapsed());
        double mbPerSec = (doneBytes / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);
        int permille = static_cast<int>(std::clamp<qint64>((doneBytes * 1000) / totalBytesEstimate, 0, 995));
        QMetaObject::invokeMethod(&progDlg, [bar, lblStatus, permille, doneBytes, totalBytesEstimate, mbPerSec, currentName]() {
            bar->setValue(permille);
            lblStatus->setText(
                QStringLiteral("⚡ %1 — %2 / %3 (%4 MB/s)")
                    .arg(currentName, formatByteSpeed(doneBytes), formatByteSpeed(totalBytesEstimate))
                    .arg(mbPerSec, 0, 'f', 1)
            );
        }, Qt::QueuedConnection);
    };

    QEventLoop loop;
    QThread *worker = QThread::create([&]() {
        struct archive *in = archive_read_new();
        archive_read_support_filter_all(in);
        archive_read_support_format_all(in);
        if (!passphrase.isEmpty()) {
            archive_read_add_passphrase(in, passphrase.toUtf8().constData());
        }

        if (archive_read_open_filename(in, archivePath.toUtf8().constData(), 65536) != ARCHIVE_OK) {
            workerError = QString::fromUtf8(archive_error_string(in));
            archive_read_free(in);
            workerOk = false;
            return;
        }

        struct archive *out = archive_write_new();
        configureArchiveWriteFormat(out, archivePath, passphrase);

        if (archive_write_open_filename(out, tmpArchivePath.toUtf8().constData()) != ARCHIVE_OK) {
            workerError = QString::fromUtf8(archive_error_string(out));
            archive_read_close(in);
            archive_read_free(in);
            archive_write_free(out);
            workerOk = false;
            return;
        }

        qint64 cumulativeBytes = 0;
        struct archive_entry *entry = nullptr;
        int rHeader = ARCHIVE_OK;

        // 1. Copy kept entries from existing archive
        while ((rHeader = archive_read_next_header(in, &entry)) == ARCHIVE_OK || rHeader == ARCHIVE_WARN) {
            if (cancelled.load(std::memory_order_relaxed)) {
                workerOk = false;
                break;
            }

            QString entryPath = QString::fromUtf8(archive_entry_pathname_utf8(entry));
            if (entryPath.isEmpty()) {
                entryPath = QString::fromLocal8Bit(archive_entry_pathname(entry));
            }
            entryPath.replace('\\', '/');
            while (entryPath.startsWith('/')) entryPath.remove(0, 1);
            while (entryPath.startsWith("./")) entryPath.remove(0, 2);
            QString cleanPath = entryPath;
            if (cleanPath.endsWith('/')) cleanPath.chop(1);

            bool skip = newRelPaths.contains(cleanPath);
            if (!skip) {
                for (const QString &rem : entriesToRemove) {
                    if (cleanPath == rem || cleanPath.startsWith(rem + "/")) {
                        skip = true;
                        break;
                    }
                }
            }

            if (skip) {
                archive_read_data_skip(in);
                continue;
            }

            if (archive_write_header(out, entry) < ARCHIVE_WARN) {
                workerError = QString::fromUtf8(archive_error_string(out));
                workerOk = false;
                break;
            }

            const void *buff = nullptr;
            size_t size = 0;
            int64_t offset = 0;
            int chunkCount = 0;
            QString fn = QFileInfo(cleanPath).fileName();
            int rBlock = ARCHIVE_OK;
            while ((rBlock = archive_read_data_block(in, &buff, &size, &offset)) == ARCHIVE_OK) {
                if (cancelled.load(std::memory_order_relaxed)) {
                    workerOk = false;
                    break;
                }
                if (archive_write_data(out, buff, size) < 0) {
                    workerError = QString::fromUtf8(archive_error_string(out));
                    workerOk = false;
                    break;
                }
                cumulativeBytes += static_cast<qint64>(size);
                chunkCount++;
                if (chunkCount % 4 == 0) {
                    reportProgress(cumulativeBytes, fn);
                }
            }
            if (rBlock != ARCHIVE_EOF && rBlock != ARCHIVE_OK) {
                workerError = QString::fromUtf8(archive_error_string(in));
                workerOk = false;
                break;
            }
            if (!workerOk) break;
        }

        if (workerOk && !cancelled.load(std::memory_order_relaxed) && rHeader != ARCHIVE_EOF && rHeader != ARCHIVE_OK && rHeader != ARCHIVE_WARN) {
            workerError = QString::fromUtf8(archive_error_string(in));
            workerOk = false;
        }

        // 2. Append newly added items from disk
        if (workerOk) {
            for (const DiskSourceItem &item : newItems) {
                if (cancelled.load(std::memory_order_relaxed)) {
                    workerOk = false;
                    break;
                }
                if (!writeDiskItemToArchive(out, item, cancelled, cumulativeBytes, reportProgress)) {
                    workerError = QString::fromUtf8(archive_error_string(out));
                    workerOk = false;
                    break;
                }
            }
        }

        archive_read_close(in);
        archive_read_free(in);
        archive_write_close(out);
        archive_write_free(out);
    });

    connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
    progDlg.show();
    worker->start();
    loop.exec();
    worker->wait();
    delete worker;
    progDlg.hide();

    if (!workerOk) {
        QFile::remove(tmpArchivePath);
        if (!cancelled.load(std::memory_order_relaxed)) {
            QMessageBox::warning(this, tr("Archive Modification Failed"),
                                 workerError.isEmpty() ? tr("An error occurred while updating the archive.") : workerError);
        }
        return false;
    }

    // Atomically replace original archive with rebuilt archive
    QString backupPath = archivePath + ".bubez_bak";
    QFile::remove(backupPath);
    if (!QFile::rename(archivePath, backupPath)) {
        QFile::remove(tmpArchivePath);
        QMessageBox::warning(this, tr("Archive Modification Failed"),
                             tr("Unable to overwrite archive file (check write permissions)."));
        return false;
    }
    if (!QFile::rename(tmpArchivePath, archivePath)) {
        QFile::rename(backupPath, archivePath);
        QFile::remove(tmpArchivePath);
        QMessageBox::warning(this, tr("Archive Modification Failed"),
                             tr("Failed to finalize updated archive."));
        return false;
    }
    QFile::remove(backupPath);

    // Reload updated archive
    loadFile(archivePath, passphrase);
    return true;
}

bool ArchiveViewer::createArchive(const QStringList &sourcePaths,
                                  const QString &destArchivePath,
                                  const QString &format,
                                  const QString &passphrase,
                                  QWidget *parentWidget)
{
    QWidget *uiParent = parentWidget ? parentWidget : this;
    if (sourcePaths.isEmpty() || destArchivePath.isEmpty()) {
        return false;
    }

    qint64 totalBytes = 0;
    QList<DiskSourceItem> items = collectDiskItems(sourcePaths, QString(), &totalBytes);
    if (items.isEmpty()) {
        QMessageBox::warning(uiParent, tr("Create Archive"),
                             tr("No valid files or folders found to compress."));
        return false;
    }

    QDir().mkpath(QFileInfo(destArchivePath).absolutePath());
    QString tmpPath = destArchivePath + ".bubez_creating";
    QFile::remove(tmpPath);

    qint64 safeTotalBytes = qMax<qint64>(1, totalBytes);

    QDialog progDlg(uiParent);
    progDlg.setWindowTitle(tr("📦 Compressing Archive..."));
    progDlg.setModal(true);
    progDlg.setMinimumWidth(460);
    QVBoxLayout *progLay = new QVBoxLayout(&progDlg);
    progLay->setContentsMargins(16, 16, 16, 16);
    progLay->setSpacing(10);

    QLabel *lblTitle = new QLabel(
        QStringLiteral("<b>%1</b>").arg(QFileInfo(destArchivePath).fileName()),
        &progDlg
    );
    QLabel *lblStatus = new QLabel(tr("⏳ Starting compression..."), &progDlg);
    lblStatus->setStyleSheet("color: #94a3b8; font-size: 12px;");

    QProgressBar *bar = new QProgressBar(&progDlg);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(true);
    bar->setFormat("%p%");
    bar->setStyleSheet(
        "QProgressBar { background-color: rgba(100, 116, 139, 0.2); border: 1px solid rgba(100, 116, 139, 0.35); border-radius: 6px; height: 22px; text-align: center; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #0284c7; border-radius: 5px; }"
    );

    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &progDlg);
    btnLay->addWidget(btnCancel);

    progLay->addWidget(lblTitle);
    progLay->addWidget(bar);
    progLay->addWidget(lblStatus);
    progLay->addLayout(btnLay);

    std::atomic<bool> cancelled{false};
    bool workerOk = true;
    QString workerError;
    QElapsedTimer timer;
    timer.start();

    connect(btnCancel, &QPushButton::clicked, &progDlg, [&]() {
        cancelled.store(true, std::memory_order_relaxed);
        btnCancel->setEnabled(false);
        lblStatus->setText(tr("Cancelling..."));
    });
    connect(&progDlg, &QDialog::rejected, &progDlg, [&]() {
        cancelled.store(true, std::memory_order_relaxed);
    });

    auto reportProgress = [&](qint64 doneBytes, const QString &currentName) {
        qint64 elapsedMs = qMax<qint64>(1, timer.elapsed());
        double mbPerSec = (doneBytes / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);
        int permille = static_cast<int>(std::clamp<qint64>((doneBytes * 1000) / safeTotalBytes, 0, 995));
        QMetaObject::invokeMethod(&progDlg, [bar, lblStatus, permille, doneBytes, safeTotalBytes, mbPerSec, currentName]() {
            bar->setValue(permille);
            lblStatus->setText(
                QStringLiteral("⚡ %1 — %2 / %3 (%4 MB/s)")
                    .arg(currentName, formatByteSpeed(doneBytes), formatByteSpeed(safeTotalBytes))
                    .arg(mbPerSec, 0, 'f', 1)
            );
        }, Qt::QueuedConnection);
    };

    const QString sevenZBin = find7zBinary();
    bool is7zTarget = false;
    const bool useMultiCore7z = (!sevenZBin.isEmpty() && is7zOrZipFormat(format.isEmpty() ? destArchivePath : format, &is7zTarget));

    QEventLoop loop;
    QThread *worker = QThread::create([&]() {
        if (useMultiCore7z) {
            workerOk = runMultiCore7zCompression(
                sevenZBin, is7zTarget, tmpPath, sourcePaths, passphrase,
                safeTotalBytes, cancelled, reportProgress, &workerError
            );
            return;
        }

        struct archive *out = archive_write_new();
        configureArchiveWriteFormat(out, format.isEmpty() ? destArchivePath : format, passphrase);

        if (archive_write_open_filename(out, tmpPath.toUtf8().constData()) != ARCHIVE_OK) {
            workerError = QString::fromUtf8(archive_error_string(out));
            archive_write_free(out);
            workerOk = false;
            return;
        }

        qint64 cumulativeBytes = 0;
        for (const DiskSourceItem &item : items) {
            if (cancelled.load(std::memory_order_relaxed)) {
                workerOk = false;
                break;
            }
            if (!writeDiskItemToArchive(out, item, cancelled, cumulativeBytes, reportProgress)) {
                workerOk = false;
                break;
            }
        }

        archive_write_close(out);
        archive_write_free(out);
    });

    connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
    progDlg.show();
    worker->start();
    loop.exec();
    worker->wait();
    delete worker;
    progDlg.hide();

    if (!workerOk) {
        QFile::remove(tmpPath);
        if (!cancelled.load(std::memory_order_relaxed) && !workerError.isEmpty()) {
            QMessageBox::warning(uiParent, tr("Create Archive Failed"),
                                 tr("Could not create output archive:\n%1").arg(workerError));
        }
        return false;
    }

    if (QFile::exists(destArchivePath)) {
        QFile::remove(destArchivePath);
    }
    if (!QFile::rename(tmpPath, destArchivePath)) {
        QFile::remove(tmpPath);
        QMessageBox::warning(uiParent, tr("Create Archive Failed"),
                             tr("Could not write final archive to:\n%1").arg(destArchivePath));
        return false;
    }

    return true;
}

QString ArchiveViewer::openCreateArchiveDialog(const QString &currentFilePath,
                                               const QString &currentFolderPath,
                                               QWidget *parentWidget,
                                               bool *outSendWithLocalSend,
                                               bool *outOpenWhenDone)
{
    if (outSendWithLocalSend) *outSendWithLocalSend = false;
    if (outOpenWhenDone) *outOpenWhenDone = false;

    QWidget *uiParent = parentWidget ? parentWidget : this;
    QDialog dlg(uiParent);
    dlg.setWindowTitle(tr("📦 Compress / Create Archive"));
    dlg.setMinimumWidth(560);
    dlg.setMinimumHeight(480);

    QVBoxLayout *mainLay = new QVBoxLayout(&dlg);
    mainLay->setContentsMargins(16, 16, 16, 16);
    mainLay->setSpacing(12);

    // 1. Quick Preset Selector (Current File vs Entire Current Folder)
    QString resolvedFolder = currentFolderPath;
    if (resolvedFolder.isEmpty() && !currentFilePath.isEmpty()) {
        QFileInfo fi(currentFilePath);
        resolvedFolder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    }

    bool hasSingleFile = (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).isFile());
    bool hasCurrentFolder = (!resolvedFolder.isEmpty() && QFileInfo(resolvedFolder).isDir());

    QListWidget *listWidget = new QListWidget(&dlg);
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listWidget->setAlternatingRowColors(true);

    QComboBox *cmbFormat = new QComboBox(&dlg);
    cmbFormat->addItem(tr("ZIP (.zip) — Universal & Compatible (Multi-Core)"), QStringLiteral("zip"));
    cmbFormat->addItem(tr("TAR.GZ (.tar.gz) — Standard Linux Gzip (Multi-Core)"), QStringLiteral("tar.gz"));
    cmbFormat->addItem(tr("TAR.XZ (.tar.xz) — High Compression XZ (Multi-Core)"), QStringLiteral("tar.xz"));
    cmbFormat->addItem(tr("TAR.ZST (.tar.zst) — Ultra-Fast Zstandard (Multi-Core)"), QStringLiteral("tar.zst"));
    cmbFormat->addItem(tr("7-Zip (.7z) — Maximum Compression (Multi-Core)"), QStringLiteral("7z"));

    QLineEdit *editDest = new QLineEdit(&dlg);

    auto populateListWithPaths = [&](const QStringList &paths) {
        listWidget->clear();
        for (const QString &p : paths) {
            QFileInfo fi(p);
            if (!fi.exists()) continue;
            QListWidgetItem *it = new QListWidgetItem(
                QIcon::fromTheme(fi.isDir() ? "folder" : "text-x-generic"),
                fi.isDir() ? QStringLiteral("📁 %1/  (%2)").arg(fi.fileName(), fi.absoluteFilePath())
                           : QStringLiteral("📄 %1  (%2)").arg(fi.fileName(), fi.absoluteFilePath())
            );
            it->setData(Qt::UserRole, fi.absoluteFilePath());
            listWidget->addItem(it);
        }
    };

    auto updateSuggestedDestPath = [&]() {
        QString ext = cmbFormat->currentData().toString();
        if (ext.isEmpty()) ext = QStringLiteral("zip");

        QString baseDir = !resolvedFolder.isEmpty() ? resolvedFolder : QDir::homePath();
        QString baseName = QStringLiteral("Archive");

        if (listWidget->count() == 1) {
            QFileInfo fi(listWidget->item(0)->data(Qt::UserRole).toString());
            baseDir = fi.isDir() ? fi.absolutePath() : fi.absolutePath();
            baseName = fi.completeBaseName().isEmpty() ? fi.fileName() : fi.completeBaseName();
            if (fi.isDir()) baseName = fi.fileName();
        } else if (listWidget->count() > 1 && !resolvedFolder.isEmpty()) {
            baseName = QFileInfo(resolvedFolder).fileName();
            if (baseName.isEmpty()) baseName = QStringLiteral("Archive");
        }

        editDest->setText(QDir(baseDir).filePath(baseName + "." + ext));
    };

    if (hasSingleFile || hasCurrentFolder) {
        QGroupBox *grpQuick = new QGroupBox(tr("Quick Source Selection"), &dlg);
        QHBoxLayout *quickLay = new QHBoxLayout(grpQuick);
        quickLay->setSpacing(8);

        if (hasSingleFile) {
            QPushButton *btnUseFile = new QPushButton(
                QStringLiteral("📄 ") + tr("Current File (%1)").arg(QFileInfo(currentFilePath).fileName()),
                grpQuick
            );
            btnUseFile->setCursor(Qt::PointingHandCursor);
            connect(btnUseFile, &QPushButton::clicked, &dlg, [&]() {
                populateListWithPaths({currentFilePath});
                updateSuggestedDestPath();
            });
            quickLay->addWidget(btnUseFile);
        }

        if (hasCurrentFolder) {
            QString folderLabel = QFileInfo(resolvedFolder).fileName();
            if (folderLabel.isEmpty()) folderLabel = resolvedFolder;
            QPushButton *btnUseFolder = new QPushButton(
                QStringLiteral("📁 ") + tr("Entire Current Folder (%1/)").arg(folderLabel),
                grpQuick
            );
            btnUseFolder->setCursor(Qt::PointingHandCursor);
            connect(btnUseFolder, &QPushButton::clicked, &dlg, [&]() {
                populateListWithPaths({resolvedFolder});
                updateSuggestedDestPath();
            });
            quickLay->addWidget(btnUseFolder);
        }

        mainLay->addWidget(grpQuick);
    }

    // 2. Files & Folders List Box
    QGroupBox *grpItems = new QGroupBox(tr("Files & Folders to Compress"), &dlg);
    QVBoxLayout *itemsLay = new QVBoxLayout(grpItems);
    itemsLay->addWidget(listWidget);

    QHBoxLayout *itemBtnsLay = new QHBoxLayout();
    QPushButton *btnAddFiles = new QPushButton(QIcon::fromTheme("list-add"), tr("+ Add Files..."), grpItems);
    QPushButton *btnAddFolder = new QPushButton(QIcon::fromTheme("folder-new"), tr("+ Add Folder..."), grpItems);
    QPushButton *btnRemoveItem = new QPushButton(QIcon::fromTheme("edit-delete"), tr("Remove Selected"), grpItems);
    itemBtnsLay->addWidget(btnAddFiles);
    itemBtnsLay->addWidget(btnAddFolder);
    itemBtnsLay->addWidget(btnRemoveItem);
    itemBtnsLay->addStretch(1);
    itemsLay->addLayout(itemBtnsLay);
    mainLay->addWidget(grpItems, 1);

    connect(btnAddFiles, &QPushButton::clicked, &dlg, [&]() {
        QString startDir = !resolvedFolder.isEmpty() ? resolvedFolder : QDir::homePath();
        QStringList files = QFileDialog::getOpenFileNames(&dlg, tr("Add Files to Compress"), startDir);
        for (const QString &f : files) {
            QFileInfo fi(f);
            QListWidgetItem *it = new QListWidgetItem(
                QIcon::fromTheme("text-x-generic"),
                QStringLiteral("📄 %1  (%2)").arg(fi.fileName(), fi.absoluteFilePath())
            );
            it->setData(Qt::UserRole, fi.absoluteFilePath());
            listWidget->addItem(it);
        }
        if (editDest->text().isEmpty() || listWidget->count() == files.size()) {
            updateSuggestedDestPath();
        }
    });

    connect(btnAddFolder, &QPushButton::clicked, &dlg, [&]() {
        QString startDir = !resolvedFolder.isEmpty() ? resolvedFolder : QDir::homePath();
        QString dir = QFileDialog::getExistingDirectory(&dlg, tr("Add Folder to Compress"), startDir);
        if (!dir.isEmpty()) {
            QFileInfo fi(dir);
            QListWidgetItem *it = new QListWidgetItem(
                QIcon::fromTheme("folder"),
                QStringLiteral("📁 %1/  (%2)").arg(fi.fileName(), fi.absoluteFilePath())
            );
            it->setData(Qt::UserRole, fi.absoluteFilePath());
            listWidget->addItem(it);
            if (editDest->text().isEmpty() || listWidget->count() == 1) {
                updateSuggestedDestPath();
            }
        }
    });

    connect(btnRemoveItem, &QPushButton::clicked, &dlg, [&]() {
        qDeleteAll(listWidget->selectedItems());
        updateSuggestedDestPath();
    });

    // 3. Archive Format, Destination, and Optional Password
    QGroupBox *grpOut = new QGroupBox(tr("Output Archive Settings"), &dlg);
    QFormLayout *formLay = new QFormLayout(grpOut);
    formLay->setSpacing(8);

    QHBoxLayout *destLay = new QHBoxLayout();
    QPushButton *btnBrowseDest = new QPushButton(tr("Browse..."), grpOut);
    destLay->addWidget(editDest, 1);
    destLay->addWidget(btnBrowseDest);

    QLineEdit *editPass = new QLineEdit(grpOut);
    editPass->setEchoMode(QLineEdit::Password);
    editPass->setPlaceholderText(tr("Optional AES-256 password (supported on .zip and .7z)"));

    QCheckBox *chkOpenAfter = new QCheckBox(tr("Open created archive in BubezView when done"), grpOut);
    chkOpenAfter->setChecked(true);

    formLay->addRow(tr("Format:"), cmbFormat);
    formLay->addRow(tr("Destination:"), destLay);
    formLay->addRow(tr("Password:"), editPass);
    formLay->addRow(QString(), chkOpenAfter);
    mainLay->addWidget(grpOut);

    // 4. Built-in BubezView Progress Bar & Live Speed Status
    QProgressBar *progressBar = new QProgressBar(&dlg);
    progressBar->setRange(0, 1000);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFormat("%p%");
    progressBar->setStyleSheet(
        "QProgressBar { background-color: rgba(100, 116, 139, 0.2); border: 1px solid rgba(100, 116, 139, 0.35); border-radius: 6px; height: 22px; text-align: center; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #0284c7; border-radius: 5px; }"
    );
    progressBar->setVisible(false);
    mainLay->addWidget(progressBar);

    QLabel *lblProgressStatus = new QLabel(&dlg);
    lblProgressStatus->setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: 500;");
    lblProgressStatus->setVisible(false);
    mainLay->addWidget(lblProgressStatus);

    connect(cmbFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) {
        QString fmt = cmbFormat->currentData().toString();
        editPass->setEnabled(fmt == QStringLiteral("zip") || fmt == QStringLiteral("7z"));
        QString cur = editDest->text().trimmed();
        if (!cur.isEmpty()) {
            static const QStringList knownExts = {".tar.gz", ".tar.xz", ".tar.zst", ".tar.bz2", ".zip", ".7z", ".tar"};
            for (const QString &ke : knownExts) {
                if (cur.endsWith(ke, Qt::CaseInsensitive)) {
                    cur.chop(ke.length());
                    break;
                }
            }
            editDest->setText(cur + "." + fmt);
        } else {
            updateSuggestedDestPath();
        }
    });

    connect(btnBrowseDest, &QPushButton::clicked, &dlg, [&]() {
        QString fmt = cmbFormat->currentData().toString();
        QString out = QFileDialog::getSaveFileName(
            &dlg,
            tr("Save Compressed Archive As"),
            editDest->text(),
            tr("Archives (*.%1);;All Files (*)").arg(fmt)
        );
        if (!out.isEmpty()) {
            editDest->setText(out);
        }
    });

    // Populate default item based on active context
    if (hasSingleFile) {
        populateListWithPaths({currentFilePath});
    } else if (hasCurrentFolder) {
        populateListWithPaths({resolvedFolder});
    }
    updateSuggestedDestPath();

    // 5. Bottom Action Bar
    QHBoxLayout *bottomLay = new QHBoxLayout();
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &dlg);
    QPushButton *btnLocalSend = new QPushButton(tr("🚀 Compress & Send (LocalSend)"), &dlg);
    QPushButton *btnCreate = new QPushButton(tr("📦 Create Archive"), &dlg);
    btnCreate->setDefault(true);

    btnLocalSend->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: #ffffff; font-weight: 600; padding: 6px 14px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    btnCreate->setStyleSheet(
        "QPushButton { background-color: #16a34a; color: #ffffff; font-weight: 600; padding: 6px 16px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #15803d; }"
    );

    bottomLay->addWidget(btnCancel);
    bottomLay->addStretch(1);
    bottomLay->addWidget(btnLocalSend);
    bottomLay->addWidget(btnCreate);
    mainLay->addLayout(bottomLay);

    bool sendLocalSendClicked = false;
    bool isCompressing = false;
    std::atomic<bool> cancelRequested{false};
    QString finalCreatedPath;

    auto startInDialogCompression = [&](bool forLocalSend) {
        if (isCompressing) return;

        QStringList sources;
        for (int i = 0; i < listWidget->count(); ++i) {
            QString p = listWidget->item(i)->data(Qt::UserRole).toString();
            if (!p.isEmpty()) sources.append(p);
        }

        QString destPath = editDest->text().trimmed();
        QString fmt = cmbFormat->currentData().toString();
        QString pass = editPass->isEnabled() ? editPass->text() : QString();

        if (sources.isEmpty() || destPath.isEmpty()) {
            QMessageBox::warning(&dlg, tr("Create Archive"),
                                 tr("Please specify at least one source item and a destination path."));
            return;
        }

        qint64 totalBytes = 0;
        QList<DiskSourceItem> items = collectDiskItems(sources, QString(), &totalBytes);
        if (items.isEmpty()) {
            QMessageBox::warning(&dlg, tr("Create Archive"),
                                 tr("No valid files or folders found to compress."));
            return;
        }

        QDir().mkpath(QFileInfo(destPath).absolutePath());
        QString tmpPath = destPath + ".bubez_creating";
        QFile::remove(tmpPath);

        isCompressing = true;
        cancelRequested.store(false, std::memory_order_relaxed);
        sendLocalSendClicked = forLocalSend;

        btnCreate->setEnabled(false);
        btnLocalSend->setEnabled(false);
        grpItems->setEnabled(false);
        grpOut->setEnabled(false);

        progressBar->setValue(0);
        progressBar->setVisible(true);
        lblProgressStatus->setText(tr("⏳ Compressing..."));
        lblProgressStatus->setVisible(true);

        qint64 safeTotalBytes = qMax<qint64>(1, totalBytes);
        QElapsedTimer timer;
        timer.start();

        auto reportProgress = [&](qint64 doneBytes, const QString &currentName) {
            qint64 elapsedMs = qMax<qint64>(1, timer.elapsed());
            double mbPerSec = (doneBytes / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);
            int permille = static_cast<int>(std::clamp<qint64>((doneBytes * 1000) / safeTotalBytes, 0, 995));
            QMetaObject::invokeMethod(&dlg, [progressBar, lblProgressStatus, permille, doneBytes, safeTotalBytes, mbPerSec, currentName]() {
                progressBar->setValue(permille);
                lblProgressStatus->setText(
                    QStringLiteral("⚡ %1 — %2 / %3  •  %4 MB/s")
                        .arg(currentName, formatByteSpeed(doneBytes), formatByteSpeed(safeTotalBytes))
                        .arg(mbPerSec, 0, 'f', 1)
                );
            }, Qt::QueuedConnection);
        };

        const QString sevenZBin = find7zBinary();
        bool is7zTarget = false;
        const bool useMultiCore7z = (!sevenZBin.isEmpty() && is7zOrZipFormat(fmt.isEmpty() ? destPath : fmt, &is7zTarget));

        bool workerOk = true;
        QString workerError;
        QEventLoop loop;
        QThread *worker = QThread::create([&]() {
            if (useMultiCore7z) {
                workerOk = runMultiCore7zCompression(
                    sevenZBin, is7zTarget, tmpPath, sources, pass,
                    safeTotalBytes, cancelRequested, reportProgress, &workerError
                );
                return;
            }

            struct archive *out = archive_write_new();
            configureArchiveWriteFormat(out, fmt.isEmpty() ? destPath : fmt, pass);

            if (archive_write_open_filename(out, tmpPath.toUtf8().constData()) != ARCHIVE_OK) {
                workerError = QString::fromUtf8(archive_error_string(out));
                archive_write_free(out);
                workerOk = false;
                return;
            }

            qint64 cumulativeBytes = 0;
            for (const DiskSourceItem &item : items) {
                if (cancelRequested.load(std::memory_order_relaxed)) {
                    workerOk = false;
                    break;
                }
                if (!writeDiskItemToArchive(out, item, cancelRequested, cumulativeBytes, reportProgress)) {
                    workerOk = false;
                    break;
                }
            }

            archive_write_close(out);
            archive_write_free(out);
        });

        connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        worker->start();
        loop.exec();
        worker->wait();
        delete worker;

        isCompressing = false;

        if (!workerOk) {
            QFile::remove(tmpPath);
            btnCreate->setEnabled(true);
            btnLocalSend->setEnabled(true);
            grpItems->setEnabled(true);
            grpOut->setEnabled(true);
            progressBar->setVisible(false);
            lblProgressStatus->setVisible(false);

            if (!cancelRequested.load(std::memory_order_relaxed) && !workerError.isEmpty()) {
                QMessageBox::warning(&dlg, tr("Create Archive Failed"),
                                     tr("Could not create output archive:\n%1").arg(workerError));
            }
            return;
        }

        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }
        if (!QFile::rename(tmpPath, destPath)) {
            QFile::remove(tmpPath);
            btnCreate->setEnabled(true);
            btnLocalSend->setEnabled(true);
            grpItems->setEnabled(true);
            grpOut->setEnabled(true);
            QMessageBox::warning(&dlg, tr("Create Archive Failed"),
                                 tr("Could not write final archive to:\n%1").arg(destPath));
            return;
        }

        progressBar->setValue(1000);
        finalCreatedPath = destPath;
        dlg.accept();
    };

    connect(btnCancel, &QPushButton::clicked, &dlg, [&]() {
        if (isCompressing) {
            cancelRequested.store(true, std::memory_order_relaxed);
            lblProgressStatus->setText(tr("Cancelling compression..."));
        } else {
            dlg.reject();
        }
    });
    connect(&dlg, &QDialog::rejected, &dlg, [&]() {
        if (isCompressing) {
            cancelRequested.store(true, std::memory_order_relaxed);
        }
    });
    connect(btnCreate, &QPushButton::clicked, &dlg, [&]() {
        startInDialogCompression(false);
    });
    connect(btnLocalSend, &QPushButton::clicked, &dlg, [&]() {
        startInDialogCompression(true);
    });

    if (dlg.exec() != QDialog::Accepted || finalCreatedPath.isEmpty()) {
        return QString();
    }

    if (outSendWithLocalSend) *outSendWithLocalSend = sendLocalSendClicked;
    if (outOpenWhenDone) *outOpenWhenDone = chkOpenAfter->isChecked();
    return finalCreatedPath;
}


