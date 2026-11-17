#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <QDebug>

int main() {
    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/test/verticale.pdf");
    auto page = doc->page(0);
    auto annots = page->annotations();
    qDebug() << "Total annots on page 0:" << annots.size();
    for (auto &a : annots) {
        qDebug() << "subType:" << a->subType() << "boundary:" << a->boundary() << "flags:" << (int)a->flags();
    }
    return 0;
}
