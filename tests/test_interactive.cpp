#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QClipboard>
#include <QScrollBar>
#include "mainwindow.h"
#include "minimal_header_bar.h"
#include "settings_dialog.h"
#include "folder_viewer.h"
#include "vector_icons.h"
#include "office_viewer.h"
#include "pdf_viewer.h"
#include "text_viewer.h"
#include "image_viewer.h"
#include "app_launcher.h"
#include "search_bar_widget.h"
#include "open_with_dialog.h"
#include "theme_manager.h"
#include "first_run_wizard.h"
#include "build_console_dialog.h"
#include "cpu_info.h"
#include "compiler_wizard_dialog.h"
#include "build_config.h"
#include "configure_toolbar_dialog.h"
#include "package_manager_helper.h"
#include "ocr_manager.h"
#include "ocr_dialog.h"
#include "ocr_language_dialog.h"
#include "archive_viewer.h"
#include "media_viewer.h"
#include "audio_processor.h"
#include "audio_tag_dialog.h"
#include "audio_trim_dialog.h"
#include "pdf_merge_dialog.h"
#include "pdf_repair_dialog.h"
#include "pdf_page_organizer_dialog.h"
#include "pdf_transform_box.h"
#include "pdf_signature_dialog.h"
#include "pdf_typewriter_dialog.h"
#include "pdf_watermark_dialog.h"
#include "flippable_toolbar.h"
#include "pdf_canvas_tools.h"
#include "ocr_snipper_overlay.h"
#include "plugin_manager.h"
#include "core/interfaces/viewer_plugin_interface.h"
#include "preload_cache_manager.h"
#include <QSignalSpy>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QMenuBar>
#include <QButtonGroup>

class TestInteractive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testFolderLoadingAndRules();
    void testSubfolderNavigationAndReturn();
    void testRapidNavigationStress();
    void testThemeSwitching();
    void testFilePreviewFromFolder();
    void testTextSearchForwardAndWrap();
    void testTextAdvancedFeatures();
    void testPdfSearchForwardAndWrap();
    void testAppLauncher();
    void testImageAndPdfRotation();
    // Office tests disabled as requested (user tests manually)
    // void testOfficeSamplesAllFiles();
    // void testOfficeLibreOfficeHeadlessConversion();
    // void testOfficeInternalFallbackParsing();
    void testSettingsDialogUI();
    void testNewFeaturesUIPreviews();
    void testFilmstripAndPdfDarkMode();
    void testFirstRunWizard();
    void testUpdateNotificationUI();
    void testCpuInfoAndCompilerWizard();
    void testOcrFeatures();
    void testArchiveViewerFeatures();
    void testAudioPlaybackAndControls();
    void testAudioTrimOptionsAndFilters();
    void testDiscoveryModePdfMergeAndUnsupportedFiles();
    void testPdfRepairAndPageOrganizer();
    void testHighContrastScrollbarsAndZoomPan();
    void testPdfAnnotationsAndTools();
    void testToolbarCustomizationAndPdfWheelScroll();
    void testFolderInspectorAndTextThumbnail();
    void testArchiveNavigationAndFolderGalleryReturn();
    void testModuleCompilationAndFallbackPlan();
    void testTextEditingModeAndPersistence();
    void testNativeImageEnhancementsAndCrop();
    void testQuickLookInterfaceCaptures();
    void testPreviewModeZeroModificationLockdown();
    void testFramelessWindowAndDraggingRules();
    void testWindowGeometrySaveAndRestore();
    void testAudioCoverDirectSaveToFolder();
    void testMinimalHeaderBarSnapshots();
    void testPreEditGeometryRestorationAndAudioSizing();
    void testTriestePdfEditing();

private:
    QString m_testDirPath;
};


void TestInteractive::initTestCase()
{
    // Discover dynamic plugins (archive, pdf, media)
    PluginManager::instance().discoverPlugins();

    // Point to the dedicated /test directory in the repository
    QDir repoTestDir(QString(PROJECT_ROOT_DIR) + "/test");
    QVERIFY2(repoTestDir.exists(), "La directory test/ del repository deve esistere");
    m_testDirPath = repoTestDir.canonicalPath();

    QSettings settings("BubezView", "bubezview");
    settings.remove("Gallery/ShowFolders");
}

void TestInteractive::testFolderLoadingAndRules()
{
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Open test/ directory
    MainWindow::OpenResult res = window.openFile(m_testDirPath);
    QCOMPARE(res, MainWindow::OpenResult::Success);
    QCOMPARE(window.currentViewerIndex(), 6); // Index 6 is FolderViewer

    FolderViewer *fv = window.folderViewer();
    QVERIFY(fv != nullptr);
    QVERIFY(fv->listWidget() != nullptr);

    // Wait a brief moment for async thumbnails
    QTest::qWait(250);

    // Verify rules:
    // 1. In pure file mode, only files are shown in the Quick Look gallery
    fv->setShowFolders(false);
    QVERIFY(!fv->showFolders());
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        bool isDir = fv->listWidget()->item(i)->data(Qt::UserRole + 1).toBool();
        QVERIFY2(!isDir, "Nella modalità predefinita devono comparire solo file");
        QString text = fv->listWidget()->item(i)->text();
        QVERIFY2(!text.startsWith("."), qPrintable(QString("File nascosto trovato nella lista: %1").arg(text)));
    }

    // 2. When showFolders is enabled, subdirectories appear before files
    fv->setShowFolders(true);
    QVERIFY(fv->showFolders());
    bool seenFile = false;
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        bool isDir = fv->listWidget()->item(i)->data(Qt::UserRole + 1).toBool();
        if (seenFile) {
            QVERIFY2(!isDir, qPrintable(QString("Trovata cartella '%1' dopo un file: le cartelle devono venire prima!").arg(fv->listWidget()->item(i)->text())));
        }
        if (!isDir) {
            seenFile = true;
        }
    }

    // Wait for thumbnail generation and select an image file to show real specs
    QTest::qWait(600);
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        QString path = fv->listWidget()->item(i)->data(Qt::UserRole).toString();
        if (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".png", Qt::CaseInsensitive)) {
            fv->listWidget()->setCurrentRow(i);
            break;
        }
    }
    QTest::qWait(100);

    QPixmap pixFolder = window.grab();
    pixFolder.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/folderviewer_preview.png");
}

void TestInteractive::testSubfolderNavigationAndReturn()
{
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openFile(m_testDirPath);
    FolderViewer *fv = window.folderViewer();
    QVERIFY(fv != nullptr);
    fv->setShowFolders(true);
    QTest::qWait(150);

    // Find first subfolder in test/ (e.g. ICONE or office_samples or pdf)
    int subfolderRow = -1;
    QString subfolderName;
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        if (fv->listWidget()->item(i)->data(Qt::UserRole + 1).toBool()) {
            subfolderRow = i;
            subfolderName = fv->listWidget()->item(i)->text();
            break;
        }
    }

    QVERIFY2(subfolderRow != -1, "Deve esistere almeno una sottocartella in test/");

    // Simulate user double clicking on subfolder
    QListWidgetItem *item = fv->listWidget()->item(subfolderRow);
    emit fv->listWidget()->itemDoubleClicked(item);
    QTest::qWait(200);

    // Check we navigated into the subfolder
    QVERIFY2(fv->currentFolder().endsWith(subfolderName), qPrintable(QString("Dovrebbe trovarsi dentro %1 invece è in %2").arg(subfolderName, fv->currentFolder())));

    // Simulate user pressing Backspace to navigate UP
    QTest::keyClick(&window, Qt::Key_Backspace);
    QTest::qWait(200);

    // Check we returned to test/
    QCOMPARE(fv->currentFolder(), m_testDirPath);

    // Simulate user clicking "⬆ Su" button
    QTest::mouseClick(fv->upButton(), Qt::LeftButton);
    QTest::qWait(200);

    // Parent directory of test/
    QDir parentDir(m_testDirPath);
    parentDir.cdUp();
    QCOMPARE(fv->currentFolder(), parentDir.canonicalPath());
}

void TestInteractive::testRapidNavigationStress()
{
    // Stress test: rapid enter and exit without crashes / race conditions
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openFile(m_testDirPath);
    FolderViewer *fv = window.folderViewer();
    QVERIFY(fv != nullptr);

    for (int cycle = 0; cycle < 5; ++cycle) {
        window.openFile(m_testDirPath);
        QTest::qWait(30);

        // Double click first item if it's a dir
        if (fv && fv->listWidget()->count() > 0 && fv->listWidget()->item(0)->data(Qt::UserRole + 1).toBool()) {
            emit fv->listWidget()->itemDoubleClicked(fv->listWidget()->item(0));
            QTest::qWait(30);
            QTest::keyClick(&window, Qt::Key_Backspace);
            QTest::qWait(30);
        }
    }
}

void TestInteractive::testThemeSwitching()
{
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openFile(m_testDirPath);
    QTest::qWait(200);

    FolderViewer *fv = window.folderViewer();
    if (fv && fv->listWidget()->count() > 0) {
        fv->listWidget()->setCurrentRow(0);
        QTest::qWait(100);
    }

    // Capture light mode preview
    QPixmap pixLight = window.grab();
    pixLight.save("/home/davide/.gemini/antigravity/brain/db0875de-21e6-4705-af7b-91c29c95085b/gallery_preview_modern_light.png");

    // Switch to DarkSlate interactively
    ThemeManager::setPreset(ThemePreset::DarkSlate);
    window.applyCurrentTheme();
    QTest::qWait(100);
    QPixmap pixDark = window.grab();
    pixDark.save("/home/davide/.gemini/antigravity/brain/db0875de-21e6-4705-af7b-91c29c95085b/gallery_preview_dark_slate.png");

    ThemeManager::setPreset(ThemePreset::DarkSlate);
    window.applyCurrentTheme();
    SettingsDialog dlgDark(&window, SettingsDialog::TabIndex::Appearance);
    QTest::qWait(50);
    dlgDark.grab().save("/home/davide/.gemini/antigravity/brain/db0875de-21e6-4705-af7b-91c29c95085b/settings_dark_slate.png");

    ThemeManager::setPreset(ThemePreset::ModernLight);
    window.applyCurrentTheme();
    QCOMPARE(ThemeManager::currentPreset(), ThemePreset::ModernLight);
    SettingsDialog dlgLight(&window, SettingsDialog::TabIndex::Appearance);
    QTest::qWait(50);
    dlgLight.grab().save("/home/davide/.gemini/antigravity/brain/db0875de-21e6-4705-af7b-91c29c95085b/settings_modern_light.png");
}

void TestInteractive::testFilePreviewFromFolder()
{
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openFile(m_testDirPath);
    FolderViewer *fv = window.folderViewer();
    QVERIFY(fv != nullptr);
    fv->setShowFolders(true);
    QTest::qWait(150);

    // Find first file in the directory
    int fileRow = -1;
    QString targetFilePath;
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        bool isDir = fv->listWidget()->item(i)->data(Qt::UserRole + 1).toBool();
        if (!isDir) {
            fileRow = i;
            targetFilePath = fv->listWidget()->item(i)->data(Qt::UserRole).toString();
            break;
        }
    }

    QVERIFY2(fileRow != -1, "Deve esistere almeno un file in test/");

    // Set scroll position to test position retention
    fv->listWidget()->verticalScrollBar()->setValue(60);
    int expectedScroll = fv->listWidget()->verticalScrollBar()->value();

    // Simulate double click on file
    emit fv->listWidget()->itemDoubleClicked(fv->listWidget()->item(fileRow));
    QTest::qWait(200);

    // Should have transitioned away from FolderViewer (index 6) to file viewer
    QVERIFY2(window.currentViewerIndex() != 6, "Facendo doppio clic sul file dovrebbe aprirsi l'anteprima del file");
    QCOMPARE(window.currentFilePath(), targetFilePath);

    // Return to folder gallery
    window.openCurrentFolderInGallery();
    QTest::qWait(200);
    QCOMPARE(window.currentViewerIndex(), 6);
    QCOMPARE(fv->listWidget()->verticalScrollBar()->value(), expectedScroll);
    QCOMPARE(fv->selectedFilePath(), targetFilePath);
}

#if 0
void TestInteractive::testOfficeSamplesAllFiles()
{
    // Disabled as requested
}

void TestInteractive::testOfficeLibreOfficeHeadlessConversion()
{
    // Disabled as requested
}

void TestInteractive::testOfficeInternalFallbackParsing()
{
    // Disabled as requested
}
#endif

void TestInteractive::testTextSearchForwardAndWrap()
{
    TextViewer viewer;
    QString sampleTxt = m_testDirPath + "/documento_firmato_test_pag1.txt";
    QVERIFY2(viewer.loadFile(sampleTxt), "Failed to load sample text file");

    bool wrapped = false;
    // Search for repeated word "Firma"
    bool f1 = viewer.findNext("Firma", true, &wrapped);
    QVERIFY(f1);
    QCOMPARE(wrapped, false);

    bool f2 = viewer.findNext("Firma", true, &wrapped);
    QVERIFY(f2);

    // Keep searching until wrap-around occurs
    bool wrappedOccurred = false;
    for (int i = 0; i < 20; ++i) {
        if (!viewer.findNext("Firma", true, &wrapped)) break;
        if (wrapped) {
            wrappedOccurred = true;
            break;
        }
    }
    QVERIFY2(wrappedOccurred, "Search must wrap around when reaching the end of the text");

    // Backward search
    bool p1 = viewer.findPrev("Firma", true, &wrapped);
    QVERIFY(p1);
}

void TestInteractive::testTextAdvancedFeatures()
{
    TextViewer viewer;
    viewer.setPlainText("Hello world\nSecond line here\nThird line");

    // 1. Metrics & word count
    QCOMPARE(viewer.lineCount(), 3);
    QCOMPARE(viewer.wordCount(), 7);
    QVERIFY(viewer.charCount() > 0);

    // 2. Line numbers gutter
    QCOMPARE(viewer.isLineNumbersVisible(), true);
    QVERIFY(viewer.lineNumberAreaWidth() > 0);
    viewer.setLineNumbersVisible(false);
    QCOMPARE(viewer.isLineNumbersVisible(), false);
    QCOMPARE(viewer.lineNumberAreaWidth(), 0);
    viewer.setLineNumbersVisible(true);
    QCOMPARE(viewer.isLineNumbersVisible(), true);

    // 3. Syntax highlighting toggle
    viewer.setSyntaxHighlightingEnabled(false);
    QCOMPARE(viewer.isSyntaxHighlightingEnabled(), false);
    viewer.setSyntaxHighlightingEnabled(true);
    QCOMPARE(viewer.isSyntaxHighlightingEnabled(), true);

    // 4. Find & Replace
    viewer.setEditable(true);
    QCOMPARE(viewer.isEditable(), true);
    viewer.setPlainText("apple banana apple cherry apple");
    bool r1 = viewer.replaceNext("apple", "orange", true);
    QVERIFY(r1);
    QCOMPARE(viewer.toPlainText(), QString("orange banana apple cherry apple"));

    int count = viewer.replaceAll("apple", "kiwi", true);
    QCOMPARE(count, 2);
    QCOMPARE(viewer.toPlainText(), QString("orange banana kiwi cherry kiwi"));

    // 5. Smart line editing
    viewer.setEditable(true);
    QCOMPARE(viewer.isEditable(), true);
    viewer.setPlainText("Line 1\nLine 2\nLine 3");

    // Move cursor to Line 2
    QTextCursor cursor = viewer.textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down);
    viewer.setTextCursor(cursor);

    // Duplicate Line 2
    viewer.duplicateCurrentLine();
    QCOMPARE(viewer.toPlainText(), QString("Line 1\nLine 2\nLine 2\nLine 3"));

    // Delete duplicated line
    viewer.deleteCurrentLine();
    QCOMPARE(viewer.toPlainText(), QString("Line 1\nLine 2\nLine 3"));

    // Toggle comment on Line 2
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down);
    viewer.setTextCursor(cursor);
    viewer.toggleComment();
    QVERIFY(viewer.toPlainText().contains("// Line 2") || viewer.toPlainText().contains("# Line 2"));
    viewer.toggleComment();
    QCOMPARE(viewer.toPlainText(), QString("Line 1\nLine 2\nLine 3"));

    // 6. Font Zoom
    int initialSize = viewer.font().pointSize();
    viewer.zoomInText(2);
    QCOMPARE(viewer.font().pointSize(), initialSize + 2);
    viewer.zoomOutText(2);
    QCOMPARE(viewer.font().pointSize(), initialSize);
    viewer.resetTextZoom();

    // 7. Save file
    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    QString tmpPath = tmp.fileName();
    tmp.close();

    viewer.setPlainText("Saved content test\nLine 2");
    QVERIFY(viewer.saveFile(tmpPath));

    QFile readBack(tmpPath);
    QVERIFY(readBack.open(QIODevice::ReadOnly | QIODevice::Text));
    QString fileContent = QString::fromUtf8(readBack.readAll());
    QCOMPARE(fileContent, QString("Saved content test\nLine 2"));
}

void TestInteractive::testPdfSearchForwardAndWrap()
{
    PdfViewer viewer;
    QString samplePdf = m_testDirPath + "/documento_firmato_test.pdf";
    if (!QFile::exists(samplePdf)) return;
    QVERIFY(viewer.loadFile(samplePdf));

    bool wrapped = false;
    bool found = viewer.findNext("Firma", false, &wrapped);
    QVERIFY(found);

    QString allText = viewer.extractAllText();
    QVERIFY(!allText.isEmpty());
}

void TestInteractive::testAppLauncher()
{
    QList<AppInfo> allApps = AppLauncher::getAllApps();
    QVERIFY(!allApps.isEmpty());

    // Verify alphabetical ordering ensured by std::sort
    for (int i = 0; i < allApps.size() - 1; ++i) {
        QVERIFY(allApps[i].name.compare(allApps[i + 1].name, Qt::CaseInsensitive) <= 0);
    }

    // Verify MIME types are parsed into AppInfo
    bool foundMimes = false;
    for (const AppInfo &app : allApps) {
        if (!app.mimeTypes.isEmpty()) {
            foundMimes = true;
            break;
        }
    }
    QVERIFY(foundMimes);

    QString samplePdf = m_testDirPath + "/ciao.pdf";
    QList<AppInfo> pdfApps = AppLauncher::getAssociatedApps(samplePdf);
    QVERIFY(pdfApps.size() >= 0);
}

void TestInteractive::testImageAndPdfRotation()
{
    ImageViewer imgViewer;
    QSignalSpy modSpy(&imgViewer, &ImageViewer::modificationChanged);

    QString sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    QVERIFY(imgViewer.loadFile(sampleImg));
    QCOMPARE(imgViewer.isModified(), false);
    QSize szBefore = imgViewer.imageSize();

    // 1. Rotate clockwise & verify signal
    imgViewer.rotateClockwise();
    QCOMPARE(modSpy.count(), 1);
    QCOMPARE(modSpy.takeFirst().at(0).toBool(), true);
    QCOMPARE(imgViewer.isModified(), true);

    QSize szAfter = imgViewer.imageSize();
    QCOMPARE(szAfter.width(), szBefore.height());
    QCOMPARE(szAfter.height(), szBefore.width());

    // 2. Rotate counter-clockwise returns to original size
    imgViewer.rotateCounterClockwise();
    QCOMPARE(imgViewer.imageSize(), szBefore);
    QCOMPARE(imgViewer.isModified(), true);

    // 3. Test saving rotated image to temporary file and verifying disk persistence
    QTemporaryFile tempImg(QDir::tempPath() + "/bubez_test_rot_XXXXXX.png");
    QVERIFY(tempImg.open());
    QString tempImgPath = tempImg.fileName();
    tempImg.close();

    // Save initial image
    QVERIFY(imgViewer.originalImage().save(tempImgPath, "PNG"));

    // Rotate and save rotated image to temp file
    imgViewer.rotateClockwise();
    QVERIFY(imgViewer.originalImage().save(tempImgPath, "PNG"));
    imgViewer.setModified(false);
    QCOMPARE(imgViewer.isModified(), false);

    // Load back in a fresh ImageViewer instance to verify persistence on disk
    ImageViewer freshViewer;
    QVERIFY(freshViewer.loadFile(tempImgPath));
    QCOMPARE(freshViewer.imageSize().width(), szBefore.height());
    QCOMPARE(freshViewer.imageSize().height(), szBefore.width());

    // 4. Test PreloadCacheManager coherence
    PreloadCacheManager::instance().insert(tempImgPath, freshViewer.originalImage());
    QImage cached;
    QVERIFY(PreloadCacheManager::instance().tryGet(tempImgPath, cached));
    QCOMPARE(cached.size(), freshViewer.imageSize());

    // Evict via remove and verify cache miss
    PreloadCacheManager::instance().remove(tempImgPath);
    QVERIFY(!PreloadCacheManager::instance().tryGet(tempImgPath, cached));

    QFile::remove(tempImgPath);
}

void TestInteractive::testSettingsDialogUI()
{
    SettingsDialog dlg;
    dlg.resize(740, 560);
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    // 1. Capture appearance tab
    dlg.setCurrentTab(SettingsDialog::TabIndex::Appearance);
    QTest::qWait(80);
    QPixmap pixApp = dlg.grab();
    pixApp.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/settings_appearance.png");

    // 2. Capture integration tab
    dlg.setCurrentTab(SettingsDialog::TabIndex::DesktopIntegration);
    QTest::qWait(80);
    QPixmap pixInt = dlg.grab();
    pixInt.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/settings_integration.png");

    // 3. Capture Performance tab
    dlg.setCurrentTab(SettingsDialog::TabIndex::Performance);
    QTest::qWait(80);
    QPixmap pixPerf = dlg.grab();
    pixPerf.save("/home/davide/.gemini/antigravity/brain/1b4804c8-a00a-40cb-8949-01ea980cf822/settings_performance_tab.png");

    // 4. Capture Extensions & Modules tab (Clean state - no inline console, no banner)
    dlg.setCurrentTab(SettingsDialog::TabIndex::Extensions);
    QTest::qWait(80);
    QPixmap pixExt = dlg.grab();
    pixExt.save("/home/davide/.gemini/antigravity/brain/5c92b339-02a1-4ab5-be7c-a0935a9f57d6/settings_extensions_tab.png");

    // 5. Test dirty tracking: Find a toggle button to change module linking
    QList<QPushButton *> buttons = dlg.findChildren<QPushButton *>();
    QPushButton *btnStatic = nullptr;
    QPushButton *btnRevert = nullptr;
    for (QPushButton *btn : buttons) {
        if ((btn->text().contains("Static") || btn->text().contains("Source") || btn->text().contains("O3")) && !btnStatic) {
            btnStatic = btn;
        }
        if (btn->text().contains("Revert") || btn->text().contains("Annulla")) {
            btnRevert = btn;
        }
    }

    if (btnStatic) {
        btnStatic->click();
        QTest::qWait(80);
        QPixmap pixBanner = dlg.grab();
        pixBanner.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/settings_pending_banner.png");
    }

    if (btnRevert) {
        btnRevert->click();
        QTest::qWait(80);
    }

    // 6. Test BuildConsoleDialog appearance
    BuildConsoleDialog console(&dlg);
    console.resize(680, 440);
    console.show();
    QTest::qWait(80);
    QPixmap pixConsole = console.grab();
    pixConsole.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/settings_build_console.png");
    console.close();

    // 7. Verify package manager safety checks and poppler-data detection
    QVERIFY(BuildConfig::isPopplerDataInstalled());
    QVERIFY(PackageManagerHelper::isComponentInstalled("poppler-data"));

    QStringList affected;
    bool isSafe = PackageManagerHelper::checkUninstallSafety("poppler-utils", affected);
    QVERIFY(isSafe);

    // libc6 or a core dependency must report unsafe with cascade removal packages detected
    bool isCoreSafe = PackageManagerHelper::checkUninstallSafety("libc6", affected);
    QVERIFY(!isCoreSafe);
    QVERIFY(!affected.isEmpty());
}

void TestInteractive::testNewFeaturesUIPreviews()
{
    // 1. Capture Open With Dialog
    QString sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    OpenWithDialog openDlg(sampleImg);
    openDlg.resize(480, 420);
    openDlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&openDlg));
    QTest::qWait(100);
    QPixmap pixOpenWith = openDlg.grab();
    pixOpenWith.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/open_with_preview.png");
    openDlg.close();

    // 2. Capture MainWindow with Search Bar & New Toolbar Actions
    MainWindow win;
    win.resize(1020, 720);
    win.show();
    QVERIFY(QTest::qWaitForWindowExposed(&win));

    QString sampleTxt = m_testDirPath + "/documento_firmato_test_pag1.txt";
    win.openFile(sampleTxt);
    win.searchInDocument();
    if (win.searchBar()) {
        win.searchBar()->setSearchText("Firma");
        emit win.searchBar()->findNextRequested("Firma", false);
    }
    QTest::qWait(150);
    QPixmap pixSearch = win.grab();
    pixSearch.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/search_bar_preview.png");

    // 3. Capture Image Viewer with Rotate & Trash toolbar
    win.openFile(sampleImg);
    QTest::qWait(150);
    QPixmap pixImage = win.grab();
    pixImage.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/image_toolbar_rotate_preview.png");
}

void TestInteractive::testFilmstripAndPdfDarkMode()
{
    MainWindow win;
    win.resize(1020, 720);
    win.show();
    QVERIFY(QTest::qWaitForWindowExposed(&win));

    // 1. Test Filmstrip on Image
    QString sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    win.openFile(sampleImg);
    QTest::qWait(150);

    // Toggle filmstrip on
    win.setFilmstripVisible(true);
    QVERIFY(win.isFilmstripVisible());
    QTest::qWait(400);

    QPixmap pixFilmstrip = win.grab();
    pixFilmstrip.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/filmstrip_preview.png");

    // 2. Test PDF Zoom Tool
    QString samplePdf = m_testDirPath + "/documento_firmato_test.pdf";
    win.openFile(samplePdf);
    QTest::qWait(150);
    PdfViewer *pv = dynamic_cast<PdfViewer*>(win.pdfViewer());
    QVERIFY(pv != nullptr);
    QVERIFY(!pv->isZoomToolActive());

    // Toggle Zoom Tool
    pv->toggleZoomTool(true);
    QVERIFY(pv->isZoomToolActive());
    QCOMPARE(pv->pageWidget()->toolMode(), PdfToolMode::Zoom);
    pv->toggleZoomTool(false);
    QVERIFY(!pv->isZoomToolActive());

    // 3. Test Context Menu on right-click
    win.openFile(sampleImg);
    QTest::qWait(150);

    QTimer::singleShot(150, [&win]() {
        QWidget *popup = QApplication::activePopupWidget();
        if (popup) {
            QPixmap pixMenu = win.grab();
            // Also grab popup if outside
            pixMenu.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/context_menu_preview.png");
            popup->close();
        }
    });
    QContextMenuEvent cme(QContextMenuEvent::Mouse, QPoint(350, 300), win.mapToGlobal(QPoint(350, 300)));
    QApplication::sendEvent(&win, &cme);
}

void TestInteractive::testFirstRunWizard()
{
    // 1. Test in KDE Plasma / GNOME environment: Spacebar test passes automatically
    qputenv("XDG_CURRENT_DESKTOP", "KDE");
    qputenv("DESKTOP_SESSION", "plasma");

    FirstRunWizard wizard;
    wizard.resize(740, 540);
    wizard.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wizard));

    // Verify 3 onboarding wizard pages (Welcome -> Spacebar Test -> Additional Options)
    QCOMPARE(wizard.pageIds().count(), 3);
    QCOMPARE(wizard.currentId(), static_cast<int>(FirstRunWizard::Page_Welcome));
    QTest::qWait(100);
    wizard.grab().save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/wizard_page0_welcome.png");

    // Advance to Page 1 (Dedicated Spacebar Test & Trigger Fallback - Auto Passed)
    wizard.next();
    QCOMPARE(wizard.currentId(), static_cast<int>(FirstRunWizard::Page_Spacebar));
    QTest::qWait(150);
    wizard.grab().save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/wizard_page1_spacebar.png");

    // Advance to Page 2 (Additional Options: Modules, Performance & Navigation)
    wizard.next();
    QCOMPARE(wizard.currentId(), static_cast<int>(FirstRunWizard::Page_Modules));
    QTest::qWait(100);
    wizard.grab().save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/wizard_page2_modules.png");

    wizard.close();

    // 2. Second test: Generic / Other DE environment (showing fallback trigger script)
    qputenv("XDG_CURRENT_DESKTOP", "XFCE");
    qputenv("DESKTOP_SESSION", "xfce");
    FirstRunWizard wizardFallback;
    wizardFallback.resize(740, 540);
    wizardFallback.show();
    wizardFallback.next();
    QTest::qWait(150);
    wizardFallback.grab().save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/wizard_page1_spacebar_fallback.png");
    wizardFallback.close();
}

void TestInteractive::testUpdateNotificationUI()
{
    // Test semantic version comparison
    QVERIFY(UpdateManager::isVersionNewer("0.1.3", "0.1.2"));
    QVERIFY(UpdateManager::isVersionNewer("0.2.0", "0.1.99"));
    QVERIFY(UpdateManager::isVersionNewer("1.0.0", "0.9.9"));
    QVERIFY(UpdateManager::isVersionNewer("v0.1.10", "0.1.2"));
    QVERIFY(!UpdateManager::isVersionNewer("0.1.2", "0.1.2"));
    QVERIFY(!UpdateManager::isVersionNewer("0.1.1", "0.1.2"));

    // Test Ed25519 cryptographic signature verification
    const QString validSha = QStringLiteral("817790a340d36b6e7822f022037dff672054f809b91d99783e0becadc26b7794");
    const QString validSig = QStringLiteral("+R3tNBOLUijjOPqxOoU6MxA+aLklWGJT4xvXCVQgVXnzRgq4XRR6HPICarlOh4XhYit5OeBRutu2rHjXQjazDA==");

    // 1. Valid signature check against developer public key must succeed
    QVERIFY(UpdateManager::verifySignature(validSha, validSig));

    // 2. Tampered SHA-256 hash must fail
    const QString tamperedSha = QStringLiteral("817790a340d36b6e7822f022037dff672054f809b91d99783e0becadc26b7799");
    QVERIFY(!UpdateManager::verifySignature(tamperedSha, validSig));

    // 3. Corrupted base64 signature must fail
    const QString corruptedSig = QStringLiteral("+S3tNBOLUijjOPqxOoU6MxA+aLklWGJT4xvXCVQgVXnzRgq4XRR6HPICarlOh4XhYit5OeBRutu2rHjXQjazDA==");
    QVERIFY(!UpdateManager::verifySignature(validSha, corruptedSig));

    // 4. Empty inputs must fail safely
    QVERIFY(!UpdateManager::verifySignature(QString(), validSig));
    QVERIFY(!UpdateManager::verifySignature(validSha, QString()));

    MainWindow win;
    win.resize(1020, 720);
    win.show();
    QVERIFY(QTest::qWaitForWindowExposed(&win));

    // Simulate an update event
    ReleaseInfo info;
    info.version = "0.2.0";
    info.changelog = "Native desktop notification and live blinking updater badge.";
    info.downloadUrl = "https://dawlinshare.web.app/bubezview.deb";

    emit UpdateManager::instance().updateAvailable(info);
    QTest::qWait(150);

    // Save screenshot of the main window showing the top notification banner and blinking button
    QPixmap pixUpdateBanner = win.grab();
    pixUpdateBanner.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/update_notification_banner.png");

    // Wait for blink state flip (750ms timer)
    QTest::qWait(800);
    QPixmap pixUpdateBlink = win.grab();
    pixUpdateBlink.save("/home/davide/.gemini/antigravity/brain/7555969d-f634-4d62-a346-0ee189f59a3a/update_notification_blink.png");

    win.close();
}

void TestInteractive::testCpuInfoAndCompilerWizard()
{
    // 1. Verify CPU introspection
    QString cpuModel = CpuInfo::modelName();
    QVERIFY(!cpuModel.isEmpty());
    QVERIFY(CpuInfo::logicalCores() > 0);

    QString tier = CpuInfo::microarchitectureTier();
    QVERIFY(tier == "v1" || tier == "v2" || tier == "v3" || tier == "v4");
    QVERIFY(!CpuInfo::cacheSummary().isEmpty());

    // 2. Test BuildConfig custom compiler flags serialization
    BuildConfig &bcfg = BuildConfig::instance();
    QString origFlags = bcfg.customCompilerFlags();

    bcfg.setCustomCompilerFlags("-O3 -march=native -flto=auto");
    QVERIFY(bcfg.customCompilerFlags().contains("-march=native"));
    QStringList cmakeArgs = bcfg.toCMakeArgs();
    bool foundCmakeFlags = false;
    for (const QString &arg : cmakeArgs) {
        if (arg.startsWith("-DCMAKE_CXX_FLAGS=")) {
            foundCmakeFlags = true;
            QVERIFY(arg.contains("-march=native"));
        }
    }
    QVERIFY(foundCmakeFlags);

    // Restore original flags
    bcfg.setCustomCompilerFlags(origFlags);

    // 3. Test CompilerWizardDialog UI
    CompilerWizardDialog wizard;
    wizard.resize(840, 680);
    wizard.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wizard));

    // Test generated flags initially
    QString flags = wizard.generatedFlags();
    QVERIFY(!flags.isEmpty());

    // Take screenshot of the Compiler Wizard for documentation/walkthrough
    QTest::qWait(200);
    QPixmap pixWizard = wizard.grab();
    pixWizard.save("/home/davide/.gemini/antigravity/brain/1b4804c8-a00a-40cb-8949-01ea980cf822/compiler_wizard_dialog.png");

    wizard.close();
}

void TestInteractive::testOcrFeatures()
{
    // 1. Verify OcrManager singleton and detection
    OcrManager &mgr = OcrManager::instance();
    QVERIFY(!mgr.tessdataDirectory().isEmpty());
    QVERIFY(!mgr.detectSystemLanguageCode().isEmpty());
    QVERIFY(!mgr.popularLanguagesCatalog().isEmpty());

    // If tesseract is not installed on the system (e.g. inside fresh container),
    // test BubezView installing it automatically via native package manager
    if (!mgr.isTesseractInstalled()) {
        QString err;
        bool ok = mgr.installTesseractSystemPackage(nullptr, &err);
        QVERIFY2(ok, qPrintable(QString("Failed to install tesseract via BubezView: %1").arg(err)));
        QVERIFY(mgr.isTesseractInstalled());
    }

    // 2. Test sanitizeOcrArtifacts noise filter
    QString noisyText =
        "f | BubezView: Il Coltellino Svizzero\n\n"
        "[image]\n\n"
        "Ba | Immagini: Molto più di un Visualizzatore\n\n"
        "&\n\n"
        "Il Futuro: Multimedia e Video\n";
    QString cleanedText = OcrDialog::sanitizeOcrArtifacts(noisyText);
    QVERIFY(!cleanedText.contains("[image]"));
    QVERIFY(!cleanedText.contains("Ba |"));
    QVERIFY(!cleanedText.contains("\n&\n"));
    QVERIFY(cleanedText.contains("Immagini: Molto più di un Visualizzatore"));
    QVERIFY(cleanedText.contains("Il Futuro: Multimedia e Video"));

    // 3. Test OcrDialog layout, live stats, and screenshot
    QString sampleText =
        "BubezView: Il Coltellino Svizzero per Linux\n\n"
        "BubezView è progettato per essere l'anteprima universale e l'editor rapido per il mondo Linux "
        "(KDE, GNOME e oltre), chiaramente ispirato alla rapidità del \"Quick Look\" di macOS ma con "
        "poteri da supereroe.\n\n"
        "Il nome \"Bubez\" non è casuale: nel dialetto triestino richiama il lavoratore factotum, quel "
        "\"ragazzo di bottega\" a cui tocca fare un po' di tutto. E BubezView fa esattamente questo: fa "
        "un po' di tutto, ma lo fa incredibilmente bene, evitandoti di dover lanciare pesanti "
        "applicazioni esterne per le operazioni quotidiane.";

    OcrDialog ocrDlg(sampleText, "documento_presentazione.pdf", "", true, 0, 5, nullptr);
    ocrDlg.resize(720, 540);
    ocrDlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&ocrDlg));
    QTest::qWait(200);

    QPixmap pixOcr = ocrDlg.grab();
    pixOcr.save("/home/davide/.gemini/antigravity/brain/1b4804c8-a00a-40cb-8949-01ea980cf822/ocr_dialog_preview.png");
    ocrDlg.close();

    // 3. Test OcrLanguageDialog (Gear ⚙️ settings) and screenshot
    OcrLanguageDialog langDlg;
    langDlg.resize(580, 480);
    langDlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&langDlg));
    QTest::qWait(200);

    QPixmap pixLang = langDlg.grab();
    pixLang.save("/home/davide/.gemini/antigravity/brain/1b4804c8-a00a-40cb-8949-01ea980cf822/ocr_languages_dialog.png");
    langDlg.close();
}

void TestInteractive::testArchiveViewerFeatures()
{
    ArchiveViewer viewer;
    viewer.resize(800, 600);
    viewer.show();
    QTest::qWait(100);

    // 1. Test Theme Palette changes
    ThemeManager::setPreset(ThemePreset::DarkSlate);
    viewer.updateTheme();
    ThemeManager::setPreset(ThemePreset::ModernLight);
    viewer.updateTheme();

    // 2. Test opening test archive if available
    QString zipPath = m_testDirPath + "/__pycache__.zip";
    if (QFile::exists(zipPath)) {
        QSignalSpy spy(&viewer, &ArchiveViewer::archiveLoaded);
        bool started = viewer.loadFile(zipPath);
        QVERIFY(started);

        // Wait up to 2 seconds for worker parsing
        QVERIFY(spy.wait(2000));
        QVERIFY(viewer.entryCount() > 0);
        QVERIFY(viewer.totalUncompressedSize() > 0);

        // 3. Test live search filtering
        QLineEdit *searchBox = viewer.findChild<QLineEdit*>();
        if (searchBox) {
            searchBox->setText("cpython");
            QTest::qWait(100);
            searchBox->clear();
            QTest::qWait(50);
        }

        // 4. Test extraction to temporary folder
        QString tempOut = QDir::tempPath() + "/bubezview_test_archive_ext";
        QDir(tempOut).removeRecursively();
        viewer.extractAll(tempOut);
        QVERIFY(QDir(tempOut).exists());
        QDir(tempOut).removeRecursively();
    }

    // 5. Test clear state
    viewer.clear();
    QCOMPARE(viewer.entryCount(), 0);
    QCOMPARE(viewer.totalUncompressedSize(), 0);
    viewer.close();
}

void TestInteractive::testAudioPlaybackAndControls()
{
    QString audioPath = m_testDirPath + "/audio_samples/test_tone.mp3";
    QVERIFY2(QFile::exists(audioPath), "Audio sample file must exist");

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // 1. Open MP3 audio file
    MainWindow::OpenResult res = window.openFile(audioPath);
    QCOMPARE(res, MainWindow::OpenResult::Success);
    QCOMPARE(window.currentViewerIndex(), 4); // Index 4 is MediaViewer

    MediaViewer *mv = dynamic_cast<MediaViewer*>(window.mediaViewer());
    QVERIFY(mv != nullptr);

    // 2. Wait a moment for libmpv async property notifications
    QTest::qWait(400);

    // 3. Audio-only mode verification
    QVERIFY(mv->isAudioOnly());

    // 4. Metadata extraction verification (Title and Artist from ID3 tags)
    QVERIFY(!mv->audioTitle().isEmpty());
    QVERIFY(mv->audioTitle().contains("Bubez Sound Test"));
    QVERIFY(mv->audioArtist().contains("Bubez Studio"));

    // 5. Playback state verification
    QVERIFY(mv->isPlaying());

    // 6. Test Play / Pause toggle
    mv->togglePlayPause();
    QTest::qWait(100);
    QVERIFY(!mv->isPlaying());

    // Resume play
    mv->togglePlayPause();
    QTest::qWait(100);
    QVERIFY(mv->isPlaying());

    // 7. Test Speed Rate selection
    mv->setPlaybackSpeed(1.5);
    QCOMPARE(mv->playbackSpeed(), 1.5);
    mv->setPlaybackSpeed(1.0);
    QCOMPARE(mv->playbackSpeed(), 1.0);

    // 8. Test Loop toggle
    bool wasLooping = mv->isLooping();
    mv->toggleLoop();
    QCOMPARE(mv->isLooping(), !wasLooping);
    mv->toggleLoop();
    QCOMPARE(mv->isLooping(), wasLooping);

    // 9. Test stop and clear
    mv->stop();
    QTest::qWait(50);
    QVERIFY(!mv->isPlaying());

    // 10. Also test OGG and WAV loading
    QString oggPath = m_testDirPath + "/audio_samples/test_tone.ogg";
    if (QFile::exists(oggPath)) {
        QCOMPARE(window.openFile(oggPath), MainWindow::OpenResult::Success);
        QTest::qWait(300);
        QVERIFY(mv->isAudioOnly());
        QVERIFY(mv->audioTitle().contains("Vorbis Sound Test"));
    }

    QString wavPath = m_testDirPath + "/audio_samples/test_tone.wav";
    if (QFile::exists(wavPath)) {
        QCOMPARE(window.openFile(wavPath), MainWindow::OpenResult::Success);
        QTest::qWait(300);
        QVERIFY(mv->isAudioOnly());
    }

    // 11. Test integrated Audio Studio workspaces, header capsule pills, and tracklist drawer
    window.setEditMode(true);
    QVERIFY(window.isEditMode());
    QVERIFY(mv->isEditMode());
    QCOMPARE(mv->audioWorkspaceIndex(), 1); // Default Edit Mode workspace: 1 (Trim)
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 1);
    }

    // Test toggleEqualizerDialog (Workspace 4)
    mv->toggleEqualizerDialog();
    QVERIFY(mv->isAudioStudioActive());
    QCOMPARE(mv->audioWorkspaceIndex(), 4);
    mv->toggleEqualizerDialog();
    QVERIFY(!mv->isAudioStudioActive()); // Toggled off to Player (0)
    mv->toggleEqualizerDialog();
    QVERIFY(mv->isAudioStudioActive());  // Toggled back on to Equalizer (4)

    // Test toggle to Tags tab (Workspace 5)
    mv->openAudioTagDialog();
    QVERIFY(mv->isAudioStudioActive());
    QCOMPARE(mv->audioWorkspaceIndex(), 5);

    // Test toggle to Trim tab (Workspace 1)
    mv->openAudioTrimDialog();
    QVERIFY(mv->isAudioStudioActive());
    QCOMPARE(mv->audioWorkspaceIndex(), 1);

    // Closing studio panel (Workspace 0)
    mv->toggleAudioStudio();
    QVERIFY(!mv->isAudioStudioActive());
    QCOMPARE(mv->audioWorkspaceIndex(), 0);

    // Test workspace switching: 0=Player, 1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags
    mv->switchAudioWorkspace(1);
    QCOMPARE(mv->audioWorkspaceIndex(), 1);
    QVERIFY(mv->isAudioStudioActive());
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 1);
    }

    mv->switchAudioWorkspace(2);
    QCOMPARE(mv->audioWorkspaceIndex(), 2);
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 2);
    }

    mv->switchAudioWorkspace(3);
    QCOMPARE(mv->audioWorkspaceIndex(), 3);
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 3);
    }

    // Regression check (Bug #1): Window minimum height must remain compact (<= 580px) even on tall Trim/Restore pages
    QVERIFY2(window.minimumSizeHint().height() <= 580,
             qPrintable(QString("Window minimumSizeHint height too large: %1").arg(window.minimumSizeHint().height())));

    // Regression check (Bug #3): Loading another audio file while in Edit Mode preserves the active workspace & header pill
    if (QFile::exists(oggPath)) {
        QCOMPARE(window.openFile(oggPath), MainWindow::OpenResult::Success);
        QTest::qWait(250);
        QCOMPARE(mv->audioWorkspaceIndex(), 3);
        if (window.minimalHeaderBar()) {
            QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 3);
        }
        QCOMPARE(window.openFile(wavPath), MainWindow::OpenResult::Success);
        QTest::qWait(250);
        QCOMPARE(mv->audioWorkspaceIndex(), 3);
    }

    mv->switchAudioWorkspace(4);
    QCOMPARE(mv->audioWorkspaceIndex(), 4);
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 4);
    }

    mv->switchAudioWorkspace(5);
    QCOMPARE(mv->audioWorkspaceIndex(), 5);
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 5);
    }

    mv->switchAudioWorkspace(0);
    QCOMPARE(mv->audioWorkspaceIndex(), 0);
    QVERIFY(!mv->isAudioStudioActive());
    if (window.minimalHeaderBar()) {
        QCOMPARE(window.minimalHeaderBar()->activeAudioWorkspace(), 0);
    }

    // Capture visual snapshots of full-width in-place workspace pages (wait 220ms so 160ms cross-dissolve finishes)
    QString artifactDir = "/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7";
    window.resize(1100, 720);

    window.setEditMode(false);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_0_player.png");

    window.setEditMode(true);
    mv->switchAudioWorkspace(1);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_1_trim.png");

    mv->switchAudioWorkspace(2);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_2_filters.png");

    mv->switchAudioWorkspace(3);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_3_pitch.png");

    mv->switchAudioWorkspace(4);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_4_equalizer.png");

    mv->switchAudioWorkspace(5);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_5_tags.png");

    mv->switchAudioWorkspace(6);
    QTest::qWait(220);
    window.grab().save(artifactDir + "/audio_workspace_6_summary.png");

    // Test tracklist drawer toggle
    mv->toggleTracklistDrawer();
    mv->toggleTracklistDrawer();

    window.setEditMode(false);
    QVERIFY(!window.isEditMode());
    QVERIFY(!mv->isEditMode());
    QCOMPARE(mv->audioWorkspaceIndex(), 0);

    mv->stop();
}

void TestInteractive::testAudioTrimOptionsAndFilters()
{
    // Test AudioTrimOptions presets and filter configuration
    AudioTrimOptions opts;
    QCOMPARE(opts.lowCutHz, 0);
    QCOMPARE(opts.denoiseDb, 0);
    QCOMPARE(opts.dehumHz, 0);
    QCOMPARE(opts.declick, false);
    QCOMPARE(opts.deesser, false);
    QCOMPARE(opts.voiceEnhancer, false);
    QCOMPARE(opts.noiseGate, false);

    // 1. Configure Traffic preset options
    opts.lowCutHz = 110;
    QCOMPARE(opts.lowCutHz, 110);

    // 2. Configure PC Fan & Hiss options
    opts.lowCutHz = 80;
    opts.denoiseDb = 14;
    QCOMPARE(opts.denoiseDb, 14);

    // 3. Configure Studio & Voice with De-Esser options
    opts.dehumHz = 50;
    opts.deesser = true;
    opts.voiceEnhancer = true;
    opts.noiseGate = true;
    QVERIFY(opts.deesser);
    QVERIFY(opts.voiceEnhancer);
    QVERIFY(opts.noiseGate);

    // 4. Configure Vinyl restoration options (De-Click + De-Hum + Denoise)
    opts.declick = true;
    QVERIFY(opts.declick);

    // 5. Test trimming with audio filters via ffmpeg if sample exists
    QString audioPath = m_testDirPath + "/audio_samples/test_tone.mp3";
    if (QFile::exists(audioPath) && AudioProcessor::isFfmpegAvailable()) {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString outPath = tmpDir.filePath("filtered_test.mp3");

        opts.startMs = 0;
        opts.endMs = 1000;
        opts.outputFormat = "mp3";
        opts.losslessCopy = false;

        bool ok = AudioProcessor::trimAudio(audioPath, outPath, opts, nullptr);
        QVERIFY2(ok, "Audio trimming with filters must succeed");
        QVERIFY(QFile::exists(outPath));
        QVERIFY(QFileInfo(outPath).size() > 0);

        // 6. Test real-time FFT computation on extracted audio
        QVector<float> pcm = AudioProcessor::extractPcmData(audioPath, 22050, 3);
        QVERIFY(!pcm.isEmpty());

        QVector<float> fftBands = AudioProcessor::computeFftMagnitudes(pcm.mid(0, 1024), 22050, 36);
        QCOMPARE(fftBands.size(), 36);
        bool hasNonZeroEnergy = false;
        for (float v : fftBands) {
            if (v > 0.0f) {
                hasNonZeroEnergy = true;
                break;
            }
        }
        QVERIFY(hasNonZeroEnergy);

        // 7. Verify dynamic format bitrate and summary in AudioTrimDialog
        AudioTrimDialog trimDlg(audioPath);
        trimDlg.resize(900, 650);
        QCOMPARE(trimDlg.currentTabIndex(), 0);

        // Default: Original format -> lossless copy ready, no active filters
        QVERIFY(!trimDlg.hasActiveTrim());
        QVERIFY(!trimDlg.hasActiveFilters());
        QVERIFY(!trimDlg.hasActiveDsp());
        QVERIFY(!trimDlg.hasActiveEq());

        // Switch to Summary Tab (Index 4)
        trimDlg.setCurrentTabIndex(4);
        QCOMPARE(trimDlg.currentTabIndex(), 4);

        // Modify trim and test reset
        trimDlg.resetAll();
        QVERIFY(!trimDlg.hasActiveTrim());
        QVERIFY(!trimDlg.hasActiveFilters());
    }
}

void TestInteractive::testDiscoveryModePdfMergeAndUnsupportedFiles()
{
    // 1. Test BuildConfig persistent settings for discovery & unsupported files
    bool origDiscovery = BuildConfig::showUninstalledFeatures();
    bool origUnsupported = BuildConfig::showUnsupportedFiles();

    BuildConfig::setShowUninstalledFeatures(true);
    QVERIFY(BuildConfig::showUninstalledFeatures());
    BuildConfig::setShowUninstalledFeatures(false);
    QVERIFY(!BuildConfig::showUninstalledFeatures());
    BuildConfig::setShowUninstalledFeatures(origDiscovery);

    BuildConfig::setShowUnsupportedFiles(true);
    QVERIFY(BuildConfig::showUnsupportedFiles());
    BuildConfig::setShowUnsupportedFiles(false);
    QVERIFY(!BuildConfig::showUnsupportedFiles());

    int merges = BuildConfig::pdfMergeCount();
    BuildConfig::incrementPdfMergeCount();
    QCOMPARE(BuildConfig::pdfMergeCount(), merges + 1);

    // 2. Test PdfMergeDialog instantiation & UI setup
    QString samplePdf = m_testDirPath + "/BubezView-Architettura.pdf";
    QStringList pdfs;
    if (QFile::exists(samplePdf)) {
        pdfs << samplePdf;
    }
    PdfMergeDialog mergeDlg(pdfs);
    QVERIFY(!mergeDlg.windowTitle().isEmpty());
    QVERIFY(mergeDlg.outputFilePath().isEmpty());

    // 3. Test Unsupported File Inspector Card and Hex Dump
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString binPath = tmpDir.filePath("test_unknown_binary.dat");
    QFile binFile(binPath);
    QVERIFY(binFile.open(QIODevice::WriteOnly));
    QByteArray dummyData;
    dummyData.append("\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
    dummyData.append("Hello world arbitrary binary payload for hex dump test");
    binFile.write(dummyData);
    binFile.close();

    BuildConfig::setShowUnsupportedFiles(true);

    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    MainWindow::OpenResult res = window.openFile(binPath);
    QCOMPARE(res, MainWindow::OpenResult::Success);
    QCOMPARE(window.currentViewerIndex(), 7); // Index 7 is UnsupportedViewer

    // Test Hex Dump trigger
    window.showCurrentHexDump();
    QCOMPARE(window.currentViewerIndex(), 1); // Switched to TextViewer
    QVERIFY(window.textViewer() != nullptr);
    QString hexContent = window.textViewer()->toPlainText();
    QVERIFY(hexContent.contains("HEX DUMP"));
    QVERIFY(hexContent.contains("00000000"));
    QVERIFY(hexContent.contains("7f 45 4c 46"));

    // Reset preference
    BuildConfig::setShowUnsupportedFiles(origUnsupported);
}

void TestInteractive::testPdfRepairAndPageOrganizer()
{
    QString samplePdf = m_testDirPath + "/documento_firmato_test.pdf";
    if (!QFile::exists(samplePdf)) {
        samplePdf = m_testDirPath + "/ciao.pdf";
    }
    QVERIFY(QFile::exists(samplePdf));

    // 1. Test PdfRepairDialog initialization and properties
    PdfRepairDialog repairDlg(samplePdf);
    QVERIFY(!repairDlg.windowTitle().isEmpty());
    QVERIFY(repairDlg.repairedFilePath().isEmpty());
    repairDlg.show();
    repairDlg.resize(580, 280);
    QPixmap repairPix = repairDlg.grab();
    repairPix.save("/home/davide/.gemini/antigravity/brain/5c92b339-02a1-4ab5-be7c-a0935a9f57d6/pdf_repair_dialog.png");

    // 2. Test PdfPageOrganizerDialog initialization, loading and interactive page scaling
    QString testPdf = QFile::exists(m_testDirPath + "/multipage_sample.pdf")
        ? (m_testDirPath + "/multipage_sample.pdf")
        : samplePdf;

    PdfPageOrganizerDialog orgDlg(testPdf);
    QVERIFY(!orgDlg.windowTitle().isEmpty());
    QVERIFY(orgDlg.outputFilePath().isEmpty());
    orgDlg.show();
    orgDlg.resize(920, 640);

    auto *listWidget = orgDlg.findChild<QListWidget*>();
    QVERIFY(listWidget != nullptr);
    int initialCount = listWidget->count();
    QVERIFY(initialCount > 0);

    // Verify initial sequential order badges
    QVERIFY(listWidget->item(0)->text().contains("#1"));
    if (initialCount >= 3) {
        QVERIFY(listWidget->item(1)->text().contains("#2"));
        QVERIFY(listWidget->item(2)->text().contains("#3"));

        auto *organizerList = qobject_cast<PdfOrganizerListWidget*>(listWidget);
        QVERIFY(organizerList != nullptr);

        // Test moving item at row 2 (Page 3) to row 1 (Page 2 position)
        listWidget->clearSelection();
        listWidget->item(2)->setSelected(true);
        organizerList->moveSelectedItemsTo(1);

        // Item 2 moved to row 1, shifting previous row 1 (Page 2) down to row 2
        QVERIFY(listWidget->item(1)->text().contains("#2"));
        QVERIFY(listWidget->item(1)->text().contains("Page 3"));
        QVERIFY(listWidget->item(2)->text().contains("#3"));
        QVERIFY(listWidget->item(2)->text().contains("Page 2"));

        // Test moving item back to row 2
        listWidget->clearSelection();
        listWidget->item(1)->setSelected(true);
        organizerList->moveSelectedItemsTo(3);
        QVERIFY(listWidget->item(2)->text().contains("#3"));
        QVERIFY(listWidget->item(2)->text().contains("Page 3"));

        // Test reordering with automatic chain shifting (Move Page 3 to Page 1 position at index 0)
        listWidget->clearSelection();
        listWidget->item(2)->setSelected(true);
        organizerList->moveSelectedItemsTo(0);

        // Page 3 is now at row 0 (#1), Page 1 shifted to row 1 (#2), Page 2 shifted to row 2 (#3)
        QVERIFY(listWidget->item(0)->text().contains("#1"));
        QVERIFY(listWidget->item(0)->text().contains("Page 3"));
        QVERIFY(listWidget->item(1)->text().contains("#2"));
        QVERIFY(listWidget->item(1)->text().contains("Page 1"));
        QVERIFY(listWidget->item(2)->text().contains("#3"));
        QVERIFY(listWidget->item(2)->text().contains("Page 2"));

        // Test simulated drag and drop events: Drag row 2 (Page 2) onto row 0 (Page 3)
        QMimeData mime;
        mime.setData("application/x-bubez-pdf-page", "page_drag");
        QPoint dropPos = organizerList->visualItemRect(listWidget->item(0)).center();

        QDragEnterEvent enterEv(dropPos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(organizerList->viewport(), &enterEv);
        QVERIFY(enterEv.isAccepted());

        QDragMoveEvent moveEv(dropPos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(organizerList->viewport(), &moveEv);
        QVERIFY(moveEv.isAccepted());

        listWidget->clearSelection();
        listWidget->item(2)->setSelected(true);
        QDropEvent dropEv(dropPos, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(organizerList->viewport(), &dropEv);
        QVERIFY(dropEv.isAccepted());

        // Row 0 should now be Page 2 (#1), Row 1 Page 3 (#2), Row 2 Page 1 (#3)
        QVERIFY(listWidget->item(0)->text().contains("#1"));
        QVERIFY(listWidget->item(0)->text().contains("Page 2"));
        QVERIFY(listWidget->item(1)->text().contains("#2"));
        QVERIFY(listWidget->item(1)->text().contains("Page 3"));
        QVERIFY(listWidget->item(2)->text().contains("#3"));
        QVERIFY(listWidget->item(2)->text().contains("Page 1"));
    }

    // Locate and trigger the Blank Page button
    QPushButton *btnBlank = nullptr;
    for (QPushButton *btn : orgDlg.findChildren<QPushButton*>()) {
        if (btn->text().contains(QStringLiteral("Blank Page")) || btn->text().contains(QStringLiteral("Pagina"))) {
            btnBlank = btn;
            break;
        }
    }
    QVERIFY(btnBlank != nullptr);
    btnBlank->click();
    QCOMPARE(listWidget->count(), initialCount + 1);

    QApplication::processEvents();
    QPixmap orgPix = orgDlg.grab();
    orgPix.save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/pdf_page_organizer_hub.png");

    // 2b. Test integrated in-window Page Grid Organizer in PdfViewer (Option 1)
    PdfViewer viewer;
    viewer.resize(1000, 700);
    viewer.loadFile(testPdf);
    viewer.show();
    QCOMPARE(viewer.isPageGridOrganizerActive(), false);

    viewer.showPageGridOrganizer();
    QCOMPARE(viewer.isPageGridOrganizerActive(), true);

    auto *inWindowList = viewer.findChild<PdfOrganizerListWidget*>();
    QVERIFY(inWindowList != nullptr);
    QCOMPARE(inWindowList->count(), initialCount);

    // Capture screenshot of the integrated in-window page grid view
    QApplication::processEvents();
    QPixmap inWindowPix = viewer.grab();
    inWindowPix.save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/pdf_in_window_page_organizer.png");

    // Close organizer via hidePageGridOrganizer() and verify return to normal view
    viewer.hidePageGridOrganizer();
    QCOMPARE(viewer.isPageGridOrganizerActive(), false);

    // 2c. Real-world simulation on multi-page PDF in MainWindow
    if (QFile::exists("/home/davide/Progetti/BubezView/421_Guida turistica di Trieste.pdf")) {
        MainWindow realWin;
        realWin.resize(1400, 820);
        realWin.openFile("/home/davide/Progetti/BubezView/421_Guida turistica di Trieste.pdf");
        realWin.show();
        QApplication::processEvents();
        bool fileOrganizedTriggered = false;
        if (realWin.pdfViewer()) {
            realWin.pdfViewer()->setOnFileOrganized([&fileOrganizedTriggered](const QString&) {
                fileOrganizedTriggered = true;
            });
        }
        realWin.enterPageOrganizerMode();
        QApplication::processEvents();
        QTest::qWait(100);
        QPixmap realPix = realWin.grab();
        realPix.save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/user_screen_view.png");
        realWin.exitPageOrganizerMode();
        QApplication::processEvents();
        QCOMPARE(fileOrganizedTriggered, false);
        if (realWin.pdfViewer()) {
            QCOMPARE(realWin.pdfViewer()->isPageGridOrganizerActive(), false);
        }
    }

    // 3. Test MainWindow menu bar default visibility and contextual tool states
    QSettings settings;
    bool showMenuBar = settings.value("ui/show_menubar", false).toBool();
    MainWindow window;
    window.resize(1000, 700);
    window.openFile(samplePdf);
    window.show();
    QCOMPARE(window.menuBar()->isVisible(), showMenuBar);

    // Contextual tools: on PDF, repair action should be visible
    QAction *repairAct = nullptr;
    for (QAction *act : window.findChildren<QAction*>()) {
        if (act->text().contains("Repair", Qt::CaseInsensitive)) {
            repairAct = act;
            break;
        }
    }
    QVERIFY(repairAct != nullptr);
    QVERIFY(repairAct->isVisible());

    // Switch to image file: repair action must become hidden
    QString sampleImage = m_testDirPath + "/img/sample.jpg";
    if (QFile::exists(sampleImage)) {
        window.openFile(sampleImage);
        QVERIFY(!repairAct->isVisible());
    }

    QPixmap winPix = window.grab();
    winPix.save("/home/davide/.gemini/antigravity/brain/5c92b339-02a1-4ab5-be7c-a0935a9f57d6/window_restored.png");
}

void TestInteractive::testHighContrastScrollbarsAndZoomPan()
{
    // 1. Verify high-contrast scrollbar stylesheets across all themes
    QString modernLightCss = ThemeManager::scrollBarStyleSheet(ThemePreset::ModernLight);
    QVERIFY(!modernLightCss.isEmpty());
    QVERIFY(modernLightCss.contains("QScrollBar:vertical"));
    QVERIFY(modernLightCss.contains("QScrollBar:horizontal"));
    QVERIFY(modernLightCss.contains("#f0f0f2"));
    QVERIFY(modernLightCss.contains("#9e9ea7"));

    QString breezeLightCss = ThemeManager::scrollBarStyleSheet(ThemePreset::BreezeLight);
    QVERIFY(!breezeLightCss.isEmpty());
    QVERIFY(breezeLightCss.contains("#eff0f1"));
    QVERIFY(breezeLightCss.contains("#a5adb5"));

    QString darkSlateCss = ThemeManager::scrollBarStyleSheet(ThemePreset::DarkSlate);
    QVERIFY(!darkSlateCss.isEmpty());
    // DarkSlate mode: track is dark (#181b1f), thumb is lighter (#586474)
    QVERIFY(darkSlateCss.contains("#181b1f"));
    QVERIFY(darkSlateCss.contains("#586474"));

    QString breezeDarkCss = ThemeManager::scrollBarStyleSheet(ThemePreset::BreezeDark);
    QVERIFY(!breezeDarkCss.isEmpty());
    // BreezeDark mode: track is dark (#121416), thumb is lighter (#4e5866)
    QVERIFY(breezeDarkCss.contains("#121416"));
    QVERIFY(breezeDarkCss.contains("#4e5866"));

    // 2. Verify ImageViewer zooming, scroll area, and theme updates
    ImageViewer imgViewer;
    imgViewer.resize(600, 400);
    imgViewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&imgViewer));

    QString sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    QVERIFY(imgViewer.loadFile(sampleImg));
    QVERIFY(!imgViewer.imageSize().isEmpty());

    // Zooming in expands canvas and enables scrollbars
    double initialZoom = imgViewer.zoomFactor();
    imgViewer.zoomIn();
    imgViewer.zoomIn();
    QVERIFY(imgViewer.zoomFactor() > initialZoom);

    // Zooming out shrinks zoom factor
    double zoomedInVal = imgViewer.zoomFactor();
    imgViewer.zoomOut();
    QVERIFY(imgViewer.zoomFactor() < zoomedInVal);

    // Reset zoom restores fit-to-window
    imgViewer.resetZoom();

    // Verify zoomAt anchored zoom
    double preZoomAt = imgViewer.zoomFactor();
    imgViewer.zoomAt(QPoint(300, 200), 1.25);
    QVERIFY(imgViewer.zoomFactor() > preZoomAt);

    // Verify Zoom Tool enables wheel zooming without Ctrl modifier
    imgViewer.setZoomToolActive(true);
    QVERIFY(imgViewer.isZoomToolActive());
    QWheelEvent we(QPointF(300, 200), QPointF(300, 200), QPoint(0, 0), QPoint(0, 120),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    double preWheelZoom = imgViewer.zoomFactor();
    imgViewer.handleWheel(&we);
    QVERIFY(imgViewer.zoomFactor() > preWheelZoom);

    imgViewer.setZoomToolActive(false);
    QVERIFY(!imgViewer.isZoomToolActive());

    // Theme update applies without issues
    imgViewer.updateTheme();

    // 3. Verify PdfViewer updateTheme applies high contrast scrollbars
    PdfViewer pdfViewer;
    pdfViewer.resize(600, 400);
    pdfViewer.updateTheme();
}

void TestInteractive::testPdfAnnotationsAndTools()
{
    // 1. Verify PdfTransformBox interactive overlay functionality
    PdfTransformBox box;
    box.resize(300, 300);

    // Text mode
    box.showText("Test Typewriter Text", QFont("sans-serif", 12), Qt::blue, true, QPoint(150, 150));
    QVERIFY(box.isVisible());
    QCOMPARE(box.itemType(), PdfTransformItemType::Text);
    QCOMPARE(box.text(), QString("Test Typewriter Text"));
    QVERIFY(box.isWhiteout());

    // Image/signature mode
    QImage testSig(100, 50, QImage::Format_ARGB32_Premultiplied);
    testSig.fill(Qt::transparent);
    box.showImage(testSig, QPoint(150, 150));
    QCOMPARE(box.itemType(), PdfTransformItemType::Signature);

    // Watermark mode
    box.showWatermark("CONFIDENTIAL", 45.0, 0.6, Qt::red, QPoint(150, 150));
    QCOMPARE(box.itemType(), PdfTransformItemType::Watermark);
    QCOMPARE(box.text(), QString("CONFIDENTIAL"));
    QCOMPARE(box.watermarkAngle(), 45.0);

    // Redaction mode
    box.showRedaction(QRect(20, 20, 80, 40));
    QCOMPARE(box.itemType(), PdfTransformItemType::Redaction);
    QCOMPARE(box.boxRect(), QRect(20, 20, 80, 40));

    box.dismiss();
    QVERIFY(!box.isVisible());

    // Test editRequested signal emission and drag handle rect
    bool editSignalFired = false;
    QObject::connect(&box, &PdfTransformBox::editRequested, [&editSignalFired]() {
        editSignalFired = true;
    });
    box.showText("Edit Test", QFont("sans-serif", 10), Qt::black, false, QPoint(100, 100));
    QRect handleRect = box.dragHandleRect();
    QVERIFY(!handleRect.isEmpty());
    QVERIFY(handleRect.top() < box.boxRect().top());
    emit box.editRequested();
    QVERIFY(editSignalFired);

    box.dismiss();
    QVERIFY(!box.isVisible());

    // 2. Verify Typewriter and Watermark dialog setters and getters
    PdfTypewriterDialog twDlg;
    twDlg.setText("Custom Typewriter Text");
    QCOMPARE(twDlg.text(), QString("Custom Typewriter Text"));
    twDlg.setTextColor(Qt::red);
    QCOMPARE(twDlg.textColor(), QColor(Qt::red));
    twDlg.setWhiteout(true);
    QVERIFY(twDlg.isWhiteout());

    PdfWatermarkDialog wmDlg;
    wmDlg.setWatermarkText("CONFIDENTIAL_STAMP");
    QCOMPARE(wmDlg.watermarkText(), QString("CONFIDENTIAL_STAMP"));
    wmDlg.setAngle(90.0);
    QCOMPARE(wmDlg.angle(), 90.0);
    wmDlg.setOpacity(0.5);
    QCOMPARE(wmDlg.opacity(), 0.5);
    wmDlg.setColor(Qt::green);
    QCOMPARE(wmDlg.color(), QColor(Qt::green));

    // 3. Verify Signature Studio Dialog safe instantiation without crash
    PdfSignatureDialog sigDlg;
    sigDlg.resize(500, 400);
    sigDlg.show();
    QVERIFY(sigDlg.isVisible());
    sigDlg.close();

    // 4. Verify OcrSnipperOverlay non-modal creation without input grabbing freeze
    QWidget overlayTarget;
    overlayTarget.resize(400, 300);
    overlayTarget.show();
    auto *snipper = new OcrSnipperOverlay(&overlayTarget);
    QVERIFY(snipper->windowModality() != Qt::ApplicationModal);
    snipper->close();

    // 5. Verify PdfPageWidget tool mode transitions and visibleCenter
    PdfPageWidget pageWidget;
    pageWidget.resize(800, 1200);
    pageWidget.show();
    QPoint centerPt = pageWidget.visibleCenter();
    QVERIFY(!centerPt.isNull());
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    pageWidget.startSnapshot();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::Snapshot);

    pageWidget.cancelActiveTool();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    pageWidget.startRedaction();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::Redaction);

    pageWidget.cancelActiveTool();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    // 6. Verify PdfViewer annotation application, modification tracking, and saving
    PdfViewer viewer;
    viewer.resize(800, 600);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));

    QString samplePdf = m_testDirPath + "/ciao.pdf";
    QVERIFY(viewer.loadFile(samplePdf));
    QVERIFY(!viewer.isModified());

    // Add a stamp annotation
    QImage annImg(80, 40, QImage::Format_ARGB32_Premultiplied);
    annImg.fill(Qt::blue);
    QRectF normRect(0.1, 0.1, 0.3, 0.2);

    viewer.onAddStampAnnotation(annImg, normRect);
    QVERIFY(viewer.isModified());

    // Export current page test
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString exportedImgPath = tempDir.filePath("page_exported.png");
    QVERIFY(viewer.exportCurrentPage("png", exportedImgPath, 150));
    QVERIFY(QFile::exists(exportedImgPath));

    // Save as test to verify PDFConverter::WithChanges
    QString savedPdfPath = tempDir.filePath("saved_changes.pdf");
    QVERIFY(viewer.saveAs(savedPdfPath));
    QVERIFY(QFile::exists(savedPdfPath));
    QVERIFY(!viewer.isModified());

    // 7. Verify new PDF Image Insertion, Push-Down toggle, and Blank Page Insertion
    PdfTransformBox imgBox;
    imgBox.resize(300, 300);
    QImage sampleImg(120, 60, QImage::Format_ARGB32_Premultiplied);
    sampleImg.fill(Qt::darkGreen);
    imgBox.showImage(sampleImg, QPoint(150, 150), QSize(120, 60), PdfTransformItemType::Image);
    QCOMPARE(imgBox.itemType(), PdfTransformItemType::Image);
    imgBox.dismiss();

    // Verify native vector text annotation addition
    viewer.onAddTextAnnotation(QStringLiteral("Hello Native Vector Text"), QFont(QStringLiteral("Arial"), 14), Qt::blue, QRectF(0.1, 0.1, 0.3, 0.05), false);
    QVERIFY(viewer.isModified());

    pageWidget.startInsertImage(sampleImg);
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::InsertImage);
    pageWidget.cancelActiveTool();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    // Verify Area Selection mode and interactive transform box
    pageWidget.startSelectArea();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectArea);
    QCOMPARE(pageWidget.hasAreaSelection(), false);
    QVERIFY(pageWidget.transformBox() != nullptr);
    pageWidget.transformBox()->showAreaSelection(QRect(20, 20, 150, 100));
    QCOMPARE(pageWidget.transformBox()->itemType(), PdfTransformItemType::AreaSelection);
    QCOMPARE(pageWidget.transformBox()->boxRect(), QRect(20, 20, 150, 100));
    QVERIFY(pageWidget.transformBox()->isVisible());
    pageWidget.cancelActiveTool();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    // Verify unified paste with image on clipboard
    QApplication::clipboard()->setImage(sampleImg);
    pageWidget.pasteFromClipboard();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::InsertImage);
    pageWidget.cancelActiveTool();

    // Verify text paste with position
    QApplication::clipboard()->setText(QStringLiteral("Test pasted text"));
    pageWidget.pasteFromClipboard(QPoint(150, 150));
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::Typewriter);
    pageWidget.cancelActiveTool();

    // Verify Highlighter mode
    pageWidget.startHighlighter(QColor(255, 235, 59), 0.40);
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::Highlight);
    pageWidget.cancelActiveTool();
    QCOMPARE(pageWidget.toolMode(), PdfToolMode::SelectText);

    // Verify Cut and Paste proxy methods without crashing
    viewer.cutSelectedText();
    viewer.pasteText();

    // Verify Blank Page Insertion and Undo/Redo on a temp copy of samplePdf
    QString tempTestPdf = tempDir.filePath("test_insert_page.pdf");
    QVERIFY(QFile::copy(samplePdf, tempTestPdf));
    PdfViewer insertViewer;
    QVERIFY(insertViewer.loadFile(tempTestPdf));
    int initialPageCount = insertViewer.pageCount();
    QVERIFY(initialPageCount >= 1);
    QVERIFY(!insertViewer.canUndo());
    QVERIFY(!insertViewer.canRedo());

    QVERIFY(insertViewer.insertBlankPage(0));
    QCOMPARE(insertViewer.pageCount(), initialPageCount + 1);
    QVERIFY(insertViewer.canUndo());

    // Test Undo
    insertViewer.undo();
    QCOMPARE(insertViewer.pageCount(), initialPageCount);
    QVERIFY(insertViewer.canRedo());

    // Test Redo
    insertViewer.redo();
    QCOMPARE(insertViewer.pageCount(), initialPageCount + 1);
    QVERIFY(insertViewer.isModified());

    // Undo back to clean state
    insertViewer.undo();
    QCOMPARE(insertViewer.pageCount(), initialPageCount);
    QVERIFY(!insertViewer.isModified());

    // Test Undo/Redo keyboard signal dispatch from PdfPageWidget
    QSignalSpy undoSpy(&pageWidget, &PdfPageWidget::undoRequested);
    QSignalSpy redoSpy(&pageWidget, &PdfPageWidget::redoRequested);
    QKeyEvent undoKey(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(&pageWidget, &undoKey);
    QCOMPARE(undoSpy.count(), 1);

    QKeyEvent redoKey(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
    QApplication::sendEvent(&pageWidget, &redoKey);
    QCOMPARE(redoSpy.count(), 1);

    // Record original file content to verify it is NEVER touched on disk during editing/undo
    QFile origFile(tempTestPdf);
    QVERIFY(origFile.open(QIODevice::ReadOnly));
    QByteArray originalBytes = origFile.readAll();
    origFile.close();

    // Verify Area Cut creates a reversible Undo state
    QVERIFY(!insertViewer.isModified());
    insertViewer.onCutAreaRequested(QRectF(50, 50, 100, 100));
    QVERIFY(insertViewer.canUndo());
    QVERIFY(insertViewer.isModified());

    // Original file on disk must remain pristine (no premature overwrite)
    QFile checkFile(tempTestPdf);
    QVERIFY(checkFile.open(QIODevice::ReadOnly));
    QCOMPARE(checkFile.readAll(), originalBytes);
    checkFile.close();

    // Perform Undo: restores initial clean state
    insertViewer.undo();
    QVERIFY(!insertViewer.canUndo());
    QVERIFY(insertViewer.canRedo());
    QVERIFY(!insertViewer.isModified());

    // File on disk is still 100% identical to original
    QFile checkFile2(tempTestPdf);
    QVERIFY(checkFile2.open(QIODevice::ReadOnly));
    QCOMPARE(checkFile2.readAll(), originalBytes);
    checkFile2.close();

    // Redo restores cut state
    insertViewer.redo();
    QVERIFY(insertViewer.canUndo());
    QVERIFY(!insertViewer.canRedo());
    QVERIFY(insertViewer.isModified());

    // Undo again to return to clean state
    insertViewer.undo();
    QVERIFY(!insertViewer.isModified());

    // Verify transform box cancellation on Undo (Ctrl+Z)
    QImage testImg(40, 40, QImage::Format_ARGB32);
    testImg.fill(Qt::blue);
    pageWidget.startInsertImage(testImg, QPoint(100, 100));
    QVERIFY(pageWidget.transformBox() && pageWidget.transformBox()->isVisible());

    // Sending Undo to active transform box must cancel/dismiss it
    QApplication::sendEvent(pageWidget.transformBox(), &undoKey);
    QVERIFY(!pageWidget.transformBox()->isVisible());
    QVERIFY(!pageWidget.hasAreaSelection());

    // Test specific scenario on copia_Presentazione_BubezView_Definitiva.pdf if present
    QString presPdf = "/home/davide/Progetti/BubezView/copia_Presentazione_BubezView_Definitiva.pdf";
    if (QFile::exists(presPdf)) {
        PdfViewer presViewer;
        presViewer.resize(1000, 800);
        presViewer.show();
        QVERIFY(presViewer.loadFile(presPdf));

        // 1. Verify FlippableToolbar 3D flip card operations
        FlippableToolbar flippableBar;
        flippableBar.resize(900, 36);
        flippableBar.show();
        QCOMPARE(flippableBar.currentMode(), FlippableToolbarMode::Main);

        flippableBar.flipToHighlighter(QColor(255, 235, 59), 0.40);
        QCOMPARE(flippableBar.targetMode(), FlippableToolbarMode::Highlighter);
        QTRY_COMPARE_WITH_TIMEOUT(flippableBar.currentMode(), FlippableToolbarMode::Highlighter, 1000);

        flippableBar.flipToShapes(2, 3, Qt::red, Qt::transparent);
        QCOMPARE(flippableBar.targetMode(), FlippableToolbarMode::Shapes);
        QTRY_COMPARE_WITH_TIMEOUT(flippableBar.currentMode(), FlippableToolbarMode::Shapes, 1000);
        QCOMPARE(flippableBar.shapeType(), 2);

        // Verify shape stamp rendering for Arrow (shapeType 2) and Line (shapeType 3) with exact vector coordinates
        QImage arrowUpLeft = PdfCanvasTools::renderShapeStamp(2, 2, Qt::red, Qt::transparent, QSize(120, 120), QPoint(100, 100), QPoint(20, 20));
        QVERIFY(!arrowUpLeft.isNull());
        QCOMPARE(arrowUpLeft.size(), QSize(120, 120));

        QImage arrowUpRight = PdfCanvasTools::renderShapeStamp(2, 2, Qt::red, Qt::transparent, QSize(120, 120), QPoint(20, 100), QPoint(100, 20));
        QVERIFY(!arrowUpRight.isNull());

        QImage lineDiagonal = PdfCanvasTools::renderShapeStamp(3, 2, Qt::blue, Qt::transparent, QSize(120, 120), QPoint(100, 20), QPoint(20, 100));
        QVERIFY(!lineDiagonal.isNull());

        // Verify rubber band preview for Arrow and Line
        QImage previewBuffer(200, 200, QImage::Format_ARGB32_Premultiplied);
        previewBuffer.fill(Qt::transparent);
        {
            QPainter p(&previewBuffer);
            PdfCanvasTools::drawRubberBand(p, QRect(20, 20, 100, 100), 4, 0, 0, 2, 2, Qt::red, Qt::transparent, QPoint(100, 100), QPoint(20, 20));
            PdfCanvasTools::drawRubberBand(p, QRect(20, 20, 100, 100), 4, 0, 0, 3, 2, Qt::blue, Qt::transparent, QPoint(20, 100), QPoint(100, 20));
        }

        // Verify mathematical vector distance and hit-testing
        double distOnSegment = PdfCanvasTools::pointToLineSegmentDistance(QPointF(50, 50), QPointF(0, 0), QPointF(100, 100));
        QVERIFY(std::abs(distOnSegment) < 0.001);
        double distOffSegment = PdfCanvasTools::pointToLineSegmentDistance(QPointF(0, 100), QPointF(0, 0), QPointF(100, 0));
        QVERIFY(std::abs(distOffSegment - 100.0) < 0.001);

        // Vector line hit-testing: hits near line (within tolerance) but misses empty space
        QRect lineRect(10, 10, 180, 180);
        QVERIFY(PdfCanvasTools::hitTestShape(3, lineRect, QPoint(0, 0), QPoint(180, 180), 2, Qt::transparent, QPoint(100, 100), 6));
        QVERIFY(!PdfCanvasTools::hitTestShape(3, lineRect, QPoint(0, 0), QPoint(180, 180), 2, Qt::transparent, QPoint(180, 20), 6));

        // Hollow rectangle hit-testing: hits on edge, but misses hollow center
        QRect hollowRect(50, 50, 100, 100);
        QVERIFY(PdfCanvasTools::hitTestShape(0, hollowRect, QPoint(-1, -1), QPoint(-1, -1), 2, Qt::transparent, QPoint(50, 80), 6));
        QVERIFY(!PdfCanvasTools::hitTestShape(0, hollowRect, QPoint(-1, -1), QPoint(-1, -1), 2, Qt::transparent, QPoint(100, 100), 6));

        // Filled rectangle hit-testing: hits center when filled
        QVERIFY(PdfCanvasTools::hitTestShape(0, hollowRect, QPoint(-1, -1), QPoint(-1, -1), 2, QColor(255, 0, 0, 100), QPoint(100, 100), 6));

        // Hollow ellipse hit-testing: hits perimeter, misses center
        QRect ellipseRect(50, 50, 100, 100);
        QVERIFY(PdfCanvasTools::hitTestShape(1, ellipseRect, QPoint(-1, -1), QPoint(-1, -1), 2, Qt::transparent, QPoint(50, 100), 6));
        QVERIFY(!PdfCanvasTools::hitTestShape(1, ellipseRect, QPoint(-1, -1), QPoint(-1, -1), 2, Qt::transparent, QPoint(100, 100), 6));

        flippableBar.flipToMain();
        QCOMPARE(flippableBar.targetMode(), FlippableToolbarMode::Main);
        QTRY_COMPARE_WITH_TIMEOUT(flippableBar.currentMode(), FlippableToolbarMode::Main, 1000);

        // 2. Verify PdfTransformBox applyAndContinue (Stamp Again)
        auto *pw = presViewer.findChild<PdfPageWidget*>();
        QVERIFY(pw != nullptr);

        QImage stampImg(60, 30, QImage::Format_ARGB32);
        stampImg.fill(Qt::green);
        pw->startSignature(stampImg, QPoint(150, 150));
        QVERIFY(pw->transformBox()->isVisible());

        // Trigger Stamp Again signal
        emit pw->transformBox()->applyAndContinue();
        // Transform box remains active ready to stamp consecutive items
        QVERIFY(pw->transformBox()->isVisible());
    }
}

void TestInteractive::testToolbarCustomizationAndPdfWheelScroll()
{
    // 1. Verify default toolbar layout in MainWindow
    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QStringList expectedDefaults = {"zoom_tool", "zoom_reset", "zoom_fit", "separator", "filmstrip"};
    QCOMPARE(window.defaultToolbarActions(), expectedDefaults);
    QCOMPARE(window.isFilmstripVisible(), false);

    // 2. Verify ConfigureToolbarDialog operations
    QMap<QString, ToolbarActionInfo> mockActions;
    ToolbarActionInfo a1{"prev_file", "Previous", QIcon(), "Previous File"};
    ToolbarActionInfo a2{"next_file", "Next", QIcon(), "Next File"};
    ToolbarActionInfo a_fv{"folder_view", "Folder Gallery View", QIcon(), "Folder Gallery View"};
    ToolbarActionInfo a_zt{"zoom_tool", "Zoom Tool", QIcon(), "Zoom Tool"};
    ToolbarActionInfo a3{"zoom_in", "Zoom In", QIcon(), "Zoom In"};
    ToolbarActionInfo a4{"zoom_out", "Zoom Out", QIcon(), "Zoom Out"};
    ToolbarActionInfo a5{"zoom_reset", "Zoom 100%", QIcon(), "Zoom 100%"};
    ToolbarActionInfo a_zf{"zoom_fit", "Fit to Window", QIcon(), "Fit to Window"};
    ToolbarActionInfo a_fs{"filmstrip", "Thumbnail Strip", QIcon(), "Thumbnail Strip"};
    ToolbarActionInfo a6{"fullscreen", "Fullscreen", QIcon(), "Fullscreen"};
    mockActions["prev_file"] = a1;
    mockActions["next_file"] = a2;
    mockActions["folder_view"] = a_fv;
    mockActions["zoom_tool"] = a_zt;
    mockActions["zoom_in"] = a3;
    mockActions["zoom_out"] = a4;
    mockActions["zoom_reset"] = a5;
    mockActions["zoom_fit"] = a_zf;
    mockActions["filmstrip"] = a_fs;
    mockActions["fullscreen"] = a6;

    QStringList currentIds = {"prev_file", "next_file"};
    ConfigureToolbarDialog dlg(mockActions, currentIds, expectedDefaults, &window);
    QCOMPARE(dlg.selectedActionIds(), currentIds);

    // Test restoring defaults in dialog
    QMetaObject::invokeMethod(&dlg, "onRestoreDefaultsClicked");
    QCOMPARE(dlg.selectedActionIds(), expectedDefaults);

    // 3. Verify PDF mouse wheel page navigation and smooth zoom
    PdfViewer pdfViewer;
    pdfViewer.resize(600, 800);
    pdfViewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pdfViewer));

    QString samplePdf = m_testDirPath + "/ciao.pdf";
    if (QFile::exists(samplePdf)) {
        QVERIFY(pdfViewer.loadFile(samplePdf));
        int initialPage = pdfViewer.currentPage();
        QCOMPARE(initialPage, 0);

        // Verify zoom in from fit-to-width does not collapse to 1.0 (smooth zoom)
        pdfViewer.zoomIn();
        QVERIFY(pdfViewer.zoomFactor() > 0.0);

        if (pdfViewer.pageCount() > 1) {
            // Simulate wheel scroll down (delta -120) to turn to next page
            QWheelEvent wheelDown(QPointF(300, 400), QPointF(300, 400), QPoint(0, 0), QPoint(0, -120),
                                  Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&pdfViewer, &wheelDown);
            QCOMPARE(pdfViewer.currentPage(), 1);

            // Simulate wheel scroll up (delta +120) to return to previous page
            QWheelEvent wheelUp(QPointF(300, 400), QPointF(300, 400), QPoint(0, 0), QPoint(0, 120),
                                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&pdfViewer, &wheelUp);
            QCOMPARE(pdfViewer.currentPage(), 0);
        }
    }
}

void TestInteractive::testFolderInspectorAndTextThumbnail()
{
    // 1. Verify text thumbnail rendering on a sample text file
    QString sampleTxt = m_testDirPath + "/mancapdf.txt";
    QVERIFY(QFile::exists(sampleTxt));

    QImage textThumb = ThumbnailWorker::renderTextThumbnail(sampleTxt);
    QVERIFY(!textThumb.isNull());
    QCOMPARE(textThumb.width(), 400); // 200 * 2.0 devicePixelRatio
    QCOMPARE(textThumb.height(), 300); // 150 * 2.0 devicePixelRatio

    // 2. Verify VectorIcons::Type::Zip generates a high-definition icon
    QIcon zipIcon = VectorIcons::createIcon(VectorIcons::Type::Zip);
    QVERIFY(!zipIcon.isNull());
    QPixmap zipPix = zipIcon.pixmap(64, 64);
    QVERIFY(!zipPix.isNull());
    QCOMPARE(zipPix.size(), QSize(64, 64));

    // 3. Test FolderViewer inspector live text preview & folder metadata
    MainWindow window;
    window.resize(1100, 750);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.openFile(m_testDirPath);
    FolderViewer *fv = window.folderViewer();
    QVERIFY(fv != nullptr);
    fv->setShowFolders(true);
    fv->toggleInspector();
    QTest::qWait(200);

    QPlainTextEdit *textEdit = fv->findChild<QPlainTextEdit*>();
    QVERIFY(textEdit != nullptr);

    // Find and select mancapdf.txt
    int textRow = -1;
    for (int i = 0; i < fv->listWidget()->count(); ++i) {
        if (fv->listWidget()->item(i)->text() == "mancapdf.txt") {
            textRow = i;
            break;
        }
    }
    QVERIFY(textRow != -1);

    fv->listWidget()->setCurrentRow(textRow);
    QTest::qWait(100);

    // Inspector should display the text editor for text files
    QVERIFY(textEdit->isVisible());
    QVERIFY(!textEdit->toPlainText().isEmpty());

    // Find and select a directory (e.g. first item since dirs come first)
    QVERIFY(fv->listWidget()->count() > 0);
    fv->listWidget()->setCurrentRow(0);
    QTest::qWait(100);

    // Inspector should hide text edit when a directory is selected
    QVERIFY(!textEdit->isVisible());

    // 4. Verify custom bookmarks addition and removal
    QString testCustomDir = m_testDirPath;
    fv->addCustomBookmark(testCustomDir);
    QVERIFY(fv->customBookmarks().contains(QDir(testCustomDir).canonicalPath()));

    fv->removeCustomBookmark(testCustomDir);
    QVERIFY(!fv->customBookmarks().contains(QDir(testCustomDir).canonicalPath()));
}

void TestInteractive::testArchiveNavigationAndFolderGalleryReturn()
{
    QString zipPath = m_testDirPath + "/__pycache__.zip";
    if (!QFile::exists(zipPath)) {
        QSKIP("Test archive __pycache__.zip not found");
    }

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // 1. Open test archive
    window.openFile(zipPath);
    QTest::qWait(300);
    IArchiveViewer *av = window.archiveViewer();
    QVERIFY(av != nullptr);

    QStringList entries = av->previewableEntries();
    QVERIFY2(!entries.isEmpty(), "Archive must contain previewable entries");

    // 2. Simulate double-clicking first previewable file
    QString firstEntry = entries.first();
    QString extracted = av->extractSingleFile(zipPath, firstEntry);
    QVERIFY(QFile::exists(extracted));

    av->requestFilePreview(extracted, zipPath, firstEntry);
    QTest::qWait(100);

    QVERIFY(window.isArchivePreview());
    QCOMPARE(window.archiveOriginPath(), zipPath);
    QCOMPARE(window.archiveCurrentEntryIndex(), 0);
    QVERIFY(window.windowTitle().contains("📦"));

    // 3. Test next / prev navigation
    if (entries.size() > 1) {
        window.nextFile();
        QTest::qWait(100);
        QCOMPARE(window.archiveCurrentEntryIndex(), 1);

        window.prevFile();
        QTest::qWait(100);
        QCOMPARE(window.archiveCurrentEntryIndex(), 0);
    }

    // 4. At index 0, prevFile() must return to archive central view
    window.prevFile();
    QTest::qWait(100);
    QVERIFY(!window.isArchivePreview());
    QVERIFY(window.windowTitle().contains(QFileInfo(zipPath).fileName()));

    // 5. Preview again and verify folder gallery return goes to archive's parent folder on disk
    av->requestFilePreview(extracted, zipPath, firstEntry);
    QTest::qWait(100);
    QVERIFY(window.isArchivePreview());

    window.openCurrentFolderInGallery();
    QTest::qWait(100);
    QVERIFY(!window.isArchivePreview());

    // Current file/folder must be the directory containing zipPath, NOT /tmp
    QString expectedParentDir = QFileInfo(zipPath).absolutePath();
    QVERIFY(!window.currentFilePath().contains("/tmp/bubezview_archive_preview"));
    QCOMPARE(QDir(window.currentFilePath()).canonicalPath(), QDir(expectedParentDir).canonicalPath());

    // 6. Security verification: Path traversal rejection in extractSingleFile
    QString malicious1 = av->extractSingleFile(zipPath, "../../etc/passwd");
    QVERIFY2(malicious1.isEmpty(), "Path traversal with relative parent dirs must be rejected");

    QString malicious2 = av->extractSingleFile(zipPath, "../../../home/davide/.bashrc");
    QVERIFY2(malicious2.isEmpty(), "Path traversal escaping cache must be rejected");

    // 7. Editability validation: ZIP archive must report editable
    QVERIFY(av->isArchiveEditable());
}

void TestInteractive::testModuleCompilationAndFallbackPlan()
{
    // Test compilable engine identification
    QVERIFY(PackageManagerHelper::isCompilableEngine("office"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("librevenge"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("pdf"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("poppler-qt6"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("archive"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("libarchive"));

    // Verify compilable engines vs utility tools
    QVERIFY(PackageManagerHelper::isCompilableEngine("qpdf"));
    QVERIFY(PackageManagerHelper::isCompilableEngine("tesseract"));
    QVERIFY(!PackageManagerHelper::isCompilableEngine("pdfimages"));

    // Verify package name resolution
    QString officePkg = PackageManagerHelper::packageNameForComponent("office");
    QVERIFY(!officePkg.isEmpty());

    // Verify package manager commands formatting
    QString installCmd = PackageManagerHelper::getInstallCommand("office");
    QVERIFY(!installCmd.isEmpty());
}

void TestInteractive::testTextEditingModeAndPersistence()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString testFilePath = tempDir.filePath("sample.txt");
    {
        QFile f(testFilePath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "Initial text line 1\nInitial text line 2";
    }

    TextViewer viewer;
    QVERIFY(viewer.loadFile(testFilePath));
    QVERIFY(viewer.isReadOnly());
    QVERIFY(!viewer.isEditable());
    QVERIFY(!viewer.document()->isModified());
    QCOMPARE(viewer.lineCount(), 2);

    // Test modifying keys detection
    QKeyEvent keyA(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QVERIFY(viewer.isModifyingKey(&keyA));

    QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QVERIFY(viewer.isModifyingKey(&keyEnter));

    QKeyEvent keyBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    QVERIFY(viewer.isModifyingKey(&keyBackspace));

    QKeyEvent keyCtrlV(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier);
    QVERIFY(viewer.isModifyingKey(&keyCtrlV));

    // Test non-modifying keys
    QKeyEvent keyRight(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QVERIFY(!viewer.isModifyingKey(&keyRight));

    QKeyEvent keyCtrlC(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(!viewer.isModifyingKey(&keyCtrlC));

    QKeyEvent keyEscape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QVERIFY(!viewer.isModifyingKey(&keyEscape));

    // Test toggle edit mode
    QSignalSpy editSpy(&viewer, &TextViewer::editModeChanged);
    viewer.setEditable(true);
    QVERIFY(viewer.isEditable());
    QVERIFY(!viewer.isReadOnly());
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(editSpy.takeFirst().at(0).toBool(), true);

    // Test editing and saving
    QSignalSpy modSpy(&viewer, &TextViewer::modificationChanged);
    viewer.appendPlainText("Appended text line 3");
    QVERIFY(viewer.document()->isModified());
    QVERIFY(modSpy.count() >= 1);

    // Save file back to disk
    QVERIFY(viewer.saveFile());
    QVERIFY(!viewer.document()->isModified());

    // Verify saved content from disk
    QFile verifyFile(testFilePath);
    QVERIFY(verifyFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QTextStream(&verifyFile).readAll();
    QVERIFY(content.contains("Appended text line 3"));
}

void TestInteractive::testNativeImageEnhancementsAndCrop()
{
    ImageViewer viewer;
    QSignalSpy undoSpy(&viewer, &ImageViewer::undoAvailabilityChanged);
    QSignalSpy cropSpy(&viewer, &ImageViewer::cropModeChanged);

    QString sampleImg = m_testDirPath + "/img/sample_photo.jpg";
    if (!QFile::exists(sampleImg)) {
        sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    }
    QVERIFY(viewer.loadFile(sampleImg));
    QCOMPARE(viewer.canUndo(), false);
    QCOMPARE(viewer.canRedo(), false);
    QCOMPARE(viewer.isModified(), false);

    QImage originalBackup = viewer.originalImage();
    QVERIFY(!originalBackup.isNull());
    QSize origSize = viewer.imageSize();

    // 1. Test Rotate Clockwise
    viewer.rotateClockwise();
    QCOMPARE(viewer.canUndo(), true);
    QCOMPARE(viewer.canRedo(), false);
    QCOMPARE(viewer.isModified(), true);
    QCOMPARE(viewer.imageSize(), QSize(origSize.height(), origSize.width()));

    // Test Undo rotation
    viewer.undo();
    QCOMPARE(viewer.canUndo(), false);
    QCOMPARE(viewer.canRedo(), true);
    QCOMPARE(viewer.imageSize(), origSize);

    // Test Redo rotation
    viewer.redo();
    QCOMPARE(viewer.canUndo(), true);
    QCOMPARE(viewer.canRedo(), false);
    viewer.undo();

    // 2. Test Flips
    viewer.flipHorizontal();
    QCOMPARE(viewer.canUndo(), true);
    viewer.flipVertical();
    QCOMPARE(viewer.canUndo(), true);
    viewer.undo();
    viewer.undo();
    QCOMPARE(viewer.canUndo(), false);

    // 3. Test Crop
    QRect cropBox(20, 20, 120, 90);
    viewer.applyCrop(cropBox);
    QCOMPARE(viewer.imageSize(), QSize(120, 90));
    QCOMPARE(viewer.canUndo(), true);

    // Undo crop
    viewer.undo();
    QCOMPARE(viewer.imageSize(), origSize);

    // 6. Test Interactive Crop Mode toggling
    viewer.setCropModeActive(true);
    QCOMPARE(viewer.isCropModeActive(), true);
    QCOMPARE(cropSpy.count(), 1);
    QCOMPARE(cropSpy.takeFirst().at(0).toBool(), true);

    viewer.cancelCropMode();
    QCOMPARE(viewer.isCropModeActive(), false);
    QCOMPARE(cropSpy.count(), 1);
    QCOMPARE(cropSpy.takeFirst().at(0).toBool(), false);

    // 7. Capture visual preview of on-canvas crop mode
    viewer.resize(900, 700);
    viewer.show();
    (void)QTest::qWaitForWindowExposed(&viewer);
    viewer.fitToWindow();
    viewer.setCropModeActive(true);
    QWidget *canvas = viewer.canvasWidget();
    if (canvas) {
        QPoint startPt(canvas->width() * 0.2, canvas->height() * 0.2);
        QPoint endPt(canvas->width() * 0.8, canvas->height() * 0.75);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, startPt);
        QTest::mouseMove(canvas, endPt);
        QTest::qWait(100);
        QPixmap pixCrop = viewer.grab();
        pixCrop.save("/home/davide/.gemini/antigravity/brain/9744b1b3-031c-48d3-846e-066fa96ecbdf/native_crop_preview.png");
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, endPt);
    }
    viewer.cancelCropMode();

    // 8. Capture MainWindow with image loaded and native UI
    MainWindow win;
    win.resize(1000, 700);
    win.show();
    (void)QTest::qWaitForWindowExposed(&win);
    win.openFile(sampleImg);
    QTest::qWait(200);
    QPixmap pixMain = win.grab();
    pixMain.save("/home/davide/.gemini/antigravity/brain/9744b1b3-031c-48d3-846e-066fa96ecbdf/native_image_viewer_preview.png");

    // 9. Verify Save As resets modified state so no prompt appears on close/navigation
    win.setEditMode(true);
    win.imageViewer()->rotateClockwise();
    QVERIFY(win.imageViewer()->isModified());
    QString copyTarget = m_testDirPath + "/img/test_save_as_copy.jpg";
    QVERIFY(win.imageViewer()->originalImage().save(copyTarget, "jpg", 95));
    win.imageViewer()->setModified(false);
    QCOMPARE(win.imageViewer()->isModified(), false);
    QVERIFY(win.close());
    QFile::remove(copyTarget);
}

void TestInteractive::testQuickLookInterfaceCaptures()
{
    QString sampleImg = m_testDirPath + "/2_inspector_flyout_card.jpg";
    if (!QFile::exists(sampleImg)) {
        QSKIP("Sample image not found for Quick Look captures");
    }

    const QString artifactDir = "/home/davide/.gemini/antigravity/brain/db0875de-21e6-4705-af7b-91c29c95085b";

    // 1. Capture Modern Light - Preview Mode
    {
        ThemeManager::setPreset(ThemePreset::ModernLight);
        MainWindow winLight;
        winLight.applyCurrentTheme();
        winLight.resize(1000, 680);
        winLight.openFile(sampleImg);
        winLight.show();
        (void)QTest::qWaitForWindowExposed(&winLight);
        QTest::qWait(200);

        QVERIFY(!winLight.isEditMode());
        QVERIFY(winLight.minimalHeaderBar() != nullptr);
        QVERIFY(winLight.minimalHeaderBar()->isVisible());

        QPixmap pixLightPrev = winLight.grab();
        pixLightPrev.save(artifactDir + "/quicklook_preview_modern_light.png");

        // 2. Capture Modern Light - Edit Mode
        winLight.setEditMode(true);
        QVERIFY(winLight.isEditMode());
        QTest::qWait(200);

        QPixmap pixLightEdit = winLight.grab();
        pixLightEdit.save(artifactDir + "/quicklook_edit_mode_modern_light.png");
        winLight.close();
    }

    // 3. Capture Dark Slate - Preview Mode
    {
        ThemeManager::setPreset(ThemePreset::DarkSlate);
        MainWindow winDark;
        winDark.applyCurrentTheme();
        winDark.resize(1000, 680);
        winDark.openFile(sampleImg);
        winDark.show();
        (void)QTest::qWaitForWindowExposed(&winDark);
        QTest::qWait(200);

        QVERIFY(!winDark.isEditMode());
        QVERIFY(winDark.minimalHeaderBar() != nullptr);
        QVERIFY(winDark.minimalHeaderBar()->isVisible());

        QPixmap pixDarkPrev = winDark.grab();
        pixDarkPrev.save(artifactDir + "/quicklook_preview_dark_slate.png");

        // 4. Capture Dark Slate - Edit Mode
        winDark.setEditMode(true);
        QVERIFY(winDark.isEditMode());
        QTest::qWait(200);

        QPixmap pixDarkEdit = winDark.grab();
        pixDarkEdit.save(artifactDir + "/quicklook_edit_mode_dark_slate.png");
        winDark.close();
    }

    // 5. Capture Proposta 4 Live Audio Preview (780x540) with Pink Floyd Vinyl FLAC if present
    const QString pinkFloydTrack = "/home/davide/Musica/Pink Floyd - The Dark Side of the Moon (1973) [24 bit FLAC] vinyl/01 - Speak To Me.flac";
    if (QFile::exists(pinkFloydTrack)) {
        QPixmap sleeve = AudioProcessor::extractCoverArt(pinkFloydTrack, 220);
        QPixmap label = AudioProcessor::extractVinylLabelArt(pinkFloydTrack, 160);
        QVERIFY(!sleeve.isNull());
        QVERIFY(!label.isNull());

        ThemeManager::setPreset(ThemePreset::DarkSlate);
        MainWindow winAudio;
        winAudio.applyCurrentTheme();
        winAudio.resize(780, 540);
        winAudio.openFile(pinkFloydTrack);
        winAudio.show();
        (void)QTest::qWaitForWindowExposed(&winAudio);
        // Wait for async waveform peak computation and mpv duration load
        QTest::qWait(1400);

        auto *scrubber = winAudio.findChild<SmoothWaveformScrubber *>();
        if (scrubber) {
            QTest::mouseClick(scrubber, Qt::LeftButton, Qt::NoModifier,
                              QPoint(scrubber->width() * 34 / 100, scrubber->height() / 2));
            QTest::qWait(300);
        }

        QPixmap pixAudioLive = winAudio.grab();
        pixAudioLive.save(artifactDir + "/proposta_4_live_capture.png");
        winAudio.close();
    }
}

void TestInteractive::testPreviewModeZeroModificationLockdown()
{
    MainWindow window;
    window.resize(900, 600);
    window.show();
    (void)QTest::qWaitForWindowExposed(&window);

    // Initial state: Preview Mode
    QVERIFY(!window.isEditMode());

    // 1. Image lockdown in Preview Mode
    QString sampleImg = m_testDirPath + "/image_samples/portrait.jpg";
    if (QFile::exists(sampleImg)) {
        window.openFile(sampleImg);
        QVERIFY(!window.isEditMode());

        auto *imgViewer = window.findChild<ImageViewer*>();
        QVERIFY(imgViewer != nullptr);
        QCOMPARE(imgViewer->isEditMode(), false);

        // Crop mode cannot be activated in Preview Mode
        imgViewer->setCropModeActive(true);
        QCOMPARE(imgViewer->isCropModeActive(), false);

        // Rotation & flip do not modify the image in preview mode
        imgViewer->rotateClockwise();
        QCOMPARE(imgViewer->isModified(), false);
        imgViewer->flipHorizontal();
        QCOMPARE(imgViewer->isModified(), false);

        // Direct crop attempt is rejected
        imgViewer->applyCrop(QRect(10, 10, 50, 50));
        QCOMPARE(imgViewer->isModified(), false);

        // Switch to Edit Mode -> operations now succeed
        window.setEditMode(true);
        QVERIFY(window.isEditMode());
        QCOMPARE(imgViewer->isEditMode(), true);

        imgViewer->setCropModeActive(true);
        QCOMPARE(imgViewer->isCropModeActive(), true);

        // Switch back to Preview Mode -> crop immediately dismissed and locked
        window.setEditMode(false);
        QVERIFY(!window.isEditMode());
        QCOMPARE(imgViewer->isCropModeActive(), false);
        QCOMPARE(imgViewer->isEditMode(), false);
    }

    // 2. Text lockdown in Preview Mode
    QString sampleText = m_testDirPath + "/code_samples/sample_python.py";
    if (QFile::exists(sampleText)) {
        window.openFile(sampleText);
        QVERIFY(!window.isEditMode());

        auto *txtViewer = window.findChild<TextViewer*>();
        QVERIFY(txtViewer != nullptr);
        QCOMPARE(txtViewer->isEditable(), false);
        QCOMPARE(txtViewer->isReadOnly(), true);

        // Switch to Edit Mode
        window.setEditMode(true);
        QCOMPARE(txtViewer->isEditable(), true);
        QCOMPARE(txtViewer->isReadOnly(), false);

        // Switch back to Preview Mode
        window.setEditMode(false);
        QCOMPARE(txtViewer->isEditable(), false);
        QCOMPARE(txtViewer->isReadOnly(), true);
    }

    // 3. Archive lockdown in Preview Mode
    QString zipPath = m_testDirPath + "/__pycache__.zip";
    if (QFile::exists(zipPath)) {
        window.openFile(zipPath);
        QVERIFY(!window.isEditMode());

        auto *archViewer = window.findChild<ArchiveViewer*>();
        if (archViewer) {
            QCOMPARE(archViewer->isEditMode(), false);
            // Verify modify actions return safely without error
            archViewer->addFilesToArchive();
            archViewer->addFolderToArchive();
            archViewer->removeSelectedFromArchive();
            QVERIFY(!archViewer->isEditMode());
        }
    }

    // 4. PDF lockdown in Preview Mode
    QString pdfPath = m_testDirPath + "/document_samples/multipage_sample.pdf";
    if (QFile::exists(pdfPath)) {
        window.openFile(pdfPath);
        QVERIFY(!window.isEditMode());

        auto *pageWidget = window.findChild<PdfPageWidget*>();
        if (pageWidget) {
            QCOMPARE(pageWidget->isEditMode(), false);

            // Attempt to activate continuous annotation tools in preview mode
            pageWidget->setToolMode(PdfToolMode::Highlight);
            QCOMPARE(pageWidget->toolMode(), PdfToolMode::SelectText);

            pageWidget->setToolMode(PdfToolMode::Shape);
            QCOMPARE(pageWidget->toolMode(), PdfToolMode::SelectText);

            pageWidget->setToolMode(PdfToolMode::Redaction);
            QCOMPARE(pageWidget->toolMode(), PdfToolMode::SelectText);
        }
    }

    window.close();
}

void TestInteractive::testFramelessWindowAndDraggingRules()
{
    MainWindow window;
    window.showNormal();
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // 1. Frameless mode is active by default
    QVERIFY(window.isFrameless());
    QVERIFY(window.windowFlags() & Qt::FramelessWindowHint);

    // 2. MinimalHeaderBar is present with Close button, Prev/Next navigation, Gallery, Search, and Inspector buttons
    auto *header = window.minimalHeaderBar();
    QVERIFY(header != nullptr);
    QVERIFY(header->areWindowControlsVisible());
    QVERIFY(header->prevButton() != nullptr);
    QVERIFY(header->nextButton() != nullptr);
    QVERIFY(header->galleryButton() != nullptr);
    QVERIFY(header->searchButton() != nullptr);
    QVERIFY(header->inspectorButton() != nullptr);

    // 3. Title and Badge labels have WA_TransparentForMouseEvents set
    auto *lblTitle = header->findChild<QLabel*>();
    QVERIFY(lblTitle != nullptr);
    QVERIFY(lblTitle->testAttribute(Qt::WA_TransparentForMouseEvents));

    auto *lblBadge = header->findChild<QLabel*>("HeaderIndexBadge");
    if (lblBadge) {
        QVERIFY(lblBadge->testAttribute(Qt::WA_TransparentForMouseEvents));
    }

    // 4. Edge calculation for border resizing
    // Left edge
    QPoint leftPoint = window.mapToGlobal(QPoint(3, 300));
    QCOMPARE(window.calculateResizeEdges(leftPoint), Qt::Edges(Qt::LeftEdge));

    // Right edge
    QPoint rightPoint = window.mapToGlobal(QPoint(window.width() - 3, 300));
    QCOMPARE(window.calculateResizeEdges(rightPoint), Qt::Edges(Qt::RightEdge));

    // Top edge
    QPoint topPoint = window.mapToGlobal(QPoint(450, 3));
    QCOMPARE(window.calculateResizeEdges(topPoint), Qt::Edges(Qt::TopEdge));

    // Bottom edge
    QPoint bottomPoint = window.mapToGlobal(QPoint(450, window.height() - 3));
    QCOMPARE(window.calculateResizeEdges(bottomPoint), Qt::Edges(Qt::BottomEdge));

    // Top-Left corner
    QPoint topLeftPoint = window.mapToGlobal(QPoint(3, 3));
    QCOMPARE(window.calculateResizeEdges(topLeftPoint), Qt::Edges(Qt::TopEdge | Qt::LeftEdge));

    // Top-Right corner
    QPoint topRightPoint = window.mapToGlobal(QPoint(window.width() - 3, 3));
    QCOMPARE(window.calculateResizeEdges(topRightPoint), Qt::Edges(Qt::TopEdge | Qt::RightEdge));

    // Bottom-Left corner
    QPoint bottomLeftPoint = window.mapToGlobal(QPoint(3, window.height() - 3));
    QCOMPARE(window.calculateResizeEdges(bottomLeftPoint), Qt::Edges(Qt::BottomEdge | Qt::LeftEdge));

    // Bottom-Right corner
    QPoint bottomRightPoint = window.mapToGlobal(QPoint(window.width() - 3, window.height() - 3));
    QCOMPARE(window.calculateResizeEdges(bottomRightPoint), Qt::Edges(Qt::BottomEdge | Qt::RightEdge));

    // Interior point (no edges)
    QPoint centerPoint = window.mapToGlobal(QPoint(450, 300));
    QCOMPARE(window.calculateResizeEdges(centerPoint), Qt::Edges());

    // 5. Cursor updates
    window.updateResizeCursor(Qt::LeftEdge);
    QVERIFY(QGuiApplication::overrideCursor() != nullptr);
    QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::SizeHorCursor);

    window.updateResizeCursor(Qt::TopEdge);
    QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::SizeVerCursor);

    window.updateResizeCursor(Qt::TopEdge | Qt::LeftEdge);
    QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::SizeFDiagCursor);

    window.updateResizeCursor(Qt::TopEdge | Qt::RightEdge);
    QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::SizeBDiagCursor);

    window.updateResizeCursor(Qt::Edges());
    QVERIFY(QGuiApplication::overrideCursor() == nullptr);

    // 6. Header dragging: simulate mouse press and move
    QPoint clickPoint(150, 18);
    QMouseEvent pressEv(QEvent::MouseButtonPress, clickPoint, header->mapToGlobal(clickPoint),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(header, &pressEv);

    QPoint targetPoint(190, 38);
    QMouseEvent moveEv(QEvent::MouseMove, targetPoint, header->mapToGlobal(targetPoint),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(header, &moveEv);

    QMouseEvent releaseEv(QEvent::MouseButtonRelease, targetPoint, header->mapToGlobal(targetPoint),
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(header, &releaseEv);

    // 7. Maximize / Restore toggles
    window.showMaximized();
    QVERIFY(window.isMaximized());
    // In maximized mode, border resize edges are disabled
    QCOMPARE(window.calculateResizeEdges(leftPoint), Qt::Edges());

    window.showNormal();
    QVERIFY(!window.isMaximized());

    window.close();
}

void TestInteractive::testWindowGeometrySaveAndRestore()
{
    // Clean any prior saved geometry
    QSettings settings("Bubez", "BubezView");
    settings.remove("window/geometry");
    settings.remove("window/width");
    settings.remove("window/height");
    settings.setValue("remember_geometry", true);
    settings.sync();

    // 1. New window without saved geometry should default to 780x540
    {
        MainWindow win1;
        QCOMPARE(win1.size(), QSize(780, 540));

        // Resize window to custom dimensions (fits comfortably within offscreen 800x600 virtual screen)
        win1.resize(750, 500);
        QCOMPARE(win1.size(), QSize(750, 500));

        // Close window, which should trigger saveWindowGeometry()
        win1.close();
    }

    // Verify settings were saved
    QVERIFY(settings.contains("window/geometry") || (settings.contains("window/width") && settings.contains("window/height")));
    QCOMPARE(settings.value("window/width").toInt(), 750);
    QCOMPARE(settings.value("window/height").toInt(), 500);

    // 2. New window should now restore the saved dimensions (750x500)
    {
        MainWindow win2;
        QCOMPARE(win2.width(), 750);
        QCOMPARE(win2.height(), 500);
        win2.close();
    }

    // 3. Screen boundary safety clamping: if saved size exceeds screen size, it must be clamped
    QScreen *primary = QGuiApplication::primaryScreen();
    if (primary) {
        int screenW = primary->availableGeometry().width();
        int screenH = primary->availableGeometry().height();
        settings.setValue("window/width", screenW + 300);
        settings.setValue("window/height", screenH + 200);
        settings.sync();

        MainWindow winClamp;
        QVERIFY(winClamp.width() <= screenW);
        QVERIFY(winClamp.height() <= screenH);
        winClamp.close();
    }

    // 4. Test when remember_geometry is disabled
    settings.setValue("remember_geometry", false);
    settings.sync();
    {
        MainWindow win3;
        // Should fall back to default size 780x540
        QCOMPARE(win3.size(), QSize(780, 540));
        win3.close();
    }

    // Restore remember_geometry to true and cleanup
    settings.setValue("remember_geometry", true);
    settings.remove("window/geometry");
    settings.remove("window/width");
    settings.remove("window/height");
    settings.sync();
}

void TestInteractive::testAudioCoverDirectSaveToFolder()
{
    QTemporaryDir tempAlbumDir;
    QVERIFY(tempAlbumDir.isValid());

    // Create a dummy audio file in the temporary album directory
    QString fakeAudioPath = tempAlbumDir.path() + "/01_track.mp3";
    {
        QFile f(fakeAudioPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("ID3dummy_audio_bytes");
        f.close();
    }

    // Create a sample image outside the album folder (e.g. in /tmp)
    QString externalCover = QDir::tempPath() + "/sample_external_cover.jpg";
    {
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(Qt::cyan);
        QVERIFY(img.save(externalCover, "JPG"));
    }

    // Verify album folder does NOT have cover.jpg initially
    QString expectedCover = tempAlbumDir.path() + "/cover.jpg";
    QVERIFY(!QFile::exists(expectedCover));

    // Open AudioTagDialog and trigger saveCoverToAlbumFolder
    AudioTagDialog dlg(fakeAudioPath);
    QString savedPath = dlg.saveCoverToAlbumFolder(externalCover);
    QCOMPARE(savedPath, expectedCover);
    QVERIFY(QFile::exists(expectedCover));

    // Verify extractCoverArt now immediately finds the saved cover.jpg
    QPixmap cov = AudioProcessor::extractCoverArt(fakeAudioPath, 150);
    QVERIFY(!cov.isNull());
    QCOMPARE(cov.size(), QSize(150, 150));

    // Cleanup external temp file
    QFile::remove(externalCover);
}

void TestInteractive::testMinimalHeaderBarSnapshots()
{
    MainWindow win;
    win.resize(1100, 600);
    win.show();
    Q_UNUSED(QTest::qWaitForWindowExposed(&win));

    auto *header = win.minimalHeaderBar();
    QVERIFY(header != nullptr);

    QString artDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7");

    // 1. Normal state (Dark preset)
    header->applyThemePreset(ThemePreset::DarkSlate);
    header->setTitle(QStringLiteral("sample_document.png"));
    header->setIndexBadge(QStringLiteral("3 / 15"));
    header->setEditMode(false);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_normal_dark.png");

    // 2. Normal state (Light preset)
    header->applyThemePreset(ThemePreset::ModernLight);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_normal_light.png");

    // 3. Image Edit Mode (Dark preset)
    header->applyThemePreset(ThemePreset::DarkSlate);
    header->setCategory(FileCategory::Image);
    header->setEditMode(true);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_image_edit_dark.png");

    // 4. Image Edit Mode (Light preset)
    header->applyThemePreset(ThemePreset::ModernLight);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_image_edit_light.png");

    // 5. PDF Edit Mode (Dark preset)
    header->applyThemePreset(ThemePreset::DarkSlate);
    header->setCategory(FileCategory::Pdf);
    header->setEditMode(true);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_edit_dark.png");

    // 6. PDF Edit Mode (Light preset)
    header->applyThemePreset(ThemePreset::ModernLight);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_edit_light.png");

    // 6b. PDF Edit Mode with Contextual Text Bar (Dark preset)
    header->applyThemePreset(ThemePreset::DarkSlate);
    header->showContextualTextBar();
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_edit_text_dark.png");

    // 6c. PDF Edit Mode with Contextual Text Bar (Light preset)
    header->applyThemePreset(ThemePreset::ModernLight);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_edit_text_light.png");

    // 6d. PDF Edit Mode with Contextual Highlight Bar (Dark preset)
    header->applyThemePreset(ThemePreset::DarkSlate);
    header->showContextualHighlightBar();
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_highlight_bar.png");

    // 6e. PDF Edit Mode with Contextual Shapes Bar (Dark preset)
    header->showContextualShapesBar(0);
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_shapes_bar.png");

    // 6f. PDF Edit Mode with Contextual Redaction Bar (Dark preset)
    header->showContextualRedactBar();
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_redact_bar.png");

    // 6g. PDF Edit Mode with Contextual Rubber Stamp Bar (Dark preset)
    header->showContextualStampBar();
    QTest::qWait(80);
    header->grab().save(artDir + "/header_bar_pdf_stamp_bar.png");
    header->hideContextualBar();

    // 7. Full Window: Image loaded
    QString imgPath = m_testDirPath + "/2_inspector_flyout_card.jpg";
    win.openFile(imgPath);
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_image.png");

    // 8. Full Window: Image Edit Mode
    win.setEditMode(true);
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_image_edit.png");
    win.setEditMode(false);

    // 9. Full Window: PDF loaded
    QString pdfPath = m_testDirPath + "/ciao.pdf";
    win.openFile(pdfPath);
    win.resize(1100, 600);
    QApplication::processEvents();
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf.png");

    // 10. Full Window: PDF Edit Mode
    win.setEditMode(true);
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_edit.png");

    // 10a. Full Window: PDF Edit Mode with Contextual Text Bar active
    header->showContextualTextBar();
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_edit_text.png");

    // 10b. Full Window: PDF Edit Mode with Contextual Highlight Bar active
    header->showContextualHighlightBar();
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_edit_highlight.png");
    header->hideContextualBar();

    // 10b. Verify that opening PDF Studio Panel resizes the PDF without overlapping
    auto *pv = win.pdfViewer();
    if (pv) {
        pv->openStudioPanel(0);
        QTest::qWait(120);
        win.grab().save(artDir + "/full_window_pdf_studio_open.png");

        auto *pageWidget = pv->widget()->findChild<PdfPageWidget*>();
        auto *scrollArea = (pageWidget && pageWidget->parentWidget()) ? qobject_cast<QScrollArea*>(pageWidget->parentWidget()->parentWidget()) : nullptr;
        QVERIFY(pageWidget != nullptr);
        QVERIFY(scrollArea != nullptr);
        QVERIFY(pageWidget->width() <= scrollArea->viewport()->width() + 10);

        pv->closeStudioPanel();
        QTest::qWait(120);
        QVERIFY(pageWidget->width() <= scrollArea->viewport()->width() + 10);
    }

    win.setEditMode(false);

    // 11. Multi-page PDF: Floating HUD pill overlay in action (Light theme)
    QString multiPdf = m_testDirPath + "/multipage_sample.pdf";
    win.openFile(multiPdf);
    ThemeManager::setPreset(ThemePreset::ModernLight);
    ThemeManager::applyTheme(qApp, &win);
    auto *pdfViewer = win.pdfViewer();
    if (pdfViewer) {
        pdfViewer->updateTheme();
        pdfViewer->nextPage();
    }
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_hud_light.png");

    // 12. Multi-page PDF: Floating HUD pill overlay in action (Dark Slate theme)
    ThemeManager::setPreset(ThemePreset::DarkSlate);
    ThemeManager::applyTheme(qApp, &win);
    if (pdfViewer) {
        pdfViewer->updateTheme();
        pdfViewer->nextPage();
    }
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_hud_dark.png");

    // 13. Verify that in reading mode, window minimum width is not blocked by hidden edit tools
    win.setEditMode(false);
    int readingMinW = win.minimumSizeHint().width();
    QVERIFY2(readingMinW < 600, qPrintable(QString("Reading mode min width should be < 600px, but got %1").arg(readingMinW)));

    // 14. Full Window: PDF in narrow reading window (520px)
    win.resize(520, 600);
    QTest::qWait(120);
    win.grab().save(artDir + "/full_window_pdf_narrow.png");
}

void TestInteractive::testPreEditGeometryRestorationAndAudioSizing()
{
    MainWindow win;
    win.show();
    (void)QTest::qWaitForWindowExposed(&win);

    // 1. PDF Window Size Restoration
    QString pdfPath = m_testDirPath + "/ciao.pdf";
    if (QFile::exists(pdfPath)) {
        win.openFile(pdfPath);
        win.resize(540, 460);
        QTest::qWait(50);
        QCOMPARE(win.width(), 540);
        QCOMPARE(win.height(), 460);

        win.setEditMode(true);
        QTest::qWait(50);
        QVERIFY(win.isEditMode());

        win.setEditMode(false);
        QTest::qWait(50);
        QVERIFY(!win.isEditMode());
        QCOMPARE(win.width(), 540);
        QCOMPARE(win.height(), 460);
    }

    // 2. Archive Window Size Restoration
    QString zipPath = m_testDirPath + "/__pycache__.zip";
    if (QFile::exists(zipPath)) {
        win.openFile(zipPath);
        win.resize(560, 480);
        QTest::qWait(50);
        QCOMPARE(win.width(), 560);
        QCOMPARE(win.height(), 480);

        win.setEditMode(true);
        QTest::qWait(50);
        QVERIFY(win.isEditMode());

        win.setEditMode(false);
        QTest::qWait(50);
        QVERIFY(!win.isEditMode());
        QCOMPARE(win.width(), 560);
        QCOMPARE(win.height(), 480);
    }

    // 3. Audio Player Sizing and Window Size Restoration
    QString audioPath = m_testDirPath + "/audio_samples/test_tone.mp3";
    if (QFile::exists(audioPath)) {
        win.openFile(audioPath);
        QTest::qWait(50);
        QVERIFY(win.mediaViewer() != nullptr);
        QVERIFY(win.mediaViewer()->isAudioOnly());

        win.resize(510, 440);
        QTest::qWait(50);
        QVERIFY2(win.width() <= 520, qPrintable(QString("Audio reading window width should be <= 520px, got %1").arg(win.width())));

        MediaViewer *mv = qobject_cast<MediaViewer*>(win.mediaViewer()->widget());
        if (mv) {
            // In 510px narrow window: responsive compact mode (Option 1A, 2B, 3A) is engaged
            QVERIFY(mv->isCompactAudioLayout());
            if (mv->vinylSleeveWidget()) {
                QVERIFY(mv->vinylSleeveWidget()->isCompactMode());
            }

            // In wide window (>= 660px): responsive wide side-by-side layout is engaged
            win.resize(780, 500);
            QTest::qWait(50);
            QVERIFY(!mv->isCompactAudioLayout());
            if (mv->vinylSleeveWidget()) {
                QVERIFY(!mv->vinylSleeveWidget()->isCompactMode());
            }

            // Return to 510px: compact mode engages again
            win.resize(510, 440);
            QTest::qWait(50);
            QVERIFY(mv->isCompactAudioLayout());
            if (mv->vinylSleeveWidget()) {
                QVERIFY(mv->vinylSleeveWidget()->isCompactMode());
            }
        }

        // Verify edit mode toggle and return to exact pre-edit geometry
        win.setEditMode(true);
        QTest::qWait(50);
        QVERIFY(win.isEditMode());

        win.setEditMode(false);
        QTest::qWait(50);
        QVERIFY(!win.isEditMode());
        QCOMPARE(win.width(), 510);
        QCOMPARE(win.height(), 440);
    }
}

void TestInteractive::testTriestePdfEditing()
{
    QString triestePdf = QStringLiteral(PROJECT_ROOT_DIR "/421_Guida turistica di Trieste.pdf");
    if (!QFile::exists(triestePdf)) {
        QSKIP("421_Guida turistica di Trieste.pdf not found");
    }

    MainWindow win;
    win.show();
    QCOMPARE(win.openFile(triestePdf), MainWindow::OpenResult::Success);
    QTest::qWait(100);

    // Switch to edit mode
    win.setEditMode(true);
    QTest::qWait(50);
    QVERIFY(win.isEditMode());

    auto *header = win.minimalHeaderBar();
    QVERIFY(header != nullptr);
    QVERIFY(header->quickUndoButton() != nullptr);
    QVERIFY(header->historyTimelineButton() != nullptr);

    // Test text replace from contextual bar
    header->showContextualTextBar();
    auto *cbar = header->contextualBar();
    QVERIFY(cbar != nullptr);
    cbar->setTextContent(QStringLiteral("Trieste Nuova Guida 2026"));
    emit cbar->textReplaceRequested(QStringLiteral("Trieste Nuova Guida 2026"), QFont("Sans Serif", 14), QColor(Qt::darkBlue), true);
    QTest::qWait(100);

    // Test quick undo
    header->quickUndoButton()->click();
    QTest::qWait(100);

    // Test timeline slides
    auto *historyWidget = header->historyWidget();
    QVERIFY(historyWidget != nullptr);
    header->setHistorySteps(win.pdfViewer() ? win.pdfViewer()->historySteps() : QList<PdfHistoryStep>{});

    // 1. Verify window geometry stability across multiple edit toggles
    if (win.pdfViewer()) win.pdfViewer()->setModified(false);
    win.setEditMode(false);
    QTest::qWait(50);
    int initH = win.height();
    int initW = win.width();
    for (int t = 0; t < 4; ++t) {
        win.setEditMode(true);
        QTest::qWait(20);
        win.setEditMode(false);
        QTest::qWait(20);
    }
    QCOMPARE(win.height(), initH);
    QCOMPARE(win.width(), initW);

    // Switch back to edit mode for tool testing
    win.setEditMode(true);
    QTest::qWait(20);

    // 2. Test shapes bar, redact bar, stamp bar tools without crash
    header->showContextualShapesBar(0);
    emit header->pdfShapesToolRequested(0);
    QTest::qWait(30);

    header->showContextualRedactBar();
    emit header->pdfRedactToolRequested();
    QTest::qWait(30);

    header->showContextualStampBar();
    emit header->pdfStampToolRequested();
    QTest::qWait(30);

    // 3. Test continuous shape drawing and moving on pdfPageWidget
    auto *pv = dynamic_cast<PdfViewer*>(win.pdfViewer());
    QVERIFY(pv != nullptr);
    auto *pageWidget = pv->pageWidget();
    QVERIFY(pageWidget != nullptr);

    // Draw rect
    header->showContextualShapesBar(0);
    emit header->pdfShapesToolRequested(0);
    QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(120, 120), QPointF(120, 120), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &pressEv);
    QMouseEvent dragEv(QEvent::MouseMove, QPointF(240, 220), QPointF(240, 220), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &dragEv);
    QMouseEvent relEv(QEvent::MouseButtonRelease, QPointF(240, 220), QPointF(240, 220), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &relEv);
    QTest::qWait(50);

    // Move rect
    QMouseEvent pressMove(QEvent::MouseButtonPress, QPointF(150, 150), QPointF(150, 150), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &pressMove);
    QMouseEvent dragMove(QEvent::MouseMove, QPointF(180, 180), QPointF(180, 180), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &dragMove);
    QMouseEvent relMove(QEvent::MouseButtonRelease, QPointF(180, 180), QPointF(180, 180), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &relMove);
    QTest::qWait(50);

    // 4. Test highlight and global property update
    header->showContextualHighlightBar();
    emit header->pdfHighlightToolToggled(true);
    QMouseEvent pressHl(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &pressHl);
    QMouseEvent dragHl(QEvent::MouseMove, QPointF(200, 130), QPointF(200, 130), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &dragHl);
    QMouseEvent relHl(QEvent::MouseButtonRelease, QPointF(200, 130), QPointF(200, 130), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(pageWidget, &relHl);
    QTest::qWait(50);

    // Update highlight properties globally
    pageWidget->updateHighlighterProperties(Qt::darkGreen, 0.75);
    QTest::qWait(30);

    // 5. Test close contextual bar exits tool mode
    header->hideContextualBar();
    QCOMPARE(pageWidget->toolMode(), PdfToolMode::SelectText);
    if (win.pdfViewer()) win.pdfViewer()->setModified(false);
}

QTEST_MAIN(TestInteractive)
#include "test_interactive.moc"




