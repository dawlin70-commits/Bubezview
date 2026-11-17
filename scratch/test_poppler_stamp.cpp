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
    if (!doc) {
        std::cerr << "Failed to open PDF\n";
        return 1;
    }
    auto page = doc->page(0);
    if (!page) {
        std::cerr << "Failed to get page\n";
        return 1;
    }

    // Create high-res test image with text
    QImage testImg(1000, 1000, QImage::Format_ARGB32);
    testImg.fill(Qt::white);
    {
        QPainter p(&testImg);
        p.setPen(Qt::black);
        QFont f = p.font();
        f.setPixelSize(40);
        p.setFont(f);
        p.drawText(50, 100, "High-Res OCR Test 1234567890 ABCDEFG");
    }

    auto *stamp = new Poppler::StampAnnotation();
    stamp->setStampCustomImage(testImg);
    stamp->setBoundary(QRectF(0.1, 0.1, 0.4, 0.4));
    page->addAnnotation(stamp);
    delete stamp;

    // Render page at 300 DPI
    QImage rendered = page->renderToImage(300.0, 300.0);
    std::cout << "Rendered size: " << rendered.width() << "x" << rendered.height() << "\n";

    // Crop the stamp area in rendered image (0.1, 0.1, 0.4, 0.4)
    QRect cropRect(rendered.width() * 0.1, rendered.height() * 0.1, rendered.width() * 0.4, rendered.height() * 0.4);
    QImage cropped = rendered.copy(cropRect);
    cropped.save("/home/davide/Progetti/BubezView/scratch/poppler_stamp_crop.png");
    std::cout << "Cropped size: " << cropped.width() << "x" << cropped.height() << "\n";

    return 0;
}
