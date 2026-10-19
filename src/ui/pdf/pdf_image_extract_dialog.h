#ifndef PDF_IMAGE_EXTRACT_DIALOG_H
#define PDF_IMAGE_EXTRACT_DIALOG_H

#include <QDialog>
#include <QString>
#include <QList>
#include <QImage>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QTemporaryDir;

/**
 * @struct PdfExtractedImageInfo
 * @brief Holds metadata and preview thumbnail for an embedded image extracted from a PDF.
 */
struct PdfExtractedImageInfo {
    QString filePath;
    int index = 0;
    int width = 0;
    int height = 0;
    qint64 sizeBytes = 0;
    QString format;
    QImage thumbnail;
};

/**
 * @class PdfImageExtractDialog
 * @brief Interactive gallery dialog for previewing and selectively exporting embedded images from a PDF.
 *
 * Replaces automatic batch extraction with an intuitive thumbnail grid where users
 * can inspect image resolutions, select individual or multiple images, save chosen images
 * to a custom directory, save a specific image with a custom name and format, or copy directly
 * to the system clipboard.
 */
class PdfImageExtractDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the PDF image extraction dialog.
     * @param pdfPath Absolute path to the source PDF document.
     * @param parent Optional parent widget.
     */
    explicit PdfImageExtractDialog(const QString &pdfPath, QWidget *parent = nullptr);

    /**
     * @brief Destructor. Automatically cleans up the temporary extraction directory.
     */
    ~PdfImageExtractDialog() override;

    /**
     * @brief Executes image extraction and loads thumbnails.
     * @return Number of images found, or -1 if extraction utility failed.
     */
    int loadImages();

private slots:
    void onSelectAll();
    void onDeselectAll();
    void onItemChanged(QListWidgetItem *item);
    void onItemSelectionChanged();
    void onCopyCurrentToClipboard();
    void onSaveCurrentAs();
    void onSaveSelected();
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    void setupUi();
    void updateSelectionStats();
    QString formatFileSize(qint64 bytes) const;

    QString m_pdfPath;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QList<PdfExtractedImageInfo> m_images;

    QLabel *m_headerLabel;
    QLabel *m_statsLabel;
    QListWidget *m_listWidget;
    QPushButton *m_btnSelectAll;
    QPushButton *m_btnDeselectAll;
    QPushButton *m_btnCopyClipboard;
    QPushButton *m_btnSaveAs;
    QPushButton *m_btnSaveSelected;
    QPushButton *m_btnClose;
};

#endif // PDF_IMAGE_EXTRACT_DIALOG_H
