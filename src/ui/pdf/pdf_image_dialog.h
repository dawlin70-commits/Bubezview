/**
 * @file pdf_image_dialog.h
 * @brief Dialog for importing, rotating, and managing images and logos for PDF placement.
 */

#pragma once

#include <QDialog>
#include <QImage>

class QTabWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class QListWidget;

/**
 * @brief Dedicated dialog for inserting images, company logos, and graphic stamps into PDFs.
 */
class PdfImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfImageDialog(QWidget *parent = nullptr);
    ~PdfImageDialog() override;

    /**
     * @brief Returns the processed image ready to be placed on the canvas.
     */
    QImage selectedImage() const;

private slots:
    void onBrowseClicked();
    void onRotateLeftClicked();
    void onRotateRightClicked();
    void processImage();
    void onSavedItemSelectionChanged();
    void onDeleteSavedClicked();
    void accept() override;

private:
    void setupUi();
    void setupImportTab();
    void setupSavedTab();
    void refreshSavedList();
    QString imagesDirPath() const;

    QTabWidget *m_tabs;

    // Import Tab
    QLabel *m_lblPreview;
    QPushButton *m_btnBrowse;
    QPushButton *m_btnRotateLeft;
    QPushButton *m_btnRotateRight;
    QCheckBox *m_chkSaveFavorite;

    // Saved Tab
    QListWidget *m_listSaved;
    QPushButton *m_btnDeleteSaved;

    // Action buttons
    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;

    QImage m_rawImage;
    QImage m_processedImage;
    int m_rotationAngle = 0;
};
