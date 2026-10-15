/**
 * @file pdf_document_ops.cpp
 * @brief Implementation of modular backend operations for PDF documents.
 */

#include "pdf_document_ops.h"
#include <poppler/qt6/poppler-qt6.h>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QtGui/QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QTextStream>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

bool PdfDocumentOps::insertBlankPage(const QString &sourceFilePath,
                                     int totalPages,
                                     int targetIndex,
                                     QString &outFilePath,
                                     QString &errorMessage)
{
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }

    // 1. Generate an empty A4 PDF document using QPdfWriter
    QTemporaryFile blankFile(QDir::tempPath() + "/bubez_blank_XXXXXX.pdf");
    blankFile.setAutoRemove(false);
    if (!blankFile.open()) {
        errorMessage = QObject::tr("Failed to create temporary blank page file.");
        return false;
    }
    QString blankPath = blankFile.fileName();
    blankFile.close();

    {
        QPdfWriter writer(blankPath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.end();
    }

    // 2. Insert blank page using QPDF
    QTemporaryFile outFile(QDir::tempPath() + "/bubez_inserted_XXXXXX.pdf");
    outFile.setAutoRemove(false);
    if (!outFile.open()) {
        QFile::remove(blankPath);
        errorMessage = QObject::tr("Failed to create output file for page insertion.");
        return false;
    }
    outFilePath = outFile.fileName();
    outFile.close();

    QStringList args;
    args << sourceFilePath << "--pages";

    int after1Based = targetIndex + 1;
    if (after1Based <= 0) {
        args << blankPath << "1" << sourceFilePath << "1-z";
    } else if (after1Based >= totalPages) {
        args << sourceFilePath << "1-z" << blankPath << "1";
    } else {
        args << sourceFilePath << QString("1-%1").arg(after1Based)
             << blankPath << "1"
             << sourceFilePath << QString("%1-z").arg(after1Based + 1);
    }
    args << "--" << outFilePath;

    int ret = QProcess::execute("qpdf", args);
    QFile::remove(blankPath);

    if (ret != 0 && ret != 3) {
        QFile::remove(outFilePath);
        outFilePath.clear();
        errorMessage = QObject::tr("Could not insert page. Please ensure qpdf is installed on your system.");
        return false;
    }

    return true;
}


bool PdfDocumentOps::exportPage(Poppler::Page *page,
                               const QString &format,
                               const QString &outputPath,
                               int dpi,
                               int rotationDegrees)
{
    if (!page) return false;

    QString fmt = format.toLower();
    if (fmt == "txt") {
        QFile f(outputPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&f);
        out << page->text(QRectF());
        return true;
    }

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (rotationDegrees == 270) r = Poppler::Page::Rotate270;

    QImage img = page->renderToImage(dpi, dpi, -1, -1, -1, -1, r);
    if (img.isNull()) return false;

    return img.save(outputPath);
}

int PdfDocumentOps::extractEmbeddedImages(const QString &filePath, const QString &targetDir)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return -1;
    }

    QDir().mkpath(targetDir);

    // Call pdfimages utility (prioritizes locally built binary if present)
    QString appDir = QCoreApplication::applicationDirPath();
    QString localExe = appDir + "/pdfimages";
    if (!QFile::exists(localExe)) {
        localExe = appDir + "/_deps/poppler-build/utils/pdfimages";
    }
    QString exe = (QFile::exists(localExe) && QFileInfo(localExe).isExecutable()) ? localExe : "pdfimages";

    QString prefix = QDir(targetDir).filePath("extracted_img");
    QStringList args;
    args << "-png" << filePath << prefix;

    int exitCode = QProcess::execute(exe, args);
    if (exitCode != 0) {
        // Fallback: try without -png (native format)
        args.clear();
        args << filePath << prefix;
        exitCode = QProcess::execute(exe, args);
    }

    if (exitCode == 0) {
        QDir dir(targetDir);
        QStringList extracted = dir.entryList(QStringList() << "extracted_img*.*", QDir::Files);
        return extracted.size();
    }

    return -1;
}

bool PdfDocumentOps::saveDocumentWithRotation(Poppler::Document *doc,
                                             const QString &currentFilePath,
                                             const QString &targetFilePath,
                                             int &rotationDegrees)
{
    if (!doc || targetFilePath.isEmpty()) return false;

    bool isSameFile = (QFileInfo(currentFilePath).canonicalFilePath() == QFileInfo(targetFilePath).canonicalFilePath());
    QString outPath = targetFilePath;
    if (isSameFile) {
        outPath = currentFilePath + ".tmp." + QString::number(QCoreApplication::applicationPid()) + ".pdf";
    }

    std::unique_ptr<Poppler::PDFConverter> conv = doc->pdfConverter();
    if (!conv) return false;

    conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
    conv->setOutputFileName(outPath);
    bool ok = conv->convert();
    if (!ok) {
        if (isSameFile && QFile::exists(outPath)) QFile::remove(outPath);
        return false;
    }

    // Optimize PDF streams with QPDF (compress Flate streams, deduplicate and generate object streams)
    // and permanently bake viewport rotation into page dictionaries if rotated.
    QString optTemp = outPath + ".opt.pdf";
    QStringList qpdfArgs;
    if (rotationDegrees != 0) {
        qpdfArgs << QStringLiteral("--rotate=+%1:1-z").arg(rotationDegrees);
    }
    qpdfArgs << QStringLiteral("--recompress-flate")
             << QStringLiteral("--object-streams=generate")
             << outPath
             << optTemp;

    int exitCode = QProcess::execute(QStringLiteral("qpdf"), qpdfArgs);
    if (exitCode == 0 && QFile::exists(optTemp)) {
        QFile::remove(outPath);
        QFile::rename(optTemp, outPath);
        if (isSameFile && rotationDegrees != 0) {
            rotationDegrees = 0;
        }
    } else {
        if (QFile::exists(optTemp)) QFile::remove(optTemp);
    }

    if (isSameFile) {
        QFile::remove(currentFilePath);
        if (!QFile::rename(outPath, currentFilePath)) {
            return false;
        }
    }

    return true;
}
