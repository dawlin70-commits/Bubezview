#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/Catalogo-SKY_p1.pdf");
    if (!doc) return 1;
    auto page = doc->page(0);
    if (!page) return 1;

    // Create high-res test image with tiny text (mimicking a scanned doc)
    QImage testImg(2000, 1500, QImage::Format_ARGB32);
    testImg.fill(Qt::white);
    {
        QPainter p(&testImg);
        p.setPen(Qt::black);
        QFont f = p.font();
        f.setPixelSize(24);
        p.setFont(f);
        p.drawText(50, 50, "Micro Text Line 1: Quick brown fox jumps over the lazy dog");
        p.drawText(50, 100, "Micro Text Line 2: The resolution should be 300 DPI or higher");
    }
    testImg.save("/home/davide/Progetti/BubezView/scratch/original_test_img.png");

    auto *stamp = new Poppler::StampAnnotation();
    stamp->setStampCustomImage(testImg);
    stamp->setBoundary(QRectF(0.1, 0.1, 0.5, 0.5));
    page->addAnnotation(stamp);
    delete stamp;

    // Save modified PDF
    auto converter = doc->pdfConverter();
    converter->setOutputFileName("/home/davide/Progetti/BubezView/scratch/saved_with_stamp.pdf");
    converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
    bool ok = converter->convert();
    std::cout << "Saved PDF: " << (ok ? "SUCCESS" : "FAIL") << "\n";

    return 0;
}
