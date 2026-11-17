/**
 * @file test_pdf_base.cpp
 * @brief Automated unit test for PdfBaseViewer lightweight native Poppler engine.
 */

#include <QTest>
#include <QObject>
#include <QTemporaryFile>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>

#include "viewers/pdf/pdf_base_viewer.h"
#include "viewers/pdf/pdf_base_canvas.h"
#include "viewers/pdf/pdf_canvas_tools.h"
#include "viewers/pdf/pdf_document_ops.h"

class TestPdfBase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testInitialState();
    void testLoadSamplePdf();
    void testNavigation();
    void testZoom();
    void testTextExtractionAndSearch();
    void testOcrRendering();
    void testPageExport();
    void testMixedPageSizesAndFit();
    void testRenderSnapshot();
    void testEditModeAndOverlay();
    void testShapeRecognitionAndTransforms();
    void testApplyStampsWithMuPdf();

private:
    QString m_testPdfPath;
    QTemporaryFile m_tempFile;
};

void TestPdfBase::initTestCase()
{
    // Generate a minimal valid 3-page PDF document dynamically
    m_tempFile.setFileTemplate(QDir::tempPath() + "/test_pdf_base_XXXXXX.pdf");
    QVERIFY(m_tempFile.open());
    m_testPdfPath = m_tempFile.fileName();
    m_tempFile.close();

    QPdfWriter writer(m_testPdfPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(72);

    QPainter painter(&writer);
    painter.drawText(100, 100, "BubezView Base PDF Viewer - Page 1 Sample Text");
    writer.newPage();
    painter.drawText(100, 100, "BubezView Base PDF Viewer - Page 2 Searchable Keyword");
    writer.newPage();
    painter.drawText(100, 100, "BubezView Base PDF Viewer - Page 3 Final Content");
    painter.end();

    QVERIFY(QFile::exists(m_testPdfPath));
}

void TestPdfBase::cleanupTestCase()
{
    if (QFile::exists(m_testPdfPath)) {
        QFile::remove(m_testPdfPath);
    }
}

void TestPdfBase::testInitialState()
{
    PdfBaseViewer viewer;
    QCOMPARE(viewer.pageCount(), 0);
    QCOMPARE(viewer.currentPage(), 0);
    QCOMPARE(viewer.zoomFactor(), 1.0);
    QVERIFY(!viewer.hasTextSelection());
    QVERIFY(!viewer.hasOutline());
    QVERIFY(!viewer.isEditMode());
}

void TestPdfBase::testLoadSamplePdf()
{
    PdfBaseViewer viewer;
    bool loaded = viewer.loadFile(m_testPdfPath);
    QVERIFY(loaded);
    QCOMPARE(viewer.pageCount(), 3);
    QCOMPARE(viewer.currentPage(), 0);
    QVERIFY(viewer.pageSize().width() > 0);
    QVERIFY(viewer.pageSize().height() > 0);
}

void TestPdfBase::testNavigation()
{
    PdfBaseViewer viewer;
    QVERIFY(viewer.loadFile(m_testPdfPath));

    viewer.nextPage();
    QCOMPARE(viewer.currentPage(), 1);

    viewer.nextPage();
    QCOMPARE(viewer.currentPage(), 2);

    // Cannot advance past last page
    viewer.nextPage();
    QCOMPARE(viewer.currentPage(), 2);

    viewer.prevPage();
    QCOMPARE(viewer.currentPage(), 1);

    viewer.goToPage(0);
    QCOMPARE(viewer.currentPage(), 0);
}

void TestPdfBase::testZoom()
{
    PdfBaseViewer viewer;
    viewer.resize(800, 600);
    QVERIFY(viewer.loadFile(m_testPdfPath));
    viewer.show();
    QCoreApplication::processEvents();

    double initialZoom = viewer.zoomFactor();
    viewer.zoomIn();
    QVERIFY(viewer.zoomFactor() > initialZoom);

    // Test zoomAt
    viewer.zoomAt(QPoint(400, 300), 1.5);
    QVERIFY(viewer.zoomFactor() > initialZoom * 1.25);

    viewer.zoomOut();
    viewer.resetZoom();
    QCOMPARE(viewer.zoomFactor(), 1.0);

    // Test Hand Tool & Right Click disable
    viewer.toggleZoomTool(true);
    QVERIFY(viewer.isZoomToolActive());

    auto *canvas = viewer.findChild<PdfBaseCanvas*>();
    QVERIFY(canvas != nullptr);
    QVERIFY(canvas->isHandToolActive());
    QCOMPARE(canvas->cursor().shape(), Qt::OpenHandCursor);

    // Simulate right-click on canvas to deactivate hand tool immediately
    QMouseEvent rightClickPress(QEvent::MouseButtonPress, QPointF(200, 200), QPointF(200, 200),
                                Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &rightClickPress);
    QVERIFY(!canvas->isHandToolActive());
    QVERIFY(!viewer.isZoomToolActive());
    QCOMPARE(canvas->cursor().shape(), Qt::IBeamCursor);

    // Test Rotation
    QCOMPARE(viewer.rotationDegrees(), 0);
    viewer.rotateClockwise();
    QCOMPARE(viewer.rotationDegrees(), 90);
    viewer.rotateClockwise();
    QCOMPARE(viewer.rotationDegrees(), 180);
    viewer.rotateCounterClockwise();
    QCOMPARE(viewer.rotationDegrees(), 90);
    viewer.rotateCounterClockwise();
    QCOMPARE(viewer.rotationDegrees(), 0);
}

void TestPdfBase::testTextExtractionAndSearch()
{
    PdfBaseViewer viewer;
    QVERIFY(viewer.loadFile(m_testPdfPath));

    QString allText = viewer.extractAllText();
    QVERIFY(!allText.isEmpty());
    QVERIFY(allText.contains("BubezView"));

    bool wrapped = false;
    bool found = viewer.findNext("Searchable", false, &wrapped);
    QVERIFY(found);
    viewer.clearSearch();
}

void TestPdfBase::testOcrRendering()
{
    PdfBaseViewer viewer;
    QVERIFY(viewer.loadFile(m_testPdfPath));

    QImage pageImg = viewer.renderPageForOcr(0, 150);
    QVERIFY(!pageImg.isNull());
    QVERIFY(pageImg.width() > 100);
    QVERIFY(pageImg.height() > 100);

    QImage curImg = viewer.renderCurrentPageForOcr(150);
    QVERIFY(!curImg.isNull());
}

void TestPdfBase::testPageExport()
{
    PdfBaseViewer viewer;
    QVERIFY(viewer.loadFile(m_testPdfPath));

    QString outPdf = QDir::tempPath() + "/test_pdf_base_page0.pdf";
    QString outPng = QDir::tempPath() + "/test_pdf_base_page0.png";

    bool savedPdf = viewer.saveCurrentPageAsPdf(outPdf);
    QVERIFY(savedPdf);
    QVERIFY(QFile::exists(outPdf));
    QFile::remove(outPdf);

    bool savedPng = viewer.saveCurrentPageAsImage(outPng, "PNG", 150);
    QVERIFY(savedPng);
    QVERIFY(QFile::exists(outPng));
    QFile::remove(outPng);
}

void TestPdfBase::testMixedPageSizesAndFit()
{
    QTemporaryFile mixedFile;
    mixedFile.setFileTemplate(QDir::tempPath() + "/test_mixed_pdf_XXXXXX.pdf");
    QVERIFY(mixedFile.open());
    QString mixedPath = mixedFile.fileName();
    mixedFile.close();

    {
        QPdfWriter writer(mixedPath);
        writer.setPageSize(QPageSize(QPageSize::A5));
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.drawText(50, 50, "Small Page A5");
        writer.setPageSize(QPageSize(QPageSize::A3));
        writer.newPage();
        painter.drawText(100, 100, "Large Page A3");
        painter.end();
    }

    PdfBaseViewer viewer;
    viewer.resize(1000, 800);
    QVERIFY(viewer.loadFile(mixedPath));
    viewer.show();
    QCoreApplication::processEvents();

    viewer.fitToPage();
    QCoreApplication::processEvents();

    QCOMPARE(viewer.pageCount(), 2);
    auto *canvas = viewer.findChild<PdfBaseCanvas*>();
    QVERIFY(canvas != nullptr);
    QCOMPARE(canvas->pageCount(), 2);

    QRect r0 = canvas->pageRect(0);
    QRect r1 = canvas->pageRect(1);
    QVERIFY(r0.isValid());
    QVERIFY(r1.isValid());

    // Both pages must fit inside viewport bounds (<= 800 height, <= 1000 width)
    QVERIFY(r0.height() <= 800);
    QVERIFY(r1.height() <= 800);
    QVERIFY(r0.width() <= 1000);
    QVERIFY(r1.width() <= 1000);

    // Test Fit to Width mode
    viewer.fitToWidth();
    QCoreApplication::processEvents();
    QRect w0 = canvas->pageRect(0);
    QRect w1 = canvas->pageRect(1);
    QVERIFY(w0.isValid());
    QVERIFY(w1.isValid());
    QVERIFY(w0.width() <= 1000 && w0.width() >= 900);
    QVERIFY(w1.width() <= 1000 && w1.width() >= 900);

    QFile::remove(mixedPath);
}

void TestPdfBase::testRenderSnapshot()
{
    QString testPdf = QStringLiteral("/home/davide/Progetti/Catalogo-SKY_Repaired_Repaired.pdf");
    if (!QFile::exists(testPdf)) {
        testPdf = m_testPdfPath;
    }
    PdfBaseViewer viewer;
    viewer.resize(1100, 750);
    viewer.loadFile(testPdf);
    viewer.show();
    QCoreApplication::processEvents();

    QMouseEvent moveEv(QEvent::MouseMove, QPointF(550, 400), QPointF(550, 400), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &moveEv);
    QCoreApplication::processEvents();

    QPixmap pix = viewer.grab();
    QVERIFY(!pix.isNull());
    QString shotPath = QDir::tempPath() + "/test_pdf_base_snap1.png";
    pix.save(shotPath, "PNG");
    if (QFile::exists(shotPath)) {
        QFile::remove(shotPath);
    }

    if (viewer.pageCount() > 3) {
        viewer.scrollToPage(3);
        QCoreApplication::processEvents();
        QPixmap pix4 = viewer.grab();
        QVERIFY(!pix4.isNull());
        QString shotPath4 = QDir::tempPath() + "/test_pdf_base_snap4.png";
        pix4.save(shotPath4, "PNG");
        if (QFile::exists(shotPath4)) {
            QFile::remove(shotPath4);
        }
    }
}

void TestPdfBase::testEditModeAndOverlay()
{
    PdfBaseViewer viewer;
    viewer.resize(800, 600);
    QVERIFY(viewer.loadFile(m_testPdfPath));
    viewer.show();
    QCoreApplication::processEvents();

    QVERIFY(!viewer.isEditMode());
    auto *canvas = viewer.canvas();
    QVERIFY(canvas != nullptr);
    QVERIFY(!canvas->isEditMode());

    // Coordinate conversions
    QRect p0Rect = canvas->pageRect(0);
    QVERIFY(p0Rect.isValid());
    int pageIdx = canvas->pageAtPos(p0Rect.center());
    QCOMPARE(pageIdx, 0);

    QPointF pagePt = canvas->mapToPagePoint(0, p0Rect.center());
    QVERIFY(pagePt.x() > 0 && pagePt.y() > 0);

    QPoint canvasPt = canvas->mapFromPagePoint(0, pagePt);
    QVERIFY(qAbs(canvasPt.x() - p0Rect.center().x()) <= 1);
    QVERIFY(qAbs(canvasPt.y() - p0Rect.center().y()) <= 1);

    QRectF ptRect(50, 50, 100, 100);
    QRect mappedRect = canvas->mapFromPageRect(0, ptRect);
    QVERIFY(mappedRect.isValid());
    QVERIFY(mappedRect.width() > 0 && mappedRect.height() > 0);

    // Overlay attachment
    auto *overlay = new QWidget();
    viewer.setEditorOverlay(overlay);
    QCOMPARE(viewer.editorOverlay(), overlay);
    QCOMPARE(canvas->editorOverlay(), overlay);

    // Enter edit mode: cursor should immediately switch from Hand to IBeam/Text selection
    viewer.setEditMode(true);
    QVERIFY(viewer.isEditMode());
    QVERIFY(canvas->isEditMode());
    QVERIFY(!canvas->isHandToolActive());
    QCOMPARE(canvas->cursor().shape(), Qt::IBeamCursor);
    QVERIFY(overlay->isVisible());

    // Exit edit mode: cursor should restore to Hand / Pan mode
    viewer.setEditMode(false);
    QVERIFY(!viewer.isEditMode());
    QVERIFY(!canvas->isEditMode());
    QVERIFY(canvas->isHandToolActive());
    QCOMPARE(canvas->cursor().shape(), Qt::OpenHandCursor);
    QVERIFY(!overlay->isVisible());
}

void TestPdfBase::testShapeRecognitionAndTransforms()
{
    // 1. Test Poppler annotation coordinate and rotation transform
    QImage testImg(100, 50, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(Qt::blue);
    QRectF normRect(0.1, 0.2, 0.3, 0.4);

    // 0 degrees
    auto xform0 = PdfCanvasTools::transformForPoppler(normRect, testImg, 0);
    QCOMPARE(xform0.boundary, normRect);
    QCOMPARE(xform0.image.size(), testImg.size());

    // 90 degrees
    auto xform90 = PdfCanvasTools::transformForPoppler(normRect, testImg, 90);
    QCOMPARE(xform90.boundary, normRect);
    QCOMPARE(xform90.image.size(), QSize(50, 100)); // rotated 270 deg (or 90 deg counter-clockwise) for poppler

    // 2. Test Smart Shape Freehand Recognition
    // Circle points
    QVector<QPoint> circlePoints;
    for (int deg = 0; deg <= 360; deg += 10) {
        double rad = deg * M_PI / 180.0;
        circlePoints.append(QPoint(qRound(150 + 60 * std::cos(rad)), qRound(150 + 60 * std::sin(rad))));
    }
    auto recCircle = PdfCanvasTools::recognizeSmartShape(circlePoints);
    QCOMPARE(recCircle.shapeType, 1); // Ellipse / Circle

    // Triangle points
    QVector<QPoint> trianglePoints;
    for (int i = 0; i <= 20; ++i) trianglePoints.append(QPoint(100 + i * 5, 200 - i * 5)); // to top
    for (int i = 0; i <= 20; ++i) trianglePoints.append(QPoint(200 + i * 5, 100 + i * 5)); // to right
    for (int i = 0; i <= 40; ++i) trianglePoints.append(QPoint(300 - i * 5, 200)); // back to left
    auto recTri = PdfCanvasTools::recognizeSmartShape(trianglePoints);
    QCOMPARE(recTri.shapeType, 5); // Triangle

    // Line points
    QVector<QPoint> linePoints;
    for (int x = 50; x <= 250; x += 10) linePoints.append(QPoint(x, 100));
    auto recLine = PdfCanvasTools::recognizeSmartShape(linePoints);
    QCOMPARE(recLine.shapeType, 3); // Line

    // 3. Test Shape Rendering with Fill (Rect, Circle, Triangle, Star, Rounded Rect)
    QColor fillCol(255, 0, 0, 255);
    QColor strokeCol(0, 0, 0, 255);

    // Circle (1) with fill: center pixel MUST be filled with fillCol
    QImage circleImg = PdfCanvasTools::renderShapeStamp(1, 2, strokeCol, fillCol, QSize(100, 100));
    QVERIFY(!circleImg.isNull());
    QColor centerColorCircle = circleImg.pixelColor(50, 50);
    QCOMPARE(centerColorCircle.red(), 255);
    QCOMPARE(centerColorCircle.green(), 0);
    QCOMPARE(centerColorCircle.blue(), 0);
    QVERIFY(centerColorCircle.alpha() > 200);

    // Rectangle (0) with fill: center pixel MUST be filled with fillCol
    QImage rectImg = PdfCanvasTools::renderShapeStamp(0, 2, strokeCol, fillCol, QSize(100, 100));
    QVERIFY(!rectImg.isNull());
    QColor centerColorRect = rectImg.pixelColor(50, 50);
    QCOMPARE(centerColorRect.red(), 255);
    QCOMPARE(centerColorRect.green(), 0);
    QCOMPARE(centerColorRect.blue(), 0);
    QVERIFY(centerColorRect.alpha() > 200);

    // Triangle (5) with fill: centroid MUST be filled with fillCol
    QImage triImg = PdfCanvasTools::renderShapeStamp(5, 2, strokeCol, fillCol, QSize(120, 120));
    QVERIFY(!triImg.isNull());
    QColor centerColorTri = triImg.pixelColor(60, 75);
    QCOMPARE(centerColorTri.red(), 255);
    QCOMPARE(centerColorTri.green(), 0);
    QCOMPARE(centerColorTri.blue(), 0);
    QVERIFY(centerColorTri.alpha() > 200);
}

void TestPdfBase::testApplyStampsWithMuPdf()
{
    // Create a stamp image
    QImage redBox(100, 100, QImage::Format_ARGB32_Premultiplied);
    redBox.fill(QColor(239, 68, 68)); // #ef4444 red

    PdfDocumentOps::ImageStampItem item;
    item.pageIndex = 0;
    item.image = redBox;
    item.normalizedRect = QRectF(0.2, 0.2, 0.3, 0.3);

    QString outPath;
    QString errorMsg;
    bool ok = PdfDocumentOps::applyStampsWithMuPdf(m_testPdfPath, {item}, outPath, errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QVERIFY(!outPath.isEmpty());
    QVERIFY(QFile::exists(outPath));

    // Verify the output PDF contains the baked stamp image
    PdfBaseViewer viewer;
    QVERIFY(viewer.loadFile(outPath));
    QCOMPARE(viewer.pageCount(), 3);

    // Render page 0 and inspect pixel at stamped region
    QImage rendered = viewer.renderPageForOcr(0, 72);
    QVERIFY(!rendered.isNull());

    int sampleX = qRound(rendered.width() * 0.35);
    int sampleY = qRound(rendered.height() * 0.35);
    QColor sampledCol = rendered.pixelColor(sampleX, sampleY);

    // Must be close to #ef4444 (red > 200, green < 100, blue < 100)
    QVERIFY(sampledCol.red() > 200);
    QVERIFY(sampledCol.green() < 100);
    QVERIFY(sampledCol.blue() < 100);

    QFile::remove(outPath);
}

QTEST_MAIN(TestPdfBase)
#include "test_pdf_base.moc"
