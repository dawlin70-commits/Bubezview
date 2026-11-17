#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/scratch/saved_with_stamp.pdf");
    auto page = doc->page(0);

    // Test with Rotate90
    double scale = 300.0 / 72.0;
    int x = 100, y = 100, w = 400, h = 400;

    QImage imgRot0 = page->renderToImage(300.0, 300.0, x, y, w, h, Poppler::Page::Rotate0);
    QImage imgRot90 = page->renderToImage(300.0, 300.0, x, y, w, h, Poppler::Page::Rotate90);

    std::cout << "imgRot0 size: " << imgRot0.width() << "x" << imgRot0.height() << "\n";
    std::cout << "imgRot90 size: " << imgRot90.width() << "x" << imgRot90.height() << "\n";
    return 0;
}
