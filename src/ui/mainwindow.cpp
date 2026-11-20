#include "mainwindow.h"
#include "minimal_header_bar.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include <QWindow>
#include "image_viewer.h"
#include "text_viewer.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "core/interfaces/i_archive_viewer.h"
#include "office_viewer.h"
#include "folder_viewer.h"
#include "core/interfaces/i_media_viewer.h"
#include "share_dialog.h"
#include "share_manager.h"
#include "app_launcher.h"
#include "update_manager.h"
#include "update_dialog.h"
#include "build_config.h"
#include "package_manager_helper.h"
#include "engine_prompt_card.h"
#include "filmstrip_widget.h"
#include "preload_cache_manager.h"
#include "settings_dialog.h"
#include "core/interfaces/viewer_plugin_interface.h"
#include <QPluginLoader>
#include <QMenuBar>
#include <QStatusBar>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QDateTime>
#include <QClipboard>
#include <QProcess>
#include <QFrame>
#include <QMessageBox>
#include <QPushButton>
#include <QActionGroup>
#include <QPainter>
#include <QPainterPath>

namespace {
/**
 * @brief Lightweight click-through overlay that renders a smooth antialiased 12px rounded rim
 *        around the frameless window so outer corners never look jagged or boxy.
 */
class FramelessBorderOverlay : public QWidget {
public:
    explicit FramelessBorderOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

    void setLightTheme(bool isLight)
    {
        if (m_isLight != isLight) {
            m_isLight = isLight;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal radius = 18.0;
        const QRectF outerBounds = QRectF(rect());
        const QRectF innerBounds = outerBounds.adjusted(0.85, 0.85, -0.85, -0.85);

        // Clear exterior corner wedges outside the rounded curve so window corners are 100% transparent
        QPainterPath fullRectPath;
        fullRectPath.addRect(outerBounds);
        QPainterPath roundedPath;
        roundedPath.addRoundedRect(outerBounds, radius, radius);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(fullRectPath.subtracted(roundedPath), Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Draw glossy top-lit Liquid Glass gradient border ring around the window
        QLinearGradient rimGrad(0, 0, 0, height());
        if (m_isLight) {
            rimGrad.setColorAt(0.0, QColor(255, 255, 255, 220));
            rimGrad.setColorAt(0.25, QColor(15, 23, 42, 35));
            rimGrad.setColorAt(1.0, QColor(15, 23, 42, 55));
        } else {
            rimGrad.setColorAt(0.0, QColor(255, 255, 255, 58));
            rimGrad.setColorAt(0.35, QColor(255, 255, 255, 28));
            rimGrad.setColorAt(1.0, QColor(255, 255, 255, 16));
        }
        p.setPen(QPen(QBrush(rimGrad), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(innerBounds, radius, radius);
    }

private:
    bool m_isLight = false;
};
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : BaseSkin(parent)
    , m_minimalHeaderBar(nullptr)
    , m_updateBanner(nullptr)
    , m_lblUpdateBannerText(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    if (menuBar()) {
        menuBar()->hide();
        menuBar()->setEnabled(false);
    }
    setupUi();
    setupUpdateBanner();
    setupDebugBanner();
    setupUpdateChecker();

    setupPdfViewerConnections();

    if (m_imageViewer) {
        connect(m_imageViewer, &ImageViewer::zoomChanged, this, [this](double factor) {
            updateStatusBar();
            if (m_minimalHeaderBar) {
                m_minimalHeaderBar->setZoomFactor(factor);
            }
        });
        connect(m_imageViewer, &ImageViewer::modificationChanged, this, [this](bool) {
            updateStatusBar();
            updateWindowTitle();
        });
        connect(m_imageViewer, &ImageViewer::contextMenuRequested, this, &BaseSkin::showContextMenuAt);
    }
    setupArchiveViewerConnections();
    setupFolderViewerConnections();

    if (m_textViewer) {
        connect(m_textViewer, &TextViewer::editModeChanged, this, [this](bool) {
            updateStatusBar();
        });
    }

    if (qApp) {
        qApp->installEventFilter(this);
    }
    setEditMode(false);
    applyCurrentTheme();
}

MainWindow::~MainWindow()
{
    if (m_isBorderCursorSet) {
        QGuiApplication::restoreOverrideCursor();
        m_isBorderCursorSet = false;
    }
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::setupUi()
{
    if (m_centralContainer) {
        m_centralContainer->setStyleSheet("#centralContainer { border: none; }");
    }
    setCentralWidget(m_centralContainer);

    setupGlobalShortcuts();

    // =========================================================================
    // Minimal Header Bar (macOS Quick Look style with Canvas-First interface)
    // =========================================================================
    QSettings settings("Bubez", "BubezView");
    m_minimalHeaderBar = new MinimalHeaderBar(this);
    m_minimalHeaderBar->applyThemePreset(ThemeManager::currentPreset());
    bool frameless = settings.value("ui/frameless_window", true).toBool();
    m_isFrameless = frameless;
    if (frameless) {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        m_minimalHeaderBar->setWindowControlsVisible(true);
    } else {
        m_minimalHeaderBar->setWindowControlsVisible(false);
    }

    connect(m_minimalHeaderBar, &MinimalHeaderBar::closeRequested, this, &QWidget::close);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::maximizeRestoreRequested, this, [this]() {
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::prevRequested, this, &BaseSkin::prevFile);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::nextRequested, this, &BaseSkin::nextFile);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::galleryRequested, this, &BaseSkin::openCurrentFolderInGallery);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::searchRequested, this, &BaseSkin::searchInDocument);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomInRequested, this, &BaseSkin::zoomInCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomOutRequested, this, &BaseSkin::zoomOutCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomFitRequested, this, &BaseSkin::fitCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomFitWidthRequested, this, &BaseSkin::fitWidthCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomFitPageRequested, this, &BaseSkin::fitPageCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomResetRequested, this, &BaseSkin::resetZoomCurrent);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::zoomSetPercentRequested, this, [this](int percent) {
        double factor = percent / 100.0;
        if (m_stackedWidget->currentIndex() == 0 && m_imageViewer) {
            m_imageViewer->setZoomFactor(factor);
        } else if (m_stackedWidget->currentIndex() == 2 && m_pdfViewer) {
            m_pdfViewer->setZoomFactor(factor);
        } else if (m_stackedWidget->currentIndex() == 5 && m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) {
            m_officeViewer->pdfViewer()->setZoomFactor(factor);
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::handToolToggleRequested, this, [this]() {
        bool newState = !isZoomToolActive();
        toggleZoomTool(newState);
        m_minimalHeaderBar->setHandToolChecked(newState);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::inspectorRequested, this, &BaseSkin::showFileInfo);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::trashRequested, this, &BaseSkin::deleteCurrentFile);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::shareRequested, this, &BaseSkin::showShareDialog);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::saveExportRequested, this, &MainWindow::showSaveExportMenu);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::exportRequested, this, &BaseSkin::exportCurrentFileAs);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::saveAsRequested, this, &BaseSkin::saveAsCurrentFile);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::settingsRequested, this, &BaseSkin::openSettings);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::editToggled, this, &MainWindow::setEditMode);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::discardEditRequested, this, &MainWindow::revertCurrentEdits);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::undoRequested, this, &MainWindow::undo);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::redoRequested, this, &MainWindow::redo);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::historyStepRestoreRequested, this, [this](int idx) {
        if (m_currentCategory == FileCategory::Image && m_imageViewer) {
            m_imageViewer->restoreHistoryStep(idx);
        } else if (m_currentCategory == FileCategory::Pdf && m_pdfViewer) {
            m_pdfViewer->restoreHistoryStep(idx);
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::showHistoryTimelineRequested, this, [this]() {
        if (m_currentCategory == FileCategory::Image && m_imageViewer) {
            m_minimalHeaderBar->setHistorySteps(m_imageViewer->historySteps());
        } else if (m_currentCategory == FileCategory::Pdf && m_pdfViewer) {
            m_minimalHeaderBar->setHistorySteps(m_pdfViewer->historySteps());
        }
    });

    // Direct Contextual Action Bar Text Replacement
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTextReplaceRequested, this, [this](const QString &text, const QFont &font, const QColor &color, bool whiteout) {
        if (m_pdfViewer) {
            m_pdfViewer->replaceSelectedText(text, font, color, whiteout);
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTextPropertiesChanged, this, [this](const QFont &font, const QColor &color, bool whiteout) {
        if (m_pdfViewer) {
            m_pdfViewer->updateTypewriterProperties(font, color, whiteout);
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTextBgChanged, this, [this](PdfTextBgMode mode, const QColor &color) {
        if (m_pdfViewer) {
            m_pdfViewer->updateTextBgProperties(mode, color);
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTextDoneRequested, this, [this]() {
        if (m_pdfViewer) {
            m_pdfViewer->commitInPlaceTextEdit();
        }
    });

    if (m_minimalHeaderBar && m_minimalHeaderBar->contextualBar()) {
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::highlightPropertiesChanged, this, [this](const QColor &color, double opacity, bool /*isSpotlight*/) {
            if (m_pdfViewer) {
                m_pdfViewer->updateHighlighterProperties(color, opacity);
            }
        });
    }

    // Contextual Edit Action Connections from MinimalHeaderBar
    // Image
    connect(m_minimalHeaderBar, &MinimalHeaderBar::rotateLeftRequested, this, &BaseSkin::rotateCurrentLeft);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::rotateRightRequested, this, &BaseSkin::rotateCurrentRight);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::flipHRequested, this, [this]() {
        if (m_imageViewer) m_imageViewer->flipHorizontal();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::flipVRequested, this, [this]() {
        if (m_imageViewer) m_imageViewer->flipVertical();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::cropRequested, this, [this]() {
        if (m_imageViewer) m_imageViewer->setCropModeActive(!m_imageViewer->isCropModeActive());
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::imageResizeRequested, this, &BaseSkin::openResizeImageDialog);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::imageInvertRequested, this, [this]() {
        if (m_imageViewer) m_imageViewer->invertColors();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::imageInspectorRequested, this, &BaseSkin::showFileInfo);

    // Office Contextual Actions
    connect(m_minimalHeaderBar, &MinimalHeaderBar::officeEditLoRequested, this, [this]() {
        if (m_officeViewer) {
            QMetaObject::invokeMethod(m_officeViewer, "onOpenInLibreOffice");
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::officeSavePdfRequested, this, [this]() {
        if (m_officeViewer) {
            QMetaObject::invokeMethod(m_officeViewer, "onSaveAsPdf");
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::officeExtractImagesRequested, this, [this]() {
        if (m_officeViewer) {
            QMetaObject::invokeMethod(m_officeViewer, "onExtractImages");
        }
    });

    // PDF Tools - Direct Contextual Actions
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTextToolToggled, this, [this](bool active) {
        if (!m_pdfViewer) return;
        if (active) {
            auto *cBar = m_minimalHeaderBar->contextualBar();
            if (cBar && m_pdfViewer->hasTextSelection()) {
                m_pdfViewer->editSelectedTextWithWhiteout();
            } else if (cBar) {
                m_pdfViewer->startDirectTypewriter(cBar->textFont(), cBar->textColor(), cBar->isWhiteout());
            }
        } else {
            m_pdfViewer->exitContinuousToolMode();
        }
    });

    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfHighlightToolToggled, this, [this](bool active) {
        if (!m_pdfViewer) return;
        if (active) {
            auto *cBar = m_minimalHeaderBar->contextualBar();
            m_pdfViewer->startHighlighter(cBar->highlighterColor(), cBar->highlighterOpacity(), cBar->isSpotlight());
        } else {
            m_pdfViewer->exitContinuousToolMode();
        }
    });

    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfShapesToolRequested, this, [this](int shapeType) {
        if (!m_pdfViewer) return;
        auto *cBar = m_minimalHeaderBar->contextualBar();
        m_pdfViewer->startShape(shapeType, cBar->strokeWidth(), cBar->strokeColor(), cBar->fillColor());
    });

    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfRedactToolRequested, this, [this]() {
        if (!m_pdfViewer) return;
        auto *cBar = m_minimalHeaderBar->contextualBar();
        QColor col = (cBar && cBar->redactStyle() == 1) ? Qt::white : Qt::black;
        bool perm = cBar ? cBar->isRedactPermanent() : false;
        m_pdfViewer->startRedaction(col, perm);
    });

    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfStampToolRequested, this, [this]() {
        if (!m_pdfViewer) return;
        auto *cBar = m_minimalHeaderBar->contextualBar();
        if (cBar) {
            m_pdfViewer->startWatermark(cBar->stampText(), cBar->stampAngle(), 0.90, cBar->stampColor(),
                                        QPoint(), cBar->stampStyle(), cBar->stampSize());
        }
    });

    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfDrawingToolRequested, this, [this](int toolId) {
        if (!m_pdfViewer) return;
        auto *cBar = m_minimalHeaderBar->contextualBar();
        if (toolId == 0) { // Highlighter
            m_pdfViewer->startHighlighter(cBar->highlighterColor(), cBar->highlighterOpacity(), cBar->isSpotlight());
        } else if (toolId == 5) { // Redact
            m_pdfViewer->startRedaction();
        } else {
            // Shapes: toolId 1=Rect (0), 2=Circle (1), 3=Arrow (2), 4=Line (3)
            int shapeType = toolId - 1;
            m_pdfViewer->startShape(shapeType, cBar->strokeWidth(), cBar->strokeColor(), cBar->fillColor());
        }
    });

    // Contextual Action Bar property modifications
    if (m_minimalHeaderBar->contextualBar()) {
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::shapePropertiesChanged, this, [this](int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor) {
            if (!m_pdfViewer) return;
            m_pdfViewer->startShape(shapeType, strokeWidth, strokeColor, fillColor);
            m_pdfViewer->updateShapeProperties(strokeWidth, strokeColor, fillColor);
        });
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::redactPropertiesChanged, this, [this](int style, bool permanent) {
            if (!m_pdfViewer) return;
            QColor col = (style == 1) ? Qt::white : Qt::black;
            m_pdfViewer->startRedaction(col, permanent);
        });
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::stampPropertiesChanged, this, [this](const QString &text, const QColor &color, qreal angle, int style, int fontSize) {
            if (!m_pdfViewer) return;
            m_pdfViewer->startWatermark(text, angle, 0.90, color, QPoint(), style, fontSize);
            m_pdfViewer->updateSelectedStampProperties(text, color, angle, style, fontSize);
        });
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::stampPropertiesCommitted, this, [this]() {
            if (!m_pdfViewer) return;
            m_pdfViewer->commitSelectedStampProperties();
        });
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::closeRequested, this, [this]() {
            if (m_pdfViewer) {
                m_pdfViewer->exitContinuousToolMode();
            }
        });
        connect(m_minimalHeaderBar->contextualBar(), &ContextualActionBar::drawingPropertiesChanged, this, [this](int toolId, int strokeWidth, const QColor &strokeColor, const QColor &fillColor) {
            if (!m_pdfViewer) return;
            auto *cBar = m_minimalHeaderBar->contextualBar();
            if (cBar && cBar->mode() == ContextBarMode::Highlight) {
                m_pdfViewer->startHighlighter(strokeColor, cBar->highlighterOpacity(), cBar->isSpotlight());
            } else if (cBar && cBar->mode() == ContextBarMode::Redact) {
                m_pdfViewer->startRedaction();
            } else if (cBar && cBar->mode() == ContextBarMode::Shapes) {
                m_pdfViewer->startShape(cBar->activeShapeType(), strokeWidth, strokeColor, fillColor);
                m_pdfViewer->updateShapeProperties(strokeWidth, strokeColor, fillColor);
            }
        });
    }

    // Signature Dropdown Actions
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfSignatureRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openSignatureDialog();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfInsertImageRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openInsertImageDialog();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfCryptoSignatureRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openCryptoSignatureDialog(1);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfScanSignatureFieldsRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openCryptoSignatureDialog(0);
    });

    // Page Dropdown Actions
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfInsertBlankPageRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->insertBlankPage();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfDeletePageRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->deleteCurrentPage();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfPagesRequested, this, [this]() {
        if (m_pdfViewer && m_pdfViewer->isPageGridOrganizerActive()) {
            exitPageOrganizerMode();
        } else {
            enterPageOrganizerMode();
        }
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerBackRequested, this, [this]() {
        exitPageOrganizerMode();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerRotateRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerRotate();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerDeleteRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerDelete();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerInsertBlankRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerInsertBlank();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerAddPdfRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerAddPdf();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerReverseRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerReverse();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOrganizerResetRequested, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->organizerReset();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfMergeRequested, this, &MainWindow::openPdfMergeDialog);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfRepairRequested, this, [this]() {
        openPdfRepairDialog(m_currentFilePath);
    });

    // Save & Export Dropdown Actions
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfSavePageAsPdfRequested, this, &BaseSkin::saveCurrentPageAsPdf);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfSavePageAsImageRequested, this, &BaseSkin::saveCurrentPageAsImage);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfOcrRequested, this, &BaseSkin::extractOcrCurrentFile);
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfExtractImagesRequested, this, [this]() {
        extractPdfImages(currentActiveTargetFilePath());
    });

    // Additional PDF Tools & Backwards Compatibility
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfStudioRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->toggleStudioPanel();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfStampRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->openWatermarkDialog();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfHighlightRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->startHighlighter(QColor("#fbbf24"), 0.5);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfSelectAreaRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->startSelectArea();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfDrawRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->startSelectArea();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfTypewriterRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->openTypewriterDialog();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfShapeRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->openShapeDialog();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::pdfRedactRequested, this, [this]() {
        if (promotePdfViewerToStudio() && m_pdfViewer) m_pdfViewer->startRedaction();
    });

    // Archive
    connect(m_minimalHeaderBar, &MinimalHeaderBar::archiveExtractRequested, this, [this]() {
        if (m_archiveViewer) m_archiveViewer->extractAll();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::archiveAddRequested, this, [this]() {
        if (m_archiveViewer) m_archiveViewer->addFilesToArchive();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::archiveRemoveRequested, this, [this]() {
        if (m_archiveViewer) m_archiveViewer->removeSelectedFromArchive();
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::archiveTestRequested, this, [this]() {
        if (m_archiveViewer) m_archiveViewer->testArchiveIntegrity();
    });

    // Text
    connect(m_minimalHeaderBar, &MinimalHeaderBar::textLineNumbersToggled, this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setLineNumbersVisible(checked);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::textSyntaxToggled, this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setSyntaxHighlightingEnabled(checked);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::textWordWrapToggled, this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setWordWrapEnabled(checked);
    });

    // Audio (1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags, 6=Summary)
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioStudioRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(1);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioRestoreRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(2);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioPitchRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(3);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioEqualizerRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(4);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioTagsRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(5);
    });
    connect(m_minimalHeaderBar, &MinimalHeaderBar::audioSummaryRequested, this, [this]() {
        if (m_mediaViewer) m_mediaViewer->switchAudioWorkspace(6);
    });

    // Folder Studio (0=Duplicates, 1=Recovery, 2=Organize, 3=Batch)
    connect(m_minimalHeaderBar, &MinimalHeaderBar::folderStudioToolRequested, this, [this](int toolIndex) {
        if (m_folderViewer) m_folderViewer->setStudioTool(toolIndex);
    });

    setupMediaViewerConnections();

    // Embed minimal header bar at top of central container
    auto *centralLayout = qobject_cast<QVBoxLayout*>(m_centralContainer->layout());
    if (centralLayout) {
        centralLayout->insertWidget(0, m_minimalHeaderBar);
        m_minimalHeaderBar->setOverlayContainer(m_centralContainer);
    }

    // Keep classic QStatusBar hidden in the canvas-first MinimalHeaderBar skin
    if (QStatusBar *bar = statusBar()) {
        bar->hide();
        bar->setFixedHeight(0);
        bar->setMaximumHeight(0);
        bar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
}

void MainWindow::setEditMode(bool editMode)
{
    const bool wasInEdit = isEditMode();

    if (!editMode && wasInEdit) {
        // Exiting Edit Mode: prompt to save or discard unsaved changes
        if (!promptAndSaveIfModified()) {
            if (m_minimalHeaderBar) {
                m_minimalHeaderBar->setEditMode(true);
            }
            return;
        }
    }

    if (editMode && !wasInEdit) {
        // Save window geometry and maximization state before transitioning to full screen
        m_wasMaximizedBeforeEdit = isMaximized() || isFullScreen();
        if (!m_wasMaximizedBeforeEdit) {
            m_preEditGeometry = geometry();
            showMaximized();
        }

        BaseSkin::setEditMode(true);
        if (!isEditMode()) {
            if (m_minimalHeaderBar) {
                m_minimalHeaderBar->setEditMode(false);
            }
            if (!m_wasMaximizedBeforeEdit && (isMaximized() || isFullScreen())) {
                const bool isOffscreen = (QGuiApplication::platformName() == QLatin1String("offscreen"));
                showNormal();
                if ((isOffscreen || !isVisible()) &&
                    m_preEditGeometry.isValid() && m_preEditGeometry.width() >= 320 && m_preEditGeometry.height() >= 240) {
                    setGeometry(m_preEditGeometry);
                }
            }
            return;
        }

        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->setAudioWorkspaceToolsAvailable(m_mediaViewer && m_mediaViewer->isAudioOnly());
            m_minimalHeaderBar->setEditMode(true);
            m_minimalHeaderBar->setHandToolChecked(false);
        }

        if (m_mediaViewer && m_mediaViewer->isAudioOnly()) {
            if (m_mediaViewer->audioWorkspaceIndex() == 0) {
                m_mediaViewer->switchAudioWorkspace(1);
                if (m_minimalHeaderBar) m_minimalHeaderBar->setActiveAudioWorkspace(1);
            }
        }
        if (m_currentCategory != FileCategory::Pdf) {
            fitCurrent();
        }
    } else if (!editMode && wasInEdit) {
        // First exit edit mode in BaseSkin so viewers hide editing tools and collapse toolbars
        BaseSkin::setEditMode(false);

        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->setAudioWorkspaceToolsAvailable(false);
            m_minimalHeaderBar->setEditMode(false);
            m_minimalHeaderBar->setHandToolChecked(true);
        }

        if (m_mediaViewer && m_mediaViewer->isAudioOnly()) {
            m_mediaViewer->switchAudioWorkspace(0);
            if (m_minimalHeaderBar) m_minimalHeaderBar->setActiveAudioWorkspace(0);
        }

        // Flush any pending layout updates so window size hints are clean prior to unmaximizing
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        if (layout()) {
            layout()->activate();
        }
        if (m_centralContainer && m_centralContainer->layout()) {
            m_centralContainer->layout()->activate();
        }

        // Return to compact preview window if not maximized prior to edit mode
        if (!m_wasMaximizedBeforeEdit && (isMaximized() || isFullScreen())) {
            const bool isOffscreen = (QGuiApplication::platformName() == QLatin1String("offscreen"));
            showNormal();
            // On real X11/Wayland window managers, showNormal() atomically restores the
            // pre-maximized normalGeometry() now that minimumSizeHint is never inflated.
            // Calling setGeometry() immediately after showNormal() on X11/Wayland sends a
            // competing configure request that causes a visible bounce. Only call setGeometry()
            // in headless offscreen tests or when the window is not yet mapped.
            if ((isOffscreen || !isVisible()) &&
                m_preEditGeometry.isValid() && m_preEditGeometry.width() >= 320 && m_preEditGeometry.height() >= 240) {
                setGeometry(m_preEditGeometry);
            }
        }
        fitCurrent();
    } else {
        BaseSkin::setEditMode(editMode);
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->setAudioWorkspaceToolsAvailable(m_isEditMode && m_mediaViewer && m_mediaViewer->isAudioOnly());
            m_minimalHeaderBar->setEditMode(editMode);
            m_minimalHeaderBar->setHandToolChecked(!editMode);
        }
    }
}

void MainWindow::setupGlobalShortcuts()
{
    auto addWindowAction = [this](const QKeySequence &shortcut, auto slot) -> QAction* {
        QAction *act = new QAction(this);
        act->setShortcut(shortcut);
        act->setShortcutContext(Qt::WindowShortcut);
        connect(act, &QAction::triggered, this, slot);
        addAction(act);
        return act;
    };

    auto addWindowActionMulti = [this](const QList<QKeySequence> &shortcuts, auto slot) -> QAction* {
        QAction *act = new QAction(this);
        act->setShortcuts(shortcuts);
        act->setShortcutContext(Qt::WindowShortcut);
        connect(act, &QAction::triggered, this, slot);
        addAction(act);
        return act;
    };

    // File Operations
    addWindowAction(QKeySequence::Open, &BaseSkin::openFileDialog);
    addWindowActionMulti({QKeySequence("Ctrl+Shift+O"), QKeySequence("Ctrl+G")}, &BaseSkin::openFolderDialog);
    addWindowAction(QKeySequence::Save, &BaseSkin::saveCurrentFile);
    addWindowAction(QKeySequence::SaveAs, &BaseSkin::saveAsCurrentFile);
    addWindowAction(QKeySequence("Ctrl+E"), &BaseSkin::exportCurrentFileAs);
    addWindowAction(QKeySequence::Print, &BaseSkin::printCurrentFile);
    addWindowAction(QKeySequence("Ctrl+Shift+C"), &BaseSkin::copyPathToClipboard);
    addWindowAction(QKeySequence("Ctrl+Return"), &BaseSkin::openWithDefault);
    auto isTextInputFocused = [this]() -> bool {
        QWidget *fw = focusWidget();
        while (fw) {
            if (qobject_cast<QLineEdit*>(fw) ||
                qobject_cast<QTextEdit*>(fw) ||
                qobject_cast<QPlainTextEdit*>(fw) ||
                qobject_cast<QAbstractSpinBox*>(fw)) {
                return true;
            }
            fw = fw->parentWidget();
        }
        return false;
    };

    addWindowAction(QKeySequence::Delete, [this, isTextInputFocused]() {
        if (isTextInputFocused()) {
            return;
        }
        if (m_isEditMode && m_currentCategory == FileCategory::Text) {
            return;
        }
        deleteCurrentFile();
    });
    addWindowAction(QKeySequence("Ctrl+Q"), &QWidget::close);
    addWindowAction(QKeySequence("Ctrl+,"), &BaseSkin::openSettings);

    // Edit Operations
    addWindowAction(QKeySequence::Undo, &MainWindow::undo);
    addWindowActionMulti({QKeySequence::Redo, QKeySequence("Ctrl+Shift+Z")}, &MainWindow::redo);
    addWindowAction(QKeySequence("Ctrl+X"), &BaseSkin::cutToClipboard);
    addWindowAction(QKeySequence("Ctrl+C"), &BaseSkin::copyToClipboard);
    addWindowAction(QKeySequence("Ctrl+V"), &BaseSkin::pasteFromClipboard);

    // Image / Canvas Transformations
    addWindowActionMulti({QKeySequence("R"), QKeySequence("Ctrl+R")}, [this, isTextInputFocused]() {
        if (isTextInputFocused()) return;
        rotateCurrentRight();
    });
    addWindowActionMulti({QKeySequence("L"), QKeySequence("Ctrl+Shift+R")}, [this, isTextInputFocused]() {
        if (isTextInputFocused()) return;
        rotateCurrentLeft();
    });
    addWindowAction(QKeySequence("V"), [this, isTextInputFocused]() {
        if (isTextInputFocused()) return;
        flipCurrentVertical();
    });
    addWindowAction(QKeySequence("C"), [this, isTextInputFocused]() {
        if (isTextInputFocused()) return;
        if (m_isEditMode && m_imageViewer) {
            m_imageViewer->setCropModeActive(!m_imageViewer->isCropModeActive());
        }
    });

    // View & Navigation
    addWindowAction(QKeySequence(Qt::Key_Left), [this, isTextInputFocused]() {
        if (isTextInputFocused() || m_isEditMode || m_currentCategory == FileCategory::Folder) return;
        prevFile();
    });
    addWindowAction(QKeySequence(Qt::Key_Right), [this, isTextInputFocused]() {
        if (isTextInputFocused() || m_isEditMode || m_currentCategory == FileCategory::Folder) return;
        nextFile();
    });
    addWindowAction(QKeySequence(Qt::Key_PageUp), [this]() {
        if (m_pdfViewer) m_pdfViewer->prevPage();
        else if (m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) m_officeViewer->pdfViewer()->prevPage();
    });
    addWindowAction(QKeySequence(Qt::Key_PageDown), [this]() {
        if (m_pdfViewer) m_pdfViewer->nextPage();
        else if (m_officeViewer && m_officeViewer->isPdfMode() && m_officeViewer->pdfViewer()) m_officeViewer->pdfViewer()->nextPage();
    });
    addWindowAction(QKeySequence("Alt+Up"), &BaseSkin::openCurrentFolderInGallery);
    addWindowAction(QKeySequence("F11"), &BaseSkin::toggleFullscreen);
    addWindowAction(QKeySequence("F5"), &BaseSkin::toggleSlideshow);
    addWindowAction(QKeySequence("F4"), &BaseSkin::toggleFilmstrip);
    addWindowActionMulti({QKeySequence("Alt+Return"), QKeySequence("Ctrl+I")}, &BaseSkin::showFileInfo);

    // Zoom & Pan
    addWindowActionMulti({QKeySequence("Ctrl++"), QKeySequence("Ctrl+=")}, &BaseSkin::zoomInCurrent);
    addWindowAction(QKeySequence("Ctrl+-"), &BaseSkin::zoomOutCurrent);
    addWindowAction(QKeySequence("Ctrl+0"), &BaseSkin::resetZoomCurrent);
    addWindowAction(QKeySequence("Ctrl+9"), &BaseSkin::fitCurrent);
    addWindowAction(QKeySequence("H"), [this, isTextInputFocused]() {
        if (isTextInputFocused()) return;
        if (m_isEditMode) {
            flipCurrentHorizontal();
        } else {
            bool newState = !isZoomToolActive();
            toggleZoomTool(newState);
            if (m_minimalHeaderBar) {
                m_minimalHeaderBar->setHandToolChecked(newState);
            }
        }
    });

    // Search & Tools
    addWindowAction(QKeySequence("Ctrl+F"), &BaseSkin::searchInDocument);
    addWindowAction(QKeySequence("F1"), &BaseSkin::showShortcutsDialog);
    addWindowAction(QKeySequence("Ctrl+T"), [this]() {
        setEditMode(!isEditMode());
    });
}

void MainWindow::undo()
{
    if (m_pdfViewer && m_pdfViewer->canUndo()) {
        m_pdfViewer->undo();
    } else if (m_textViewer && m_textViewer->document()->isUndoAvailable()) {
        m_textViewer->undo();
    } else if (m_imageViewer && m_imageViewer->canUndo()) {
        m_imageViewer->undo();
        if (!m_isEditMode) {
            saveCurrentImage(true);
        }
    }
}

void MainWindow::redo()
{
    if (m_pdfViewer && m_pdfViewer->canRedo()) {
        m_pdfViewer->redo();
    } else if (m_textViewer && m_textViewer->document()->isRedoAvailable()) {
        m_textViewer->redo();
    } else if (m_imageViewer && m_imageViewer->canRedo()) {
        m_imageViewer->redo();
        if (!m_isEditMode) {
            saveCurrentImage(true);
        }
    }
}

void MainWindow::updateWindowTitle()
{
    BaseSkin::updateWindowTitle();
    if (m_minimalHeaderBar) {
        if (!m_currentFilePath.isEmpty()) {
            QFileInfo fi(m_currentFilePath);
            m_minimalHeaderBar->setTitle(fi.fileName());
        } else {
            m_minimalHeaderBar->setTitle(tr("BubezView"));
        }
        m_minimalHeaderBar->setIndexBadge(fileIndexText());
        m_minimalHeaderBar->setAudioWorkspaceToolsAvailable(m_isEditMode && m_mediaViewer && m_mediaViewer->isAudioOnly());
        m_minimalHeaderBar->setCategory(m_currentCategory);
        m_minimalHeaderBar->setSearchAvailable(m_currentCategory == FileCategory::Text || m_currentCategory == FileCategory::Pdf || m_currentCategory == FileCategory::Archive);
    }
}

void MainWindow::onFileLoaded(const QString &filePath, FileCategory category)
{
    if (m_minimalHeaderBar) {
        if (!filePath.isEmpty()) {
            QFileInfo fi(filePath);
            m_minimalHeaderBar->setTitle(fi.fileName());
        } else {
            m_minimalHeaderBar->setTitle(tr("BubezView"));
        }
        m_minimalHeaderBar->setIndexBadge(fileIndexText());
        m_minimalHeaderBar->setAudioWorkspaceToolsAvailable(m_isEditMode && m_mediaViewer && m_mediaViewer->isAudioOnly());
        m_minimalHeaderBar->setCategory(category);
        m_minimalHeaderBar->setSearchAvailable(category == FileCategory::Text || category == FileCategory::Pdf || category == FileCategory::Archive);
        if (category == FileCategory::Text && m_textViewer) {
            m_minimalHeaderBar->setTextEditStates(
                m_textViewer->isLineNumbersVisible(),
                m_textViewer->isSyntaxHighlightingEnabled(),
                m_textViewer->isWordWrapEnabled()
            );
        }
    }

    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    // Status bar is intentionally hidden in the canvas-first MinimalHeaderBar skin.
}

void MainWindow::applyCurrentTheme()
{
    BaseSkin::applyCurrentTheme();
    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    if (m_minimalHeaderBar) {
        m_minimalHeaderBar->setUiScale(ThemeManager::uiScalePercent());
        m_minimalHeaderBar->applyThemePreset(preset);
    }
    if (m_centralContainer) {
        const int radius = (m_isFrameless && !isMaximized() && !isFullScreen()) ? 18 : 0;
        const double alpha = qBound(50, ThemeManager::glassOpacity(), 100) / 100.0;
        const QString aStr = QString::number(alpha, 'f', 2);
        if (preset == ThemePreset::ZincGray) {
            m_centralContainer->setStyleSheet(
                QStringLiteral("#centralContainer { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
                               "stop:0 rgba(62, 63, 68, %2), stop:0.5 rgba(52, 53, 58, %2), stop:1 rgba(42, 43, 47, %2)); "
                               "border: none; border-radius: %1px; }").arg(QString::number(radius), aStr)
            );
        } else if (ThemeManager::isDark(preset)) {
            m_centralContainer->setStyleSheet(
                QStringLiteral("#centralContainer { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
                               "stop:0 rgba(30, 37, 56, %2), stop:0.5 rgba(21, 26, 40, %2), stop:1 rgba(14, 18, 28, %2)); "
                               "border: none; border-radius: %1px; }").arg(QString::number(radius), aStr)
            );
        } else {
            m_centralContainer->setStyleSheet(
                QStringLiteral("#centralContainer { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
                               "stop:0 rgba(238, 241, 246, %2), stop:0.5 rgba(228, 232, 238, %2), stop:1 rgba(216, 221, 228, %2)); "
                               "border: none; border-radius: %1px; }").arg(QString::number(radius), aStr)
            );
        }
    }
    updateFramelessBorderStyle();
    updateStatusBar();
}

void MainWindow::setupUpdateBanner()
{
    if (!m_centralContainer || !m_centralContainer->layout()) return;

    m_updateBanner = new QFrame(m_centralContainer);
    m_updateBanner->setObjectName("updateBanner");
    m_updateBanner->setStyleSheet(
        "#updateBanner {"
        "   background-color: #0c4a6e;"
        "   border-bottom: 2px solid #0284c7;"
        "   padding: 4px 12px;"
        "}"
    );

    QHBoxLayout *bannerLayout = new QHBoxLayout(m_updateBanner);
    bannerLayout->setContentsMargins(12, 6, 12, 6);
    bannerLayout->setSpacing(10);

    QLabel *iconLabel = new QLabel("🚀", m_updateBanner);
    iconLabel->setStyleSheet("font-size: 16px; background: transparent;");
    bannerLayout->addWidget(iconLabel);

    m_lblUpdateBannerText = new QLabel(m_updateBanner);
    m_lblUpdateBannerText->setStyleSheet("color: #f0f9ff; font-weight: bold; font-size: 13px; background: transparent;");
    bannerLayout->addWidget(m_lblUpdateBannerText, 1);

    QPushButton *btnUpdateNow = new QPushButton(tr("Update Now"), m_updateBanner);
    btnUpdateNow->setCursor(Qt::PointingHandCursor);
    btnUpdateNow->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "   padding: 4px 14px;"
        "   border-radius: 4px;"
        "   border: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #0369a1;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #075985;"
        "}"
    );
    connect(btnUpdateNow, &QPushButton::clicked, this, [this]() {
        if (!m_availableRelease.version.isEmpty()) {
            showUpdateDialog(m_availableRelease);
        }
    });
    bannerLayout->addWidget(btnUpdateNow);

    QPushButton *btnCloseBanner = new QPushButton("✕", m_updateBanner);
    btnCloseBanner->setCursor(Qt::PointingHandCursor);
    btnCloseBanner->setToolTip(tr("Dismiss"));
    btnCloseBanner->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #94a3b8;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "   border: none;"
        "   padding: 2px 6px;"
        "}"
        "QPushButton:hover {"
        "   color: #ffffff;"
        "   background-color: rgba(255, 255, 255, 0.1);"
        "   border-radius: 4px;"
        "}"
    );
    connect(btnCloseBanner, &QPushButton::clicked, this, [this]() {
        if (m_updateBanner) {
            m_updateBanner->hide();
        }
    });
    bannerLayout->addWidget(btnCloseBanner);

    m_updateBanner->hide();

    // Insert banner immediately below MinimalHeaderBar (index 1) so top rounded header corners remain intact
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(m_centralContainer->layout());
    if (vbox) {
        const int insertIdx = (m_minimalHeaderBar && vbox->indexOf(m_minimalHeaderBar) == 0) ? 1 : 0;
        vbox->insertWidget(insertIdx, m_updateBanner, 0);
    }
}

void MainWindow::setupDebugBanner()
{
    // Banner appears only if debug mode was explicitly activated by user
    QSettings settings("Bubez", "BubezView");
    const bool debugActive = settings.value("debug_mode", false).toBool();
    if (!debugActive) {
        return;
    }

    m_debugBanner = new QFrame(this);
    m_debugBanner->setStyleSheet(
        "QFrame {"
        "   background-color: #7c2d12;"
        "   border-bottom: 1px solid #c2410c;"
        "   padding: 3px 12px;"
        "}"
    );

    QHBoxLayout *bannerLayout = new QHBoxLayout(m_debugBanner);
    bannerLayout->setContentsMargins(10, 4, 10, 4);
    bannerLayout->setSpacing(10);

    QLabel *iconLabel = new QLabel("🛠️", m_debugBanner);
    iconLabel->setStyleSheet("font-size: 15px; background: transparent;");
    bannerLayout->addWidget(iconLabel);

    QLabel *lblText = new QLabel(
        tr("<b>Debug Mode Active:</b> Remember to deactivate debug mode in Settings to restore standard fast user mode."),
        m_debugBanner
    );
    lblText->setStyleSheet("color: #ffedd5; font-size: 12px; font-weight: 500; background: transparent;");
    bannerLayout->addWidget(lblText, 1);

    QPushButton *btnSettings = new QPushButton(tr("Settings..."), m_debugBanner);
    btnSettings->setCursor(Qt::PointingHandCursor);
    btnSettings->setStyleSheet(
        "QPushButton {"
        "   background-color: #ea580c;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "   padding: 3px 12px;"
        "   border-radius: 4px;"
        "   border: none;"
        "}"
        "QPushButton:hover { background-color: #c2410c; }"
    );
    connect(btnSettings, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extra);
        dlg.exec();
    });
    bannerLayout->addWidget(btnSettings);

    QPushButton *btnCloseBanner = new QPushButton("✕", m_debugBanner);
    btnCloseBanner->setCursor(Qt::PointingHandCursor);
    btnCloseBanner->setToolTip(tr("Dismiss"));
    btnCloseBanner->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #fed7aa;"
        "   font-size: 13px;"
        "   font-weight: bold;"
        "   border: none;"
        "   padding: 2px 6px;"
        "}"
        "QPushButton:hover {"
        "   color: #ffffff;"
        "   background-color: rgba(255, 255, 255, 0.1);"
        "   border-radius: 4px;"
        "}"
    );
    connect(btnCloseBanner, &QPushButton::clicked, this, [this]() {
        if (m_debugBanner) {
            m_debugBanner->hide();
        }
    });
    bannerLayout->addWidget(btnCloseBanner);

    // Insert debug banner immediately below MinimalHeaderBar so frameless window controls remain at the top
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(m_centralContainer->layout());
    if (vbox) {
        const int insertIdx = (m_minimalHeaderBar && vbox->indexOf(m_minimalHeaderBar) == 0) ? 1 : 0;
        vbox->insertWidget(insertIdx, m_debugBanner, 0);
    }
}

void MainWindow::setupUpdateChecker()
{
    UpdateManager &mgr = UpdateManager::instance();

    connect(&mgr, &UpdateManager::updateAvailable, this, [this](const ReleaseInfo &info) {
        m_availableRelease = info;

        // 1. Configure and show top banner
        if (m_updateBanner && m_lblUpdateBannerText) {
            m_lblUpdateBannerText->setText(
                tr("A new version of BubezView (v%1) is available!").arg(info.version)
            );
            m_updateBanner->show();
        }

        // 2. Fire native desktop notification if notify-send is available
        QProcess::startDetached("notify-send", {
            "-a", "BubezView",
            "-i", "system-software-update",
            tr("BubezView Update Available"),
            tr("Version %1 is now available for download and installation.").arg(info.version)
        });
    });

    // Throttle automatic startup update checks to at most once every 24 hours
    // to avoid spinning up QNetworkAccessManager / TLS on every quick Spacebar preview
    QSettings settings("Bubez", "BubezView");
    const qint64 nowEpoch = QDateTime::currentSecsSinceEpoch();
    const qint64 lastCheckEpoch = settings.value("updates/last_check_epoch", 0).toLongLong();
    constexpr qint64 kUpdateIntervalSecs = 24 * 60 * 60;

    if (nowEpoch - lastCheckEpoch >= kUpdateIntervalSecs) {
        QTimer::singleShot(2500, this, [nowEpoch]() {
            QSettings s("Bubez", "BubezView");
            s.setValue("updates/last_check_epoch", nowEpoch);
            UpdateManager::instance().checkForUpdates(true);
        });
    }
}

void MainWindow::showUpdateDialog(const ReleaseInfo &info)
{
    UpdateDialog dlg(info, this);
    dlg.exec();
}

void MainWindow::openPdfMergeDialog()
{
    if (ensurePdfViewerCreated() && m_pdfViewer) {
        m_pdfViewer->openPdfMergeDialog(this);
    } else if (m_enginePromptCard) {
        m_enginePromptCard->setTargetFile(FileCategory::Pdf, currentActiveTargetFilePath());
        m_stackedWidget->setCurrentWidget(m_enginePromptCard);
    }
}

QString MainWindow::currentActiveTargetFilePath() const
{
    if (m_currentCategory == FileCategory::Folder && m_folderViewer) {
        QString sel = m_folderViewer->selectedFilePath();
        if (!sel.isEmpty() && QFile::exists(sel)) {
            return sel;
        }
    }
    return m_currentFilePath;
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMaximized() || isFullScreen()) {
            // Immediately clear rounded mask and border overlay when maximizing
            updateFramelessBorderStyle();
        } else {
            // When unmaximizing on X11/Wayland, WindowStateChange fires before the WM
            // sends the restored size ConfigureNotify. Avoid applying a full-screen 1-bit
            // rounded mask while the geometry is still at maximized dimensions; the
            // subsequent resizeEvent() will apply the mask at the restored compact size.
            const QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
            const bool stillAtScreenSize = scr &&
                (size() == scr->availableGeometry().size() || size() == scr->geometry().size());
            if (!stillAtScreenSize) {
                updateFramelessBorderStyle();
            }
        }
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->update();
        }
    }
    BaseSkin::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    BaseSkin::resizeEvent(event);
    updateFramelessBorderStyle();
    if (m_minimalHeaderBar) {
        m_minimalHeaderBar->updateContextualBarPosition();
    }
}

Qt::Edges MainWindow::calculateResizeEdges(const QPoint &globalPos) const
{
    if (!m_isFrameless || isMaximized() || isFullScreen()) {
        return Qt::Edges();
    }

    QPoint localPos = mapFromGlobal(globalPos);
    QRect r = rect();

    if (localPos.x() < 0 || localPos.y() < 0 || localPos.x() > r.width() || localPos.y() > r.height()) {
        return Qt::Edges();
    }

    Qt::Edges edges;
    if (localPos.x() <= RESIZE_BORDER_MARGIN) {
        edges |= Qt::LeftEdge;
    } else if (localPos.x() >= r.width() - RESIZE_BORDER_MARGIN) {
        edges |= Qt::RightEdge;
    }

    if (localPos.y() <= RESIZE_BORDER_MARGIN) {
        edges |= Qt::TopEdge;
    } else if (localPos.y() >= r.height() - RESIZE_BORDER_MARGIN) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void MainWindow::updateResizeCursor(Qt::Edges edges)
{
    if (!edges) {
        if (m_isBorderCursorSet) {
            unsetCursor();
            while (QGuiApplication::overrideCursor()) {
                QGuiApplication::restoreOverrideCursor();
            }
            m_isBorderCursorSet = false;
        }
        return;
    }

    QCursor resizeCursor;
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
        (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        resizeCursor = Qt::SizeFDiagCursor;
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
               (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        resizeCursor = Qt::SizeBDiagCursor;
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        resizeCursor = Qt::SizeHorCursor;
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        resizeCursor = Qt::SizeVerCursor;
    }

    setCursor(resizeCursor);
    m_isBorderCursorSet = true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_isFrameless || isMaximized() || isFullScreen()) {
        return BaseSkin::eventFilter(watched, event);
    }

    QWidget *widget = qobject_cast<QWidget*>(watched);
    if (!widget || widget->window() != this) {
        return BaseSkin::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseMove: {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (m_isManualResizing && (me->buttons() & Qt::LeftButton)) {
            performManualResize(me->globalPosition().toPoint());
            return true;
        } else if (me->buttons() == Qt::NoButton) {
            Qt::Edges edges = calculateResizeEdges(me->globalPosition().toPoint());
            updateResizeCursor(edges);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            Qt::Edges edges = calculateResizeEdges(me->globalPosition().toPoint());
            if (edges) {
                if (windowHandle() && windowHandle()->startSystemResize(edges)) {
                    me->accept();
                    return true;
                }
                // Manual resize fallback
                m_isManualResizing = true;
                m_manualResizeEdges = edges;
                m_manualResizeStartPos = me->globalPosition().toPoint();
                m_manualResizeStartGeom = geometry();
                me->accept();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_isManualResizing) {
            m_isManualResizing = false;
            updateResizeCursor(calculateResizeEdges(me->globalPosition().toPoint()));
            return true;
        }
        break;
    }
    case QEvent::Leave:
    case QEvent::WindowDeactivate:
    case QEvent::FocusOut: {
        if (m_isBorderCursorSet && !m_isManualResizing) {
            updateResizeCursor(Qt::Edges());
        }
        break;
    }
    default:
        break;
    }

    return BaseSkin::eventFilter(watched, event);
}

void MainWindow::performManualResize(const QPoint &globalPos)
{
    QPoint delta = globalPos - m_manualResizeStartPos;
    QRect newGeom = m_manualResizeStartGeom;
    const int minW = 400;
    const int minH = 300;

    if (m_manualResizeEdges & Qt::LeftEdge) {
        int targetWidth = m_manualResizeStartGeom.width() - delta.x();
        if (targetWidth >= minW) {
            newGeom.setLeft(m_manualResizeStartGeom.left() + delta.x());
        } else {
            newGeom.setLeft(m_manualResizeStartGeom.right() - minW);
        }
    }
    if (m_manualResizeEdges & Qt::RightEdge) {
        int targetWidth = m_manualResizeStartGeom.width() + delta.x();
        if (targetWidth >= minW) {
            newGeom.setRight(m_manualResizeStartGeom.left() + targetWidth);
        }
    }
    if (m_manualResizeEdges & Qt::TopEdge) {
        int targetHeight = m_manualResizeStartGeom.height() - delta.y();
        if (targetHeight >= minH) {
            newGeom.setTop(m_manualResizeStartGeom.top() + delta.y());
        } else {
            newGeom.setTop(m_manualResizeStartGeom.bottom() - minH);
        }
    }
    if (m_manualResizeEdges & Qt::BottomEdge) {
        int targetHeight = m_manualResizeStartGeom.height() + delta.y();
        if (targetHeight >= minH) {
            newGeom.setBottom(m_manualResizeStartGeom.top() + targetHeight);
        }
    }

    setGeometry(newGeom);
}

void MainWindow::updateFramelessBorderStyle()
{
    if (!m_centralContainer) return;

    if (m_isFrameless && !isMaximized() && !isFullScreen() && width() > 24 && height() > 24) {
        if (!m_borderOverlay) {
            m_borderOverlay = new FramelessBorderOverlay(this);
        }

        // Rely on WA_TranslucentBackground + FramelessBorderOverlay's antialiased CompositionMode_Clear
        // instead of a 1-bit QBitmap setMask() so all 4 rounded corners render smoothly without jagged edges.
        static_cast<FramelessBorderOverlay*>(m_borderOverlay)->setLightTheme(
            ThemeManager::isLight(ThemeManager::currentPreset())
        );
        m_borderOverlay->setGeometry(rect());
        m_borderOverlay->show();
        m_borderOverlay->raise();
        m_borderOverlay->update();
    } else {
        if (m_borderOverlay) {
            m_borderOverlay->hide();
        }
    }
}

void MainWindow::onViewerInitialized(FileCategory category)
{
    switch (category) {
    case FileCategory::Pdf:
        setupPdfViewerConnections();
        break;
    case FileCategory::Archive:
        setupArchiveViewerConnections();
        break;
    case FileCategory::Media:
        setupMediaViewerConnections();
        break;
    case FileCategory::Folder:
        setupFolderViewerConnections();
        break;
    default:
        break;
    }
}

void MainWindow::setupPdfViewerConnections()
{
    if (!m_pdfViewer) return;

    m_pdfViewer->setOnPageChanged([this](int, int) {
        updateStatusBar();
    });
    m_pdfViewer->setOnZoomChanged([this](double factor) {
        updateStatusBar();
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->setZoomFactor(factor);
        }
    });
    m_pdfViewer->setOnSelectionChanged([this]() {
        if (isEditMode() && m_minimalHeaderBar && m_minimalHeaderBar->contextualBar()) {
            auto *cBar = m_minimalHeaderBar->contextualBar();
            if (cBar->mode() == ContextBarMode::Text && m_pdfViewer && m_pdfViewer->hasTextSelection()) {
                cBar->setTextContent(m_pdfViewer->selectedText());
            }
        }
    });
    m_pdfViewer->setOnDocumentModified([this](bool) {
        updateWindowTitle();
        updateStatusBar();
    });
    m_pdfViewer->setOnFileOrganized([this](const QString &newPath) {
        openFile(newPath);
    });
    m_pdfViewer->setOnContinuousModeExited([this]() {
        if (m_minimalHeaderBar) {
            if (m_minimalHeaderBar->pdfTextButton() && m_minimalHeaderBar->pdfTextButton()->isChecked()) {
                m_minimalHeaderBar->pdfTextButton()->blockSignals(true);
                m_minimalHeaderBar->pdfTextButton()->setChecked(false);
                m_minimalHeaderBar->pdfTextButton()->blockSignals(false);
            }
            m_minimalHeaderBar->hideContextualBar();
        }
    });
    m_pdfViewer->setOnSavePageAsPdfRequested([this]() {
        saveCurrentPageAsPdf();
    });
    m_pdfViewer->setOnSavePageAsImageRequested([this]() {
        saveCurrentPageAsImage();
    });
    m_pdfViewer->setOnInPlaceTextEditStarted([this](const QString &text, const QFont &font, const QColor &color, PdfTextBgMode bgMode, const QColor &bgColor) {
        if (!isEditMode()) return;
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->showContextualTextBar(text, font, color, bgMode, bgColor);
        }
    });
    m_pdfViewer->setOnInPlaceTextEditFinished([this]() {
        if (m_minimalHeaderBar && m_minimalHeaderBar->contextualBar() &&
            m_minimalHeaderBar->contextualBar()->mode() == ContextBarMode::Text) {
            m_minimalHeaderBar->hideContextualBar();
        }
    });
    m_pdfViewer->setOnTextSelectedForContextualEdit([this](const QString &text, const QFont &font, const QColor &color, bool whiteout) {
        if (!isEditMode()) return;
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->showContextualTextBar(text, font, color, whiteout ? PdfTextBgMode::Auto : PdfTextBgMode::Transparent);
            auto *cBar = m_minimalHeaderBar->contextualBar();
            if (cBar) {
                cBar->setTextContent(text);
                cBar->setTextFont(font);
                cBar->setTextColor(color);
                cBar->setWhiteout(whiteout);
            }
        }
    });
    m_pdfViewer->setOnStampSelectedForContextualEdit([this](const QString &text, const QColor &color, qreal angle, int style, int fontSize) {
        if (!isEditMode()) return;
        if (m_minimalHeaderBar) {
            m_minimalHeaderBar->showContextualStampBar();
            auto *cBar = m_minimalHeaderBar->contextualBar();
            if (cBar) {
                cBar->setStampProperties(text, color, angle, style, fontSize);
            }
        }
    });
    m_pdfViewer->setOnPageOrganizerOpened([this]() {
        if (m_minimalHeaderBar && !m_minimalHeaderBar->isPageOrganizerMode()) {
            m_wasMaximizedBeforeOrganizer = isMaximized() || isFullScreen();
            if (!m_wasMaximizedBeforeOrganizer) {
                m_preOrganizerGeometry = geometry();
                showMaximized();
            }
            m_minimalHeaderBar->setPageOrganizerMode(true);
        }
    });
    m_pdfViewer->setOnPageOrganizerClosed([this](int targetPageIndex) {
        exitPageOrganizerMode(targetPageIndex);
    });
}

void MainWindow::setupArchiveViewerConnections()
{
    if (!m_archiveViewer) return;

    m_archiveViewer->setTopBarVisible(false);
    m_archiveViewer->setOnArchiveLoaded([this](const QString&, int count, qint64 totalSize) {
        if (m_minimalHeaderBar) {
            QString sizeStr;
            if (totalSize >= 1024 * 1024 * 1024) sizeStr = QString("%1 GB").arg(double(totalSize) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
            else if (totalSize >= 1024 * 1024) sizeStr = QString("%1 MB").arg(double(totalSize) / (1024.0 * 1024.0), 0, 'f', 1);
            else sizeStr = QString("%1 KB").arg(double(totalSize) / 1024.0, 0, 'f', 1);
            m_minimalHeaderBar->setIndexBadge(tr("%1 items • %2").arg(count).arg(sizeStr));
        }
        updateStatusBar();
    });
}

void MainWindow::setupMediaViewerConnections()
{
    if (!m_mediaViewer) return;

    m_mediaViewer->setOnAudioWorkspaceChanged([this](int idx) {
        if (m_minimalHeaderBar) m_minimalHeaderBar->setActiveAudioWorkspace(idx);
    });
    m_mediaViewer->setOnAudioWorkspaceModified([this](int idx, bool modified) {
        if (m_minimalHeaderBar) m_minimalHeaderBar->setAudioWorkspaceModified(idx, modified);
    });
}

void MainWindow::setupFolderViewerConnections()
{
    if (!m_folderViewer) return;

    connect(m_folderViewer, &FolderViewer::folderLoaded, this, [this](const QString&, int, qint64) {
        updateStatusBar();
    });
    connect(m_folderViewer, &FolderViewer::selectionChanged, this, &MainWindow::updateStatusBar);
}

void MainWindow::revertCurrentEdits()
{
    bool isImgMod = (m_stackedWidget->currentIndex() == 0 && m_imageViewer && m_imageViewer->isModified());
    bool isTextMod = (m_stackedWidget->currentIndex() == 1 && m_textViewer && m_textViewer->document()->isModified());
    bool isPdfMod = (m_stackedWidget->currentIndex() == 2 && m_pdfViewer && m_pdfViewer->isModified());
    bool isAudMod = (m_mediaViewer && m_mediaViewer->isAudioOnly() && m_mediaViewer->audioWorkspaceIndex() > 0);

    if (isImgMod || isTextMod || isPdfMod || isAudMod) {
        QMessageBox::StandardButton res = QMessageBox::question(
            this,
            tr("Discard Changes"),
            tr("Are you sure you want to discard all changes made to this document?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (res != QMessageBox::Yes) {
            return;
        }
    }

    if (m_imageViewer && isImgMod) {
        m_imageViewer->loadFile(m_currentFilePath);
        m_imageViewer->setModified(false);
    }
    if (m_textViewer && isTextMod) {
        m_textViewer->loadFile(m_currentFilePath);
        m_textViewer->document()->setModified(false);
    }
    if (m_pdfViewer && isPdfMod) {
        m_pdfViewer->loadFile(m_currentFilePath);
        m_pdfViewer->setModified(false);
    }
    if (m_mediaViewer && isAudMod) {
        m_mediaViewer->switchAudioWorkspace(0);
    }

    updateWindowTitle();
    updateStatusBar();

    setEditMode(false);
}

void MainWindow::showSaveExportMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QColor icColor = ThemeManager::iconColor(ThemeManager::currentPreset());

    QAction *actSave = menu.addAction(VectorIcons::createUiIcon(VectorIcons::UiIcon::Save, icColor, 16), tr("Save"), this, &BaseSkin::saveCurrentFile);
    actSave->setShortcut(QKeySequence::Save);

    QAction *actSaveAs = menu.addAction(VectorIcons::createUiIcon(VectorIcons::UiIcon::Save, icColor, 16), tr("Save As..."), this, &BaseSkin::saveAsCurrentFile);
    actSaveAs->setShortcut(QKeySequence::SaveAs);
    menu.addSeparator();

    QAction *actExport = menu.addAction(VectorIcons::createUiIcon(VectorIcons::UiIcon::ExternalLink, icColor, 16), tr("Export..."), this, &BaseSkin::exportCurrentFileAs);
    actExport->setShortcut(QKeySequence("Ctrl+E"));

    menu.addSeparator();
    QAction *actPrint = menu.addAction(VectorIcons::createUiIcon(VectorIcons::UiIcon::File, icColor, 16), tr("Print..."), this, &BaseSkin::printCurrentFile);
    actPrint->setShortcut(QKeySequence::Print);

    menu.exec(globalPos);
}

void MainWindow::enterPageOrganizerMode()
{
    if (!m_pdfViewer) return;
    if (m_pdfViewer->isPageGridOrganizerActive()) return;

    m_wasMaximizedBeforeOrganizer = isMaximized() || isFullScreen();
    if (!m_wasMaximizedBeforeOrganizer) {
        m_preOrganizerGeometry = geometry();
        showMaximized();
    }

    if (m_minimalHeaderBar) {
        m_minimalHeaderBar->setPageOrganizerMode(true);
    }

    m_pdfViewer->showPageGridOrganizer();
}

void MainWindow::exitPageOrganizerMode(int targetPageIndex)
{
    if (!m_pdfViewer) return;
    const bool organizerActive = (m_minimalHeaderBar && m_minimalHeaderBar->isPageOrganizerMode()) ||
                                 m_pdfViewer->isPageGridOrganizerActive();
    if (!organizerActive) return;

    if (m_minimalHeaderBar) {
        m_minimalHeaderBar->setPageOrganizerMode(false);
    }
    if (m_pdfViewer->isPageGridOrganizerActive()) {
        m_pdfViewer->hidePageGridOrganizer(targetPageIndex);
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    if (layout()) layout()->activate();
    if (m_centralContainer && m_centralContainer->layout()) m_centralContainer->layout()->activate();

    if (!m_wasMaximizedBeforeOrganizer && (isMaximized() || isFullScreen())) {
        const bool isOffscreen = (QGuiApplication::platformName() == QLatin1String("offscreen"));
        showNormal();
        if ((isOffscreen || !isVisible()) &&
            m_preOrganizerGeometry.isValid() && m_preOrganizerGeometry.width() >= 320 && m_preOrganizerGeometry.height() >= 240) {
            setGeometry(m_preOrganizerGeometry);
        }
    }
}





