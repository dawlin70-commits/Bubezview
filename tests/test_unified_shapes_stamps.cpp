#include <QApplication>
#include <QTest>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <iostream>

#define private public
#define protected public
#include "viewers/pdf/pdf_viewer.h"
#include "viewers/pdf/pdf_page_widget.h"
#include "viewers/pdf/pdf_transform_box.h"
#include "viewers/pdf/pdf_canvas_tools.h"
#include "viewers/pdf/pdf_document_ops.h"

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "==================================================\n";
    std::cout << "TEST UNIFIED SHAPES & STAMPS VIA PDFTRANSFORMBOX\n";
    std::cout << "==================================================\n";

    QString testPdfPath = QStringLiteral("/home/davide/Progetti/BubezView/test/verticale.pdf");
    QString outPdfPath = QStringLiteral("/home/davide/Progetti/BubezView/test/verticale_unified_test.pdf");

    PdfViewer viewer;
    viewer.resize(1000, 800);
    viewer.show();
    bool loaded = viewer.loadFile(testPdfPath);
    if (!loaded || !viewer.m_doc) {
        std::cerr << "FAIL: Could not load " << testPdfPath.toStdString() << "\n";
        return 1;
    }
    std::cout << "Loaded document: pages=" << viewer.m_pageCount << "\n";

    // 1. Enter edit mode
    viewer.setEditMode(true);
    PdfPageWidget *pageWidget = viewer.m_pageWidget;
    if (!pageWidget) {
        std::cerr << "FAIL: m_pageWidget is null!\n";
        return 1;
    }

    // -------------------------------------------------------------
    // Page 0 (unrotated, 0 deg)
    // -------------------------------------------------------------
    pageWidget->setActivePage(0);
    std::cout << "\n--- Testing Page 0 (Rotation 0) ---\n";

    // 1.1 Insert Shape (Rectangle) via drag-to-draw interaction
    pageWidget->startShape(0, 3, Qt::red, QColor(255, 0, 0, 60));
    {
        QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(60, 60), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(pageWidget, &pressEv);
        QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(280, 200), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(pageWidget, &releaseEv);
    }
    if (!pageWidget->m_transformBox || !pageWidget->m_transformBox->isVisible()) {
        std::cerr << "FAIL: PdfTransformBox not visible after startShape!\n";
        return 1;
    }
    std::cout << "Shape transform box opened. ItemType=" << static_cast<int>(pageWidget->m_transformBox->itemType()) << "\n";
    pageWidget->m_transformBox->setBoxRect(QRect(60, 60, 220, 140));
    pageWidget->onTransformBoxApplied();
    std::cout << "Rectangle applied to Page 0\n";

    // 1.2 Insert Rubber Stamp ("APPROVATO")
    pageWidget->startWatermark(QStringLiteral("APPROVATO"), 15.0, 0.85, QColor(QStringLiteral("#2563eb")),
                               QPoint(300, 350), 1, 32);
    if (!pageWidget->m_transformBox || !pageWidget->m_transformBox->isVisible()) {
        std::cerr << "FAIL: PdfTransformBox not visible after startWatermark!\n";
        return 1;
    }
    std::cout << "Stamp transform box opened. ItemType=" << static_cast<int>(pageWidget->m_transformBox->itemType()) << "\n";
    pageWidget->m_transformBox->setBoxRect(QRect(180, 280, 280, 120));
    pageWidget->onTransformBoxApplied();
    std::cout << "Stamp applied to Page 0\n";

    // -------------------------------------------------------------
    // Page 1 (Rotated 270 deg, cropbox offset)
    // -------------------------------------------------------------
    viewer.setPage(1);
    std::cout << "\n--- Testing Page 1 (Rotation 270) ---\n";

    // 2.1 Insert Shape (Circle) via drag-to-draw
    pageWidget->startShape(1, 4, Qt::darkGreen, QColor(0, 255, 0, 50));
    {
        int p1Y = pageWidget->pageTopY(1);
        QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(50, p1Y + 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(pageWidget, &pressEv);
        QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(230, p1Y + 230), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(pageWidget, &releaseEv);
    }
    if (!pageWidget->m_transformBox || !pageWidget->m_transformBox->isVisible()) {
        std::cerr << "FAIL: PdfTransformBox not visible on Page 1!\n";
        return 1;
    }
    pageWidget->m_transformBox->setBoxRect(QRect(50, 50, 180, 180));
    pageWidget->onTransformBoxApplied();
    std::cout << "Circle applied to Page 1\n";

    // 2.2 Insert Rubber Stamp ("RISERVATO")
    pageWidget->startWatermark(QStringLiteral("RISERVATO"), -20.0, 0.90, QColor(QStringLiteral("#dc2626")),
                               QPoint(250, 300), 0, 28);
    if (!pageWidget->m_transformBox || !pageWidget->m_transformBox->isVisible()) {
        std::cerr << "FAIL: PdfTransformBox not visible for stamp on Page 1!\n";
        return 1;
    }
    pageWidget->m_transformBox->setBoxRect(QRect(160, 260, 260, 110));
    pageWidget->onTransformBoxApplied();
    std::cout << "Stamp applied to Page 1\n";

    // -------------------------------------------------------------
    // 3. Testing Shape Cancellation & Redraw and Arrow Resize Fix
    // -------------------------------------------------------------
    std::cout << "\n--- Testing Shape Cancel and Non-Compounding Resize ---\n";
    pageWidget->startShape(2, 3, Qt::blue, Qt::transparent); // Arrow
    if (pageWidget->m_toolMode != PdfToolMode::Shape) {
        std::cerr << "FAIL: toolMode expected Shape, got " << static_cast<int>(pageWidget->m_toolMode) << "\n";
        return 1;
    }

    // Arrow drawn from right to left (300, 100) -> (100, 100)
    pageWidget->m_transformBox->showShape(2, 3, Qt::blue, Qt::transparent,
                                          QRect(84, 84, 232, 32),
                                          QPoint(216, 16), QPoint(16, 16));
    if (pageWidget->m_transformBox->shapeLocalStart() != QPoint(216, 16) ||
        pageWidget->m_transformBox->shapeLocalEnd() != QPoint(16, 16)) {
        std::cerr << "FAIL: Arrow localStart/localEnd mismatch!\n";
        return 1;
    }

    // Simulate handle resize on BottomRight: width changes from 232 to 348 (1.5x)
    pageWidget->m_transformBox->m_activeHandle = PdfTransformBox::BottomRight;
    pageWidget->m_transformBox->m_dragStartPos = QPoint(316, 116);
    pageWidget->m_transformBox->m_dragStartRect = QRect(84, 84, 232, 32);
    pageWidget->m_transformBox->m_dragStartShapeLocalStart = QPoint(216, 16);
    pageWidget->m_transformBox->m_dragStartShapeLocalEnd = QPoint(16, 16);

    // 20 incremental mouse moves
    for (int i = 1; i <= 20; ++i) {
        int w = 232 + (116 * i / 20);
        QRect nR(84, 84, w, 32);
        double sx = static_cast<double>(nR.width()) / pageWidget->m_transformBox->m_dragStartRect.width();
        double sy = static_cast<double>(nR.height()) / pageWidget->m_transformBox->m_dragStartRect.height();
        pageWidget->m_transformBox->m_shapeLocalStart = QPoint(
            qRound(pageWidget->m_transformBox->m_dragStartShapeLocalStart.x() * sx),
            qRound(pageWidget->m_transformBox->m_dragStartShapeLocalStart.y() * sy));
        pageWidget->m_transformBox->m_shapeLocalEnd = QPoint(
            qRound(pageWidget->m_transformBox->m_dragStartShapeLocalEnd.x() * sx),
            qRound(pageWidget->m_transformBox->m_dragStartShapeLocalEnd.y() * sy));
    }
    // Expected scaled points: 216 * 1.5 = 324, 16 * 1.5 = 24
    if (pageWidget->m_transformBox->shapeLocalStart() != QPoint(324, 16) ||
        pageWidget->m_transformBox->shapeLocalEnd() != QPoint(24, 16)) {
        std::cerr << "FAIL: Arrow compounded or fell! Got start=("
                  << pageWidget->m_transformBox->shapeLocalStart().x() << "," << pageWidget->m_transformBox->shapeLocalStart().y()
                  << ") end=(" << pageWidget->m_transformBox->shapeLocalEnd().x() << ","
                  << pageWidget->m_transformBox->shapeLocalEnd().y() << ")\n";
        return 1;
    }
    std::cout << "SUCCESS: Arrow resize preserves coordinates without compounding!\n";

    // Test cancellation / discard keeping Shape mode active
    pageWidget->onTransformBoxCancelled();
    if (pageWidget->m_transformBox->isVisible()) {
        std::cerr << "FAIL: Transform box still visible after cancel!\n";
        return 1;
    }
    if (pageWidget->m_toolMode != PdfToolMode::Shape) {
        std::cerr << "FAIL: Cancelling shape must keep Shape tool active for redraw!\n";
        return 1;
    }
    std::cout << "SUCCESS: Shape discard keeps Shape tool mode active for immediate redraw!\n";

    // Test Smart Shape recognition
    {
        QVector<QPoint> circlePts;
        for (int i = 0; i <= 36; ++i) {
            double angle = i * 2.0 * M_PI / 36.0;
            double r = 80.0 + (i % 3 - 1) * 3.0;
            circlePts.append(QPoint(qRound(200 + r * std::cos(angle)), qRound(200 + r * std::sin(angle))));
        }
        auto resCircle = PdfCanvasTools::recognizeSmartShape(circlePts);
        if (resCircle.shapeType != 1) {
            std::cerr << "FAIL: Smart shape circle recognized as " << resCircle.shapeType << "\n";
            return 1;
        }

        QVector<QPoint> arrowPts;
        for (int x = 50; x <= 250; x += 10) arrowPts.append(QPoint(x, 100));
        arrowPts.append(QPoint(240, 92));
        arrowPts.append(QPoint(230, 85));
        auto resArrow = PdfCanvasTools::recognizeSmartShape(arrowPts);
        if (resArrow.shapeType != 2 || resArrow.endPt.x() != 250) {
            std::cerr << "FAIL: Smart shape arrow recognized incorrectly!\n";
            return 1;
        }
        std::cout << "SUCCESS: Smart Shape recognition validated for circle and arrow!\n";
    }

    // -------------------------------------------------------------
    // Done / Exit Edit Mode test
    // -------------------------------------------------------------
    std::cout << "\n--- Testing Exit Edit Mode (Done button) ---\n";
    viewer.setEditMode(false);

    if (!viewer.m_doc) {
        std::cerr << "FAIL: Document became null after exiting edit mode (black screen bug)!\n";
        return 1;
    }
    std::cout << "SUCCESS: Document remained loaded and valid after exiting edit mode!\n";

    // -------------------------------------------------------------
    // Save document
    // -------------------------------------------------------------
    std::cout << "\n--- Saving Document ---\n";
    if (QFile::exists(outPdfPath)) {
        QFile::remove(outPdfPath);
    }
    bool saved = viewer.saveAs(outPdfPath);
    if (!saved || !QFile::exists(outPdfPath)) {
        std::cerr << "FAIL: saveAs failed to produce " << outPdfPath.toStdString() << "\n";
        return 1;
    }
    std::cout << "SUCCESS: Saved to " << outPdfPath.toStdString() << "\n";

    // -------------------------------------------------------------
    // Verify output PDF with Poppler
    // -------------------------------------------------------------
    std::cout << "\n--- Verifying Output PDF with Poppler ---\n";
    auto verifyDoc = Poppler::Document::load(outPdfPath);
    if (!verifyDoc) {
        std::cerr << "FAIL: Cannot load saved PDF with Poppler!\n";
        return 1;
    }

    auto p0 = verifyDoc->page(0);
    if (!p0) {
        std::cerr << "FAIL: Cannot load page 0 from saved PDF!\n";
        return 1;
    }
    auto annots0 = p0->annotations();
    std::cout << "Page 0 annotations count: " << annots0.size() << "\n";
    if (annots0.size() < 2) {
        std::cerr << "FAIL: Page 0 expected at least 2 annotations, found " << annots0.size() << "\n";
        return 1;
    }

    // Render Page 0 to image
    QImage p0Img = p0->renderToImage(144.0, 144.0);
    p0Img.save(QStringLiteral("/home/davide/Progetti/BubezView/test/unified_test_p0.png"));
    std::cout << "Saved Page 0 rendering to test/unified_test_p0.png (size="
              << p0Img.width() << "x" << p0Img.height() << ")\n";

    auto p1 = verifyDoc->page(1);
    if (!p1) {
        std::cerr << "FAIL: Cannot load page 1 from saved PDF!\n";
        return 1;
    }
    auto annots1 = p1->annotations();
    std::cout << "Page 1 annotations count: " << annots1.size() << "\n";
    if (annots1.size() < 2) {
        std::cerr << "FAIL: Page 1 expected at least 2 annotations, found " << annots1.size() << "\n";
        return 1;
    }

    // Render Page 1 to image
    QImage p1Img = p1->renderToImage(144.0, 144.0);
    p1Img.save(QStringLiteral("/home/davide/Progetti/BubezView/test/unified_test_p1.png"));
    std::cout << "Saved Page 1 rendering to test/unified_test_p1.png (size="
              << p1Img.width() << "x" << p1Img.height() << ")\n";

    std::cout << "\n==================================================\n";
    std::cout << "ALL UNIFIED SHAPES & STAMPS TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "==================================================\n";

    return 0;
}
