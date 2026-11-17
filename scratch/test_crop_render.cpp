#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/scratch/saved_with_stamp.pdf");
    if (!doc) return 1;
    auto page = doc->page(0);
    if (!page) return 1;

    // Page size in points
    QSizeF ptSize = page->pageSizeF();
    std::cout << "Page point size: " << ptSize.width() << "x" << ptSize.height() << "\n";

    // Point rect corresponding to the stamp (0.1, 0.1, 0.5, 0.5)
    QRectF pointRect(ptSize.width() * 0.1, ptSize.height() * 0.1, ptSize.width() * 0.5, ptSize.height() * 0.5);
    std::cout << "pointRect: " << pointRect.x() << "," << pointRect.y() << " " << pointRect.width() << "x" << pointRect.height() << "\n";

    // Current code in PdfViewer::onOcrAreaRequested:
    double scale = 300.0 / 72.0;
    int x = qRound(pointRect.x() * scale);
    int y = qRound(pointRect.y() * scale);
    int w = qRound(pointRect.width() * scale);
    int h = qRound(pointRect.height() * scale);

    std::cout << "renderToImage params: x=" << x << " y=" << y << " w=" << w << " h=" << h << "\n";

    QImage snippet = page->renderToImage(300.0, 300.0, x, y, w, h);
    std::cout << "Snippet size: " << snippet.width() << "x" << snippet.height() << "\n";
    snippet.save("/home/davide/Progetti/BubezView/scratch/snippet_rendered.png");

    return 0;
}
