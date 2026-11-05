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
    void testRenderSnapshot();

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
    QVERIFY(viewer.loadFile(m_testPdfPath));

    double initialZoom = viewer.zoomFactor();
    viewer.zoomIn();
    QVERIFY(viewer.zoomFactor() > initialZoom);

    viewer.zoomOut();
    viewer.resetZoom();
    QCOMPARE(viewer.zoomFactor(), 1.0);
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

void TestPdfBase::testRenderSnapshot()
{
    QString triestePdf = QStringLiteral("421_Guida turistica di Trieste.pdf");
    if (!QFile::exists(triestePdf)) {
        triestePdf = m_testPdfPath;
    }
    PdfBaseViewer viewer;
    viewer.resize(1100, 750);
    viewer.loadFile(triestePdf);
    viewer.show();
    QCoreApplication::processEvents();

    QMouseEvent moveEv(QEvent::MouseMove, QPointF(550, 400), QPointF(550, 400), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewer, &moveEv);
    QCoreApplication::processEvents();

    QPixmap pix = viewer.grab();
    QString shotPath = QStringLiteral("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/pdf_base_viewer_live.png");
    pix.save(shotPath, "PNG");
}

QTEST_MAIN(TestPdfBase)
#include "test_pdf_base.moc"
