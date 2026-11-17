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

QImage ThumbnailWorker::renderFileThumbnail(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || fi.isDir()) {
        return QImage();
    }
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
        if (!img.isNull() && (img.width() > targetSize.width() || img.height() > targetSize.height())) {
            img = img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return img;
    } else if (ext == "pdf") {
        QImage img = PluginManager::renderPdfThumbnail(path, 220);
        if (!img.isNull()) {
            return img.scaled(220, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    } else if (isPreviewableTextFile(ext, fi.fileName())) {
        return renderTextThumbnail(path);
    }

    return QImage();
}

void ThumbnailWorker::run()
{
    for (const QString &path : m_filePaths) {
        if (m_isCancelled.load(std::memory_order_relaxed)) break;

        QImage img = renderFileThumbnail(path);
        if (!img.isNull()) {
            emit thumbnailReady(path, img);
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
    return QSize(148, 152);
}

void FolderCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    FolderViewer *fv = qobject_cast<FolderViewer*>(parent());
    if (!fv && option.widget) {
        fv = qobject_cast<FolderViewer*>(option.widget->parentWidget());
    }
    if (fv && !fv->cardHiddenForTransition().isEmpty()) {
        QString itemPath = index.data(Qt::UserRole).toString();
        if (!itemPath.isEmpty() && itemPath == fv->cardHiddenForTransition()) {
            return;
        }
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    bool isHover = bool(option.state & QStyle::State_MouseOver);
    bool isSelected = bool(option.state & QStyle::State_Selected);
    bool isDir = index.data(Qt::UserRole + 1).toBool();

    ThemePreset preset = ThemeManager::currentPreset();

    QColor cardBg = Qt::transparent;
    QColor cardBorder = Qt::transparent;
    QColor textColor;
    QColor subTextColor;

    if (preset == ThemePreset::ModernLight) {
        if (isSelected) {
            cardBg = QColor(0, 122, 255, 22);
            cardBorder = QColor(0, 122, 255, 65);
            textColor = QColor("#0062cc");
            subTextColor = QColor("#48484a");
        } else if (isHover) {
            cardBg = QColor(0, 0, 0, 11);
            cardBorder = Qt::transparent;
            textColor = QColor("#1d1d1f");
            subTextColor = QColor("#6e6e73");
        } else {
            textColor = QColor("#1d1d1f");
            subTextColor = QColor("#86868b");
        }
    } else if (preset == ThemePreset::BreezeLight) {
        if (isSelected) {
            cardBg = QColor(61, 174, 233, 26);
            cardBorder = QColor(61, 174, 233, 70);
            textColor = QColor("#1d7099");
            subTextColor = QColor("#4d5257");
        } else if (isHover) {
            cardBg = QColor(0, 0, 0, 11);
            cardBorder = Qt::transparent;
            textColor = QColor("#232629");
            subTextColor = QColor("#626569");
        } else {
            textColor = QColor("#232629");
            subTextColor = QColor("#7f8c8d");
        }
    } else if (preset == ThemePreset::DarkSlate) {
        if (isSelected) {
            cardBg = QColor(56, 189, 248, 28);
            cardBorder = QColor(56, 189, 248, 65);
            textColor = QColor("#ffffff");
            subTextColor = QColor("#bae6fd");
        } else if (isHover) {
            cardBg = QColor(255, 255, 255, 14);
            cardBorder = Qt::transparent;
            textColor = QColor("#f8fafc");
            subTextColor = QColor("#94a3b8");
        } else {
            textColor = QColor("#e2e8f0");
            subTextColor = QColor("#64748b");
        }
    } else { // BreezeDark
        if (isSelected) {
            cardBg = QColor(61, 174, 233, 30);
            cardBorder = QColor(61, 174, 233, 70);
            textColor = QColor("#ffffff");
            subTextColor = QColor("#bae6fd");
        } else if (isHover) {
            cardBg = QColor(255, 255, 255, 14);
            cardBorder = Qt::transparent;
            textColor = QColor("#fcfcfc");
            subTextColor = QColor("#a0a6ad");
        } else {
            textColor = QColor("#d8dce0");
            subTextColor = QColor("#64748b");
        }
    }

    // 1. Draw Soft Squircle Highlight ONLY on Hover or Selection (unselected items float cleanly)
    if (isSelected || isHover) {
        if (cardBorder.alpha() > 0) {
            painter->setPen(QPen(cardBorder, 1.0));
        } else {
            painter->setPen(Qt::NoPen);
        }
        painter->setBrush(QBrush(cardBg));
        painter->drawRoundedRect(QRectF(rect), 16, 16);
    }

    // 2. Draw Thumbnail or Icon (centered in upper visual zone)
    QRect thumbArea(rect.left() + 8, rect.top() + 8, rect.width() - 16, 94);
    QVariant thumbVar = index.data(Qt::UserRole + 2); // Custom thumbnail pixmap

    if (thumbVar.isValid() && !thumbVar.value<QPixmap>().isNull()) {
        QPixmap pix = thumbVar.value<QPixmap>();
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

        QRect imgRect(x, y, scaled.width(), scaled.height());

        // Soft drop shadow behind thumbnail
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 45));
        painter->drawRoundedRect(QRectF(imgRect).adjusted(1, 3, 1, 3), 10, 10);

        // Draw thumbnail with smooth 10px rounded clip
        painter->save();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(imgRect), 10, 10);
        painter->setClipPath(clip);
        painter->drawPixmap(x, y, scaled);
        painter->restore();

        // Subtle inner rim around thumbnail
        painter->setPen(QPen(QColor(255, 255, 255, 26), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(imgRect), 10, 10);
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

    // 3. Draw Primary Label (File / Folder Name - single line with middle elision)
    int titleTop = isDir ? (rect.top() + 106) : (rect.top() + 101);
    QRect titleArea(rect.left() + 8, titleTop, rect.width() - 16, 20);
    QFont titleFont = option.font;
    titleFont.setPointSizeF(9.2);
    titleFont.setWeight(isSelected ? QFont::DemiBold : QFont::Medium);
    painter->setFont(titleFont);
    painter->setPen(textColor);

    QString text = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm(titleFont);
    QString elidedTitle = fm.elidedText(text, Qt::ElideMiddle, titleArea.width());
    painter->drawText(titleArea, Qt::AlignHCenter | Qt::AlignVCenter, elidedTitle);

    // 4. For files, draw subtle secondary metadata (file size) below the filename
    if (!isDir) {
        QString subText = index.data(Qt::UserRole + 3).toString();
        if (!subText.isEmpty()) {
            QRect subArea(rect.left() + 8, rect.top() + 120, rect.width() - 16, 16);
            QFont subFont = option.font;
            subFont.setPointSizeF(7.8);
            subFont.setWeight(QFont::Normal);
            painter->setFont(subFont);
            painter->setPen(subTextColor);
            painter->drawText(subArea, Qt::AlignHCenter | Qt::AlignVCenter, subText);
        }
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
    , m_metaScrollArea(nullptr)
    , m_previewTitleLabel(nullptr)
    , m_previewMetaLabel(nullptr)
    , m_btnOpenSelected(nullptr)
    , m_worker(nullptr)
    , m_itemCount(0)
    , m_totalSize(0)
{
    QSettings s("Bubez", "BubezView");
    if (!s.contains("Gallery/ShowFolders")) {
        QSettings legacy("BubezView", "bubezview");
        if (legacy.contains("Gallery/ShowFolders")) {
            s.setValue("Gallery/ShowFolders", legacy.value("Gallery/ShowFolders"));
        }
    }
    m_showFolders = s.value("Gallery/ShowFolders", false).toBool();
    setupUi();

    if (m_listWidget && m_listWidget->verticalScrollBar()) {
        connect(m_listWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int val) {
            if (!m_currentFolder.isEmpty()) {
                m_folderScrollPositions[m_currentFolder] = val;
            }
        });
    }
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

    // Top Navigation & Filter Bar (seamless canvas header with pill controls)
    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(44);

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(18, 8, 18, 4);
    topLayout->setSpacing(10);

    m_btnUp = new QPushButton(tr("‹ Up"), m_topBar);
    m_btnUp->setCursor(Qt::PointingHandCursor);
    m_btnUp->setToolTip(tr("Go to parent folder (Backspace / Alt+Up)"));
    m_btnUp->setFixedHeight(28);
    m_btnUp->setEnabled(false);
    connect(m_btnUp, &QPushButton::clicked, this, &FolderViewer::goUpDirectory);
    topLayout->addWidget(m_btnUp, 0, Qt::AlignVCenter);

    topLayout->addSpacing(4);

    // Pill badge is hidden: folder title is already presented cleanly in MinimalHeaderBar
    m_pillBadge = new QFrame(m_topBar);
    m_pillBadge->setObjectName("FolderPillBadge");
    m_pillBadge->setFixedHeight(26);
    m_pillBadge->setVisible(false);

    QHBoxLayout *pillLayout = new QHBoxLayout(m_pillBadge);
    pillLayout->setContentsMargins(10, 0, 10, 0);
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

    // Quick Filter Search Pill
    m_filterEdit = new QLineEdit(m_topBar);
    m_filterEdit->setPlaceholderText(tr("Filter files..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setFixedHeight(28);
    m_filterEdit->setFixedWidth(210);
    m_filterEdit->addAction(
        VectorIcons::createUiIcon(VectorIcons::UiIcon::Search, QColor("#64748b"), 15),
        QLineEdit::LeadingPosition
    );
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FolderViewer::onFilterTextChanged);
    topLayout->addWidget(m_filterEdit, 0, Qt::AlignVCenter);

    m_btnToggleInspector = new QPushButton(tr("Details"), m_topBar);
    m_btnToggleInspector->setCheckable(true);
    m_btnToggleInspector->setChecked(false);
    m_btnToggleInspector->setCursor(Qt::PointingHandCursor);
    m_btnToggleInspector->setToolTip(tr("Show or hide details panel (I)"));
    m_btnToggleInspector->setFixedHeight(28);
    connect(m_btnToggleInspector, &QPushButton::clicked, this, &FolderViewer::toggleInspector);
    topLayout->addWidget(m_btnToggleInspector, 0, Qt::AlignVCenter);

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
        if (ThemeManager::isDark(preset)) {
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
        } else {
            menu.setStyleSheet(
                "QMenu {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   border: 1px solid #d2d2d7;"
                "   border-radius: 6px;"
                "   padding: 4px 0px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px 6px 10px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #007aff;"
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
    m_bookmarksWidget->setVisible(false);

    // Center Column: Card Grid List Widget with Soft Floating Squircle Items
    m_listWidget = new FolderListWidget(this);
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setMovement(QListWidget::Static);
    m_listWidget->setSpacing(12);
    m_listWidget->setGridSize(QSize(148, 152));
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

    // Preview Card Container (hidden to avoid duplicating grid thumbnails and wasting inspector space)
    m_previewCard = new QFrame(m_previewPane);
    m_previewCard->setFixedHeight(140);
    m_previewCard->hide();
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

    // Scrollable area for file metadata and properties (allows smooth mouse wheel scrolling)
    m_metaScrollArea = new QScrollArea(m_previewPane);
    m_metaScrollArea->setWidgetResizable(true);
    m_metaScrollArea->setFrameShape(QFrame::NoFrame);
    m_metaScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_metaScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_metaScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget *metaContainer = new QWidget(m_metaScrollArea);
    metaContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *metaLayout = new QVBoxLayout(metaContainer);
    metaLayout->setContentsMargins(0, 4, 2, 4);
    metaLayout->setSpacing(8);

    // Item Title
    m_previewTitleLabel = new QLabel(tr("Select an item"), metaContainer);
    m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    m_previewTitleLabel->setWordWrap(true);
    m_previewTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_previewTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    metaLayout->addWidget(m_previewTitleLabel);

    // Metadata Card
    m_previewMetaLabel = new QLabel(metaContainer);
    m_previewMetaLabel->setStyleSheet("font-size: 11px; line-height: 1.4;");
    m_previewMetaLabel->setWordWrap(true);
    m_previewMetaLabel->setTextFormat(Qt::RichText);
    m_previewMetaLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_previewMetaLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    metaLayout->addWidget(m_previewMetaLabel);

    metaLayout->addStretch(1);
    m_metaScrollArea->setWidget(metaContainer);

    prevLayout->addWidget(m_metaScrollArea, 1);

    // Open Action Button at bottom of inspector
    m_btnOpenSelected = new QPushButton(tr("Open"), m_previewPane);
    m_btnOpenSelected->setCursor(Qt::PointingHandCursor);
    m_btnOpenSelected->setFixedHeight(32);
    connect(m_btnOpenSelected, &QPushButton::clicked, this, &FolderViewer::openSelectedItem);
    prevLayout->addWidget(m_btnOpenSelected);

    contentLayout->addWidget(m_previewPane);
    m_previewPane->setVisible(false);

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

namespace {

QListWidgetItem *lookupItemByPath(const QHash<QString, QListWidgetItem*> &pathToItem, const QString &filePath)
{
    if (filePath.isEmpty()) return nullptr;
    if (QListWidgetItem *it = pathToItem.value(filePath, nullptr)) return it;
    if (QListWidgetItem *it = pathToItem.value(QFileInfo(filePath).absoluteFilePath(), nullptr)) return it;
    return pathToItem.value(QFileInfo(filePath).canonicalFilePath(), nullptr);
}

} // namespace

QRect FolderViewer::cardRectForPath(const QString &filePath) const
{
    if (m_listWidget) {
        QListWidgetItem *item = lookupItemByPath(m_pathToItem, filePath);
        if (!item && m_listWidget->count() > 0) {
            item = m_listWidget->currentItem();
        }
        if (item) {
            QRect r = m_listWidget->visualItemRect(item);
            if (r.isValid() && r.width() > 0 && r.height() > 0 && m_listWidget->viewport()) {
                QPoint localPt = m_listWidget->viewport()->mapTo(const_cast<FolderViewer*>(this), r.topLeft());
                return QRect(localPt, r.size());
            }
        }
    }
    const int cw = 148;
    const int ch = 152;
    return QRect((width() - cw) / 2, (height() - ch) / 2, cw, ch);
}

QRect FolderViewer::thumbnailRectForPath(const QString &filePath) const
{
    QRect optionRect = cardRectForPath(filePath);
    QRect cardRect = optionRect.adjusted(4, 4, -4, -4);
    QRect thumbArea(cardRect.left() + 8, cardRect.top() + 8, cardRect.width() - 16, 94);

    if (m_listWidget) {
        QListWidgetItem *item = lookupItemByPath(m_pathToItem, filePath);
        if (!item && m_listWidget->count() > 0) {
            item = m_listWidget->currentItem();
        }
        if (item) {
            QVariant thumbVar = item->data(Qt::UserRole + 2);
            if (thumbVar.isValid() && !thumbVar.value<QPixmap>().isNull()) {
                QPixmap pix = thumbVar.value<QPixmap>();
                QSize scaledSize;
                if (pix.width() <= thumbArea.width() && pix.height() <= thumbArea.height()) {
                    scaledSize = pix.size();
                } else {
                    QSize bounded(qMin(pix.width(), thumbArea.width()), qMin(pix.height(), thumbArea.height()));
                    scaledSize = pix.size().scaled(bounded, Qt::KeepAspectRatio);
                }
                int x = thumbArea.left() + (thumbArea.width() - scaledSize.width()) / 2;
                int y = thumbArea.top() + (thumbArea.height() - scaledSize.height()) / 2;
                return QRect(x, y, scaledSize.width(), scaledSize.height());
            } else {
                const int iconSize = 76;
                int x = thumbArea.left() + (thumbArea.width() - iconSize) / 2;
                int y = thumbArea.top() + (thumbArea.height() - iconSize) / 2;
                return QRect(x, y, iconSize, iconSize);
            }
        }
    }
    return thumbArea;
}

void FolderViewer::ensureThumbnailForPath(const QString &filePath)
{
    if (!m_listWidget || filePath.isEmpty()) return;
    QListWidgetItem *item = lookupItemByPath(m_pathToItem, filePath);
    if (!item) return;

    QVariant existing = item->data(Qt::UserRole + 2);
    if (existing.isValid() && !existing.value<QPixmap>().isNull()) {
        return;
    }

    QString targetPath = item->data(Qt::UserRole).toString();
    QImage img = ThumbnailWorker::renderFileThumbnail(targetPath);
    if (!img.isNull()) {
        onThumbnailReady(targetPath, img);
    }
}

void FolderViewer::setCardHiddenForTransition(const QString &filePath, bool hidden)
{
    m_cardHiddenForTransition = hidden ? filePath : QString();
    if (m_listWidget && m_listWidget->viewport()) {
        m_listWidget->viewport()->update();
    }
}

void FolderViewer::selectItemByPath(const QString &filePath, bool ensureVisible)
{
    if (!m_listWidget || filePath.isEmpty()) return;

    QListWidgetItem *item = lookupItemByPath(m_pathToItem, filePath);

    if (item) {
        m_listWidget->blockSignals(true);
        m_listWidget->setCurrentItem(item);
        m_listWidget->blockSignals(false);

        if (ensureVisible && m_listWidget->viewport()) {
            QRect r = m_listWidget->visualItemRect(item);
            QRect viewRect = m_listWidget->viewport()->rect();
            if (!viewRect.contains(r)) {
                m_listWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
        }
        if (m_previewPane && m_previewPane->isVisible()) {
            updateInspectorPane(item->data(Qt::UserRole).toString());
        }
    }
}

void FolderViewer::setShowFolders(bool show)
{
    if (m_showFolders == show) return;
    m_showFolders = show;
    QSettings s("Bubez", "BubezView");
    s.setValue("Gallery/ShowFolders", show);
    if (!m_currentFolder.isEmpty()) {
        loadFolder(m_currentFolder, true);
    }
}

bool FolderViewer::loadFolder(const QString &folderPath, bool forceReload)
{
    if (folderPath.isEmpty()) return false;

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    QString canonical = dir.canonicalPath();
    if (canonical.isEmpty()) {
        canonical = dir.absolutePath();
    }

    // If folder is already loaded and no forced reload requested, keep loaded state and preserve scroll
    if (!forceReload && m_currentFolder == canonical && m_listWidget && m_listWidget->count() > 0) {
        if (m_folderScrollPositions.contains(canonical) && m_listWidget->verticalScrollBar()) {
            m_listWidget->verticalScrollBar()->setValue(m_folderScrollPositions.value(canonical));
        }
        return true;
    }

    // Save scroll position for the current folder before wiping
    if (!m_currentFolder.isEmpty() && m_listWidget && m_listWidget->verticalScrollBar()) {
        m_folderScrollPositions[m_currentFolder] = m_listWidget->verticalScrollBar()->value();
    }

    clear();

    m_currentFolder = canonical;

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
            if (m_showFolders) {
                subdirs.append(info);
            }
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

    if (m_showFolders) {
        std::sort(subdirs.begin(), subdirs.end(), compareFileInfos);
    }
    std::sort(files.begin(), files.end(), compareFileInfos);

    m_itemCount = (m_showFolders ? subdirs.size() : 0) + files.size();

    // 1. Add Subfolders (only if enabled)
    if (m_showFolders) {
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

    // Launch async thumbnail worker guarded by current folder load generation
    if (!asyncThumbnailCandidates.isEmpty()) {
        ThumbnailWorker *worker = new ThumbnailWorker(asyncThumbnailCandidates);
        m_worker = worker;
        const quint64 gen = m_loadGeneration;
        connect(worker, &ThumbnailWorker::thumbnailReady, this,
                [this, gen](const QString &path, const QImage &img) {
                    if (gen == m_loadGeneration) {
                        onThumbnailReady(path, img);
                    }
                });
        connect(worker, &ThumbnailWorker::finished, worker, &QObject::deleteLater);
        worker->start();
    }

    // Synchronously layout items so scrollbar ranges and visual rects are immediate
    m_listWidget->doItemsLayout();

    emit folderLoaded(m_currentFolder, m_itemCount, m_totalSize);

    // Restore saved scroll position for this folder, or select row 0 if newly visited
    if (m_folderScrollPositions.contains(m_currentFolder) && m_listWidget->verticalScrollBar()) {
        m_listWidget->verticalScrollBar()->setValue(m_folderScrollPositions.value(m_currentFolder));
    } else if (m_listWidget->count() > 0) {
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
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
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
    ++m_loadGeneration;
    m_cardHiddenForTransition.clear();
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

    if (!ThemeManager::isDark(preset)) {
        QString winBg = (preset == ThemePreset::ModernLight) ? "#e2e5ea" : "#eff0f1";
        QString cardBg = (preset == ThemePreset::ModernLight) ? "#f1f3f6" : "#fcfcfc";
        QString borderCol = (preset == ThemePreset::ModernLight) ? "#c8cbd0" : "#bdc3c7";
        QString textCol = (preset == ThemePreset::ModernLight) ? "#1e293b" : "#232629";
        QString subTextCol = (preset == ThemePreset::ModernLight) ? "#64748b" : "#626569";
        QString accentCol = (preset == ThemePreset::ModernLight) ? "#0284c7" : "#3daee9";
        QString hoverCol = (preset == ThemePreset::ModernLight) ? "#d8dce2" : "#d8dbde";

        // Seamless sub-bar matching canvas background (no double horizontal divider line)
        m_topBar->setStyleSheet(QString("background-color: %1; border: none;").arg(winBg));
        m_pillBadge->setStyleSheet(
            QString("#FolderPillBadge {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 13px;"
            "}").arg(cardBg, borderCol)
        );
        m_pillText->setStyleSheet(QString("border: none; background: transparent; color: %1; font-size: 11px; font-weight: 500;").arg(textCol));

        m_btnUp->setStyleSheet(
            QString("QPushButton {"
            "   background-color: rgba(0, 0, 0, 0.05);"
            "   color: %1;"
            "   border: none;"
            "   border-radius: 14px;"
            "   padding: 0px 14px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "   background-color: %2;"
            "   color: #ffffff;"
            "}"
            "QPushButton:disabled {"
            "   background-color: rgba(0, 0, 0, 0.025);"
            "   color: rgba(0, 0, 0, 0.25);"
            "}").arg(textCol, accentCol)
        );

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                QString("QLineEdit {"
                "   background-color: %1;"
                "   color: %2;"
                "   border: 1px solid rgba(0, 0, 0, 0.10);"
                "   border-radius: 14px;"
                "   padding: 2px 12px 2px 6px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:focus {"
                "   border: 1px solid %3;"
                "}").arg(cardBg, textCol, accentCol)
            );
        }

        m_listWidget->setStyleSheet(
            QString("QListWidget {"
            "   background-color: %1;"
            "   border: none;"
            "   padding: 6px 18px 16px 18px;"
            "}\n").arg(winBg) + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                QString("QListWidget {"
                "   background-color: %1;"
                "   border-right: 1px solid %2;"
                "   padding: 8px 0px;"
                "   color: %3;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 6px 12px;"
                "   border-radius: 10px;"
                "   margin: 1px 8px;"
                "}"
                "QListWidget::item:hover:!disabled {"
                "   background-color: %4;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: %5;"
                "   color: #ffffff;"
                "   font-weight: 600;"
                "}"
                "QListWidget::item:disabled {"
                "   padding-top: 8px;"
                "   padding-bottom: 2px;"
                "   font-weight: bold;"
                "}").arg(winBg, borderCol, textCol, hoverCol, accentCol)
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                QString("QWidget {"
                "   background-color: %1;"
                "   border-left: 1px solid %2;"
                "}").arg(winBg, borderCol)
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                QString("QFrame {"
                "   background-color: %1;"
                "   border: 1px solid %2;"
                "   border-radius: 14px;"
                "}").arg(cardBg, borderCol)
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet("background-color: transparent; border: none; padding: 2px;");
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.8px;").arg(accentCol));
        }
        if (m_btnCloseInspector) {
            m_btnCloseInspector->setStyleSheet(QString("QPushButton { background-color: transparent; border: none; font-size: 11px; border-radius: 11px; } QPushButton:hover { background-color: %1; }").arg(hoverCol));
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet(QString("font-weight: bold; font-size: 13px; color: %1;").arg(textCol));
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet(QString("color: %1; font-size: 11px; line-height: 1.4;").arg(subTextCol));
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                QString("QPlainTextEdit {"
                "   background-color: %1;"
                "   color: %2;"
                "   border: none;"
                "   padding: 4px;"
                "}").arg(cardBg, textCol)
            );
        }
        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                QString("QPushButton {"
                "   background-color: rgba(0, 0, 0, 0.05);"
                "   color: %1;"
                "   border: none;"
                "   border-radius: 14px;"
                "   padding: 0px 14px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(0, 0, 0, 0.09);"
                "}"
                "QPushButton:checked {"
                "   background-color: %2;"
                "   color: #ffffff;"
                "}").arg(textCol, accentCol)
            );
        }
        if (m_btnOpenSelected) {
            m_btnOpenSelected->setStyleSheet(
                QString("QPushButton {"
                "   background-color: %1;"
                "   color: #ffffff;"
                "   border: none;"
                "   border-radius: 16px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:disabled {"
                "   background-color: %2;"
                "   color: %3;"
                "}").arg(accentCol, cardBg, subTextCol)
            );
        }
    } else if (preset == ThemePreset::DarkSlate) {
        // Seamless top bar matching #181a1f canvas background (eliminates double horizontal divider bar)
        m_topBar->setStyleSheet("background-color: #181a1f; border: none;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   border: 1px solid rgba(255, 255, 255, 0.08);"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #e2e8f0; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(255, 255, 255, 0.06);"
            "   color: #e2e8f0;"
            "   border: none;"
            "   border-radius: 14px;"
            "   padding: 0px 14px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(56, 189, 248, 0.22);"
            "   color: #38bdf8;"
            "}"
            "QPushButton:disabled {"
            "   background-color: rgba(255, 255, 255, 0.025);"
            "   color: rgba(255, 255, 255, 0.22);"
            "}"
        );

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: rgba(255, 255, 255, 0.05);"
                "   color: #f1f5f9;"
                "   border: 1px solid rgba(255, 255, 255, 0.07);"
                "   border-radius: 14px;"
                "   padding: 2px 12px 2px 6px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:hover {"
                "   background-color: rgba(255, 255, 255, 0.07);"
                "   border-color: rgba(255, 255, 255, 0.13);"
                "}"
                "QLineEdit:focus {"
                "   background-color: rgba(15, 23, 42, 0.65);"
                "   border: 1px solid rgba(56, 189, 248, 0.65);"
                "}"
            );
        }

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: rgba(255, 255, 255, 0.06);"
                "   color: #e2e8f0;"
                "   border: none;"
                "   border-radius: 14px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "   padding: 0px 14px;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(255, 255, 255, 0.12);"
                "   color: #ffffff;"
                "}"
                "QPushButton:checked {"
                "   background-color: rgba(56, 189, 248, 0.22);"
                "   color: #38bdf8;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #181a1f;"
            "   border: none;"
            "   padding: 6px 18px 16px 18px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #14161b;"
                "   border-right: 1px solid rgba(255, 255, 255, 0.06);"
                "   padding: 8px 0px;"
                "   color: #94a3b8;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 6px 12px;"
                "   border-radius: 10px;"
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
                "   border-left: 1px solid rgba(255, 255, 255, 0.06);"
                "}"
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: #111317;"
                "   border: 1px solid rgba(255, 255, 255, 0.07);"
                "   border-radius: 14px;"
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
                "   border-radius: 16px;"
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
    } else if (preset == ThemePreset::ZincGray) {
        // Zinc Gray Theme (#36373b base, #585960 pills/buttons)
        m_topBar->setStyleSheet("background-color: #36373b; border: none;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: #585960;"
            "   border: 1px solid rgba(255, 255, 255, 0.22);"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #ffffff; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: #585960;"
            "   color: #ffffff;"
            "   border: 1px solid rgba(255, 255, 255, 0.20);"
            "   border-radius: 14px;"
            "   padding: 0px 14px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "   background-color: #63646c;"
            "}"
            "QPushButton:disabled {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   color: rgba(255, 255, 255, 0.25);"
            "}"
        );

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: #3f4044;"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.20);"
                "   border-radius: 14px;"
                "   padding: 2px 12px 2px 6px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:hover {"
                "   border-color: rgba(255, 255, 255, 0.35);"
                "}"
                "QLineEdit:focus {"
                "   border: 1px solid #727379;"
                "}"
            );
        }

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: #727379;"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.25);"
                "   border-radius: 14px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "   padding: 0px 14px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #82838a;"
                "}"
                "QPushButton:checked {"
                "   background-color: #8a8b92;"
                "   color: #ffffff;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #494a4f;"
            "   border: none;"
            "   padding: 6px 18px 16px 18px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #3f4044;"
                "   border-right: 1px solid rgba(255, 255, 255, 0.12);"
                "   padding: 8px 0px;"
                "   color: #d1d5db;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 6px 12px;"
                "   border-radius: 10px;"
                "   margin: 1px 8px;"
                "}"
                "QListWidget::item:hover:!disabled {"
                "   background-color: rgba(255, 255, 255, 0.10);"
                "   color: #ffffff;"
                "}"
                "QListWidget::item:selected {"
                "   background-color: #727379;"
                "   color: #ffffff;"
                "   font-weight: 600;"
                "}"
                "QListWidget::item:disabled {"
                "   padding-top: 8px;"
                "   padding-bottom: 2px;"
                "   font-weight: bold;"
                "   color: #9ca3af;"
                "}"
            );
        }

        if (m_previewPane) {
            m_previewPane->setStyleSheet(
                "QWidget {"
                "   background-color: #505156;"
                "   border-left: 1px solid rgba(255, 255, 255, 0.14);"
                "}"
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: #3f4044;"
                "   border: 1px solid rgba(255, 255, 255, 0.20);"
                "   border-radius: 14px;"
                "}"
            );
        }

        if (m_previewImgLabel) {
            m_previewImgLabel->setStyleSheet("background-color: transparent; border: none; padding: 2px;");
        }
        if (m_inspectorBadge) {
            m_inspectorBadge->setStyleSheet("color: #727379; font-size: 11px; font-weight: bold; letter-spacing: 0.8px;");
        }
        if (m_btnCloseInspector) {
            m_btnCloseInspector->setStyleSheet("QPushButton { background-color: transparent; border: none; font-size: 11px; border-radius: 11px; } QPushButton:hover { background-color: rgba(255, 255, 255, 0.12); }");
        }
        if (m_previewTitleLabel) {
            m_previewTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #ffffff;");
        }
        if (m_previewMetaLabel) {
            m_previewMetaLabel->setStyleSheet("color: #d1d5db; font-size: 11px; line-height: 1.4;");
        }
        if (m_previewTextEdit) {
            m_previewTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "   background-color: #38393d;"
                "   color: #ffffff;"
                "   border: none;"
                "   padding: 4px;"
                "}"
            );
        }
        if (m_btnOpenSelected) {
            m_btnOpenSelected->setStyleSheet(
                "QPushButton {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.25);"
                "   border-radius: 16px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "   background-color: #82838a;"
                "}"
                "QPushButton:disabled {"
                "   background-color: #38393d;"
                "   color: #6b7280;"
                "}"
            );
        }
    } else { // BreezeDark
        // Seamless top bar matching #17191c canvas background
        m_topBar->setStyleSheet("background-color: #17191c; border: none;");
        m_pillBadge->setStyleSheet(
            "#FolderPillBadge {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   border: 1px solid rgba(255, 255, 255, 0.08);"
            "   border-radius: 13px;"
            "}"
        );
        m_pillText->setStyleSheet("border: none; background: transparent; color: #eff0f1; font-size: 11px; font-weight: 500;");

        m_btnUp->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(255, 255, 255, 0.06);"
            "   color: #cfd2d5;"
            "   border: none;"
            "   border-radius: 14px;"
            "   padding: 0px 14px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(61, 174, 233, 0.24);"
            "   color: #3daee9;"
            "}"
            "QPushButton:disabled {"
            "   background-color: rgba(255, 255, 255, 0.025);"
            "   color: rgba(255, 255, 255, 0.22);"
            "}"
        );

        if (m_filterEdit) {
            m_filterEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: rgba(255, 255, 255, 0.05);"
                "   color: #eff0f1;"
                "   border: 1px solid rgba(255, 255, 255, 0.07);"
                "   border-radius: 14px;"
                "   padding: 2px 12px 2px 6px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:hover {"
                "   background-color: rgba(255, 255, 255, 0.07);"
                "   border-color: rgba(255, 255, 255, 0.13);"
                "}"
                "QLineEdit:focus {"
                "   background-color: rgba(15, 18, 22, 0.7);"
                "   border: 1px solid rgba(61, 174, 233, 0.65);"
                "}"
            );
        }

        if (m_btnToggleInspector) {
            m_btnToggleInspector->setStyleSheet(
                "QPushButton {"
                "   background-color: rgba(255, 255, 255, 0.06);"
                "   color: #cfd2d5;"
                "   border: none;"
                "   border-radius: 14px;"
                "   font-size: 12px;"
                "   font-weight: 600;"
                "   padding: 0px 14px;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(255, 255, 255, 0.12);"
                "   color: #eff0f1;"
                "}"
                "QPushButton:checked {"
                "   background-color: rgba(61, 174, 233, 0.22);"
                "   color: #3daee9;"
                "}"
            );
        }

        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #17191c;"
            "   border: none;"
            "   padding: 6px 18px 16px 18px;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );

        if (m_bookmarksWidget) {
            m_bookmarksWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #151719;"
                "   border-right: 1px solid rgba(255, 255, 255, 0.06);"
                "   padding: 8px 0px;"
                "   color: #a0a6ad;"
                "   font-size: 12px;"
                "}"
                "QListWidget::item {"
                "   padding: 6px 12px;"
                "   border-radius: 10px;"
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
                "   border-left: 1px solid rgba(255, 255, 255, 0.06);"
                "}"
            );
        }

        if (m_previewCard) {
            m_previewCard->setStyleSheet(
                "QFrame {"
                "   background-color: #101214;"
                "   border: 1px solid rgba(255, 255, 255, 0.07);"
                "   border-radius: 14px;"
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
                "   border-radius: 16px;"
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
    QSettings settings("Bubez", "BubezView");
    if (!settings.contains("FolderViewer/CustomBookmarks")) {
        QSettings legacy("BubezView", "BubezView");
        if (legacy.contains("FolderViewer/CustomBookmarks")) {
            return legacy.value("FolderViewer/CustomBookmarks").toStringList();
        }
    }
    return settings.value("FolderViewer/CustomBookmarks").toStringList();
}

void FolderViewer::addCustomBookmark(const QString &folderPath)
{
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
    QString canonical = QDir(folderPath).canonicalPath();
    QStringList bms = customBookmarks();
    if (!bms.contains(canonical)) {
        bms.append(canonical);
        QSettings settings("Bubez", "BubezView");
        settings.setValue("FolderViewer/CustomBookmarks", bms);
        populateBookmarks();
    }
}

void FolderViewer::removeCustomBookmark(const QString &folderPath)
{
    QString canonical = QDir(folderPath).canonicalPath();
    QStringList bms = customBookmarks();
    if (bms.removeAll(canonical) > 0 || bms.removeAll(folderPath) > 0) {
        QSettings settings("Bubez", "BubezView");
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

    bool isDark = ThemeManager::isDarkTheme();

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
                const double mp = (static_cast<double>(sz.width()) * sz.height()) / 1000000.0;
                const QString mpStr = (mp >= 0.1) ? QString("%1 MP").arg(mp, 0, 'f', 1) : QStringLiteral("<0.1 MP");
                html += makeRow(tr("Resolution"), QString("%1 × %2 px (%3)").arg(sz.width()).arg(sz.height()).arg(mpStr));
            }
        }
        html += "</table></div>";

        m_previewMetaLabel->setText(html);
    }
}

