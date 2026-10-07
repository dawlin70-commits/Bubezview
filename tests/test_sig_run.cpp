
#include <QApplication>
#include <QDebug>
#include "src/ui/pdf_signature_dialog.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    PdfSignatureDialog dlg;
    dlg.show();
    qDebug() << "Signature dialog shown successfully!";
    return 0;
}
