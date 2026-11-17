#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/Catalogo-SKY_p1.pdf");
    if (!doc) return 1;
    auto page = doc->page(0);
    if (!page) return 1;

    // Load an image with fine text or create one
    QImage testImg(1200, 800, QImage::Format_ARGB32);
    testImg.fill(Qt::white);
    {
        QPainter p(&testImg);
        p.setPen(Qt::black);
        QFont f = p.font();
        f.setPixelSize(20);
        p.setFont(f);
        for (int y = 30; y < 780; y += 30) {
            p.drawText(20, y, QString("Sample fine text line at y=%1: The quick brown fox jumps over the lazy dog 1234567890").arg(y));
        }
    }

    // Normalized rect on page: say 0.1, 0.1, 0.8, 0.6
    QRectF normRect(0.1, 0.1, 0.8, 0.6);

    auto *stamp = new Poppler::StampAnnotation();
    stamp->setStampCustomImage(testImg);
    stamp->setBoundary(normRect);
    page->addAnnotation(stamp);
    delete stamp;

    // 1. Render at default 96 DPI
    doc->setRenderHint(Poppler::Document::Antialiasing, true);
    doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    // Let's check what other render hints Poppler has!
    QImage rendered96 = page->renderToImage(96.0, 96.0);
    rendered96.save("/home/davide/Progetti/BubezView/scratch/rendered_96dpi.png");

    // 2. Render at 150 DPI
    QImage rendered150 = page->renderToImage(150.0, 150.0);
    rendered150.save("/home/davide/Progetti/BubezView/scratch/rendered_150dpi.png");

    // 3. Render at 300 DPI
    QImage rendered300 = page->renderToImage(300.0, 300.0);
    rendered300.save("/home/davide/Progetti/BubezView/scratch/rendered_300dpi.png");

    std::cout << "Rendered at 96, 150, 300 DPI\n";
    return 0;
}
