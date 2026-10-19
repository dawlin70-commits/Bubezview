#ifndef OCR_LANGUAGE_DIALOG_H
#define OCR_LANGUAGE_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>

/**
 * @brief Modal dialog to manage, search, and download OCR language models.
 *
 * Provides a clean interface to view installed Tesseract models, download additional
 * languages directly from the official repository into user space without requiring
 * administrative permissions, or specify custom ISO language codes.
 */
class OcrLanguageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OcrLanguageDialog(QWidget *parent = nullptr);
    ~OcrLanguageDialog() override = default;

private slots:
    void refreshLanguagesList();
    void filterLanguages(const QString &query);
    void handleDownloadClicked(const QString &langCode, QPushButton *btn, QProgressBar *bar);
    void handleCustomDownload();
    void onDownloadProgress(const QString &langCode, qint64 rec, qint64 tot);
    void onDownloadFinished(const QString &langCode, bool success, const QString &err);

private:
    void setupUi();

    QLineEdit *m_searchEdit;
    QTableWidget *m_table;
    QLineEdit *m_customCodeEdit;
    QPushButton *m_btnCustomDownload;
    QLabel *m_statusLabel;

    QMap<QString, QProgressBar *> m_activeBars;
    QMap<QString, QPushButton *> m_activeButtons;
};

#endif // OCR_LANGUAGE_DIALOG_H
