#include <QGuiApplication>
#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <QDebug>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/test/verticale_backup.pdf");
    if (!doc) {
        qDebug() << "Cannot open pdf";
        return 1;
    }
    auto page = doc->page(0);

    // Desired on-screen box:
    // Screen page size is 596 x 842.
    // User wants text at X=100, Y=250, W=380, H=60
    double screenW = 596.0;
    double screenH = 842.0;
    double sx = 100.0 / screenW;
    double sy = 250.0 / screenH;
    double sw = 380.0 / screenW;
    double sh = 60.0 / screenH;

    // Render crisp unrotated text image for the screen box (380 x 60)
    int scale = 3;
    QImage screenImg(380 * scale, 60 * scale, QImage::Format_ARGB32_Premultiplied);
    screenImg.fill(Qt::transparent);
    QPainter p(&screenImg);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    QFont f("Sans Serif", 24 * scale, QFont::Bold);
    p.setFont(f);
    p.setPen(QColor("#fbbf24")); // gold
    p.drawText(screenImg.rect(), Qt::AlignLeft | Qt::AlignVCenter, "ciao Davide");
    p.end();

    // Since page->orientation() == Seascape (270 deg clockwise):
    // 1. Image on the page must be rotated by +90 deg so when the page is displayed at 270 deg,
    // (90 + 270) % 360 = 0 (perfectly horizontal)!
    QTransform tf;
    tf.rotate(90);
    QImage pageStampImg = screenImg.transformed(tf, Qt::SmoothTransformation);

    // 2. Coordinate transformation from screen [0, 1] to page [0, 1]:
    // For 270 deg clockwise rotation:
    // px = sy
    // py = 1.0 - (sx + sw)
    // pw = sh
    // ph = sw
    QRectF pageNormRect(sy, 1.0 - (sx + sw), sh, sw);

    auto *stamp = new Poppler::StampAnnotation();
    stamp->setStampCustomImage(pageStampImg);
    stamp->setBoundary(pageNormRect);
    page->addAnnotation(stamp);
    delete stamp;

    // Save and render
    auto conv = doc->pdfConverter();
    conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
    conv->setOutputFileName("/home/davide/Progetti/BubezView/scratch/fixed_coordinate.pdf");
    conv->convert();

    auto docReload = Poppler::Document::load("/home/davide/Progetti/BubezView/scratch/fixed_coordinate.pdf");
    // Render at Rotate0 (as BubezView does)
    QImage rendered0 = docReload->page(0)->renderToImage(150, 150, -1, -1, -1, -1, Poppler::Page::Rotate0);
    rendered0.save("/home/davide/Progetti/BubezView/scratch/fixed_rendered0.png");

    // Also render at Rotate270
    QImage rendered270 = docReload->page(0)->renderToImage(150, 150, -1, -1, -1, -1, Poppler::Page::Rotate270);
    rendered270.save("/home/davide/Progetti/BubezView/scratch/fixed_rendered270.png");

    qDebug() << "Test executed successfully!";
    return 0;
}
