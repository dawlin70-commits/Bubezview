/**
 * @file pdf_repair_dialog.h
 * @brief Interactive dialog for repairing damaged or corrupted PDF files via QPDF recovery.
 */

#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;

/**
 * @class PdfRepairDialog
 * @brief Dialog that guides the user through repairing damaged PDFs using QPDF's xref reconstruction engine.
 */
class PdfRepairDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfRepairDialog(const QString &initialFilePath = QString(), QWidget *parent = nullptr);
    ~PdfRepairDialog() override = default;

    /**
     * @brief Returns the absolute path of the successfully repaired PDF file.
     */
    QString repairedFilePath() const { return m_repairedPath; }

signals:
    void fileRepaired(const QString &filePath);

private slots:
    void onBrowseInput();
    void onBrowseOutput();
    void onStartRepair();

private:
    void setupUi();

    QString m_inputPath;
    QString m_repairedPath;

    QLineEdit *m_editInput;
    QPushButton *m_btnBrowseInput;
    QLineEdit *m_editOutput;
    QPushButton *m_btnBrowseOutput;
    QLabel *m_lblStatus;
    QProgressBar *m_progressBar;
    QPushButton *m_btnRepair;
    QPushButton *m_btnClose;
};
