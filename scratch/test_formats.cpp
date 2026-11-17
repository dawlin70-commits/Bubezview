#include <poppler/qt6/poppler-qt6.h>
#include <QGuiApplication>
#include <QImage>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/Catalogo-SKY_p1.pdf");
    auto page = doc->page(0);

    QImage img1(100, 100, QImage::Format_RGB888);
    img1.fill(Qt::blue);
    auto *stamp1 = new Poppler::StampAnnotation();
    stamp1->setStampCustomImage(img1);
    stamp1->setBoundary(QRectF(0.1, 0.1, 0.2, 0.2));
    page->addAnnotation(stamp1);
    delete stamp1;

    std::cout << "Added RGB888 stamp successfully\n";
    return 0;
}
