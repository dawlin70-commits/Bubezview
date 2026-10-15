#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <QByteArray>
#include <QThread>
#include <QProcess>

class IPdfViewer;

class OfficeConvertWorker : public QThread
{
    Q_OBJECT
public:
    explicit OfficeConvertWorker(const QString &sourcePath, QObject *parent = nullptr);
    void cancel();

signals:
    void conversionFinished(const QString &sourcePath, const QString &pdfPath);
    void conversionFailed(const QString &sourcePath, const QString &errorMsg);

protected:
    void run() override;

private:
    QString m_sourcePath;
    std::atomic<bool> m_cancelled{false};
    QProcess *m_process{nullptr};
};

/**
 * @brief Hybrid office document viewer (Word, Excel, OpenDocument, RTF, EPUB).
 *
 * Implements a dual-engine architecture:
 * 1. Instant fallback engine: internal lightweight C++ parsers that extract XML
 *    structure and text into formatted rich HTML instantly (<50ms).
 * 2. Background LibreOffice engine: asynchronous headless conversion worker that
 *    seamlessly upgrades the view to high-fidelity PDF rendering when available.
 */
class OfficeViewer : public QWidget
{
    Q_OBJECT

public:
    explicit OfficeViewer(QWidget *parent = nullptr);
    ~OfficeViewer() override;

    /**
     * @brief Opens and renders an office document.
     * Loads the internal fast C++ parser immediately, and launches asynchronous LibreOffice conversion if enabled.
     * @param filePath Absolute path to the office file.
     * @return True if document parsing started successfully.
     */
    bool loadFile(const QString &filePath);

    /**
     * @brief Parses and displays the file solely using internal C++ parser without external tools.
     * @param filePath Target document path.
     * @return True if parsed successfully.
     */
    bool loadFileInternal(const QString &filePath);

    /**
     * @brief Cleans up workers, temp files, and viewer state.
     */
    void clear();

    /**
     * @brief Refreshes visual theme styling of toolbar, text browser, and internal PDF viewer.
     */
    void updateTheme();

    int wordCount() const { return m_wordCount; }
    bool isPdfMode() const { return m_isPdfMode; }
    QString convertedPdfPath() const { return m_convertedPdfPath; }
    IPdfViewer *pdfViewer() const { return m_pdfViewer; }
    QTextBrowser *textBrowser() const { return m_textBrowser; }

private slots:
    void onConversionFinished(const QString &sourcePath, const QString &pdfPath);
    void onConversionFailed(const QString &sourcePath, const QString &errorMsg);
    void onOpenInLibreOffice();
    void onSaveAsPdf();
    void onExtractImages();

private:
    void setupUi();
    bool parseInternal(const QString &filePath);
    bool parseDocx(const QString &filePath);
    bool parseOdt(const QString &filePath);
    bool parseOds(const QString &filePath);
    bool parseXlsx(const QString &filePath);
    bool parseOldDoc(const QString &filePath);
    bool parseOldXls(const QString &filePath);
    bool parseRtf(const QString &filePath);
    bool parseEpub(const QString &filePath);

    QByteArray extractZipEntry(const QString &zipPath, const QString &entryName);
    QMap<QString, QByteArray> extractZipEntries(const QString &zipPath, const QStringList &entryNames);
    QString parseWordXmlToHtml(const QByteArray &xmlData);
    QString parseOdtXmlToHtml(const QByteArray &xmlData);
    QString parseOdsXmlToHtml(const QByteArray &xmlData);
    QString parseSpreadsheetXmlToHtml(const QByteArray &sheetData, const QByteArray &sharedStringsData);

    // Toolbar & Navigation
    QWidget *m_toolbarWidget;
    QPushButton *m_btnEditLo;
    QPushButton *m_btnSavePdf;
    QPushButton *m_btnExtractImg;
    QLabel *m_lblDocInfo;

    // View stack: 0 = Loading, 1 = PDF (Poppler), 2 = Text / HTML Fallback
    QStackedWidget *m_stack;
    QWidget *m_loadingWidget;
    QLabel *m_lblLoading;
    IPdfViewer *m_pdfViewer;
    QTextBrowser *m_textBrowser;

    OfficeConvertWorker *m_worker;
    QString m_currentFilePath;
    QString m_convertedPdfPath;
    int m_wordCount;
    bool m_isPdfMode;
};
