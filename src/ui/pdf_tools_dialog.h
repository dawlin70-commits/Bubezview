/**
 * @file pdf_tools_dialog.h
 * @brief Unified multi-tab PDF markup and tools dialog with persistent continuous mode and graphic libraries.
 *
 * Centralizes all PDF annotation, drawing, and markup tools:
 * - 🖍️ Highlighter (color presets, custom color, opacity slider, live preview)
 * - 📐 Shapes & Arrows (rectangles, ellipses, lines, arrows, stroke, fill)
 * - ⬛ Confidential Redaction / Blackout (blackout or whiteout eraser)
 * - 🔖 Watermarks and standard stamps (presets and custom text)
 * - 🖼️ Image & Emblem insertion (file browser + saved emblems/logos gallery)
 * - ✍️ Signatures (drawing studio, scan import with white-removal, saved library, digital signature roadmap)
 */

#pragma once

#include <QDialog>
#include <QColor>
#include <QImage>
#include "pdf_shape_dialog.h"

class QTabWidget;
class QSlider;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QFrame;
class QListWidget;
class SignatureCanvasWidget;

/**
 * @brief The action triggered when the user accepts one of the tool tabs.
 */
enum class PdfToolAction {
    None,
    Highlighter,
    Shape,
    Redact,
    Watermark,
    InsertImage,
    Signature,
    CryptoSignature
};

/**
 * @brief Tab indices for direct tab switching.
 */
enum class PdfToolTab {
    Highlighter = 0,
    Shapes = 1,
    Redact = 2,
    Watermark = 3,
    Image = 4,
    Signature = 5
};

/**
 * @brief Unified dialog aggregating all PDF markup and insertion tools.
 */
class PdfToolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfToolsDialog(int defaultTab = 0, QWidget *parent = nullptr);
    ~PdfToolsDialog() override;

    PdfToolAction selectedAction() const { return m_selectedAction; }

    // --- Highlighter Settings ---
    QColor highlighterColor() const { return m_highlighterColor; }
    double highlighterOpacity() const { return m_highlighterOpacity; }

    // --- Shape Settings ---
    PdfShapeType shapeType() const;
    int shapeStrokeWidth() const;
    QColor shapeStrokeColor() const { return m_shapeStrokeColor; }
    QColor shapeFillColor() const;

    // --- Redaction Settings ---
    bool isRedactionWhiteout() const;

    // --- Watermark Settings ---
    QString watermarkText() const;
    qreal watermarkAngle() const;
    qreal watermarkOpacity() const;
    QColor watermarkColor() const { return m_watermarkColor; }

    // --- Image Settings ---
    QImage imageToInsert() const;

    // --- Signature Settings ---
    QImage signatureImage() const;

    void setActiveTab(int tabIndex);

private slots:
    // Highlighter
    void onHighlighterColorPresetClicked();
    void onHighlighterCustomColorClicked();
    void onHighlighterOpacityChanged(int val);
    void onApplyHighlighter();

    // Shapes
    void onShapeStrokeColorClicked();
    void onShapeFillColorClicked();
    void onApplyShape();

    // Redaction
    void onApplyRedaction();

    // Watermark
    void onWatermarkPresetChanged(int index);
    void onWatermarkColorClicked();
    void onApplyWatermark();

    // Image
    void onBrowseImageClicked();
    void onSavedImageSelectionChanged();
    void onDeleteSavedImageClicked();
    void onApplyImage();

    // Signature
    void onColorChanged(const QColor &color);
    void onBrowseSignatureImageClicked();
    void onThresholdSliderChanged(int value);
    void onSavedSigSelectionChanged();
    void onDeleteSavedSigClicked();
    void onApplySignature();

private:
    void setupUi();
    QWidget *createHighlighterTab();
    QWidget *createShapesTab();
    QWidget *createRedactTab();
    QWidget *createWatermarkTab();
    QWidget *createImageTab();
    QWidget *createSignatureTab();

    void updateHighlighterPreview();
    void updateShapeColorsUi();
    void processImportedSignatureImage();

    // Storage directories
    QString signaturesDirPath() const;
    QString savedImagesDirPath() const;
    void loadSavedSignatures();
    void loadSavedImages();

    QTabWidget *m_tabWidget;
    PdfToolAction m_selectedAction;

    // --- Tab 1: Highlighter ---
    QColor m_highlighterColor;
    double m_highlighterOpacity;
    QSlider *m_sliderHlOpacity;
    QLabel *m_lblHlOpacityVal;
    QFrame *m_hlPreviewBox;
    QPushButton *m_btnHlCustomColor;

    // --- Tab 2: Shapes & Arrows ---
    QRadioButton *m_radioRect;
    QRadioButton *m_radioEllipse;
    QRadioButton *m_radioLine;
    QRadioButton *m_radioArrow;
    QSpinBox *m_spinShapeWidth;
    QPushButton *m_btnShapeStrokeColor;
    QColor m_shapeStrokeColor;
    QCheckBox *m_chkShapeFill;
    QPushButton *m_btnShapeFillColor;
    QColor m_shapeFillColor;

    // --- Tab 3: Redact ---
    QRadioButton *m_radioRedactBlack;
    QRadioButton *m_radioRedactWhite;

    // --- Tab 4: Watermark ---
    QComboBox *m_comboWkPresets;
    QLineEdit *m_txtWkCustom;
    QComboBox *m_comboWkAngle;
    QSlider *m_sliderWkOpacity;
    QLabel *m_lblWkOpacityVal;
    QPushButton *m_btnWkColor;
    QColor m_watermarkColor;

    // --- Tab 5: Image ---
    QTabWidget *m_imageSubTabs;
    QPushButton *m_btnBrowseImage;
    QLabel *m_lblImagePath;
    QLabel *m_lblImagePreview;
    QCheckBox *m_chkSaveImageToLibrary;
    QImage m_imageFromFile;
    QListWidget *m_listSavedImages;
    QPushButton *m_btnDeleteSavedImage;
    QPushButton *m_btnApplyImage;

    // --- Tab 6: Signature ---
    QTabWidget *m_sigSubTabs;
    SignatureCanvasWidget *m_sigCanvas;
    QPushButton *m_btnSigColorBlack;
    QPushButton *m_btnSigColorBlue;
    QSlider *m_sliderSigWidth;
    QPushButton *m_btnClearSigCanvas;

    QLabel *m_lblSigScanPreview;
    QPushButton *m_btnBrowseSigScan;
    QCheckBox *m_chkAutoRemoveWhite;
    QSlider *m_sliderSigThreshold;
    QImage m_rawSigImportImage;
    QImage m_processedSigImportImage;

    QListWidget *m_listSavedSigs;
    QPushButton *m_btnDeleteSavedSig;
    QCheckBox *m_chkSaveSigToLibrary;

    QImage m_finalSignatureImage;
};
