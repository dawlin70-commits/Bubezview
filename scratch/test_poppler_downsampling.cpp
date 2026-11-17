#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // Create a 2000x1500 high-res image with sharp text
    QImage highRes(2000, 1500, QImage::Format_ARGB32);
    highRes.fill(Qt::white);
    {
        QPainter p(&highRes);
        p.setPen(Qt::black);
        QFont f = p.font();
        f.setPixelSize(36);
        f.setBold(true);
        p.setFont(f);
        for (int y = 50; y < 1450; y += 60) {
            p.drawText(50, y, QString("Sample fine sharp text line %1: Testing PDF Image Quality").arg(y / 60));
        }
    }
    highRes.save("/home/davide/Progetti/BubezView/scratch/sample_highres.png");

    // What PdfTransformBox showed on screen: target size 500x375 (1/4 scale)
    QSize targetBoxSize(500, 375);
    QImage qtScaled = highRes.scaled(targetBoxSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    qtScaled.save("/home/davide/Progetti/BubezView/scratch/qt_smooth_scaled.png");

    // Now what Poppler does:
    auto doc = Poppler::Document::load("/home/davide/Progetti/Catalogo-SKY_p1.pdf");
    auto page = doc->page(0);

    // Suppose the page is 595x842 pt.
    // At 100% zoom (scaleX = 96/72 = 1.333), the page on screen is 793 x 1122 px.
    // The box is 500x375 px, which corresponds to normRect:
    QRectF normRect(0.1, 0.1, 500.0 / 793.0, 375.0 / 1122.0);

    auto *stamp = new Poppler::StampAnnotation();
    stamp->setStampCustomImage(highRes);
    stamp->setBoundary(normRect);
    page->addAnnotation(stamp);
    delete stamp;

    // Poppler renders page at 96 DPI
    QImage page96 = page->renderToImage(96.0, 96.0);
    // Crop the stamp area from page96
    QRect stampPix(qRound(normRect.x() * page96.width()),
                   qRound(normRect.y() * page96.height()),
                   qRound(normRect.width() * page96.width()),
                   qRound(normRect.height() * page96.height()));
    QImage popplerCrop = page96.copy(stampPix);
    popplerCrop.save("/home/davide/Progetti/BubezView/scratch/poppler_stamp_at_96dpi.png");

    std::cout << "Qt scaled size: " << qtScaled.width() << "x" << qtScaled.height() << "\n";
    std::cout << "Poppler crop size: " << popplerCrop.width() << "x" << popplerCrop.height() << "\n";

    return 0;
}
