#include <QGuiApplication>
#include <QImage>
#include <iostream>

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    for (const char *name : {"canvas_normal_96.png", "canvas_supersampled_192.png"}) {
        QImage im(QString("/home/davide/Progetti/BubezView/scratch/") + name);
        int w = im.width();
        int h = im.height();
        QImage crop = im.copy(0.1*w, 0.1*h, 0.5*w, 0.5*h);
        crop.save(QString("/home/davide/Progetti/BubezView/scratch/crop_") + name);
    }
    return 0;
}
