/**
 * @file pdf_signature_dialog.h
 * @brief Multi-tab signature creation, scanning, and management dialog for PDF documents.
 */

#pragma once

#include <QDialog>
#include <QImage>
#include <QPainterPath>
#include <vector>

class QTabWidget;
class QListWidget;
class QSlider;
class QCheckBox;
class QPushButton;
class QLabel;

/**
 * @brief Interactive drawing canvas widget for capturing freehand signatures.
 */
class SignatureCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignatureCanvasWidget(QWidget *parent = nullptr);

    void clearCanvas();
    bool isEmpty() const { return m_paths.empty(); }
    void setStrokeColor(const QColor &color);
    void setStrokeWidth(int width);
    QImage renderSignatureImage() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct Stroke {
        QPainterPath path;
        QColor color;
        int width;
    };

    std::vector<Stroke> m_paths;
    QPainterPath m_currentPath;
    QColor m_currentColor;
    int m_currentWidth;
    bool m_isDrawing;
};

/**
 * @brief Comprehensive dialog supporting freehand drawing, image import with background removal, and saved signature management.
 */
class PdfSignatureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfSignatureDialog(QWidget *parent = nullptr);
    ~PdfSignatureDialog() override;

    /**
     * @brief Returns the final transparent signature image selected or created.
     * @return Transparent QImage.
     */
    QImage signatureImage() const { return m_finalSignature; }

    /**
     * @brief Returns true if the user requested to open the Cryptographic Digital Signature Hub.
     */
    bool isCryptoSignatureRequested() const { return m_cryptoSigRequested; }

private slots:
    void onTabChanged(int index);
    void onColorChanged(const QColor &color);
    void onBrowseImageClicked();
    void onThresholdSliderChanged(int value);
    void onSavedItemSelectionChanged();
    void onDeleteSavedClicked();
    void onAcceptClicked();

private:
    void setupUi();
    void setupDrawTab();
    void setupImageTab();
    void setupSavedTab();
    void setupDigitalTab();
    void loadSavedSignatures();
    void processImportedImage();
    QString signaturesDirPath() const;

    QTabWidget *m_tabs = nullptr;

    // Draw Tab
    SignatureCanvasWidget *m_canvas = nullptr;
    QPushButton *m_btnColorBlack = nullptr;
    QPushButton *m_btnColorBlue = nullptr;
    QSlider *m_sliderWidth = nullptr;
    QPushButton *m_btnClearCanvas = nullptr;

    // Image Tab
    QString m_importedSourcePath;
    QImage m_rawImportedImage;
    QImage m_processedImportedImage;
    QLabel *m_lblImagePreview = nullptr;
    QSlider *m_sliderThreshold = nullptr;
    QCheckBox *m_chkAutoRemoveWhite = nullptr;
    QPushButton *m_btnBrowseImage = nullptr;

    // Saved Tab
    QListWidget *m_listSaved = nullptr;
    QPushButton *m_btnDeleteSaved = nullptr;

    // Common Footer
    QCheckBox *m_chkSaveToLibrary = nullptr;
    QPushButton *m_btnOk = nullptr;
    QPushButton *m_btnCancel = nullptr;

    QImage m_finalSignature;
    bool m_cryptoSigRequested = false;
};
