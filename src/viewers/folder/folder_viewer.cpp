#include "folder_viewer.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include "plugin_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QScrollBar>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QPlainTextEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QDateTime>
#include <QLocale>
#include <QCollator>
#include <algorithm>
#include <QStandardPaths>
#include <QDebug>
#include <QSettings>
#include <QFileDialog>
#include <QMenu>

// ==========================================
// ThumbnailWorker
// ==========================================
// Runs in a dedicated worker thread to read images, PDF first-pages, and text micro-previews asynchronously,
// preventing disk I/O and Poppler rendering from blocking the GUI thread or causing frame drops.
ThumbnailWorker::ThumbnailWorker(const QStringList &filePaths, QObject *parent)
    : QThread(parent)
    , m_filePaths(filePaths)
    , m_isCancelled(false)
{
}

ThumbnailWorker::~ThumbnailWorker()
{
    cancelAndWait();
}

void ThumbnailWorker::cancelAndWait()
{
    // Atomic cancel flag allows the worker loop to exit immediately between files
    m_isCancelled.store(true, std::memory_order_relaxed);
    if (isRunning()) {
        wait();
    }
}

QImage ThumbnailWorker::renderTextThumbnail(const QString &filePath)
{
    // Read first ~45 lines of text from file for Dolphin-style micro-text preview
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QImage();
    }

    QStringList lines;
    QTextStream in(&file);
    int lineCount = 0;
    while (!in.atEnd() && lineCount < 45) {
        QString line = in.readLine(160);
        lines.append(line);
        lineCount++;
    }
    file.close();

    // Render 200x150 Hi-DPI document sheet with folded corner and real micro-text
    const int canvasW = 200;
    const int canvasH = 150;
    QImage img(canvasW * 2, canvasH * 2, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(2.0);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Sheet dimensions: centered in 200x150 canvas with A4 proportions
    const qreal sheetW = 96.0;
    const qreal sheetH = 132.0;
    const qreal sheetX = (canvasW - sheetW) / 2.0; // 52.0
    const qreal sheetY = (canvasH - sheetH) / 2.0; // 9.0
    const qreal fold = 16.0;

    // Subtle drop shadow under paper sheet
    QColor shadowColor(0, 0, 0, 30);
    p.setPen(Qt::NoPen);
    p.setBrush(shadowColor);
    p.drawRoundedRect(QRectF(sheetX + 2, sheetY + 3, sheetW, sheetH), 2, 2);

    // Main sheet polygon with top-right folded corner notch
    QPainterPath sheetPath;
    sheetPath.moveTo(sheetX, sheetY);
    sheetPath.lineTo(sheetX + sheetW - fold, sheetY);
    sheetPath.lineTo(sheetX + sheetW, sheetY + fold);
    sheetPath.lineTo(sheetX + sheetW, sheetY + sheetH);
    sheetPath.lineTo(sheetX, sheetY + sheetH);
    sheetPath.closeSubpath();

    // Clean white paper sheet with crisp subtle outline
    p.setPen(QPen(QColor("#94a3b8"), 0.9));
    p.setBrush(QColor("#ffffff"));
    p.drawPath(sheetPath);

    // Folded dog-ear corner (triangle)
    QPainterPath foldPath;
    foldPath.moveTo(sheetX + sheetW - fold, sheetY);
    foldPath.lineTo(sheetX + sheetW - fold, sheetY + fold);
    foldPath.lineTo(sheetX + sheetW, sheetY + fold);
    foldPath.closeSubpath();

    p.setPen(QPen(QColor("#94a3b8"), 0.8));
    p.setBrush(QColor("#e2e8f0")); // Shaded folded corner flap
    p.drawPath(foldPath);

    // Render micro-text lines on sheet
    QFont font("Monospace", 4);
    font.setStyleHint(QFont::Monospace);
    p.setFont(font);
    p.setPen(QColor("#334155")); // Readable dark slate micro-text

    QRectF textArea(sheetX + 7, sheetY + 7, sheetW - 14, sheetH - 14);
    p.setClipRect(textArea);

    qreal curY = sheetY + 11;
    const qreal lineSpacing = 5.2;

    for (const QString &line : lines) {
        if (curY > sheetY + sheetH - 8) break;
        if (line.trimmed().isEmpty()) {
            curY += lineSpacing * 0.7;
            continue;
        }

        QString text = line;
        text.replace("\t", "  ");
        p.drawText(QPointF(sheetX + 7, curY), text);
        curY += lineSpacing;
    }

    p.end();
    return img;
}

void ThumbnailWorker::run()
{
    // Common image extensions supported for fast preview thumbnailing
    static const QStringList imgExts = {
        "png", "jpg", "jpeg", "webp", "svg", "bmp", "gif", "ico", "tiff", "tif", "avif"
    };

    static const QStringList textExts = {
        "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
        "sh", "bash", "zsh", "fish", "js", "ts", "html", "htm", "css", "scss", "sass",
        "json", "xml", "yaml", "yml", "ini", "conf", "cfg", "csv", "tsv", "log",
        "sql", "cmake", "desktop", "service", "diff", "patch", "rs", "go", "java", "lua", "php", "rb"
    };

    for (const QString &path : m_filePaths) {
        if (m_isCancelled.load(std::memory_order_relaxed)) break;

        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        if (imgExts.contains(ext)) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                QSize targetSize(220, 150);
                QSize scaled = sz.scaled(targetSize, Qt::KeepAspectRatio);
                reader.setScaledSize(scaled);
                QImage img = reader.read();
                if (!img.isNull()) {
                    emit thumbnailReady(path, img);
                }
            }
        } else if (ext == "pdf") {
            QImage img = PluginManager::renderPdfThumbnail(path, 220);
            if (!img.isNull()) {
                emit thumbnailReady(path, img.scaled(220, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        } else if (textExts.contains(ext)) {
            QImage img = renderTextThumbnail(path);
            if (!img.isNull()) {
                emit thumbnailReady(path, img);
            }
        }
    }
}

// ==========================================
// FolderCardDelegate
// ==========================================
FolderCardDelegate::FolderCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize FolderCardDelegate::sizeHint(const QStyleOptionViewItem & /*option*/, const QModelIndex & /*index*/) const
{
    return QSize(150, 160);
}

void FolderCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    bool isHover = bool(option.state & QStyle::State_MouseOver);
    bool isSelected = bool(option.state & QStyle::State_Selected);

    ThemePreset preset = ThemeManager::currentPreset();

    QColor bgColor = Qt::transparent;
    QColor borderColor = Qt::transparent;
    QColor textColor;

    if (preset == ThemePreset::SystemNative) {
        QPalette pal = option.palette;
        if (isSelected) {
            bgColor = pal.highlight().color();
            bgColor.setAlpha(45);
            borderColor = pal.highlight().color();
            textColor = pal.text().color();
        } else if (isHover) {
            bgColor = pal.highlight().color();
            bgColor.setAlpha(20);
            borderColor = pal.highlight().color();
            borderColor.setAlpha(70);
            textColor = pal.text().color();
        } else {
            textColor = pal.text().color();
        }
    } else if (preset == ThemePreset::DarkSlate) {
        if (isSelected) {
            bgColor = QColor(56, 189, 248, 45);
            borderColor = QColor("#38bdf8");
            textColor = QColor("#ffffff");
        } else if (isHover) {
            bgColor = QColor(255, 255, 255, 18);
            borderColor = QColor(255, 255, 255, 45);
            textColor = QColor("#f1f5f9");
        } else {
            textColor = QColor("#cbd5e1");
        }
    } else { // BreezeDark
        if (isSelected) {
            bgColor = QColor(61, 174, 233, 45);
            borderColor = QColor("#3daee9");
            textColor = QColor("#ffffff");
        } else if (isHover) {
            bgColor = QColor(255, 255, 255, 18);
            borderColor = QColor(255, 255, 255, 45);
            textColor = QColor("#eff0f1");
        } else {
            textColor = QColor("#cfd2d5");
        }
    }

    // 1. Draw Selection or Hover Highlight (clean modern file manager style - no border for unselected)
    if (isSelected || isHover) {
        painter->setPen(QPen(borderColor, isSelected ? 1.5 : 1.0));
        painter->setBrush(QBrush(bgColor));
        painter->drawRoundedRect(QRectF(rect), 6, 6);
    }

    // 2. Draw Thumbnail or Icon
    QRect thumbArea(rect.left() + 4, rect.top() + 6, rect.width() - 8, 100);
    QVariant thumbVar = index.data(Qt::UserRole + 2); // Custom thumbnail pixmap

    if (thumbVar.isValid() && !thumbVar.value<QPixmap>().isNull()) {
        QPixmap pix = thumbVar.value<QPixmap>();
        int pxW = qMin(pix.width(), thumbArea.width());
        int pxH = qMin(pix.height(), thumbArea.height());
        QPixmap scaled = pix.scaled(pxW, pxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        int x = thumbArea.left() + (thumbArea.width() - scaled.width()) / 2;
        int y = thumbArea.top() + (thumbArea.height() - scaled.height()) / 2;
        painter->drawPixmap(x, y, scaled);
    } else {
        QVariant iconVar = index.data(Qt::DecorationRole);
        if (iconVar.isValid() && iconVar.canConvert<QIcon>()) {
            QIcon icon = iconVar.value<QIcon>();
            int iconSize = 76;
            int x = thumbArea.left() + (thumbArea.width() - iconSize) / 2;
            int y = thumbArea.top() + (thumbArea.height() - iconSize) / 2;
            icon.paint(painter, x, y, iconSize, iconSize);
        }
    }

    // 3. Draw Label Text
    QRect textArea(rect.left() + 4, rect.top() + 110, rect.width() - 8, 38);
    QFont font = option.font;
    font.setPointSize(9);
    font.setWeight(isSelected ? QFont::DemiBold : QFont::Medium);
    painter->setFont(font);
    painter->setPen(textColor);

    QString text = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(font);
    QString elided = fm.elidedText(text, Qt::ElideMiddle, textArea.width());
    painter->drawText(textArea, Qt::AlignHCenter | Qt::AlignTop, elided);

    painter->restore();
}

// ==========================================
// FolderViewer
// ==========================================
FolderViewer::FolderViewer(QWidget *parent)
    : QWidget(parent)
    , m_topBar(nullptr)
    , m_btnUp(nullptr)
    , m_pillBadge(nullptr)
    , m_pillIcon(nullptr)
    , m_pillText(nullptr)
    , m_btnToggleInspector(nullptr)
    , m_bookmarksWidget(nullptr)
    , m_listWidget(nullptr)
    , m_previewPane(nullptr)
    , m_inspectorBadge(nullptr)
    , m_previewImgLabel(nullptr)
    , m_previewTextEdit(nullptr)
    , m_previewTitleLabel(nullptr)
    , m_previewMetaLabel(nullptr)
    , m_worker(nullptr)
    , m_itemCount(0)
    , m_totalSize(0)
{
    setupUi();
}

FolderViewer::~FolderViewer()
{
    clear();
}

void FolderViewer::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top Navigation & Stats Bar
    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(46);

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(14, 6, 14, 6);
    topLayout->setSpacing(12);

    m_btnUp = new QPushButton(tr("⬆ Up"), m_topBar);
    m_btnUp->setCursor(Qt::PointingHandCursor);
    m_btnUp->setToolTip(tr("Go to parent folder (Backspace)"));
    m_btnUp->setFixedHeight(28);
    m_btnUp->setMinimumWidth(75);
    connect(m_btnUp, &QPushButton::clicked, this, &FolderViewer::goUpDirectory);
    topLayout->addWidget(m_btnUp);

    topLayout->addStretch(1);

    m_pillBadge = new QFrame(m_topBar);
    m_pillBadge->setObjectName("FolderPillBadge");
    m_pillBadge->setFixedHeight(26);

    QHBoxLayout *pillLayout = new QHBoxLayout(m_pillBadge);
    pillLayout->setContentsMargins(12, 0, 14, 0);
    pillLayout->setSpacing(6);
    pillLayout->setAlignment(Qt::AlignVCenter);

    m_pillIcon = new QLabel(m_pillBadge);
    m_pillIcon->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Folder).pixmap(14, 14));
    m_pillIcon->setStyleSheet("border: none; background: transparent;");
    pillLayout->addWidget(m_pillIcon);

    m_pillText = new QLabel(m_pillBadge);
    pillLayout->addWidget(m_pillText);

    topLayout->addWidget(m_pillBadge);
    topLayout->addStretch(1);

    m_btnToggleInspector = new QPushButton(QIcon::fromTheme("dialog-information"), tr("Inspector"), m_topBar);
    m_btnToggleInspector->setCheckable(true);
    m_btnToggleInspector->setChecked(true);
    m_btnToggleInspector->setCursor(Qt::PointingHandCursor);
    m_btnToggleInspector->setToolTip(tr("Show or hide inspector panel (key I)"));
    m_btnToggleInspector->setFixedHeight(28);
    connect(m_btnToggleInspector, &QPushButton::clicked, this, &FolderViewer::toggleInspector);
    topLayout->addWidget(m_btnToggleInspector);

    mainLayout->addWidget(m_topBar);

    // 3-Column Content Layout
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Left Column: Bookmarks Sidebar
    m_bookmarksWidget = new QListWidget(this);
    m_bookmarksWidget->setFixedWidth(170);
    m_bookmarksWidget->setFrameShape(QFrame::NoFrame);
    m_bookmarksWidget->setFocusPolicy(Qt::NoFocus);
    connect(m_bookmarksWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        bool isAddAction = item->data(Qt::UserRole + 2).toBool();
        if (isAddAction) {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder to Bookmark"), m_currentFolder);
            if (!dir.isEmpty()) {
                addCustomBookmark(dir);
            }
            return;
        }
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            loadFolder(path);
        }
    });

    m_bookmarksWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bookmarksWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_bookmarksWidget->itemAt(pos);
        QMenu menu(this);
        ThemePreset preset = ThemeManager::currentPreset();
        if (preset != ThemePreset::SystemNative) {
            menu.setStyleSheet(
                "QMenu {"
                "   background-color: #21252b;"
                "   color: #e2e8f0;"
                "   border: 1px solid #3e4451;"
                "   border-radius: 6px;"
                "   padding: 4px 0px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px 6px 10px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #38bdf8;"
                "   color: #ffffff;"
                "}"
            );
        }

        if (item && item->data(Qt::UserRole + 1).toBool()) {
            QString path = item->data(Qt::UserRole).toString();
            menu.addAction(QIcon::fromTheme("edit-delete"), tr("Remove from Bookmarks"), this, [this, path]() {
                removeCustomBookmark(path);
            });
            menu.addSeparator();
        }

        menu.addAction(QIcon::fromTheme("list-add"), tr("Add Folder to Bookmarks..."), this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder to Bookmark"), m_currentFolder);
            if (!dir.isEmpty()) {
                addCustomBookmark(dir);
            }
        });

        menu.exec(m_bookmarksWidget->mapToGlobal(pos));
    });

    contentLayout->addWidget(m_bookmarksWidget);

    // Center Column: Card Grid List Widget with Large Cards
    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setMovement(QListWidget::Static);
    m_listWidget->setSpacing(8);
    m_listWidget->setGridSize(QSize(150, 160));
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setItemDelegate(new FolderCardDelegate(m_listWidget));

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &FolderViewer::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &FolderViewer::onItemSelectionChanged);

    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_listWidget->itemAt(pos);
        if (item) {
            m_listWidget->setCurrentItem(item);
        }
        emit itemContextMenuRequested(m_listWidget->mapToGlobal(pos), selectedFilePath());
    });

    contentLayout->addWidget(m_listWidget, 1);

    // Right Column: Dolphin-Style Inspector & Preview
    m_previewPane = new QWidget(this);
    m_previewPane->setFixedWidth(280);
    QVBoxLayout *prevLayout = new QVBoxLayout(m_previewPane);
    prevLayout->setContentsMargins(14, 14, 14, 14);
    prevLayout->setSpacing(10);
    prevLayout->setAlignment(Qt::AlignTop);

    m_inspectorBadge = new QLabel(tr("📋 INSPECTOR"), m_previewPane);
    m_inspectorBadge->setStyleSheet("font-size: 11px; font-weight: bold; letter-spacing: 0.5px;");
    prevLayout->addWidget(m_inspectorBadge);

    // Graphic preview widget (shown for images, archives, PDFs, folders)
    m_previewImgLabel = new QLabel(m_previewPane);
    m_previewImgLabel->setFixedHeight(130);
    m_previewImgLabel->setAlignment(Qt::AlignCenter);
    prevLayout->addWidget(m_previewImgLabel);

    // Text preview widget (shown for text/code files)
    m_previewTextEdit = new QPlainTextEdit(m_previewPane);
    m_previewTextEdit->setReadOnly(true);
    m_previewTextEdit->setFixedHeight(150);
    m_previewTextEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);
    m_previewTextEdit->setFont(monoFont);
    m_previewTextEdit->setVisible(false);
    prevLayout->addWidget(m_previewTextEdit);

    m_previewTitleLabel = new QLabel(tr("Select an item"), m_previewPane);
    m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_previewTitleLabel->setWordWrap(true);
    m_previewTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    prevLayout->addWidget(m_previewTitleLabel);

    m_previewMetaLabel = new QLabel(m_previewPane);
    m_previewMetaLabel->setStyleSheet("font-size: 11px; line-height: 1.5;");
    m_previewMetaLabel->setWordWrap(true);
    m_previewMetaLabel->setTextFormat(Qt::RichText);
    m_previewMetaLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    prevLayout->addWidget(m_previewMetaLabel);

    prevLayout->addStretch(1);

    contentLayout->addWidget(m_previewPane);

    mainLayout->addLayout(contentLayout, 1);

    populateBookmarks();
    updateTheme();
}

QString FolderViewer::selectedFilePath() const
{
    if (m_listWidget && m_listWidget->currentItem()) {
        return m_listWidget->currentItem()->data(Qt::UserRole).toString();
    }
    return QString();
}

bool FolderViewer::loadFolder(const QString &folderPath)
{
    clear();

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    m_currentFolder = dir.canonicalPath();

    // Check if parent directory exists
    QDir parentDir(m_currentFolder);
    bool hasParent = parentDir.cdUp();
    m_btnUp->setEnabled(hasParent);

    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::NoSort
    );

    QFileInfoList subdirs;
    QFileInfoList files;
    QStringList asyncThumbnailCandidates;

    for (const QFileInfo &info : entries) {
        QString name = info.fileName();
        if (name.startsWith('.')) continue; // Rule from bubez py: skip hidden

        if (info.isDir()) {
            subdirs.append(info);
        } else {
            files.append(info);
            m_totalSize += info.size();

            static const QStringList textExts = {
                "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
                "sh", "bash", "zsh", "fish", "js", "ts", "html", "htm", "css", "scss", "sass",
                "json", "xml", "yaml", "yml", "ini", "conf", "cfg", "csv", "tsv", "log",
                "sql", "cmake", "desktop", "service", "diff", "patch", "rs", "go", "java", "lua", "php", "rb"
            };
            static const QStringList nameMatches = {"makefile", "cmakelists.txt", "readme", "license", "authors", "changelog", "dockerfile"};

            QString ext = info.suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" ||
                ext == "svg" || ext == "bmp" || ext == "gif" || ext == "ico" || ext == "pdf" ||
                textExts.contains(ext) || nameMatches.contains(name.toLower())) {
                asyncThumbnailCandidates.append(info.absoluteFilePath());
            }
        }
    }

    // Natural numeric sorting identical to Dolphin / KDE Plasma
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    auto compareFileInfos = [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    };

    std::sort(subdirs.begin(), subdirs.end(), compareFileInfos);
    std::sort(files.begin(), files.end(), compareFileInfos);

    m_itemCount = subdirs.size() + files.size();

    // 1. Add Subfolders
    for (const QFileInfo &dInfo : subdirs) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(dInfo.fileName());
        item->setIcon(getFolderIcon(dInfo.absoluteFilePath()));
        item->setData(Qt::UserRole, dInfo.absoluteFilePath());
        item->setData(Qt::UserRole + 1, true); // isDir
        item->setToolTip(QString("📁 %1\n%2").arg(dInfo.fileName(), dInfo.absoluteFilePath()));
        m_listWidget->addItem(item);
    }

    // 2. Add Files
    for (const QFileInfo &fInfo : files) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(fInfo.fileName());
        item->setIcon(getFileIcon(fInfo.absoluteFilePath()));
        item->setData(Qt::UserRole, fInfo.absoluteFilePath());
        item->setData(Qt::UserRole + 1, false); // isDir
        item->setToolTip(QString("📄 %1 (%2)\n%3").arg(fInfo.fileName(), formatSize(fInfo.size()), fInfo.absoluteFilePath()));
        m_listWidget->addItem(item);
    }

    // Update Pill Badge
    QFileInfo currentDirInfo(m_currentFolder);
    QString folderTitle = currentDirInfo.fileName();
    if (folderTitle.isEmpty()) folderTitle = m_currentFolder;

    m_pillText->setText(QString(tr("%1 • %2 Items (%3)"))
        .arg(folderTitle)
        .arg(m_itemCount)
        .arg(formatSize(m_totalSize)));

    // Launch async thumbnail worker
    if (!asyncThumbnailCandidates.isEmpty()) {
        ThumbnailWorker *worker = new ThumbnailWorker(asyncThumbnailCandidates);
        m_worker = worker;
        connect(worker, &ThumbnailWorker::thumbnailReady, this, &FolderViewer::onThumbnailReady);
        connect(worker, &ThumbnailWorker::finished, worker, &QObject::deleteLater);
        worker->start();
    }

    emit folderLoaded(m_currentFolder, m_itemCount, m_totalSize);

    // Auto-select first item so preview pane is immediately populated
    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }

    return true;
}

void FolderViewer::onThumbnailReady(const QString &filePath, const QImage &image)
{
    QPixmap pixmap = QPixmap::fromImage(image);
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == filePath) {
            item->setData(Qt::UserRole + 2, pixmap);
            m_listWidget->update(m_listWidget->visualItemRect(item));
            
            // If this is the currently selected item, update the side preview immediately
            if (m_listWidget->currentItem() == item && m_previewImgLabel) {
                m_previewImgLabel->setPixmap(pixmap.scaled(m_previewImgLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
    }
}

void FolderViewer::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;

    QString targetPath = item->data(Qt::UserRole).toString();
    bool isDir = item->data(Qt::UserRole + 1).toBool();

    if (isDir) {
        loadFolder(targetPath);
    } else {
        emit filePreviewRequested(targetPath);
    }
}

void FolderViewer::goUpDirectory()
{
    if (m_currentFolder.isEmpty()) return;

    QDir dir(m_currentFolder);
    if (dir.cdUp()) {
        loadFolder(dir.canonicalPath());
    }
}

void FolderViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Backspace) {
        goUpDirectory();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QListWidgetItem *cur = m_listWidget->currentItem();
        if (cur) {
            onItemDoubleClicked(cur);
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_I) {
        toggleInspector();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FolderViewer::clear()
{
    if (m_worker) {
        m_worker->cancelAndWait();
        delete m_worker.data();
        m_worker = nullptr;
    }
    m_listWidget->clear();
    m_itemCount = 0;
    m_totalSize = 0;
}

QString FolderViewer::formatSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QIcon FolderViewer::getFolderIcon(const QString &path) const
{
    QString name = QFileInfo(path).fileName().toLower();
    if (name == "downloads" || name == "scaricati" || name == "download") {
        return QIcon::fromTheme("folder-download", QIcon::fromTheme("folder-downloads", VectorIcons::createIcon(VectorIcons::Type::Folder)));
    } else if (name == "pictures" || name == "immagini" || name == "photos" || name == "foto") {
        return QIcon::fromTheme("folder-pictures", QIcon::fromTheme("folder-images", VectorIcons::createIcon(VectorIcons::Type::Folder)));
    } else if (name == "documents" || name == "documenti" || name == "docs") {
        return QIcon::fromTheme("folder-documents", VectorIcons::createIcon(VectorIcons::Type::Folder));
    } else if (name == "music" || name == "musica" || name == "audio") {
        return QIcon::fromTheme("folder-music", QIcon::fromTheme("folder-sound", VectorIcons::createIcon(VectorIcons::Type::Folder)));
    } else if (name == "videos" || name == "video" || name == "movies" || name == "film") {
        return QIcon::fromTheme("folder-videos", QIcon::fromTheme("folder-video", VectorIcons::createIcon(VectorIcons::Type::Folder)));
    } else if (name == "desktop" || name == "scrivania") {
        return QIcon::fromTheme("user-desktop", VectorIcons::createIcon(VectorIcons::Type::Folder));
    }
    return QIcon::fromTheme("folder", VectorIcons::createIcon(VectorIcons::Type::Folder));
}

QIcon FolderViewer::getFileIcon(const QString &path) const
{
    VectorIcons::Type t = VectorIcons::detectType(path, false);
    return VectorIcons::createIcon(t);
}

void FolderViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::SystemNative) {
        m_topBar->setStyleSheet("background-color: palette(window); border-bottom: 1px solid palette(mid);");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: palette(button);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: palette(window-text); font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: palette(button);"
            "   color: palette(button-text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 5px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: palette(highlight);"
            "   color: palette(highlighted-text);"
            "   border-color: palette(highlight);"
            "}"
        );

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: palette(window);"
            "   border: none;"
            "   padding: 12px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: palette(window);"
                "   border-right: 1px solid palette(mid);"
                "   padding: 8px 4px;"
                "   color: palette(window-text);"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 8px 10px;"
                "   border-radius: 6px;"
                "   margin: 2px 4px;"
                "}"
                "QListWidget::item:hover {"
                "   background-color: palette(alternate-base);"
                "}"
                "QListWidget::item:selected {"
                "   background-color: palette(highlight);"
                "   color: palette(highlighted-text);"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: palette(window);"
                "   border-left: 1px solid palette(mid);"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet(
                "background-color: transparent;"
                "border: none;"
                "padding: 2px;"
            );
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: palette(highlight); font-size: 11px; font-weight: bold; letter-spacing: 0.5px;");
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: palette(window-text); font-size: 11px; line-height: 1.5;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: palette(base);"
                "   color: palette(text);"
                "   border: 1px solid palette(mid);"
                "   border-radius: 6px;"
                "   padding: 6px;"
                "}"
            );
        }
        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet("padding: 4px 8px; font-size: 12px;");
        }
    } else if (preset == ThemePreset::DarkSlate) {
        m_topBar->setStyleSheet("background-color: #22262b; border-bottom: 1px solid #2a2f35;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: #3b3e44;"
            "   border: 1px solid #4a4f56;"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #e2e6eb; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: #333842;"
            "   color: #e2e8f0;"
            "   border: 1px solid #424955;"
            "   border-radius: 5px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #38bdf8;"
            "   color: #0f172a;"
            "   border-color: #38bdf8;"
            "}"
        );

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: #333842;"
                "   color: #e2e8f0;"
                "   border: 1px solid #424955;"
                "   border-radius: 5px;"
                "   font-size: 12px;"
                "   padding: 4px 10px;"
                "}"
                "QPushButton:checked {"
                "   background-color: #0284c7;"
                "   color: #ffffff;"
                "   border-color: #38bdf8;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #1e2126;"
            "   border: none;"
            "   padding: 12px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #181b20;"
                "   border-right: 1px solid #282d36;"
                "   padding: 8px 4px;"
                "   color: #94a3b8;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 8px 10px;"
                "   border-radius: 6px;"
                "   margin: 2px 4px;"
                "}"
                "QListWidget::item:hover {"
                "   background-color: #262c35;"
                "   color: #38bdf8;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: #0284c7;"
                "   color: #ffffff;"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: #181b20;"
                "   border-left: 1px solid #282d36;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet(
                "background-color: transparent;"
                "border: none;"
                "padding: 2px;"
            );
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;");
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #f8fafc;");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: #94a3b8; font-size: 11px; line-height: 1.5;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: #111317;"
                "   color: #e2e8f0;"
                "   border: 1px solid #282d36;"
                "   border-radius: 6px;"
                "   padding: 6px;"
                "}"
            );
        }
    } else { // BreezeDark
        m_topBar->setStyleSheet("background-color: #202326; border-bottom: 1px solid #2a2e33;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: #31363b;"
            "   border: 1px solid #3e444b;"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #fcfcfc; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: #31363b;"
            "   color: #eff0f1;"
            "   border: 1px solid #3e444b;"
            "   border-radius: 5px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #3daee9;"
            "   color: #ffffff;"
            "   border-color: #3daee9;"
            "}"
        );

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: #31363b;"
                "   color: #eff0f1;"
                "   border: 1px solid #3e444b;"
                "   border-radius: 5px;"
                "   font-size: 12px;"
                "   padding: 4px 10px;"
                "}"
                "QPushButton:checked {"
                "   background-color: #3daee9;"
                "   color: #ffffff;"
                "   border-color: #3daee9;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #1a1c1e;"
            "   border: none;"
            "   padding: 12px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #151719;"
                "   border-right: 1px solid #26292d;"
                "   padding: 8px 4px;"
                "   color: #a0a6ad;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 8px 10px;"
                "   border-radius: 6px;"
                "   margin: 2px 4px;"
                "}"
                "QListWidget::item:hover {"
                "   background-color: #23272b;"
                "   color: #3daee9;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: #3daee9;"
                "   color: #ffffff;"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: #151719;"
                "   border-left: 1px solid #26292d;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet(
                "background-color: transparent;"
                "border: none;"
                "padding: 2px;"
            );
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: #3daee9; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;");
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #fcfcfc;");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: #a0a6ad; font-size: 11px; line-height: 1.5;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: #0f1011;"
                "   color: #eff0f1;"
                "   border: 1px solid #26292d;"
                "   border-radius: 6px;"
                "   padding: 6px;"
                "}"
            );
        }
    }

    m_listWidget->viewport()->update();
}

void FolderViewer::toggleInspector()
{
    if (!m_previewPane) return;
    bool visible = !m_previewPane->isVisible();
    m_previewPane->setVisible(visible);
    if (m_btnToggleInspector) {
        m_btnToggleInspector->setChecked(visible);
    }
}

QStringList FolderViewer::customBookmarks() const
{
    QSettings settings("BubezView", "BubezView");
    return settings.value("FolderViewer/CustomBookmarks").toStringList();
}

void FolderViewer::addCustomBookmark(const QString &folderPath)
{
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
    QString canonical = QDir(folderPath).canonicalPath();
    QStringList bms = customBookmarks();
    if (!bms.contains(canonical)) {
        bms.append(canonical);
        QSettings settings("BubezView", "BubezView");
        settings.setValue("FolderViewer/CustomBookmarks", bms);
        populateBookmarks();
    }
}

void FolderViewer::removeCustomBookmark(const QString &folderPath)
{
    QString canonical = QDir(folderPath).canonicalPath();
    QStringList bms = customBookmarks();
    if (bms.removeAll(canonical) > 0 || bms.removeAll(folderPath) > 0) {
        QSettings settings("BubezView", "BubezView");
        settings.setValue("FolderViewer/CustomBookmarks", bms);
        populateBookmarks();
    }
}

void FolderViewer::populateBookmarks()
{
    if (!m_bookmarksWidget) return;
    m_bookmarksWidget->clear();

    auto findDir = [](QStandardPaths::StandardLocation loc, const QStringList &fallbacks) -> QString {
        QString p = QStandardPaths::writableLocation(loc);
        if (!p.isEmpty() && QDir(p).exists()) return p;
        QString home = QDir::homePath();
        for (const QString &fb : fallbacks) {
            QString candidate = home + "/" + fb;
            if (QDir(candidate).exists()) return candidate;
        }
        return QString();
    };

    struct BM {
        QString label;
        QString path;
    };
    QList<BM> list = {
        {"🏠 " + tr("Home"), QDir::homePath()},
        {"📄 " + tr("Documents"), findDir(QStandardPaths::DocumentsLocation, {"Documenti", "Documents", "Docs"})},
        {"⬇️ " + tr("Downloads"), findDir(QStandardPaths::DownloadLocation, {"Scaricati", "Downloads", "Download"})},
        {"🖼️ " + tr("Pictures"), findDir(QStandardPaths::PicturesLocation, {"Immagini", "Pictures", "Photos", "Foto"})},
        {"🎵 " + tr("Music"), findDir(QStandardPaths::MusicLocation, {"Musica", "Music", "Audio"})},
        {"🎬 " + tr("Videos"), findDir(QStandardPaths::MoviesLocation, {"Video", "Videos", "Movies", "Film"})},
        {"🖥️ " + tr("Desktop"), findDir(QStandardPaths::DesktopLocation, {"Scrivania", "Desktop"})},
        {"💾 " + tr("File System"), "/"}
    };

    for (const auto &bm : list) {
        if (!bm.path.isEmpty() && QDir(bm.path).exists()) {
            QListWidgetItem *item = new QListWidgetItem(bm.label, m_bookmarksWidget);
            item->setData(Qt::UserRole, bm.path);
            item->setData(Qt::UserRole + 1, false);
            item->setToolTip(bm.path);
        }
    }

    // Custom user bookmarks
    QStringList customList = customBookmarks();
    for (const QString &path : customList) {
        if (!path.isEmpty() && QDir(path).exists()) {
            QString folderName = QFileInfo(path).fileName();
            if (folderName.isEmpty()) folderName = path;
            QListWidgetItem *item = new QListWidgetItem("📌 " + folderName, m_bookmarksWidget);
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole + 1, true); // isCustom = true
            item->setToolTip(path);
        }
    }

    // Action item: Add custom bookmark
    QListWidgetItem *addItem = new QListWidgetItem("➕ " + tr("Add Folder..."), m_bookmarksWidget);
    addItem->setData(Qt::UserRole, QString());
    addItem->setData(Qt::UserRole + 1, false);
    addItem->setData(Qt::UserRole + 2, true); // isAddAction = true
    addItem->setToolTip(tr("Click to bookmark a custom folder"));
}

void FolderViewer::onItemSelectionChanged()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur) {
        updateInspectorPane(QString());
        emit selectionChanged(QString());
        return;
    }
    QString path = cur->data(Qt::UserRole).toString();
    updateInspectorPane(path);
    emit selectionChanged(path);
}

static QString formatPermissions(QFile::Permissions p)
{
    QString s;
    s += (p & QFile::ReadUser) ? "r" : "-";
    s += (p & QFile::WriteUser) ? "w" : "-";
    s += (p & QFile::ExeUser) ? "x" : "-";
    s += (p & QFile::ReadGroup) ? "r" : "-";
    s += (p & QFile::WriteGroup) ? "w" : "-";
    s += (p & QFile::ExeGroup) ? "x" : "-";
    s += (p & QFile::ReadOther) ? "r" : "-";
    s += (p & QFile::WriteOther) ? "w" : "-";
    s += (p & QFile::ExeOther) ? "x" : "-";
    return s;
}

void FolderViewer::updateInspectorPane(const QString &filePath)
{
    if (!m_previewPane) return;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        m_previewImgLabel->setVisible(true);
        m_previewImgLabel->clear();
        m_previewImgLabel->setText(tr("No item"));
        if (m_previewTextEdit) {
            m_previewTextEdit->clear();
            m_previewTextEdit->setVisible(false);
        }
        m_previewTitleLabel->setText(tr("Select a file or folder"));
        m_previewMetaLabel->clear();
        return;
    }

    QFileInfo fi(filePath);
    m_previewTitleLabel->setText(fi.fileName());

    auto makeRow = [](const QString &label, const QString &val) {
        return QString("<tr><td style='vertical-align: top; color: #828b99; padding-right: 8px; padding-bottom: 5px; white-space: nowrap;'><b>%1</b></td>"
                       "<td style='vertical-align: top; padding-bottom: 5px; word-break: break-all;'>%2</td></tr>")
               .arg(label, val);
    };

    if (fi.isDir()) {
        if (m_previewTextEdit) {
            m_previewTextEdit->setVisible(false);
        }
        m_previewImgLabel->setVisible(true);
        m_previewImgLabel->setPixmap(getFolderIcon(filePath).pixmap(84, 84));

        QDir d(filePath);
        QFileInfoList subEntries = d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        int fileCount = 0;
        int dirCount = 0;
        qint64 dirSize = 0;
        for (const QFileInfo &sub : subEntries) {
            if (sub.isDir()) {
                dirCount++;
            } else {
                fileCount++;
                dirSize += sub.size();
            }
        }

        QString html = "<table style='width: 100%; border-collapse: collapse; margin-top: 4px;'>";
        html += makeRow(tr("Type:"), tr("Folder"));
        html += makeRow(tr("Items:"), QString(tr("%1 items (%2 files, %3 folders)")).arg(subEntries.size()).arg(fileCount).arg(dirCount));
        html += makeRow(tr("Total Size:"), formatSize(dirSize));
        html += makeRow(tr("Location:"), fi.dir().dirName());
        html += makeRow(tr("Modified:"), fi.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        if (fi.birthTime().isValid()) {
            html += makeRow(tr("Created:"), fi.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
        } else if (fi.metadataChangeTime().isValid()) {
            html += makeRow(tr("Created:"), fi.metadataChangeTime().toString("yyyy-MM-dd HH:mm:ss"));
        }
        html += makeRow(tr("Permissions:"), formatPermissions(fi.permissions()));
        html += "</table>";

        m_previewMetaLabel->setText(html);
    } else {
        QMimeDatabase mimeDb;
        QMimeType mime = mimeDb.mimeTypeForFile(fi);
        QString typeComment = mime.comment();
        if (typeComment.isEmpty()) {
            typeComment = fi.suffix().toUpper() + " " + tr("File");
        }

        static const QStringList textExts = {
            "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
            "sh", "bash", "zsh", "fish", "js", "ts", "html", "htm", "css", "scss", "sass",
            "json", "xml", "yaml", "yml", "ini", "conf", "cfg", "csv", "tsv", "log",
            "sql", "cmake", "desktop", "service", "diff", "patch", "rs", "go", "java", "lua", "php", "rb"
        };
        static const QStringList nameMatches = {"makefile", "cmakelists.txt", "readme", "license", "authors", "changelog", "dockerfile"};

        QString ext = fi.suffix().toLower();
        bool isText = mime.inherits("text/plain") || textExts.contains(ext) || nameMatches.contains(fi.fileName().toLower());

        if (isText && m_previewTextEdit) {
            // Live micro-text preview
            m_previewImgLabel->setVisible(false);
            m_previewTextEdit->setVisible(true);

            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                QString previewContent;
                int linesRead = 0;
                while (!in.atEnd() && linesRead < 100) {
                    previewContent += in.readLine() + "\n";
                    linesRead++;
                }
                f.close();
                m_previewTextEdit->setPlainText(previewContent);
            } else {
                m_previewTextEdit->setPlainText(tr("(Unable to read text file)"));
            }
        } else {
            if (m_previewTextEdit) {
                m_previewTextEdit->setVisible(false);
            }
            m_previewImgLabel->setVisible(true);

            // Use custom thumbnail if already cached in item
            QListWidgetItem *item = m_listWidget->currentItem();
            QVariant thumbVar = item ? item->data(Qt::UserRole + 2) : QVariant();
            if (thumbVar.isValid() && !thumbVar.value<QPixmap>().isNull()) {
                m_previewImgLabel->setPixmap(thumbVar.value<QPixmap>().scaled(150, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                static const QStringList raster = {"png", "jpg", "jpeg", "webp", "bmp", "gif", "ico", "avif"};
                if (raster.contains(ext)) {
                    QImageReader reader(filePath);
                    reader.setAutoTransform(true);
                    QImage img = reader.read();
                    if (!img.isNull()) {
                        QPixmap pm = QPixmap::fromImage(img);
                        m_previewImgLabel->setPixmap(pm.scaled(150, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    } else {
                        m_previewImgLabel->setPixmap(getFileIcon(filePath).pixmap(72, 72));
                    }
                } else {
                    m_previewImgLabel->setPixmap(getFileIcon(filePath).pixmap(72, 72));
                }
            }
        }

        // Exact byte count with system locale formatting
        QString formattedBytes = QString("%1 (%2 %3)")
            .arg(formatSize(fi.size()))
            .arg(QLocale().toString(fi.size()))
            .arg(tr("bytes"));

        QString html = "<table style='width: 100%; border-collapse: collapse; margin-top: 4px;'>";
        html += makeRow(tr("Type:"), typeComment);
        html += makeRow(tr("Size:"), formattedBytes);
        html += makeRow(tr("Location:"), fi.dir().dirName());
        html += makeRow(tr("Modified:"), fi.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        if (fi.birthTime().isValid()) {
            html += makeRow(tr("Created:"), fi.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
        } else if (fi.metadataChangeTime().isValid()) {
            html += makeRow(tr("Created:"), fi.metadataChangeTime().toString("yyyy-MM-dd HH:mm:ss"));
        }
        html += makeRow(tr("Permissions:"), formatPermissions(fi.permissions()));

        // Extra image metadata
        static const QStringList imgExts = {"png", "jpg", "jpeg", "webp", "bmp", "gif", "ico", "tiff", "tif", "avif"};
        if (imgExts.contains(ext)) {
            QImageReader reader(filePath);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                html += makeRow(tr("Resolution:"), QString("%1 × %2 px").arg(sz.width()).arg(sz.height()));
            }
        }
        html += "</table>";

        m_previewMetaLabel->setText(html);
    }
}

