#ifndef PDF_IMAGE_EXTRACT_DIALOG_H
#define PDF_IMAGE_EXTRACT_DIALOG_H

#include "dialogs/frameless_dialog.h"
#include <QString>
#include <QList>
#include <QImage>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QProgressBar;
class QTemporaryDir;
class QProcess;

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
class PdfImageExtractDialog : public FramelessDialog
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
    void startExtraction();

private slots:
    void onSelectAll();
    void onDeselectAll();
    void onItemChanged(QListWidgetItem *item);
    void onItemSelectionChanged();
    void onCopyCurrentToClipboard();
    void onSaveCurrentAs();
    void onSaveSelected();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onProcessFinished(int exitCode);

private:
    void setupUi();
    void updateSelectionStats();
    void populateImagesFromDirectory();
    QString formatFileSize(qint64 bytes) const;

    QString m_pdfPath;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QList<PdfExtractedImageInfo> m_images;
    QProcess *m_extractProcess = nullptr;

    QLabel *m_headerLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statsLabel = nullptr;
    QListWidget *m_listWidget = nullptr;
    QPushButton *m_btnSelectAll = nullptr;
    QPushButton *m_btnDeselectAll = nullptr;
    QPushButton *m_btnCopyClipboard = nullptr;
    QPushButton *m_btnSaveAs = nullptr;
    QPushButton *m_btnSaveSelected = nullptr;
    QPushButton *m_btnClose = nullptr;
};

#endif // PDF_IMAGE_EXTRACT_DIALOG_H
