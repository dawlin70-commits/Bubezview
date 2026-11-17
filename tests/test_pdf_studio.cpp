/**
 * @file test_pdf_studio.cpp
 * @brief Automated unit test suite verifying PdfStudioPanel and dynamic PdfTransformBox.
 */

#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QSplitter>
#include <QListWidget>
#include <QStandardPaths>
#include <QFileIconProvider>
#include <QTimer>
#include "ui/vector_icons.h"

#include "core/services/theme_manager.h"

#include "ui/pdf/pdf_studio_panel.h"
#include "ui/pdf/pdf_history_widget.h"
#include "viewers/pdf/pdf_transform_box.h"
#include "viewers/pdf/pdf_canvas_tools.h"
#include "ui/minimal_header_bar.h"
#include "ui/contextual_action_bar.h"
#include "ui/pdf/pdf_signature_dialog.h"
#include "ui/dialogs/unified_export_dialog.h"
#include "ui/pdf/pdf_image_extract_dialog.h"

class TestPdfStudio : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testStudioPanelTabs();
    void testStudioPanelTextSignals();
    void testStudioPanelShapeSignals();
    void testStudioPanelWatermarkSignals();
    void testStudioPanelSignatureSignals();
    void testStudioPanelRedactionSignals();
    void testStudioPanelPageOrganizer();
    void testTransformBoxTextInline();
    void testTransformBoxDynamicHeightExpansion();
    void testTransformBoxColumnWidthResize();
    void testTransformBoxShapeMode();
    void testTransformBoxSignals();
    void testHistoryWidgetCarousel();
    void testMinimalHeaderBarRefinements();
    void testContextualActionBarRefinements();
    void testInPlaceTextEditingAndSmartBackground();
    void testZoomSystem();
    void testSignatureDialogUnifiedFlow();
    void testExportAndImageExtractDialogs();
    void testUiSnapshots();
};

void TestPdfStudio::initTestCase()
{
    // Ensure Qt GUI test environment is ready
    QVERIFY(qApp != nullptr);
}

void TestPdfStudio::testStudioPanelTabs()
{
    PdfStudioPanel panel;
    panel.show();

    // Verify initial tab is 0 (Text)
    QCOMPARE(panel.currentTabIndex(), 0);

    // Switch between all 5 studio tabs (Text, Shapes, Stamps, Sign, Redact)
    for (int i = 0; i < 5; ++i) {
        panel.setCurrentTab(i);
        QCOMPARE(panel.currentTabIndex(), i);
    }

    panel.setCurrentTab(0);
    QCOMPARE(panel.currentTabIndex(), 0);
}

void TestPdfStudio::testStudioPanelTextSignals()
{
    PdfStudioPanel panel;

    QSignalSpy fontSpy(&panel, &PdfStudioPanel::textFontChanged);
    QSignalSpy colorSpy(&panel, &PdfStudioPanel::textColorChanged);
    QSignalSpy whiteoutSpy(&panel, &PdfStudioPanel::textWhiteoutChanged);
    QSignalSpy alignSpy(&panel, &PdfStudioPanel::textAlignmentChanged);

    // Initial values
    QFont f = panel.currentFont();
    QVERIFY(f.pointSize() > 0);
    QCOMPARE(panel.textColor(), QColor(Qt::black));
    QVERIFY(!panel.isWhiteout());
    QCOMPARE(panel.textAlignment(), Qt::AlignLeft);
}

void TestPdfStudio::testStudioPanelShapeSignals()
{
    PdfStudioPanel panel;

    QSignalSpy shapeSpy(&panel, &PdfStudioPanel::shapeParamsChanged);

    panel.setCurrentTab(1); // Shapes tab

    // Check defaults
    QCOMPARE(panel.shapeType(), 0); // Rectangle
    QCOMPARE(panel.shapeStrokeWidth(), 2);
    QCOMPARE(panel.shapeFillColor(), QColor(Qt::transparent));
}

void TestPdfStudio::testStudioPanelWatermarkSignals()
{
    PdfStudioPanel panel;

    QSignalSpy watermarkSpy(&panel, &PdfStudioPanel::watermarkParamsChanged);

    panel.setCurrentTab(2); // Watermarks/Stamps tab

    // Verify defaults
    QCOMPARE(panel.watermarkAngle(), 45.0);
    QVERIFY(panel.watermarkOpacity() > 0.0);
}

void TestPdfStudio::testStudioPanelSignatureSignals()
{
    PdfStudioPanel panel;
    QSignalSpy cryptoSpy(&panel, &PdfStudioPanel::cryptoSignatureRequested);
    QSignalSpy verifySpy(&panel, &PdfStudioPanel::verifyCryptoSignaturesRequested);
    QSignalSpy largePadSpy(&panel, &PdfStudioPanel::openLargeSignaturePadRequested);

    panel.setCurrentTab(3); // Sign tab

    auto buttons = panel.findChildren<QPushButton*>();
    for (auto *b : buttons) {
        if (b->text().contains("Large") || b->text().contains("Ampia")) {
            b->click();
        } else if (b->text().contains("Sign") || b->text().contains("Firma")) {
            b->click();
        } else if (b->text().contains("Verify") || b->text().contains("Verifica")) {
            b->click();
        }
    }

    QVERIFY(largePadSpy.count() >= 1);
    QVERIFY(cryptoSpy.count() >= 1);
    QVERIFY(verifySpy.count() >= 1);
}

void TestPdfStudio::testStudioPanelRedactionSignals()
{
    PdfStudioPanel panel;

    QSignalSpy redactionColorSpy(&panel, &PdfStudioPanel::redactionColorChanged);
    QSignalSpy startRedactionSpy(&panel, &PdfStudioPanel::startRedactionRequested);

    panel.setCurrentTab(4); // Redaction tab

    // Check default redaction color is black
    QCOMPARE(panel.redactionColor(), QColor(Qt::black));
}

void TestPdfStudio::testStudioPanelPageOrganizer()
{
    PdfStudioPanel panel;
    panel.updatePageInfo(2, 5);
    QCOMPARE(panel.currentTabIndex(), 0);
    panel.updatePageInfo(0, 10);
    QCOMPARE(panel.currentTabIndex(), 0);
}

void TestPdfStudio::testTransformBoxTextInline()
{
    QWidget parent;
    parent.resize(800, 600);
    parent.show();

    PdfTransformBox box(&parent);
    QFont font("Arial", 14);

    box.resize(800, 600);
    box.showText("Hello World", font, QColor(Qt::black), false, QPoint(200, 200));
    QVERIFY(box.isVisible());
    QCOMPARE(box.itemType(), PdfTransformItemType::Text);
    QCOMPARE(box.text(), QStringLiteral("Hello World"));

    // Check that inline text editor exists and has focus
    QTextEdit *editor = box.inlineTextEdit();
    QVERIFY(editor != nullptr);
    QVERIFY(editor->isVisible());
    QCOMPARE(editor->toPlainText(), QStringLiteral("Hello World"));
}

void TestPdfStudio::testTransformBoxDynamicHeightExpansion()
{
    QWidget parent;
    parent.resize(800, 800);
    parent.show();

    PdfTransformBox box(&parent);
    QFont font("Arial", 12);

    box.showText("Line 1", font, QColor(Qt::black), false, QPoint(200, 200));
    qreal initialHeight = box.boxRect().height();

    // Type a multi-line paragraph that wraps significantly
    QTextEdit *editor = box.inlineTextEdit();
    QVERIFY(editor != nullptr);
    editor->setPlainText("Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8");

    // The box rect should dynamically expand in height to fit the content
    qreal expandedHeight = box.boxRect().height();
    QVERIFY(expandedHeight > initialHeight);
}

void TestPdfStudio::testTransformBoxColumnWidthResize()
{
    QWidget parent;
    parent.resize(800, 800);
    parent.show();

    PdfTransformBox box(&parent);
    QFont font("Arial", 12);

    QString longText = "This is a long paragraph that wraps across multiple lines when the column width is narrow.";
    box.showText(longText, font, QColor(Qt::black), false, QPoint(200, 200));
    qreal narrowHeight = box.boxRect().height();

    // Now widen the column width: text has more horizontal space, so height should decrease
    box.setBoxRect(QRect(100, 100, 450, 40));
    box.adjustHeightToFitText();
    qreal wideHeight = box.boxRect().height();

    QVERIFY(wideHeight <= narrowHeight);
}

void TestPdfStudio::testTransformBoxShapeMode()
{
    QWidget parent;
    parent.resize(800, 600);
    parent.show();

    PdfTransformBox box(&parent);
    QRect shapeRect(100, 100, 200, 150);

    box.showShape(1, 4, QColor(Qt::red), QColor(0, 0, 255, 80), shapeRect);
    QVERIFY(box.isVisible());
    QCOMPARE(box.itemType(), PdfTransformItemType::Shape);
    QCOMPARE(box.shapeType(), 1);
    QCOMPARE(box.shapeStrokeWidth(), 4);
    QCOMPARE(box.shapeStrokeColor(), QColor(Qt::red));
    QCOMPARE(box.shapeFillColor(), QColor(0, 0, 255, 80));

    // Update shape parameters live
    box.setShapeType(2);
    QCOMPARE(box.shapeType(), 2);

    box.setShapeStrokeWidth(8);
    QCOMPARE(box.shapeStrokeWidth(), 8);

    box.setShapeStrokeColor(QColor(Qt::green));
    QCOMPARE(box.shapeStrokeColor(), QColor(Qt::green));

    box.setShapeFillColor(QColor(Qt::yellow));
    QCOMPARE(box.shapeFillColor(), QColor(Qt::yellow));
}

void TestPdfStudio::testTransformBoxSignals()
{
    QWidget parent;
    parent.resize(800, 600);
    parent.show();

    PdfTransformBox box(&parent);
    QSignalSpy applySpy(&box, &PdfTransformBox::applied);
    QSignalSpy cancelSpy(&box, &PdfTransformBox::cancelled);

    box.resize(800, 600);
    box.showText("Test", QFont(), QColor(Qt::black), false, QPoint(200, 200));

    QPushButton *applyBtn = box.findChild<QPushButton*>("BtnApply");
    QPushButton *cancelBtn = box.findChild<QPushButton*>("BtnCancel");

    QVERIFY(applyBtn != nullptr);
    QVERIFY(cancelBtn != nullptr);

    applyBtn->click();
    QCOMPARE(applySpy.count(), 1);

    cancelBtn->click();
    QCOMPARE(cancelSpy.count(), 1);
}

void TestPdfStudio::testHistoryWidgetCarousel()
{
    PdfHistoryWidget historyWidget;
    historyWidget.show();

    QSignalSpy stepSpy(&historyWidget, &PdfHistoryWidget::stepSelected);

    QList<PdfHistoryStep> steps;
    PdfHistoryStep s1;
    s1.index = 0;
    s1.description = "Original Document";
    s1.timestamp = QDateTime::currentDateTime();
    s1.isCurrent = false;

    PdfHistoryStep s2;
    s2.index = 1;
    s2.description = "Add Rectangle";
    s2.timestamp = QDateTime::currentDateTime().addSecs(10);
    s2.isCurrent = true;

    steps << s1 << s2;
    historyWidget.setHistorySteps(steps);

    auto cards = historyWidget.findChildren<PdfHistoryCard*>();
    QCOMPARE(cards.size(), 2);
    QCOMPARE(cards[0]->stepIndex(), 0);
    QCOMPARE(cards[1]->stepIndex(), 1);

    emit cards[0]->clicked(0);
    QCOMPARE(stepSpy.count(), 1);
    QCOMPARE(stepSpy.first().first().toInt(), 0);
}

void TestPdfStudio::testMinimalHeaderBarRefinements()
{
    MinimalHeaderBar bar;
    bar.setCategory(FileCategory::Pdf);
    bar.setEditMode(true);
    bar.show();

    // Verify Edit button displays Done or Confirm
    QVERIFY(bar.editButton() != nullptr);
    QVERIFY(bar.editButton()->toolTip().contains("Done") || bar.editButton()->text().contains("Confirm"));

    // Verify Quick Undo button is separate and visible
    QVERIFY(bar.quickUndoButton() != nullptr);
    QVERIFY(bar.quickUndoButton()->toolTip().contains("Annulla") || bar.quickUndoButton()->toolTip().contains("Undo"));

    // Verify History Timeline button is separate and visible
    QVERIFY(bar.historyTimelineButton() != nullptr);
    QVERIFY(bar.historyTimelineButton()->toolTip().contains("Cronologia") ||
            bar.historyTimelineButton()->toolTip().contains("Timeline") ||
            bar.historyTimelineButton()->toolTip().contains("History"));

    // Verify Insert button has a menu containing Geometric Shapes, Insert Image, Highlight, and Stamp
    QVERIFY(bar.pdfShapesButton() != nullptr);
    QVERIFY(bar.pdfShapesButton()->toolTip().contains("Inserisci") || bar.pdfShapesButton()->toolTip().contains("Insert"));
    QMenu *shapesMenu = bar.pdfShapesButton()->menu();
    QVERIFY(shapesMenu != nullptr);
    QCOMPARE(shapesMenu->actions().size(), 4);
    QVERIFY(shapesMenu->actions()[0]->text().contains("Geometric Shapes"));
    QVERIFY(shapesMenu->actions()[1]->text().contains("Insert Image"));
    QVERIFY(shapesMenu->actions()[2]->text().contains("Highlight"));
    QVERIFY(shapesMenu->actions()[3]->text().contains("Rubber Stamp"));
}

void TestPdfStudio::testContextualActionBarRefinements()
{
    ContextualActionBar cbar;
    cbar.show();

    // 1. Test Shapes mode
    cbar.setMode(ContextBarMode::Shapes);
    QCOMPARE(cbar.mode(), ContextBarMode::Shapes);
    QCOMPARE(cbar.height(), 36);

    // 2. Test Stamp mode & ComboBox NoInsert, Angle slider, Size slider, Style dropdown, Colors
    cbar.setMode(ContextBarMode::Stamp);
    QCOMPARE(cbar.mode(), ContextBarMode::Stamp);
    auto *cmbStamp = cbar.stampComboBox();
    QVERIFY(cmbStamp != nullptr);
    QCOMPARE(cmbStamp->insertPolicy(), QComboBox::NoInsert);

    auto *cmbStyle = cbar.stampStyleComboBox();
    QVERIFY(cmbStyle != nullptr);
    QCOMPARE(cmbStyle->count(), 5);

    cbar.setStampProperties(QStringLiteral("CONFIDENTIAL"), QColor(QStringLiteral("#2563eb")), 20.0, 1, 36);
    QCOMPARE(cbar.stampText(), QStringLiteral("CONFIDENTIAL"));
    QCOMPARE(cbar.stampColor(), QColor(QStringLiteral("#2563eb")));
    QCOMPARE(cbar.stampAngle(), 20.0);
    QCOMPARE(cbar.stampStyle(), 1);
    QCOMPARE(cbar.stampSize(), 36);

    QSignalSpy spyStamp(&cbar, &ContextualActionBar::stampPropertiesChanged);
    QSignalSpy spyCommit(&cbar, &ContextualActionBar::stampPropertiesCommitted);
    cbar.setStampAngle(-10.0);
    cbar.emitStampProperties();
    QCOMPARE(spyStamp.count(), 1);
    QCOMPARE(spyStamp.at(0).at(0).toString(), QStringLiteral("CONFIDENTIAL"));
    QCOMPARE(spyStamp.at(0).at(2).toDouble(), -10.0);
    QCOMPARE(spyStamp.at(0).at(3).toInt(), 1);
    QCOMPARE(spyStamp.at(0).at(4).toInt(), 36);

    cbar.stampStyleComboBox()->activated(2);
    QCOMPARE(spyCommit.count(), 1);

    // 3. Test Redact mode
    cbar.setMode(ContextBarMode::Redact);
    QCOMPARE(cbar.mode(), ContextBarMode::Redact);

    // 4. Test Highlight mode (Spotlight removed)
    cbar.setMode(ContextBarMode::Highlight);
    QCOMPARE(cbar.isSpotlight(), false);

    // 5. Test Text mode direct replacement
    cbar.setMode(ContextBarMode::Text);
    cbar.setTextContent("New edited brochure title");
    QCOMPARE(cbar.textContent(), QString("New edited brochure title"));

    // 6. Test MinimalHeaderBar preview vs edit mode guard
    MinimalHeaderBar hdr;
    hdr.setCategory(FileCategory::Pdf);
    hdr.setEditMode(false); // In preview mode!
    hdr.showContextualTextBar();
    QVERIFY(hdr.contextualBar() == nullptr || hdr.contextualBar()->mode() == ContextBarMode::Hidden);
    hdr.showContextualStampBar();
    QVERIFY(hdr.contextualBar() == nullptr || hdr.contextualBar()->mode() == ContextBarMode::Hidden);

    hdr.setEditMode(true); // In edit mode!
    hdr.showContextualTextBar();
    QVERIFY(hdr.contextualBar() != nullptr && hdr.contextualBar()->mode() == ContextBarMode::Text);
}

void TestPdfStudio::testInPlaceTextEditingAndSmartBackground()
{
    // 1. Test Smart Background Color Sampling with solid TIM Blue (#004785)
    QImage timSlide(400, 200, QImage::Format_ARGB32_Premultiplied);
    timSlide.fill(QColor("#004785"));
    {
        // Draw white text in center
        QPainter p(&timSlide);
        p.setFont(QFont("Sans Serif", 16, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(QRect(50, 50, 150, 40), Qt::AlignCenter, "TIM Telecom");
    }

    QRect textRect(50, 50, 150, 40);
    QColor sampledBg = PdfCanvasTools::sampleBackgroundColor(timSlide, textRect);
    QCOMPARE(sampledBg, QColor("#004785"));

    // 2. Test Smart Background Sampling with aged cream paper (#fdf8e2)
    QImage creamDoc(400, 200, QImage::Format_ARGB32_Premultiplied);
    creamDoc.fill(QColor("#fdf8e2"));
    {
        QPainter p(&creamDoc);
        p.setFont(QFont("Times", 12));
        p.setPen(Qt::black);
        p.drawText(QRect(40, 40, 120, 30), Qt::AlignLeft, "Invoice 2026");
    }
    QColor creamSample = PdfCanvasTools::sampleBackgroundColor(creamDoc, QRect(40, 40, 120, 30));
    QCOMPARE(creamSample, QColor("#fdf8e2"));

    // 3. Test Feathered Patch Generation
    QSize patchSize(120, 40);
    QImage feathered = PdfCanvasTools::createFeatheredPatch(patchSize, QColor("#004785"), 6);
    QCOMPARE(feathered.size(), patchSize);
    // Center pixel should be fully opaque or near-opaque
    QRgb centerPixel = feathered.pixel(60, 20);
    QVERIFY(qAlpha(centerPixel) >= 240);
    // Outer border pixel (0, 0) should be fully transparent
    QRgb cornerPixel = feathered.pixel(0, 0);
    QCOMPARE(qAlpha(cornerPixel), 0);

    // 4. Test ContextualActionBar background mode and custom color
    ContextualActionBar cbar;
    cbar.setTextBgMode(PdfTextBgMode::Auto);
    QCOMPARE(cbar.textBgMode(), PdfTextBgMode::Auto);
    QVERIFY(cbar.isWhiteout());

    cbar.setTextBgMode(PdfTextBgMode::Transparent);
    QCOMPARE(cbar.textBgMode(), PdfTextBgMode::Transparent);
    QVERIFY(!cbar.isWhiteout());

    cbar.setTextBgMode(PdfTextBgMode::Feathered);
    QCOMPARE(cbar.textBgMode(), PdfTextBgMode::Feathered);

    cbar.setTextBgColor(QColor("#004785"));
    QCOMPARE(cbar.textBgColor(), QColor("#004785"));

    // 5. Test PdfTransformBox in-place text edit initialization with exact rect & sampled background
    PdfTransformBox box;
    box.resize(800, 600);
    QFont testFont("Sans Serif", 14);
    box.showText("In-Place Sample", testFont, Qt::white, PdfTextBgMode::Auto, QColor("#004785"), QRect(100, 120, 160, 36));

    QVERIFY(box.inlineTextEdit() != nullptr);
    QCOMPARE(box.inlineTextEdit()->toPlainText(), QString("In-Place Sample"));
    QCOMPARE(box.textBgMode(), PdfTextBgMode::Auto);
    QCOMPARE(box.textBgColor(), QColor("#004785"));
    QVERIFY(box.boxRect().contains(QRect(100, 120, 160, 36)));
    // Verify that single-line text does not expand beyond initialRect + padding
    QFontMetrics fm(testFont);
    QVERIFY(box.boxRect().height() <= std::max(36 + 6, fm.lineSpacing() + 10));

    // 6. Test Placement Mode (centered on erased area) and Collision Sensor state
    box.setPlacementMode(true, QPoint(240, 180));
    QVERIFY(box.isPlacementMode());
    QVERIFY(!box.inlineTextEdit()->isVisible());
    QCOMPARE(box.text(), QString("In-Place Sample"));
    QVERIFY(std::abs(box.boxRect().center().x() - 240) <= 2);
    QVERIFY(std::abs(box.boxRect().center().y() - 180) <= 2);

    QVERIFY(!box.hasCollision());
    box.setHasCollision(true);
    QVERIFY(box.hasCollision());
    box.setHasCollision(false);
    QVERIFY(!box.hasCollision());

    // 7. Test suggested width and topLeftPos in setPlacementMode
    box.setPlacementMode(true, QPoint(), 300, QPoint(100, 120));
    QCOMPARE(box.boxRect().topLeft(), QPoint(100, 120));
    QVERIFY(box.boxRect().width() >= 300);

    // 8. Test font resolution in ContextualActionBar
    ContextualActionBar fontBar;
    fontBar.setTextFont(QFont("FSMe", 14));
    QVERIFY(!fontBar.textFont().family().isEmpty());

    // 9. Test screenFont zoom scaling
    box.setScaleFactor(1.0);
    QCOMPARE(box.screenFont().pixelSize(), 14);
    box.setScaleFactor(2.0);
    QCOMPARE(box.screenFont().pixelSize(), 28);
    box.setScaleFactor(0.5);
    QCOMPARE(box.screenFont().pixelSize(), 7);

    // 10. Test inline editor reactivated on edit request
    box.setPlacementMode(false);
    QVERIFY(!box.isPlacementMode());
    QVERIFY(box.inlineTextEdit()->isVisible());
}

void TestPdfStudio::testZoomSystem()
{
    MinimalHeaderBar bar;
    bar.resize(800, 46);
    bar.show();

    // 1. Verify zoom button exists and initial factor is 1.0
    QVERIFY(bar.zoomButton() != nullptr);
    QCOMPARE(bar.zoomFactor(), 1.0);

    // 2. Set zoom factor and verify update
    bar.setZoomFactor(1.5);
    QCOMPARE(bar.zoomFactor(), 1.5);

    // 3. Verify zoom actions exist and emit requested signals
    QSignalSpy spyIn(&bar, &MinimalHeaderBar::zoomInRequested);
    QSignalSpy spyOut(&bar, &MinimalHeaderBar::zoomOutRequested);
    QSignalSpy spyFit(&bar, &MinimalHeaderBar::zoomFitRequested);
    QSignalSpy spyReset(&bar, &MinimalHeaderBar::zoomResetRequested);
    QSignalSpy spyPercent(&bar, &MinimalHeaderBar::zoomSetPercentRequested);

    QVERIFY(bar.zoomInAction() != nullptr);
    QVERIFY(bar.zoomOutAction() != nullptr);
    QVERIFY(bar.zoomFitAction() != nullptr);
    QVERIFY(bar.zoomResetAction() != nullptr);

    bar.zoomInAction()->trigger();
    QCOMPARE(spyIn.count(), 1);

    bar.zoomOutAction()->trigger();
    QCOMPARE(spyOut.count(), 1);

    bar.zoomFitAction()->trigger();
    QCOMPARE(spyFit.count(), 1);

    bar.zoomResetAction()->trigger();
    QCOMPARE(spyReset.count(), 1);

    // 4. Verify Trash button in view mode
    QVERIFY(bar.trashButton() != nullptr);
    QSignalSpy spyTrash(&bar, &MinimalHeaderBar::trashRequested);
    bar.setTitle("test_file.png");
    bar.setEditMode(false);
    QVERIFY(bar.trashButton()->isVisible());
    bar.trashButton()->click();
    QCOMPARE(spyTrash.count(), 1);
}

void TestPdfStudio::testSignatureDialogUnifiedFlow()
{
    PdfSignatureDialog dlg;
    dlg.resize(540, 500);
    dlg.show();
    QVERIFY(dlg.isVisible());

    // 1. Verify default tab is Saved Signatures (Index 0)
    auto *tabs = dlg.findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->currentIndex(), 0);

    // 2. Verify creation buttons exist
    QList<QPushButton*> buttons = dlg.findChildren<QPushButton*>();
    QPushButton *btnFreehand = nullptr;
    QPushButton *btnImage = nullptr;
    for (QPushButton *btn : buttons) {
        if (btn->text().contains(QStringLiteral("Add Freehand"))) {
            btnFreehand = btn;
        } else if (btn->text().contains(QStringLiteral("Add from Image"))) {
            btnImage = btn;
        }
    }
    QVERIFY(btnFreehand != nullptr);
    QVERIFY(btnImage != nullptr);

    // 3. Click Add Freehand button -> Tab should switch to 2 (Draw Freehand)
    btnFreehand->click();
    QCOMPARE(tabs->currentIndex(), 2);

    // 4. Click back button in Draw tab -> Tab should switch back to 0 (Saved Signatures)
    QPushButton *btnBack = nullptr;
    for (QPushButton *btn : dlg.findChildren<QPushButton*>()) {
        if (btn->text().contains(QStringLiteral("Back to Saved Signatures"))) {
            btnBack = btn;
            break;
        }
    }
    QVERIFY(btnBack != nullptr);
    btnBack->click();
    QCOMPARE(tabs->currentIndex(), 0);

    // 5. Capture snapshot of redesigned dialog for user inspection
    QString artDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7");
    QTest::qWait(50);
    dlg.grab().save(QStringLiteral("%1/signature_dialog_unified_saved.png").arg(artDir));

    dlg.close();
}

void TestPdfStudio::testExportAndImageExtractDialogs()
{
    QString artDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/4e70622e-50ab-4665-afcd-a70645769460");
    QString samplePdf = QStringLiteral("/home/davide/Progetti/BubezView/421_Guida turistica di Trieste.pdf");

    // 1. UnifiedExportDialog test
    UnifiedExportDialog expDlg(FileCategory::Pdf, samplePdf, 0, 9);
    expDlg.resize(520, 320);
    expDlg.show();
    QTest::qWait(100);

    QPushButton *btnExtract = nullptr;
    for (QPushButton *btn : expDlg.findChildren<QPushButton*>()) {
        if (btn->text().contains("Extract") || btn->text().contains("Estrai")) {
            btnExtract = btn;
            break;
        }
    }
    QVERIFY(btnExtract != nullptr);
    expDlg.grab().save(QStringLiteral("%1/unified_export_dialog_pdf.png").arg(artDir));

    // Test that clicking Extract Images returns UnifiedExportDialog::ExtractImages (code 2)
    QTimer::singleShot(50, btnExtract, &QPushButton::click);
    int res = expDlg.exec();
    QCOMPARE(res, static_cast<int>(UnifiedExportDialog::ExtractImages));

    // 2. PdfImageExtractDialog test
    if (QFile::exists(samplePdf)) {
        PdfImageExtractDialog extractDlg(samplePdf);
        extractDlg.resize(800, 600);
        extractDlg.show();
        // Wait for asynchronous pdfimages extraction to populate the thumbnail grid
        for (int w = 0; w < 60; ++w) {
            QTest::qWait(200);
            QListWidget *lw = extractDlg.findChild<QListWidget*>();
            if (lw && lw->count() > 0) {
                break;
            }
        }
        QListWidget *listWidget = extractDlg.findChild<QListWidget*>();
        QVERIFY(listWidget != nullptr);
        QVERIFY(listWidget->count() > 0);
        extractDlg.grab().save(QStringLiteral("%1/pdf_image_extract_dialog.png").arg(artDir));
    }
}

void TestPdfStudio::testUiSnapshots()
{
    PdfStudioPanel panel;
    panel.resize(340, 680);
    panel.show();

    QString artDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/4e70622e-50ab-4665-afcd-a70645769460");

    for (int i = 0; i < 5; ++i) {
        panel.setCurrentTab(i);
        QTest::qWait(50);
        QPixmap pix = panel.grab();
        pix.save(QStringLiteral("%1/pdf_studio_tab_%2.png").arg(artDir).arg(i));
    }

    // Capture Header Bar in View Mode (Light Theme - ModernLight)
    {
        MinimalHeaderBar viewLightBar;
        viewLightBar.applyThemePreset(ThemePreset::ModernLight);
        viewLightBar.setCategory(FileCategory::Pdf);
        viewLightBar.setEditMode(false);
        viewLightBar.resize(1150, 46);
        viewLightBar.show();
        QTest::qWait(50);
        viewLightBar.grab().save(QStringLiteral("%1/header_view_light_theme.png").arg(artDir));
    }

    // Capture Header Bar in View Mode (Dark Theme - DarkSlate)
    {
        MinimalHeaderBar viewDarkBar;
        viewDarkBar.applyThemePreset(ThemePreset::DarkSlate);
        viewDarkBar.setCategory(FileCategory::Pdf);
        viewDarkBar.setEditMode(false);
        viewDarkBar.resize(1150, 46);
        viewDarkBar.show();
        QTest::qWait(50);
        viewDarkBar.grab().save(QStringLiteral("%1/header_view_dark_theme.png").arg(artDir));
    }

    // Capture Header Bar in Edit Mode (Light Theme - ModernLight)
    {
        MinimalHeaderBar lightBar;
        lightBar.applyThemePreset(ThemePreset::ModernLight);
        lightBar.setCategory(FileCategory::Pdf);
        lightBar.setEditMode(true);
        lightBar.resize(1150, 46);
        lightBar.show();
        QTest::qWait(50);
        lightBar.grab().save(QStringLiteral("%1/editor_header_light_theme.png").arg(artDir));
    }

    // Capture Header Bar in Edit Mode (Dark Theme - DarkSlate)
    {
        MinimalHeaderBar darkBar;
        darkBar.applyThemePreset(ThemePreset::DarkSlate);
        darkBar.setCategory(FileCategory::Pdf);
        darkBar.setEditMode(true);
        darkBar.resize(1150, 46);
        darkBar.show();
        QTest::qWait(50);
        darkBar.grab().save(QStringLiteral("%1/editor_header_dark_theme.png").arg(artDir));
    }

    // Capture Header Bar showing new Zoom control
    MinimalHeaderBar zoomHeader;
    zoomHeader.setCategory(FileCategory::Pdf);
    zoomHeader.setZoomFactor(1.25);
    zoomHeader.resize(950, 46);
    zoomHeader.show();
    QTest::qWait(50);
    zoomHeader.grab().save(QStringLiteral("%1/header_bar_zoom.png").arg(artDir));

    // Capture Header Bar in Page Organizer mode showing the new dedicated Repair PDF pill
    MinimalHeaderBar orgHeader;
    orgHeader.setCategory(FileCategory::Pdf);
    orgHeader.setEditMode(true);
    orgHeader.setPageOrganizerMode(true);
    orgHeader.resize(1100, 46);
    orgHeader.show();
    QTest::qWait(50);
    orgHeader.grab().save(QStringLiteral("/home/davide/.gemini/antigravity/brain/4e70622e-50ab-4665-afcd-a70645769460/organizer_header_repair_btn.png"));

    // Capture Header Bar with Undo & Timeline buttons and Contextual Text Bar
    MinimalHeaderBar header;
    header.setCategory(FileCategory::Pdf);
    header.setEditMode(true);
    header.resize(1100, 88);
    header.showContextualTextBar();
    header.show();
    QTest::qWait(50);
    header.grab().save(QStringLiteral("%1/header_undo_timeline_bar.png").arg(artDir));

    // Capture Stamp Contextual Bar
    header.showContextualStampBar();
    QTest::qWait(50);
    header.grab().save(QStringLiteral("%1/header_bar_pdf_stamp_bar_new.png").arg(artDir));

    // Capture Redesigned Visual History Timeline Carousel with 160x230 slides and 220x275 active selection
    PdfHistoryWidget historyWidget;
    QList<PdfHistoryStep> steps;

    PdfHistoryStep s0; s0.index = 0; s0.description = "Original Document"; s0.timestamp = QDateTime::currentDateTime().addSecs(-120); s0.isCurrent = false;
    PdfHistoryStep s1; s1.index = 1; s1.description = "Yellow Highlight"; s1.timestamp = QDateTime::currentDateTime().addSecs(-60); s1.isCurrent = false;
    PdfHistoryStep s2; s2.index = 2; s2.description = "Edit Text Header"; s2.timestamp = QDateTime::currentDateTime().addSecs(-20); s2.isCurrent = true;
    PdfHistoryStep s3; s3.index = 3; s3.description = "Add Redaction"; s3.timestamp = QDateTime::currentDateTime(); s3.isCurrent = false;

    steps << s0 << s1 << s2 << s3;
    historyWidget.setHistorySteps(steps);
    historyWidget.resize(920, 360);
    historyWidget.show();
    QTest::qWait(50);
    historyWidget.grab().save(QStringLiteral("%1/history_timeline_carousel.png").arg(artDir));

    // Capture styled QFileDialog in DarkSlate theme
    ThemeManager::setPreset(ThemePreset::DarkSlate);
    ThemeManager::applyTheme(qApp, nullptr);
    QFileDialog fileDialog;
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog.setDirectory(QStringLiteral("/home/davide/Progetti/BubezView"));
    ThemeManager::configureFileDialog(&fileDialog);
    fileDialog.resize(800, 500);
    fileDialog.show();
    QTest::qWait(100);
    fileDialog.grab().save(QStringLiteral("%1/styled_file_dialog_dark.png").arg(artDir));

    // Capture styled QFileDialog in BreezeDark theme
    ThemeManager::setPreset(ThemePreset::BreezeDark);
    ThemeManager::applyTheme(qApp, nullptr);
    QFileDialog fileDialogBreeze;
    fileDialogBreeze.setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialogBreeze.setDirectory(QStringLiteral("/home/davide/Progetti/BubezView"));
    ThemeManager::configureFileDialog(&fileDialogBreeze);
    fileDialogBreeze.resize(800, 500);
    fileDialogBreeze.show();
    QTest::qWait(100);
    fileDialogBreeze.grab().save(QStringLiteral("%1/styled_file_dialog_breeze_dark.png").arg(artDir));
}

QTEST_MAIN(TestPdfStudio)
#include "test_pdf_studio.moc"
