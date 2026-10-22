#include "base_skin.h"
#include "image_viewer.h"
#include "text_viewer.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "core/interfaces/i_archive_viewer.h"
#include "core/interfaces/i_media_viewer.h"
#include "viewers/media/system_media_viewer.h"
#include "engine_prompt_card.h"
#include "build_options_dialog.h"
#include "office_viewer.h"
#include "folder_viewer.h"
#include "theme_manager.h"
#include "settings_dialog.h"
#include "action_registry.h"
#include "search_bar_widget.h"
#include "open_with_dialog.h"
#include "app_launcher.h"
#include "preload_cache_manager.h"
#include "update_manager.h"
#include "update_dialog.h"
#include "ocr_manager.h"
#include "ocr_dialog.h"
#include "pdf_image_extract_dialog.h"
#include "pdf_repair_dialog.h"
#include "fallback_notice_dialog.h"
#include "build_config.h"
#include "package_manager_helper.h"
#include <QProgressDialog>
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
#include "goto_folder_dialog.h"
#include "share_dialog.h"
#include "share_manager.h"
#include "filmstrip_widget.h"
#include "image_inspector_dialog.h"
#include "image_resize_dialog.h"
#include "photo_studio_dialog.h"
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

        const QRectF mappedRect = tx.mapRect(QRectF(bounds));
        if (!mappedRect.contains(QRectF(bounds))) {
            QPainterPath outer;
            outer.addRect(QRectF(bounds));
            QPainterPath inner;
            inner.addRect(mappedRect);
            p.fillPath(outer.subtracted(inner), m_bgColor);
        }

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
    , m_overlayOpacity(nullptr)
    , m_animGroup(nullptr)
    , m_lastSwitchTimeMs(0)
    , m_slideshowTimer(nullptr)
{
    setWindowIcon(QIcon::fromTheme("bubezview", QIcon(":/icons/bubezview.png")));
    resize(1000, 700);

    QSettings settings;
    m_filmstripWanted = settings.value("ui/filmstrip", false).toBool();

    m_centralContainer = new QWidget(this);
    m_centralContainer->setAutoFillBackground(true);
    QVBoxLayout *centralLayout = new QVBoxLayout(m_centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(m_centralContainer);
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

    auto *pv = PluginManager::instance().createViewer(FileCategory::Pdf, this);
    m_pdfViewer = dynamic_cast<IPdfViewer*>(pv);

    auto *av = PluginManager::instance().createViewer(FileCategory::Archive, this);
    m_archiveViewer = dynamic_cast<IArchiveViewer*>(av);

    QSettings initSettings("Bubez", "BubezView");
    bool mpvEnabled = initSettings.value("modules/mpv_enabled", true).toBool();
    if (mpvEnabled) {
        auto *mv = PluginManager::instance().createViewer(FileCategory::Media, this);
        m_mediaViewer = dynamic_cast<IMediaViewer*>(mv);
    }
    if (!m_mediaViewer) {
        m_mediaViewer = new SystemMediaViewer(this);
    }

    m_officeViewer = new OfficeViewer(this);
    m_folderViewer = new FolderViewer(this);

    // 7. Unsupported File Technical Inspection Card
    m_unsupportedViewer = new QWidget(this);
    QVBoxLayout *unsupLay = new QVBoxLayout(m_unsupportedViewer);
    unsupLay->setAlignment(Qt::AlignCenter);
    unsupLay->setSpacing(14);

    QFrame *cardFrame = new QFrame(m_unsupportedViewer);
    cardFrame->setObjectName("unsupportedCard");
    cardFrame->setStyleSheet("#unsupportedCard { background-color: rgba(30, 41, 59, 0.7); border: 1px solid rgba(148, 163, 184, 0.25); border-radius: 12px; padding: 24px; }");
    cardFrame->setFixedWidth(540);

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
    m_stackedWidget->addWidget(m_pdfViewer ? m_pdfViewer->widget() : new QWidget(this));         // 2
    m_stackedWidget->addWidget(m_archiveViewer ? m_archiveViewer->widget() : new QWidget(this)); // 3
    m_stackedWidget->addWidget(m_mediaViewer ? m_mediaViewer->widget() : new QWidget(this));     // 4
    m_stackedWidget->addWidget(m_officeViewer);                                                  // 5
    m_stackedWidget->addWidget(m_folderViewer);                                                  // 6
    m_stackedWidget->addWidget(m_unsupportedViewer);                                             // 7
    m_stackedWidget->addWidget(m_enginePromptCard);                                              // 8

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

    if (m_mediaViewer) {
        m_mediaViewer->setOnTrackChangeRequested([this](const QString &path) {
            openFile(path);
        });
    }

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

    m_animGroup = new QParallelAnimationGroup(this);
    connect(m_animGroup, &QParallelAnimationGroup::finished, this, [this]() {
        if (m_transitionOverlay) {
            m_transitionOverlay->resetOverlay();
            if (m_stackedWidget) {
                m_transitionOverlay->setGeometry(m_stackedWidget->geometry());
            }
        }
    });

    // Sub-viewer signals
    if (m_archiveViewer) {
        m_archiveViewer->setOnFilePreviewRequested(
            [this](const QString &extractedFilePath, const QString &archiveOriginPath, const QString &internalEntryPath) {
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
    }

    ShareManager::setFolderCompressor([this](const QString &folderPath, const QString &destZipPath, QWidget *parentWidget) -> bool {
        if (!m_archiveViewer) return false;
        return m_archiveViewer->createArchive({folderPath}, destZipPath, QStringLiteral("zip"), QString(), parentWidget);
    });

    connect(m_folderViewer, &FolderViewer::folderLoaded, this, [this](const QString &folderPath, int, qint64) {
        if (m_currentCategory == FileCategory::Folder && !folderPath.isEmpty()) {
            m_currentFilePath = folderPath;
            updateWindowTitle();
        }
    });
    connect(m_folderViewer, &FolderViewer::filePreviewRequested, this, [this](const QString &path) {
        openFile(path);
    });
    connect(m_folderViewer, &FolderViewer::itemContextMenuRequested, this, [this](const QPoint &globalPos, const QString &) {
        showContextMenuAt(globalPos);
    });

    applyCurrentTheme();
}

BaseSkin::~BaseSkin()
{
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

BaseSkin::OpenResult BaseSkin::openFile(const QString &filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) {
        return OpenResult::Failed;
    }

    // Check for unsaved changes before opening another file
    if (!m_currentFilePath.isEmpty() && info.canonicalFilePath() != QFileInfo(m_currentFilePath).canonicalFilePath()) {
        if (!promptAndSaveIfModified()) {
            return OpenResult::Failed;
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
        if (!m_pdfViewer) {
            auto *pv = PluginManager::instance().createViewer(FileCategory::Pdf, this);
            m_pdfViewer = dynamic_cast<IPdfViewer*>(pv);
            if (m_pdfViewer) {
                QWidget *oldW = m_stackedWidget->widget(2);
                m_stackedWidget->removeWidget(oldW);
                delete oldW;
                m_stackedWidget->insertWidget(2, m_pdfViewer->widget());
            }
        }
        if (!m_pdfViewer) {
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
        if (!m_archiveViewer) {
            auto *av = PluginManager::instance().createViewer(FileCategory::Archive, this);
            m_archiveViewer = dynamic_cast<IArchiveViewer*>(av);
            if (m_archiveViewer) {
                QWidget *oldW = m_stackedWidget->widget(3);
                m_stackedWidget->removeWidget(oldW);
                delete oldW;
                m_stackedWidget->insertWidget(3, m_archiveViewer->widget());
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
            }
        }
        if (!m_archiveViewer) {
            m_enginePromptCard->setTargetFile(FileCategory::Archive, m_currentFilePath);
            m_stackedWidget->setCurrentWidget(m_enginePromptCard);
            return OpenResult::Success;
        }
        m_stackedWidget->setCurrentIndex(3);
        opened = m_archiveViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Media) {
        QSettings s("Bubez", "BubezView");
        const bool mpvEnabled = s.value("modules/mpv_enabled", true).toBool();
        const bool canUseMpv = mpvEnabled && PluginManager::instance().isEngineAvailable(FileCategory::Media);
        const bool isSystemViewer = (dynamic_cast<SystemMediaViewer*>(m_mediaViewer) != nullptr);

        if (!m_mediaViewer || (canUseMpv && isSystemViewer) || (!canUseMpv && !isSystemViewer)) {
            if (m_mediaViewer) {
                m_mediaViewer->stop();
            }
            IMediaViewer *newViewer = nullptr;
            if (canUseMpv) {
                auto *mv = PluginManager::instance().createViewer(FileCategory::Media, this);
                newViewer = dynamic_cast<IMediaViewer*>(mv);
            }
            if (!newViewer) {
                newViewer = new SystemMediaViewer(this);
            }
            QWidget *oldW = m_stackedWidget->widget(4);
            m_stackedWidget->removeWidget(oldW);
            delete oldW;
            m_mediaViewer = newViewer;
            m_stackedWidget->insertWidget(4, m_mediaViewer->widget());
            m_mediaViewer->setOnTrackChangeRequested([this](const QString &path) {
                openFile(path);
            });
        }
        m_stackedWidget->setCurrentIndex(4);
        m_mediaViewer->setSiblingFiles(m_siblingFiles);
        opened = m_mediaViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Office) {
        m_stackedWidget->setCurrentIndex(5);
        opened = m_officeViewer->loadFile(m_currentFilePath);
    } else if (m_currentCategory == FileCategory::Folder) {
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

        QList<UIActionItem> actions = ActionRegistry::getActionsForCategory(m_currentCategory, this);
        onFileLoaded(m_currentFilePath, m_currentCategory, actions);

        // Asynchronously prefetch adjacent candidate files into memory
        if (!m_siblingFiles.isEmpty() && m_currentIndex >= 0) {
            PreloadCacheManager::instance().requestPrefetchNeighbors(m_siblingFiles, m_currentIndex);
            PreloadCacheManager::instance().inspectFolderForPdfs(m_siblingFiles);
        }

        if (m_transitionOverlay && m_animGroup && m_animGroup->state() == QAbstractAnimation::Running) {
            m_transitionOverlay->raise();
        }

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
        QList<UIActionItem> actions = ActionRegistry::getActionsForCategory(m_currentCategory, this);
        onFileLoaded(extractedFilePath, m_currentCategory, actions);
    }
}

void BaseSkin::onFileLoaded(const QString &filePath, FileCategory category, const QList<UIActionItem> &actions)
{
    Q_UNUSED(filePath);
    Q_UNUSED(category);
    Q_UNUSED(actions);
    // Base implementation does nothing; overridden by derived skins
}

void BaseSkin::scanDirectory(const QString &currentFilePath)
{
    QFileInfo info(currentFilePath);
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
    if (m_stackedWidget->currentIndex() == 1 || m_stackedWidget->currentIndex() == 2) {
        if (m_searchBar) {
            repositionSearchBar();
            m_searchBar->showAndFocus();
            m_searchBar->raise();
        }
    } else {
        QMessageBox::information(this, tr("Find in Document"), tr("In-document text search is available for Text and PDF documents."));
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

    QFileInfo fi(m_currentFilePath);
    QString baseName = fi.completeBaseName();
    QString defaultExportPath = fi.dir().filePath(baseName + "_exported");

    QString filter;
    QString selectedFilter;

    if (m_currentCategory == FileCategory::Image) {
        filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp);;BMP Image (*.bmp);;TIFF Image (*.tiff);;PDF Document (*.pdf);;All Files (*)");
        defaultExportPath += ".png";
    } else if (m_currentCategory == FileCategory::Text) {
        filter = tr("Plain Text (*.txt);;Markdown Document (*.md);;HTML Document (*.html);;PDF Document (*.pdf);;All Files (*)");
        defaultExportPath += ".txt";
    } else if (m_currentCategory == FileCategory::Pdf) {
        filter = tr("PDF Document (*.pdf);;PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp);;TIFF Image (*.tiff);;BMP Image (*.bmp);;Plain Text (*.txt);;All Files (*)");
        defaultExportPath += ".pdf";
    } else if (m_currentCategory == FileCategory::Office) {
        filter = tr("PDF Document (*.pdf);;Plain Text (*.txt);;HTML Document (*.html);;All Files (*)");
        defaultExportPath += ".pdf";
    } else {
        filter = tr("All Files (*)");
        defaultExportPath = fi.dir().filePath(tr("copy_") + fi.fileName());
    }

    QString targetPath = QFileDialog::getSaveFileName(this, tr("Export As"), defaultExportPath, filter, &selectedFilter);
    if (targetPath.isEmpty()) return;

    bool success = false;
    QString ext = QFileInfo(targetPath).suffix().toLower();

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
            int totalPages = m_pdfViewer->pageCount();
            int curPage = m_pdfViewer->currentPage();
            bool exportAll = false;

            if (totalPages > 1) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("Export PDF as Image"));
                msgBox.setIcon(QMessageBox::Question);
                msgBox.setText(tr("This PDF contains %1 pages.<br>Which pages would you like to export as <b>%2</b>?")
                                   .arg(totalPages)
                                   .arg(ext.toUpper()));
                QPushButton *btnCurrent = msgBox.addButton(tr("Current Page (Page %1)").arg(curPage + 1), QMessageBox::ActionRole);
                QPushButton *btnAll = msgBox.addButton(tr("All Pages (%1)").arg(totalPages), QMessageBox::ActionRole);
                QPushButton *btnCancel = msgBox.addButton(QMessageBox::Cancel);
                msgBox.setDefaultButton(btnCurrent);
                msgBox.exec();

                if (msgBox.clickedButton() == btnCancel) {
                    return;
                }
                exportAll = (msgBox.clickedButton() == btnAll);
            }

            if (!exportAll) {
                QImage pageImg = m_pdfViewer->renderPageForOcr(curPage, 150);
                if (!pageImg.isNull()) {
                    success = pageImg.save(targetPath);
                }
            } else {
                QFileInfo targetFi(targetPath);
                QString baseDir = targetFi.dir().absolutePath();
                QString baseStem = targetFi.completeBaseName();
                int exportedCount = 0;

                for (int p = 0; p < totalPages; ++p) {
                    QImage pageImg = m_pdfViewer->renderPageForOcr(p, 150);
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
            if (QFile::exists(targetPath)) QFile::remove(targetPath);
            success = QFile::copy(m_currentFilePath, targetPath);
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
        QMessageBox::information(this, tr("Export As"), tr("File exported successfully to:\n%1").arg(targetPath));
    } else {
        QMessageBox::warning(this, tr("Export Error"), tr("Could not export file to:\n%1").arg(targetPath));
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
        if (!m_siblingFiles.isEmpty() && indexToRemove >= 0 && indexToRemove < m_siblingFiles.size()) {
            m_siblingFiles.removeAt(indexToRemove);
            if (m_siblingFiles.isEmpty()) {
                m_currentIndex = -1;
                m_currentFilePath.clear();
                setWindowTitle(tr("BubezView"));
                if (m_stackedWidget->currentIndex() == 0) m_imageViewer->clear();
                else if (m_stackedWidget->currentIndex() == 1) m_textViewer->clear();
                else if (m_stackedWidget->currentIndex() == 2) m_pdfViewer->clear();
                else if (m_stackedWidget->currentIndex() == 3) m_archiveViewer->clear();
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

void BaseSkin::saveCurrentFile()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified()) {
        if (m_currentFilePath.isEmpty()) {
            QMessageBox::warning(this, tr("Save Error"), tr("No active image file to save."));
            return;
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

            QMessageBox::information(this, tr("Save"), tr("Image modifications saved successfully!"));
        } else {
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save image changes to:\n%1\n\nPlease check write permissions or disk space.").arg(m_currentFilePath));
        }
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
            updateWindowTitle();
            updateStatusBar();
            QMessageBox::information(this, tr("Save As"), tr("File saved successfully to:\n%1").arg(target));
        }
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
    QMessageBox::StandardButton res = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("The document \"%1\" has unsaved changes.\nDo you want to save them before proceeding?").arg(docName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    if (res == QMessageBox::Save) {
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
    } else if (res == QMessageBox::Discard) {
        if (isImgMod) m_imageViewer->setModified(false);
        if (isTextMod) m_textViewer->document()->setModified(false);
        if (isPdfMod) m_pdfViewer->setModified(false);
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
}

void BaseSkin::closeEvent(QCloseEvent *event)
{
    if (!promptAndSaveIfModified()) {
        event->ignore();
        return;
    }

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

    if (keepAliveMin > 0) {
        event->ignore();
        hide();
        m_isDaemonMode = true;

        if (!m_backgroundTimer) {
            m_backgroundTimer = new QTimer(this);
            m_backgroundTimer->setSingleShot(true);
            connect(m_backgroundTimer, &QTimer::timeout, this, []() {
                qApp->quit();
            });
        }
        m_backgroundTimer->start(keepAliveMin * 60 * 1000);
    } else {
        if (!m_isDaemonMode) {
            // Ensure process exits completely and does not leave zombie background processes
            qApp->quit();
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
        QString editState = m_textViewer->isEditable() ? tr(" • ✏️ Edit Mode") : tr(" • 🔒 Read-Only");
        if (m_textViewer->document()->isModified()) {
            editState += tr(" (Modified*)");
        }
        return tr("%1 lines • %2 words • %3 characters • %4%5")
            .arg(m_textViewer->lineCount())
            .arg(m_textViewer->wordCount())
            .arg(m_textViewer->charCount())
            .arg(formatFileSize(info.size()))
            .arg(editState);
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

void BaseSkin::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (!m_archiveOriginPath.isEmpty() && QFile::exists(m_archiveOriginPath)) {
            QString returnArchive = m_archiveOriginPath;
            m_archiveOriginPath.clear();
            m_archiveEntries.clear();
            m_archiveCurrentEntryIndex = -1;
            openFile(returnArchive);
            event->accept();
            return;
        }
        close();
        event->accept();
        break;
    case Qt::Key_Q:
        close();
        event->accept();
        break;
    case Qt::Key_Space:
        if (m_stackedWidget->currentIndex() == 4) {
            m_mediaViewer->togglePlayPause();
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer->currentPage() < m_pdfViewer->pageCount() - 1) {
            m_pdfViewer->nextPage();
        } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer() && m_officeViewer->pdfViewer()->currentPage() < m_officeViewer->pdfViewer()->pageCount() - 1) {
            m_officeViewer->pdfViewer()->nextPage();
        } else {
            nextFile();
        }
        event->accept();
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        if (m_stackedWidget->currentIndex() == 6) {
            QMainWindow::keyPressEvent(event);
        } else {
            nextFile();
            event->accept();
        }
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
        if (m_stackedWidget->currentIndex() == 6) {
            QMainWindow::keyPressEvent(event);
        } else {
            prevFile();
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

void BaseSkin::rotateCurrentRight()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->rotateClockwise();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->rotateClockwise();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->rotateClockwise();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::rotateCurrentLeft()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->rotateCounterClockwise();
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
        m_pdfViewer->rotateCounterClockwise();
    } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
        m_officeViewer->pdfViewer()->rotateCounterClockwise();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::flipCurrentHorizontal()
{
    if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
        m_imageViewer->flipHorizontal();
    }
    updateWindowTitle();
    updateStatusBar();
}

void BaseSkin::flipCurrentVertical()
{
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

void BaseSkin::openAdjustImageDialog()
{
    if (!m_imageViewer || m_stackedWidget->currentIndex() != 0) return;
    const QImage &img = m_imageViewer->originalImage();
    if (img.isNull()) return;

    PhotoStudioDialog dlg(img, this);
    if (dlg.exec() == QDialog::Accepted) {
        QImage res = dlg.resultImage();
        if (!res.isNull()) {
            m_imageViewer->applyModifiedImage(res);
            updateWindowTitle();
            updateStatusBar();
        }
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
    if (target.isEmpty() || !QFile::exists(target)) return;

    PdfImageExtractDialog dlg(target, this);
    int count = dlg.loadImages();
    if (count > 0) {
        dlg.exec();
    } else if (count == 0) {
        QMessageBox::information(
            this,
            tr("No Images Found"),
            tr("No embedded raster images were found in this PDF document.")
        );
    } else if (count == -2) {
        if (PackageManagerHelper::installComponentInteractive("pdfimages", this)) {
            // Re-try after successful installation
            extractPdfImages(target);
        }
    } else {
        QMessageBox::warning(
            this,
            tr("Extraction Failed"),
            tr("Failed to extract images from PDF.<br>Make sure the 'poppler-utils' package is installed and the document is accessible.")
        );
    }
}

void BaseSkin::openPdfRepairDialog(const QString &filePath)
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
    if (target.isEmpty() && m_currentCategory == FileCategory::Pdf) {
        target = m_currentFilePath;
    }

    PdfRepairDialog dlg(target, this);
    connect(&dlg, &PdfRepairDialog::fileRepaired, this, [this](const QString &repairedPath) {
        openFile(repairedPath);
    });
    dlg.exec();
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

    if (m_pdfViewer) {
        m_pdfViewer->setOnFileOrganized([this](const QString &organizedPath) {
            openFile(organizedPath);
        });
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

void BaseSkin::toggleZoomTool(bool active)
{
    if (m_pdfViewer) {
        m_pdfViewer->toggleZoomTool(active);
    }
    if (m_imageViewer) {
        m_imageViewer->setZoomToolActive(active);
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
        return m_imageViewer->isZoomToolActive();
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
    if (m_currentFilePath.isEmpty()) return;
    QString dir;
    if (!m_archiveOriginPath.isEmpty()) {
        dir = QFileInfo(m_archiveOriginPath).absolutePath();
    } else {
        QFileInfo fi(m_currentFilePath);
        dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    }
    openFile(dir);
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
            m_folderViewer->loadFolder(m_folderViewer->currentFolder());
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
            m_folderViewer->loadFolder(m_folderViewer->currentFolder());
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
            m_folderViewer->loadFolder(currentDir);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Unable to create folder."));
        }
    }
}

void BaseSkin::openCreateArchiveDialog()
{
    if (!m_archiveViewer) {
        auto *av = PluginManager::instance().createViewer(FileCategory::Archive, this);
        m_archiveViewer = dynamic_cast<IArchiveViewer*>(av);
        if (m_archiveViewer) {
            QWidget *oldW = m_stackedWidget->widget(3);
            m_stackedWidget->removeWidget(oldW);
            delete oldW;
            m_stackedWidget->insertWidget(3, m_archiveViewer->widget());
        }
    }
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
        m_folderViewer->loadFolder(m_folderViewer->currentFolder());
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
    aboutBox.setText(
        QString("<h3>BubezView v%1</h3>").arg(QApplication::applicationVersion()) +
        "<p><b>" + tr("Native C++ / Qt6 Universal Viewer for Linux") + "</b></p>"
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
    QSettings settings("BubezView", "BubezView");
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

    // 1. Standard actions: Copy & Copy Path
    QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy"), this, &BaseSkin::copyToClipboard);
    actCopy->setShortcut(QKeySequence("Ctrl+C"));

    QAction *actCopyPath = menu.addAction(QIcon::fromTheme("edit-copy-path"), tr("Copy Path"), this, [targetPath]() {
        QApplication::clipboard()->setText(targetPath);
    });
    actCopyPath->setShortcut(QKeySequence("Ctrl+Shift+C"));

    menu.addSeparator();

    // 2. Open With... & Share submenus
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

    QMenu *menuShare = menu.addMenu(QIcon::fromTheme("document-share"), tr("Share"));
    ShareManager shareMgr(this, [targetPath]() { return targetPath; });
    shareMgr.populateMenu(menuShare);

    menu.addAction(QIcon::fromTheme("package-x-generic"), tr("Compress / Create Archive..."), this, &BaseSkin::openCreateArchiveDialog);

    menu.addSeparator();

    // 3. Category Specific Actions
    if (activeCategory == FileCategory::Image && !isFolder) {
        if (m_currentCategory != FileCategory::Folder) {
            menu.addAction(QIcon::fromTheme("object-rotate-right"), tr("Rotate 90° Right"), this, &BaseSkin::rotateCurrentRight);
            menu.addAction(QIcon::fromTheme("object-rotate-left"), tr("Rotate 90° Left"), this, &BaseSkin::rotateCurrentLeft);
            menu.addAction(QIcon::fromTheme("object-flip-horizontal"), tr("Flip Horizontally"), this, &BaseSkin::flipCurrentHorizontal);
            menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("Fit to Window"), this, &BaseSkin::fitCurrent);
            menu.addAction(QIcon::fromTheme("zoom-original"), tr("Original Size (100%)"), this, &BaseSkin::resetZoomCurrent);
            menu.addSeparator();
        }
    } else if (activeCategory == FileCategory::Pdf) {
        if (m_currentCategory != FileCategory::Folder) {
            menu.addAction(QIcon::fromTheme("object-rotate-right"), tr("Rotate Clockwise"), this, &BaseSkin::rotateCurrentRight);
            menu.addAction(QIcon::fromTheme("object-rotate-left"), tr("Rotate Counterclockwise"), this, &BaseSkin::rotateCurrentLeft);
            menu.addAction(QIcon::fromTheme("zoom-fit-best"), tr("Fit to Width"), this, &BaseSkin::fitCurrent);
            menu.addAction(QIcon::fromTheme("zoom-original"), tr("Original Size (100%)"), this, &BaseSkin::resetZoomCurrent);
        }

        // PDF Tools available in both direct PDF viewer and when selecting a PDF card in Folder view
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
    } else if (activeCategory == FileCategory::Text && !isFolder) {
        if (m_currentCategory != FileCategory::Folder) {
            menu.addAction(QIcon::fromTheme("edit-find"), tr("Find in Document..."), this, &BaseSkin::searchInDocument);
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
    if (m_currentFilePath.isEmpty()) {
        setWindowTitle(tr("BubezView"));
        return;
    }
    QFileInfo info(m_currentFilePath);
    QString mod;
    if (m_stackedWidget->currentIndex() == 1 && m_textViewer && m_textViewer->document()->isModified()) {
        mod = "* ";
    } else if (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified()) {
        mod = "* ";
    } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer && m_pdfViewer->isModified()) {
        mod = "* ";
    }

    if (!m_archiveOriginPath.isEmpty()) {
        QFileInfo originInfo(m_archiveOriginPath);
        setWindowTitle(QString("%1%2 (📦 %3) — BubezView").arg(mod, info.fileName(), originInfo.fileName()));
    } else {
        setWindowTitle(QString("%1%2 — BubezView").arg(mod, info.fileName()));
    }
}

