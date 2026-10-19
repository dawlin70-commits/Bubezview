/**
 * @file pdf_page_organizer_dialog.h
 * @brief Visual drag-and-drop PDF page organizer, reorderer, rotator, and page remover.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QProgressBar;

namespace Poppler {
    class Document;
}

/**
 * @class PdfPageOrganizerDialog
 * @brief Interactive visual page manager for PDF documents utilizing QPDF lossless page pipeline.
 */
class PdfPageOrganizerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfPageOrganizerDialog(const QString &pdfPath, QWidget *parent = nullptr);
    ~PdfPageOrganizerDialog() override;

    /**
     * @brief Returns the output path if saved or overwritten.
     */
    QString outputFilePath() const { return m_outPath; }

signals:
    void fileOrganized(const QString &filePath);

private slots:
    void onRotateLeft();
    void onRotateRight();
    void onDeleteSelected();
    void onReverseOrder();
    void onResetOrder();
    void onSaveAs();
    void onSaveOverwrite();
    void onSelectionChanged();

private:
    void setupUi();
    void loadThumbnails();
    void updateItemVisuals(QListWidgetItem *item);
    void updateStatsLabel();
    bool executeQpdfReorder(const QString &destinationPath);

    QString m_pdfPath;
    QString m_outPath;
    std::unique_ptr<Poppler::Document> m_popplerDoc;

    QLabel *m_lblHeader;
    QLabel *m_lblStats;
    QListWidget *m_listWidget;
    QPushButton *m_btnRotateLeft;
    QPushButton *m_btnRotateRight;
    QPushButton *m_btnDelete;
    QPushButton *m_btnReverse;
    QPushButton *m_btnReset;
    QPushButton *m_btnSaveAs;
    QPushButton *m_btnSaveOverwrite;
    QPushButton *m_btnClose;
    QProgressBar *m_progressBar;
};
