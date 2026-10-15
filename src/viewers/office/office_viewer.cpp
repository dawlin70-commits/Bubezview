#include "office_viewer.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "core/plugin_manager.h"
#include "theme_manager.h"
#include <archive.h>
#include <archive_entry.h>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScrollBar>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

/**
 * @brief Fast single-pass word counter operating on a QStringView.
 * Avoids any heap allocation or regex overhead by iterating through code points
 * and tracking transitions between whitespace and non-whitespace characters.
 */
static inline int countWordsFast(QStringView sv)
{
    int count = 0;
    bool inWord = false;
    for (QChar ch : sv) {
        if (ch.isSpace()) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            count++;
        }
    }
    return count;
}

// --- OfficeConvertWorker Implementation ---
// Executes LibreOffice in a separate background thread with '--headless --convert-to pdf'
// to avoid freezing the GUI during complex conversions. Supports graceful atomic cancellation.
OfficeConvertWorker::OfficeConvertWorker(const QString &sourcePath, QObject *parent)
    : QThread(parent)
    , m_sourcePath(sourcePath)
{
}


void OfficeConvertWorker::cancel()
{
    m_cancelled.store(true);
    if (m_process) {
        m_process->kill();
    }
}

void OfficeConvertWorker::run()
{
    if (m_cancelled.load() || m_sourcePath.isEmpty()) return;

    QString outDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/bubez_office_pdf";
    QDir().mkpath(outDir);

    QFileInfo fi(m_sourcePath);
    QString expectedPdf = outDir + "/" + fi.completeBaseName() + ".pdf";

    // Remove stale file if any
    if (QFile::exists(expectedPdf)) {
        QFile::remove(expectedPdf);
    }

    QString loBin = QStandardPaths::findExecutable("libreoffice");
    if (loBin.isEmpty()) loBin = QStandardPaths::findExecutable("soffice");
    if (loBin.isEmpty()) {
        emit conversionFailed(m_sourcePath, tr("LibreOffice not found or cannot be launched."));
        return;
    }

    QProcess proc;
    m_process = &proc;
    QStringList args;
    args << "--headless" << "--convert-to" << "pdf" << "--outdir" << outDir << m_sourcePath;

    proc.start(loBin, args);
    if (!proc.waitForStarted(2000)) {
        m_process = nullptr;
        emit conversionFailed(m_sourcePath, tr("Unable to launch LibreOffice."));
        return;
    }

    // Wait for conversion with cancellation check
    while (!proc.waitForFinished(100)) {
        if (m_cancelled.load()) {
            proc.kill();
            proc.waitForFinished(300);
            m_process = nullptr;
            return;
        }
    }

    m_process = nullptr;

    if (m_cancelled.load()) return;

    if (proc.exitCode() == 0 && QFile::exists(expectedPdf)) {
        emit conversionFinished(m_sourcePath, expectedPdf);
    } else {
        QString err = QString::fromUtf8(proc.readAllStandardError());
        emit conversionFailed(m_sourcePath, err.isEmpty() ? tr("Conversion failed.") : err);
    }
}

// --- OfficeViewer Implementation ---
OfficeViewer::OfficeViewer(QWidget *parent)
    : QWidget(parent)
    , m_worker(nullptr)
    , m_wordCount(0)
    , m_isPdfMode(false)
{
    setupUi();
}

OfficeViewer::~OfficeViewer()
{
    if (m_worker) {
        m_worker->disconnect();
        m_worker->cancel();
        m_worker->wait(500);
        m_worker = nullptr;
    }
    clear();
}

void OfficeViewer::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Top Action Toolbar
    m_toolbarWidget = new QWidget(this);
    m_toolbarWidget->setObjectName("OfficeToolbar");
    m_toolbarWidget->setStyleSheet(
        "#OfficeToolbar {"
        "   background-color: #202428;"
        "   border-bottom: 1px solid #334155;"
        "}"
        "QPushButton {"
        "   background-color: #2b3036;"
        "   color: #f1f5f9;"
        "   border: 1px solid #475569;"
        "   border-radius: 4px;"
        "   padding: 5px 12px;"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3b82f6;"
        "   border-color: #60a5fa;"
        "   color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #2563eb;"
        "}"
    );

    QHBoxLayout *tbLayout = new QHBoxLayout(m_toolbarWidget);
    tbLayout->setContentsMargins(12, 6, 12, 6);
    tbLayout->setSpacing(8);

    m_btnEditLo = new QPushButton(tr("✏️ Edit in LibreOffice"), m_toolbarWidget);
    m_btnEditLo->setToolTip(tr("Open file directly in LibreOffice desktop suite"));
    connect(m_btnEditLo, &QPushButton::clicked, this, &OfficeViewer::onOpenInLibreOffice);

    m_btnSavePdf = new QPushButton(tr("💾 Save as PDF"), m_toolbarWidget);
    m_btnSavePdf->setToolTip(tr("Save and export a copy of the document in PDF format"));
    connect(m_btnSavePdf, &QPushButton::clicked, this, &OfficeViewer::onSaveAsPdf);

    m_btnExtractImg = new QPushButton(tr("🖼️ Extract Images"), m_toolbarWidget);
    m_btnExtractImg->setToolTip(tr("Extract all images embedded in document"));
    connect(m_btnExtractImg, &QPushButton::clicked, this, &OfficeViewer::onExtractImages);

    m_lblDocInfo = new QLabel(m_toolbarWidget);

    tbLayout->addWidget(m_btnEditLo);
    tbLayout->addWidget(m_btnSavePdf);
    tbLayout->addWidget(m_btnExtractImg);
    tbLayout->addWidget(m_lblDocInfo);
    tbLayout->addStretch();

    mainLayout->addWidget(m_toolbarWidget);

    // 2. Stacked widget for views
    m_stack = new QStackedWidget(this);

    // View 0: Loading
    m_loadingWidget = new QWidget(m_stack);
    QVBoxLayout *loadLayout = new QVBoxLayout(m_loadingWidget);
    m_lblLoading = new QLabel(tr("⚡ Rendering document layout..."), m_loadingWidget);
    m_lblLoading->setAlignment(Qt::AlignCenter);
    m_lblLoading->setStyleSheet("font-size: 15px; font-weight: bold; color: #38bdf8; padding: 40px;");
    loadLayout->addWidget(m_lblLoading);
    m_stack->addWidget(m_loadingWidget);

    // View 1: High-Definition PDF Viewer (Poppler via plugin)
    auto *viewer = PluginManager::instance().createViewer(FileCategory::Pdf, m_stack);
    m_pdfViewer = dynamic_cast<IPdfViewer*>(viewer);
    if (m_pdfViewer) {
        m_stack->addWidget(m_pdfViewer->widget());
    } else {
        m_stack->addWidget(new QWidget(m_stack));
    }

    // View 2: Internal Fallback QTextBrowser
    m_textBrowser = new QTextBrowser(m_stack);
    m_textBrowser->setOpenExternalLinks(false);
    m_textBrowser->setFrameShape(QFrame::NoFrame);
    m_stack->addWidget(m_textBrowser);

    mainLayout->addWidget(m_stack, 1);

    updateTheme();
}

void OfficeViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::SystemNative) {
        m_toolbarWidget->setStyleSheet(
            "#OfficeToolbar {"
            "   background-color: palette(window);"
            "   border-bottom: 1px solid palette(mid);"
            "}"
            "QPushButton {"
            "   background-color: palette(button);"
            "   color: palette(button-text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 4px;"
            "   padding: 5px 12px;"
            "   font-size: 13px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: palette(highlight);"
            "   color: palette(highlighted-text);"
            "}"
        );
        m_lblDocInfo->setStyleSheet("color: palette(window-text); font-size: 12px; margin-left: 10px; font-weight: 500;");
        m_textBrowser->setStyleSheet(
            "QTextBrowser {"
            "   background-color: palette(base);"
            "   color: palette(text);"
            "   selection-background-color: palette(highlight);"
            "   selection-color: palette(highlighted-text);"
            "   font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif;"
            "   font-size: 14px;"
            "   padding: 24px 32px;"
            "   border: none;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );
    } else {
        m_toolbarWidget->setStyleSheet(
            "#OfficeToolbar {"
            "   background-color: #202428;"
            "   border-bottom: 1px solid #334155;"
            "}"
            "QPushButton {"
            "   background-color: #2b3036;"
            "   color: #f1f5f9;"
            "   border: 1px solid #475569;"
            "   border-radius: 4px;"
            "   padding: 5px 12px;"
            "   font-size: 13px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #3b82f6;"
            "   border-color: #60a5fa;"
            "   color: #ffffff;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #2563eb;"
            "}"
        );
        m_lblDocInfo->setStyleSheet("color: #94a3b8; font-size: 12px; margin-left: 10px; font-weight: 500;");
        m_textBrowser->setStyleSheet(
            "QTextBrowser {"
            "   background-color: #1b1e20;"
            "   color: #e2e8f0;"
            "   selection-background-color: #3daee9;"
            "   selection-color: #ffffff;"
            "   font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif;"
            "   font-size: 14px;"
            "   padding: 24px 32px;"
            "   border: none;"
            "}\n" + ThemeManager::scrollBarStyleSheet(preset)
        );
    }

    if (m_pdfViewer) {
        m_pdfViewer->updateTheme();
    }
}

bool OfficeViewer::loadFile(const QString &filePath)
{
    clear();

    QFileInfo fi(filePath);
    if (!fi.exists()) return false;

    m_currentFilePath = fi.absoluteFilePath();
    m_lblDocInfo->setText("📄 " + fi.fileName());

    QString outDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/bubez_office_pdf";
    QString expectedPdf = outDir + "/" + fi.completeBaseName() + ".pdf";

    // 1. If LibreOffice mode is active (or if it's a presentation format pptx/ppt/odp) and cached converted PDF is ready, load it immediately!
    QString ext = fi.suffix().toLower();
    bool isPresentation = (ext == "pptx" || ext == "ppt" || ext == "odp");
    bool useLo = ThemeManager::useLibreOffice();
    bool tryLibreOffice = useLo || isPresentation;

#ifndef ENABLE_INTERNAL_OFFICE
    if (!tryLibreOffice) {
        m_lblLoading->setText(tr("⚠️ LibreOffice preview is disabled in Settings and the Internal Office module is not compiled.<br>Enable LibreOffice in Preferences or compile the internal module in Settings → Extensions."));
        m_stack->setCurrentWidget(m_loadingWidget);
        return false;
    }
#endif

    if (tryLibreOffice && QFile::exists(expectedPdf) && QFileInfo(expectedPdf).lastModified() >= fi.lastModified()) {
        if (!m_pdfViewer) {
            auto *v = PluginManager::instance().createViewer(FileCategory::Pdf, m_stack);
            m_pdfViewer = dynamic_cast<IPdfViewer*>(v);
            if (m_pdfViewer) {
                QWidget *oldW = m_stack->widget(1);
                m_stack->removeWidget(oldW);
                delete oldW;
                m_stack->insertWidget(1, m_pdfViewer->widget());
            }
        }
        if (m_pdfViewer && m_pdfViewer->loadFile(expectedPdf)) {
            m_convertedPdfPath = expectedPdf;
            m_isPdfMode = true;
            m_stack->setCurrentWidget(m_pdfViewer->widget());
            return true;
        }
    }

    // 2. Render instantly with internal native C++ parser (< 15 ms) if enabled
    bool parsed = parseInternal(m_currentFilePath);
    if (parsed) {
        m_isPdfMode = false;
        m_stack->setCurrentWidget(m_textBrowser);
    } else {
        if (isPresentation) {
            m_lblLoading->setText(tr("⏳ Loading presentation with LibreOffice..."));
        } else {
            m_lblLoading->setText(tr("⏳ Converting office document with LibreOffice..."));
        }
        m_stack->setCurrentWidget(m_loadingWidget);
    }

    // 3. If LibreOffice conversion is requested or required, launch background convert worker
    if (tryLibreOffice) {
        static bool s_hasLo = !QStandardPaths::findExecutable("libreoffice").isEmpty() ||
                              !QStandardPaths::findExecutable("soffice").isEmpty();
        if (s_hasLo) {
            m_worker = new OfficeConvertWorker(m_currentFilePath, nullptr);
            connect(m_worker, &OfficeConvertWorker::conversionFinished, this, &OfficeViewer::onConversionFinished);
            connect(m_worker, &OfficeConvertWorker::conversionFailed, this, &OfficeViewer::onConversionFailed);
            connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
            m_worker->start();
        } else {
#ifndef ENABLE_INTERNAL_OFFICE
            m_lblLoading->setText(tr("⚠️ LibreOffice is not installed and the Internal Office module is disabled.<br>Please install LibreOffice or enable the internal module in Settings → Extensions."));
            m_stack->setCurrentWidget(m_loadingWidget);
            return false;
#else
            if (isPresentation && !parsed) {
                m_lblLoading->setText(tr("⚠️ LibreOffice is required to preview presentation files (.pptx, .odp)."));
                m_stack->setCurrentWidget(m_loadingWidget);
            }
#endif
        }
    }

    return parsed || isPresentation;
}

bool OfficeViewer::loadFileInternal(const QString &filePath)
{
    clear();

    QFileInfo fi(filePath);
    if (!fi.exists()) return false;

    m_currentFilePath = fi.absoluteFilePath();
    m_lblDocInfo->setText("📄 " + fi.fileName());

    bool parsed = parseInternal(m_currentFilePath);
    if (parsed) {
        m_isPdfMode = false;
        m_stack->setCurrentWidget(m_textBrowser);
        return true;
    }

    m_lblLoading->setText(tr("⚠️ Unable to display the document."));
    m_stack->setCurrentWidget(m_loadingWidget);
    return false;
}


bool OfficeViewer::parseInternal(const QString &filePath)
{
#ifndef ENABLE_INTERNAL_OFFICE
    Q_UNUSED(filePath);
    return false;
#else
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    bool parsed = false;
    if (ext == "docx") {
        parsed = parseDocx(filePath);
    } else if (ext == "odt") {
        parsed = parseOdt(filePath);
    } else if (ext == "ods") {
        parsed = parseOds(filePath);
    } else if (ext == "xlsx" || ext == "xltx" || ext == "xlsm") {
        parsed = parseXlsx(filePath);
    } else if (ext == "doc") {
        parsed = parseOldDoc(filePath);
    } else if (ext == "xls") {
        parsed = parseOldXls(filePath);
    } else if (ext == "rtf") {
        parsed = parseRtf(filePath);
    } else if (ext == "epub") {
        parsed = parseEpub(filePath);
    }
    return parsed;
#endif
}

void OfficeViewer::onConversionFinished(const QString &sourcePath, const QString &pdfPath)
{
    if (sourcePath != m_currentFilePath) return;

    m_convertedPdfPath = pdfPath;
    if (!m_pdfViewer) {
        auto *v = PluginManager::instance().createViewer(FileCategory::Pdf, m_stack);
        m_pdfViewer = dynamic_cast<IPdfViewer*>(v);
        if (m_pdfViewer) {
            QWidget *oldW = m_stack->widget(1);
            m_stack->removeWidget(oldW);
            delete oldW;
            m_stack->insertWidget(1, m_pdfViewer->widget());
        }
    }
    if (m_pdfViewer && m_pdfViewer->loadFile(pdfPath)) {
        m_isPdfMode = true;
        m_stack->setCurrentWidget(m_pdfViewer->widget());
    }
}

void OfficeViewer::onConversionFailed(const QString &sourcePath, const QString &errorMsg)
{
    Q_UNUSED(errorMsg);
    if (sourcePath != m_currentFilePath) return;

    if (m_stack->currentWidget() == m_loadingWidget) {
        parseInternal(m_currentFilePath);
        m_stack->setCurrentWidget(m_textBrowser);
    }
}

void OfficeViewer::onOpenInLibreOffice()
{
    if (m_currentFilePath.isEmpty()) return;

    QString bin = QStandardPaths::findExecutable("libreoffice");
    if (bin.isEmpty()) bin = QStandardPaths::findExecutable("soffice");
    if (!bin.isEmpty()) {
        QProcess::startDetached(bin, QStringList() << m_currentFilePath);
    } else {
        QMessageBox::warning(this, tr("LibreOffice"), tr("LibreOffice is not installed on your system."));
    }
}

void OfficeViewer::onSaveAsPdf()
{
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QString defName = fi.completeBaseName() + ".pdf";
    QString defPath = fi.dir().absoluteFilePath(defName);

    QString savePath = QFileDialog::getSaveFileName(this, tr("Save as PDF"), defPath, tr("PDF Documents (*.pdf)"));
    if (savePath.isEmpty()) return;

    if (!m_convertedPdfPath.isEmpty() && QFile::exists(m_convertedPdfPath)) {
        if (QFile::exists(savePath)) QFile::remove(savePath);
        if (QFile::copy(m_convertedPdfPath, savePath)) {
            QMessageBox::information(this, tr("Save Complete"), tr("File saved successfully to:\n") + savePath);
            return;
        }
    }

    QMessageBox::warning(this, tr("Error"), tr("Unable to save document as PDF."));
}

void OfficeViewer::onExtractImages()
{
    if (m_currentFilePath.isEmpty()) return;

    QFileInfo fi(m_currentFilePath);
    QString targetDir = QFileDialog::getExistingDirectory(this, tr("Select directory for extracted images"), fi.dir().absolutePath());
    if (targetDir.isEmpty()) return;

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, m_currentFilePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        QMessageBox::information(this, tr("No Images"), tr("Unable to extract images from this format."));
        return;
    }

    struct archive_entry *entry;
    int count = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        QString p = QString::fromUtf8(archive_entry_pathname(entry));
        if (p.startsWith("word/media/") || p.startsWith("ppt/media/") || p.startsWith("Pictures/")) {
            QFileInfo entryFi(p);
            QString outName = entryFi.fileName();
            if (!outName.isEmpty()) {
                QString outPath = targetDir + "/" + outName;
                QFile outFile(outPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    la_int64_t size = archive_entry_size(entry);
                    if (size > 0) {
                        QByteArray b(size, Qt::Uninitialized);
                        archive_read_data(a, b.data(), size);
                        outFile.write(b);
                    }
                    outFile.close();
                    count++;
                }
            }
        }
    }
    archive_read_free(a);

    if (count > 0) {
        QMessageBox::information(this, tr("Images Extracted"), QString(tr("Successfully extracted %1 images to:\n%2")).arg(count).arg(targetDir));
    } else {
        QMessageBox::information(this, tr("No Images"), tr("No images found inside this document."));
    }
}

void OfficeViewer::clear()
{
    if (m_worker) {
        m_worker->disconnect();
        m_worker->cancel();
        connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
        m_worker = nullptr;
    }

    if (m_pdfViewer) m_pdfViewer->clear();
    m_textBrowser->clear();
    m_currentFilePath.clear();
    m_convertedPdfPath.clear();
    m_lblDocInfo->clear();
    m_wordCount = 0;
    m_isPdfMode = false;
}

QByteArray OfficeViewer::extractZipEntry(const QString &zipPath, const QString &entryName)
{
    auto map = extractZipEntries(zipPath, QStringList() << entryName);
    return map.value(entryName);
}

QMap<QString, QByteArray> OfficeViewer::extractZipEntries(const QString &zipPath, const QStringList &entryNames)
{
    QMap<QString, QByteArray> result;
    if (zipPath.isEmpty() || entryNames.isEmpty()) return result;

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, zipPath.toUtf8().constData(), 16384) != ARCHIVE_OK) {
        archive_read_free(a);
        return result;
    }

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *p = archive_entry_pathname(entry);
        if (!p) continue;
        QString path = QString::fromUtf8(p);

        for (const QString &target : entryNames) {
            if (path == target || path.endsWith("/" + target)) {
                la_int64_t size = archive_entry_size(entry);
                QByteArray data;
                if (size > 0) {
                    data.resize(size);
                    ssize_t total = 0;
                    while (total < size) {
                        ssize_t r = archive_read_data(a, data.data() + total, size - total);
                        if (r <= 0) break;
                        total += r;
                    }
                    data.resize(total);
                } else {
                    char buffer[16384];
                    ssize_t r;
                    while ((r = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                        data.append(buffer, r);
                    }
                }
                result[target] = data;
                break;
            }
        }
        if (result.size() == entryNames.size()) break;
    }

    archive_read_close(a);
    archive_read_free(a);
    return result;
}

bool OfficeViewer::parseDocx(const QString &filePath)
{
    QByteArray xmlData = extractZipEntry(filePath, "word/document.xml");
    if (xmlData.isEmpty()) {
        return false;
    }

    QString html = parseWordXmlToHtml(xmlData);
    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

QString OfficeViewer::parseWordXmlToHtml(const QByteArray &xmlData)
{
    QXmlStreamReader xml(xmlData);
    QString html;
    html.reserve(32 * 1024);
    html += "<html><head><style>"
            "body { line-height: 1.6; color: #e2e8f0; max-width: 800px; margin: auto; }"
            "h1, h2, h3 { color: #38bdf8; margin-top: 18px; margin-bottom: 8px; }"
            "p { margin-bottom: 12px; }"
            "table { border-collapse: collapse; width: 100%; margin: 16px 0; border: 1px solid #3f3f46; }"
            "td, th { border: 1px solid #3f3f46; padding: 6px 10px; }"
            "</style></head><body>";

    bool isBold = false;
    bool isItalic = false;

    QString currentParagraph;
    currentParagraph.reserve(1024);
    m_wordCount = 0;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();

            if (name == u"tbl") {
                html += "<table>";
            } else if (name == u"tr") {
                html += "<tr>";
            } else if (name == u"tc") {
                html += "<td>";
            } else if (name == u"p") {
                currentParagraph.clear();
            } else if (name == u"r") {
                isBold = false;
                isItalic = false;
            } else if (name == u"b") {
                isBold = true;
            } else if (name == u"i") {
                isItalic = true;
            } else if (name == u"t") {
                QString text = xml.readElementText();
                if (!text.isEmpty()) {
                    QString formatted = text.toHtmlEscaped();
                    if (isBold) formatted = "<b>" + formatted + "</b>";
                    if (isItalic) formatted = "<i>" + formatted + "</i>";
                    currentParagraph += formatted;
                    m_wordCount += countWordsFast(text);
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            auto name = xml.name();

            if (name == u"p") {
                if (!currentParagraph.trimmed().isEmpty()) {
                    html += "<p>" + currentParagraph + "</p>";
                }
            } else if (name == u"tc") {
                html += "</td>";
            } else if (name == u"tr") {
                html += "</tr>";
            } else if (name == u"tbl") {
                html += "</table>";
            }
        }
    }

    html += "</body></html>";
    return html;
}

bool OfficeViewer::parseOdt(const QString &filePath)
{
    QByteArray xmlData = extractZipEntry(filePath, "content.xml");
    if (xmlData.isEmpty()) return false;

    QString html = parseOdtXmlToHtml(xmlData);
    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

QString OfficeViewer::parseOdtXmlToHtml(const QByteArray &xmlData)
{
    QXmlStreamReader xml(xmlData);
    QString html;
    html.reserve(32 * 1024);
    html += "<html><head><style>"
            "body { line-height: 1.6; color: #e2e8f0; max-width: 800px; margin: auto; }"
            "h1, h2, h3 { color: #38bdf8; margin-top: 18px; margin-bottom: 8px; }"
            "p { margin-bottom: 12px; }"
            "</style></head><body>";

    m_wordCount = 0;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();
            if (name == u"h") {
                html += "<h3>" + xml.readElementText().toHtmlEscaped() + "</h3>";
            } else if (name == u"p") {
                QString text = xml.readElementText();
                if (!text.trimmed().isEmpty()) {
                    html += "<p>" + text.toHtmlEscaped() + "</p>";
                    m_wordCount += countWordsFast(text);
                }
            }
        }
    }

    html += "</body></html>";
    return html;
}

bool OfficeViewer::parseXlsx(const QString &filePath)
{
    // Extract sharedStrings.xml (values) and sheet1.xml (layout) in a single pass
    auto entries = extractZipEntries(filePath, {"xl/sharedStrings.xml", "xl/worksheets/sheet1.xml"});
    QByteArray sheetData = entries.value("xl/worksheets/sheet1.xml");

    if (sheetData.isEmpty()) return false;

    QString html = parseSpreadsheetXmlToHtml(sheetData, entries.value("xl/sharedStrings.xml"));
    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

QString OfficeViewer::parseSpreadsheetXmlToHtml(const QByteArray &sheetData, const QByteArray &sharedStringsData)
{
    // 1. Read shared strings table
    QStringList sharedStrings;
    if (!sharedStringsData.isEmpty()) {
        sharedStrings.reserve(1024);
        QXmlStreamReader sxml(sharedStringsData);
        while (!sxml.atEnd() && !sxml.hasError()) {
            if (sxml.readNext() == QXmlStreamReader::StartElement && sxml.name() == u"t") {
                sharedStrings.append(sxml.readElementText());
            }
        }
    }

    // 2. Read sheet1
    QXmlStreamReader xml(sheetData);
    QString html;
    html.reserve(64 * 1024);
    html += "<html><head><style>"
            "body { background-color: #1b1e20; color: #e2e8f0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 16px; }"
            "table { border-collapse: collapse; border: 1px solid #3f3f46; margin: 8px 0; }"
            "td, th { border: 1px solid #334155; padding: 6px 14px; white-space: nowrap; font-size: 13px; }"
            "tr:nth-child(even) { background-color: #232629; }"
            "tr:first-child { background-color: #27272a; font-weight: bold; color: #38bdf8; }"
            "</style></head><body><table>";

    int rowCount = 0;
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();
            if (name == u"row") {
                if (++rowCount > 500) {
                    break;
                }
                html += "<tr>";
            } else if (name == u"c") {
                QString type = xml.attributes().value(QLatin1String("t")).toString();
                while (!(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == u"v") &&
                       !(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == u"c") &&
                       !xml.atEnd()) {
                    xml.readNext();
                }

                QString val;
                if (xml.name() == u"v") {
                    val = xml.readElementText();
                    if (type == "s") {
                        int idx = val.toInt();
                        if (idx >= 0 && idx < sharedStrings.size()) {
                            val = sharedStrings[idx];
                        }
                    }
                }
                html += "<td>";
                html += val.toHtmlEscaped();
                html += "</td>";
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == u"row") {
                html += "</tr>";
            }
        }
    }

    html += "</table></body></html>";
    return html;
}

bool OfficeViewer::parseOds(const QString &filePath)
{
    QByteArray xmlData = extractZipEntry(filePath, "content.xml");
    if (xmlData.isEmpty()) return false;

    QString html = parseOdsXmlToHtml(xmlData);
    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

QString OfficeViewer::parseOdsXmlToHtml(const QByteArray &xmlData)
{
    QXmlStreamReader xml(xmlData);
    QString html;
    html.reserve(64 * 1024);
    html += "<html><head><style>"
            "body { background-color: #1b1e20; color: #e2e8f0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 16px; }"
            "table { border-collapse: collapse; margin: 12px 0; border: 1px solid #3f3f46; }"
            "td, th { border: 1px solid #334155; padding: 6px 14px; white-space: nowrap; font-size: 13px; }"
            "tr:nth-child(even) { background-color: #232629; }"
            "tr:first-child { background-color: #27272a; font-weight: bold; color: #38bdf8; }"
            "h2 { color: #38bdf8; margin: 20px 0 8px 0; font-size: 16px; }"
            "</style></head><body>";

    m_wordCount = 0;
    bool inCell = false;
    QString cellText;
    cellText.reserve(512);
    int repeat = 1;
    int rowCellCount = 0;
    int rowCount = 0;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            auto name = xml.name();
            if (name == u"table") {
                auto tableName = xml.attributes().value(QLatin1String("name"));
                if (!tableName.isEmpty()) {
                    html += "<h2>📊 ";
                    html += tableName.toString().toHtmlEscaped();
                    html += "</h2>";
                }
                html += "<table>";
                rowCount = 0;
            } else if (name == u"table-row") {
                if (++rowCount > 500) {
                    continue;
                }
                html += "<tr>";
                rowCellCount = 0;
            } else if (name == u"table-cell") {
                if (rowCount > 500) continue;
                inCell = true;
                cellText.clear();
                auto repAttr = xml.attributes().value(QLatin1String("number-columns-repeated"));
                repeat = repAttr.isEmpty() ? 1 : repAttr.toInt();
                if (repeat <= 0) repeat = 1;
                if (repeat > 20) repeat = 1;
            }
        } else if (token == QXmlStreamReader::Characters && inCell) {
            cellText += xml.text();
        } else if (token == QXmlStreamReader::EndElement) {
            auto name = xml.name();
            if (name == u"table-cell") {
                if (rowCount > 500) continue;
                inCell = false;
                QStringView trimmed = QStringView(cellText).trimmed();
                if (rowCellCount < 40 || !trimmed.isEmpty()) {
                    QString escaped = trimmed.toString().toHtmlEscaped();
                    for (int i = 0; i < repeat; ++i) {
                        html += "<td>";
                        html += escaped;
                        html += "</td>";
                        rowCellCount++;
                    }
                }
                if (!trimmed.isEmpty()) {
                    m_wordCount += countWordsFast(trimmed);
                }
                cellText.clear();
            } else if (name == u"table-row") {
                if (rowCount <= 500) {
                    html += "</tr>";
                }
            } else if (name == u"table") {
                html += "</table>";
            }
        }
    }

    html += "</body></html>";
    return html;
}

bool OfficeViewer::parseOldDoc(const QString &filePath)
{
    // 1. Try antiword only if actually installed on the system
    static QString s_antiword = QStandardPaths::findExecutable("antiword");
    if (!s_antiword.isEmpty()) {
        QProcess proc;
        proc.start(s_antiword, QStringList() << filePath);
        if (proc.waitForFinished(300) && proc.exitCode() == 0) {
            QString text = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            if (text.length() > 20) {
                m_textBrowser->setPlainText(text);
                return true;
            }
        }
    }

    // 2. Intelligent native binary parser for Word 97-2003 (.doc OLE container)
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QByteArray data = f.read(2 * 1024 * 1024);
    f.close();

    QString html = "<html><head><style>"
                   "body { font-family: 'Segoe UI', sans-serif; line-height: 1.6; color: #e2e8f0; padding: 24px; max-width: 850px; margin: auto; }"
                   "h2 { color: #38bdf8; margin-top: 16px; margin-bottom: 8px; }"
                   "p { margin-bottom: 12px; }"
                   "</style></head><body>";

    QStringList blocks;
    QString currentU16;

    // Scan for UTF-16LE text blocks (Word 97-2003 stores text primarily in 16-bit unicode)
    for (int i = 0; i < data.size() - 1; i += 2) {
        quint16 code = static_cast<quint8>(data[i]) | (static_cast<quint8>(data[i + 1]) << 8);
        if ((code >= 32 && code <= 126) || (code >= 160 && code <= 383) || code == 10 || code == 13 || code == 9) {
            currentU16.append(QChar(code));
        } else {
            if (currentU16.length() >= 15) {
                QString s = currentU16.trimmed();
                if (!s.contains("Root Entry") && !s.contains("WordDocument") &&
                    !s.contains("SummaryInformation") && !s.contains("DocumentSummaryInformation") &&
                    !s.contains("CompObj") && !s.contains("Table") && !s.contains("Data") &&
                    !s.contains("Normal") && !s.contains("Body Text")) {
                    blocks.append(s);
                }
            }
            currentU16.clear();
        }
    }

    // If UTF-16LE didn't find substantial text, scan 8-bit ASCII blocks
    if (blocks.isEmpty()) {
        QString currentAscii;
        for (int i = 0; i < data.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
                currentAscii.append(QChar(c));
            } else {
                if (currentAscii.length() >= 15) {
                    QString s = currentAscii.trimmed();
                    if (!s.contains("Microsoft Word") && !s.contains("Root Entry") && !s.contains("WordDocument")) {
                        blocks.append(s);
                    }
                }
                currentAscii.clear();
            }
        }
    }

    if (blocks.isEmpty()) return false;

    m_wordCount = 0;
    for (const QString &block : blocks) {
        QStringList paragraphs = block.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        for (const QString &p : paragraphs) {
            QString clean = p.trimmed();
            if (clean.length() < 3) continue;
            if (clean.startsWith("Capitolo") || clean.startsWith("Relazione")) {
                html += "<h2>" + clean.toHtmlEscaped() + "</h2>";
            } else {
                html += "<p>" + clean.toHtmlEscaped() + "</p>";
            }
            m_wordCount += clean.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
        }
    }

    html += "</body></html>";
    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

bool OfficeViewer::parseOldXls(const QString &filePath)
{
    // Try strings or text extractor for old BIFF8 .xls files
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QByteArray data = f.read(2 * 1024 * 1024);
    f.close();

    QString html = "<html><head><style>"
                   "body { background-color: #1b1e20; color: #e2e8f0; font-family: sans-serif; padding: 16px; }"
                   "p { line-height: 1.5; }"
                   "</style></head><body><h3>📄 Contenuto Rilevato (.xls)</h3>";

    QString currentWord;
    QStringList lines;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c >= 32 && c <= 126) {
            currentWord.append(QChar(c));
        } else {
            if (currentWord.length() >= 3) {
                // Filter out non-printable garbage strings
                if (!currentWord.startsWith("Root Entry") && !currentWord.startsWith("Workbook") &&
                    !currentWord.contains("Microsoft Excel") && !currentWord.contains("Arial")) {
                    lines.append(currentWord);
                }
            }
            currentWord.clear();
        }
    }

    if (lines.isEmpty()) return false;

    for (const QString &line : lines) {
        html += "<p>" + line.toHtmlEscaped() + "</p>";
    }
    html += "</body></html>";

    m_textBrowser->setHtml(html);
    return true;
}

bool OfficeViewer::parseRtf(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QByteArray rtfData = f.readAll();
    f.close();

    // Use a brace-matching group stripper for header metadata groups:
    // {\fonttbl...}, {\colortbl...}, {\stylesheet...}, {\*...}, {\info...}
    auto removeGroup = [](QString &str, const QString &tag) {
        int idx = 0;
        while ((idx = str.indexOf("{\\" + tag, idx)) != -1) {
            int depth = 0;
            int end = idx;
            for (; end < str.length(); ++end) {
                if (str[end] == '{') depth++;
                else if (str[end] == '}') {
                    depth--;
                    if (depth == 0) {
                        end++;
                        break;
                    }
                }
            }
            str.remove(idx, end - idx);
        }
    };

    QString raw = QString::fromUtf8(rtfData);

    removeGroup(raw, "fonttbl");
    removeGroup(raw, "colortbl");
    removeGroup(raw, "stylesheet");
    removeGroup(raw, "*\\listtable");
    removeGroup(raw, "*\\listoverridetable");
    removeGroup(raw, "*\\generator");
    removeGroup(raw, "*\\userprops");
    removeGroup(raw, "*\\ftnsep");
    removeGroup(raw, "*\\");
    removeGroup(raw, "info");

    // Replace \par and \line with newlines
    raw.replace("\\par", "\n");
    raw.replace("\\line", "\n");
    raw.replace("\\tab", "\t");

    // Replace unicode escapes \u232\'e8 or \uXXXX
    raw.replace(QRegularExpression("\\\\u([0-9]+)\\\\'[0-9a-fA-F]{2}"), "\\u\\1");
    QRegularExpression uRegex("\\\\u([0-9]+)");
    QRegularExpressionMatchIterator uIt = uRegex.globalMatch(raw);
    while (uIt.hasNext()) {
        QRegularExpressionMatch match = uIt.next();
        int code = match.captured(1).toInt();
        raw.replace(match.captured(0), QString(QChar(code)));
    }

    // Remove remaining backslash commands \loch, \f5, \fs20, etc.
    raw.remove(QRegularExpression("\\\\[a-zA-Z0-9]+"));
    raw.remove("{");
    raw.remove("}");

    QStringList paragraphs = raw.split("\n", Qt::SkipEmptyParts);
    QString html = "<html><head><style>"
                   "body { font-family: 'Segoe UI', sans-serif; line-height: 1.6; color: #e2e8f0; padding: 24px; max-width: 850px; margin: auto; }"
                   "h2 { color: #38bdf8; margin-top: 16px; margin-bottom: 8px; }"
                   "p { margin-bottom: 12px; }"
                   "</style></head><body>";

    m_wordCount = 0;
    for (const QString &p : paragraphs) {
        QString clean = p.trimmed();
        if (clean.isEmpty() || clean.length() <= 1) continue;
        if (clean.startsWith("Capitolo") || clean.startsWith("Relazione")) {
            html += "<h2>" + clean.toHtmlEscaped() + "</h2>";
        } else {
            html += "<p>" + clean.toHtmlEscaped() + "</p>";
        }
        m_wordCount += clean.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    }
    html += "</body></html>";

    m_textBrowser->setHtml(html);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

bool OfficeViewer::parseEpub(const QString &filePath)
{
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, filePath.toUtf8().constData(), 16384) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry *entry;
    QMap<QString, QByteArray> htmlEntries;
    QByteArray containerXml;
    QString opfPath;
    QByteArray opfData;
    QByteArray coverImage;
    QString coverExt = "jpg";

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *p = archive_entry_pathname(entry);
        if (!p) continue;
        QString path = QString::fromUtf8(p);

        if (path == "META-INF/container.xml") {
            la_int64_t size = archive_entry_size(entry);
            if (size > 0) {
                containerXml.resize(size);
                archive_read_data(a, containerXml.data(), size);
            }
        } else if (path.endsWith(".opf", Qt::CaseInsensitive)) {
            opfPath = path;
            la_int64_t size = archive_entry_size(entry);
            if (size > 0) {
                opfData.resize(size);
                archive_read_data(a, opfData.data(), size);
            }
        } else if (path.endsWith(".html", Qt::CaseInsensitive) || 
                   path.endsWith(".xhtml", Qt::CaseInsensitive) || 
                   path.endsWith(".htm", Qt::CaseInsensitive)) {
            la_int64_t size = archive_entry_size(entry);
            if (size > 0 && size < 5 * 1024 * 1024) { // limit 5MB per chapter
                QByteArray data(size, Qt::Uninitialized);
                archive_read_data(a, data.data(), size);
                htmlEntries[path] = data;
            }
        } else if (coverImage.isEmpty() && (path.contains("cover", Qt::CaseInsensitive)) &&
                   (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".jpeg", Qt::CaseInsensitive) || path.endsWith(".png", Qt::CaseInsensitive))) {
            la_int64_t size = archive_entry_size(entry);
            if (size > 0 && size < 10 * 1024 * 1024) {
                coverImage.resize(size);
                archive_read_data(a, coverImage.data(), size);
                coverExt = path.endsWith(".png", Qt::CaseInsensitive) ? "png" : "jpeg";
            }
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    if (htmlEntries.isEmpty()) {
        return false;
    }

    // Extract title and author from OPF if present
    QString bookTitle = QFileInfo(filePath).completeBaseName();
    QString bookAuthor;
    if (!opfData.isEmpty()) {
        QXmlStreamReader xml(opfData);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == QLatin1String("title")) {
                    bookTitle = xml.readElementText().trimmed();
                } else if (xml.name() == QLatin1String("creator")) {
                    bookAuthor = xml.readElementText().trimmed();
                }
            }
        }
    }

    QString fullHtml;
    fullHtml.reserve(256 * 1024);
    fullHtml += "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
                "body { font-family: 'Segoe UI', 'Ubuntu', sans-serif; line-height: 1.6; color: #e2e8f0; background-color: #1b1e20; padding: 28px 36px; max-width: 820px; margin: auto; }"
                ".header { text-align: center; border-bottom: 2px solid #334155; padding-bottom: 20px; margin-bottom: 30px; }"
                ".book-title { font-size: 26px; font-weight: bold; color: #38bdf8; margin-bottom: 6px; }"
                ".book-author { font-size: 16px; color: #94a3b8; margin-bottom: 16px; }"
                ".cover-box { margin: 15px auto; max-width: 280px; }"
                ".cover-img { width: 100%; border-radius: 6px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); }"
                "h1, h2, h3 { color: #60a5fa; margin-top: 24px; margin-bottom: 12px; }"
                "p { margin-bottom: 14px; text-align: justify; font-size: 14px; }"
                "img { max-width: 100%; height: auto; display: block; margin: 16px auto; border-radius: 4px; }"
                "</style></head><body>";

    // Header banner with title, author and cover
    fullHtml += "<div class=\"header\">";
    fullHtml += QString("<div class=\"book-title\">📖 %1</div>").arg(bookTitle.toHtmlEscaped());
    if (!bookAuthor.isEmpty()) {
        fullHtml += QString("<div class=\"book-author\">Autore: %1</div>").arg(bookAuthor.toHtmlEscaped());
    }
    if (!coverImage.isEmpty()) {
        QString b64 = QString::fromLatin1(coverImage.toBase64());
        fullHtml += QString("<div class=\"cover-box\"><img class=\"cover-img\" src=\"data:image/%1;base64,%2\"/></div>")
                    .arg(coverExt, b64);
    }
    fullHtml += "</div>";

    // Append chapters in alphabetical/path order
    m_wordCount = 0;
    for (auto it = htmlEntries.begin(); it != htmlEntries.end(); ++it) {
        QString rawChapter = QString::fromUtf8(it.value());
        
        // Strip <html>, <head>, <style>, <script>
        rawChapter.remove(QRegularExpression("<style[^>]*>.*?</style>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption));
        rawChapter.remove(QRegularExpression("<script[^>]*>.*?</script>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption));

        // Extract body content if present
        QRegularExpression bodyRegex("<body[^>]*>(.*?)</body>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = bodyRegex.match(rawChapter);
        QString bodyContent = match.hasMatch() ? match.captured(1) : rawChapter;

        fullHtml += "<div class=\"chapter\">";
        fullHtml += bodyContent;
        fullHtml += "</div><hr style=\"border: 0; height: 1px; background: #334155; margin: 30px 0;\"/>";

        m_wordCount += countWordsFast(bodyContent);
    }

    fullHtml += "</body></html>";

    m_textBrowser->setHtml(fullHtml);
    m_textBrowser->verticalScrollBar()->setValue(0);
    return true;
}

