#include "folder_viewer.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include "plugin_manager.h"
#include "core/interfaces/i_folder_studio_plugin.h"
#include "core/services/exif_parser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QListView>
#include <QTimer>
#include <QThreadPool>
#include <QRegularExpression>
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

#ifdef HAVE_POPPLER_QT6
#include <poppler/qt6/poppler-qt6.h>
#endif

#ifdef HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

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
        for (QWidget *w = option.widget->parentWidget(); w && !fv; w = w->parentWidget()) {
            fv = qobject_cast<FolderViewer*>(w);
        }
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

    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());

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
    } else if (preset == ThemePreset::ZincGray) {
        if (isSelected) {
            cardBg = QColor(56, 189, 248, 32);
            cardBorder = QColor(56, 189, 248, 85);
            textColor = QColor("#ffffff");
            subTextColor = QColor("#bae6fd");
        } else if (isHover) {
            cardBg = QColor(255, 255, 255, 16);
            cardBorder = Qt::transparent;
            textColor = QColor("#ffffff");
            subTextColor = QColor("#cbd5e1");
        } else {
            textColor = QColor("#f4f4f5");
            subTextColor = QColor("#a1a1aa");
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
    , m_btnShowFolders(nullptr)
    , m_btnTrash(nullptr)
    , m_pillBadge(nullptr)
    , m_pillIcon(nullptr)
    , m_pillText(nullptr)
    , m_filterEdit(nullptr)
    , m_listWidget(nullptr)
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
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_viewStack = new QStackedWidget(this);
    rootLayout->addWidget(m_viewStack, 1);

    // Page 0: Standard Folder Gallery View
    m_galleryPage = new QWidget(m_viewStack);
    QVBoxLayout *mainLayout = new QVBoxLayout(m_galleryPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top Navigation & Universal Filter Bar (seamless canvas header with pill controls)
    m_topBar = new QWidget(m_galleryPage);
    m_topBar->setFixedHeight(44);

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(18, 8, 18, 4);
    topLayout->setSpacing(8);

    m_btnUp = new QPushButton(tr("↑ Up"), m_topBar);
    m_btnUp->setCursor(Qt::PointingHandCursor);
    m_btnUp->setToolTip(tr("Go to parent folder (Backspace / Alt+Up)"));
    m_btnUp->setFixedHeight(28);
    m_btnUp->setEnabled(false);
    connect(m_btnUp, &QPushButton::clicked, this, &FolderViewer::goUpDirectory);
    topLayout->addWidget(m_btnUp, 0, Qt::AlignVCenter);

    m_btnShowFolders = new QPushButton(tr("📁 Show Folders"), m_topBar);
    m_btnShowFolders->setCheckable(true);
    m_btnShowFolders->setChecked(m_showFolders);
    m_btnShowFolders->setCursor(Qt::PointingHandCursor);
    m_btnShowFolders->setToolTip(tr("Show or hide subfolders in the gallery"));
    m_btnShowFolders->setFixedHeight(28);
    connect(m_btnShowFolders, &QPushButton::toggled, this, &FolderViewer::setShowFolders);
    topLayout->addWidget(m_btnShowFolders, 0, Qt::AlignVCenter);

    m_btnTrash = new QPushButton(tr("🗑️ Trash"), m_topBar);
    m_btnTrash->setCursor(Qt::PointingHandCursor);
    m_btnTrash->setToolTip(tr("Move selected item to trash (Del)"));
    m_btnTrash->setFixedHeight(28);
    m_btnTrash->setEnabled(false);
    connect(m_btnTrash, &QPushButton::clicked, this, &FolderViewer::trashSelectedRequested);
    topLayout->addWidget(m_btnTrash, 0, Qt::AlignVCenter);

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

    // Universal Multi-Format Category Filter
    m_comboCategoryFilter = new QComboBox(m_topBar);
    m_comboCategoryFilter->setView(new QListView(m_comboCategoryFilter));
    m_comboCategoryFilter->addItem(tr("All Types"), 0);
    m_comboCategoryFilter->addItem(tr("Documents (PDF / Office)"), 1);
    m_comboCategoryFilter->addItem(tr("Text && Code"), 2);
    m_comboCategoryFilter->addItem(tr("Images"), 3);
    m_comboCategoryFilter->addItem(tr("Audio && Video"), 4);
    m_comboCategoryFilter->addItem(tr("Archives"), 5);
    m_comboCategoryFilter->setFixedHeight(28);
    m_comboCategoryFilter->setMinimumWidth(148);
    m_comboCategoryFilter->setToolTip(tr("Filter gallery by file category"));
    connect(m_comboCategoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyCombinedFilter(); });
    topLayout->addWidget(m_comboCategoryFilter, 0, Qt::AlignVCenter);

    // Quick Filter Search Pill (supports filename, size expressions like >10MB, and deep content search)
    m_filterEdit = new QLineEdit(m_topBar);
    m_filterEdit->setPlaceholderText(tr("Filter name, >10MB, or content..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setFixedHeight(28);
    m_filterEdit->setFixedWidth(220);
    m_filterEdit->addAction(
        VectorIcons::createUiIcon(VectorIcons::UiIcon::Search, QColor("#64748b"), 15),
        QLineEdit::LeadingPosition
    );
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FolderViewer::onFilterTextChanged);
    topLayout->addWidget(m_filterEdit, 0, Qt::AlignVCenter);

    // Deep Content Filter Toggle (searches inside text, PDFs, archives, and EXIF asynchronously)
    m_btnDeepFilter = new QPushButton(tr("Deep"), m_topBar);
    m_btnDeepFilter->setCheckable(true);
    m_btnDeepFilter->setCursor(Qt::PointingHandCursor);
    m_btnDeepFilter->setFixedHeight(28);
    m_btnDeepFilter->setToolTip(tr("Deep search inside text/code, PDFs, archive contents, and EXIF metadata"));
    connect(m_btnDeepFilter, &QPushButton::toggled, this, [this](bool) {
        onFilterTextChanged(m_filterEdit ? m_filterEdit->text() : QString());
    });
    topLayout->addWidget(m_btnDeepFilter, 0, Qt::AlignVCenter);

    m_deepFilterTimer = new QTimer(this);
    m_deepFilterTimer->setSingleShot(true);
    m_deepFilterTimer->setInterval(180);
    connect(m_deepFilterTimer, &QTimer::timeout, this, &FolderViewer::triggerAsyncDeepFilter);

    mainLayout->addWidget(m_topBar);

    // Full-Width Card Grid List Widget with Soft Floating Squircle Items
    m_listWidget = new FolderListWidget(m_galleryPage);
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

    mainLayout->addWidget(m_listWidget, 1);
    m_viewStack->addWidget(m_galleryPage);

    // Page 1: Modular Folder Studio Container (populated lazily from bubez_folder.so in Edit Mode)
    m_studioContainer = new QWidget(m_viewStack);
    auto *studioLay = new QVBoxLayout(m_studioContainer);
    studioLay->setContentsMargins(0, 0, 0, 0);
    studioLay->setSpacing(0);
    m_viewStack->addWidget(m_studioContainer);
    m_viewStack->setCurrentIndex(0);

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
        if (m_btnTrash) {
            m_btnTrash->setEnabled(true);
        }
    }
}

void FolderViewer::setShowFolders(bool show)
{
    if (m_btnShowFolders && m_btnShowFolders->isChecked() != show) {
        m_btnShowFolders->blockSignals(true);
        m_btnShowFolders->setChecked(show);
        m_btnShowFolders->blockSignals(false);
    }
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
            item->setData(Qt::UserRole + 4, static_cast<int>(FileCategory::Folder));
            item->setData(Qt::UserRole + 5, static_cast<qint64>(0));
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
        item->setData(Qt::UserRole + 4, static_cast<int>(PluginManager::categorizeFile(fInfo.absoluteFilePath())));
        item->setData(Qt::UserRole + 5, fInfo.size());
        item->setToolTip(QString("📄 %1 (%2)\n%3").arg(fInfo.fileName(), formatSize(fInfo.size()), fInfo.absoluteFilePath()));
        m_listWidget->addItem(item);
        m_pathToItem.insert(fInfo.absoluteFilePath(), item);
    }

    // Clear filter inputs if any
    if (m_filterEdit && !m_filterEdit->text().isEmpty()) {
        m_filterEdit->blockSignals(true);
        m_filterEdit->clear();
        m_filterEdit->blockSignals(false);
    }
    if (m_comboCategoryFilter && m_comboCategoryFilter->currentIndex() != 0) {
        m_comboCategoryFilter->blockSignals(true);
        m_comboCategoryFilter->setCurrentIndex(0);
        m_comboCategoryFilter->blockSignals(false);
    }
    m_deepMatchedPaths.clear();
    m_hasDeepResults = false;

    if (m_studioWidget) {
        m_studioWidget->setTargetFolder(m_currentFolder);
    }

    // Update Pill Badge
    QFileInfo currentDirInfo(m_currentFolder);
    QString folderTitle = currentDirInfo.fileName();
    if (folderTitle.isEmpty()) folderTitle = m_currentFolder;

    m_pillText->setText(QString(tr("%1 • %2 Items (%3)"))
        .arg(folderTitle)
        .arg(m_itemCount)
        .arg(formatSize(m_totalSize)));

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
        if (m_btnTrash) m_btnTrash->setEnabled(false);
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
        // Pass the currently visible (filtered) gallery file list so Prev/Next stays within the active filter
        emit filePreviewRequested(targetPath, visibleFilePaths());
    }
}

QStringList FolderViewer::visibleFilePaths() const
{
    QStringList files;
    if (!m_listWidget) return files;
    const int count = m_listWidget->count();
    files.reserve(count);
    for (int i = 0; i < count; ++i) {
        QListWidgetItem *it = m_listWidget->item(i);
        if (!it || it->isHidden()) continue;
        if (it->data(Qt::UserRole + 1).toBool()) continue; // Skip directories
        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty()) {
            files.append(p);
        }
    }
    return files;
}

void FolderViewer::notifyStudioFileRemoved(const QString &filePath)
{
    if (m_studioWidget && !filePath.isEmpty()) {
        m_studioWidget->notifyFileRemoved(filePath);
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
        emit propertiesRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FolderViewer::clear()
{
    ++m_loadGeneration;
    m_deepFilterGeneration.fetch_add(1, std::memory_order_relaxed);
    if (m_deepFilterTimer) m_deepFilterTimer->stop();
    m_deepMatchedPaths.clear();
    m_hasDeepResults = false;
    m_cardHiddenForTransition.clear();
    if (m_studioWidget) {
        m_studioWidget->cancelActiveOperations();
    }
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
    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    QString canvasBg;
    QString pillBg;
    QString pillBorder;
    QString pillText;
    QString btnBg;
    QString btnText;
    QString btnBorder;
    QString btnHoverBg;
    QString btnHoverText;
    QString btnCheckedBg;
    QString btnCheckedText;
    QString btnDisabledBg;
    QString btnDisabledText;
    QString filterBg;
    QString filterText;
    QString filterBorder;
    QString filterFocusBg;
    QString filterFocusBorder;

    if (!ThemeManager::isDark(preset)) {
        const bool modern = (preset == ThemePreset::ModernLight);
        canvasBg = modern ? "#e2e5ea" : "#eff0f1";
        pillBg = modern ? "#f1f3f6" : "#fcfcfc";
        pillBorder = modern ? "#c8cbd0" : "#bdc3c7";
        pillText = modern ? "#1e293b" : "#232629";
        btnBg = "rgba(0, 0, 0, 0.05)";
        btnText = pillText;
        btnBorder = "none";
        btnHoverBg = modern ? "#0284c7" : "#3daee9";
        btnHoverText = "#ffffff";
        btnCheckedBg = btnHoverBg;
        btnCheckedText = "#ffffff";
        btnDisabledBg = "rgba(0, 0, 0, 0.025)";
        btnDisabledText = "rgba(0, 0, 0, 0.25)";
        filterBg = pillBg;
        filterText = pillText;
        filterBorder = "rgba(0, 0, 0, 0.10)";
        filterFocusBg = "#ffffff";
        filterFocusBorder = btnHoverBg;
    } else if (preset == ThemePreset::ZincGray) {
        canvasBg = "#494a4f";
        pillBg = "#585960";
        pillBorder = "rgba(255, 255, 255, 0.22)";
        pillText = "#ffffff";
        btnBg = "#585960";
        btnText = "#ffffff";
        btnBorder = "1px solid rgba(255, 255, 255, 0.20)";
        btnHoverBg = "#63646c";
        btnHoverText = "#ffffff";
        btnCheckedBg = "#0284c7";
        btnCheckedText = "#ffffff";
        btnDisabledBg = "rgba(255, 255, 255, 0.05)";
        btnDisabledText = "rgba(255, 255, 255, 0.25)";
        filterBg = "#3f4044";
        filterText = "#ffffff";
        filterBorder = "rgba(255, 255, 255, 0.20)";
        filterFocusBg = "#36373b";
        filterFocusBorder = "#38bdf8";
    } else if (preset == ThemePreset::DarkSlate) {
        canvasBg = "#181a1f";
        pillBg = "rgba(255, 255, 255, 0.05)";
        pillBorder = "rgba(255, 255, 255, 0.08)";
        pillText = "#e2e8f0";
        btnBg = "rgba(255, 255, 255, 0.06)";
        btnText = "#e2e8f0";
        btnBorder = "none";
        btnHoverBg = "rgba(56, 189, 248, 0.22)";
        btnHoverText = "#38bdf8";
        btnCheckedBg = "rgba(56, 189, 248, 0.24)";
        btnCheckedText = "#38bdf8";
        btnDisabledBg = "rgba(255, 255, 255, 0.025)";
        btnDisabledText = "rgba(255, 255, 255, 0.22)";
        filterBg = "rgba(255, 255, 255, 0.05)";
        filterText = "#f1f5f9";
        filterBorder = "rgba(255, 255, 255, 0.07)";
        filterFocusBg = "rgba(15, 23, 42, 0.65)";
        filterFocusBorder = "rgba(56, 189, 248, 0.65)";
    } else { // BreezeDark
        canvasBg = "#17191c";
        pillBg = "rgba(255, 255, 255, 0.05)";
        pillBorder = "rgba(255, 255, 255, 0.08)";
        pillText = "#eff0f1";
        btnBg = "rgba(255, 255, 255, 0.06)";
        btnText = "#cfd2d5";
        btnBorder = "none";
        btnHoverBg = "rgba(61, 174, 233, 0.24)";
        btnHoverText = "#3daee9";
        btnCheckedBg = "rgba(61, 174, 233, 0.22)";
        btnCheckedText = "#3daee9";
        btnDisabledBg = "rgba(255, 255, 255, 0.025)";
        btnDisabledText = "rgba(255, 255, 255, 0.22)";
        filterBg = "rgba(255, 255, 255, 0.05)";
        filterText = "#eff0f1";
        filterBorder = "rgba(255, 255, 255, 0.07)";
        filterFocusBg = "rgba(15, 18, 22, 0.7)";
        filterFocusBorder = "rgba(61, 174, 233, 0.65)";
    }

    const QString topBarBg = (preset == ThemePreset::ZincGray) ? QStringLiteral("#36373b") : canvasBg;
    m_topBar->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(topBarBg));
    m_pillBadge->setStyleSheet(QStringLiteral(
        "#FolderPillBadge { background-color: %1; border: 1px solid %2; border-radius: 13px; }")
        .arg(pillBg, pillBorder));
    m_pillText->setStyleSheet(QStringLiteral(
        "border: none; background: transparent; color: %1; font-size: 11px; font-weight: 500;").arg(pillText));

    const QString pillBtnStyle = QStringLiteral(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: %3;"
        "   border-radius: 14px;"
        "   padding: 0px 13px;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   color: %5;"
        "}"
        "QPushButton:checked {"
        "   background-color: %6;"
        "   color: %7;"
        "}"
        "QPushButton:disabled {"
        "   background-color: %8;"
        "   color: %9;"
        "}")
        .arg(btnBg, btnText, btnBorder, btnHoverBg, btnHoverText,
             btnCheckedBg, btnCheckedText, btnDisabledBg, btnDisabledText);

    if (m_btnUp) m_btnUp->setStyleSheet(pillBtnStyle);
    if (m_btnShowFolders) m_btnShowFolders->setStyleSheet(pillBtnStyle);
    if (m_btnDeepFilter) m_btnDeepFilter->setStyleSheet(pillBtnStyle);
    if (m_comboCategoryFilter) {
        m_comboCategoryFilter->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   border-radius: 14px;"
            "   padding: 2px 12px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "}"
            "QComboBox::drop-down { border: none; width: 22px; }"
            "QComboBox QAbstractItemView {"
            "   background-color: %4;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   selection-background-color: #0284c7;"
            "   selection-color: #ffffff;"
            "   outline: none;"
            "}").arg(filterBg, filterText, filterBorder, filterFocusBg));
    }
    if (m_btnTrash) {
        m_btnTrash->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: %3;"
            "   border-radius: 14px;"
            "   padding: 0px 13px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(239, 68, 68, 0.22);"
            "   color: #ef4444;"
            "}"
            "QPushButton:disabled {"
            "   background-color: %4;"
            "   color: %5;"
            "}").arg(btnBg, btnText, btnBorder, btnDisabledBg, btnDisabledText));
    }

    if (m_filterEdit) {
        m_filterEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   border-radius: 14px;"
            "   padding: 2px 12px 2px 6px;"
            "   font-size: 12px;"
            "}"
            "QLineEdit:focus {"
            "   background-color: %4;"
            "   border: 1px solid %5;"
            "}").arg(filterBg, filterText, filterBorder, filterFocusBg, filterFocusBorder));
    }

    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "   background-color: %1;"
        "   border: none;"
        "   padding: 6px 18px 16px 18px;"
        "}\n").arg(canvasBg) + ThemeManager::scrollBarStyleSheet(preset));

    if (m_studioContainer) {
        m_studioContainer->setStyleSheet(QStringLiteral("background-color: %1;").arg(canvasBg));
    }
    if (m_lblStudioFallback) {
        m_lblStudioFallback->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; padding: 32px;").arg(pillText));
    }
    if (m_studioWidget) {
        m_studioWidget->updateTheme();
    }

    m_listWidget->viewport()->update();
}

void FolderViewer::onItemSelectionChanged()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur) {
        if (m_btnTrash) m_btnTrash->setEnabled(false);
        emit selectionChanged(QString());
        return;
    }
    const QString path = cur->data(Qt::UserRole).toString();
    if (m_btnTrash) m_btnTrash->setEnabled(!path.isEmpty());
    emit selectionChanged(path);
}

namespace {

// Parses optional size expressions such as ">10MB", "<500KB", ">=1GB", "<=200B".
bool parseSizeFilterExpr(const QString &query, QString &opOut, qint64 &bytesOut)
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*(>=|<=|>|<)\\s*(\\d+(?:\\.\\d+)?)\\s*(b|kb|mb|gb)?\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(query);
    if (!m.hasMatch()) return false;
    opOut = m.captured(1);
    const double val = m.captured(2).toDouble();
    const QString unit = m.captured(3).toLower();
    double mult = 1.0;
    if (unit == QLatin1String("kb")) mult = 1024.0;
    else if (unit == QLatin1String("mb")) mult = 1024.0 * 1024.0;
    else if (unit == QLatin1String("gb")) mult = 1024.0 * 1024.0 * 1024.0;
    bytesOut = static_cast<qint64>(val * mult);
    return true;
}

bool matchesCategoryFilter(int filterMode, FileCategory cat, bool isDir)
{
    if (filterMode == 0) return true; // All Types
    if (isDir) return false;          // Hide folders when filtering by a specific file category
    switch (filterMode) {
    case 1: return (cat == FileCategory::Pdf || cat == FileCategory::Office);
    case 2: return (cat == FileCategory::Text);
    case 3: return (cat == FileCategory::Image);
    case 4: return (cat == FileCategory::Media);
    case 5: return (cat == FileCategory::Archive);
    default: return true;
    }
}

bool deepContentMatchesFile(const QString &path, FileCategory cat, const QString &needleLower)
{
    if (cat == FileCategory::Text) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QString chunk = QString::fromUtf8(f.read(128 * 1024)).toLower();
            if (chunk.contains(needleLower)) return true;
        }
        return false;
    }
#ifdef HAVE_POPPLER_QT6
    if (cat == FileCategory::Pdf) {
        std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
        if (doc && !doc->isLocked()) {
            const QString meta = (doc->info(QStringLiteral("Title")) + QLatin1Char(' ')
                                + doc->info(QStringLiteral("Author")) + QLatin1Char(' ')
                                + doc->info(QStringLiteral("Subject"))).toLower();
            if (meta.contains(needleLower)) return true;
            const int maxPages = qMin(2, doc->numPages());
            for (int p = 0; p < maxPages; ++p) {
                std::unique_ptr<Poppler::Page> page(doc->page(p));
                if (page && page->text(QRectF()).toLower().contains(needleLower)) {
                    return true;
                }
            }
        }
        return false;
    }
#endif
#ifdef HAVE_LIBARCHIVE
    if (cat == FileCategory::Archive) {
        struct archive *a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        bool matched = false;
        if (archive_read_open_filename(a, QFile::encodeName(path).constData(), 16384) == ARCHIVE_OK) {
            struct archive_entry *entry = nullptr;
            int inspected = 0;
            while (inspected < 400 && archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                ++inspected;
                const char *p = archive_entry_pathname(entry);
                if (p && QString::fromUtf8(p).toLower().contains(needleLower)) {
                    matched = true;
                    break;
                }
                archive_read_data_skip(a);
            }
        }
        archive_read_free(a);
        return matched;
    }
#endif
    if (cat == FileCategory::Image) {
        const ExifData exif = ExifParser::parseFile(path);
        if (exif.hasExif) {
            const QString blob = (exif.make + QLatin1Char(' ')
                                + exif.model + QLatin1Char(' ')
                                + exif.lensModel + QLatin1Char(' ')
                                + exif.software + QLatin1Char(' ')
                                + exif.dateTimeOriginal + QLatin1Char(' ')
                                + exif.fNumber + QLatin1Char(' ')
                                + (exif.isoSpeed > 0 ? QStringLiteral("iso %1").arg(exif.isoSpeed) : QString())
                                + QLatin1Char(' ')
                                + (exif.gps.hasGps ? exif.gps.formattedDecimal() : QString())).toLower();
            if (blob.contains(needleLower)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

void FolderViewer::onFilterTextChanged(const QString & /*text*/)
{
    m_deepFilterGeneration.fetch_add(1, std::memory_order_relaxed);
    m_deepMatchedPaths.clear();
    m_hasDeepResults = false;
    applyCombinedFilter();

    const QString query = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    QString dummyOp;
    qint64 dummyBytes = 0;
    const bool isSizeExpr = parseSizeFilterExpr(query, dummyOp, dummyBytes);

    if (m_btnDeepFilter && m_btnDeepFilter->isChecked() && !query.isEmpty() && !isSizeExpr) {
        if (m_deepFilterTimer) m_deepFilterTimer->start();
    } else if (m_deepFilterTimer) {
        m_deepFilterTimer->stop();
    }
}

void FolderViewer::applyCombinedFilter()
{
    const QString query = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    const int categoryMode = m_comboCategoryFilter ? m_comboCategoryFilter->currentData().toInt() : 0;

    QString sizeOp;
    qint64 sizeTarget = 0;
    const bool isSizeExpr = parseSizeFilterExpr(query, sizeOp, sizeTarget);

    QListWidgetItem *firstVisible = nullptr;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!item) continue;

        const bool isDir = item->data(Qt::UserRole + 1).toBool();
        const FileCategory cat = static_cast<FileCategory>(item->data(Qt::UserRole + 4).toInt());
        const qint64 fileSize = item->data(Qt::UserRole + 5).toLongLong();
        const QString path = item->data(Qt::UserRole).toString();

        bool visible = matchesCategoryFilter(categoryMode, cat, isDir);
        if (visible && !query.isEmpty()) {
            if (isSizeExpr) {
                if (isDir) {
                    visible = false;
                } else if (sizeOp == QLatin1String(">"))  visible = (fileSize > sizeTarget);
                else if (sizeOp == QLatin1String(">="))   visible = (fileSize >= sizeTarget);
                else if (sizeOp == QLatin1String("<"))    visible = (fileSize < sizeTarget);
                else if (sizeOp == QLatin1String("<="))   visible = (fileSize <= sizeTarget);
            } else {
                const bool nameMatch = item->text().contains(query, Qt::CaseInsensitive);
                const bool deepMatch = (m_hasDeepResults && m_deepMatchedPaths.contains(path));
                visible = (nameMatch || deepMatch);
            }
        }

        item->setHidden(!visible);
        if (visible && !firstVisible) {
            firstVisible = item;
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
            if (m_btnTrash) m_btnTrash->setEnabled(false);
            emit selectionChanged(QString());
        }
    }
}

void FolderViewer::triggerAsyncDeepFilter()
{
    if (!m_btnDeepFilter || !m_btnDeepFilter->isChecked() || !m_filterEdit) return;
    const QString query = m_filterEdit->text().trimmed();
    if (query.isEmpty()) return;

    const quint64 gen = m_deepFilterGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
    const QString needleLower = query.toLower();

    struct Candidate {
        QString path;
        FileCategory cat;
    };
    QList<Candidate> candidates;
    candidates.reserve(m_listWidget->count());
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!item || item->data(Qt::UserRole + 1).toBool()) continue;
        // Skip files already matched by filename to avoid redundant disk I/O
        if (item->text().contains(query, Qt::CaseInsensitive)) continue;
        candidates.append({
            item->data(Qt::UserRole).toString(),
            static_cast<FileCategory>(item->data(Qt::UserRole + 4).toInt())
        });
    }

    if (candidates.isEmpty()) return;

    QPointer<FolderViewer> self(this);
    QThreadPool::globalInstance()->start([self, gen, needleLower, candidates]() {
        QSet<QString> matched;
        for (const Candidate &c : candidates) {
            if (!self || self->m_deepFilterGeneration.load(std::memory_order_relaxed) != gen) {
                return;
            }
            if (deepContentMatchesFile(c.path, c.cat, needleLower)) {
                matched.insert(c.path);
            }
        }
        QMetaObject::invokeMethod(self, [self, gen, matched]() {
            if (!self || self->m_deepFilterGeneration.load(std::memory_order_relaxed) != gen) return;
            self->m_deepMatchedPaths = matched;
            self->m_hasDeepResults = true;
            self->applyCombinedFilter();
        }, Qt::QueuedConnection);
    });
}

void FolderViewer::ensureStudioWidget()
{
    if (m_studioWidget || !m_studioContainer) return;

    IFolderStudioPlugin *plugin = PluginManager::instance().folderStudioPlugin();
    if (plugin) {
        if (m_lblStudioFallback) {
            m_lblStudioFallback->hide();
        }
        m_studioWidget = plugin->createStudioWidget(m_currentFolder, m_studioContainer);
        if (m_studioWidget) {
            m_studioWidget->setOnFolderModified([this]() {
                if (!m_currentFolder.isEmpty()) {
                    loadFolder(m_currentFolder, true);
                }
            });
            m_studioWidget->setOnPreviewFileRequested([this](const QString &path, const QStringList &contextFiles) {
                if (!path.isEmpty()) {
                    emit filePreviewRequested(path, contextFiles);
                }
            });
            m_studioWidget->setActiveTool(static_cast<FolderStudioTool>(qBound(0, m_activeStudioTool, 3)));
            m_studioWidget->updateTheme();
            if (auto *lay = qobject_cast<QVBoxLayout*>(m_studioContainer->layout())) {
                lay->addWidget(m_studioWidget, 1);
            }
            return;
        }
    }

    // Graceful degradation if bubez_folder.so is not installed
    if (!m_lblStudioFallback) {
        m_lblStudioFallback = new QLabel(
            tr("Folder Studio module (bubez_folder.so) is not installed.\n\n"
               "Build or install the Folder Studio plugin to unlock multi-format Duplicates, "
               "PhotoRec/TestDisk Recovery, Smart Tree Organizer, and Batch Tools."),
            m_studioContainer);
        m_lblStudioFallback->setAlignment(Qt::AlignCenter);
        m_lblStudioFallback->setWordWrap(true);
        if (auto *lay = qobject_cast<QVBoxLayout*>(m_studioContainer->layout())) {
            lay->addWidget(m_lblStudioFallback, 1);
        }
        updateTheme();
    }
    m_lblStudioFallback->show();
}

void FolderViewer::setEditMode(bool editMode)
{
    m_isEditMode = editMode;
    if (!m_viewStack) return;

    if (editMode) {
        ensureStudioWidget();
        if (m_studioWidget) {
            m_studioWidget->setTargetFolder(m_currentFolder);
            m_studioWidget->setActiveTool(static_cast<FolderStudioTool>(qBound(0, m_activeStudioTool, 3)));
            m_studioWidget->updateTheme();
        }
        m_viewStack->setCurrentIndex(1);
    } else {
        if (m_studioWidget) {
            m_studioWidget->cancelActiveOperations();
        }
        m_viewStack->setCurrentIndex(0);
    }
}

void FolderViewer::setStudioTool(int toolIndex)
{
    m_activeStudioTool = qBound(0, toolIndex, 3);
    if (m_studioWidget) {
        m_studioWidget->setActiveTool(static_cast<FolderStudioTool>(m_activeStudioTool));
    }
}

void FolderViewer::detachStudioWidget()
{
    if (!m_studioWidget) return;
    m_studioWidget->cancelActiveOperations();
    if (m_studioContainer) {
        if (auto *lay = qobject_cast<QVBoxLayout*>(m_studioContainer->layout())) {
            lay->removeWidget(m_studioWidget);
        }
    }
    m_studioWidget->hide();
    // Synchronous delete is required so all plugin vtables are destroyed before dlclose()
    delete m_studioWidget;
    m_studioWidget = nullptr;

    m_isEditMode = false;
    if (m_viewStack) {
        m_viewStack->setCurrentIndex(0);
    }
}

