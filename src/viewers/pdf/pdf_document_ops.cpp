/**
 * @file pdf_document_ops.cpp
 * @brief Implementation of modular backend operations for PDF documents.
 */

#include "pdf_document_ops.h"
#include "core/system/build_config.h"
#include <poppler/qt6/poppler-qt6.h>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QtGui/QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QTextStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFontDatabase>
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

    QString qpdfBin = BuildConfig::qpdfExecutable();
    if (qpdfBin.isEmpty()) qpdfBin = QStringLiteral("qpdf");
    int ret = QProcess::execute(qpdfBin, args);
    QFile::remove(blankPath);

    if (ret != 0 && ret != 3) {
        QFile::remove(outFilePath);
        outFilePath.clear();
        errorMessage = QObject::tr("Could not insert page. Please ensure qpdf is installed on your system.");
        return false;
    }

    return true;
}

bool PdfDocumentOps::deletePage(const QString &sourceFilePath,
                                int totalPages,
                                int pageIndex,
                                QString &outFilePath,
                                QString &errorMessage)
{
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }
    if (totalPages <= 1) {
        errorMessage = QObject::tr("Cannot delete the only page in the document.");
        return false;
    }

    QTemporaryFile outFile(QDir::tempPath() + QStringLiteral("/bubez_deleted_XXXXXX.pdf"));
    outFile.setAutoRemove(false);
    if (!outFile.open()) {
        errorMessage = QObject::tr("Failed to create output file for page deletion.");
        return false;
    }
    outFilePath = outFile.fileName();
    outFile.close();

    int del1Based = pageIndex + 1;
    QStringList args;
    args << sourceFilePath << QStringLiteral("--pages") << sourceFilePath;

    if (del1Based <= 1) {
        args << QStringLiteral("2-z");
    } else if (del1Based >= totalPages) {
        args << QStringLiteral("1-%1").arg(totalPages - 1);
    } else {
        args << QStringLiteral("1-%1,%2-z").arg(del1Based - 1).arg(del1Based + 1);
    }
    args << QStringLiteral("--") << outFilePath;

    QString qpdfBin = BuildConfig::qpdfExecutable();
    if (qpdfBin.isEmpty()) qpdfBin = QStringLiteral("qpdf");
    int ret = QProcess::execute(qpdfBin, args);
    if (ret != 0 && ret != 3) {
        QFile::remove(outFilePath);
        outFilePath.clear();
        errorMessage = QObject::tr("Could not delete page. Please ensure qpdf is installed on your system.");
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

bool PdfDocumentOps::extractSinglePageToPdf(const QString &sourceFilePath,
                                           int pageIndex0Based,
                                           const QString &outFilePath,
                                           QString &errorMessage)
{
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }

    QString qpdfExe = BuildConfig::qpdfExecutable();
    if (qpdfExe.isEmpty()) {
        errorMessage = QObject::tr("QPDF utility is not installed on this system.");
        return false;
    }

    int pageNum1Based = pageIndex0Based + 1;
    QProcess proc;
    QStringList args;
    args << "--warning-exit-0" << sourceFilePath << "--pages" << sourceFilePath << QString::number(pageNum1Based) << "--" << outFilePath;
    proc.start(qpdfExe, args);
    proc.waitForFinished(10000);

    int exitCode = proc.exitCode();
    if ((exitCode == 0 || exitCode == 3) && QFile::exists(outFilePath) && QFileInfo(outFilePath).size() > 0) {
        return true;
    }

    errorMessage = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    if (errorMessage.isEmpty()) {
        errorMessage = QObject::tr("QPDF extraction process failed with code %1.").arg(exitCode);
    }
    return false;
}

int PdfDocumentOps::extractEmbeddedImages(const QString &filePath, const QString &targetDir)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return -1;
    }

    QDir().mkpath(targetDir);

    // Call pdfimages utility (prioritizes locally built binary if present)
    QString exe = BuildConfig::pdfimagesExecutable();
    if (exe.isEmpty()) {
        exe = QStringLiteral("pdfimages");
    }

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

    QString qpdfExe = BuildConfig::qpdfExecutable();
    int exitCode = -1;
    if (!qpdfExe.isEmpty()) {
        exitCode = QProcess::execute(qpdfExe, qpdfArgs);
    }
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

bool PdfDocumentOps::removeVectorTextFromPage(const QString &sourceFilePath,
                                             int pageIndex,
                                             const QRectF &pointRect,
                                             QString &outFilePath,
                                             QString &errorMessage)
{
    outFilePath.clear();
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }

    QString pyBin = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyBin.isEmpty()) {
        errorMessage = QObject::tr("Python 3 executable not found.");
        return false;
    }

    QTemporaryFile outFile(QDir::tempPath() + "/bubez_vector_redact_XXXXXX.pdf");
    outFile.setAutoRemove(false);
    if (!outFile.open()) {
        errorMessage = QObject::tr("Failed to create temporary output file for redaction.");
        return false;
    }
    outFilePath = outFile.fileName();
    outFile.close();

    const QString script = QStringLiteral(
        "import sys, os\n"
        "try:\n"
        "    import fitz\n"
        "    doc = fitz.open(sys.argv[1])\n"
        "    p = doc[int(sys.argv[2])]\n"
        "    target_vis = fitz.Rect(float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6]))\n"
        "    annots = []\n"
        "    for w in p.get_text('words'):\n"
        "        w_fitz = fitz.Rect(w[:4])\n"
        "        w_vis = w_fitz * p.rotation_matrix\n"
        "        cx = (w_vis.x0 + w_vis.x1) * 0.5\n"
        "        cy = (w_vis.y0 + w_vis.y1) * 0.5\n"
        "        if target_vis.x0 <= cx <= target_vis.x1 and target_vis.y0 <= cy <= target_vis.y1:\n"
        "            dx = min(0.8, w_vis.width * 0.08)\n"
        "            dy = w_vis.height * 0.32\n"
        "            core_vis = fitz.Rect(w_vis.x0 + dx, w_vis.y0 + dy, w_vis.x1 - dx, w_vis.y1 - dy)\n"
        "            annots.append(p.add_redact_annot(core_vis * p.derotation_matrix, fill=False))\n"
        "    if not annots:\n"
        "        dy = target_vis.height * 0.25\n"
        "        core_vis = fitz.Rect(target_vis.x0, target_vis.y0 + dy, target_vis.x1, target_vis.y1 - dy)\n"
        "        annots.append(p.add_redact_annot(core_vis * p.derotation_matrix, fill=False))\n"
        "    p.apply_redactions(images=fitz.PDF_REDACT_IMAGE_NONE)\n"
        "    doc.save(sys.argv[7])\n"
        "    os._exit(0)\n"
        "except Exception as e:\n"
        "    sys.stderr.write(f'Vector redaction error: {e}\\n')\n"
        "    sys.stderr.flush()\n"
        "    os._exit(1)\n"
    );

    QStringList args;
    args << QStringLiteral("-c") << script
         << sourceFilePath
         << QString::number(pageIndex)
         << QString::number(pointRect.left(), 'f', 4)
         << QString::number(pointRect.top(), 'f', 4)
         << QString::number(pointRect.right(), 'f', 4)
         << QString::number(pointRect.bottom(), 'f', 4)
         << outFilePath;

    QProcess proc;
    proc.start(pyBin, args);
    bool finished = proc.waitForFinished(4000);

    if (!finished || proc.exitCode() != 0 || !QFile::exists(outFilePath) || QFileInfo(outFilePath).size() == 0) {
        if (QFile::exists(outFilePath)) {
            QFile::remove(outFilePath);
        }
        outFilePath.clear();
        QString errOutput = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        errorMessage = !errOutput.isEmpty()
            ? errOutput
            : QObject::tr("Vector redaction engine (PyMuPDF) unavailable or encountered an error.");
        return false;
    }

    return true;
}

bool PdfDocumentOps::applyStampsWithMuPdf(const QString &sourceFilePath,
                                         const QList<ImageStampItem> &items,
                                         QString &outFilePath,
                                         QString &errorMessage)
{
    errorMessage.clear();
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }
    if (items.isEmpty()) {
        outFilePath = sourceFilePath;
        return true;
    }

    QString pyBin = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyBin.isEmpty()) {
        pyBin = QStandardPaths::findExecutable(QStringLiteral("python"));
    }
    if (pyBin.isEmpty()) {
        errorMessage = QObject::tr("Python 3 is required for PDF stamping but was not found.");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        errorMessage = QObject::tr("Failed to create temporary working directory.");
        return false;
    }

    QJsonArray jsonItems;
    for (int i = 0; i < items.size(); ++i) {
        const auto &it = items[i];
        if (it.image.isNull() || !it.normalizedRect.isValid()) continue;

        QString imgPath = tempDir.filePath(QStringLiteral("stamp_%1.png").arg(i));
        if (!it.image.save(imgPath, "PNG")) {
            continue;
        }

        QJsonObject obj;
        obj[QStringLiteral("page")] = it.pageIndex;
        obj[QStringLiteral("image")] = imgPath;
        obj[QStringLiteral("x")] = it.normalizedRect.x();
        obj[QStringLiteral("y")] = it.normalizedRect.y();
        obj[QStringLiteral("w")] = it.normalizedRect.width();
        obj[QStringLiteral("h")] = it.normalizedRect.height();
        jsonItems.append(obj);
    }

    if (jsonItems.isEmpty()) {
        outFilePath = sourceFilePath;
        return true;
    }

    QString manifestPath = tempDir.filePath(QStringLiteral("stamps.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)) {
        errorMessage = QObject::tr("Failed to write stamp manifest file.");
        return false;
    }
    manifestFile.write(QJsonDocument(jsonItems).toJson(QJsonDocument::Compact));
    manifestFile.close();

    if (outFilePath.isEmpty()) {
        outFilePath = QDir::tempPath() + QStringLiteral("/bubez_baked_%1.pdf")
                          .arg(QDateTime::currentMSecsSinceEpoch());
    }

    const QString script = QStringLiteral(
        "import fitz\n"
        "import json\n"
        "import sys\n"
        "try:\n"
        "    with open(sys.argv[2], 'r', encoding='utf-8') as f:\n"
        "        stamps = json.load(f)\n"
        "    doc = fitz.open(sys.argv[1])\n"
        "    cleaned_pages = set()\n"
        "    for it in stamps:\n"
        "        p_idx = int(it.get('page', 0))\n"
        "        if p_idx < 0 or p_idx >= len(doc):\n"
        "            continue\n"
        "        p = doc[p_idx]\n"
        "        if p_idx not in cleaned_pages:\n"
        "            p.clean_contents()\n"
        "            cleaned_pages.add(p_idx)\n"
        "        w = p.rect.width\n"
        "        h = p.rect.height\n"
        "        rx = p.rect.x0 + float(it.get('x', 0.0)) * w\n"
        "        ry = p.rect.y0 + float(it.get('y', 0.0)) * h\n"
        "        rw = float(it.get('w', 0.0)) * w\n"
        "        rh = float(it.get('h', 0.0)) * h\n"
        "        vis = fitz.Rect(rx, ry, rx + rw, ry + rh)\n"
        "        native = vis * ~p.rotation_matrix\n"
        "        p.insert_image(native, filename=it['image'], rotate=p.rotation)\n"
        "    doc.save(sys.argv[3], garbage=3, deflate=True)\n"
        "    doc.close()\n"
        "    sys.exit(0)\n"
        "except Exception as e:\n"
        "    sys.stderr.write(str(e) + '\\n')\n"
        "    sys.stderr.flush()\n"
        "    sys.exit(1)\n"
    );

    QStringList args;
    args << QStringLiteral("-c") << script
         << sourceFilePath
         << manifestPath
         << outFilePath;

    QProcess proc;
    proc.start(pyBin, args);
    bool finished = proc.waitForFinished(7000);

    if (!finished || proc.exitCode() != 0 || !QFile::exists(outFilePath) || QFileInfo(outFilePath).size() == 0) {
        if (QFile::exists(outFilePath)) {
            QFile::remove(outFilePath);
        }
        outFilePath.clear();
        QString errOutput = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        errorMessage = !errOutput.isEmpty()
            ? errOutput
            : QObject::tr("PyMuPDF engine failed to bake stamps into PDF.");
        return false;
    }

    return true;
}

QString PdfDocumentOps::resolveFontFamily(const QString &rawFontName, bool &outBold, bool &outItalic)
{
    QString name = rawFontName;
    int plusIdx = name.indexOf('+');
    if (plusIdx != -1) {
        name = name.mid(plusIdx + 1);
    }

    if (name.contains(QStringLiteral("Bold"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Heavy"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Black"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("W6"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("W7"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("W8"), Qt::CaseInsensitive)) {
        outBold = true;
    }
    if (name.contains(QStringLiteral("Italic"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Oblique"), Qt::CaseInsensitive)) {
        outItalic = true;
    }

    const auto availableFamilies = QFontDatabase::families();
    for (const QString &fam : availableFamilies) {
        if (fam.compare(name, Qt::CaseInsensitive) == 0) {
            return fam;
        }
    }

    int hyphenIdx = name.indexOf('-');
    if (hyphenIdx != -1) {
        QString prefixName = name.left(hyphenIdx);
        for (const QString &fam : availableFamilies) {
            if (fam.compare(prefixName, Qt::CaseInsensitive) == 0) {
                return fam;
            }
        }
        name = prefixName;
    }

    static const QStringList suffixes = {
        QStringLiteral(" Regular"), QStringLiteral(" regular"),
        QStringLiteral(" Bold"), QStringLiteral(" bold"),
        QStringLiteral(" Italic"), QStringLiteral(" italic"),
        QStringLiteral(" Oblique"), QStringLiteral(" oblique"),
        QStringLiteral(" Medium"), QStringLiteral(" medium")
    };
    for (const QString &sfx : suffixes) {
        if (name.endsWith(sfx, Qt::CaseInsensitive)) {
            name.chop(sfx.length());
        }
    }
    QString trimmed = name.trimmed();
    if (trimmed.compare(QStringLiteral("SegoeUI"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Segoe UI");
    } else if (trimmed.compare(QStringLiteral("NotoSans"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Noto Sans");
    } else if (trimmed.compare(QStringLiteral("LiberationSans"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Liberation Sans");
    } else if (trimmed.compare(QStringLiteral("LiberationSerif"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Liberation Serif");
    }

    for (const QString &fam : availableFamilies) {
        if (fam.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return fam;
        }
    }

    return trimmed;
}

bool PdfDocumentOps::inspectTextFormat(const QString &sourceFilePath,
                                      int pageIndex,
                                      const QRectF &pointRect,
                                      TextFormatInfo &outFormat,
                                      QString &errorMessage)
{
    outFormat = TextFormatInfo();
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        errorMessage = QObject::tr("Source PDF file does not exist.");
        return false;
    }

    QString pyBin = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyBin.isEmpty()) {
        errorMessage = QObject::tr("Python 3 executable not found.");
        return false;
    }

    const QString script = QStringLiteral(
        "import sys, os, json\n"
        "try:\n"
        "    import fitz\n"
        "    doc = fitz.open(sys.argv[1])\n"
        "    p = doc[int(sys.argv[2])]\n"
        "    target_vis = fitz.Rect(float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6]))\n"
        "    spans = []\n"
        "    distinct_styles = set()\n"
        "    d = p.get_text('dict')\n"
        "    for b in d.get('blocks', []):\n"
        "        for l in b.get('lines', []):\n"
        "            for s in l.get('spans', []):\n"
        "                s_vis = fitz.Rect(s['bbox']) * p.rotation_matrix\n"
        "                inter = s_vis & target_vis\n"
        "                if not inter.is_empty:\n"
        "                    w = inter.width * inter.height\n"
        "                    spans.append((w, s))\n"
        "                    c_int = s.get('color', 0)\n"
        "                    distinct_styles.add(f\"{s.get('font')}_{round(s.get('size',0))}_{c_int}\")\n"
        "    if not spans:\n"
        "        print(json.dumps({'valid': False}), flush=True)\n"
        "        sys.exit(0)\n"
        "    spans.sort(key=lambda x: x[0], reverse=True)\n"
        "    best = spans[0][1]\n"
        "    c_int = best.get('color', 0)\n"
        "    r = (c_int >> 16) & 0xFF\n"
        "    g = (c_int >> 8) & 0xFF\n"
        "    b = c_int & 0xFF\n"
        "    flags = best.get('flags', 0)\n"
        "    is_bold = bool(flags & (1 << 4) or flags & (1 << 5) or 'bold' in best.get('font','').lower() or 'heavy' in best.get('font','').lower() or 'black' in best.get('font','').lower())\n"
        "    is_italic = bool(flags & (1 << 1) or 'italic' in best.get('font','').lower() or 'oblique' in best.get('font','').lower())\n"
        "    res = {\n"
        "        'valid': True,\n"
        "        'font': best.get('font', ''),\n"
        "        'size': best.get('size', 12.0),\n"
        "        'color': f'#{r:02x}{g:02x}{b:02x}',\n"
        "        'is_bold': is_bold,\n"
        "        'is_italic': is_italic,\n"
        "        'ascender': best.get('ascender', 0.0),\n"
        "        'descender': best.get('descender', 0.0),\n"
        "        'has_multiple_styles': len(distinct_styles) > 1\n"
        "    }\n"
        "    print(json.dumps(res), flush=True)\n"
        "    sys.exit(0)\n"
        "except Exception as e:\n"
        "    sys.stderr.write(str(e) + '\\n')\n"
        "    sys.stderr.flush()\n"
        "    sys.exit(1)\n"
    );

    QStringList args;
    args << QStringLiteral("-c") << script
         << sourceFilePath
         << QString::number(pageIndex)
         << QString::number(pointRect.left(), 'f', 4)
         << QString::number(pointRect.top(), 'f', 4)
         << QString::number(pointRect.right(), 'f', 4)
         << QString::number(pointRect.bottom(), 'f', 4);

    QProcess proc;
    proc.start(pyBin, args);
    if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
        errorMessage = QObject::tr("Failed to extract text format from page.");
        return false;
    }

    QByteArray out = proc.readAllStandardOutput().trimmed();
    QJsonDocument jdoc = QJsonDocument::fromJson(out);
    if (!jdoc.isObject()) {
        errorMessage = QObject::tr("Invalid JSON format response from text inspection: '") + QString::fromUtf8(out) + QStringLiteral("' STDERR: '") + QString::fromUtf8(proc.readAllStandardError()) + QStringLiteral("'");
        return false;
    }

    QJsonObject obj = jdoc.object();
    if (!obj.value(QStringLiteral("valid")).toBool()) {
        return false;
    }

    outFormat.valid = true;
    outFormat.fontName = obj.value(QStringLiteral("font")).toString();
    outFormat.pointSize = obj.value(QStringLiteral("size")).toDouble(12.0);
    outFormat.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#000000")));
    bool bold = obj.value(QStringLiteral("is_bold")).toBool();
    bool italic = obj.value(QStringLiteral("is_italic")).toBool();
    outFormat.family = resolveFontFamily(outFormat.fontName, bold, italic);
    outFormat.isBold = bold;
    outFormat.isItalic = italic;
    outFormat.ascender = obj.value(QStringLiteral("ascender")).toDouble(0.0);
    outFormat.descender = obj.value(QStringLiteral("descender")).toDouble(0.0);
    outFormat.hasMultipleStyles = obj.value(QStringLiteral("has_multiple_styles")).toBool();

    return true;
}

bool PdfDocumentOps::inspectTextFormatByText(const QString &sourceFilePath,
                                            int pageIndex,
                                            const QString &searchText,
                                            TextFormatInfo &outFormat,
                                            QString &errorMessage)
{
    outFormat = TextFormatInfo();
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath) || searchText.trimmed().isEmpty()) {
        errorMessage = QObject::tr("Source PDF file or search text is empty.");
        return false;
    }

    QString pyBin = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyBin.isEmpty()) {
        errorMessage = QObject::tr("Python 3 executable not found.");
        return false;
    }

    const QString script = QStringLiteral(
        "import sys, os, json\n"
        "try:\n"
        "    import fitz\n"
        "    doc = fitz.open(sys.argv[1])\n"
        "    p = doc[int(sys.argv[2])]\n"
        "    st = sys.argv[3].strip().lower()\n"
        "    spans = []\n"
        "    distinct_styles = set()\n"
        "    d = p.get_text('dict')\n"
        "    for b in d.get('blocks', []):\n"
        "        for l in b.get('lines', []):\n"
        "            for s in l.get('spans', []):\n"
        "                text = s.get('text', '').strip().lower()\n"
        "                if not text: continue\n"
        "                w = 0\n"
        "                if text == st: w = 10000\n"
        "                elif st in text or text in st: w = 5000 + len(text)\n"
        "                if w > 0:\n"
        "                    spans.append((w, s))\n"
        "                    c_int = s.get('color', 0)\n"
        "                    distinct_styles.add(f\"{s.get('font')}_{round(s.get('size',0))}_{c_int}\")\n"
        "    if not spans:\n"
        "        print(json.dumps({'valid': False}), flush=True)\n"
        "        sys.exit(0)\n"
        "    spans.sort(key=lambda x: x[0], reverse=True)\n"
        "    best = spans[0][1]\n"
        "    c_int = best.get('color', 0)\n"
        "    r = (c_int >> 16) & 0xFF\n"
        "    g = (c_int >> 8) & 0xFF\n"
        "    b = c_int & 0xFF\n"
        "    flags = best.get('flags', 0)\n"
        "    is_bold = bool(flags & (1 << 4) or flags & (1 << 5) or 'bold' in best.get('font','').lower() or 'heavy' in best.get('font','').lower() or 'black' in best.get('font','').lower())\n"
        "    is_italic = bool(flags & (1 << 1) or 'italic' in best.get('font','').lower() or 'oblique' in best.get('font','').lower())\n"
        "    res = {\n"
        "        'valid': True,\n"
        "        'font': best.get('font', ''),\n"
        "        'size': best.get('size', 12.0),\n"
        "        'color': f'#{r:02x}{g:02x}{b:02x}',\n"
        "        'is_bold': is_bold,\n"
        "        'is_italic': is_italic,\n"
        "        'ascender': best.get('ascender', 0.0),\n"
        "        'descender': best.get('descender', 0.0),\n"
        "        'has_multiple_styles': len(distinct_styles) > 1\n"
        "    }\n"
        "    print(json.dumps(res), flush=True)\n"
        "    sys.exit(0)\n"
        "except Exception as e:\n"
        "    sys.stderr.write(str(e) + '\\n')\n"
        "    sys.stderr.flush()\n"
        "    sys.exit(1)\n"
    );

    QStringList args;
    args << QStringLiteral("-c") << script
         << sourceFilePath
         << QString::number(pageIndex)
         << searchText;

    QProcess proc;
    proc.start(pyBin, args);
    if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
        errorMessage = QObject::tr("Failed to extract text format from page.");
        return false;
    }

    QByteArray out = proc.readAllStandardOutput().trimmed();
    QJsonDocument jdoc = QJsonDocument::fromJson(out);
    if (!jdoc.isObject()) {
        errorMessage = QObject::tr("Invalid JSON format response from text inspection: '") + QString::fromUtf8(out) + QStringLiteral("' STDERR: '") + QString::fromUtf8(proc.readAllStandardError()) + QStringLiteral("'");
        return false;
    }

    QJsonObject obj = jdoc.object();
    if (!obj.value(QStringLiteral("valid")).toBool()) {
        return false;
    }

    outFormat.valid = true;
    outFormat.fontName = obj.value(QStringLiteral("font")).toString();
    outFormat.pointSize = obj.value(QStringLiteral("size")).toDouble(12.0);
    outFormat.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#000000")));
    bool bold = obj.value(QStringLiteral("is_bold")).toBool();
    bool italic = obj.value(QStringLiteral("is_italic")).toBool();
    outFormat.family = resolveFontFamily(outFormat.fontName, bold, italic);
    outFormat.isBold = bold;
    outFormat.isItalic = italic;
    outFormat.ascender = obj.value(QStringLiteral("ascender")).toDouble(0.0);
    outFormat.descender = obj.value(QStringLiteral("descender")).toDouble(0.0);
    outFormat.hasMultipleStyles = obj.value(QStringLiteral("has_multiple_styles")).toBool();

    return true;
}

void PdfDocumentOps::showDocumentInfoDialog(QWidget *parent,
                                           Poppler::Document *doc,
                                           const QString &filePath,
                                           int currentPage,
                                           int pageCount)
{
    if (!doc) return;

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("PDF Document Properties & Metadata"));
    dlg.setMinimumWidth(440);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setContentsMargins(18, 18, 18, 18);
    dlgLayout->setSpacing(12);

    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto addRow = [form](const QString &label, const QString &val) {
        if (!val.isEmpty()) {
            QLabel *lblVal = new QLabel(val);
            lblVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow(QStringLiteral("<b>") + label + QStringLiteral(":</b>"), lblVal);
        }
    };

    QFileInfo fi(filePath);
    addRow(QObject::tr("File Name"), fi.fileName());
    addRow(QObject::tr("Location"), fi.absolutePath());
    addRow(QObject::tr("File Size"), QString("%1 KB").arg(fi.size() / 1024));
    addRow(QObject::tr("Total Pages"), QString::number(pageCount));

    if (currentPage >= 0 && currentPage < pageCount) {
        std::unique_ptr<Poppler::Page> p(doc->page(currentPage));
        if (p) {
            QSizeF sz = p->pageSize();
            double wMm = sz.width() * 25.4 / 72.0;
            double hMm = sz.height() * 25.4 / 72.0;
            addRow(QObject::tr("Page Dimensions"), QString("%1 × %2 mm (%3 × %4 pt)")
                .arg(wMm, 0, 'f', 1).arg(hMm, 0, 'f', 1)
                .arg(sz.width(), 0, 'f', 0).arg(sz.height(), 0, 'f', 0));
        }
    }

    addRow(QObject::tr("Title"), doc->info("Title"));
    addRow(QObject::tr("Author"), doc->info("Author"));
    addRow(QObject::tr("Subject"), doc->info("Subject"));
    addRow(QObject::tr("Keywords"), doc->info("Keywords"));
    addRow(QObject::tr("Creator"), doc->info("Creator"));
    addRow(QObject::tr("Producer"), doc->info("Producer"));

    QDateTime cDate = doc->creationDate();
    if (cDate.isValid()) addRow(QObject::tr("Created"), cDate.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    QDateTime mDate = doc->modificationDate();
    if (mDate.isValid()) addRow(QObject::tr("Modified"), mDate.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    auto ver = doc->getPdfVersion();
    addRow(QObject::tr("PDF Version"), QString("v%1.%2").arg(ver.major).arg(ver.minor));
    addRow(QObject::tr("Fast Web View"), doc->isLinearized() ? QObject::tr("Yes (Linearized)") : QObject::tr("No"));
    addRow(QObject::tr("Encrypted"), doc->isEncrypted() ? QObject::tr("Yes (Password Protected)") : QObject::tr("No"));

    dlgLayout->addLayout(form);
    dlgLayout->addStretch(1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *btnClose = new QPushButton(QObject::tr("Close"), &dlg);
    btnClose->setCursor(Qt::PointingHandCursor);
    QObject::connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(btnClose);
    dlgLayout->addLayout(btnRow);

    dlg.exec();
}

