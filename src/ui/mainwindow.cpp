#include "mainwindow.h"
#include "theme_manager.h"
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
#include "pdf_merge_dialog.h"
#include "build_config.h"
#include "package_manager_helper.h"
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
#include <QClipboard>
#include <QProcess>
#include <QFrame>
#include <QMessageBox>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : BaseSkin(parent)
    , m_toolbar(nullptr)
    , m_flippableToolbar(nullptr)
    , m_breadcrumbWidget(nullptr)
    , m_breadcrumbLayout(nullptr)
    , m_fileIndexLabel(nullptr)
    , m_fileInfoLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_shareManager(nullptr)
    , m_menuShare(nullptr)
    , m_menuOpenWith(nullptr)
    , m_menuFile(nullptr)
    , m_menuEdit(nullptr)
    , m_menuView(nullptr)
    , m_menuTools(nullptr)
    , m_menuSettings(nullptr)
    , m_menuHelp(nullptr)
    , m_actSave(nullptr)
    , m_actSaveAs(nullptr)
    , m_actExportAs(nullptr)
    , m_actTrashFile(nullptr)
    , m_actPrint(nullptr)
    , m_actUndo(nullptr)
    , m_actRedo(nullptr)
    , m_actCut(nullptr)
    , m_actCopy(nullptr)
    , m_actPaste(nullptr)
    , m_actRotateRight(nullptr)
    , m_actRotateLeft(nullptr)
    , m_actFlipH(nullptr)
    , m_actFlipV(nullptr)
    , m_actFolderRename(nullptr)
    , m_actFolderTrash(nullptr)
    , m_actFolderNew(nullptr)
    , m_actCreateArchive(nullptr)
    , m_actArchiveAddFiles(nullptr)
    , m_actArchiveAddFolder(nullptr)
    , m_actArchiveRemoveSelected(nullptr)
    , m_actToggleTextEdit(nullptr)
    , m_actLineNumbers(nullptr)
    , m_actSyntaxHighlight(nullptr)
    , m_actWordWrap(nullptr)
    , m_actShowWhitespace(nullptr)
    , m_actGotoLine(nullptr)
    , m_actFindReplace(nullptr)
    , m_actDuplicateLine(nullptr)
    , m_actDeleteLine(nullptr)
    , m_actToggleComment(nullptr)
    , m_actOcr(nullptr)
    , m_actSearch(nullptr)
    , m_actPdfToc(nullptr)
    , m_actExtractImages(nullptr)
    , m_actToolsExportAs(nullptr)
    , m_actPrev(nullptr)
    , m_actNext(nullptr)
    , m_actFolderView(nullptr)
    , m_actZoomIn(nullptr)
    , m_actZoomOut(nullptr)
    , m_actZoomReset(nullptr)
    , m_actFit(nullptr)
    , m_actZoomTool(nullptr)
    , m_actSlideshow(nullptr)
    , m_actFilmstrip(nullptr)
    , m_actPdfHighlighter(nullptr)
    , m_actPdfTypewriter(nullptr)
    , m_actPdfSignature(nullptr)
    , m_actPdfWatermark(nullptr)
    , m_actPdfShape(nullptr)
    , m_actPdfSnapshot(nullptr)
    , m_actPdfSelectArea(nullptr)
    , m_actPdfRedact(nullptr)
    , m_menuPdfMarkup(nullptr)
    , m_menuPdfDocOps(nullptr)
    , m_actAudioTrim(nullptr)
    , m_actAudioTags(nullptr)
    , m_actAudioExtract(nullptr)
    , m_actMergePdf(nullptr)
    , m_actImageCrop(nullptr)
    , m_actOpenFile(nullptr)
    , m_actOpenFolder(nullptr)
    , m_actOpenWith(nullptr)
    , m_actCopyPath(nullptr)
    , m_actFullscreen(nullptr)
    , m_actInspector(nullptr)
    , m_actPdfPrevPage(nullptr)
    , m_actPdfNextPage(nullptr)
    , m_actConfigureToolbars(nullptr)
    , m_actToggleToolbar(nullptr)
    , m_actSettings(nullptr)
    , m_actCheckUpdates(nullptr)
    , m_actShortcuts(nullptr)
    , m_actAbout(nullptr)
    , m_actShowUninstalledFeatures(nullptr)
    , m_actShowUnsupportedFiles(nullptr)
    , m_actOfficeDiscovery(nullptr)
    , m_actQpdfDiscovery(nullptr)
    , m_actPdfimagesDiscovery(nullptr)
    , m_btnUpdateNotification(nullptr)
    , m_updateBlinkTimer(nullptr)
    , m_updateBlinkState(false)
    , m_updateBanner(nullptr)
    , m_lblUpdateBannerText(nullptr)
{
    setupUi();
    setupUpdateBanner();
    setupDebugBanner();
    setupUpdateChecker();

    if (m_pdfViewer) {
        m_pdfViewer->setOnPageChanged([this](int, int) {
            updateStatusBar();
            updateContextualMenuStates();
        });
        m_pdfViewer->setOnZoomChanged([this](double) {
            updateStatusBar();
        });
        m_pdfViewer->setOnSelectionChanged([this]() {
            updateContextualMenuStates();
        });
        m_pdfViewer->setOnDocumentModified([this](bool) {
            updateContextualMenuStates();
        });
        m_pdfViewer->setOnHistoryChanged([this]() {
            updateContextualMenuStates();
        });
        m_pdfViewer->setOnFileOrganized([this](const QString &newPath) {
            openFile(newPath);
        });
    }

    if (m_imageViewer) {
        connect(m_imageViewer, &ImageViewer::zoomChanged, this, [this](double) {
            updateStatusBar();
        });
        connect(m_imageViewer, &ImageViewer::modificationChanged, this, [this](bool) {
            updateContextualMenuStates();
            updateStatusBar();
            updateWindowTitle();
        });
        connect(m_imageViewer, &ImageViewer::undoAvailabilityChanged, this, [this](bool, bool) {
            updateContextualMenuStates();
        });
        connect(m_imageViewer, &ImageViewer::cropModeChanged, this, [this](bool) {
            updateContextualMenuStates();
        });
        connect(m_imageViewer, &ImageViewer::nextFileRequested, this, &BaseSkin::nextFile);
        connect(m_imageViewer, &ImageViewer::prevFileRequested, this, &BaseSkin::prevFile);
    }
    if (m_archiveViewer) {
        m_archiveViewer->setOnArchiveLoaded([this](const QString&, int, qint64) {
            updateStatusBar();
        });
    }
    connect(m_folderViewer, &FolderViewer::folderLoaded, this, [this](const QString&, int, qint64) {
        updateStatusBar();
    });
    connect(m_folderViewer, &FolderViewer::selectionChanged, this, &MainWindow::updateContextualMenuStates);

    if (m_textViewer) {
        connect(m_textViewer, &TextViewer::editModeChanged, this, [this](bool editable) {
            if (m_actToggleTextEdit) {
                m_actToggleTextEdit->setChecked(editable);
            }
            updateStatusBar();
            updateContextualMenuStates();
        });
        connect(m_textViewer, &TextViewer::modificationChanged, this, [this](bool) {
            updateContextualMenuStates();
            updateStatusBar();
            updateWindowTitle();
        });
        connect(m_textViewer, &QPlainTextEdit::selectionChanged, this, &MainWindow::updateContextualMenuStates);
        connect(m_textViewer->document(), &QTextDocument::undoAvailable, this, [this](bool) {
            updateContextualMenuStates();
        });
        connect(m_textViewer->document(), &QTextDocument::redoAvailable, this, [this](bool) {
            updateContextualMenuStates();
        });
    }

    // FlippableToolbar annotation synchronization
    if (m_flippableToolbar) {
        connect(m_flippableToolbar, &FlippableToolbar::highlighterParamsChanged, this, [this](const QColor &c, double op) {
            if (m_pdfViewer) m_pdfViewer->startHighlighter(c, op);
        });
        connect(m_flippableToolbar, &FlippableToolbar::shapeParamsChanged, this, [this](int type, int width, const QColor &stroke, const QColor &fill) {
            if (m_pdfViewer) m_pdfViewer->startShape(type, width, stroke, fill);
        });
        connect(m_flippableToolbar, &FlippableToolbar::annotationModeExited, this, [this]() {
            if (m_pdfViewer) m_pdfViewer->exitContinuousToolMode();
        });
        if (m_pdfViewer) {
            m_pdfViewer->setOnContinuousModeExited([this]() {
                if (m_flippableToolbar) m_flippableToolbar->flipToMain();
            });
        }
    }
}

void MainWindow::setupUi()
{
    setCentralWidget(m_centralContainer);

    // =========================================================================
    // 1. FILE MENU
    // =========================================================================
    m_menuFile = menuBar()->addMenu(tr("&File"));
    
    m_actOpenFile = m_menuFile->addAction(QIcon::fromTheme("document-open"), tr("Open File..."), this, &BaseSkin::openFileDialog);
    m_actOpenFile->setShortcut(QKeySequence::Open);

    m_actOpenFolder = m_menuFile->addAction(QIcon::fromTheme("folder-open"), tr("Open Folder..."), this, &BaseSkin::openFolderDialog);
    m_actOpenFolder->setShortcuts({QKeySequence("Ctrl+Shift+O"), QKeySequence("Ctrl+G")});

    m_menuFile->addSeparator();

    m_actOpenWith = m_menuFile->addAction(QIcon::fromTheme("system-run"), tr("Open with Default Application"), this, &BaseSkin::openWithDefault);
    m_actOpenWith->setShortcut(QKeySequence("Ctrl+Return"));

    m_menuOpenWith = m_menuFile->addMenu(QIcon::fromTheme("system-run"), tr("Open With..."));
    connect(m_menuOpenWith, &QMenu::aboutToShow, this, &MainWindow::populateOpenWithMenu);

    m_actSave = m_menuFile->addAction(QIcon::fromTheme("document-save"), tr("Save Changes"), this, &BaseSkin::saveCurrentFile);
    m_actSave->setShortcut(QKeySequence("Ctrl+S"));
    m_actSave->setEnabled(false);

    m_actExportAs = m_menuFile->addAction(QIcon::fromTheme("document-export"), tr("Export As..."), this, &BaseSkin::exportCurrentFileAs);
    m_actExportAs->setShortcut(QKeySequence("Ctrl+E"));

    m_actSaveAs = m_menuFile->addAction(QIcon::fromTheme("document-save-as"), tr("Save Copy As..."), this, &BaseSkin::saveAsCurrentFile);
    m_actSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

    m_actCreateArchive = m_menuFile->addAction(QIcon::fromTheme("package-x-generic"), tr("Compress / Create Archive..."), this, &BaseSkin::openCreateArchiveDialog);
    m_actCreateArchive->setShortcut(QKeySequence("Ctrl+Alt+C"));

    m_actTrashFile = m_menuFile->addAction(QIcon::fromTheme("user-trash"), tr("Move to Trash"), this, &BaseSkin::deleteCurrentFile);
    m_actTrashFile->setShortcut(QKeySequence::Delete);

    m_menuFile->addSeparator();

    m_actPrint = m_menuFile->addAction(QIcon::fromTheme("document-print"), tr("Print..."), this, &BaseSkin::printCurrentFile);
    m_actPrint->setShortcut(QKeySequence("Ctrl+P"));

    // Share Submenu
    m_menuShare = m_menuFile->addMenu(QIcon::fromTheme("document-share"), tr("Share"));
    m_shareManager = new ShareManager(this, [this]() { return currentShareFilePath(); });
    m_shareManager->populateMenu(m_menuShare);
    connect(m_menuShare, &QMenu::aboutToShow, this, [this]() {
        m_shareManager->populateMenu(m_menuShare);
    });

    m_actCopyPath = m_menuFile->addAction(QIcon::fromTheme("edit-copy-path"), tr("Copy Path"), this, &BaseSkin::copyPathToClipboard);
    m_actCopyPath->setShortcut(QKeySequence("Ctrl+Shift+C"));

    m_menuFile->addSeparator();

    connect(m_menuFile, &QMenu::aboutToShow, this, &MainWindow::updateContextualMenuStates);

    QAction *actClose = m_menuFile->addAction(QIcon::fromTheme("application-exit"), tr("Exit"), this, &QWidget::close);

    actClose->setShortcut(QKeySequence("Ctrl+Q"));

    // =========================================================================
    // 2. EDIT MENU (Actions altering file/content with clipboard triad)
    // =========================================================================
    m_menuEdit = menuBar()->addMenu(tr("&Edit"));

    m_actUndo = m_menuEdit->addAction(QIcon::fromTheme("edit-undo"), tr("Undo"), this, [this]() {
        if (m_pdfViewer && m_pdfViewer->canUndo()) {
            m_pdfViewer->undo();
            updateContextualMenuStates();
        } else if (m_textViewer && m_textViewer->document()->isUndoAvailable()) {
            m_textViewer->undo();
            updateContextualMenuStates();
        } else if (m_imageViewer && m_imageViewer->canUndo()) {
            m_imageViewer->undo();
            updateContextualMenuStates();
        }
    });
    m_actUndo->setShortcut(QKeySequence::Undo);

    m_actRedo = m_menuEdit->addAction(QIcon::fromTheme("edit-redo"), tr("Redo"), this, [this]() {
        if (m_pdfViewer && m_pdfViewer->canRedo()) {
            m_pdfViewer->redo();
            updateContextualMenuStates();
        } else if (m_textViewer && m_textViewer->document()->isRedoAvailable()) {
            m_textViewer->redo();
            updateContextualMenuStates();
        } else if (m_imageViewer && m_imageViewer->canRedo()) {
            m_imageViewer->redo();
            updateContextualMenuStates();
        }
    });
    m_actRedo->setShortcut(QKeySequence::Redo);

    m_menuEdit->addSeparator();

    m_actCut = m_menuEdit->addAction(QIcon::fromTheme("edit-cut"), tr("Cut"), this, &BaseSkin::cutToClipboard);
    m_actCut->setShortcut(QKeySequence("Ctrl+X"));

    m_actCopy = m_menuEdit->addAction(QIcon::fromTheme("edit-copy"), tr("Copy"), this, &BaseSkin::copyToClipboard);
    m_actCopy->setShortcut(QKeySequence("Ctrl+C"));

    m_actPaste = m_menuEdit->addAction(QIcon::fromTheme("edit-paste"), tr("Paste"), this, &BaseSkin::pasteFromClipboard);
    m_actPaste->setShortcut(QKeySequence("Ctrl+V"));

    m_menuEdit->addSeparator();

    m_actRotateRight = m_menuEdit->addAction(QIcon::fromTheme("object-rotate-right"), tr("Rotate 90° Clockwise"), this, &BaseSkin::rotateCurrentRight);
    m_actRotateRight->setShortcut(QKeySequence("R"));

    m_actRotateLeft = m_menuEdit->addAction(QIcon::fromTheme("object-rotate-left"), tr("Rotate 90° Counterclockwise"), this, &BaseSkin::rotateCurrentLeft);
    m_actRotateLeft->setShortcut(QKeySequence("L"));

    m_actFlipH = m_menuEdit->addAction(QIcon::fromTheme("object-flip-horizontal"), tr("Flip Horizontally"), this, &BaseSkin::flipCurrentHorizontal);
    m_actFlipH->setShortcut(QKeySequence("H"));

    m_actFlipV = m_menuEdit->addAction(QIcon::fromTheme("object-flip-vertical"), tr("Flip Vertically"), this, &BaseSkin::flipCurrentVertical);
    m_actFlipV->setShortcut(QKeySequence("V"));

    m_actImageCrop = m_menuEdit->addAction(QIcon::fromTheme("transform-crop-and-resize"), tr("✂️ Crop Image (Canvas Selection)..."), this, [this]() {
        if (m_imageViewer) {
            m_imageViewer->setCropModeActive(!m_imageViewer->isCropModeActive());
            updateContextualMenuStates();
        }
    });
    m_actImageCrop->setShortcut(QKeySequence("C"));
    m_actImageCrop->setCheckable(true);

    m_menuEdit->addSeparator();

    m_actFolderRename = m_menuEdit->addAction(QIcon::fromTheme("edit-rename"), tr("Rename Item..."), this, &BaseSkin::renameCurrentFolderItem);
    m_actFolderRename->setShortcut(QKeySequence("F2"));

    m_actFolderTrash = m_menuEdit->addAction(QIcon::fromTheme("user-trash"), tr("Move to Trash"), this, &BaseSkin::deleteCurrentFile);
    m_actFolderTrash->setShortcut(QKeySequence::Delete);

    m_actFolderNew = m_menuEdit->addAction(QIcon::fromTheme("folder-new"), tr("New Folder..."), this, &BaseSkin::createFolderInCurrentFolder);
    m_actFolderNew->setShortcut(QKeySequence("F7"));

    // Archive modification actions (visible in Edit menu only when viewing an Archive)
    m_actArchiveAddFiles = m_menuEdit->addAction(QIcon::fromTheme("list-add"), tr("Add Files to Archive..."), this, &BaseSkin::addFilesToCurrentArchive);
    m_actArchiveAddFolder = m_menuEdit->addAction(QIcon::fromTheme("folder-new"), tr("Add Folder to Archive..."), this, &BaseSkin::addFolderToCurrentArchive);
    m_actArchiveRemoveSelected = m_menuEdit->addAction(QIcon::fromTheme("edit-delete"), tr("Remove Selected from Archive"), this, &BaseSkin::removeSelectedFromCurrentArchive);

    m_menuEdit->addSeparator();

    // Text File Edit Mode
    m_actToggleTextEdit = m_menuEdit->addAction(QIcon::fromTheme("document-edit"), tr("Edit Text Mode"), this, [this]() {
        if (m_textViewer) {
            m_textViewer->setEditable(!m_textViewer->isEditable());
        }
    });
    m_actToggleTextEdit->setCheckable(true);
    m_actToggleTextEdit->setShortcut(QKeySequence("F4"));

    m_actFindReplace = m_menuEdit->addAction(QIcon::fromTheme("edit-find-replace"), tr("Find & Replace..."), this, &BaseSkin::showFindReplace);
    m_actFindReplace->setShortcut(QKeySequence("Ctrl+H"));

    m_actGotoLine = m_menuEdit->addAction(QIcon::fromTheme("go-jump"), tr("Go to Line..."), this, &BaseSkin::gotoLineInDocument);
    m_actGotoLine->setShortcut(QKeySequence("Ctrl+G"));

    m_actDuplicateLine = m_menuEdit->addAction(QIcon::fromTheme("edit-copy"), tr("Duplicate Current Line"), this, [this]() {
        if (m_textViewer) m_textViewer->duplicateCurrentLine();
    });
    m_actDuplicateLine->setShortcut(QKeySequence("Ctrl+D"));

    m_actDeleteLine = m_menuEdit->addAction(QIcon::fromTheme("edit-delete"), tr("Delete Current Line"), this, [this]() {
        if (m_textViewer) m_textViewer->deleteCurrentLine();
    });
    m_actDeleteLine->setShortcut(QKeySequence("Ctrl+Shift+K"));

    m_actToggleComment = m_menuEdit->addAction(QIcon::fromTheme("code-context"), tr("Toggle Comment"), this, [this]() {
        if (m_textViewer) m_textViewer->toggleComment();
    });
    m_actToggleComment->setShortcut(QKeySequence("Ctrl+/"));

    m_menuEdit->addSeparator();

    // 1. Select Area (draw rectangular selection on canvas)
    m_actPdfSelectArea = m_menuEdit->addAction(QIcon::fromTheme("edit-select"), tr("Select Area..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->startSelectArea();
    });
    m_actPdfSelectArea->setShortcut(QKeySequence("Ctrl+Shift+A"));
    addAction(m_actPdfSelectArea);

    // 2. Insert Image directly into PDF
    m_actPdfInsertImage = m_menuEdit->addAction(QIcon::fromTheme("insert-image"), tr("Insert Image..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openInsertImageDialog();
    });

    // 3. Typewriter / Text insertion
    m_actPdfTypewriter = m_menuEdit->addAction(QIcon::fromTheme("insert-text"), tr("Typewriter / Text Insert..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openTypewriterDialog();
    });

    // 4. Submenu: Direct shortcuts to individual markup tools
    m_menuPdfMarkup = m_menuEdit->addMenu(QIcon::fromTheme("document-edit"), tr("Annotations & Markup Tools"));

    m_actPdfHighlighter = m_menuPdfMarkup->addAction(QIcon::fromTheme("draw-highlight"), tr("Highlighter..."), this, [this]() {
        if (m_flippableToolbar) {
            m_flippableToolbar->flipToHighlighter();
        }
    });
    m_actPdfHighlighter->setShortcut(QKeySequence("Ctrl+Shift+H"));

    m_actPdfShape = m_menuPdfMarkup->addAction(QIcon::fromTheme("draw-rectangle"), tr("Shapes & Arrows..."), this, [this]() {
        if (m_flippableToolbar) {
            m_flippableToolbar->flipToShapes();
        }
    });

    m_actPdfRedact = m_menuPdfMarkup->addAction(QIcon::fromTheme("edit-clear"), tr("Redact / Blackout..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->startRedaction();
    });

    m_actPdfWatermark = m_menuPdfMarkup->addAction(QIcon::fromTheme("format-text-strikethrough"), tr("Watermark / Stamp..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openWatermarkDialog();
    });

    m_actPdfSignature = m_menuPdfMarkup->addAction(QIcon::fromTheme("document-sign"), tr("Signature (Visual & Digital)..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openSignatureDialog();
    });

    m_menuPdfMarkup->addAction(QIcon::fromTheme("security-high"), tr("Cryptographic Digital Signature (PAdES / .p7m)..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->openCryptoSignatureDialog(0);
    });

    // 5. Submenu: PDF Document Structure Operations
    m_menuPdfDocOps = m_menuEdit->addMenu(QIcon::fromTheme("document-properties"), tr("PDF Page & Document Operations"));

    m_actPdfInsertBlankPage = m_menuPdfDocOps->addAction(QIcon::fromTheme("document-new"), tr("Insert Blank A4 Page..."), this, [this]() {
        if (m_pdfViewer) m_pdfViewer->insertBlankPage();
    });
    m_actPdfInsertBlankPage->setShortcut(QKeySequence("Ctrl+Shift+N"));

    m_actOrganizePdf = m_menuPdfDocOps->addAction(QIcon::fromTheme("view-pages-overview"), tr("Organize PDF Pages..."), this, [this]() {
        openPdfPageOrganizer();
    });

    m_actMergePdf = m_menuPdfDocOps->addAction(QIcon::fromTheme("document-import"), tr("Merge PDF Documents..."), this, &MainWindow::openPdfMergeDialog);

    m_actRepairPdf = m_menuPdfDocOps->addAction(QIcon::fromTheme("tools-check-spelling"), tr("Repair PDF Document..."), this, [this]() {
        openPdfRepairDialog();
    });

    connect(m_menuEdit, &QMenu::aboutToShow, this, &MainWindow::updateContextualMenuStates);

    // =========================================================================
    // 3. VIEW MENU
    // =========================================================================
    m_menuView = menuBar()->addMenu(tr("&View"));

    m_actFolderView = m_menuView->addAction(QIcon::fromTheme("folder-open"), tr("Folder Gallery View"), this, &BaseSkin::openCurrentFolderInGallery);
    m_actFolderView->setShortcut(QKeySequence("Alt+Up"));

    m_menuView->addSeparator();

    m_actPrev = m_menuView->addAction(QIcon::fromTheme("go-previous"), "◀", this, &BaseSkin::prevFile);
    m_actPrev->setToolTip(tr("Previous File (Left Arrow)"));
    m_actPrev->setShortcut(QKeySequence(Qt::Key_Left));

    m_actNext = m_menuView->addAction(QIcon::fromTheme("go-next"), "▶", this, &BaseSkin::nextFile);
    m_actNext->setToolTip(tr("Next File (Right Arrow)"));
    m_actNext->setShortcut(QKeySequence(Qt::Key_Right));

    m_menuView->addSeparator();

    m_actFullscreen = m_menuView->addAction(QIcon::fromTheme("view-fullscreen"), tr("Fullscreen"), this, &BaseSkin::toggleFullscreen);
    m_actFullscreen->setShortcut(QKeySequence("F11"));

    m_actSlideshow = m_menuView->addAction(QIcon::fromTheme("media-playback-start"), tr("Photo Slideshow"), this, &BaseSkin::toggleSlideshow);
    m_actSlideshow->setShortcut(QKeySequence("F5"));

    m_actInspector = m_menuView->addAction(QIcon::fromTheme("document-properties"), tr("Inspector / Properties"), this, &BaseSkin::showFileInfo);
    m_actInspector->setShortcut(QKeySequence("Ctrl+I"));

    // PDF page navigation actions (for toolbar / shortcuts)
    m_actPdfPrevPage = new QAction(QIcon::fromTheme("go-up"), tr("▲ Prev Page"), this);
    m_actPdfPrevPage->setShortcut(QKeySequence(Qt::Key_PageUp));
    connect(m_actPdfPrevPage, &QAction::triggered, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->prevPage();
        else if (m_officeViewer && m_officeViewer->isPdfMode()) m_officeViewer->pdfViewer()->prevPage();
    });
    addAction(m_actPdfPrevPage);

    m_actPdfNextPage = new QAction(QIcon::fromTheme("go-down"), tr("▼ Next Page"), this);
    m_actPdfNextPage->setShortcut(QKeySequence(Qt::Key_PageDown));
    connect(m_actPdfNextPage, &QAction::triggered, this, [this]() {
        if (m_pdfViewer) m_pdfViewer->nextPage();
        else if (m_officeViewer && m_officeViewer->isPdfMode()) m_officeViewer->pdfViewer()->nextPage();
    });
    addAction(m_actPdfNextPage);

    m_menuView->addSeparator();

    m_actZoomTool = m_menuView->addAction(QIcon::fromTheme("zoom-fit-best"), tr("Zoom Tool (Mouse Wheel)"), this, &BaseSkin::toggleZoomTool);
    m_actZoomTool->setCheckable(true);
    m_actZoomTool->setChecked(false);
    m_actZoomTool->setToolTip(tr("Toggle mouse wheel functionality between scrolling and zooming"));

    m_actZoomIn = m_menuView->addAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"), this, &BaseSkin::zoomInCurrent);
    m_actZoomIn->setShortcut(QKeySequence("Ctrl++"));

    m_actZoomOut = m_menuView->addAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"), this, &BaseSkin::zoomOutCurrent);
    m_actZoomOut->setShortcut(QKeySequence("Ctrl+-"));

    m_actZoomReset = m_menuView->addAction(QIcon::fromTheme("zoom-original"), tr("Zoom 100%"), this, &BaseSkin::resetZoomCurrent);
    m_actZoomReset->setShortcut(QKeySequence("Ctrl+0"));

    m_actFit = m_menuView->addAction(QIcon::fromTheme("zoom-fit-best"), tr("Fit to Window"), this, &BaseSkin::fitCurrent);
    m_actFit->setShortcut(QKeySequence("Ctrl+9"));

    m_menuView->addSeparator();

    m_actFilmstrip = m_menuView->addAction(QIcon::fromTheme("view-preview"), tr("Thumbnail Strip"), this, &BaseSkin::toggleFilmstrip);
    m_actFilmstrip->setCheckable(true);
    m_actFilmstrip->setChecked(m_filmstripWanted);
    m_actFilmstrip->setShortcut(QKeySequence("F4"));

    m_actPdfToc = m_menuView->addAction(QIcon::fromTheme("view-list-tree"), tr("Document Outline (TOC)"), this, &BaseSkin::togglePdfToc);
    m_actPdfToc->setCheckable(true);
    m_actPdfToc->setShortcut(QKeySequence("F3"));

    m_actLineNumbers = m_menuView->addAction(QIcon::fromTheme("format-list-ordered"), tr("Line Numbers"), this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setLineNumbersVisible(checked);
    });
    m_actLineNumbers->setCheckable(true);
    m_actLineNumbers->setShortcut(QKeySequence("F8"));

    m_actSyntaxHighlight = m_menuView->addAction(QIcon::fromTheme("code-class"), tr("Syntax Highlighting"), this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setSyntaxHighlightingEnabled(checked);
    });
    m_actSyntaxHighlight->setCheckable(true);
    m_actSyntaxHighlight->setShortcut(QKeySequence("F9"));

    m_actWordWrap = m_menuView->addAction(QIcon::fromTheme("format-justify-fill"), tr("Word Wrap"), this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setWordWrapEnabled(checked);
    });
    m_actWordWrap->setCheckable(true);
    m_actWordWrap->setShortcut(QKeySequence("Alt+Z"));

    m_actShowWhitespace = m_menuView->addAction(QIcon::fromTheme("draw-text"), tr("Show Whitespace Characters"), this, [this](bool checked) {
        if (m_textViewer) m_textViewer->setShowWhitespace(checked);
    });
    m_actShowWhitespace->setCheckable(true);

    connect(m_menuView, &QMenu::aboutToShow, this, [this]() {
        updateContextualMenuStates();
        if (m_actFilmstrip) m_actFilmstrip->setChecked(isFilmstripVisible());
        if (m_pdfViewer) {
            m_actPdfToc->setChecked(m_pdfViewer->isTocVisible());
            m_actPdfToc->setEnabled(m_pdfViewer->hasOutline());
        }
        if (m_textViewer) {
            if (m_actLineNumbers) m_actLineNumbers->setChecked(m_textViewer->isLineNumbersVisible());
            if (m_actSyntaxHighlight) m_actSyntaxHighlight->setChecked(m_textViewer->isSyntaxHighlightingEnabled());
            if (m_actWordWrap) m_actWordWrap->setChecked(m_textViewer->isWordWrapEnabled());
            if (m_actShowWhitespace) m_actShowWhitespace->setChecked(m_textViewer->isShowWhitespace());
        }
    });

    m_menuView->addSeparator();

    m_actToggleToolbar = m_menuView->addAction(tr("Show Toolbar"), this, [this](bool checked) {
        if (m_flippableToolbar) m_flippableToolbar->setVisible(checked);
    });
    m_actToggleToolbar->setCheckable(true);
    m_actToggleToolbar->setChecked(true);
    m_actToggleToolbar->setShortcut(QKeySequence("Ctrl+T"));

    QAction *actToggleStatus = m_menuView->addAction(tr("Show Status Bar"), this, [this](bool checked) {
        statusBar()->setVisible(checked);
    });
    actToggleStatus->setCheckable(true);
    actToggleStatus->setChecked(true);

    QSettings settings;
    bool showMenuBar = settings.value("ui/show_menubar", true).toBool();

    QAction *actToggleMenuBar = m_menuView->addAction(tr("Show Menu Bar"), this, [this](bool checked) {
        menuBar()->setVisible(checked);
        QSettings s;
        s.setValue("ui/show_menubar", checked);
    });
    actToggleMenuBar->setCheckable(true);
    actToggleMenuBar->setChecked(showMenuBar);
    actToggleMenuBar->setShortcut(QKeySequence("Ctrl+M"));
    addAction(actToggleMenuBar);
    menuBar()->setVisible(showMenuBar);

    m_menuView->addSeparator();

    m_actShowUninstalledFeatures = m_menuView->addAction(tr("Show Uninstalled Features"), this, [this](bool checked) {
        BuildConfig::setShowUninstalledFeatures(checked);
        updateDiscoveryActions();
    });
    m_actShowUninstalledFeatures->setCheckable(true);
    m_actShowUninstalledFeatures->setChecked(BuildConfig::showUninstalledFeatures());

    m_actShowUnsupportedFiles = m_menuView->addAction(tr("Show Unsupported Files"), this, [this](bool checked) {
        BuildConfig::setShowUnsupportedFiles(checked);
        if (!m_currentFilePath.isEmpty()) {
            scanDirectory(m_currentFilePath);
            m_filmstrip->setFiles(m_siblingFiles, m_currentIndex);
        }
    });
    m_actShowUnsupportedFiles->setCheckable(true);
    m_actShowUnsupportedFiles->setChecked(BuildConfig::showUnsupportedFiles());


    // =========================================================================
    // 4. TOOLS MENU
    // =========================================================================
    m_menuTools = menuBar()->addMenu(tr("&Tools"));

    m_actSearch = m_menuTools->addAction(QIcon::fromTheme("edit-find"), tr("Find in Document..."), this, &BaseSkin::searchInDocument);
    m_actSearch->setShortcut(QKeySequence("Ctrl+F"));

    m_actOcr = m_menuTools->addAction(QIcon::fromTheme("scanner"), tr("Extract Text (OCR Tesseract)..."), this, &BaseSkin::extractOcrCurrentFile);
    m_actExtractImages = m_menuTools->addAction(QIcon::fromTheme("image-x-generic"), tr("Extract Images from PDF..."), this, [this]() {
        extractPdfImages(currentActiveTargetFilePath());
    });

    m_actAudioTrim = m_menuTools->addAction(QIcon::fromTheme("edit-cut"), tr("Trim / Cut Audio Clip..."), this, [this]() {
        if (m_mediaViewer) m_mediaViewer->openAudioTrimDialog();
    });
    m_actAudioTrim->setShortcut(QKeySequence("Ctrl+Shift+T"));

    m_actAudioTags = m_menuTools->addAction(QIcon::fromTheme("tag"), tr("Edit Audio Tags & Cover..."), this, [this]() {
        if (m_mediaViewer) m_mediaViewer->openAudioTagDialog();
    });

    m_actAudioExtract = m_menuTools->addAction(QIcon::fromTheme("audio-x-generic"), tr("Extract Audio Track from Video..."), this, [this]() {
        if (m_mediaViewer) m_mediaViewer->extractAudioTrack();
    });

    m_menuTools->addSeparator();

    // Discovery placeholder items for uninstalled features
    m_actOfficeDiscovery = m_menuTools->addAction(QIcon::fromTheme("document-open"), tr("⚙️ Internal Office Viewer [Inactive]..."), this, [this]() {
        if (PackageManagerHelper::installComponentInteractive("office", this)) {
            updateDiscoveryActions();
        }
    });

    m_actQpdfDiscovery = m_menuTools->addAction(QIcon::fromTheme("document-save"), tr("⚙️ Advanced PDF Tools (QPDF) [Not installed]..."), this, [this]() {
        if (PackageManagerHelper::installComponentInteractive("qpdf", this)) {
            updateDiscoveryActions();
        }
    });

    m_actPdfimagesDiscovery = m_menuTools->addAction(QIcon::fromTheme("image-x-generic"), tr("⚙️ Extract Images from PDF [pdfimages missing]..."), this, [this]() {
        if (PackageManagerHelper::installComponentInteractive("pdfimages", this)) {
            updateDiscoveryActions();
        }
    });

    updateDiscoveryActions();

    m_menuTools->addSeparator();

    m_menuTools->addAction(QIcon::fromTheme("dialog-information"), tr("File Metadata & Properties..."), this, &BaseSkin::showFileInfo);
    m_actToolsExportAs = m_menuTools->addAction(QIcon::fromTheme("document-export"), tr("Export As..."), this, &BaseSkin::exportCurrentFileAs);
    m_menuTools->addAction(QIcon::fromTheme("system-run"), tr("Open with Other Application..."), this, &BaseSkin::openWithOther);

    connect(m_menuTools, &QMenu::aboutToShow, this, &MainWindow::updateContextualMenuStates);

    // =========================================================================
    // 5. SETTINGS MENU
    // =========================================================================
    m_menuSettings = menuBar()->addMenu(tr("&Settings"));
    m_actConfigureToolbars = m_menuSettings->addAction(QIcon::fromTheme("configure-toolbars"), tr("Configure Toolbars..."), this, &MainWindow::configureToolbars);
    m_menuSettings->addSeparator();
    m_menuSettings->addAction(tr("🎨 Appearance..."), this, &BaseSkin::openAppearanceSettings);
    m_menuSettings->addAction(tr("🐧 Desktop Integration..."), this, &BaseSkin::openSettings);
    m_menuSettings->addSeparator();
    m_menuSettings->addAction(tr("⚙️ Configure BubezView..."), this, &BaseSkin::openAppearanceSettings);

    // =========================================================================
    // 6. HELP MENU
    // =========================================================================
    m_menuHelp = menuBar()->addMenu(tr("&Help"));
    m_actCheckUpdates = m_menuHelp->addAction(QIcon::fromTheme("system-software-update"), tr("Check for Updates..."), this, &BaseSkin::checkForUpdates);
    m_menuHelp->addSeparator();

    m_actShortcuts = m_menuHelp->addAction(QIcon::fromTheme("help-browser"), tr("Keyboard Shortcuts Guide"), this, &BaseSkin::showShortcutsDialog);
    m_actShortcuts->setShortcut(QKeySequence("F1"));

    m_menuHelp->addAction(QIcon::fromTheme("tools-report-bug"), tr("Report an Issue / Bug Tracker..."), this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/davide-bubez/BubezView/issues"));
    });

    m_menuHelp->addSeparator();
    m_actAbout = m_menuHelp->addAction(QIcon::fromTheme("help-about"), tr("About BubezView"), this, &BaseSkin::showAboutDialog);

    // =========================================================================
    // Flippable Toolbar (KDE Customizable style with 3D Annotation Flip)
    // =========================================================================
    m_flippableToolbar = new FlippableToolbar(this);
    m_toolbar = m_flippableToolbar->mainToolBar();
    m_toolbar->setIconSize(QSize(20, 20));

    // Embed flippable toolbar into top layout of MainWindow
    auto *topToolBar = addToolBar(tr("Main Toolbar"));
    topToolBar->setObjectName("MainToolbarDock");
    topToolBar->setMovable(false);
    topToolBar->addWidget(m_flippableToolbar);

    int savedStyle = settings.value("toolbar/button_style", static_cast<int>(Qt::ToolButtonIconOnly)).toInt();
    m_toolbar->setToolButtonStyle(static_cast<Qt::ToolButtonStyle>(savedStyle));

    m_toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_toolbar, &QToolBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction(QIcon::fromTheme("configure-toolbars"), tr("Configure Toolbars..."), this, &MainWindow::configureToolbars);
        menu.addSeparator();

        QMenu *styleMenu = menu.addMenu(tr("Button Style"));
        auto addStyleAction = [this, styleMenu](const QString &title, Qt::ToolButtonStyle style) {
            QAction *act = styleMenu->addAction(title);
            act->setCheckable(true);
            act->setChecked(m_toolbar->toolButtonStyle() == style);
            connect(act, &QAction::triggered, this, [this, style]() {
                m_toolbar->setToolButtonStyle(style);
                QSettings s;
                s.setValue("toolbar/button_style", static_cast<int>(style));
            });
        };

        addStyleAction(tr("Icons Only"), Qt::ToolButtonIconOnly);
        addStyleAction(tr("Text Only"), Qt::ToolButtonTextOnly);
        addStyleAction(tr("Text Beside Icons"), Qt::ToolButtonTextBesideIcon);
        addStyleAction(tr("Text Under Icons"), Qt::ToolButtonTextUnderIcon);

        menu.addSeparator();
        QAction *toggleAct = menu.addAction(tr("Show Toolbar"), this, [this](bool visible) {
            m_flippableToolbar->setVisible(visible);
            if (m_actToggleToolbar) m_actToggleToolbar->setChecked(visible);
        });
        toggleAct->setCheckable(true);
        toggleAct->setChecked(m_flippableToolbar->isVisible());

        menu.exec(m_toolbar->mapToGlobal(pos));
    });

    setupToolbarActionsMap();
    rebuildToolbar();

    // Status bar with Breadcrumbs
    QStatusBar *bar = statusBar();

    m_breadcrumbWidget = new QWidget(this);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbWidget);
    m_breadcrumbLayout->setContentsMargins(4, 0, 8, 0);
    m_breadcrumbLayout->setSpacing(4);

    m_fileIndexLabel = new QLabel(this);
    m_fileIndexLabel->setStyleSheet("color: #8a9098; padding-left: 6px; padding-right: 8px; font-size: 11px;");
    m_fileInfoLabel = new QLabel(this);
    m_fileInfoLabel->setStyleSheet("color: #a0a6af; font-size: 11px;");
    m_statusLabel = new QLabel(tr("⚡ Native C++ Engine"), this);
    m_statusLabel->setStyleSheet("color: #10b981; font-weight: bold; padding-right: 8px; font-size: 11px;");

    m_btnUpdateNotification = new QPushButton(this);
    m_btnUpdateNotification->setCursor(Qt::PointingHandCursor);
    m_btnUpdateNotification->setFlat(true);
    m_btnUpdateNotification->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "   padding: 2px 8px;"
        "   border-radius: 4px;"
        "   margin-right: 6px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #0369a1;"
        "}"
    );
    m_btnUpdateNotification->hide();

    bar->addWidget(m_breadcrumbWidget);
    bar->addWidget(m_fileIndexLabel);
    bar->addWidget(m_fileInfoLabel, 1);
    bar->addPermanentWidget(m_btnUpdateNotification);
    bar->addPermanentWidget(m_statusLabel);
}

QStringList MainWindow::defaultToolbarActions() const
{
    return QStringList() << "prev_file"
                         << "next_file"
                         << "folder_view"
                         << "separator"
                         << "zoom_tool"
                         << "zoom_reset"
                         << "separator"
                         << "fullscreen";
}

void MainWindow::setupToolbarActionsMap()
{
    m_idToActionMap.clear();
    m_toolbarActionInfoMap.clear();

    auto registerAction = [this](const QString &id, QAction *act, const QString &customLabel = QString()) {
        if (!act) return;
        m_idToActionMap[id] = act;
        ToolbarActionInfo info;
        info.id = id;
        info.text = customLabel.isEmpty() ? act->text().remove('&') : customLabel;
        info.icon = act->icon();
        QString shortcutStr = act->shortcut().toString(QKeySequence::NativeText);
        info.tooltip = shortcutStr.isEmpty() ? info.text : QString("%1 (%2)").arg(info.text, shortcutStr);
        m_toolbarActionInfoMap[id] = info;
    };

    // Universal navigation
    registerAction("prev_file", m_actPrev, tr("Previous (◀)"));
    registerAction("next_file", m_actNext, tr("Next (▶)"));
    registerAction("folder_view", m_actFolderView, tr("Folder Gallery View"));

    // PDF page navigation
    registerAction("pdf_prev_page", m_actPdfPrevPage, tr("Previous Page (PDF)"));
    registerAction("pdf_next_page", m_actPdfNextPage, tr("Next Page (PDF)"));

    // Zoom & view
    registerAction("zoom_tool", m_actZoomTool, tr("Zoom Tool (Mouse Wheel)"));
    registerAction("zoom_in", m_actZoomIn, tr("Zoom In"));
    registerAction("zoom_out", m_actZoomOut, tr("Zoom Out"));
    registerAction("zoom_reset", m_actZoomReset, tr("Zoom 100%"));
    registerAction("zoom_fit", m_actFit, tr("Fit to Window"));
    registerAction("fullscreen", m_actFullscreen, tr("Fullscreen"));
    registerAction("slideshow", m_actSlideshow, tr("Photo Slideshow"));
    registerAction("filmstrip", m_actFilmstrip, tr("Thumbnail Strip"));
    registerAction("pdf_toc", m_actPdfToc, tr("Document Outline (TOC)"));
    registerAction("file_info", m_actInspector, tr("File Properties / Info"));

    // File
    registerAction("open_file", m_actOpenFile, tr("Open File..."));
    registerAction("open_folder", m_actOpenFolder, tr("Open Folder..."));
    registerAction("open_with_default", m_actOpenWith, tr("Open with Default Application"));
    registerAction("save_file", m_actSave, tr("Save Changes"));
    registerAction("save_as", m_actSaveAs, tr("Save Copy As..."));
    registerAction("export_as", m_actExportAs, tr("Export As..."));
    registerAction("print_file", m_actPrint, tr("Print..."));
    registerAction("copy_path", m_actCopyPath, tr("Copy File Path"));
    registerAction("delete_file", m_actTrashFile, tr("Move to Trash"));

    // Edit & Transform
    registerAction("undo", m_actUndo, tr("Undo"));
    registerAction("redo", m_actRedo, tr("Redo"));
    registerAction("cut", m_actCut, tr("Cut"));
    registerAction("copy", m_actCopy, tr("Copy"));
    registerAction("paste", m_actPaste, tr("Paste"));
    registerAction("rotate_right", m_actRotateRight, tr("Rotate 90° Clockwise"));
    registerAction("rotate_left", m_actRotateLeft, tr("Rotate 90° Counterclockwise"));
    registerAction("flip_h", m_actFlipH, tr("Flip Horizontally"));
    registerAction("flip_v", m_actFlipV, tr("Flip Vertically"));

    // Native Image Crop
    registerAction("image_crop", m_actImageCrop, tr("✂️ Crop Image"));

    // Folder actions
    registerAction("folder_rename", m_actFolderRename, tr("Rename Folder Item"));
    registerAction("folder_trash", m_actFolderTrash, tr("Move Item to Trash"));
    registerAction("folder_new", m_actFolderNew, tr("New Folder"));

    // Text Editor actions
    registerAction("edit_text", m_actToggleTextEdit, tr("Edit Text Mode"));
    registerAction("line_numbers", m_actLineNumbers, tr("Line Numbers"));
    registerAction("syntax_highlight", m_actSyntaxHighlight, tr("Syntax Highlighting"));
    registerAction("word_wrap", m_actWordWrap, tr("Word Wrap"));
    registerAction("show_whitespace", m_actShowWhitespace, tr("Show Whitespace Characters"));
    registerAction("goto_line", m_actGotoLine, tr("Go to Line"));
    registerAction("find_replace", m_actFindReplace, tr("Find & Replace"));
    registerAction("duplicate_line", m_actDuplicateLine, tr("Duplicate Current Line"));
    registerAction("delete_line", m_actDeleteLine, tr("Delete Current Line"));
    registerAction("toggle_comment", m_actToggleComment, tr("Toggle Comment"));

    // PDF Annotation & Tools
    registerAction("search_doc", m_actSearch, tr("Find in Document"));
    registerAction("ocr", m_actOcr, tr("Extract Text (OCR)"));
    registerAction("extract_images", m_actExtractImages, tr("Extract Images from PDF"));
    registerAction("pdf_cut_text", m_actCut, tr("Cut (Text / Area)"));
    registerAction("pdf_highlighter", m_actPdfHighlighter, tr("Highlighter"));
    registerAction("pdf_select_area", m_actPdfSelectArea, tr("Select Area"));
    registerAction("pdf_typewriter", m_actPdfTypewriter, tr("Typewriter / Text Insert"));
    registerAction("pdf_signature", m_actPdfSignature, tr("Signature"));
    registerAction("pdf_watermark", m_actPdfWatermark, tr("Watermark / Stamp"));
    registerAction("pdf_shape", m_actPdfShape, tr("Shapes & Arrows"));
    registerAction("pdf_insert_image", m_actPdfInsertImage, tr("Insert Image"));
    registerAction("pdf_snapshot", m_actPdfSnapshot, tr("Snapshot (300 DPI)"));
    registerAction("pdf_redact", m_actPdfRedact, tr("Redact / Blackout"));
    registerAction("pdf_merge", m_actMergePdf, tr("Merge PDF Documents"));
    registerAction("pdf_organize", m_actOrganizePdf, tr("Organize PDF Pages"));
    registerAction("pdf_repair", m_actRepairPdf, tr("Repair PDF Document"));

    // Media
    registerAction("audio_trim", m_actAudioTrim, tr("Trim Audio Clip"));
    registerAction("audio_tags", m_actAudioTags, tr("Edit Audio Tags"));
    registerAction("audio_extract", m_actAudioExtract, tr("Extract Audio Track"));
}

void MainWindow::rebuildToolbar()
{
    m_toolbar->clear();

    QSettings settings;
    if (m_configuredToolbarActionIds.isEmpty()) {
        m_configuredToolbarActionIds = settings.value("toolbar/actions", defaultToolbarActions()).toStringList();
    }

    for (const QString &id : m_configuredToolbarActionIds) {
        if (id == "separator") {
            m_toolbar->addSeparator();
        } else if (m_idToActionMap.contains(id)) {
            m_toolbar->addAction(m_idToActionMap[id]);
        }
    }
}

void MainWindow::configureToolbars()
{
    QSettings settings;
    QStringList currentIds = settings.value("toolbar/actions", defaultToolbarActions()).toStringList();
    ConfigureToolbarDialog dlg(m_toolbarActionInfoMap, currentIds, defaultToolbarActions(), this);

    connect(&dlg, &ConfigureToolbarDialog::applied, this, [this](const QStringList &actionIds) {
        m_configuredToolbarActionIds = actionIds;
        QSettings s;
        s.setValue("toolbar/actions", actionIds);
        rebuildToolbar();
        updateContextualMenuStates();
    });

    dlg.exec();
}

void MainWindow::onFileLoaded(const QString &filePath, FileCategory category, const QList<UIActionItem> &actions)
{
    Q_UNUSED(filePath);
    Q_UNUSED(category);
    Q_UNUSED(actions);

    updateStatusBar();
    updateContextualMenuStates();
}

void MainWindow::updateStatusBar()
{
    updateBreadcrumbs();
    m_fileIndexLabel->setText(fileIndexText());
    m_fileInfoLabel->setText(fileInfoText());
}

void MainWindow::updateBreadcrumbs()
{
    if (!m_breadcrumbLayout) return;

    // Clear old breadcrumbs
    QLayoutItem *child;
    while ((child = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QDir d = fi.isDir() ? QDir(m_currentFilePath) : fi.dir();
    QString canonical = d.canonicalPath();

    QString home = QDir::homePath();
    QString displayPath = canonical;
    bool isUnderHome = false;
    if (canonical.startsWith(home)) {
        displayPath = "~" + canonical.mid(home.length());
        isUnderHome = true;
    }

    QStringList parts = displayPath.split("/", Qt::SkipEmptyParts);
    QString accumulated = isUnderHome ? home : "";

    QLabel *lblIcon = new QLabel("📁", m_breadcrumbWidget);
    lblIcon->setStyleSheet("font-size: 12px; margin-right: 2px;");
    m_breadcrumbLayout->addWidget(lblIcon);

    for (int i = 0; i < parts.size(); ++i) {
        const QString &part = parts[i];
        if (isUnderHome && i == 0 && part == "~") {
            // accumulated already set to home
        } else {
            accumulated += "/" + part;
        }

        QPushButton *btn = new QPushButton(part, m_breadcrumbWidget);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        btn->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   color: #38bdf8;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "   padding: 1px 4px;"
            "   border-radius: 3px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #334155;"
            "   color: #ffffff;"
            "}"
        );

        QString targetFolder = accumulated;
        connect(btn, &QPushButton::clicked, this, [this, targetFolder]() {
            openFile(targetFolder);
        });

        m_breadcrumbLayout->addWidget(btn);

        if (i < parts.size() - 1) {
            QLabel *sep = new QLabel("›", m_breadcrumbWidget);
            sep->setStyleSheet("color: #64748b; font-size: 11px; margin: 0 1px;");
            m_breadcrumbLayout->addWidget(sep);
        }
    }
}

void MainWindow::applyCurrentTheme()
{
    BaseSkin::applyCurrentTheme();
    updateStatusBar();
}

void MainWindow::populateOpenWithMenu()
{
    m_menuOpenWith->clear();
    if (m_currentFilePath.isEmpty() || !QFile::exists(m_currentFilePath)) {
        QAction *noneAct = m_menuOpenWith->addAction(tr("No file currently open"));
        noneAct->setEnabled(false);
        return;
    }

    QList<AppInfo> apps = AppLauncher::getAssociatedApps(m_currentFilePath);
    int count = 0;
    for (const AppInfo &app : apps) {
        if (count >= 6) break;
        QAction *act = m_menuOpenWith->addAction(app.icon(), app.name);
        connect(act, &QAction::triggered, this, [this, app]() {
            AppLauncher::launchApp(app, m_currentFilePath);
        });
        count++;
    }

    if (count > 0) {
        m_menuOpenWith->addSeparator();
    }

    m_menuOpenWith->addAction(QIcon::fromTheme("system-run"), tr("Other Application..."), this, &BaseSkin::openWithOther);
}

void MainWindow::updateContextualMenuStates()
{
    QString targetPath = currentActiveTargetFilePath();
    FileCategory activeCategory = m_currentCategory;
    if (m_currentCategory == FileCategory::Folder && !targetPath.isEmpty()) {
        activeCategory = PluginManager::categorizeFile(targetPath);
    }

    bool isImage = (activeCategory == FileCategory::Image);
    bool isText = (activeCategory == FileCategory::Text);
    bool isPdf = (activeCategory == FileCategory::Pdf);
    bool isArchive = (activeCategory == FileCategory::Archive);
    bool isMedia = (activeCategory == FileCategory::Media);
    bool isOffice = (activeCategory == FileCategory::Office);
    bool isFolder = (m_currentCategory == FileCategory::Folder);
    bool hasFile = !targetPath.isEmpty() && QFile::exists(targetPath);

    // 1. FILE MENU
    if (m_actSave) {
        bool canSave = (isImage && !isFolder && m_imageViewer && m_imageViewer->isModified())
                    || (isPdf && !isFolder && m_pdfViewer && m_pdfViewer->isModified())
                    || (isText && !isFolder && m_textViewer && m_textViewer->document()->isModified());
        m_actSave->setVisible((isImage || isPdf || isText) && !isFolder);
        m_actSave->setEnabled(canSave);
    }

    if (m_actSaveAs) {
        m_actSaveAs->setVisible((isImage || isText || isPdf || isOffice) && !isFolder);
        m_actSaveAs->setEnabled(hasFile && !isFolder);
    }

    if (m_actCreateArchive) {
        m_actCreateArchive->setVisible(true);
        m_actCreateArchive->setEnabled(true);
    }

    if (m_actExportAs) {
        m_actExportAs->setVisible((isImage || isPdf || isText || isOffice) && !isFolder);
        m_actExportAs->setEnabled(hasFile && !isFolder);
    }

    if (m_actTrashFile) {
        m_actTrashFile->setVisible(hasFile || isFolder);
        m_actTrashFile->setEnabled(hasFile || isFolder);
    }

    if (m_actPrint) {
        m_actPrint->setVisible((isImage || isText || isPdf || isOffice) && !isFolder);
        m_actPrint->setEnabled(hasFile && !isFolder);
    }

    if (m_menuShare) {
        m_menuShare->menuAction()->setVisible(hasFile || isFolder);
    }

    // 2. EDIT MENU
    bool isArchiveDoc = (isArchive && !isFolder);
    if (m_menuEdit) {
        m_menuEdit->menuAction()->setVisible(isFolder || isArchiveDoc || !isMedia);
    }

    if (m_actUndo) {
        bool canUndo = (isPdf && !isFolder && m_pdfViewer && m_pdfViewer->canUndo())
                    || (isText && !isFolder && m_textViewer && m_textViewer->document()->isUndoAvailable())
                    || (isImage && !isFolder && m_imageViewer && m_imageViewer->canUndo());
        m_actUndo->setVisible((isPdf || isText || isImage) && !isFolder);
        m_actUndo->setEnabled(canUndo);
    }

    if (m_actRedo) {
        bool canRedo = (isPdf && !isFolder && m_pdfViewer && m_pdfViewer->canRedo())
                    || (isText && !isFolder && m_textViewer && m_textViewer->document()->isRedoAvailable())
                    || (isImage && !isFolder && m_imageViewer && m_imageViewer->canRedo());
        m_actRedo->setVisible((isPdf || isText || isImage) && !isFolder);
        m_actRedo->setEnabled(canRedo);
    }

    if (m_actCut) {
        bool canCut = (isText && !isFolder && m_textViewer && m_textViewer->textCursor().hasSelection()) ||
                      (isPdf && !isFolder && m_pdfViewer && (m_pdfViewer->hasTextSelection() || m_pdfViewer->hasAreaSelection()));
        m_actCut->setVisible((isText || isPdf) && !isFolder);
        m_actCut->setEnabled(canCut);
    }

    if (m_actCopy) {
        m_actCopy->setVisible(isImage || isText || isPdf || isFolder);
        bool canCopy = (isImage || isText || isFolder || (isPdf && (!isFolder ? (m_pdfViewer && (m_pdfViewer->hasTextSelection() || m_pdfViewer->pageCount() > 0)) : hasFile)));
        m_actCopy->setEnabled(canCopy);
    }

    if (m_actPaste) {
        const QClipboard *clip = QApplication::clipboard();
        bool hasClipContent = !clip->text().trimmed().isEmpty() || (!clip->image().isNull() && clip->image().width() > 0);
        bool canPaste = (isText && !isFolder) || (isPdf && !isFolder && hasClipContent);
        m_actPaste->setVisible((isText || isPdf) && !isFolder);
        m_actPaste->setEnabled(canPaste);
    }

    if (m_actRotateRight) {
        m_actRotateRight->setVisible((isImage || isPdf) && !isFolder);
        m_actRotateRight->setEnabled((isImage || isPdf) && !isFolder);
    }
    if (m_actRotateLeft) {
        m_actRotateLeft->setVisible((isImage || isPdf) && !isFolder);
        m_actRotateLeft->setEnabled((isImage || isPdf) && !isFolder);
    }
    if (m_actFlipH) {
        m_actFlipH->setVisible(isImage && !isFolder);
        m_actFlipH->setEnabled(isImage && !isFolder);
    }
    if (m_actFlipV) {
        m_actFlipV->setVisible(isImage && !isFolder);
        m_actFlipV->setEnabled(isImage && !isFolder);
    }

    bool isImageDoc = (isImage && !isFolder);
    if (m_actImageCrop) {
        m_actImageCrop->setVisible(isImageDoc);
        m_actImageCrop->setEnabled(isImageDoc && hasFile);
        m_actImageCrop->setChecked(m_imageViewer && m_imageViewer->isCropModeActive());
    }

    if (m_actFolderRename) {
        m_actFolderRename->setVisible(isFolder);
        m_actFolderRename->setEnabled(isFolder && hasFile);
    }
    if (m_actFolderTrash) {
        m_actFolderTrash->setVisible(isFolder);
        m_actFolderTrash->setEnabled(isFolder && hasFile);
    }
    if (m_actFolderNew) {
        m_actFolderNew->setVisible(isFolder);
        m_actFolderNew->setEnabled(isFolder);
    }

    if (m_actArchiveAddFiles) {
        m_actArchiveAddFiles->setVisible(isArchiveDoc);
        m_actArchiveAddFiles->setEnabled(isArchiveDoc);
    }
    if (m_actArchiveAddFolder) {
        m_actArchiveAddFolder->setVisible(isArchiveDoc);
        m_actArchiveAddFolder->setEnabled(isArchiveDoc);
    }
    if (m_actArchiveRemoveSelected) {
        m_actArchiveRemoveSelected->setVisible(isArchiveDoc);
        m_actArchiveRemoveSelected->setEnabled(isArchiveDoc);
    }

    // Text File Editing in Edit Menu
    bool isTextDoc = (isText && !isFolder);
    if (m_actToggleTextEdit) {
        m_actToggleTextEdit->setVisible(isTextDoc);
        m_actToggleTextEdit->setEnabled(isTextDoc);
        if (m_textViewer) {
            m_actToggleTextEdit->setChecked(m_textViewer->isEditable());
        }
    }
    if (m_actFindReplace) {
        m_actFindReplace->setVisible(isTextDoc);
        m_actFindReplace->setEnabled(isTextDoc);
    }
    if (m_actGotoLine) {
        m_actGotoLine->setVisible(isTextDoc);
        m_actGotoLine->setEnabled(isTextDoc);
    }
    if (m_actDuplicateLine) {
        m_actDuplicateLine->setVisible(isTextDoc);
        m_actDuplicateLine->setEnabled(isTextDoc && m_textViewer && m_textViewer->isEditable());
    }
    if (m_actDeleteLine) {
        m_actDeleteLine->setVisible(isTextDoc);
        m_actDeleteLine->setEnabled(isTextDoc && m_textViewer && m_textViewer->isEditable());
    }
    if (m_actToggleComment) {
        m_actToggleComment->setVisible(isTextDoc);
        m_actToggleComment->setEnabled(isTextDoc && m_textViewer && m_textViewer->isEditable());
    }
    if (m_actLineNumbers) {
        m_actLineNumbers->setVisible(isTextDoc);
        m_actLineNumbers->setEnabled(isTextDoc);
        if (m_textViewer) m_actLineNumbers->setChecked(m_textViewer->isLineNumbersVisible());
    }
    if (m_actSyntaxHighlight) {
        m_actSyntaxHighlight->setVisible(isTextDoc);
        m_actSyntaxHighlight->setEnabled(isTextDoc);
        if (m_textViewer) m_actSyntaxHighlight->setChecked(m_textViewer->isSyntaxHighlightingEnabled());
    }
    if (m_actWordWrap) {
        m_actWordWrap->setVisible(isTextDoc);
        m_actWordWrap->setEnabled(isTextDoc);
        if (m_textViewer) m_actWordWrap->setChecked(m_textViewer->isWordWrapEnabled());
    }
    if (m_actShowWhitespace) {
        m_actShowWhitespace->setVisible(isTextDoc);
        m_actShowWhitespace->setEnabled(isTextDoc);
        if (m_textViewer) m_actShowWhitespace->setChecked(m_textViewer->isShowWhitespace());
    }

    // PDF Tools in Edit Menu
    bool isPdfDoc = (isPdf && !isFolder);

    if (m_actPdfHighlighter) {
        m_actPdfHighlighter->setVisible(isPdfDoc);
        m_actPdfHighlighter->setEnabled(isPdfDoc);
    }
    if (m_menuPdfMarkup) {
        m_menuPdfMarkup->menuAction()->setVisible(isPdfDoc);
        m_menuPdfMarkup->menuAction()->setEnabled(isPdfDoc);
    }
    if (m_menuPdfDocOps) {
        m_menuPdfDocOps->menuAction()->setVisible(isPdfDoc);
        m_menuPdfDocOps->menuAction()->setEnabled(isPdfDoc);
    }
    if (m_actPdfSelectArea) {
        m_actPdfSelectArea->setVisible(isPdfDoc);
        m_actPdfSelectArea->setEnabled(isPdfDoc);
    }
    if (m_actPdfTypewriter) {
        m_actPdfTypewriter->setVisible(isPdfDoc);
        m_actPdfTypewriter->setEnabled(isPdfDoc);
    }
    if (m_actPdfSignature) {
        m_actPdfSignature->setVisible(isPdfDoc);
        m_actPdfSignature->setEnabled(isPdfDoc);
    }
    if (m_actPdfInsertImage) {
        m_actPdfInsertImage->setVisible(isPdfDoc);
        m_actPdfInsertImage->setEnabled(isPdfDoc);
    }
    if (m_actPdfInsertBlankPage) {
        m_actPdfInsertBlankPage->setVisible(isPdfDoc);
        m_actPdfInsertBlankPage->setEnabled(isPdfDoc);
    }
    if (m_actPdfWatermark) {
        m_actPdfWatermark->setVisible(isPdfDoc);
        m_actPdfWatermark->setEnabled(isPdfDoc);
    }
    if (m_actPdfShape) {
        m_actPdfShape->setVisible(isPdfDoc);
        m_actPdfShape->setEnabled(isPdfDoc);
    }
    if (m_actPdfSnapshot) {
        m_actPdfSnapshot->setVisible(isPdfDoc);
        m_actPdfSnapshot->setEnabled(isPdfDoc);
    }
    if (m_actPdfRedact) {
        m_actPdfRedact->setVisible(isPdfDoc);
        m_actPdfRedact->setEnabled(isPdfDoc);
    }
    if (m_actMergePdf) {
        m_actMergePdf->setVisible(isPdfDoc);
        m_actMergePdf->setEnabled(isPdfDoc);
    }
    if (m_actOrganizePdf) {
        m_actOrganizePdf->setVisible(isPdfDoc);
        m_actOrganizePdf->setEnabled(isPdfDoc);
    }
    if (m_actRepairPdf) {
        m_actRepairPdf->setVisible(isPdfDoc);
        m_actRepairPdf->setEnabled(isPdfDoc);
    }

    // 3. VIEW MENU
    if (m_actFolderView) {
        m_actFolderView->setVisible(true);
        m_actFolderView->setEnabled(!isFolder);
    }
    if (!m_archiveOriginPath.isEmpty() && !m_archiveEntries.isEmpty()) {
        if (m_actPrev) {
            m_actPrev->setVisible(true);
            m_actPrev->setEnabled(true);
            m_actPrev->setToolTip(m_archiveCurrentEntryIndex == 0
                ? tr("Return to Archive (Left Arrow / Backspace)")
                : tr("Previous File (Left Arrow)"));
        }
        if (m_actNext) {
            m_actNext->setVisible(true);
            m_actNext->setEnabled(m_archiveCurrentEntryIndex < m_archiveEntries.size() - 1);
            m_actNext->setToolTip(tr("Next File (Right Arrow)"));
        }
    } else {
        if (m_actPrev) {
            m_actPrev->setVisible(true);
            m_actPrev->setEnabled(!isFolder && m_currentIndex > 0);
            m_actPrev->setToolTip(tr("Previous File (Left Arrow)"));
        }
        if (m_actNext) {
            m_actNext->setVisible(true);
            m_actNext->setEnabled(!isFolder && m_currentIndex >= 0 && m_currentIndex < m_siblingFiles.size() - 1);
            m_actNext->setToolTip(tr("Next File (Right Arrow)"));
        }
    }
    if (m_actPdfPrevPage) {
        m_actPdfPrevPage->setEnabled(isPdfDoc && m_pdfViewer && m_pdfViewer->currentPage() > 0);
    }
    if (m_actPdfNextPage) {
        m_actPdfNextPage->setEnabled(isPdfDoc && m_pdfViewer && m_pdfViewer->currentPage() < m_pdfViewer->pageCount() - 1);
    }
    if (m_actFullscreen) {
        m_actFullscreen->setEnabled(true);
    }

    if (m_actSlideshow) {
        m_actSlideshow->setVisible(isImage && !isFolder);
        m_actSlideshow->setEnabled(isImage && !isFolder);
    }

    bool canZoom = (isImage || isPdf || isOffice) && !isFolder;
    if (m_actZoomIn) {
        m_actZoomIn->setVisible(true);
        m_actZoomIn->setEnabled(canZoom);
    }
    if (m_actZoomOut) {
        m_actZoomOut->setVisible(true);
        m_actZoomOut->setEnabled(canZoom);
    }
    if (m_actZoomReset) {
        m_actZoomReset->setVisible(true);
        m_actZoomReset->setEnabled(canZoom);
    }
    if (m_actFit) {
        m_actFit->setVisible(true);
        m_actFit->setEnabled(canZoom);
    }
    if (m_actZoomTool) {
        m_actZoomTool->setVisible(true);
        m_actZoomTool->setEnabled(canZoom);
        m_actZoomTool->setChecked(isZoomToolActive());
    }

    if (m_actFilmstrip) {
        m_actFilmstrip->setVisible(!isFolder);
        m_actFilmstrip->setEnabled(!isFolder && !m_siblingFiles.isEmpty());
    }

    if (m_actPdfToc) {
        m_actPdfToc->setVisible(isPdf && !isFolder);
        m_actPdfToc->setEnabled(isPdf && !isFolder && m_pdfViewer && m_pdfViewer->hasOutline());
        if (m_pdfViewer) m_actPdfToc->setChecked(m_pdfViewer->isTocVisible());
    }


    // 4. TOOLS MENU
    if (m_actSearch) {
        m_actSearch->setVisible((isText || isPdf) && !isFolder);
        m_actSearch->setEnabled((isText || isPdf) && !isFolder);
    }
    if (m_actOcr) {
        m_actOcr->setVisible(isImage || isPdf);
        m_actOcr->setEnabled(hasFile);
    }
    bool hasPdfimages = BuildConfig::isPdfimagesAvailable();
    if (m_actExtractImages) {
        m_actExtractImages->setVisible(isPdf && hasPdfimages);
        m_actExtractImages->setEnabled(isPdf && hasPdfimages && hasFile);
    }
    bool hasQpdf = BuildConfig::isQpdfAvailable();
    if (m_actOrganizePdf) {
        m_actOrganizePdf->setVisible(isPdf && hasQpdf);
        m_actOrganizePdf->setEnabled(isPdf && hasQpdf && hasFile);
    }
    if (m_actMergePdf) {
        m_actMergePdf->setVisible(isPdf && hasQpdf);
        m_actMergePdf->setEnabled(isPdf && hasQpdf && hasFile);
    }
    if (m_actRepairPdf) {
        m_actRepairPdf->setVisible(isPdf && hasQpdf);
        m_actRepairPdf->setEnabled(isPdf && hasQpdf && hasFile);
    }
    if (m_actAudioTrim) {
        m_actAudioTrim->setVisible(isMedia && !isFolder);
        m_actAudioTrim->setEnabled(isMedia && !isFolder);
    }
    if (m_actAudioTags) {
        bool isAudioOnly = isMedia && !isFolder && m_mediaViewer && m_mediaViewer->isAudioOnly();
        m_actAudioTags->setVisible(isAudioOnly);
        m_actAudioTags->setEnabled(isAudioOnly);
    }
    if (m_actAudioExtract) {
        bool isVideo = isMedia && !isFolder && m_mediaViewer && !m_mediaViewer->isAudioOnly();
        m_actAudioExtract->setVisible(isVideo);
        m_actAudioExtract->setEnabled(isVideo);
    }
    if (m_actToolsExportAs) {
        m_actToolsExportAs->setVisible((isImage || isPdf || isText || isOffice) && !isFolder);
        m_actToolsExportAs->setEnabled(hasFile && !isFolder);
    }
    updateDiscoveryActions();
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

    // Insert banner at the very top of central container (index 0)
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(m_centralContainer->layout());
    if (vbox) {
        vbox->insertWidget(0, m_updateBanner, 0);
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

    QVBoxLayout *vbox = qobject_cast<QVBoxLayout*>(m_centralContainer->layout());
    if (vbox) {
        vbox->insertWidget(0, m_debugBanner, 0);
    }
}

void MainWindow::updateBlinkEffect()
{
    if (!m_btnUpdateNotification || !m_btnUpdateNotification->isVisible()) return;

    m_updateBlinkState = !m_updateBlinkState;

    if (m_updateBlinkState) {
        // Bright glowing Amber state
        m_btnUpdateNotification->setText(QString("🔔 ") + tr("Update: v%1").arg(m_availableRelease.version));
        m_btnUpdateNotification->setStyleSheet(
            "QPushButton {"
            "   background-color: #f59e0b;"
            "   color: #0f172a;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "   padding: 2px 8px;"
            "   border-radius: 4px;"
            "   margin-right: 6px;"
            "   border: 1px solid #fbbf24;"
            "}"
            "QPushButton:hover {"
            "   background-color: #d97706;"
            "   color: #ffffff;"
            "}"
        );
    } else {
        // Crisp Sky Blue state
        m_btnUpdateNotification->setText(QString("✨ ") + tr("Update: v%1").arg(m_availableRelease.version));
        m_btnUpdateNotification->setStyleSheet(
            "QPushButton {"
            "   background-color: #0284c7;"
            "   color: #ffffff;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "   padding: 2px 8px;"
            "   border-radius: 4px;"
            "   margin-right: 6px;"
            "   border: 1px solid #38bdf8;"
            "}"
            "QPushButton:hover {"
            "   background-color: #0369a1;"
            "}"
        );
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

        // 2. Configure and show pulsing/blinking status bar button
        if (m_btnUpdateNotification) {
            m_btnUpdateNotification->setToolTip(
                tr("New version v%1 available! Click here to inspect and update.").arg(info.version)
            );
            m_btnUpdateNotification->disconnect();
            connect(m_btnUpdateNotification, &QPushButton::clicked, this, [this, info]() {
                showUpdateDialog(info);
            });
            m_btnUpdateNotification->show();

            // Start blinking animation timer (alternating styles every 750ms)
            if (!m_updateBlinkTimer) {
                m_updateBlinkTimer = new QTimer(this);
                connect(m_updateBlinkTimer, &QTimer::timeout, this, &MainWindow::updateBlinkEffect);
            }
            m_updateBlinkState = false;
            updateBlinkEffect();
            m_updateBlinkTimer->start(750);
        }

        // 3. Fire native desktop notification if notify-send is available
        QProcess::startDetached("notify-send", {
            "-a", "BubezView",
            "-i", "system-software-update",
            tr("BubezView Update Available"),
            tr("Version %1 is now available for download and installation.").arg(info.version)
        });
    });

    // Run silent check in background after 2.5 seconds to guarantee 0ms instant startup
    QTimer::singleShot(2500, this, []() {
        UpdateManager::instance().checkForUpdates(true);
    });
}

void MainWindow::showUpdateDialog(const ReleaseInfo &info)
{
    UpdateDialog dlg(info, this);
    dlg.exec();
}

void MainWindow::openPdfMergeDialog()
{
    QStringList initialFiles;
    QString targetPath = currentActiveTargetFilePath();
    if (!targetPath.isEmpty() && targetPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        initialFiles << targetPath;
    }
    PdfMergeDialog dlg(initialFiles, this);
    connect(&dlg, &PdfMergeDialog::fileMerged, this, [this](const QString &outPath) {
        openFile(outPath);
    });
    dlg.exec();
}

void MainWindow::updateDiscoveryActions()
{
    bool showUninstalled = BuildConfig::showUninstalledFeatures();

    QString targetPath = currentActiveTargetFilePath();
    FileCategory activeCat = m_currentCategory;
    if (m_currentCategory == FileCategory::Folder && !targetPath.isEmpty()) {
        activeCat = PluginManager::categorizeFile(targetPath);
    }
    bool isPdf = (activeCat == FileCategory::Pdf);
    bool isOffice = (activeCat == FileCategory::Office);

    if (m_actOfficeDiscovery) {
        bool officeActive = BuildConfig::instance().enableInternalOffice();
        m_actOfficeDiscovery->setVisible(showUninstalled && isOffice && !officeActive);
    }

    if (m_actQpdfDiscovery) {
        bool qpdfAvail = BuildConfig::isQpdfAvailable();
        m_actQpdfDiscovery->setVisible(showUninstalled && isPdf && !qpdfAvail);
    }

    if (m_actPdfimagesDiscovery) {
        bool pdfimgAvail = BuildConfig::isPdfimagesAvailable();
        m_actPdfimagesDiscovery->setVisible(showUninstalled && isPdf && !pdfimgAvail);
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



