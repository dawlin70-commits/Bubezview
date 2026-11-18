#include "base_skin.h"
#include <QAbstractSlider>
#include <QComboBox>
#include <QKeyEvent>
#include <QStatusBar>
#include "image_viewer.h"
#include "text_viewer.h"
#include "pdf_base_viewer.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "core/interfaces/i_archive_viewer.h"
#include "core/interfaces/i_media_viewer.h"
#include "viewers/media/system_media_viewer.h"
#include "engine_prompt_card.h"
#include "build_options_dialog.h"
#include "office_viewer.h"
#include "folder_viewer.h"
#include "gallery_zoom_transition.h"
#include "theme_manager.h"
#include "settings_dialog.h"
#include "search_bar_widget.h"
#include "open_with_dialog.h"
#include "app_launcher.h"
#include "preload_cache_manager.h"
#include "update_manager.h"
#include "update_dialog.h"
#include "ocr_manager.h"
#include "ocr_dialog.h"
#include "fallback_notice_dialog.h"
#include "build_config.h"
#include "package_manager_helper.h"
#include "ui/pdf/pdf_image_extract_dialog.h"
#include "ui/pdf/pdf_repair_dialog.h"
#include <QProgressDialog>
#include <QCheckBox>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QMimeType>

#include <QVBoxLayout>
#include <QTextStream>
#include <QDir>
#include <QCollator>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <QTemporaryFile>
#include <QApplication>
#include <QToolTip>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QClipboard>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QProcess>
#include <QStandardPaths>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QEventLoop>

namespace {

QPixmap copyLogicalRect(const QPixmap &source, const QRect &logicalRect)
{
    if (source.isNull() || !logicalRect.isValid() || logicalRect.isEmpty()) {
        return QPixmap();
    }
    const qreal dpr = source.devicePixelRatio();
    const QRect physicalRect(
        qRound(logicalRect.x() * dpr),
        qRound(logicalRect.y() * dpr),
        qRound(logicalRect.width() * dpr),
        qRound(logicalRect.height() * dpr)
    );
    const QRect bounded = physicalRect.intersected(source.rect());
    if (bounded.isEmpty()) {
        return QPixmap();
    }
    QPixmap cropped = source.copy(bounded);
    cropped.setDevicePixelRatio(dpr);
    return cropped;
}

/**
 * @brief Dynamic stacked widget whose minimum size hint only reflects the active viewer,
 * preventing inactive engine cards or unsupported file inspectors from forcing large minimum sizes.
 */
class DynamicStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize sizeHint() const override {
        if (QWidget *w = currentWidget()) {
            return w->sizeHint();
        }
        return QStackedWidget::sizeHint();
    }

    QSize minimumSizeHint() const override {
        // Keep minimum size modest so inner viewer layouts never force the window to expand
        return QSize(320, 240);
    }
};
} // namespace
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QContextMenuEvent>
#include <QMenu>
#include <QSettings>
#include <QScreen>
#include <QWindow>
#include "goto_folder_dialog.h"
#include "share_dialog.h"
#include "share_manager.h"
#include "filmstrip_widget.h"
#include "dialogs/unified_export_dialog.h"
#include "image_inspector_dialog.h"
#include "image_resize_dialog.h"
#include <QVariantAnimation>
#include <QPainterPath>

/**
 * @brief Tear-free, single-pass transition overlay widget.
 *
 * Renders file switch transitions (Zoom & Fade, Parallax Slide, Cross Fade)
 * directly via QPainter opacity and transforms within a fixed geometry, avoiding
 * QGraphicsOpacityEffect offscreen FBO reallocations and layout overflow glitches.
 */
class TransitionOverlayWidget : public QWidget {
public:
    explicit TransitionOverlayWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        hide();
    }

    void begin(const QPixmap &pixmap, const QColor &bgColor, TransitionEffect effect, bool forward)
    {
        m_pixmap = pixmap;
        m_bgColor = bgColor;
        m_effect = effect;
        m_forward = forward;
        m_progress = 0.0;
        show();
        raise();
        update();
    }

    void setProgress(qreal progress)
    {
        m_progress = qBound(0.0, progress, 1.0);
        update();
    }

    void resetOverlay()
    {
        m_pixmap = QPixmap();
        m_progress = 0.0;
        hide();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_pixmap.isNull() || m_progress >= 1.0) return;

        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setOpacity(1.0 - m_progress);

        const QRect bounds = rect();
        QTransform tx;
        switch (m_effect) {
        case TransitionEffect::ZoomFade: {
            const qreal deltaScale = (m_forward ? 0.035 : -0.035) * m_progress;
            const qreal scale = 1.0 + deltaScale;
            const QPointF center = QRectF(bounds).center();
            tx.translate(center.x(), center.y());
            tx.scale(scale, scale);
            tx.translate(-center.x(), -center.y());
            break;
        }
        case TransitionEffect::ParallaxSlide: {
            const qreal slideOffset = (m_forward ? -64.0 : 64.0) * m_progress;
            tx.translate(slideOffset, 0.0);
            break;
        }
        case TransitionEffect::CrossFade:
        default:
            break;
        }

        p.fillRect(bounds, m_bgColor);
        p.setTransform(tx);
        p.drawPixmap(bounds, m_pixmap);
    }

private:
    QPixmap m_pixmap;
    QColor m_bgColor = QColor("#22262b");
    TransitionEffect m_effect = TransitionEffect::ZoomFade;
    bool m_forward = true;
    qreal m_progress = 0.0;
};


BaseSkin::BaseSkin(QWidget *parent)
    : QMainWindow(parent)
    , m_centralContainer(nullptr)
    , m_stackedWidget(nullptr)
    , m_searchBar(nullptr)
    , m_filmstrip(nullptr)
    , m_imageViewer(nullptr)
    , m_textViewer(nullptr)
    , m_pdfViewer(nullptr)
    , m_archiveViewer(nullptr)
    , m_mediaViewer(nullptr)
    , m_officeViewer(nullptr)
    , m_folderViewer(nullptr)
    , m_filmstripWanted(false)
    , m_currentIndex(-1)
    , m_currentCategory(FileCategory::Unknown)
    , m_transitionOverlay(nullptr)
    , m_galleryTransition(nullptr)
    , m_overlayOpacity(nullptr)
    , m_animGroup(nullptr)
    , m_lastSwitchTimeMs(0)
    , m_slideshowTimer(nullptr)
{
    setWindowIcon(QIcon::fromTheme("bubezview", QIcon(":/icons/bubezview.png")));

    m_geometrySaveTimer = new QTimer(this);
    m_geometrySaveTimer->setSingleShot(true);
    connect(m_geometrySaveTimer, &QTimer::timeout, this, &BaseSkin::saveWindowGeometry);

    restoreWindowGeometry();

    QSettings settings("Bubez", "BubezView");
    if (!settings.contains("ui/filmstrip")) {
        QSettings legacySettings("BubezView", "BubezView");
        if (legacySettings.contains("ui/filmstrip")) {
            settings.setValue("ui/filmstrip", legacySettings.value("ui/filmstrip"));
        }
    }
    m_filmstripWanted = settings.value("ui/filmstrip", false).toBool();

    m_centralContainer = new QWidget(this);
    m_centralContainer->setObjectName("centralContainer");
    m_centralContainer->setAutoFillBackground(false);
    QVBoxLayout *centralLayout = new QVBoxLayout(m_centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_stackedWidget = new DynamicStackedWidget(m_centralContainer);
    m_stackedWidget->setAutoFillBackground(true);

    m_imageViewer = new ImageViewer(this);
    m_textViewer = new TextViewer(this);
    connect(m_textViewer, &TextViewer::modificationChanged, this, [this](bool) {
        updateWindowTitle();
        updateStatusBar();
    });
    m_enginePromptCard = new EnginePromptCard(this);
    connect(m_enginePromptCard, &EnginePromptCard::engineActivated, this, [this](const QString &path) {
        openFile(path);
    });
    connect(m_enginePromptCard, &EnginePromptCard::compileRequested, this, [this]() {
        BuildOptionsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            PluginManager::instance().discoverPlugins();
            if (!m_currentFilePath.isEmpty()) {
                openFile(m_currentFilePath);
            }
        }
    });

    // 7. Unsupported File Technical Inspection Card
    m_unsupportedViewer = new QWidget(this);
    QVBoxLayout *unsupLay = new QVBoxLayout(m_unsupportedViewer);
    unsupLay->setAlignment(Qt::AlignCenter);
    unsupLay->setSpacing(14);

    QFrame *cardFrame = new QFrame(m_unsupportedViewer);
    cardFrame->setObjectName("unsupportedCard");
    cardFrame->setStyleSheet("#unsupportedCard { background-color: rgba(30, 41, 59, 0.7); border: 1px solid rgba(148, 163, 184, 0.25); border-radius: 12px; padding: 24px; }");
    cardFrame->setMaximumWidth(540);
    cardFrame->setMinimumWidth(320);

    QVBoxLayout *cardLay = new QVBoxLayout(cardFrame);
    cardLay->setSpacing(14);
    cardLay->setAlignment(Qt::AlignCenter);

    m_lblUnsupportedIcon = new QLabel(cardFrame);
    m_lblUnsupportedIcon->setAlignment(Qt::AlignCenter);
    cardLay->addWidget(m_lblUnsupportedIcon);

    m_lblUnsupportedName = new QLabel(cardFrame);
    m_lblUnsupportedName->setAlignment(Qt::AlignCenter);
    m_lblUnsupportedName->setStyleSheet("font-size: 16px; font-weight: bold; color: #f8fafc;");
    m_lblUnsupportedName->setWordWrap(true);
    cardLay->addWidget(m_lblUnsupportedName);

    m_lblUnsupportedDetails = new QLabel(cardFrame);
    m_lblUnsupportedDetails->setAlignment(Qt::AlignLeft);
    m_lblUnsupportedDetails->setStyleSheet("font-size: 12px; color: #cbd5e1; line-height: 1.6; padding: 12px; background-color: rgba(15, 23, 42, 0.5); border-radius: 8px; border: 1px solid rgba(148, 163, 184, 0.15);");
    m_lblUnsupportedDetails->setWordWrap(true);
    cardLay->addWidget(m_lblUnsupportedDetails);

    QHBoxLayout *unsupBtnLay = new QHBoxLayout();
    unsupBtnLay->setSpacing(12);

    m_btnUnsupportedOpenWith = new QPushButton(tr("Open with another application..."), cardFrame);
    m_btnUnsupportedOpenWith->setCursor(Qt::PointingHandCursor);
    m_btnUnsupportedOpenWith->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 8px 16px; border-radius: 6px; font-size: 12px; }"
                                           "QPushButton:hover { background-color: #0369a1; }");
    connect(m_btnUnsupportedOpenWith, &QPushButton::clicked, this, &BaseSkin::openCurrentWithExternal);

    m_btnUnsupportedHexDump = new QPushButton(tr("View Hex Dump"), cardFrame);
    m_btnUnsupportedHexDump->setCursor(Qt::PointingHandCursor);
    m_btnUnsupportedHexDump->setStyleSheet("QPushButton { background-color: rgba(100, 116, 139, 0.25); color: #f1f5f9; padding: 8px 16px; border-radius: 6px; font-size: 12px; border: 1px solid rgba(100, 116, 139, 0.35); }"
                                          "QPushButton:hover { background-color: rgba(100, 116, 139, 0.4); color: white; }");
    connect(m_btnUnsupportedHexDump, &QPushButton::clicked, this, &BaseSkin::showCurrentHexDump);

    unsupBtnLay->addWidget(m_btnUnsupportedOpenWith);
    unsupBtnLay->addWidget(m_btnUnsupportedHexDump);
    cardLay->addLayout(unsupBtnLay);

    unsupLay->addWidget(cardFrame);

    m_stackedWidget->addWidget(m_imageViewer);       // 0
    m_stackedWidget->addWidget(m_textViewer);        // 1
    m_stackedWidget->addWidget(new QWidget(this));   // 2 (Lazy IPdfViewer)
    m_stackedWidget->addWidget(new QWidget(this));   // 3 (Lazy IArchiveViewer)
    m_stackedWidget->addWidget(new QWidget(this));   // 4 (Lazy IMediaViewer)
    m_stackedWidget->addWidget(new QWidget(this));   // 5 (Lazy OfficeViewer)
    m_stackedWidget->addWidget(new QWidget(this));   // 6 (Lazy FolderViewer)
    m_stackedWidget->addWidget(m_unsupportedViewer); // 7
    m_stackedWidget->addWidget(m_enginePromptCard);  // 8

    centralLayout->addWidget(m_stackedWidget, 1);

    m_filmstrip = new FilmstripWidget(m_centralContainer);
    m_filmstrip->hide();
    centralLayout->addWidget(m_filmstrip, 0);

    connect(m_filmstrip, &FilmstripWidget::fileSelected, this, [this](const QString &path) {
        if (path != m_currentFilePath) {
            int newIdx = m_siblingFiles.indexOf(path);
            bool forward = (newIdx >= m_currentIndex);
            startTransitionAnimation(forward);
            if (newIdx >= 0) m_currentIndex = newIdx;
        }
        openFile(path);
    });

    m_searchBar = new SearchBarWidget(m_centralContainer);
    m_searchBar->hide();

    setCentralWidget(m_centralContainer);


    // Search bar connections
    connect(m_searchBar, &SearchBarWidget::findNextRequested, this, [this](const QString &text, bool matchCase) {
        if (text.isEmpty()) return;
        bool wrapped = false;
        bool found = false;
        if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
            found = m_textViewer->findNext(text, matchCase, &wrapped);
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            found = m_pdfViewer->findNext(text, matchCase, &wrapped);
        } else if (m_stackedWidget->currentIndex() == 3 && m_archiveViewer) {
            m_archiveViewer->filterEntries(text);
            found = true;
        }
        m_searchBar->updateStatus(found, wrapped);
    });

    connect(m_searchBar, &SearchBarWidget::findPreviousRequested, this, [this](const QString &text, bool matchCase) {
        if (text.isEmpty()) return;
        bool wrapped = false;
        bool found = false;
        if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
            found = m_textViewer->findPrev(text, matchCase, &wrapped);
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            found = m_pdfViewer->findPrev(text, matchCase, &wrapped);
        }
        m_searchBar->updateStatus(found, wrapped);
    });

    connect(m_searchBar, &SearchBarWidget::replaceRequested, this, [this](const QString &findText, const QString &replaceText, bool matchCase) {
        if (findText.isEmpty()) return;
        if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
            bool found = m_textViewer->replaceNext(findText, replaceText, matchCase);
            m_searchBar->updateStatus(found, false);
            updateStatusBar();
            updateWindowTitle();
        }
    });

    connect(m_searchBar, &SearchBarWidget::replaceAllRequested, this, [this](const QString &findText, const QString &replaceText, bool matchCase) {
        if (findText.isEmpty()) return;
        if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
            int count = m_textViewer->replaceAll(findText, replaceText, matchCase);
            m_searchBar->setStatus(tr("Replaced %n occurrence(s)", "", count));
            updateStatusBar();
            updateWindowTitle();
        }
    });

    connect(m_searchBar, &SearchBarWidget::searchClosed, this, [this]() {
        m_searchBar->hide();
    });

    // Transitions
    m_transitionOverlay = new TransitionOverlayWidget(m_centralContainer);
    m_galleryTransition = new GalleryZoomTransition(m_centralContainer);

    m_animGroup = new QParallelAnimationGroup(this);
    connect(m_animGroup, &QParallelAnimationGroup::finished, this, [this]() {
        if (m_transitionOverlay) {
            m_transitionOverlay->resetOverlay();
            if (m_stackedWidget) {
                m_transitionOverlay->setGeometry(m_stackedWidget->geometry());
            }
        }
    });

    ShareManager::setFolderCompressor([this](const QString &folderPath, const QString &destZipPath, QWidget *parentWidget) -> bool {
        if (!ensureArchiveViewerCreated()) return false;
        return m_archiveViewer->createArchive({folderPath}, destZipPath, QStringLiteral("zip"), QString(), parentWidget);
    });

    // Theme will be applied once on the fully-constructed MainWindow derived instance
    // applyCurrentTheme();

    setEditMode(false);

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

BaseSkin::~BaseSkin()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    if (m_geometrySaveTimer) {
        m_geometrySaveTimer->stop();
    }
    if (m_slideshowTimer) {
        m_slideshowTimer->stop();
    }
    if (m_animGroup) {
        m_animGroup->stop();
    }
    if (m_galleryTransition) {
        m_galleryTransition->stop();
    }
    if (m_mediaViewer) {
        m_mediaViewer->stop();
    }
    if (m_folderViewer) {
        m_folderViewer->clear();
    }
    if (m_officeViewer) {
        m_officeViewer->clear();
    }
}

IPdfViewer *BaseSkin::ensurePdfViewerCreated()
{
    if (m_pdfViewer) {
        return m_pdfViewer;
    }
    IPdfViewer *newViewer = nullptr;
    if (PluginManager::instance().isEngineAvailable(FileCategory::Pdf)) {
        auto *vw = PluginManager::instance().createViewer(FileCategory::Pdf, this);
        newViewer = dynamic_cast<IPdfViewer*>(vw);
    }
    if (!newViewer) {
        newViewer = new PdfBaseViewer(this);
    }
    m_pdfViewer = newViewer;

    if (m_pdfViewer) {
        safeReplaceStackedWidget(2, m_pdfViewer->widget());
        m_pdfViewer->updateTheme();
        if (auto *qobj = dynamic_cast<QObject*>(m_pdfViewer)) {
            connect(qobj, SIGNAL(scanTextRequested()), this, SLOT(extractOcrCurrentFile()));
            connect(qobj, SIGNAL(scanSnippetRequested(QImage)), this, SLOT(extractOcrSnippet(QImage)));
        }
        if (auto *basePdf = dynamic_cast<PdfBaseViewer*>(m_pdfViewer)) {
            connect(basePdf, &PdfBaseViewer::requestStudio, this, [this]() {
                setEditMode(!isEditMode());
            });
            connect(basePdf, &PdfBaseViewer::savePageAsPdfRequested, this, &BaseSkin::saveCurrentPageAsPdf);
            connect(basePdf, &PdfBaseViewer::savePageAsImageRequested, this, &BaseSkin::saveCurrentPageAsImage);
            connect(basePdf, &PdfBaseViewer::saveAsRequested, this, &BaseSkin::saveAsCurrentFile);
            connect(basePdf, &PdfBaseViewer::exportRequested, this, &BaseSkin::exportCurrentFileAs);
            connect(basePdf, &PdfBaseViewer::scanTextRequested, this, &BaseSkin::extractOcrCurrentFile);
            connect(basePdf, &PdfBaseViewer::scanSnippetRequested, this, &BaseSkin::extractOcrSnippet);
        }
        onViewerInitialized(FileCategory::Pdf);
    }
    return m_pdfViewer;
}

void BaseSkin::safeReplaceStackedWidget(int index, QWidget *newWidget)
{
    if (!m_stackedWidget || !newWidget) return;

    QWidget *oldW = m_stackedWidget->widget(index);
    const bool wasCurrent = (m_stackedWidget->currentWidget() == oldW);

    m_stackedWidget->insertWidget(index, newWidget);
    if (wasCurrent) {
        m_stackedWidget->setCurrentWidget(newWidget);
        newWidget->show();
    }
    if (oldW && oldW != newWidget) {
        oldW->hide();
        m_stackedWidget->removeWidget(oldW);
        oldW->setParent(nullptr);
        oldW->deleteLater();
    }
}

bool BaseSkin::promotePdfViewerToStudio()
{
    if (!m_pdfViewer) {
        ensurePdfViewerCreated();
    }
    if (!m_pdfViewer) return false;

    // If it's already the studio plugin viewer (not PdfBaseViewer), nothing to do
    if (!dynamic_cast<PdfBaseViewer*>(m_pdfViewer)) {
        return true;
    }

    // Check if the PDF Studio plugin is available
    if (!PluginManager::instance().isEngineAvailable(FileCategory::Pdf)) {
        QMessageBox::information(
            this,
            tr("PDF Studio Plugin Required"),
            tr("<p>The <b>PDF Studio</b> module (<code>bubez_pdf.so</code>) is required for editing, digital signatures, and page management.</p>"
               "<p>You can compile or install this module from Settings → Extensions.</p>")
        );
        return false;
    }

    int curPage = m_pdfViewer->currentPage();
    double curZoom = m_pdfViewer->zoomFactor();
    QString curPath = m_currentFilePath;
    int curRot = 0;
    if (auto *baseVw = dynamic_cast<PdfBaseViewer*>(m_pdfViewer)) {
        curRot = baseVw->rotationDegrees();
    }

    IViewerWidget *vw = PluginManager::instance().createViewer(FileCategory::Pdf, this);
    IPdfViewer *studioPdf = dynamic_cast<IPdfViewer*>(vw);
    if (!studioPdf) {
        delete vw;
        return false;
    }

    QPixmap preSnap;
    const bool useCrossFade = (ThemeManager::currentTransition() != TransitionEffect::None &&
                               m_stackedWidget && m_stackedWidget->isVisible() &&
                               QGuiApplication::platformName() != QLatin1String("offscreen"));
    if (useCrossFade) {
        preSnap = m_stackedWidget->grab();
    }

    if (m_stackedWidget) {
        m_stackedWidget->setUpdatesEnabled(false);
    }

    m_pdfViewer = studioPdf;
    safeReplaceStackedWidget(2, m_pdfViewer->widget());
    if (m_currentCategory == FileCategory::Pdf) {
        m_stackedWidget->setCurrentIndex(2);
    }
    m_pdfViewer->updateTheme();
    onViewerInitialized(FileCategory::Pdf);

    if (auto *qobj = dynamic_cast<QObject*>(m_pdfViewer)) {
        connect(qobj, SIGNAL(scanTextRequested()), this, SLOT(extractOcrCurrentFile()));
        connect(qobj, SIGNAL(scanSnippetRequested(QImage)), this, SLOT(extractOcrSnippet(QImage)));
    }

    if (!curPath.isEmpty() && QFile::exists(curPath)) {
        m_pdfViewer->loadFile(curPath);
        if (curRot != 0) {
            if (curRot == 90) m_pdfViewer->rotateClockwise();
            else if (curRot == 180) { m_pdfViewer->rotateClockwise(); m_pdfViewer->rotateClockwise(); }
            else if (curRot == 270) m_pdfViewer->rotateCounterClockwise();
        }
        // Only apply explicit zoom if user had customized it away from fit-to-width default
        if (curZoom > 0.05 && !qFuzzyCompare(curZoom, 1.0)) {
            m_pdfViewer->setZoomFactor(curZoom);
        }
        if (curPage > 0) {
            m_pdfViewer->goToPage(curPage);
        }
    }

    if (m_stackedWidget) {
        m_stackedWidget->setUpdatesEnabled(true);
    }

    if (useCrossFade && !preSnap.isNull() && m_transitionOverlay && m_animGroup) {
        if (m_animGroup->state() == QAbstractAnimation::Running) {
            m_animGroup->stop();
        }
        m_animGroup->clear();
        m_transitionOverlay->resetOverlay();
        m_transitionOverlay->setGeometry(m_stackedWidget->geometry());
        QColor bgColor = m_imageViewer ? m_imageViewer->canvasBackgroundColor() : palette().window().color();
        m_transitionOverlay->begin(preSnap, bgColor, TransitionEffect::CrossFade, true);

        auto *progressAnim = new QVariantAnimation(m_animGroup);
        progressAnim->setDuration(150);
        progressAnim->setStartValue(0.0);
        progressAnim->setEndValue(1.0);
        progressAnim->setEasingCurve(QEasingCurve::InOutQuad);
        connect(progressAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
            if (m_transitionOverlay) {
                m_transitionOverlay->setProgress(val.toReal());
            }
        });
        m_animGroup->addAnimation(progressAnim);
        m_animGroup->start();
    }
    return true;
}

IArchiveViewer *BaseSkin::ensureArchiveViewerCreated()
{
    if (m_archiveViewer) {
        return m_archiveViewer;
    }
    auto *av = PluginManager::instance().createViewer(FileCategory::Archive, this);
    m_archiveViewer = dynamic_cast<IArchiveViewer*>(av);
    if (m_archiveViewer) {
        safeReplaceStackedWidget(3, m_archiveViewer->widget());
        m_archiveViewer->setOnFilePreviewRequested([this](const QString &extractedFilePath, const QString &archiveOriginPath, const QString &internalEntryPath) {
            m_archiveOriginPath = archiveOriginPath;
            if (m_archiveViewer) {
                m_archiveEntries = m_archiveViewer->previewableEntries();
            }
            m_archiveCurrentEntryIndex = m_archiveEntries.indexOf(internalEntryPath);
            if (m_archiveCurrentEntryIndex < 0 && !m_archiveEntries.isEmpty()) {
                m_archiveCurrentEntryIndex = 0;
            }
            openArchiveFile(extractedFilePath, internalEntryPath);
        });
        m_archiveViewer->updateTheme();
        onViewerInitialized(FileCategory::Archive);
    }
    return m_archiveViewer;
}

IMediaViewer *BaseSkin::ensureMediaViewerCreated()
{
    QSettings s("Bubez", "BubezView");
    const bool mpvEnabled = s.value("modules/mpv_enabled", true).toBool();
    const bool canUseMpv = mpvEnabled && PluginManager::instance().isEngineAvailable(FileCategory::Media);
    const bool isSystemViewer = (dynamic_cast<SystemMediaViewer*>(m_mediaViewer) != nullptr);

    if (!m_mediaViewer || (canUseMpv && isSystemViewer) || (!canUseMpv && !isSystemViewer)) {
        if (m_mediaViewer) {
            m_mediaViewer->clear();
        }
        IMediaViewer *newViewer = nullptr;
        if (canUseMpv) {
            auto *mv = PluginManager::instance().createViewer(FileCategory::Media, this);
            newViewer = dynamic_cast<IMediaViewer*>(mv);
        }
        if (!newViewer) {
            newViewer = new SystemMediaViewer(this);
        }
        m_mediaViewer = newViewer;
        safeReplaceStackedWidget(4, m_mediaViewer->widget());
        m_mediaViewer->setOnTrackChangeRequested([this](const QString &path) {
            openFile(path);
        });
        m_mediaViewer->setOnUpgradeToProRequested([this]() {
            promoteMediaViewerToStudio();
        });
        m_mediaViewer->updateTheme();
        onViewerInitialized(FileCategory::Media);
    }
    return m_mediaViewer;
}

bool BaseSkin::promoteMediaViewerToStudio()
{
    if (!m_mediaViewer) {
        ensureMediaViewerCreated();
    }
    if (!m_mediaViewer) return false;

    // If it's already the MPV studio plugin (not SystemMediaViewer), nothing to do
    if (!dynamic_cast<SystemMediaViewer*>(m_mediaViewer)) {
        return true;
    }

    // Check if the Media Studio plugin is available
    if (!PluginManager::instance().isEngineAvailable(FileCategory::Media)) {
        const QString distro = PackageManagerHelper::distroDisplayName();
        const QString installCmd = PackageManagerHelper::getInstallCommand(QStringLiteral("mpv"));
        QMessageBox prompt(this);
        prompt.setWindowTitle(tr("Multimedia Studio (Pro Mode)"));
        prompt.setIcon(QMessageBox::Information);
        prompt.setText(tr("<h3>Activate Multimedia Studio (Pro Mode)</h3>"
                          "<p>The <b>Multimedia Studio (MPV)</b> engine unlocks precision seeking, 64-bit float audio playback, and full audio editing tools (cutter, tags, filters, pitch shifting, and equalizer).</p>"
                          "<p>Detected distribution: <b>%1</b></p>"
                          "<p>Installation command:<br><code>%2</code></p>")
                          .arg(distro, installCmd));

        QPushButton *btnInstall = prompt.addButton(tr("⚡ Install & Activate Pro Mode"), QMessageBox::AcceptRole);
        QPushButton *btnCancel = prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
        Q_UNUSED(btnCancel);
        prompt.setDefaultButton(btnInstall);
        prompt.exec();

        if (prompt.clickedButton() == btnInstall) {
            if (PackageManagerHelper::installComponentInteractive(QStringLiteral("mpv"), this)) {
                // If bubez_media.so has not been compiled yet, attempt to build it
                QString appDir = QCoreApplication::applicationDirPath();
                QString pluginPath = appDir + QStringLiteral("/plugins/bubez_media.so");
                if (!QFile::exists(pluginPath)) {
                    QProcess configProc;
                    configProc.start(QStringLiteral("cmake"), {QStringLiteral("-B"), appDir});
                    configProc.waitForFinished(30000);

                    QProcess buildProc;
                    buildProc.start(QStringLiteral("cmake"), {QStringLiteral("--build"), appDir, QStringLiteral("--target"), QStringLiteral("bubez_media"), QStringLiteral("-j4")});
                    buildProc.waitForFinished(60000);
                }
                PluginManager::instance().discoverPlugins();
            }
        }
        if (!PluginManager::instance().isEngineAvailable(FileCategory::Media)) {
            return false;
        }
    }

    QString curPath = m_currentFilePath;

    IViewerWidget *vw = PluginManager::instance().createViewer(FileCategory::Media, this);
    IMediaViewer *studioMv = dynamic_cast<IMediaViewer*>(vw);
    if (!studioMv) {
        delete vw;
        return false;
    }

    m_mediaViewer->clear();
    m_mediaViewer = studioMv;
    safeReplaceStackedWidget(4, m_mediaViewer->widget());
    if (m_currentCategory == FileCategory::Media) {
        m_stackedWidget->setCurrentIndex(4);
    }
    m_mediaViewer->setSiblingFiles(m_siblingFiles);
    m_mediaViewer->setOnTrackChangeRequested([this](const QString &path) {
        openFile(path);
    });
    m_mediaViewer->setOnUpgradeToProRequested([this]() {
        promoteMediaViewerToStudio();
    });
    m_mediaViewer->updateTheme();
    onViewerInitialized(FileCategory::Media);

    if (!curPath.isEmpty() && QFile::exists(curPath)) {
        m_mediaViewer->loadFile(curPath);
    }
    return true;
}

OfficeViewer *BaseSkin::ensureOfficeViewerCreated()
{
    if (m_officeViewer) {
        return m_officeViewer;
    }
    m_officeViewer = new OfficeViewer(this);
    safeReplaceStackedWidget(5, m_officeViewer);
    onViewerInitialized(FileCategory::Office);
    return m_officeViewer;
}

FolderViewer *BaseSkin::ensureFolderViewerCreated()
{
    if (m_folderViewer) {
        return m_folderViewer;
    }
    m_folderViewer = new FolderViewer(this);
    safeReplaceStackedWidget(6, m_folderViewer);

    connect(m_folderViewer, &FolderViewer::folderLoaded, this, [this](const QString &folderPath, int, qint64) {
        if (m_currentCategory == FileCategory::Folder && !folderPath.isEmpty()) {
            m_currentFilePath = folderPath;
            updateWindowTitle();
        }
    });
    connect(m_folderViewer, &FolderViewer::filePreviewRequested, this, [this](const QString &path) {
        openFileFromGalleryWithZoom(path);
    });
    connect(m_folderViewer, &FolderViewer::itemContextMenuRequested, this, [this](const QPoint &globalPos, const QString &) {
        showContextMenuAt(globalPos);
    });

    onViewerInitialized(FileCategory::Folder);
    return m_folderViewer;
}

BaseSkin::OpenResult BaseSkin::openFile(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) {
        return OpenResult::Failed;
    }

    // Check for unsaved changes if in edit mode before opening another file
    if (m_isEditMode && !m_currentFilePath.isEmpty() && info.canonicalFilePath() != QFileInfo(m_currentFilePath).canonicalFilePath()) {
        if (!promptAndSaveIfModified()) {
            return OpenResult::Failed;
        }
        // If the current and incoming files are both audio files (Audio Studio active),
        // preserve edit mode so workspace page and studio session remain intact.
        FileCategory nextCat = PluginManager::categorizeFile(info.absoluteFilePath());
        if (m_currentCategory != FileCategory::Media || nextCat != FileCategory::Media) {
            setEditMode(false);
        }
    }

    if (!m_isLoadingArchivePreview) {
        m_archiveOriginPath.clear();
        m_archiveEntries.clear();
        m_archiveCurrentEntryIndex = -1;
    }

    if (m_backgroundTimer && m_backgroundTimer->isActive()) {
        m_backgroundTimer->stop();
    }
    if (!isVisible()) {
        show();
        raise();
        activateWindow();
    }

    if (m_mediaViewer) {
        m_mediaViewer->stop();
    }
    m_currentFilePath = info.absoluteFilePath();
    updateWindowTitle();

    if (!m_isLoadingArchivePreview) {
        scanDirectory(m_currentFilePath);
    }

    m_currentCategory = PluginManager::categorizeFile(m_currentFilePath);

    bool opened = false;
    if (m_currentCategory == FileCategory::Image) {
        m_stackedWidget->setCurrentIndex(0);
        QImage cachedImg;
        if (PreloadCacheManager::instance().tryGet(m_currentFilePath, cachedImg)) {
            opened = m_imageViewer->loadImage(cachedImg);
        } else {
            opened = m_imageViewer->loadFile(m_currentFilePath);
            if (opened) {
                PreloadCacheManager::instance().insert(m_currentFilePath, m_imageViewer->originalImage());
            }
        }
    } else if (m_currentCategory == FileCategory::Text) {
        m_stackedWidget->setCurrentIndex(1);
        opened = m_textViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Pdf) {
        if (!ensurePdfViewerCreated()) {
            m_enginePromptCard->setTargetFile(FileCategory::Pdf, m_currentFilePath);
            m_stackedWidget->setCurrentWidget(m_enginePromptCard);
            return OpenResult::Success;
        }
        m_stackedWidget->setCurrentIndex(2);
        opened = m_pdfViewer->loadFile(m_currentFilePath);

        if (!opened && QFile::exists(m_currentFilePath)) {
            QMessageBox::StandardButton ask = QMessageBox::question(
                this,
                tr("Corrupted PDF Detected"),
                tr("<p>Failed to open <b>%1</b>.</p>"
                   "<p>The document structure appears damaged or incomplete.</p>"
                   "<p>Would you like to repair it using QPDF?</p>").arg(QFileInfo(m_currentFilePath).fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (ask == QMessageBox::Yes) {
                openPdfRepairDialog(m_currentFilePath);
                return OpenResult::Success;
            }
        }
    } else if (m_currentCategory == FileCategory::Archive) {
        if (!ensureArchiveViewerCreated()) {
            m_enginePromptCard->setTargetFile(FileCategory::Archive, m_currentFilePath);
            m_stackedWidget->setCurrentWidget(m_enginePromptCard);
            return OpenResult::Success;
        }
        m_stackedWidget->setCurrentIndex(3);
        opened = m_archiveViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Media) {
        const QString ext = QFileInfo(m_currentFilePath).suffix().toLower();
        static const QSet<QString> s_exoticMediaExtensions = {
            QStringLiteral("dsf"), QStringLiteral("dff"), QStringLiteral("dsd"),
            QStringLiteral("ape"), QStringLiteral("mac"),
            QStringLiteral("wv"),
            QStringLiteral("mod"), QStringLiteral("xm"), QStringLiteral("it"), QStringLiteral("s3m"),
            QStringLiteral("med"), QStringLiteral("okt"), QStringLiteral("669"), QStringLiteral("mtm"),
            QStringLiteral("ac3"), QStringLiteral("eac3"), QStringLiteral("dts"), QStringLiteral("dtshd"),
            QStringLiteral("mpc"), QStringLiteral("mp+"), QStringLiteral("mpp"),
            QStringLiteral("tta"), QStringLiteral("tak")
        };
        const bool isExotic = s_exoticMediaExtensions.contains(ext);
        const bool hasMpv = PluginManager::instance().isEngineAvailable(FileCategory::Media);

        if (isExotic) {
            if (!hasMpv) {
                const QString distro = PackageManagerHelper::distroDisplayName();
                const QString installCmd = PackageManagerHelper::getInstallCommand(QStringLiteral("mpv"));
                QMessageBox prompt(this);
                prompt.setWindowTitle(tr("Multimedia Studio Module Required"));
                prompt.setIcon(QMessageBox::Warning);
                prompt.setText(tr("<h3>Advanced Media Engine Required</h3>"
                                  "<p>The audio format <b>.%1</b> requires the <b>Multimedia Studio</b> engine (<code>bubez_media.so</code> / <code>libmpv</code>) for decoding and playback.</p>"
                                  "<p>Detected distribution: <b>%2</b></p>"
                                  "<p>Manual installation command:<br><code>%3</code></p>")
                                  .arg(ext.toUpper(), distro, installCmd));

                QPushButton *btnInstall = prompt.addButton(tr("Install MPV Engine"), QMessageBox::AcceptRole);
                QPushButton *btnCancel = prompt.addButton(tr("Cancel"), QMessageBox::RejectRole);
                Q_UNUSED(btnCancel);
                prompt.setDefaultButton(btnInstall);
                prompt.exec();

                if (prompt.clickedButton() == btnInstall) {
                    if (PackageManagerHelper::installComponentInteractive(QStringLiteral("mpv"), this)) {
                        QString appDir = QCoreApplication::applicationDirPath();
                        QString pluginPath = appDir + QStringLiteral("/plugins/bubez_media.so");
                        if (!QFile::exists(pluginPath)) {
                            QProcess configProc;
                            configProc.start(QStringLiteral("cmake"), {QStringLiteral("-B"), appDir});
                            configProc.waitForFinished(30000);

                            QProcess buildProc;
                            buildProc.start(QStringLiteral("cmake"), {QStringLiteral("--build"), appDir, QStringLiteral("--target"), QStringLiteral("bubez_media"), QStringLiteral("-j4")});
                            buildProc.waitForFinished(60000);
                        }
                        PluginManager::instance().discoverPlugins();
                    }
                }
                if (!PluginManager::instance().isEngineAvailable(FileCategory::Media)) {
                    return OpenResult::Failed;
                }
            }
            promoteMediaViewerToStudio();
        } else {
            // If the high-performance MPV engine is available, use it directly for hardware-accelerated playback
            if (hasMpv) {
                promoteMediaViewerToStudio();
            }
        }

        ensureMediaViewerCreated();
        m_stackedWidget->setCurrentIndex(4);
        m_mediaViewer->setSiblingFiles(m_siblingFiles);
        opened = m_mediaViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Office) {
        ensureOfficeViewerCreated();
        m_stackedWidget->setCurrentIndex(5);
        opened = m_officeViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Folder) {
        ensureFolderViewerCreated();
        m_stackedWidget->setCurrentIndex(6);
        opened = m_folderViewer->loadFolder(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Unknown || !PluginManager::isNativeSupported(m_currentFilePath)) {
        m_stackedWidget->setCurrentIndex(7);
        opened = loadUnsupportedFileInfo(m_currentFilePath);
    }

    if (opened) {
        if (m_searchBar && m_searchBar->isVisible() && m_currentCategory != FileCategory::Text && m_currentCategory != FileCategory::Pdf) {
            m_searchBar->hide();
        }

        if (m_currentCategory == FileCategory::Folder) {
            m_filmstrip->hide();
        } else {
            m_filmstrip->setFiles(m_siblingFiles, m_currentIndex);
            if (m_filmstripWanted && !m_siblingFiles.isEmpty()) {
                m_filmstrip->show();
            } else {
                m_filmstrip->hide();
            }
        }

        onFileLoaded(m_currentFilePath, m_currentCategory);

        // Asynchronously prefetch adjacent candidate files into memory
        if (!m_siblingFiles.isEmpty() && m_currentIndex >= 0) {
            PreloadCacheManager::instance().requestPrefetchNeighbors(m_siblingFiles, m_currentIndex);
            PreloadCacheManager::instance().inspectFolderForPdfs(m_siblingFiles);
        }

        if (m_transitionOverlay && m_animGroup && m_animGroup->state() == QAbstractAnimation::Running) {
            m_transitionOverlay->raise();
        }

        setEditMode(m_isEditMode);

        return OpenResult::Success;
    }


    return OpenResult::Failed;
}

void BaseSkin::openArchiveFile(const QString &extractedFilePath, const QString &internalEntryPath)
{
    m_isLoadingArchivePreview = true;
    OpenResult res = openFile(extractedFilePath);
    m_isLoadingArchivePreview = false;

    if (res == OpenResult::Success) {
        updateWindowTitle();
        onFileLoaded(extractedFilePath, m_currentCategory);
    }
}

void BaseSkin::onFileLoaded(const QString &filePath, FileCategory category)
{
    Q_UNUSED(filePath);
    Q_UNUSED(category);
    // Base implementation does nothing; overridden by derived skins
}

void BaseSkin::scanDirectory(const QString &currentFilePath)
{
    QFileInfo info(currentFilePath);
    const QString absPath = info.absoluteFilePath();

    // Fast path: if file is already present in sibling list, avoid redundant disk scan
    const int existingIdx = m_siblingFiles.indexOf(absPath);
    if (existingIdx != -1) {
        m_currentIndex = existingIdx;
        return;
    }

    QDir dir = info.dir();

    QDir::Filters filters = QDir::Files;
    if (info.isDir()) {
        filters = QDir::Dirs | QDir::NoDotAndDotDot;
    }
    QStringList entries = dir.entryList(filters, QDir::NoSort);

    // Natural numeric sorting identical to Dolphin / KDE Plasma and terminal (ls -v)
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), collator);

    m_siblingFiles.clear();

    for (const QString &f : entries) {
        if (f.startsWith('.')) continue;
        QString absPath = dir.absoluteFilePath(f);
        if (PluginManager::isNativeSupported(absPath) || BuildConfig::showUnsupportedFiles()) {
            m_siblingFiles.append(absPath);
        }
    }

    m_currentIndex = m_siblingFiles.indexOf(info.absoluteFilePath());
    if (m_currentIndex == -1 && !m_siblingFiles.isEmpty()) {
        m_currentIndex = 0;
    }
}

void BaseSkin::startTransitionAnimation(bool forward)
{
    TransitionEffect effect = ThemeManager::currentTransition();
    if (effect == TransitionEffect::None) return;

    if (!m_stackedWidget || !m_stackedWidget->isVisible() || m_stackedWidget->width() <= 0) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSwitchTimeMs < 80) {
        m_lastSwitchTimeMs = now;
        if (m_animGroup && m_animGroup->state() == QAbstractAnimation::Running) {
            m_animGroup->stop();
        }
        if (m_transitionOverlay) m_transitionOverlay->resetOverlay();
        return;
    }
    m_lastSwitchTimeMs = now;

    if (!m_transitionOverlay || !m_animGroup) return;

    if (m_animGroup->state() == QAbstractAnimation::Running) {
        m_animGroup->stop();
    }
    m_animGroup->clear();
    m_transitionOverlay->resetOverlay();

    QPixmap oldPix = m_stackedWidget->grab();
    if (oldPix.isNull()) return;

    QRect widgetRect = m_stackedWidget->geometry();
    m_transitionOverlay->setGeometry(widgetRect);

    QColor bgColor = m_imageViewer ? m_imageViewer->canvasBackgroundColor() : palette().window().color();
    m_transitionOverlay->begin(oldPix, bgColor, effect, forward);

    auto *progressAnim = new QVariantAnimation(m_animGroup);
    int durationMs = 140;
    QEasingCurve::Type curve = QEasingCurve::OutCubic;
    if (effect == TransitionEffect::ZoomFade) {
        durationMs = 145;
        curve = QEasingCurve::OutQuad;
    } else if (effect == TransitionEffect::CrossFade) {
        durationMs = 130;
        curve = QEasingCurve::InOutQuad;
    }
    progressAnim->setDuration(durationMs);
    progressAnim->setStartValue(0.0);
    progressAnim->setEndValue(1.0);
    progressAnim->setEasingCurve(curve);
    connect(progressAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        if (m_transitionOverlay) {
            m_transitionOverlay->setProgress(val.toReal());
        }
    });
    m_animGroup->addAnimation(progressAnim);
    m_animGroup->start();
}

void BaseSkin::nextFile()
{
    // Categorically disallow sibling file navigation when in Edit Mode
    if (m_isEditMode) {
        return;
    }

    if (!m_archiveOriginPath.isEmpty() && !m_archiveEntries.isEmpty()) {
        if (m_archiveCurrentEntryIndex < m_archiveEntries.size() - 1 && m_archiveViewer) {
            startTransitionAnimation(true);
            m_archiveCurrentEntryIndex++;
            QString nextEntry = m_archiveEntries.at(m_archiveCurrentEntryIndex);
            QString extracted = m_archiveViewer->extractSingleFile(m_archiveOriginPath, nextEntry);
            if (!extracted.isEmpty() && QFile::exists(extracted)) {
                openArchiveFile(extracted, nextEntry);
            }
        }
        return;
    }

    if (m_siblingFiles.size() <= 1 || m_currentIndex < 0) return;
    startTransitionAnimation(true);
    m_currentIndex = (m_currentIndex + 1) % m_siblingFiles.size();
    openFile(m_siblingFiles[m_currentIndex]);
}

void BaseSkin::prevFile()
{
    // Categorically disallow sibling file navigation when in Edit Mode
    if (m_isEditMode) {
        return;
    }

    if (!m_archiveOriginPath.isEmpty() && !m_archiveEntries.isEmpty()) {
        if (m_archiveCurrentEntryIndex > 0 && m_archiveViewer) {
            startTransitionAnimation(false);
            m_archiveCurrentEntryIndex--;
            QString prevEntry = m_archiveEntries.at(m_archiveCurrentEntryIndex);
            QString extracted = m_archiveViewer->extractSingleFile(m_archiveOriginPath, prevEntry);
            if (!extracted.isEmpty() && QFile::exists(extracted)) {
                openArchiveFile(extracted, prevEntry);
            }
        } else if (m_archiveCurrentEntryIndex == 0) {
            // Return to archive central directory view
            QString origin = m_archiveOriginPath;
            m_archiveOriginPath.clear();
            m_archiveEntries.clear();
            m_archiveCurrentEntryIndex = -1;
            openFile(origin);
        }
        return;
    }

    if (m_siblingFiles.size() <= 1 || m_currentIndex < 0) return;
    startTransitionAnimation(false);
    m_currentIndex = (m_currentIndex - 1 + m_siblingFiles.size()) % m_siblingFiles.size();
    openFile(m_siblingFiles[m_currentIndex]);
}

void BaseSkin::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void BaseSkin::cutToClipboard()
{
    if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->cut();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->cutSelectedText();
    }
}

void BaseSkin::copyToClipboard()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        QApplication::clipboard()->setImage(m_imageViewer->originalImage());
    } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        if (m_textViewer->textCursor().hasSelection()) {
            m_textViewer->copy();
        } else {
            QApplication::clipboard()->setText(m_textViewer->toPlainText());
        }
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        if (m_pdfViewer->hasTextSelection()) {
            m_pdfViewer->copySelectedText();
        } else {
            QApplication::clipboard()->setText(m_pdfViewer->extractAllText());
        }
    } else if (m_stackedWidget->currentIndex() == 6 && m_folderViewer) {
        QString sel = m_folderViewer->selectedFilePath();
        if (!sel.isEmpty()) {
            QApplication::clipboard()->setText(sel);
        }
    }
}

void BaseSkin::pasteFromClipboard()
{
    if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->paste();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->paste();
    }
}

void BaseSkin::copyPathToClipboard()
{
    if (!m_currentFilePath.isEmpty()) {
        QApplication::clipboard()->setText(m_currentFilePath);
    }
}

void BaseSkin::openWithDefault()
{
    if (!m_currentFilePath.isEmpty() && QFile::exists(m_currentFilePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentFilePath));
    }
}

void BaseSkin::openWithOther()
{
    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) return;
    OpenWithDialog dlg(m_currentFilePath, this);
    if (dlg.exec() == QDialog::Accepted) {
        AppInfo app = dlg.selectedApp();
        if (!app.execCmd.isEmpty()) {
            AppLauncher::launchApp(app, m_currentFilePath);
        } else if (!dlg.customCommand().isEmpty()) {
            QProcess::startDetached(dlg.customCommand(), QStringList() << m_currentFilePath);
        }
    }
}

void BaseSkin::showFileInfo()
{
    QString targetPath = currentShareFilePath();
    if (targetPath.isEmpty()) {
        targetPath = m_currentFilePath;
    }
    if (targetPath.isEmpty()) return;

    if (m_stackedWidget && m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        ImageInspectorDialog dlg(targetPath, m_imageViewer->originalImage(), this);
        dlg.exec();
        return;
    }

    if (m_stackedWidget && m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->showDocumentInfo();
        return;
    }

    QFileInfo fi(targetPath);
    QString info = QString(
        "<b>" + tr("Name:") + "</b> %1<br>"
        "<b>" + tr("Path:") + "</b> %2<br>"
        "<b>" + tr("Size:") + "</b> %3<br>"
        "<b>" + tr("Last modified:") + "</b> %4<br>"
        "<b>" + tr("Permissions:") + "</b> %5<br>"
    ).arg(fi.fileName(), fi.absoluteFilePath(), formatFileSize(fi.size()),
          fi.lastModified().toString("dd/MM/yyyy HH:mm:ss"),
          QString::number(fi.permissions(), 8));

    if (m_stackedWidget && m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        info += QString("<b>" + tr("Total pages:") + "</b> %1<br>").arg(m_pdfViewer->pageCount());
    } else if (m_stackedWidget && m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        info += QString("<b>" + tr("Text lines:") + "</b> %1<br><b>" + tr("Characters:") + "</b> %2<br>")
            .arg(m_textViewer->lineCount()).arg(m_textViewer->charCount());
    }

    QMessageBox::information(this, tr("File Metadata & Properties"), info);
}

void BaseSkin::searchInDocument()
{
    if (m_stackedWidget->currentIndex() == 1 || m_stackedWidget->currentIndex() == 2 || m_stackedWidget->currentIndex() == 3) {
        if (m_searchBar) {
            repositionSearchBar();
            m_searchBar->showAndFocus();
            m_searchBar->raise();
        }
    } else {
        QMessageBox::information(this, tr("Find in Document"), tr("In-document text search is available for Text, PDF, and Archive documents."));
    }
}

void BaseSkin::showFindReplace()
{
    if (m_stackedWidget->currentIndex() == 1) { // Text viewer
        if (m_searchBar) {
            m_searchBar->showReplaceAndFocus();
            repositionSearchBar();
            m_searchBar->raise();
        }
    } else {
        searchInDocument();
    }
}

void BaseSkin::gotoLineInDocument()
{
    if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->gotoLineDialog();
    }
}

void BaseSkin::exportCurrentFileAs()
{
    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) return;

    int curPage = 0;
    int totalPages = 1;
    if (m_currentCategory == FileCategory::Pdf && m_pdfViewer) {
        curPage = m_pdfViewer->currentPage();
        totalPages = m_pdfViewer->pageCount();
    }

    UnifiedExportDialog dlg(m_currentCategory, m_currentFilePath, curPage, totalPages, this);
    int res = dlg.exec();
    if (res == UnifiedExportDialog::ExtractImages) {
        extractPdfImages(m_currentFilePath);
        return;
    }
    if (res != QDialog::Accepted) {
        return;
    }

    QString targetPath = dlg.targetPath();
    if (targetPath.isEmpty()) return;

    QString ext = dlg.selectedFormat();
    bool allPages = dlg.isAllPages();
    bool success = false;

    // Dispatch export transformation based on active viewer category and requested destination extension
    if (m_currentCategory == FileCategory::Image && m_imageViewer) {
        if (ext == "pdf") {
            // Render bitmap directly into PDF canvas with crisp pixel coordinates
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(targetPath);
            QPainter painter(&printer);
            QRect targetRect = printer.pageRect(QPrinter::DevicePixel).toRect();
            QImage scaled = m_imageViewer->originalImage().scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int x = targetRect.x() + (targetRect.width() - scaled.width()) / 2;
            int y = targetRect.y() + (targetRect.height() - scaled.height()) / 2;
            painter.drawImage(x, y, scaled);
            painter.end();
            success = true;
        } else {
            success = m_imageViewer->originalImage().save(targetPath);
        }
    } else if (m_currentCategory == FileCategory::Text && m_textViewer) {
        if (ext == "pdf") {
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(targetPath);
            m_textViewer->print(&printer);
            success = true;
        } else {
            QFile outFile(targetPath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&outFile);
                out << m_textViewer->toPlainText();
                outFile.close();
                success = true;
            }
        }
    } else if (m_currentCategory == FileCategory::Pdf && m_pdfViewer) {
        if (ext == "txt") {
            QFile outFile(targetPath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&outFile);
                out << m_pdfViewer->extractAllText();
                outFile.close();
                success = true;
            }
        } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "tiff" || ext == "bmp") {
            if (!allPages) {
                QImage pageImg = m_pdfViewer->renderPageForOcr(curPage, 300);
                if (!pageImg.isNull()) {
                    success = pageImg.save(targetPath);
                }
            } else {
                QFileInfo targetFi(targetPath);
                QString baseDir = targetFi.dir().absolutePath();
                QString baseStem = targetFi.completeBaseName();
                int exportedCount = 0;

                for (int p = 0; p < totalPages; ++p) {
                    QImage pageImg = m_pdfViewer->renderPageForOcr(p, 300);
                    if (!pageImg.isNull()) {
                        QString pageFilePath = QString("%1/%2_%3.%4")
                                                   .arg(baseDir)
                                                   .arg(baseStem)
                                                   .arg(p + 1, 2, 10, QChar('0'))
                                                   .arg(ext);
                        if (pageImg.save(pageFilePath)) {
                            exportedCount++;
                        }
                    }
                }
                success = (exportedCount > 0);
            }
        } else if (ext == "pdf") {
            if (!allPages) {
                QString err;
                success = m_pdfViewer->saveCurrentPageAsPdf(targetPath, &err);
            } else {
                success = m_pdfViewer->saveAs(targetPath);
            }
        } else {
            if (QFile::exists(targetPath)) QFile::remove(targetPath);
            success = QFile::copy(m_currentFilePath, targetPath);
        }
    } else if (m_currentCategory == FileCategory::Office && m_officeViewer) {
        if (ext == "pdf") {
            if (m_officeViewer->isPdfMode() && !m_officeViewer->convertedPdfPath().isEmpty() && QFile::exists(m_officeViewer->convertedPdfPath())) {
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                success = QFile::copy(m_officeViewer->convertedPdfPath(), targetPath);
            } else if (m_officeViewer->textBrowser()) {
                QPrinter printer(QPrinter::HighResolution);
                printer.setOutputFormat(QPrinter::PdfFormat);
                printer.setOutputFileName(targetPath);
                m_officeViewer->textBrowser()->print(&printer);
                success = true;
            }
        } else if (ext == "txt") {
            QFile outFile(targetPath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&outFile);
                if (m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
                    out << m_officeViewer->pdfViewer()->extractAllText();
                } else if (m_officeViewer->textBrowser()) {
                    out << m_officeViewer->textBrowser()->toPlainText();
                }
                outFile.close();
                success = true;
            }
        } else if (ext == "html" && !m_officeViewer->isPdfMode() && m_officeViewer->textBrowser()) {
            QFile outFile(targetPath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&outFile);
                out << m_officeViewer->textBrowser()->toHtml();
                outFile.close();
                success = true;
            }
        } else {
            if (QFile::exists(targetPath)) QFile::remove(targetPath);
            success = QFile::copy(m_currentFilePath, targetPath);
        }
    } else {
        if (QFile::exists(targetPath)) QFile::remove(targetPath);
        success = QFile::copy(m_currentFilePath, targetPath);
    }

    if (success) {
        QMessageBox::information(this, tr("Export Successful"),
            tr("File exported successfully to:\n%1").arg(targetPath));
    } else {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Failed to export file to:\n%1").arg(targetPath));
    }
}

void BaseSkin::deleteCurrentFile()
{
    // Folder gallery has its own item-selection deletion handling
    if (m_currentCategory == FileCategory::Folder) {
        deleteCurrentFolderItem();
        return;
    }

    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) return;

    QFileInfo fi(m_currentFilePath);
    auto res = QMessageBox::question(this, tr("Delete File"),
                                     tr("Are you sure you want to move '%1' to trash?").arg(fi.fileName()),
                                     QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    QString fileToDelete = m_currentFilePath;
    int indexToRemove = m_currentIndex;

    // Utilize FreeDesktop trash spec via QFile::moveToTrash for safe recovery
    if (QFile::moveToTrash(fileToDelete)) {
        PreloadCacheManager::instance().remove(fileToDelete);
        if (!m_siblingFiles.isEmpty() && indexToRemove >= 0 && indexToRemove < m_siblingFiles.size()) {
            m_siblingFiles.removeAt(indexToRemove);
            if (m_siblingFiles.isEmpty()) {
                m_currentIndex = -1;
                m_currentFilePath.clear();
                updateWindowTitle();
                updateStatusBar();
                if (m_imageViewer) m_imageViewer->clear();
                if (m_textViewer) m_textViewer->clear();
                if (m_pdfViewer) m_pdfViewer->clear();
                if (m_archiveViewer) m_archiveViewer->clear();
                if (m_mediaViewer) m_mediaViewer->clear();
                if (m_officeViewer) m_officeViewer->clear();
                if (m_folderViewer) m_folderViewer->clear();
                if (m_filmstrip) {
                    m_filmstrip->clear();
                    m_filmstrip->hide();
                }
            } else {
                // Navigate seamlessly to adjacent sibling file
                if (indexToRemove >= m_siblingFiles.size()) {
                    indexToRemove = m_siblingFiles.size() - 1;
                }
                m_currentIndex = indexToRemove;
                openFile(m_siblingFiles.at(m_currentIndex));
            }
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Unable to move file to trash."));
    }
}


void BaseSkin::extractOcrCurrentFile()
{
    if (!OcrManager::instance().isTesseractInstalled()) {
        if (!PackageManagerHelper::installComponentInteractive("tesseract", this)) {
            return;
        }
        if (!OcrManager::instance().isTesseractInstalled()) {
            return;
        }
    } else {
        // If High-Performance profile is active and system tesseract is used, notify user
        QString exePath = OcrManager::instance().tesseractExecutablePath();
        FallbackNoticeDialog::checkAndPrompt("tesseract", tr("Tesseract OCR"), exePath, this);
    }

    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) return;

    // Ensure system locale language and English are available (download without root if missing)
    QString sysLang = OcrManager::instance().detectSystemLanguageCode();
    if (!OcrManager::instance().isLanguageInstalled(sysLang)) {
        QProgressDialog progress(tr("Downloading OCR language model for '%1' from repository...").arg(sysLang),
                                 tr("Cancel"), 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        QString err;
        OcrManager::instance().downloadLanguageBlocking(sysLang, [&](qint64 rec, qint64 tot) {
            if (tot > 0) progress.setValue(static_cast<int>((rec * 100) / tot));
            QApplication::processEvents();
        }, &err);
        progress.close();
    }

    if (sysLang != "eng" && !OcrManager::instance().isLanguageInstalled("eng")) {
        QProgressDialog progress(tr("Downloading English OCR model (eng)..."), tr("Cancel"), 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        QString err;
        OcrManager::instance().downloadLanguageBlocking("eng", [&](qint64 rec, qint64 tot) {
            if (tot > 0) progress.setValue(static_cast<int>((rec * 100) / tot));
            QApplication::processEvents();
        }, &err);
        progress.close();
    }

    const QStringList installedLangs = OcrManager::instance().installedLanguages();
    QString chosenLang;
    if (installedLangs.contains("ita") && installedLangs.contains("eng")) {
        chosenLang = "ita+eng";
    } else if (installedLangs.contains(sysLang)) {
        chosenLang = sysLang;
    } else if (installedLangs.contains("eng")) {
        chosenLang = "eng";
    } else if (!installedLangs.isEmpty()) {
        chosenLang = installedLangs.first();
    } else {
        chosenLang = "eng";
    }

    // Prepare input image file:
    QString inputImagePath;
    std::unique_ptr<QTemporaryFile> tempFile;
    bool isPdf = false;
    int currentPage = 0;
    int totalPages = 1;
    std::function<QImage(int)> pageRenderer = nullptr;

    if (m_currentCategory == FileCategory::Pdf && m_pdfViewer) {
        isPdf = true;
        currentPage = m_pdfViewer->currentPage();
        totalPages = m_pdfViewer->pageCount();
        pageRenderer = [this](int pageIdx) -> QImage {
            return m_pdfViewer ? m_pdfViewer->renderPageForOcr(pageIdx, 300) : QImage();
        };

        QImage pageImg = m_pdfViewer->renderCurrentPageForOcr(300);
        if (pageImg.isNull()) {
            QMessageBox::warning(this, tr("OCR"), tr("Unable to render PDF page for OCR."));
            return;
        }
        tempFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/bubez_ocr_XXXXXX.png");
        tempFile->setAutoRemove(true);
        if (tempFile->open()) {
            pageImg.save(tempFile->fileName(), "PNG");
            inputImagePath = tempFile->fileName();
            tempFile->close();
        }
    } else if (m_currentCategory == FileCategory::Image) {
        if (m_imageViewer && m_imageViewer->isModified()) {
            tempFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/bubez_ocr_XXXXXX.png");
            tempFile->setAutoRemove(true);
            if (tempFile->open()) {
                m_imageViewer->originalImage().save(tempFile->fileName(), "PNG");
                inputImagePath = tempFile->fileName();
                tempFile->close();
            }
        } else {
            inputImagePath = m_currentFilePath;
        }
    } else {
        inputImagePath = m_currentFilePath;
    }

    if (inputImagePath.isEmpty()) {
        QMessageBox::warning(this, tr("OCR"), tr("Unable to prepare input image for OCR."));
        return;
    }

    // Run OCR on active page/image
    QString text, err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = OcrManager::instance().runOcr(inputImagePath, chosenLang, text, err);
    QApplication::restoreOverrideCursor();

    if (!ok && text.isEmpty()) {
        QMessageBox::warning(this, tr("OCR Tesseract"),
            tr("Tesseract encountered an error:<br><code>%1</code>").arg(err.toHtmlEscaped()));
        return;
    }

    // Open dedicated OCR inspection dialog modelessly to prevent modal deadlock during area snipping
    OcrDialog *dlg = new OcrDialog(text,
                                   QFileInfo(m_currentFilePath).fileName(),
                                   inputImagePath,
                                   isPdf,
                                   currentPage,
                                   totalPages,
                                   pageRenderer,
                                   this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void BaseSkin::extractOcrSnippet(const QImage &snippetImage)
{
    if (snippetImage.isNull() || snippetImage.width() < 4 || snippetImage.height() < 4) return;

    if (!OcrManager::instance().isTesseractInstalled()) {
        if (!PackageManagerHelper::installComponentInteractive("tesseract", this)) {
            return;
        }
        if (!OcrManager::instance().isTesseractInstalled()) {
            return;
        }
    }

    auto tempFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/bubez_ocr_snippet_XXXXXX.png");
    tempFile->setAutoRemove(false);
    if (!tempFile->open()) return;
    QString snippetPath = tempFile->fileName();
    snippetImage.save(snippetPath, "PNG");
    tempFile->close();

    const QStringList installedLangs = OcrManager::instance().installedLanguages();
    QString sysLang = OcrManager::instance().detectSystemLanguageCode();
    QString chosenLang;
    if (installedLangs.contains("ita") && installedLangs.contains("eng")) {
        chosenLang = "ita+eng";
    } else if (installedLangs.contains(sysLang)) {
        chosenLang = sysLang;
    } else if (installedLangs.contains("eng")) {
        chosenLang = "eng";
    } else if (!installedLangs.isEmpty()) {
        chosenLang = installedLangs.first();
    } else {
        chosenLang = "eng";
    }

    QString text, err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = OcrManager::instance().runOcr(snippetPath, chosenLang, text, err);
    QApplication::restoreOverrideCursor();

    if (!ok && text.isEmpty()) {
        QMessageBox::warning(this, tr("OCR Tesseract"),
            tr("Tesseract encountered an error on snippet:<br><code>%1</code>").arg(err.toHtmlEscaped()));
        QFile::remove(snippetPath);
        return;
    }

    QString trimmedText = text.trimmed();
    QApplication::clipboard()->setText(trimmedText);
    int wordCount = trimmedText.isEmpty() ? 0 : trimmedText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    QToolTip::showText(QCursor::pos(), tr("✓ OCR Text copied to clipboard (%1 words)").arg(wordCount));

    OcrDialog *dlg = new OcrDialog(trimmedText,
                                   QFileInfo(m_currentFilePath).fileName() + tr(" (Area)"),
                                   snippetPath,
                                   false,
                                   0,
                                   1,
                                   nullptr,
                                   this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

bool BaseSkin::saveCurrentImage(bool silent)
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified()) {
        if (m_currentFilePath.isEmpty()) {
            if (!silent) {
                QMessageBox::warning(this, tr("Save Error"), tr("No active image file to save."));
            }
            return false;
        }

        QFileInfo fi(m_currentFilePath);
        QByteArray ext = fi.suffix().toLower().toUtf8();
        const char *fmt = ext.isEmpty() ? nullptr : ext.constData();
        int quality = (ext == "jpg" || ext == "jpeg") ? 95 : -1;

        if (m_imageViewer->originalImage().save(m_currentFilePath, fmt, quality)) {
            m_imageViewer->setModified(false);

            // Invalidate/refresh PreloadCacheManager so cache stays in sync
            PreloadCacheManager::instance().remove(m_currentFilePath);
            PreloadCacheManager::instance().insert(m_currentFilePath, m_imageViewer->originalImage());

            // Update filmstrip and folder gallery thumbnails
            if (m_filmstrip) {
                m_filmstrip->updateThumbnail(m_currentFilePath, m_imageViewer->originalImage());
            }
            if (m_folderViewer) {
                m_folderViewer->updateThumbnail(m_currentFilePath, m_imageViewer->originalImage());
            }
            updateWindowTitle();
            updateStatusBar();
            if (!silent) {
                QMessageBox::information(this, tr("Save"), tr("Image modifications saved successfully!"));
            } else if (statusBar()) {
                statusBar()->showMessage(tr("Image rotated and saved"), 2500);
            }
            return true;
        } else {
            if (!silent) {
                QMessageBox::warning(this, tr("Save Error"), tr("Failed to save image changes to:\n%1\n\nPlease check write permissions or disk space.").arg(m_currentFilePath));
            }
            return false;
        }
    }
    return false;
}

void BaseSkin::saveCurrentFile()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified()) {
        saveCurrentImage(false);
    } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer && m_textViewer->document()->isModified()) {
        if (m_textViewer->saveFile()) {
            QMessageBox::information(this, tr("Save"), tr("Text file saved successfully!"));
        } else {
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save changes to text file."));
        }
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer && m_pdfViewer->isModified()) {
        if (m_pdfViewer->save()) {
            QMessageBox::information(this, tr("Save"), tr("PDF modifications saved successfully!"));
        } else {
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save modifications to PDF."));
        }
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::saveAsCurrentFile()
{
    if (m_currentFilePath.isEmpty()) return;
    QFileInfo fi(m_currentFilePath);
    QString target = QFileDialog::getSaveFileName(this, tr("Save As"), fi.dir().filePath(tr("copy_") + fi.fileName()));
    if (!target.isEmpty()) {
        bool saved = false;
        if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
            QFileInfo targetFi(target);
            QByteArray ext = targetFi.suffix().toLower().toUtf8();
            const char *fmt = ext.isEmpty() ? nullptr : ext.constData();
            int quality = (ext == "jpg" || ext == "jpeg") ? 95 : -1;
            if (m_imageViewer->originalImage().save(target, fmt, quality)) {
                m_imageViewer->setModified(false);
                PreloadCacheManager::instance().remove(target);
                PreloadCacheManager::instance().insert(target, m_imageViewer->originalImage());
                saved = true;
            } else {
                QMessageBox::warning(this, tr("Save As Error"), tr("Failed to save image to:\n%1").arg(target));
            }
        } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
            if (m_textViewer->saveFile(target)) {
                saved = true;
            } else {
                QMessageBox::warning(this, tr("Save As Error"), tr("Failed to save text file to:\n%1").arg(target));
            }
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            if (m_pdfViewer->saveAs(target)) {
                saved = true;
            } else {
                QMessageBox::warning(this, tr("Save As Error"), tr("Failed to save PDF to:\n%1").arg(target));
            }
        } else {
            if (QFile::copy(m_currentFilePath, target)) {
                saved = true;
            } else {
                QMessageBox::warning(this, tr("Save As Error"), tr("Failed to copy file to:\n%1").arg(target));
            }
        }

        if (saved) {
            m_currentFilePath = target;
            scanDirectory(target);
            if (m_filmstrip) {
                m_filmstrip->setFiles(m_siblingFiles, m_currentIndex);
            }
            if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) m_imageViewer->setModified(false);
            if (m_stackedWidget->currentIndex() == 1 && m_textViewer) m_textViewer->document()->setModified(false);
            if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) m_pdfViewer->setModified(false);
            updateWindowTitle();
            updateStatusBar();
            QMessageBox::information(this, tr("Save As"), tr("File saved successfully to:\n%1").arg(target));
        }
    }
}

void BaseSkin::saveCurrentPageAsPdf()
{
    if (m_currentCategory != FileCategory::Pdf || !m_pdfViewer) return;

    QFileInfo fi(m_currentFilePath);
    int curPage = m_pdfViewer->currentPage() + 1;
    QString defaultName = fi.dir().filePath(QString("%1_p%2.pdf").arg(fi.completeBaseName()).arg(curPage));

    QString target = QFileDialog::getSaveFileName(this, tr("Save Current Page as PDF"), defaultName, tr("PDF Document (*.pdf)"));
    if (target.isEmpty()) return;

    if (!target.endsWith(".pdf", Qt::CaseInsensitive)) {
        target += ".pdf";
    }

    QString err;
    if (m_pdfViewer->saveCurrentPageAsPdf(target, &err)) {
        QMessageBox::information(this, tr("Page Saved"), tr("Current page successfully saved to:\n%1").arg(target));
    } else {
        QMessageBox::warning(this, tr("Error Saving Page"), tr("Failed to save current page as PDF:\n%1").arg(err));
    }
}

void BaseSkin::saveCurrentPageAsImage()
{
    if (m_currentCategory != FileCategory::Pdf || !m_pdfViewer) return;

    QFileInfo fi(m_currentFilePath);
    int curPage = m_pdfViewer->currentPage() + 1;
    QString defaultName = fi.dir().filePath(QString("%1_p%2.png").arg(fi.completeBaseName()).arg(curPage));

    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp);;All Files (*)");
    QString selectedFilter;
    QString target = QFileDialog::getSaveFileName(this, tr("Save Current Page as Image"), defaultName, filter, &selectedFilter);
    if (target.isEmpty()) return;

    QImage img = m_pdfViewer->renderPageForOcr(curPage - 1, 300);
    if (!img.isNull() && img.save(target)) {
        QMessageBox::information(this, tr("Page Saved"), tr("Current page successfully saved to:\n%1").arg(target));
    } else {
        QMessageBox::warning(this, tr("Error Saving Page"), tr("Failed to save current page as image."));
    }
}

void BaseSkin::printCurrentFile()
{
    if (m_currentFilePath.isEmpty()) return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print Document"));
    if (dlg.exec() == QDialog::Accepted) {
        if (m_stackedWidget->currentIndex() == 0) {
            QPainter painter(&printer);
            painter.drawImage(printer.pageRect(QPrinter::DevicePixel).toRect(), m_imageViewer->originalImage());
        } else if (m_stackedWidget->currentIndex() == 1) {
            m_textViewer->print(&printer);
        }
    }
}

void BaseSkin::openFileDialog()
{
    QString startDir = m_currentFilePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentFilePath).dir().absolutePath();
    QString filter = tr("All Supported Files (*.png *.jpg *.jpeg *.webp *.svg *.bmp *.gif *.ico *.tiff *.tif *.avif "
                        "*.pdf *.epub *.docx *.odt *.ods *.xlsx *.xltx *.xlsm *.doc *.xls *.rtf "
                        "*.txt *.md *.csv *.json *.xml *.py *.cpp *.h *.c *.js *.ts *.html *.css *.sh "
                        "*.zip *.tar *.gz *.bz2 *.xz *.7z *.rar *.iso "
                        "*.mp4 *.mkv *.avi *.webm *.mov *.mp3 *.ogg *.flac *.wav *.m4a);;"
                        "All Files (*)");
    QString chosen = QFileDialog::getOpenFileName(this, tr("Open File"), startDir, filter);
    if (!chosen.isEmpty()) {
        openFile(chosen);
    }
    activateWindow();
    raise();
}

void BaseSkin::openFolderDialog()
{
    QString startDir = m_currentFilePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentFilePath).dir().absolutePath();
    QString chosen = QFileDialog::getExistingDirectory(this, tr("Open Folder"), startDir);
    if (!chosen.isEmpty()) {
        openFile(chosen);
    }
    activateWindow();
    raise();
}

void BaseSkin::setDaemonMode(bool daemon)
{
    m_isDaemonMode = daemon;
}

bool BaseSkin::isDaemonMode() const
{
    return m_isDaemonMode;
}

bool BaseSkin::promptAndSaveIfModified()
{
    bool isImgMod = (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified());
    bool isTextMod = (m_stackedWidget->currentIndex() == 1 && m_textViewer && m_textViewer->document()->isModified());
    bool isPdfMod = (m_stackedWidget->currentIndex() == 2 && m_pdfViewer && m_pdfViewer->isModified());

    if (!isImgMod && !isTextMod && !isPdfMod) {
        return true;
    }

    QString docName = QFileInfo(m_currentFilePath).fileName();
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Unsaved Changes"));
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText(tr("The document \"%1\" has unsaved changes.\nDo you want to save them before proceeding?").arg(docName));

    QPushButton *btnSave = msgBox.addButton(tr("Save"), QMessageBox::AcceptRole);
    QPushButton *btnDiscard = msgBox.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(btnSave);

    msgBox.exec();

    QAbstractButton *clicked = msgBox.clickedButton();

    if (clicked == btnSave) {
        if (isImgMod) {
            if (!m_currentFilePath.isEmpty()) {
                QFileInfo fi(m_currentFilePath);
                QByteArray ext = fi.suffix().toLower().toUtf8();
                const char *fmt = ext.isEmpty() ? nullptr : ext.constData();
                int quality = (ext == "jpg" || ext == "jpeg") ? 95 : -1;

                if (m_imageViewer->originalImage().save(m_currentFilePath, fmt, quality)) {
                    m_imageViewer->setModified(false);
                    PreloadCacheManager::instance().remove(m_currentFilePath);
                    PreloadCacheManager::instance().insert(m_currentFilePath, m_imageViewer->originalImage());
                    if (m_filmstrip) {
                        m_filmstrip->updateThumbnail(m_currentFilePath, m_imageViewer->originalImage());
                    }
                    if (m_folderViewer) {
                        m_folderViewer->updateThumbnail(m_currentFilePath, m_imageViewer->originalImage());
                    }
                    return true;
                }
            }
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save image changes."));
            return false;
        } else if (isTextMod) {
            if (m_textViewer->saveFile()) {
                return true;
            } else {
                QMessageBox::warning(this, tr("Save Error"), tr("Failed to save text file changes."));
                return false;
            }
        } else if (isPdfMod) {
            if (m_pdfViewer->save()) {
                return true;
            } else {
                QMessageBox::warning(this, tr("Save Error"), tr("Failed to save PDF changes."));
                return false;
            }
        }
    } else if (clicked == btnDiscard) {
        if (isImgMod) {
            m_imageViewer->loadFile(m_currentFilePath);
            m_imageViewer->setModified(false);
        }
        if (isTextMod) {
            m_textViewer->loadFile(m_currentFilePath);
            m_textViewer->document()->setModified(false);
        }
        if (isPdfMod) {
            m_pdfViewer->loadFile(m_currentFilePath);
            m_pdfViewer->setModified(false);
        }
        return true;
    }

    // Cancel
    return false;
}

void BaseSkin::repositionSearchBar()
{
    if (m_searchBar && m_centralContainer) {
        m_searchBar->adjustSize();
        int x = m_centralContainer->width() - m_searchBar->width() - 24;
        int y = 14;
        m_searchBar->move(qMax(10, x), y);
        m_searchBar->raise();
    }
}

void BaseSkin::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionSearchBar();
    if (m_transitionOverlay && m_stackedWidget && (!m_animGroup || m_animGroup->state() != QAbstractAnimation::Running)) {
        m_transitionOverlay->setGeometry(m_stackedWidget->geometry());
    }
    if (m_galleryTransition && m_stackedWidget && !m_galleryTransition->isRunning()) {
        m_galleryTransition->setGeometry(m_stackedWidget->geometry());
    }

    if (m_geometrySaveTimer && isVisible() && !isMinimized()) {
        m_geometrySaveTimer->start(350);
    }
}

void BaseSkin::closeEvent(QCloseEvent *event)
{
    if (m_isEditMode && !promptAndSaveIfModified()) {
        event->ignore();
        return;
    }

    if (m_geometrySaveTimer && m_geometrySaveTimer->isActive()) {
        m_geometrySaveTimer->stop();
    }
    saveWindowGeometry();

    // 1. Immediately stop media playback and release audio hardware locks
    if (m_mediaViewer) {
        m_mediaViewer->stop();
        if (m_mediaViewer->isBitPerfect() || m_mediaViewer->currentAudioDevice().startsWith("alsa/")) {
            m_mediaViewer->resetMixer();
        }
    }

    QMainWindow::closeEvent(event);
    if (!event->isAccepted()) return;

    QSettings settings("Bubez", "BubezView");
    int keepAliveMin = settings.value("background_keep_alive_minutes", 0).toInt();

    if (keepAliveMin != 0) {
        event->ignore();
        hide();
        m_isDaemonMode = true;

        if (keepAliveMin > 0) {
            if (!m_backgroundTimer) {
                m_backgroundTimer = new QTimer(this);
                m_backgroundTimer->setSingleShot(true);
                connect(m_backgroundTimer, &QTimer::timeout, this, []() {
                    qApp->quit();
                });
            }
            m_backgroundTimer->start(keepAliveMin * 60 * 1000);
        } else if (m_backgroundTimer && m_backgroundTimer->isActive()) {
            m_backgroundTimer->stop();
        }
    } else {
        if (!m_isDaemonMode) {
            // Ensure process exits completely and does not leave zombie background processes
            qApp->quit();
        }
    }
}

void BaseSkin::saveWindowGeometry()
{
    // Do not save geometry while window is minimized
    if (isMinimized()) {
        return;
    }

    // Do not overwrite normal preview geometry when temporarily in edit mode or page organizer
    if (m_isEditMode || (m_pdfViewer && m_pdfViewer->isPageGridOrganizerActive())) {
        return;
    }

    QSettings settings("Bubez", "BubezView");
    if (!settings.value("remember_geometry", false).toBool()) {
        return;
    }

    // Save full Qt geometry state (preserves screen position on X11, maximized state, etc.)
    settings.setValue("window/geometry", saveGeometry());

    // Also persist width & height separately to ensure accurate window dimensions on Wayland compositors
    const QRect r = (isMaximized() || isFullScreen()) ? normalGeometry() : geometry();
    if (r.isValid() && r.width() >= 320 && r.height() >= 240) {
        settings.setValue("window/width", r.width());
        settings.setValue("window/height", r.height());
    }
    settings.sync();
}

void BaseSkin::restoreWindowGeometry()
{
    // Default initial preview dimensions (compact Quick Look style)
    const QSize defaultSize(780, 540);

    QSettings settings("Bubez", "BubezView");
    const bool remember = settings.value("remember_geometry", false).toBool();

    bool restored = false;
    if (remember) {
        if (settings.contains("window/geometry")) {
            const QByteArray geom = settings.value("window/geometry").toByteArray();
            if (!geom.isEmpty() && restoreGeometry(geom)) {
                restored = true;
            }
        }

        // On Wayland compositors or if restoreGeometry didn't apply sizing, enforce saved width and height
        if (settings.contains("window/width") && settings.contains("window/height")) {
            int w = settings.value("window/width").toInt();
            int h = settings.value("window/height").toInt();
            if (w >= 320 && h >= 240) {
                resize(w, h);
                restored = true;
            }
        }
    }

    if (!restored) {
        resize(defaultSize);
    }

    // In preview mode, ensure window starts in normal compact mode (not maximized or fullscreen)
    if (!m_isEditMode && (isMaximized() || isFullScreen())) {
        showNormal();
    }

    // Screen boundary safety: ensure window fits within the active screen geometry
    QScreen *targetScreen = screen();
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (targetScreen) {
        const QRect avail = targetScreen->availableGeometry();
        if (avail.isValid() && avail.width() > 0 && avail.height() > 0) {
            int maxW = avail.width();
            int maxH = avail.height();
            int targetW = qMin(width(), maxW);
            int targetH = qMin(height(), maxH);
            if (targetW != width() || targetH != height()) {
                resize(targetW, targetH);
            }

            // Center on screen if newly launched or if outside visible screen bounds
            if (!restored || !avail.intersects(frameGeometry())) {
                move(avail.center().x() - width() / 2, avail.center().y() - height() / 2);
            }
        }
    }
}


QString BaseSkin::currentShareFilePath() const
{
    if (m_currentCategory == FileCategory::Folder && m_folderViewer) {
        QString sel = m_folderViewer->selectedFilePath();
        if (!sel.isEmpty() && QFile::exists(sel)) {
            return sel;
        }
    }
    return m_currentFilePath;
}

void BaseSkin::showShareDialog()
{
    QString path = currentShareFilePath();
    if (!path.isEmpty()) {
        ShareDialog dlg(path, this);
        dlg.exec();
    }
}

void BaseSkin::openSettings()
{
    SettingsDialog dlg(this, SettingsDialog::TabIndex::DesktopIntegration);
    connect(&dlg, &SettingsDialog::themeChanged, this, &BaseSkin::applyCurrentTheme);
    dlg.exec();
}

void BaseSkin::openAppearanceSettings()
{
    SettingsDialog dlg(this, SettingsDialog::TabIndex::Appearance);
    connect(&dlg, &SettingsDialog::themeChanged, this, &BaseSkin::applyCurrentTheme);
    if (dlg.exec() == QDialog::Accepted) {
        if (m_stackedWidget->currentIndex() == 5 && !m_currentFilePath.isEmpty()) {
            openFile(m_currentFilePath);
        }
    }
}

void BaseSkin::applyCurrentTheme()
{
    ThemeManager::applyTheme(qobject_cast<QApplication*>(QApplication::instance()), this);
    if (m_imageViewer) m_imageViewer->updateTheme();
    if (m_archiveViewer) m_archiveViewer->updateTheme();
    if (m_folderViewer) m_folderViewer->updateTheme();
    if (m_pdfViewer) m_pdfViewer->updateTheme();
    if (m_textViewer) m_textViewer->updateTheme();
    if (m_officeViewer) m_officeViewer->updateTheme();
    if (m_mediaViewer) m_mediaViewer->updateTheme();
}

QString BaseSkin::fileIndexText() const
{
    if (!m_archiveOriginPath.isEmpty() && !m_archiveEntries.isEmpty() && m_archiveCurrentEntryIndex >= 0) {
        return QString("[%1/%2]").arg(m_archiveCurrentEntryIndex + 1).arg(m_archiveEntries.size());
    }
    if (m_siblingFiles.isEmpty() || m_currentIndex < 0) return QString();
    return QString("[%1/%2]").arg(m_currentIndex + 1).arg(m_siblingFiles.size());
}

/**
 * @brief Computes a standardized human-readable aspect ratio string from pixel dimensions.
 *
 * Checks exact integer ratios using std::gcd (standardizing 8:5 to industry-standard 16:10),
 * and matches close photography/cinematic display ratios (e.g. 16:9, 4:3, 3:2, 21:9)
 * within a small tolerance for slightly cropped images.
 *
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @return Formatted aspect ratio string (e.g. "16:9", "4:3", "3:2"), or empty string if invalid.
 */
static QString formatAspectRatio(int w, int h)
{
    if (w <= 0 || h <= 0) {
        return QString();
    }

    // Check exact integer ratio via greatest common divisor
    const int g = std::gcd(w, h);
    const int rw = w / g;
    const int rh = h / g;

    // Check if directly reducible to standard small integer ratio (<= 25)
    if (rw <= 25 && rh <= 25) {
        // Standardize 8:5 to industry-standard 16:10 display ratio
        if (rw == 8 && rh == 5) return QStringLiteral("16:10");
        if (rw == 5 && rh == 8) return QStringLiteral("10:16");
        return QString("%1:%2").arg(rw).arg(rh);
    }

    // Handle common ultra-wide / cinema or slight sensor crop ratios
    const double ratio = static_cast<double>(w) / static_cast<double>(h);
    static const struct { double val; const char *name; } s_standards[] = {
        {1.0,           "1:1"},
        {4.0 / 3.0,     "4:3"},
        {3.0 / 4.0,     "3:4"},
        {3.0 / 2.0,     "3:2"},
        {2.0 / 3.0,     "2:3"},
        {16.0 / 9.0,    "16:9"},
        {9.0 / 16.0,    "9:16"},
        {16.0 / 10.0,   "16:10"},
        {10.0 / 16.0,   "10:16"},
        {5.0 / 4.0,     "5:4"},
        {4.0 / 5.0,     "4:5"},
        {64.0 / 27.0,   "21:9"},
        {27.0 / 64.0,   "9:21"},
        {43.0 / 18.0,   "21:9"},
        {18.0 / 43.0,   "9:21"}
    };

    for (const auto &stdRatio : s_standards) {
        if (std::abs(ratio - stdRatio.val) < 0.02) {
            return QString::fromLatin1(stdRatio.name);
        }
    }

    // Fallback: decimal ratio format (e.g. "2.35:1")
    return QString("%1:1").arg(ratio, 0, 'f', 2);
}

QString BaseSkin::fileInfoText() const
{
    if (m_currentFilePath.isEmpty()) return QString();
    QFileInfo info(m_currentFilePath);

    int idx = m_stackedWidget->currentIndex();
    if (idx == 0) { // Image
        QSize sz = m_imageViewer->imageSize();
        int zoomPercent = qRound(m_imageViewer->zoomFactor() * 100.0);
        int w = sz.width();
        int h = sz.height();

        QString dimStr;
        if (w > 0 && h > 0) {
            double mp = (static_cast<double>(w) * h) / 1000000.0;
            QString mpStr = (mp >= 0.1) ? QString("%1 MP").arg(mp, 0, 'f', 1) : QStringLiteral("<0.1 MP");
            QString aspectStr = formatAspectRatio(w, h);
            if (!aspectStr.isEmpty()) {
                dimStr = QString("%1x%2 px (%3 • %4)").arg(w).arg(h).arg(mpStr, aspectStr);
            } else {
                dimStr = QString("%1x%2 px (%3)").arg(w).arg(h).arg(mpStr);
            }
        } else {
            dimStr = QString("%1x%2 px").arg(w).arg(h);
        }

        return QString("%1 • %2 • %3%")
            .arg(dimStr)
            .arg(formatFileSize(info.size()))
            .arg(zoomPercent);
    } else if (idx == 1) { // Text
        return tr("%1 lines • %2 words • %3 characters • %4")
            .arg(m_textViewer->lineCount())
            .arg(m_textViewer->wordCount())
            .arg(m_textViewer->charCount())
            .arg(formatFileSize(info.size()));
    } else if (idx == 2) { // PDF
        int cur = m_pdfViewer->currentPage() + 1;
        int total = m_pdfViewer->pageCount();
        return tr("Page %1 of %2 • %3")
            .arg(cur).arg(total)
            .arg(formatFileSize(info.size()));
    } else if (idx == 3) { // Archive
        return tr("📦 %1 items • Uncompressed: %2 • Archive size: %3")
            .arg(m_archiveViewer->entryCount())
            .arg(formatFileSize(m_archiveViewer->totalUncompressedSize()))
            .arg(formatFileSize(info.size()));
    } else if (idx == 4) { // Media
        return tr("Media • %1").arg(formatFileSize(info.size()));
    } else if (idx == 5 && m_officeViewer) { // Office
        if (m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
            int cur = m_officeViewer->pdfViewer()->currentPage() + 1;
            int total = m_officeViewer->pdfViewer()->pageCount();
            return tr("Page %1 of %2 • %3 (LibreOffice HD)")
                .arg(cur).arg(total)
                .arg(formatFileSize(info.size()));
        } else {
            return tr("%1 estimated words • %2 (Fast C++ Viewer)")
                .arg(m_officeViewer->wordCount())
                .arg(formatFileSize(info.size()));
        }
    } else if (idx == 6) { // Folder
        return tr("📁 %1 items • %2")
            .arg(m_folderViewer->itemCount())
            .arg(formatFileSize(m_folderViewer->totalSize()));
    }
    return formatFileSize(info.size());
}

QString BaseSkin::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

void BaseSkin::setEditMode(bool editMode)
{
    if (editMode && m_currentCategory == FileCategory::Pdf) {
        if (!promotePdfViewerToStudio()) {
            return;
        }
    }
    if (editMode && m_currentCategory == FileCategory::Media) {
        if (!promoteMediaViewerToStudio()) {
            return;
        }
    }

    m_isEditMode = editMode;
    if (m_imageViewer) {
        m_imageViewer->setEditMode(editMode);
    }
    if (m_textViewer) {
        m_textViewer->setEditMode(editMode);
    }
    if (m_pdfViewer) {
        m_pdfViewer->setEditMode(editMode);
    }
    if (m_archiveViewer) {
        m_archiveViewer->setEditMode(editMode);
    }
    if (m_mediaViewer) {
        m_mediaViewer->setEditMode(editMode);
    }
    if (m_officeViewer) {
        m_officeViewer->setEditMode(editMode);
    }
}

void BaseSkin::handleEscapeAction()
{
    if (m_pdfViewer && m_pdfViewer->isPageGridOrganizerActive()) {
        m_pdfViewer->hidePageGridOrganizer();
        return;
    }
    if (m_isEditMode) {
        setEditMode(false);
        return;
    }
    if (!m_archiveOriginPath.isEmpty() && QFile::exists(m_archiveOriginPath)) {
        QString returnArchive = m_archiveOriginPath;
        m_archiveOriginPath.clear();
        m_archiveEntries.clear();
        m_archiveCurrentEntryIndex = -1;
        openFile(returnArchive);
        return;
    }
    close();
}

void BaseSkin::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        handleEscapeAction();
        event->accept();
        break;
    case Qt::Key_Q:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
            return;
        }
        close();
        event->accept();
        break;
    case Qt::Key_Space:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
            return;
        }
        // In macOS Quick Look preview mode, pressing Spacebar closes the preview
        close();
        event->accept();
        break;
    case Qt::Key_Right:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
        } else {
            nextFile();
            event->accept();
        }
        break;
    case Qt::Key_Left:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
        } else {
            prevFile();
            event->accept();
        }
        break;
    case Qt::Key_Down:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
        } else {
            scrollCurrentView(1);
            event->accept();
        }
        break;
    case Qt::Key_Up:
        if (m_isEditMode) {
            QMainWindow::keyPressEvent(event);
        } else {
            scrollCurrentView(-1);
            event->accept();
        }
        break;
    case Qt::Key_Backspace:
        if (!m_archiveOriginPath.isEmpty() && QFile::exists(m_archiveOriginPath)) {
            QString returnArchive = m_archiveOriginPath;
            m_archiveOriginPath.clear();
            m_archiveEntries.clear();
            m_archiveCurrentEntryIndex = -1;
            openFile(returnArchive);
            event->accept();
            return;
        }
        if (m_stackedWidget->currentIndex() == 6) {
            m_folderViewer->goUpDirectory();
            event->accept();
            return;
        }
        break;
    case Qt::Key_Delete:
        if (!m_isEditMode || m_currentCategory != FileCategory::Text) {
            deleteCurrentFile();
            event->accept();
            return;
        }
        break;
    case Qt::Key_F11:
        toggleFullscreen();
        event->accept();
        break;
    case Qt::Key_F5:
        toggleSlideshow();
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}

bool BaseSkin::eventFilter(QObject *watched, QEvent *event)
{
    // In Edit mode, navigation shortcuts (Left/Right = previous/next file) must not
    // steal arrow keys from focused sliders or combo boxes (e.g. the equalizer bands).
    if (event->type() == QEvent::ShortcutOverride && m_isEditMode) {
        auto *w = qobject_cast<QWidget*>(watched);
        if (w && w->window() == this && (qobject_cast<QAbstractSlider*>(w) || qobject_cast<QComboBox*>(w))) {
            switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                event->accept();
                break;
            default:
                break;
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        auto *widget = qobject_cast<QWidget*>(watched);

        // Only enforce preview rules on this window and its embedded viewers
        // (Never intercept events from separate modal/sub-dialogs, or when in Edit Mode)
        if (widget && widget->window() == this && !m_isEditMode) {
            // Never intercept typing or navigation within text input fields or search bars
            QWidget *w = widget;
            bool isInput = false;
            while (w) {
                if (qobject_cast<QLineEdit*>(w) ||
                    qobject_cast<QAbstractSpinBox*>(w) ||
                    (m_searchBar && (w == m_searchBar || m_searchBar->isAncestorOf(w)))) {
                    isInput = true;
                    break;
                }
                if (auto *te = qobject_cast<QTextEdit*>(w)) {
                    if (!te->isReadOnly()) {
                        isInput = true;
                        break;
                    }
                }
                if (auto *pte = qobject_cast<QPlainTextEdit*>(w)) {
                    if (!pte->isReadOnly()) {
                        isInput = true;
                        break;
                    }
                }
                w = w->parentWidget();
            }

            if (!isInput) {
                int key = keyEvent->key();
                const bool isFolderActive = (m_stackedWidget && m_stackedWidget->currentIndex() == 6);

                switch (key) {
                case Qt::Key_Space:
                    if (!isFolderActive) {
                        close();
                        return true;
                    }
                    break;
                case Qt::Key_Escape:
                    handleEscapeAction();
                    return true;
                case Qt::Key_Right:
                    if (!isFolderActive) {
                        nextFile();
                        return true;
                    }
                    break;
                case Qt::Key_Left:
                    if (!isFolderActive) {
                        prevFile();
                        return true;
                    }
                    break;
                case Qt::Key_Down:
                    if (!isFolderActive) {
                        scrollCurrentView(1);
                        return true;
                    }
                    break;
                case Qt::Key_Up:
                    if (!isFolderActive) {
                        scrollCurrentView(-1);
                        return true;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void BaseSkin::scrollCurrentView(int direction)
{
    QWidget *curr = m_stackedWidget ? m_stackedWidget->currentWidget() : nullptr;
    if (!curr) return;

    auto doScroll = [direction](QScrollBar *vbar) -> bool {
        if (!vbar || vbar->maximum() <= 0) return false;
        int step = vbar->singleStep();
        if (step <= 0) step = 40;
        else step = qMax(40, step * 3);
        int oldVal = vbar->value();
        vbar->setValue(oldVal + direction * step);
        return vbar->value() != oldVal;
    };

    // 1. Current widget is directly a QAbstractScrollArea (e.g. TextViewer, ArchiveViewer)
    if (auto *scrollArea = qobject_cast<QAbstractScrollArea*>(curr)) {
        if (doScroll(scrollArea->verticalScrollBar())) {
            return;
        }
    }

    // 2. Search child QAbstractScrollArea widgets (e.g. ImageViewer, PdfViewer, OfficeViewer, FolderViewer)
    const auto scrollAreas = curr->findChildren<QAbstractScrollArea*>();
    for (auto *sa : scrollAreas) {
        if (sa->isVisible() && doScroll(sa->verticalScrollBar())) {
            return;
        }
    }

    // 3. Fallback: visible vertical QScrollBar children
    const auto scrollBars = curr->findChildren<QScrollBar*>();
    for (auto *bar : scrollBars) {
        if (bar->isVisible() && bar->orientation() == Qt::Vertical && doScroll(bar)) {
            return;
        }
    }

    // 4. Fallback for PDF viewer single-page mode or page transitions
    if (m_stackedWidget && m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        if (direction > 0) {
            m_pdfViewer->nextPage();
        } else {
            m_pdfViewer->prevPage();
        }
    }
}

void BaseSkin::rotateCurrentRight()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->rotateClockwise();
        if (!m_isEditMode) {
            saveCurrentImage(true);
        }
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->rotateClockwise();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::rotateCurrentLeft()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->rotateCounterClockwise();
        if (!m_isEditMode) {
            saveCurrentImage(true);
        }
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->rotateCounterClockwise();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::flipCurrentHorizontal()
{
    if (!m_isEditMode) return;
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->flipHorizontal();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::flipCurrentVertical()
{
    if (!m_isEditMode) return;
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->flipVertical();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::openResizeImageDialog()
{
    if (!m_imageViewer || m_stackedWidget->currentIndex() != 0) return;
    const QImage &img = m_imageViewer->originalImage();
    if (img.isNull()) return;

    ImageResizeDialog dlg(img.width(), img.height(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_imageViewer->resizeImage(dlg.targetWidth(), dlg.targetHeight(), dlg.transformationMode());
        updateWindowTitle();
        updateStatusBar();
    }
}

void BaseSkin::togglePdfToc()
{
    if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->toggleTocSidebar();
    }
}

void BaseSkin::extractPdfImages(const QString &filePath)
{
    QString target = filePath;
    if (target.isEmpty()) {
        if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            target = m_currentFilePath;
        } else if (m_stackedWidget->currentIndex() == 6 && m_folderViewer) {
            QString sel = m_folderViewer->selectedFilePath();
            if (!sel.isEmpty() && PluginManager::categorizeFile(sel) == FileCategory::Pdf) {
                target = sel;
            }
        }
    }
    if (target.isEmpty()) {
        target = m_currentFilePath;
    }
    if (target.isEmpty() || !QFile::exists(target)) return;

    PdfImageExtractDialog dlg(target, this);
    dlg.exec();
}

void BaseSkin::openPdfRepairDialog(const QString &filePath)
{
    QString target = filePath;
    if (target.isEmpty()) {
        if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            target = m_currentFilePath;
        } else if (m_folderViewer) {
            QString sel = m_folderViewer->selectedFilePath();
            if (!sel.isEmpty() && PluginManager::categorizeFile(sel) == FileCategory::Pdf) {
                target = sel;
            }
        }
    }
    if (target.isEmpty() && m_currentCategory == FileCategory::Pdf) {
        target = m_currentFilePath;
    }
    if (target.isEmpty() || !QFile::exists(target)) return;

    PdfRepairDialog dlg(target, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString repPath = dlg.repairedFilePath();
        if (!repPath.isEmpty() && QFile::exists(repPath)) {
            openFile(repPath);
        }
    }
}

void BaseSkin::openPdfPageOrganizer(const QString &filePath)
{
    QString target = filePath;
    if (target.isEmpty()) {
        if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            target = m_currentFilePath;
        } else if (m_stackedWidget->currentIndex() == 6 && m_folderViewer) {
            QString sel = m_folderViewer->selectedFilePath();
            if (!sel.isEmpty() && PluginManager::categorizeFile(sel) == FileCategory::Pdf) {
                target = sel;
            }
        }
    }
    if (target.isEmpty() || !QFile::exists(target)) return;

    if (ensurePdfViewerCreated() && promotePdfViewerToStudio() && m_pdfViewer) {
        m_pdfViewer->openPageOrganizer(target, this);
    }
}



void BaseSkin::zoomInCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->zoomIn();
    } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->zoomInText(1);
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->zoomIn();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->zoomIn();
    }
}

void BaseSkin::zoomOutCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->zoomOut();
    } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->zoomOutText(1);
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->zoomOut();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->zoomOut();
    }
}

void BaseSkin::resetZoomCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->resetZoom();
    } else if (m_stackedWidget->currentIndex() == 1 && m_textViewer) {
        m_textViewer->resetTextZoom();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->resetZoom();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->resetZoom();
    }
}

void BaseSkin::fitCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->fitToWindow();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->fitToWidth();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->fitToWidth();
    }
}

void BaseSkin::fitWidthCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->fitToWindow();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->fitToWidth();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->fitToWidth();
    }
}

void BaseSkin::fitPageCurrent()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->fitToWindow();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->fitToPage();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->fitToPage();
    }
}

void BaseSkin::toggleZoomTool(bool active)
{
    if (m_pdfViewer) {
        m_pdfViewer->toggleZoomTool(active);
    }
    if (m_imageViewer) {
        if (!active) {
            m_imageViewer->setZoomToolActive(false);
            if (m_imageViewer->canvasWidget()) {
                m_imageViewer->canvasWidget()->setCursor(Qt::ArrowCursor);
            }
        } else {
            m_imageViewer->setZoomToolActive(false);
            if (m_imageViewer->canvasWidget()) {
                m_imageViewer->canvasWidget()->setCursor(Qt::OpenHandCursor);
            }
        }
    }
    if (m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->toggleZoomTool(active);
    }
}

bool BaseSkin::isZoomToolActive() const
{
    if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        return m_pdfViewer->isZoomToolActive();
    }
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        return m_imageViewer->canvasWidget() && m_imageViewer->canvasWidget()->cursor().shape() == Qt::OpenHandCursor;
    }
    if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        return m_officeViewer->pdfViewer()->isZoomToolActive();
    }
    return false;
}

void BaseSkin::nextImageFile()
{
    if (m_siblingFiles.size() <= 1 || m_currentIndex < 0) return;

    // Find next sibling file that is strictly an image
    int count = m_siblingFiles.size();
    for (int step = 1; step < count; ++step) {
        int nextIdx = (m_currentIndex + step) % count;
        if (PluginManager::categorizeFile(m_siblingFiles[nextIdx]) == FileCategory::Image) {
            startTransitionAnimation(true);
            m_currentIndex = nextIdx;
            openFile(m_siblingFiles[m_currentIndex]);
            return;
        }
    }
}

void BaseSkin::toggleSlideshow()
{
    // Photo Slideshow is strictly for images
    if (m_currentCategory != FileCategory::Image) {
        if (m_slideshowTimer && m_slideshowTimer->isActive()) {
            m_slideshowTimer->stop();
        }
        return;
    }

    if (!m_slideshowTimer) {
        m_slideshowTimer = new QTimer(this);
        connect(m_slideshowTimer, &QTimer::timeout, this, &BaseSkin::nextImageFile);
    }
    if (m_slideshowTimer->isActive()) {
        m_slideshowTimer->stop();
    } else {
        m_slideshowTimer->start(3000); // 3 seconds per photo
    }
}

bool BaseSkin::isSlideshowActive() const
{
    return m_slideshowTimer && m_slideshowTimer->isActive();
}

void BaseSkin::openCurrentFolderInGallery()
{
    // Categorically disallow switching to folder gallery when in Edit Mode
    if (m_isEditMode) {
        return;
    }

    if (m_currentFilePath.isEmpty()) return;

    // Toggle behavior: if already inside Folder Gallery, return to active file or selected item
    if (m_currentCategory == FileCategory::Folder) {
        QString targetPath;
        if (m_folderViewer) {
            QString selected = m_folderViewer->selectedFilePath();
            if (!selected.isEmpty() && !QFileInfo(selected).isDir()) {
                targetPath = selected;
            }
        }
        if (targetPath.isEmpty() && !m_previousFilePathBeforeGallery.isEmpty() && QFileInfo::exists(m_previousFilePathBeforeGallery)) {
            targetPath = m_previousFilePathBeforeGallery;
        }

        if (!targetPath.isEmpty()) {
            openFileFromGalleryWithZoom(targetPath);
            return;
        }
        return;
    }

    m_previousFilePathBeforeGallery = m_currentFilePath;
    QString dir;
    if (!m_archiveOriginPath.isEmpty()) {
        dir = QFileInfo(m_archiveOriginPath).absolutePath();
    } else {
        QFileInfo fi(m_currentFilePath);
        dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    }

    returnToFolderWithZoom(dir, m_previousFilePathBeforeGallery);
}

void BaseSkin::openFileFromGalleryWithZoom(const QString &filePath)
{
    if (ThemeManager::currentTransition() == TransitionEffect::None || !m_galleryTransition || !m_folderViewer) {
        openFile(filePath);
        return;
    }

    m_folderViewer->ensureThumbnailForPath(filePath);
    const QRect oldStackGeom = m_stackedWidget->geometry();
    QRect cardRect = m_folderViewer->cardRectForPath(filePath);
    QRect thumbRect = m_folderViewer->thumbnailRectForPath(filePath);
    const QPoint fvOffset = m_folderViewer->mapTo(m_stackedWidget, QPoint(0, 0));
    cardRect.translate(fvOffset);
    thumbRect.translate(fvOffset);

    const QPixmap galleryPix = m_stackedWidget->grab();
    const QRect stackLocal = m_stackedWidget->rect();
    const QPixmap cardThumb = copyLogicalRect(galleryPix, cardRect.intersected(stackLocal));
    const QPixmap thumbPix = copyLogicalRect(galleryPix, thumbRect.intersected(stackLocal));

    m_folderViewer->setCardHiddenForTransition(filePath, true);
    const QPixmap emptySlotGalleryPix = m_stackedWidget->grab();
    m_folderViewer->setCardHiddenForTransition(QString(), false);

    openFile(filePath);

    if (m_centralContainer && m_centralContainer->layout()) {
        m_centralContainer->layout()->activate();
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QRect newStackGeom = m_stackedWidget->geometry();
    const QPixmap viewerPix = m_stackedWidget->grab();
    const QPoint stackShift = oldStackGeom.topLeft() - newStackGeom.topLeft();
    cardRect.translate(stackShift);
    thumbRect.translate(stackShift);

    m_galleryTransition->setGeometry(newStackGeom);
    m_galleryTransition->start(emptySlotGalleryPix, viewerPix, cardRect,
                               GalleryZoomTransition::Direction::ZoomInToViewer, 220,
                               cardThumb, thumbRect, thumbPix, m_stackedWidget->rect());
}

void BaseSkin::returnToFolderWithZoom(const QString &dir, const QString &targetFilePath)
{
    if (ThemeManager::currentTransition() == TransitionEffect::None || !m_galleryTransition) {
        openFile(dir);
        if (!targetFilePath.isEmpty() && m_folderViewer) {
            m_folderViewer->selectItemByPath(targetFilePath, true);
        }
        return;
    }

    const QRect oldStackGeom = m_stackedWidget->geometry();
    const QPixmap viewerPix = m_stackedWidget->grab();

    openFile(dir);

    if (!m_folderViewer) {
        return;
    }

    if (m_centralContainer && m_centralContainer->layout()) {
        m_centralContainer->layout()->activate();
    }
    if (m_folderViewer->layout()) {
        m_folderViewer->layout()->activate();
    }
    if (m_folderViewer->listWidget()) {
        m_folderViewer->listWidget()->doItemsLayout();
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!targetFilePath.isEmpty()) {
        m_folderViewer->ensureThumbnailForPath(targetFilePath);
        m_folderViewer->selectItemByPath(targetFilePath, true);
        if (m_folderViewer->listWidget()) {
            m_folderViewer->listWidget()->doItemsLayout();
        }
    }

    const QRect newStackGeom = m_stackedWidget->geometry();
    QRect cardRect = m_folderViewer->cardRectForPath(targetFilePath);
    QRect thumbRect = m_folderViewer->thumbnailRectForPath(targetFilePath);
    const QPoint fvOffset = m_folderViewer->mapTo(m_stackedWidget, QPoint(0, 0));
    cardRect.translate(fvOffset);
    thumbRect.translate(fvOffset);

    const QPixmap normalGalleryPix = m_stackedWidget->grab();
    const QRect stackLocal = m_stackedWidget->rect();
    const QPixmap cardThumb = copyLogicalRect(normalGalleryPix, cardRect.intersected(stackLocal));
    const QPixmap thumbPix = copyLogicalRect(normalGalleryPix, thumbRect.intersected(stackLocal));

    m_folderViewer->setCardHiddenForTransition(targetFilePath, true);
    const QPixmap emptySlotGalleryPix = m_stackedWidget->grab();

    const QRect viewerRectInOverlay(oldStackGeom.topLeft() - newStackGeom.topLeft(), oldStackGeom.size());

    // Keep target card hidden underneath so the zoom creates the card seamlessly into the slot without double-render
    connect(m_galleryTransition, &GalleryZoomTransition::finished, this, [this]() {
        if (m_folderViewer) {
            m_folderViewer->setCardHiddenForTransition(QString(), false);
        }
    }, Qt::SingleShotConnection);

    m_galleryTransition->setGeometry(newStackGeom);
    m_galleryTransition->start(viewerPix, emptySlotGalleryPix, cardRect,
                               GalleryZoomTransition::Direction::ZoomOutToGallery, 220,
                               cardThumb, thumbRect, thumbPix, viewerRectInOverlay);
}

void BaseSkin::renameCurrentFolderItem()
{
    if (m_currentCategory != FileCategory::Folder || !m_folderViewer) return;
    QString selected = m_folderViewer->selectedFilePath();
    if (selected.isEmpty()) return;

    QFileInfo fi(selected);
    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename Item"), tr("New name:"), QLineEdit::Normal, fi.fileName(), &ok);
    if (ok && !newName.isEmpty() && newName != fi.fileName()) {
        QString newPath = fi.dir().filePath(newName);
        if (QFile::rename(selected, newPath)) {
            m_folderViewer->loadFolder(m_folderViewer->currentFolder(), true);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Unable to rename item."));
        }
    }
}

void BaseSkin::deleteCurrentFolderItem()
{
    if (m_currentCategory != FileCategory::Folder || !m_folderViewer) return;
    QString selected = m_folderViewer->selectedFilePath();
    if (selected.isEmpty()) return;

    QFileInfo fi(selected);
    auto res = QMessageBox::question(this, tr("Delete Item"), tr("Move '%1' to trash?").arg(fi.fileName()),
                                     QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (QFile::moveToTrash(selected)) {
            m_folderViewer->loadFolder(m_folderViewer->currentFolder(), true);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Unable to move item to trash."));
        }
    }
}

void BaseSkin::createFolderInCurrentFolder()
{
    if (m_currentCategory != FileCategory::Folder || !m_folderViewer) return;
    QString currentDir = m_folderViewer->currentFolder();
    if (currentDir.isEmpty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal, tr("New Folder"), &ok);
    if (ok && !name.isEmpty()) {
        QDir dir(currentDir);
        if (dir.mkdir(name)) {
            m_folderViewer->loadFolder(currentDir, true);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Unable to create folder."));
        }
    }
}

void BaseSkin::openCreateArchiveDialog()
{
    ensureArchiveViewerCreated();
    if (!m_archiveViewer) {
        QMessageBox::information(this, tr("Create Archive"),
                                 tr("The Archive Engine plugin (libarchive) is not currently active."));
        return;
    }

    QString activeFile;
    QString activeFolder;
    if (m_currentCategory == FileCategory::Folder && m_folderViewer) {
        activeFolder = m_folderViewer->currentFolder();
        QString sel = m_folderViewer->selectedFilePath();
        if (!sel.isEmpty() && QFileInfo::exists(sel)) {
            if (QFileInfo(sel).isDir()) {
                activeFolder = sel;
            } else {
                activeFile = sel;
            }
        }
    } else if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        if (fi.isDir()) {
            activeFolder = fi.absoluteFilePath();
        } else if (fi.isFile()) {
            activeFile = fi.absoluteFilePath();
            activeFolder = fi.absolutePath();
        }
    }

    bool sendLocalSend = false;
    bool openWhenDone = false;
    QString createdPath = m_archiveViewer->openCreateArchiveDialog(
        activeFile, activeFolder, this, &sendLocalSend, &openWhenDone
    );
    if (createdPath.isEmpty() || !QFile::exists(createdPath)) {
        return;
    }

    if (m_currentCategory == FileCategory::Folder && m_folderViewer && !m_folderViewer->currentFolder().isEmpty()) {
        m_folderViewer->loadFolder(m_folderViewer->currentFolder(), true);
    }

    if (sendLocalSend) {
        ShareManager::shareLocalSend(createdPath, this);
    }

    if (openWhenDone) {
        openFile(createdPath);
    }
}

void BaseSkin::addFilesToCurrentArchive()
{
    if (m_currentCategory == FileCategory::Archive && m_archiveViewer) {
        m_archiveViewer->addFilesToArchive();
    }
}

void BaseSkin::addFolderToCurrentArchive()
{
    if (m_currentCategory == FileCategory::Archive && m_archiveViewer) {
        m_archiveViewer->addFolderToArchive();
    }
}

void BaseSkin::removeSelectedFromCurrentArchive()
{
    if (m_currentCategory == FileCategory::Archive && m_archiveViewer) {
        m_archiveViewer->removeSelectedFromArchive();
    }
}

void BaseSkin::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About BubezView"));
    aboutBox.setTextFormat(Qt::RichText);
    aboutBox.setIconPixmap(QIcon::fromTheme("bubezview", windowIcon()).pixmap(64, 64));
    const bool isDev = (QCoreApplication::applicationDirPath().contains("build_deb") ||
                        QCoreApplication::applicationDirPath().contains("Progetti/BubezView"));
    aboutBox.setText(
        QString("<h3>BubezView v%1%2</h3>").arg(QApplication::applicationVersion(), isDev ? QStringLiteral(" <span style='color: #38bdf8;'>[Sviluppo / Dev Build]</span>") : QString()) +
        "<p><b>" + tr("Native C++ / Qt6 Universal Viewer for Linux") + "</b>" +
        (isDev ? ("<br><small style='color: #a855f7;'>Percorso: " + QCoreApplication::applicationFilePath() + "</small>") : QString()) + "</p>"
        "<p>" + tr("Developed by <b>Davide (Bubez)</b>.<br>Designed to be ultra-fast, responsive, and well integrated into the Linux desktop ecosystem.") + "</p>"
        "<hr>"
        "<p><b>" + tr("Active engines and libraries:") + "</b><br>"
        "• <b>Qt 6.11</b> — " + tr("Core, Widgets, Network, PrintSupport") + "<br>"
        "• <b>Poppler-Qt6</b> — " + tr("Ultra-HD PDF vector rendering") + "<br>"
        "• <b>LibArchive</b> — " + tr("ZIP, 7Z, TAR, RAR, ISO archive manager") + "<br>"
        "• <b>LibRevenge / LibreOffice</b> — " + tr("Office document processing and conversion") + "<br>"
        "• <b>libmpv</b> — " + tr("Studio audio and hardware GPU video playback") + "<br>"
        "• <b>LibQrencode</b> — " + tr("Instant Wi-Fi / local network sharing") + "</p>"
        "<p><small>" + tr("© 2026 Davide / Bubez — Released under Open Source license.") + "</small></p>"
    );
    aboutBox.setStandardButtons(QMessageBox::Ok);
    aboutBox.exec();
}

void BaseSkin::showShortcutsDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Keyboard Shortcuts — BubezView"));
    dlg.resize(580, 560);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    QLabel *title = new QLabel("<b>" + tr("Keyboard Shortcuts Guide") + "</b>", &dlg);
    title->setStyleSheet("font-size: 15px;");
    lay->addWidget(title);

    QLineEdit *searchEdit = new QLineEdit(&dlg);
    searchEdit->setPlaceholderText(tr("Search shortcut or action..."));
    searchEdit->setClearButtonEnabled(true);
    lay->addWidget(searchEdit);

    struct ShortcutRow { QString category; QString action; QString key; };
    const std::vector<ShortcutRow> rows = {
        // Navigation
        {tr("Navigation"), tr("Previous File"), tr("Left / Backspace")},
        {tr("Navigation"), tr("Next File"), tr("Right / Space")},
        {tr("Navigation"), tr("Previous Page (PDF)"), tr("Page Up (or Wheel Up at top)")},
        {tr("Navigation"), tr("Next Page (PDF)"), tr("Page Down (or Wheel Down at bottom)")},
        {tr("Navigation"), tr("Folder Gallery View"), "Alt+Up"},

        // View & Zoom
        {tr("View"), tr("Toggle Fullscreen"), "F11"},
        {tr("View"), tr("Photo Slideshow"), "F5"},
        {tr("View"), tr("Zoom In"), tr("Ctrl++ (or Ctrl+Wheel Up)")},
        {tr("View"), tr("Zoom Out"), tr("Ctrl+- (or Ctrl+Wheel Down)")},
        {tr("View"), tr("Zoom 100% (Original Size)"), "Ctrl+0"},
        {tr("View"), tr("Fit to Window"), "Ctrl+9"},
        {tr("View"), tr("Thumbnail Strip (Filmstrip)"), "F4"},
        {tr("View"), tr("Document Outline (TOC)"), "F3"},
        {tr("View"), tr("File Metadata & Properties"), "Ctrl+I / I"},
        {tr("View"), tr("Show / Hide Menu Bar"), "Ctrl+M"},
        {tr("View"), tr("Show / Hide Toolbar"), "Ctrl+T"},

        // File & Clipboard
        {tr("File"), tr("Open File..."), "Ctrl+O"},
        {tr("File"), tr("Open Folder..."), "Ctrl+Shift+O / Ctrl+G"},
        {tr("File"), tr("Open with Default Application"), "Ctrl+Return"},
        {tr("File"), tr("Save Changes"), "Ctrl+S"},
        {tr("File"), tr("Save Copy As..."), "Ctrl+Shift+S"},
        {tr("File"), tr("Export As..."), "Ctrl+E"},
        {tr("File"), tr("Print Document"), "Ctrl+P"},
        {tr("File"), tr("Copy Content / Text"), "Ctrl+C"},
        {tr("File"), tr("Copy File Path"), "Ctrl+Shift+C"},
        {tr("File"), tr("Cut Text"), "Ctrl+X"},
        {tr("File"), tr("Paste Text"), "Ctrl+V"},
        {tr("File"), tr("Move to Trash"), "Delete"},
        {tr("File"), tr("Rename Item (Folder)"), "F2"},
        {tr("File"), tr("Create New Folder"), "F7"},
        {tr("File"), tr("Exit Application"), "Ctrl+Q"},

        // Transformations
        {tr("Transform"), tr("Rotate 90° Clockwise"), "R"},
        {tr("Transform"), tr("Rotate 90° Counterclockwise"), "L"},
        {tr("Transform"), tr("Flip Horizontally"), "H"},
        {tr("Transform"), tr("Flip Vertically"), "V"},

        // Tools & Media
        {tr("Tools"), tr("Find in Document"), "Ctrl+F"},
        {tr("Tools"), tr("Undo"), "Ctrl+Z"},
        {tr("Tools"), tr("Redo"), "Ctrl+Y"},
        {tr("Tools"), tr("Trim Audio Clip"), "Ctrl+Shift+T"},
        {tr("Tools"), tr("Play / Pause Media"), tr("Space")},

        // Text & Code
        {tr("Text"), tr("Toggle Text Editing"), "F4"},
        {tr("Text"), tr("Find and Replace"), "Ctrl+H"},
        {tr("Text"), tr("Toggle Word Wrap"), "Alt+Z"},
        {tr("Text"), tr("Toggle Line Numbers"), "F8"},
        {tr("Text"), tr("Toggle Syntax Highlighting"), "F9"},

        // PDF Tools
        {tr("PDF"), tr("PDF Tools Hub"), "Ctrl+Shift+M"},
        {tr("PDF"), tr("Text Highlighter"), "Ctrl+Shift+H"},
        {tr("PDF"), tr("Insert Blank Page"), "Ctrl+Shift+N"},
        {tr("PDF"), tr("Snapshot / Area Select"), "Ctrl+Shift+A"},

        {tr("Help"), tr("Keyboard Shortcuts Guide"), "F1"}
    };

    QTableWidget *table = new QTableWidget(static_cast<int>(rows.size()), 3, &dlg);
    table->setHorizontalHeaderLabels(QStringList() << tr("Category") << tr("Action") << tr("Shortcut"));
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);

    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        QTableWidgetItem *catItem = new QTableWidgetItem(rows[r].category);
        catItem->setForeground(QBrush(QColor(130, 140, 155)));
        table->setItem(r, 0, catItem);

        table->setItem(r, 1, new QTableWidgetItem(rows[r].action));

        QTableWidgetItem *keyItem = new QTableWidgetItem(rows[r].key);
        keyItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(r, 2, keyItem);
    }
    lay->addWidget(table);

    connect(searchEdit, &QLineEdit::textChanged, [table, rows](const QString &text) {
        QString q = text.trimmed();
        for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
            bool match = q.isEmpty()
                      || rows[r].category.contains(q, Qt::CaseInsensitive)
                      || rows[r].action.contains(q, Qt::CaseInsensitive)
                      || rows[r].key.contains(q, Qt::CaseInsensitive);
            table->setRowHidden(r, !match);
        }
    });

    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->addStretch();
    QPushButton *btnClose = new QPushButton(tr("Close"), &dlg);
    btnClose->setDefault(true);
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLay->addWidget(btnClose);
    lay->addLayout(btnLay);

    dlg.exec();
}

void BaseSkin::checkForUpdates()
{
    UpdateManager &mgr = UpdateManager::instance();

    // Disconnect any previous temporary slots
    disconnect(&mgr, &UpdateManager::updateAvailable, this, nullptr);
    disconnect(&mgr, &UpdateManager::alreadyUpToDate, this, nullptr);
    disconnect(&mgr, &UpdateManager::checkFailed, this, nullptr);

    connect(&mgr, &UpdateManager::updateAvailable, this, [this](const ReleaseInfo &info) {
        UpdateDialog dlg(info, this);
        dlg.exec();
    });

    connect(&mgr, &UpdateManager::alreadyUpToDate, this, [this, &mgr]() {
        QMessageBox::information(
            this,
            tr("Check for Updates"),
            "<h3>" + tr("✅ BubezView is up to date!") + "</h3>"
            "<p>" + tr("Installed version: <b>v%1 (Native C++)</b>").arg(mgr.currentVersion()) + "</p>"
            "<p>" + tr("There are no updates available at this time.") + "</p>"
        );
    });

    connect(&mgr, &UpdateManager::checkFailed, this, [this](const QString &err) {
        QMessageBox::warning(
            this,
            tr("Check for Updates"),
            tr("Could not check for updates: %1").arg(err)
        );
    });

    mgr.checkForUpdates(false);
}

void BaseSkin::toggleFilmstrip()
{
    setFilmstripVisible(!m_filmstripWanted);
}

void BaseSkin::setFilmstripVisible(bool visible)
{
    m_filmstripWanted = visible;
    QSettings settings("Bubez", "BubezView");
    settings.setValue("ui/filmstrip", visible);

    if (m_currentCategory != FileCategory::Folder && m_filmstripWanted && !m_siblingFiles.isEmpty()) {
        m_filmstrip->setFiles(m_siblingFiles, m_currentIndex);
        m_filmstrip->show();
    } else {
        m_filmstrip->hide();
    }
}

bool BaseSkin::isFilmstripVisible() const
{
    return m_filmstripWanted;
}

void BaseSkin::contextMenuEvent(QContextMenuEvent *event)
{
    showContextMenuAt(event->globalPos());
}

void BaseSkin::showContextMenuAt(const QPoint &globalPos)
{
    QString targetPath = currentShareFilePath();
    if (targetPath.isEmpty()) {
        targetPath = m_currentFilePath;
    }
    if (targetPath.isEmpty()) {
        return;
    }

    FileCategory activeCategory = m_currentCategory;
    if (m_currentCategory == FileCategory::Folder && !targetPath.isEmpty() && targetPath != m_currentFilePath) {
        activeCategory = PluginManager::categorizeFile(targetPath);
    }
    bool isFolder = (m_currentCategory == FileCategory::Folder && (targetPath == m_currentFilePath || QFileInfo(targetPath).isDir()));

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
            "   padding: 6px 24px 6px 12px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #38bdf8;"
            "   color: #ffffff;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: #3e4451;"
            "   margin: 4px 8px;"
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
            "   padding: 6px 24px 6px 12px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #007aff;"
            "   color: #ffffff;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: #e5e5ea;"
            "   margin: 4px 8px;"
            "}"
        );
    }

    // If browsing in folder view and a file card is selected, add direct Open action
    if (m_currentCategory == FileCategory::Folder && !isFolder && !targetPath.isEmpty()) {
        menu.addAction(QIcon::fromTheme("document-open"), tr("Open"), this, [this, targetPath]() {
            openFile(targetPath);
        });
        menu.addSeparator();
    } else if (m_currentCategory == FileCategory::Folder && isFolder && !targetPath.isEmpty()) {
        menu.addAction(QIcon::fromTheme("bookmark-new"), tr("Add to Bookmarks"), this, [this, targetPath]() {
            if (m_folderViewer) {
                m_folderViewer->addCustomBookmark(targetPath);
            }
        });
        menu.addSeparator();
    }

    // 1. Standard actions: Copy
    QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy"), this, &BaseSkin::copyToClipboard);
    actCopy->setShortcut(QKeySequence("Ctrl+C"));

    menu.addSeparator();

    // 2. Open With... & Share submenus
    if (m_currentCategory == FileCategory::Folder) {
        QMenu *menuOpenWith = menu.addMenu(QIcon::fromTheme("system-run"), tr("Open With..."));
        QList<AppInfo> apps = AppLauncher::getAssociatedApps(targetPath);
        if (!apps.isEmpty()) {
            for (const AppInfo &app : apps) {
                QIcon icon = app.iconName.isEmpty() ? QIcon::fromTheme("application-x-executable") : QIcon::fromTheme(app.iconName);
                menuOpenWith->addAction(icon, app.name, this, [targetPath, app]() {
                    AppLauncher::launchApp(app, targetPath);
                });
            }
            menuOpenWith->addSeparator();
        }
        menuOpenWith->addAction(QIcon::fromTheme("applications-other"), tr("Other Application..."), this, &BaseSkin::openWithOther);
    }

    QMenu *menuShare = menu.addMenu(QIcon::fromTheme("document-share"), tr("Share"));
    QAction *actCopyPath = menuShare->addAction(QIcon::fromTheme("edit-copy-path"), tr("Copy File Path"), this, [targetPath]() {
        QApplication::clipboard()->setText(targetPath);
    });
    actCopyPath->setShortcut(QKeySequence("Ctrl+Shift+C"));
    menuShare->addSeparator();

    if (m_currentCategory != FileCategory::Folder) {
        QMenu *menuOpenWithApp = menuShare->addMenu(QIcon::fromTheme("system-run"), tr("Share with Application..."));
        QList<AppInfo> apps = AppLauncher::getAssociatedApps(targetPath);
        if (!apps.isEmpty()) {
            for (const AppInfo &app : apps) {
                QIcon icon = app.iconName.isEmpty() ? QIcon::fromTheme("application-x-executable") : QIcon::fromTheme(app.iconName);
                menuOpenWithApp->addAction(icon, app.name, this, [targetPath, app]() {
                    AppLauncher::launchApp(app, targetPath);
                });
            }
            menuOpenWithApp->addSeparator();
        }
        menuOpenWithApp->addAction(QIcon::fromTheme("applications-other"), tr("Other Application..."), this, &BaseSkin::openWithOther);
        menuShare->addSeparator();
    }

    ShareManager shareMgr(this, [targetPath]() { return targetPath; });
    shareMgr.setImageProvider([this, activeCategory, isFolder]() -> QImage {
        if (isFolder) return QImage();
        if (activeCategory == FileCategory::Image && m_imageViewer) {
            return m_imageViewer->originalImage();
        }
        if (activeCategory == FileCategory::Pdf && m_pdfViewer) {
            return m_pdfViewer->renderPageForOcr(m_pdfViewer->currentPage(), 200);
        }
        if (activeCategory == FileCategory::Office && m_officeViewer && m_officeViewer->pdfViewer()) {
            IPdfViewer *ovPdf = m_officeViewer->pdfViewer();
            return ovPdf->renderPageForOcr(ovPdf->currentPage(), 200);
        }
        return QImage();
    }, !isFolder && (activeCategory == FileCategory::Pdf || activeCategory == FileCategory::Office));
    shareMgr.populateMenu(menuShare);

    if (m_currentCategory == FileCategory::Folder) {
        menu.addAction(QIcon::fromTheme("package-x-generic"), tr("Compress / Create Archive..."), this, &BaseSkin::openCreateArchiveDialog);
    }

    if (m_currentCategory == FileCategory::Folder && m_folderViewer) {
        QAction *actShowFolders = menu.addAction(tr("Show Folders"));
        actShowFolders->setCheckable(true);
        actShowFolders->setChecked(m_folderViewer->showFolders());
        connect(actShowFolders, &QAction::toggled, m_folderViewer, &FolderViewer::setShowFolders);
    }

    menu.addSeparator();

    // 3. Category Specific Actions
    if (activeCategory == FileCategory::Image && !isFolder) {
        if (m_currentCategory != FileCategory::Folder) {
            if (m_isEditMode) {
                menu.addAction(QIcon::fromTheme("object-rotate-right"), tr("Rotate 90° Right"), this, &BaseSkin::rotateCurrentRight);
                menu.addAction(QIcon::fromTheme("object-rotate-left"), tr("Rotate 90° Left"), this, &BaseSkin::rotateCurrentLeft);
                menu.addAction(QIcon::fromTheme("object-flip-horizontal"), tr("Flip Horizontally"), this, &BaseSkin::flipCurrentHorizontal);
                menu.addSeparator();
            }
            menu.addAction(QIcon::fromTheme("document-save-as"), tr("Save As..."), this, &BaseSkin::saveAsCurrentFile);
            menu.addAction(QIcon::fromTheme("document-export"), tr("Convert / Export in Other Formats..."), this, &BaseSkin::exportCurrentFileAs);
            menu.addSeparator();
        }
    } else if (activeCategory == FileCategory::Pdf) {
        if (m_currentCategory != FileCategory::Folder) {
            menu.addAction(QIcon::fromTheme("document-save-as"), tr("Save As..."), this, &BaseSkin::saveAsCurrentFile);
            menu.addAction(QIcon::fromTheme("document-export"), tr("Export in Other Formats..."), this, &BaseSkin::exportCurrentFileAs);
            menu.addSeparator();
        } else {
            // PDF Tools when selecting a PDF card in Folder view
            if (BuildConfig::isQpdfAvailable()) {
                menu.addAction(QIcon::fromTheme("document-edit"), tr("Organize PDF Pages..."), this, [this, targetPath]() {
                    openPdfPageOrganizer(targetPath);
                });
                menu.addAction(QIcon::fromTheme("tools-check"), tr("Repair PDF..."), this, [this, targetPath]() {
                    openPdfRepairDialog(targetPath);
                });
            }
            if (BuildConfig::isPdfimagesAvailable()) {
                menu.addAction(QIcon::fromTheme("image-x-generic"), tr("Extract Images from PDF..."), this, [this, targetPath]() {
                    extractPdfImages(targetPath);
                });
            }
            menu.addSeparator();
        }
    } else if (activeCategory == FileCategory::Text && !isFolder) {
        if (m_currentCategory != FileCategory::Folder) {
            menu.addAction(QIcon::fromTheme("edit-find"), tr("Find in Document..."), this, &BaseSkin::searchInDocument);
            menu.addAction(QIcon::fromTheme("document-save-as"), tr("Save As..."), this, &BaseSkin::saveAsCurrentFile);
            menu.addAction(QIcon::fromTheme("document-export"), tr("Export in Other Formats..."), this, &BaseSkin::exportCurrentFileAs);
            menu.addSeparator();
        }
    } else if (activeCategory == FileCategory::Media && !isFolder) {
        if (m_currentCategory != FileCategory::Folder && m_mediaViewer) {
            menu.addAction(QIcon::fromTheme("media-playback-start"), tr("Play / Pause"), this, [this]() {
                if (m_mediaViewer) m_mediaViewer->togglePlayPause();
            });
            menu.addAction(QIcon::fromTheme("media-playback-stop"), tr("Stop"), this, [this]() {
                if (m_mediaViewer) m_mediaViewer->stop();
            });
            menu.addSeparator();
        }
    }

    // 4. Thumbnail Filmstrip Toggle
    if (m_currentCategory != FileCategory::Folder) {
        QAction *actFilmstrip = menu.addAction(QIcon::fromTheme("view-preview"), tr("Thumbnail Strip"), this, &BaseSkin::toggleFilmstrip);
        actFilmstrip->setCheckable(true);
        actFilmstrip->setChecked(m_filmstripWanted);
        menu.addSeparator();
    }

    // 5. File Operations & Properties
    menu.addAction(QIcon::fromTheme("user-trash"), tr("Move to Trash"), this, &BaseSkin::deleteCurrentFile);
    menu.addAction(QIcon::fromTheme("dialog-information"), tr("File Properties & Metadata..."), this, &BaseSkin::showFileInfo);

    menu.exec(globalPos);
}

bool BaseSkin::loadUnsupportedFileInfo(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) return false;

    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(info);
    if (icon.isNull()) {
        icon = QIcon::fromTheme("application-octet-stream", QIcon::fromTheme("unknown"));
    }
    m_lblUnsupportedIcon->setPixmap(icon.pixmap(96, 96));
    m_lblUnsupportedName->setText(info.fileName());

    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);

    QString details = QString(
        "<b>%1:</b> %2<br>"
        "<b>%3:</b> %4 (%5 byte)<br>"
        "<b>%6:</b> %7<br>"
        "<b>%8:</b> %9<br>"
        "<b>%10:</b> %11"
    ).arg(tr("MIME Type"), mimeType.name().isEmpty() ? tr("Unknown") : mimeType.name())
     .arg(tr("Size"), formatFileSize(info.size()), QString::number(info.size()))
     .arg(tr("Last modified"), info.lastModified().toString("yyyy-MM-dd hh:mm:ss"))
     .arg(tr("Permissions"), QString("%1%2%3")
          .arg(info.isReadable() ? "r" : "-")
          .arg(info.isWritable() ? "w" : "-")
          .arg(info.isExecutable() ? "x" : "-"))
     .arg(tr("Path"), info.absoluteFilePath());

    m_lblUnsupportedDetails->setText(details);
    return true;
}

void BaseSkin::openCurrentWithExternal()
{
    if (m_currentFilePath.isEmpty()) return;
    OpenWithDialog dlg(m_currentFilePath, this);
    if (dlg.exec() == QDialog::Accepted) {
        AppLauncher::launchApp(dlg.selectedApp(), m_currentFilePath);
    }
}

void BaseSkin::showCurrentHexDump()
{
    if (m_currentFilePath.isEmpty()) return;
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.read(65536); // read up to 64KB
    file.close();

    QString hexView;
    hexView.reserve(data.size() * 5);
    hexView += QString("--- HEX DUMP (primi %1 byte di %2): %3 ---\n\n")
        .arg(data.size())
        .arg(formatFileSize(QFileInfo(m_currentFilePath).size()))
        .arg(QFileInfo(m_currentFilePath).fileName());

    for (int i = 0; i < data.size(); i += 16) {
        // Offset
        hexView += QString("%1  ").arg(i, 8, 16, QChar('0'));

        // Hex bytes
        QString hexBytes;
        QString asciiBytes;
        for (int j = 0; j < 16; ++j) {
            if (i + j < data.size()) {
                unsigned char c = static_cast<unsigned char>(data.at(i + j));
                hexBytes += QString("%1 ").arg(c, 2, 16, QChar('0'));
                if (c >= 32 && c <= 126) {
                    asciiBytes += QChar(c);
                } else {
                    asciiBytes += '.';
                }
            } else {
                hexBytes += "   ";
            }
            if (j == 7) hexBytes += " ";
        }

        hexView += hexBytes + " |" + asciiBytes + "|\n";
    }

    if (m_textViewer) {
        m_stackedWidget->setCurrentIndex(1);
        m_textViewer->setPlainText(hexView);
    }
}

void BaseSkin::updateWindowTitle()
{
    const QString devBadge = (QCoreApplication::applicationDirPath().contains("build_deb") ||
                              QCoreApplication::applicationDirPath().contains("Progetti/BubezView"))
                             ? QStringLiteral(" [Sviluppo]") : QString();

    if (m_currentFilePath.isEmpty()) {
        setWindowTitle(tr("BubezView") + devBadge);
        return;
    }
    QFileInfo info(m_currentFilePath);
    QString mod;
    if (m_isEditMode) {
        if (m_stackedWidget->currentIndex() == 1 && m_textViewer && m_textViewer->document()->isModified()) {
            mod = "* ";
        } else if (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified()) {
            mod = "* ";
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer && m_pdfViewer->isModified()) {
            mod = "* ";
        }
    }

    if (!m_archiveOriginPath.isEmpty()) {
        QFileInfo originInfo(m_archiveOriginPath);
        setWindowTitle(QString("%1%2 (📦 %3) — BubezView%4").arg(mod, info.fileName(), originInfo.fileName(), devBadge));
    } else {
        setWindowTitle(QString("%1%2 — BubezView%3").arg(mod, info.fileName(), devBadge));
    }
}

