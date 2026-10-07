#include "archive_viewer.h"
#include "theme_manager.h"
#include <archive.h>
#include <archive_entry.h>
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
#include <functional>
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
    wait(200);
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
                if (!dt.isValid() || dt > st.latestMtime) {
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

    // Sort entries in background thread so directories come first, then files
    std::sort(m_entries.begin(), m_entries.end(), [](const ArchiveItemRecord &a, const ArchiveItemRecord &b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.fullPath.compare(b.fullPath, Qt::CaseInsensitive) < 0;
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
        m_worker->cancel();
        m_worker->wait(200);
    }
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

    // Cancel any previous worker
    if (m_worker) {
        m_worker->cancel();
        m_worker->wait(100);
        m_worker->deleteLater();
        m_worker = nullptr;
    }

    m_worker = new ArchiveWorker(m_currentArchiveFilePath, m_currentPassphrase, this);
    connect(m_worker, &ArchiveWorker::parsed, this, &ArchiveViewer::onArchiveParsed);
    connect(m_worker, &ArchiveWorker::error, this, &ArchiveViewer::onArchiveError);
    connect(m_worker, &ArchiveWorker::passphraseRequired, this, &ArchiveViewer::onPassphraseRequired);
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
    if (filePath != m_currentArchiveFilePath) {
        return; // Stale worker result from previous file
    }

    QList<ArchiveItemRecord> entries = m_worker ? m_worker->takeEntries() : QList<ArchiveItemRecord>();
    QMap<QString, FolderStats> dirStats = m_worker ? m_worker->takeDirStats() : QMap<QString, FolderStats>();
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
                QTreeWidgetItem *fileItem = nullptr;
                if (parentItem) {
                    fileItem = new QTreeWidgetItem(parentItem);
                } else {
                    fileItem = new QTreeWidgetItem();
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

                // Store internal path and isDir flag
                fileItem->setData(0, Qt::UserRole, rec.fullPath);
                fileItem->setData(0, Qt::UserRole + 1, false);
                fileItem->setData(0, Qt::UserRole + 2, rec.isEncrypted);
            } else {
                // Directory node
                if (!dirNodes.contains(curPath)) {
                    QTreeWidgetItem *dirItem = nullptr;
                    if (parentItem) {
                        dirItem = new QTreeWidgetItem(parentItem);
                    } else {
                        dirItem = new QTreeWidgetItem();
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

                    dirItem->setData(0, Qt::UserRole, curPath);
                    dirItem->setData(0, Qt::UserRole + 1, true);

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
        m_pillText->setText(QString(tr("%1 Items (%2 Total)")).arg(m_entryCount).arg(formatSize(m_totalSize)));
    } else {
        m_pillText->setText(QString("%1 • %2 Items (%3 Total)").arg(formatDisplay).arg(m_entryCount).arg(formatSize(m_totalSize)));
    }

    m_pillBadge->setToolTip(
        QString(tr("Archive: %1\nFormat: %2\nCompression: %3\nItems: %4\nTotal Size: %5"))
            .arg(QFileInfo(m_currentArchiveFilePath).fileName())
            .arg(m_formatName.isEmpty() ? "-" : m_formatName)
            .arg(m_filterName.isEmpty() ? "-" : m_filterName)
            .arg(m_entryCount)
            .arg(formatSize(m_totalSize))
    );

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
    if (!item) return;

    QMenu menu(this);
    bool isDir = item->data(0, Qt::UserRole + 1).toBool();
    QString internalPath = item->data(0, Qt::UserRole).toString();

    if (!isDir) {
        QAction *previewAct = menu.addAction(tr("Preview"));
        connect(previewAct, &QAction::triggered, this, [this, item]() {
            onItemDoubleClicked(item, 0);
        });
    }

    QAction *extractAct = menu.addAction(tr("Extract Selected..."));
    connect(extractAct, &QAction::triggered, this, [this]() {
        extractSelected();
    });

    menu.addSeparator();
    QAction *copyPathAct = menu.addAction(tr("Copy Path"));
    connect(copyPathAct, &QAction::triggered, this, [internalPath]() {
        QApplication::clipboard()->setText(internalPath);
    });

    menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));
}

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

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
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

        // Filter if selective extraction
        if (!filterPaths.isEmpty()) {
            bool matches = false;
            for (const QString &fp : filterPaths) {
                if (entryPath == fp || entryPath.startsWith(fp + "/")) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                archive_read_data_skip(a);
                continue;
            }
        }

        QString fullDest = QDir(target).filePath(entryPath);
        archive_entry_set_pathname(entry, fullDest.toUtf8().constData());
        progress.setLabelText(tr("Extracting: %1").arg(QFileInfo(fullDest).fileName()));
        QApplication::processEvents();

        r = archive_write_header(ext, entry);
        if (r >= ARCHIVE_OK) {
            const void *buff;
            size_t size;
            int64_t offset;
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                archive_write_data_block(ext, buff, size, offset);
            }
            archive_write_finish_entry(ext);
            extractedCount++;
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    progress.setValue(progress.maximum());

    if (!progress.wasCanceled()) {
        if (destDir.isEmpty()) {
            QMessageBox::information(this, tr("Extraction Complete"),
                                     tr("Successfully extracted %1 items to:\n%2").arg(extractedCount).arg(target));
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

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
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
        while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
            // Read data blocks sequentially to verify checksums and decompression
        }
        if (r != ARCHIVE_EOF && r != ARCHIVE_OK) {
            ok = false;
            errorMsg = QString::fromUtf8(archive_error_string(a));
            break;
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
    QString tempCacheDir = QDir::tempPath() + "/bubezview_archive_preview";
    QDir().mkpath(tempCacheDir);

    QString rel = internalName;
    while (rel.startsWith('/')) rel.remove(0, 1);
    QString outPath = tempCacheDir + "/" + rel;
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

        if (p == internalName) {
            found = true;
            QFile outFile(outPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                const void *buff;
                size_t size;
                int64_t offset;
                while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                    outFile.write(reinterpret_cast<const char*>(buff), static_cast<qint64>(size));
                }
                outFile.close();
            }
            break;
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    return found ? outPath : QString();
}

void ArchiveViewer::clear()
{
    if (m_worker) {
        m_worker->cancel();
        m_worker->wait(100);
        m_worker->deleteLater();
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
}

QString ArchiveViewer::formatSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
