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
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QWheelEvent>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QPlainTextEdit>
#include <QLineEdit>
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
    // Reset DPR to 1.0 after painting at 2x resolution so downstream QPixmap scaling
    // and QPainter::drawPixmap use standard logical pixel dimensions without 50% shrinkage.
    img.setDevicePixelRatio(1.0);
    return img;
}

namespace {

bool isPreviewableImageExt(const QString &ext)
{
    static const QStringList imgExts = {
        "png", "jpg", "jpeg", "webp", "svg", "bmp", "gif", "ico", "tiff", "tif", "avif"
    };
    return imgExts.contains(ext);
}

bool isPreviewableTextFile(const QString &ext, const QString &fileName)
{
    static const QStringList textExts = {
        "txt", "md", "markdown", "py", "c", "cpp", "h", "hpp", "cc", "cxx",
        "sh", "bash", "zsh", "fish", "js", "ts", "html", "htm", "css", "scss", "sass",
        "json", "xml", "yaml", "yml", "ini", "conf", "cfg", "csv", "tsv", "log",
        "sql", "cmake", "desktop", "service", "diff", "patch", "rs", "go", "java", "lua", "php", "rb"
    };
    static const QStringList nameMatches = {
        "makefile", "cmakelists.txt", "readme", "license", "authors", "changelog", "dockerfile"
    };
    return textExts.contains(ext) || nameMatches.contains(fileName.toLower());
}

} // namespace

void ThumbnailWorker::run()
{
    for (const QString &path : m_filePaths) {
        if (m_isCancelled.load(std::memory_order_relaxed)) break;

        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        if (isPreviewableImageExt(ext)) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QSize sz = reader.size();
            const QSize targetSize(220, 150);
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                reader.setScaledSize(sz.scaled(targetSize, Qt::KeepAspectRatio));
            } else if (ext == "svg") {
                reader.setScaledSize(targetSize);
            }
            QImage img = reader.read();
            if (!img.isNull()) {
                if (img.width() > targetSize.width() || img.height() > targetSize.height()) {
                    img = img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                emit thumbnailReady(path, img);
            }
        } else if (ext == "pdf") {
            QImage img = PluginManager::renderPdfThumbnail(path, 220);
            if (!img.isNull()) {
                emit thumbnailReady(path, img.scaled(220, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        } else if (isPreviewableTextFile(ext, fi.fileName())) {
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
    return QSize(154, 168);
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

    QColor cardBg;
    QColor cardBorder;
    QColor textColor;
    QColor subTextColor;

    if (preset == ThemePreset::SystemNative) {
        QPalette pal = option.palette;
        if (isSelected) {
            cardBg = pal.highlight().color();
            cardBg.setAlpha(40);
            cardBorder = pal.highlight().color();
            textColor = pal.text().color();
            subTextColor = pal.placeholderText().color();
        } else if (isHover) {
            cardBg = pal.highlight().color();
            cardBg.setAlpha(18);
            cardBorder = pal.highlight().color();
            cardBorder.setAlpha(70);
            textColor = pal.text().color();
            subTextColor = pal.placeholderText().color();
        } else {
            cardBg = pal.base().color();
            cardBorder = pal.mid().color();
            cardBorder.setAlpha(80);
            textColor = pal.text().color();
            subTextColor = pal.placeholderText().color();
        }
    } else if (preset == ThemePreset::DarkSlate) {
        if (isSelected) {
            cardBg = QColor(56, 189, 248, 38);
            cardBorder = QColor("#38bdf8");
            textColor = QColor("#ffffff");
            subTextColor = QColor("#94a3b8");
        } else if (isHover) {
            cardBg = QColor(255, 255, 255, 18);
            cardBorder = QColor(56, 189, 248, 120);
            textColor = QColor("#f8fafc");
            subTextColor = QColor("#94a3b8");
        } else {
            cardBg = QColor(255, 255, 255, 8);
            cardBorder = QColor(255, 255, 255, 14);
            textColor = QColor("#cbd5e1");
            subTextColor = QColor("#64748b");
        }
    } else { // BreezeDark
        if (isSelected) {
            cardBg = QColor(61, 174, 233, 40);
            cardBorder = QColor("#3daee9");
            textColor = QColor("#ffffff");
            subTextColor = QColor("#a0a6ad");
        } else if (isHover) {
            cardBg = QColor(255, 255, 255, 18);
            cardBorder = QColor(61, 174, 233, 120);
            textColor = QColor("#fcfcfc");
            subTextColor = QColor("#a0a6ad");
        } else {
            cardBg = QColor(255, 255, 255, 8);
            cardBorder = QColor(255, 255, 255, 14);
            textColor = QColor("#cfd2d5");
            subTextColor = QColor("#64748b");
        }
    }

    // 1. Draw Card Base Surface (physical elevation feel)
    painter->setPen(QPen(cardBorder, isSelected ? 1.6 : 1.0));
    painter->setBrush(QBrush(cardBg));
    painter->drawRoundedRect(QRectF(rect), 9, 9);

    // 2. Draw Thumbnail or Icon
    QRect thumbArea(rect.left() + 6, rect.top() + 8, rect.width() - 12, 94);
    QVariant thumbVar = index.data(Qt::UserRole + 2); // Custom thumbnail pixmap

    if (thumbVar.isValid() && !thumbVar.value<QPixmap>().isNull()) {
        QPixmap pix = thumbVar.value<QPixmap>();
        // Fast path: if the thumbnail is already pre-scaled to fit thumbArea, use it directly to eliminate per-frame resizing
        QPixmap scaled;
        if (pix.width() <= thumbArea.width() && pix.height() <= thumbArea.height()) {
            scaled = pix;
        } else {
            int pxW = qMin(pix.width(), thumbArea.width());
            int pxH = qMin(pix.height(), thumbArea.height());
            scaled = pix.scaled(pxW, pxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        int x = thumbArea.left() + (thumbArea.width() - scaled.width()) / 2;
        int y = thumbArea.top() + (thumbArea.height() - scaled.height()) / 2;

        // Draw thumbnail with subtle rounded clip and frame
        QRect imgRect(x, y, scaled.width(), scaled.height());
        painter->save();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(imgRect), 5, 5);
        painter->setClipPath(clip);
        painter->drawPixmap(x, y, scaled);
        painter->restore();

        // 1px border around thumbnail
        painter->setPen(QPen(QColor(255, 255, 255, 30), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(imgRect), 5, 5);
    } else {
        QVariant iconVar = index.data(Qt::DecorationRole);
        if (iconVar.isValid() && iconVar.canConvert<QIcon>()) {
            QIcon icon = iconVar.value<QIcon>();
            int iconSize = 70;
            int x = thumbArea.left() + (thumbArea.width() - iconSize) / 2;
            int y = thumbArea.top() + (thumbArea.height() - iconSize) / 2;
            icon.paint(painter, x, y, iconSize, iconSize);
        }
    }

    // 3. Draw Primary Label (File / Folder Name)
    QRect titleArea(rect.left() + 6, rect.top() + 106, rect.width() - 12, 20);
    QFont titleFont = option.font;
    titleFont.setPointSize(9);
    titleFont.setWeight(isSelected ? QFont::DemiBold : QFont::Medium);
    painter->setFont(titleFont);
    painter->setPen(textColor);

    QString text = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(titleFont);
    QString elidedTitle = fm.elidedText(text, Qt::ElideMiddle, titleArea.width());
    painter->drawText(titleArea, Qt::AlignHCenter | Qt::AlignVCenter, elidedTitle);

    // 4. Draw Secondary Label (Sub-info: Size or "Folder")
    QRect subArea(rect.left() + 6, rect.top() + 128, rect.width() - 12, 16);
    QFont subFont = option.font;
    subFont.setPointSize(8);
    subFont.setWeight(QFont::Normal);
    painter->setFont(subFont);
    painter->setPen(subTextColor);

    QString subText = index.data(Qt::UserRole + 3).toString();
    if (subText.isEmpty()) {
        bool isDir = index.data(Qt::UserRole + 1).toBool();
        subText = isDir ? tr("Folder") : QString();
    }
    if (!subText.isEmpty()) {
        QFontMetrics subFm(subFont);
        QString elidedSub = subFm.elidedText(subText, Qt::ElideRight, subArea.width());
        painter->drawText(subArea, Qt::AlignHCenter | Qt::AlignVCenter, elidedSub);
    }

    painter->restore();
}

/**
 * @brief High-performance QListWidget with smooth progressive mouse-wheel scrolling.
 *
 * Replaces default violent multi-row jumps with a responsive, easing-interpolated
 * progressive scroll (60px per wheel notch) and 1:1 precision touchpad tracking.
 */
class FolderListWidget : public QListWidget {
public:
    explicit FolderListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
        , m_targetScrollValue(0)
    {
        m_scrollAnimation = new QPropertyAnimation(verticalScrollBar(), "value", this);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

        if (verticalScrollBar()) {
            verticalScrollBar()->setSingleStep(40);
            connect(verticalScrollBar(), &QScrollBar::sliderPressed,
                    m_scrollAnimation, &QPropertyAnimation::stop);
        }
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        // 1. High-precision touchpad / smooth kinetic wheel support (Wayland/libinput/macOS)
        if (event->pixelDelta().y() != 0) {
            m_scrollAnimation->stop();
            if (verticalScrollBar()) {
                verticalScrollBar()->setValue(verticalScrollBar()->value() - event->pixelDelta().y());
            }
            event->accept();
            return;
        }

        // 2. Progressive easing scroll for standard stepped mouse wheels
        int angleY = event->angleDelta().y();
        if (angleY != 0 && verticalScrollBar()) {
            double notches = static_cast<double>(angleY) / 120.0;
            // 60 pixels per notch for a smooth, progressive descent/ascent
            int deltaPixels = qRound(notches * 60.0);

            int currentVal = (m_scrollAnimation->state() == QAbstractAnimation::Running)
                                 ? m_targetScrollValue
                                 : verticalScrollBar()->value();

            int minVal = verticalScrollBar()->minimum();
            int maxVal = verticalScrollBar()->maximum();
            m_targetScrollValue = qBound(minVal, currentVal - deltaPixels, maxVal);

            if (m_targetScrollValue != verticalScrollBar()->value()) {
                m_scrollAnimation->stop();
                m_scrollAnimation->setDuration(160);
                m_scrollAnimation->setStartValue(verticalScrollBar()->value());
                m_scrollAnimation->setEndValue(m_targetScrollValue);
                m_scrollAnimation->start();
            }
            event->accept();
            return;
        }

        QListWidget::wheelEvent(event);
    }

    void updateGeometries() override
    {
        QListWidget::updateGeometries();
        if (verticalScrollBar()) {
            // Keep singleStep at a comfortable progressive 40px
            verticalScrollBar()->setSingleStep(40);
        }
    }

    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override
    {
        if (m_scrollAnimation && m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
        }
        QListWidget::currentChanged(current, previous);
    }

private:
    QPropertyAnimation *m_scrollAnimation = nullptr;
    int m_targetScrollValue = 0;
};

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
    , m_filterEdit(nullptr)
    , m_btnToggleInspector(nullptr)
    , m_bookmarksWidget(nullptr)
    , m_listWidget(nullptr)
    , m_previewPane(nullptr)
    , m_inspectorBadge(nullptr)
    , m_btnCloseInspector(nullptr)
    , m_previewCard(nullptr)
    , m_previewImgLabel(nullptr)
    , m_previewTextEdit(nullptr)
    , m_previewTitleLabel(nullptr)
    , m_previewMetaLabel(nullptr)
    , m_btnOpenSelected(nullptr)
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
    m_topBar->setFixedHeight(40);

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(10, 4, 10, 4);
    topLayout->setSpacing(8);

    m_btnUp = new QPushButton(QIcon::fromTheme("go-up"), tr("Up"), m_topBar);
    m_btnUp->setCursor(Qt::PointingHandCursor);
    m_btnUp->setToolTip(tr("Go to parent folder (Backspace)"));
    m_btnUp->setFixedHeight(28);
    m_btnUp->setMinimumWidth(68);
    connect(m_btnUp, &QPushButton::clicked, this, &FolderViewer::goUpDirectory);
    topLayout->addWidget(m_btnUp);

    topLayout->addSpacing(4);

    m_pillBadge = new QFrame(m_topBar);
    m_pillBadge->setObjectName("FolderPillBadge");
    m_pillBadge->setFixedHeight(28);

    QHBoxLayout *pillLayout = new QHBoxLayout(m_pillBadge);
    pillLayout->setContentsMargins(10, 0, 12, 0);
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

    // Quick Filter Edit
    m_filterEdit = new QLineEdit(m_topBar);
    m_filterEdit->setPlaceholderText(tr("Filter files..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setFixedHeight(28);
    m_filterEdit->setFixedWidth(175);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FolderViewer::onFilterTextChanged);
    topLayout->addWidget(m_filterEdit);

    m_btnToggleInspector = new QPushButton(QIcon::fromTheme("sidebar-show-right", QIcon::fromTheme("dialog-information")), tr("Details"), m_topBar);
    m_btnToggleInspector->setCheckable(true);
    m_btnToggleInspector->setChecked(true);
    m_btnToggleInspector->setCursor(Qt::PointingHandCursor);
    m_btnToggleInspector->setToolTip(tr("Show or hide details panel (Key I)"));
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
    m_bookmarksWidget->setFixedWidth(180);
    m_bookmarksWidget->setIconSize(QSize(18, 18));
    m_bookmarksWidget->setFrameShape(QFrame::NoFrame);
    m_bookmarksWidget->setFocusPolicy(Qt::NoFocus);
    m_bookmarksWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    if (m_bookmarksWidget->verticalScrollBar()) {
        m_bookmarksWidget->verticalScrollBar()->setSingleStep(28);
    }
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
        if (item && item->data(Qt::UserRole + 10).toBool()) {
            return;
        }
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
    m_listWidget = new FolderListWidget(this);
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setMovement(QListWidget::Static);
    m_listWidget->setSpacing(10);
    m_listWidget->setGridSize(QSize(154, 168));
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (m_listWidget->verticalScrollBar()) {
        m_listWidget->verticalScrollBar()->setSingleStep(40);
    }
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

    // Right Column: Modern Inspector & Preview
    m_previewPane = new QWidget(this);
    m_previewPane->setFixedWidth(290);
    QVBoxLayout *prevLayout = new QVBoxLayout(m_previewPane);
    prevLayout->setContentsMargins(14, 12, 14, 14);
    prevLayout->setSpacing(10);
    prevLayout->setAlignment(Qt::AlignTop);

    // Top Header Row with Title and Close Button
    QWidget *headerWidget = new QWidget(m_previewPane);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_inspectorBadge = new QLabel(tr("DETAILS"), headerWidget);
    m_inspectorBadge->setStyleSheet("font-size: 11px; font-weight: bold; letter-spacing: 0.8px;");
    headerLayout->addWidget(m_inspectorBadge);
    headerLayout->addStretch(1);

    m_btnCloseInspector = new QPushButton("✕", headerWidget);
    m_btnCloseInspector->setFixedSize(22, 22);
    m_btnCloseInspector->setCursor(Qt::PointingHandCursor);
    m_btnCloseInspector->setToolTip(tr("Close details panel"));
    connect(m_btnCloseInspector, &QPushButton::clicked, this, &FolderViewer::toggleInspector);
    headerLayout->addWidget(m_btnCloseInspector);

    prevLayout->addWidget(headerWidget);

    // Preview Card Container (rounded dark box)
    m_previewCard = new QFrame(m_previewPane);
    m_previewCard->setFixedHeight(140);
    QVBoxLayout *cardLayout = new QVBoxLayout(m_previewCard);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(0);

    m_previewImgLabel = new QLabel(m_previewCard);
    m_previewImgLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_previewImgLabel);

    m_previewTextEdit = new QPlainTextEdit(m_previewCard);
    m_previewTextEdit->setReadOnly(true);
    m_previewTextEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(8.5);
    m_previewTextEdit->setFont(monoFont);
    m_previewTextEdit->setVisible(false);
    cardLayout->addWidget(m_previewTextEdit);

    prevLayout->addWidget(m_previewCard);

    // Item Title
    m_previewTitleLabel = new QLabel(tr("Select an item"), m_previewPane);
    m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_previewTitleLabel->setWordWrap(true);
    m_previewTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_previewTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    prevLayout->addWidget(m_previewTitleLabel);

    // Metadata Card
    m_previewMetaLabel = new QLabel(m_previewPane);
    m_previewMetaLabel->setStyleSheet("font-size: 11px; line-height: 1.4;");
    m_previewMetaLabel->setWordWrap(true);
    m_previewMetaLabel->setTextFormat(Qt::RichText);
    m_previewMetaLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_previewMetaLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    prevLayout->addWidget(m_previewMetaLabel);

    prevLayout->addStretch(1);

    // Open Action Button at bottom of inspector
    m_btnOpenSelected = new QPushButton(tr("Open"), m_previewPane);
    m_btnOpenSelected->setCursor(Qt::PointingHandCursor);
    m_btnOpenSelected->setFixedHeight(32);
    connect(m_btnOpenSelected, &QPushButton::clicked, this, &FolderViewer::openSelectedItem);
    prevLayout->addWidget(m_btnOpenSelected);

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
    if (folderPath.isEmpty()) return false;

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    clear();

    m_currentFolder = dir.canonicalPath();
    if (m_currentFolder.isEmpty()) {
        m_currentFolder = dir.absolutePath();
    }

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

            QString ext = info.suffix().toLower();
            if (isPreviewableImageExt(ext) || ext == "pdf" || isPreviewableTextFile(ext, name)) {
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
        item->setData(Qt::UserRole + 3, tr("Folder")); // Sub-label
        item->setToolTip(QString("📁 %1\n%2").arg(dInfo.fileName(), dInfo.absoluteFilePath()));
        m_listWidget->addItem(item);
        m_pathToItem.insert(dInfo.absoluteFilePath(), item);
    }

    // 2. Add Files
    for (const QFileInfo &fInfo : files) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(fInfo.fileName());
        item->setIcon(getFileIcon(fInfo.absoluteFilePath()));
        item->setData(Qt::UserRole, fInfo.absoluteFilePath());
        item->setData(Qt::UserRole + 1, false); // isDir
        item->setData(Qt::UserRole + 3, formatSize(fInfo.size())); // Sub-label
        item->setToolTip(QString("📄 %1 (%2)\n%3").arg(fInfo.fileName(), formatSize(fInfo.size()), fInfo.absoluteFilePath()));
        m_listWidget->addItem(item);
        m_pathToItem.insert(fInfo.absoluteFilePath(), item);
    }

    // Clear filter input if any
    if (m_filterEdit && !m_filterEdit->text().isEmpty()) {
        m_filterEdit->blockSignals(true);
        m_filterEdit->clear();
        m_filterEdit->blockSignals(false);
    }

    // Update Pill Badge
    QFileInfo currentDirInfo(m_currentFolder);
    QString folderTitle = currentDirInfo.fileName();
    if (folderTitle.isEmpty()) folderTitle = m_currentFolder;

    m_pillText->setText(QString(tr("%1 • %2 Items (%3)"))
        .arg(folderTitle)
        .arg(m_itemCount)
        .arg(formatSize(m_totalSize)));

    // Synchronize bookmarks sidebar active item
    updateBookmarksHighlight();

    // Launch async thumbnail worker
    if (!asyncThumbnailCandidates.isEmpty()) {
        ThumbnailWorker *worker = new ThumbnailWorker(asyncThumbnailCandidates);
        m_worker = worker;
        connect(worker, &ThumbnailWorker::thumbnailReady, this, &FolderViewer::onThumbnailReady);
        connect(worker, &ThumbnailWorker::finished, worker, &QObject::deleteLater);
        worker->start();
    }

    emit folderLoaded(m_currentFolder, m_itemCount, m_totalSize);

    // Auto-select first item so preview pane is immediately populated, or reset if folder is empty
    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    } else {
        updateInspectorPane(QString());
        emit selectionChanged(QString());
    }

    return true;
}

void FolderViewer::onThumbnailReady(const QString &filePath, const QImage &image)
{
    if (image.isNull()) return;

    QListWidgetItem *item = m_pathToItem.value(filePath, nullptr);
    if (!item) return;

    // Pre-scale thumbnail to exact delegate thumbArea size (134x94) once to eliminate per-frame scaling in paint()
    QPixmap fullPixmap = QPixmap::fromImage(image);
    QPixmap cardPixmap = fullPixmap.scaled(134, 94, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    item->setData(Qt::UserRole + 2, cardPixmap);
    m_listWidget->update(m_listWidget->visualItemRect(item));

    // If this is the currently selected item and the image preview label is active, update the side preview from fullPixmap
    if (m_listWidget->currentItem() == item && m_previewImgLabel && m_previewImgLabel->isVisible()) {
        m_previewImgLabel->setPixmap(fullPixmap.scaled(150, 110, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void FolderViewer::updateThumbnail(const QString &filePath, const QImage &image)
{
    if (image.isNull()) return;
    onThumbnailReady(filePath, image.scaled(220, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
        disconnect(m_worker.data(), nullptr, nullptr, nullptr);
        m_worker->cancelAndWait();
        delete m_worker.data();
        m_worker = nullptr;
    }
    m_pathToItem.clear();
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
        return VectorIcons::createIcon(VectorIcons::Type::FolderDownloads);
    } else if (name == "pictures" || name == "immagini" || name == "photos" || name == "foto") {
        return VectorIcons::createIcon(VectorIcons::Type::FolderPictures);
    } else if (name == "documents" || name == "documenti" || name == "docs") {
        return VectorIcons::createIcon(VectorIcons::Type::FolderDocuments);
    } else if (name == "music" || name == "musica" || name == "audio") {
        return VectorIcons::createIcon(VectorIcons::Type::FolderMusic);
    } else if (name == "videos" || name == "video" || name == "movies" || name == "film") {
        return VectorIcons::createIcon(VectorIcons::Type::FolderVideos);
    } else if (name == "desktop" || name == "scrivania") {
        return VectorIcons::createIcon(VectorIcons::Type::FolderDesktop);
    }
    return VectorIcons::createIcon(VectorIcons::Type::Folder);
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
            "   border-radius: 14px;"
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

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: palette(base);"
                "   color: palette(text);"
                "   border: 1px solid palette(mid);"
                "   border-radius: 6px;"
                "   padding: 3px 8px;"
                "   font-size: 11px;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: palette(window);"
            "   border: none;"
            "   padding: 10px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: palette(window);"
                "   border-right: 1px solid palette(mid);"
                "   padding: 6px 0px;"
                "   color: palette(window-text);"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 5px 10px;"
                "   border-radius: 6px;"
                "   margin: 1px 6px;"
                "}"
                "QListWidget::item:hover:!disabled {"
                "   background-color: palette(alternate-base);"
                "}"
                "QListWidget::item:selected {"
                "   background-color: palette(highlight);"
                "   color: palette(highlighted-text);"
                "   font-weight: 600;"
                "}"
                "QListWidget::item:disabled {"
                "   padding-top: 8px;"
                "   padding-bottom: 2px;"
                "   font-weight: bold;"
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

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: palette(base);"
                "   border: 1px solid palette(mid);"
                "   border-radius: 8px;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet("background-color: transparent; border: none; padding: 2px;");
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: palette(highlight); font-size: 11px; font-weight: bold; letter-spacing: 0.8px;");
        }
        if (m_btnCloseInspector) {
            m_btnCloseInspector->setStyleSheet("QPushButton { background-color: transparent; border: none; font-size: 11px; border-radius: 11px; } QPushButton:hover { background-color: palette(mid); }");
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: palette(window-text); font-size: 11px; line-height: 1.4;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: palette(base);"
                "   color: palette(text);"
                "   border: none;"
                "   padding: 4px;"
                "}"
            );
        }
        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: palette(button);"
                "   color: palette(button-text);"
                "   border: 1px solid palette(mid);"
                "   border-radius: 5px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QPushButton:checked {"
                "   background-color: palette(highlight);"
                "   color: palette(highlighted-text);"
                "   border-color: palette(highlight);"
                "}"
            );
        }
        if (m_btnOpenSelected) {
            m_btnOpenSelected->setStyleSheet(
                "QPushButton {"
                "   background-color: palette(highlight);"
                "   color: palette(highlighted-text);"
                "   border: none;"
                "   border-radius: 6px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:disabled {"
                "   background-color: palette(button);"
                "   color: palette(placeholder-text);"
                "}"
            );
        }
    } else if (preset == ThemePreset::DarkSlate) {
        m_topBar->setStyleSheet("background-color: #1e2228; border-bottom: 1px solid #282d36;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   border: 1px solid rgba(255, 255, 255, 0.09);"
            "   border-radius: 14px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #e2e8f0; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: #262c35;"
            "   color: #cbd5e1;"
            "   border: 1px solid #3b4252;"
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

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: #14171d;"
                "   color: #f1f5f9;"
                "   border: 1px solid #333842;"
                "   border-radius: 6px;"
                "   padding: 3px 8px;"
                "   font-size: 11px;"
                "}"
                "QLineEdit:focus {"
                "   border-color: #38bdf8;"
                "}"
            );
        }

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: #262c35;"
                "   color: #cbd5e1;"
                "   border: 1px solid #3b4252;"
                "   border-radius: 5px;"
                "   font-size: 12px;"
                "   padding: 4px 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #333b47;"
                "   color: #f8fafc;"
                "}"
                "QPushButton:checked {"
                "   background-color: rgba(56, 189, 248, 0.22);"
                "   color: #38bdf8;"
                "   border-color: #38bdf8;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #181a1f;"
            "   border: none;"
            "   padding: 10px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #14161b;"
                "   border-right: 1px solid #23272f;"
                "   padding: 6px 0px;"
                "   color: #94a3b8;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 5px 10px;"
                "   border-radius: 6px;"
                "   margin: 1px 8px;"
                "}"
                "QListWidget::item:hover:!disabled {"
                "   background-color: rgba(255, 255, 255, 0.06);"
                "   color: #f1f5f9;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: rgba(56, 189, 248, 0.16);"
                "   color: #38bdf8;"
                "   font-weight: 600;"
                "}"
                "QListWidget::item:disabled {"
                "   padding-top: 8px;"
                "   padding-bottom: 2px;"
                "   font-weight: bold;"
                "   color: #64748b;"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: #14161b;"
                "   border-left: 1px solid #23272f;"
                "}"
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: #111317;"
                "   border: 1px solid #282d36;"
                "   border-radius: 8px;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet("background-color: transparent; border: none; padding: 2px;");
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: bold; letter-spacing: 0.8px;");
        }
        if (m_btnCloseInspector) {
            m_btnCloseInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: transparent;"
                "   color: #64748b;"
                "   border: none;"
                "   border-radius: 11px;"
                "   font-size: 11px;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(255, 255, 255, 0.1);"
                "   color: #f1f5f9;"
                "}"
            );
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #f8fafc;");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: #94a3b8; font-size: 11px; line-height: 1.4;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: #111317;"
                "   color: #e2e8f0;"
                "   border: none;"
                "   padding: 4px;"
                "}"
            );
        }
        if (m_btnOpenSelected) {
            m_btnOpenSelected->setStyleSheet(
                "QPushButton {"
                "   background-color: #0284c7;"
                "   color: #ffffff;"
                "   border: none;"
                "   border-radius: 6px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "   background-color: #0369a1;"
                "}"
                "QPushButton:disabled {"
                "   background-color: #22262d;"
                "   color: #475569;"
                "}"
            );
        }
    } else { // BreezeDark
        m_topBar->setStyleSheet("background-color: #202326; border-bottom: 1px solid #2a2e33;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   border: 1px solid rgba(255, 255, 255, 0.09);"
            "   border-radius: 14px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #eff0f1; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: #26292d;"
            "   color: #cfd2d5;"
            "   border: 1px solid #31363b;"
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

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: #141618;"
                "   color: #eff0f1;"
                "   border: 1px solid #31363b;"
                "   border-radius: 6px;"
                "   padding: 3px 8px;"
                "   font-size: 11px;"
                "}"
                "QLineEdit:focus {"
                "   border-color: #3daee9;"
                "}"
            );
        }

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: #26292d;"
                "   color: #cfd2d5;"
                "   border: 1px solid #31363b;"
                "   border-radius: 5px;"
                "   font-size: 12px;"
                "   padding: 4px 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #31363b;"
                "   color: #eff0f1;"
                "}"
                "QPushButton:checked {"
                "   background-color: rgba(61, 174, 233, 0.22);"
                "   color: #3daee9;"
                "   border-color: #3daee9;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #17191c;"
            "   border: none;"
            "   padding: 10px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #151719;"
                "   border-right: 1px solid #23262a;"
                "   padding: 6px 0px;"
                "   color: #a0a6ad;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 5px 10px;"
                "   border-radius: 6px;"
                "   margin: 1px 8px;"
                "}"
                "QListWidget::item:hover:!disabled {"
                "   background-color: rgba(255, 255, 255, 0.06);"
                "   color: #fcfcfc;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: rgba(61, 174, 233, 0.18);"
                "   color: #3daee9;"
                "   font-weight: 600;"
                "}"
                "QListWidget::item:disabled {"
                "   padding-top: 8px;"
                "   padding-bottom: 2px;"
                "   font-weight: bold;"
                "   color: #64748b;"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: #151719;"
                "   border-left: 1px solid #23262a;"
                "}"
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: #101214;"
                "   border: 1px solid #26292d;"
                "   border-radius: 8px;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet("background-color: transparent; border: none; padding: 2px;");
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: #3daee9; font-size: 11px; font-weight: bold; letter-spacing: 0.8px;");
        }
        if (m_btnCloseInspector) {
            m_btnCloseInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: transparent;"
                "   color: #64748b;"
                "   border: none;"
                "   border-radius: 11px;"
                "   font-size: 11px;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(255, 255, 255, 0.1);"
                "   color: #fcfcfc;"
                "}"
            );
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #fcfcfc;");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: #a0a6ad; font-size: 11px; line-height: 1.4;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: #0f1011;"
                "   color: #eff0f1;"
                "   border: none;"
                "   padding: 4px;"
                "}"
            );
        }
        if (m_btnOpenSelected) {
            m_btnOpenSelected->setStyleSheet(
                "QPushButton {"
                "   background-color: #3daee9;"
                "   color: #ffffff;"
                "   border: none;"
                "   border-radius: 6px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "   background-color: #2980b9;"
                "}"
                "QPushButton:disabled {"
                "   background-color: #222528;"
                "   color: #475569;"
                "}"
            );
        }
    }

    updateInspectorPane(selectedFilePath());
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
    if (visible) {
        updateInspectorPane(selectedFilePath());
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

    QFont hFont = m_bookmarksWidget->font();
    hFont.setPointSize(8);
    hFont.setBold(true);

    // Category Header: PLACES
    QListWidgetItem *placesHeader = new QListWidgetItem(tr("PLACES"), m_bookmarksWidget);
    placesHeader->setFlags(Qt::NoItemFlags); // Non-selectable header
    placesHeader->setData(Qt::UserRole + 10, true); // isHeader = true
    placesHeader->setFont(hFont);
    placesHeader->setSizeHint(QSize(170, 24));

    struct BM {
        QString label;
        QString path;
        VectorIcons::Type iconType;
    };
    QList<BM> list = {
        {tr("Home"), QDir::homePath(), VectorIcons::Type::Folder},
        {tr("Documents"), findDir(QStandardPaths::DocumentsLocation, {"Documenti", "Documents", "Docs"}), VectorIcons::Type::FolderDocuments},
        {tr("Downloads"), findDir(QStandardPaths::DownloadLocation, {"Scaricati", "Downloads", "Download"}), VectorIcons::Type::FolderDownloads},
        {tr("Pictures"), findDir(QStandardPaths::PicturesLocation, {"Immagini", "Pictures", "Photos", "Foto"}), VectorIcons::Type::FolderPictures},
        {tr("Music"), findDir(QStandardPaths::MusicLocation, {"Musica", "Music", "Audio"}), VectorIcons::Type::FolderMusic},
        {tr("Videos"), findDir(QStandardPaths::MoviesLocation, {"Video", "Videos", "Movies", "Film"}), VectorIcons::Type::FolderVideos},
        {tr("Desktop"), findDir(QStandardPaths::DesktopLocation, {"Scrivania", "Desktop"}), VectorIcons::Type::FolderDesktop},
        {tr("File System"), "/", VectorIcons::Type::Folder}
    };

    for (const auto &bm : list) {
        if (!bm.path.isEmpty() && QDir(bm.path).exists()) {
            QListWidgetItem *item = new QListWidgetItem(bm.label, m_bookmarksWidget);
            item->setIcon(VectorIcons::createIcon(bm.iconType));
            item->setData(Qt::UserRole, bm.path);
            item->setData(Qt::UserRole + 1, false); // isCustom = false
            item->setSizeHint(QSize(170, 30));
            item->setToolTip(bm.path);
        }
    }

    // Category Header: BOOKMARKS
    QListWidgetItem *bmHeader = new QListWidgetItem(tr("BOOKMARKS"), m_bookmarksWidget);
    bmHeader->setFlags(Qt::NoItemFlags); // Non-selectable header
    bmHeader->setData(Qt::UserRole + 10, true); // isHeader = true
    bmHeader->setFont(hFont);
    bmHeader->setSizeHint(QSize(170, 28));

    // Custom user bookmarks
    QStringList customList = customBookmarks();
    for (const QString &path : customList) {
        if (!path.isEmpty() && QDir(path).exists()) {
            QString folderName = QFileInfo(path).fileName();
            if (folderName.isEmpty()) folderName = path;
            QListWidgetItem *item = new QListWidgetItem(folderName, m_bookmarksWidget);
            item->setIcon(VectorIcons::createIcon(VectorIcons::Type::Folder));
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole + 1, true); // isCustom = true
            item->setSizeHint(QSize(170, 30));
            item->setToolTip(path);
        }
    }

    // Action item: Add custom bookmark
    QListWidgetItem *addItem = new QListWidgetItem(tr("+ Add Folder..."), m_bookmarksWidget);
    addItem->setIcon(QIcon::fromTheme("list-add"));
    addItem->setData(Qt::UserRole, QString());
    addItem->setData(Qt::UserRole + 1, false);
    addItem->setData(Qt::UserRole + 2, true); // isAddAction = true
    addItem->setSizeHint(QSize(170, 30));
    addItem->setToolTip(tr("Click to bookmark a custom folder"));

    updateBookmarksHighlight();
}

void FolderViewer::updateBookmarksHighlight()
{
    if (!m_bookmarksWidget) return;
    if (m_currentFolder.isEmpty()) {
        m_bookmarksWidget->setCurrentItem(nullptr);
        return;
    }
    for (int i = 0; i < m_bookmarksWidget->count(); ++i) {
        QListWidgetItem *it = m_bookmarksWidget->item(i);
        if (it && !it->data(Qt::UserRole + 10).toBool()) {
            QString path = it->data(Qt::UserRole).toString();
            if (!path.isEmpty() && QDir(path).canonicalPath() == m_currentFolder) {
                m_bookmarksWidget->setCurrentItem(it);
                return;
            }
        }
    }
    m_bookmarksWidget->setCurrentItem(nullptr);
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

void FolderViewer::onFilterTextChanged(const QString &text)
{
    QString query = text.trimmed();
    QListWidgetItem *firstVisible = nullptr;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!item) continue;
        if (query.isEmpty()) {
            item->setHidden(false);
            if (!firstVisible) firstVisible = item;
        } else {
            bool matches = item->text().contains(query, Qt::CaseInsensitive);
            item->setHidden(!matches);
            if (matches && !firstVisible) {
                firstVisible = item;
            }
        }
    }

    // Ensure selection tracks visible items to avoid phantom selection of hidden elements
    QListWidgetItem *current = m_listWidget->currentItem();
    if (!current || current->isHidden()) {
        if (firstVisible) {
            m_listWidget->setCurrentItem(firstVisible);
        } else {
            m_listWidget->clearSelection();
            m_listWidget->setCurrentItem(nullptr);
            updateInspectorPane(QString());
            emit selectionChanged(QString());
        }
    }
}

void FolderViewer::openSelectedItem()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (cur) {
        onItemDoubleClicked(cur);
    }
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
        if (m_btnOpenSelected) m_btnOpenSelected->setEnabled(false);
        m_previewImgLabel->setVisible(true);
        m_previewImgLabel->clear();
        m_previewImgLabel->setText(tr("No item selected"));
        if (m_previewTextEdit) {
            m_previewTextEdit->clear();
            m_previewTextEdit->setVisible(false);
        }
        m_previewTitleLabel->setText(tr("Select a file or folder"));
        m_previewMetaLabel->clear();
        return;
    }

    if (m_btnOpenSelected) m_btnOpenSelected->setEnabled(true);

    QFileInfo fi(filePath);
    m_previewTitleLabel->setText(fi.fileName());

    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);
    if (preset == ThemePreset::SystemNative) {
        isDark = (palette().color(QPalette::WindowText).lightness() > 128);
    }

    QString labelColor = isDark ? "#94a3b8" : "#64748b";
    QString valColor = isDark ? "#f1f5f9" : "#0f172a";
    QString pillBg = isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.06)";
    QString pillColor = isDark ? "#38bdf8" : "#0284c7";
    QString cardBg = isDark ? "rgba(255, 255, 255, 0.03)" : "rgba(0, 0, 0, 0.03)";
    QString cardBorder = isDark ? "rgba(255, 255, 255, 0.07)" : "rgba(0, 0, 0, 0.08)";

    auto makeRow = [labelColor, valColor, pillBg, pillColor](const QString &label, const QString &val, bool isPill = false) {
        const QString safeLabel = label.toHtmlEscaped();
        const QString safeVal = val.toHtmlEscaped();
        if (isPill) {
            return QString(
                "<tr>"
                "  <td style='width: 82px; vertical-align: middle; padding: 4px 0; color: %1; font-size: 11px; font-weight: 500;'>%2</td>"
                "  <td style='vertical-align: middle; padding: 4px 0;'><span style='background-color: %3; color: %4; padding: 2px 7px; border-radius: 4px; font-family: monospace; font-size: 11px;'>%5</span></td>"
                "</tr>"
            ).arg(labelColor, safeLabel, pillBg, pillColor, safeVal);
        }
        return QString(
            "<tr>"
            "  <td style='width: 82px; vertical-align: top; padding: 4px 0; color: %1; font-size: 11px; font-weight: 500;'>%2</td>"
            "  <td style='vertical-align: top; padding: 4px 0; color: %3; font-size: 11px; font-weight: 500; word-break: break-all;'>%4</td>"
            "</tr>"
        ).arg(labelColor, safeLabel, valColor, safeVal);
    };

    if (fi.isDir()) {
        if (m_previewTextEdit) {
            m_previewTextEdit->setVisible(false);
        }
        m_previewImgLabel->setVisible(true);
        m_previewImgLabel->setPixmap(getFolderIcon(filePath).pixmap(84, 84));

        QDir d(filePath);
        QFileInfoList subEntries = d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::NoSort);
        int fileCount = 0;
        int dirCount = 0;
        qint64 dirSize = 0;
        for (const QFileInfo &sub : subEntries) {
            if (sub.fileName().startsWith('.')) continue;
            if (sub.isDir()) {
                dirCount++;
            } else {
                fileCount++;
                dirSize += sub.size();
            }
        }

        QString html = QString("<div style='background-color: %1; border: 1px solid %2; border-radius: 8px; padding: 10px 12px; margin-top: 6px;'>")
            .arg(cardBg, cardBorder);
        html += "<table style='width: 100%; border-collapse: collapse;'>";
        html += makeRow(tr("Type"), tr("Folder"));
        html += makeRow(tr("Items"), QString(tr("%1 (%2 files, %3 folders)")).arg(fileCount + dirCount).arg(fileCount).arg(dirCount));
        html += makeRow(tr("Total Size"), formatSize(dirSize));
        html += makeRow(tr("Location"), fi.dir().dirName());
        html += makeRow(tr("Modified"), fi.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        if (fi.birthTime().isValid()) {
            html += makeRow(tr("Created"), fi.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
        } else if (fi.metadataChangeTime().isValid()) {
            html += makeRow(tr("Created"), fi.metadataChangeTime().toString("yyyy-MM-dd HH:mm:ss"));
        }
        html += makeRow(tr("Permissions"), formatPermissions(fi.permissions()), true);
        html += "</table></div>";

        m_previewMetaLabel->setText(html);
    } else {
        QMimeDatabase mimeDb;
        QMimeType mime = mimeDb.mimeTypeForFile(fi);
        QString typeComment = mime.comment();
        if (typeComment.isEmpty()) {
            typeComment = fi.suffix().toUpper() + " " + tr("File");
        }

        QString ext = fi.suffix().toLower();
        // Exclude SVG from raw text preview since image/svg+xml inherits text/plain in shared-mime-info
        bool isText = (ext != "svg") && (mime.inherits("text/plain") || isPreviewableTextFile(ext, fi.fileName()));

        if (isText && m_previewTextEdit) {
            // Live micro-text preview (bounded line length to prevent hanging on minified files)
            m_previewImgLabel->setVisible(false);
            m_previewTextEdit->setVisible(true);

            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                QString previewContent;
                int linesRead = 0;
                while (!in.atEnd() && linesRead < 100) {
                    previewContent += in.readLine(500) + "\n";
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
                if (isPreviewableImageExt(ext)) {
                    QImageReader reader(filePath);
                    reader.setAutoTransform(true);
                    QSize originalSize = reader.size();
                    if (originalSize.isValid() && originalSize.width() > 0 && originalSize.height() > 0) {
                        reader.setScaledSize(originalSize.scaled(QSize(150, 110), Qt::KeepAspectRatio));
                    } else if (ext == "svg") {
                        reader.setScaledSize(QSize(150, 110));
                    }
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

        QString html = QString("<div style='background-color: %1; border: 1px solid %2; border-radius: 8px; padding: 10px 12px; margin-top: 6px;'>")
            .arg(cardBg, cardBorder);
        html += "<table style='width: 100%; border-collapse: collapse;'>";
        html += makeRow(tr("Type"), typeComment);
        html += makeRow(tr("Size"), formattedBytes);
        html += makeRow(tr("Location"), fi.dir().dirName());
        html += makeRow(tr("Modified"), fi.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        if (fi.birthTime().isValid()) {
            html += makeRow(tr("Created"), fi.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
        } else if (fi.metadataChangeTime().isValid()) {
            html += makeRow(tr("Created"), fi.metadataChangeTime().toString("yyyy-MM-dd HH:mm:ss"));
        }
        html += makeRow(tr("Permissions"), formatPermissions(fi.permissions()), true);

        // Extra image metadata
        if (isPreviewableImageExt(ext)) {
            QImageReader reader(filePath);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                html += makeRow(tr("Resolution"), QString("%1 × %2 px").arg(sz.width()).arg(sz.height()));
            }
        }
        html += "</table></div>";

        m_previewMetaLabel->setText(html);
    }
}

