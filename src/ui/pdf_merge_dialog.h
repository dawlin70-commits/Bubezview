#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QProcess;

/**
 * @class PdfMergeDialog
 * @brief Interactive dialog for combining multiple PDF documents into a single optimized document.
 * 
 * Automatically detects whether QPDF is available on the system. When QPDF is present,
 * it performs font deduplication, structure linearization, and bookmark preservation.
 * Otherwise, it falls back to Poppler's pdfunite with proactive guidance.
 */
class PdfMergeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfMergeDialog(const QStringList &initialFiles = QStringList(), QWidget *parent = nullptr);
    ~PdfMergeDialog() override;

    QString outputFilePath() const;

signals:
    void fileMerged(const QString &outputPath);

private slots:
    void onAddFiles();
    void onRemoveSelected();
    void onMoveUp();
    void onMoveDown();
    void onClearAll();
    void onBrowseOutput();
    void onStartMerge();
    void onProcessFinished(int exitCode);

private:
    void setupUi();
    void updateEngineBanner();
    void checkSmartTip();

    QListWidget *m_fileListWidget;
    QLineEdit *m_editOutput;
    QPushButton *m_btnAdd;
    QPushButton *m_btnRemove;
    QPushButton *m_btnMoveUp;
    QPushButton *m_btnMoveDown;
    QPushButton *m_btnClear;
    QPushButton *m_btnBrowseOutput;
    QPushButton *m_btnMerge;
    QPushButton *m_btnClose;

    QLabel *m_lblEngineStatus;
    QLabel *m_lblStatus;
    QProgressBar *m_progressBar;

    QProcess *m_process;
    QString m_outPath;
    bool m_usingQpdf;
};
