#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/scratch/saved_with_stamp.pdf");
    auto page = doc->page(0);

    // Suppose logical page size is 559 x 794
    int logicalW = 559;
    int logicalH = 794;

    // 1. Normal 96 DPI render:
    QImage normalImg = page->renderToImage(96.0, 96.0);
    // Draw normalImg to canvas
    QImage canvas1(logicalW, logicalH, QImage::Format_ARGB32);
    canvas1.fill(Qt::white);
    {
        QPainter p(&canvas1);
        p.drawImage(0, 0, normalImg);
    }
    canvas1.save("/home/davide/Progetti/BubezView/scratch/canvas_normal_96.png");

    // 2. Supersampled 2x (192 DPI) render:
    double factor = 2.0;
    QImage superImg = page->renderToImage(96.0 * factor, 96.0 * factor);
    superImg.setDevicePixelRatio(factor);

    QImage canvas2(logicalW, logicalH, QImage::Format_ARGB32);
    canvas2.fill(Qt::white);
    {
        QPainter p(&canvas2);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawImage(0, 0, superImg);
    }
    canvas2.save("/home/davide/Progetti/BubezView/scratch/canvas_supersampled_192.png");

    std::cout << "Saved canvas_normal_96.png and canvas_supersampled_192.png\n";
    return 0;
}
