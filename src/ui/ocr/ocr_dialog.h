#ifndef OCR_DIALOG_H
#define OCR_DIALOG_H

#include "dialogs/frameless_dialog.h"
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QCloseEvent>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

/**
 * @brief Modern, dedicated dialog for reviewing, copying, saving, and re-extracting OCR text.
 *
 * Supports language selection, 1-click downloading of new languages via an embedded
 * settings gear (⚙️), PDF page vs. full-document extraction, and live word/character statistics.
 */
class OcrDialog : public FramelessDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the OCR dialog.
     * @param initialText Text initially recognized.
     * @param sourceName Human-readable name of the file being processed.
     * @param sourcePath Absolute path to the source file.
     * @param isPdf Whether the source file is a PDF document.
     * @param currentPage 0-based index of the currently viewed page.
     * @param totalPages Total number of pages in the PDF (or 1 for images).
     * @param pageRenderer Optional function to render any page of the document at high DPI.
     * @param parent Parent widget.
     */
    explicit OcrDialog(const QString &initialText,
                       const QString &sourceName,
                       const QString &sourcePath,
                       bool isPdf = false,
                       int currentPage = 0,
                       int totalPages = 1,
                       std::function<QImage(int)> pageRenderer = nullptr,
                       QWidget *parent = nullptr);

    ~OcrDialog() override;

private slots:
    void updateStats();
    void copyToClipboard();
    void restoreCopyButton();
    void saveToFile();
    void openLanguageManager();
    void refreshLanguagesCombo();
    void runExtraction();
    void cleanText();
    void startAreaSnipping();
    void handleScopePageClicked();
    void handleScopeAllClicked();

public:
    /**
     * @brief Filters common OCR noise artifacts, spurious graphic placeholders, and isolated symbols.
     * @param rawText Text extracted from OCR.
     * @return Cleaned, formatted text.
     */
    static QString sanitizeOcrArtifacts(const QString &rawText);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setControlsBusy(bool busy);
    QString currentSelectedLanguageCode() const;

    QString m_sourceName;
    QString m_sourcePath;
    bool m_isPdf;
    int m_currentPage;
    int m_totalPages;
    std::function<QImage(int)> m_pageRenderer;

    QString m_singlePageText;
    QString m_fullDocText;
    bool m_isShowingFullDoc;
    QString m_rawTextBeforeClean;
    bool m_isCleaned;

    std::unique_ptr<std::thread> m_workerThread;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    bool m_isExtracting = false;

    QLabel *m_lblInfo;
    QLabel *m_lblStats;
    QPushButton *m_btnScopePage;
    QPushButton *m_btnScopeAll;
    QPushButton *m_btnSnipArea;
    QComboBox *m_comboLanguage;
    QPushButton *m_btnSettingsGear;
    QPushButton *m_btnRunOcr;
    QPushButton *m_btnCleanText;
    QProgressBar *m_progressBar;

    QPlainTextEdit *m_textEdit;
    QPushButton *m_btnSave;
    QPushButton *m_btnCopy;
    QPushButton *m_btnClose;
};

#endif // OCR_DIALOG_H
