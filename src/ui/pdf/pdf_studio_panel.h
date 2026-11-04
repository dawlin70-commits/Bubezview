/**
 * @file pdf_studio_panel.h
 * @brief Professional, collapsible right-hand side panel for PDF editing, annotations, and page management.
 *
 * Inherits BaseStudioPanel and provides unified, in-place editing workspaces:
 * - Text & Typewriter: font family, point size, bold/italic/underline, color swatches, whiteout, alignment.
 * - Geometric Shapes: rectangle, rounded rect, ellipse, arrow, line, stroke width, colors, fill.
 * - Watermark & Stamps: preset badges, custom text, style, rotation angle, opacity, colors.
 * - Signature & Image: in-panel drawing pad with smooth strokes, pen colors, insert logo/graphic.
 * - Redaction: privacy blackout and whiteout options with permanent burn warning.
 * - Page Organizer: rotation, deletion, and blank page insertion.
 */

#pragma once

#include "ui/base_studio_panel.h"
#include "pdf_signature_dialog.h"
#include <QFont>
#include <QColor>
#include <QImage>

class QComboBox;
class QSpinBox;
class QSlider;
class QLineEdit;
class QCheckBox;
class QButtonGroup;
class QLabel;

/**
 * @brief Integrated PDF editing studio side panel inheriting BaseStudioPanel shell.
 */
class PdfStudioPanel : public BaseStudioPanel
{
    Q_OBJECT

public:
    explicit PdfStudioPanel(QWidget *parent = nullptr);
    ~PdfStudioPanel() override = default;

    // --- Page status synchronization ---
    void updatePageInfo(int currentPage, int totalPages);

    // --- Text tab properties ---
    QFont currentFont() const;
    QColor textColor() const { return m_textColor; }
    bool isWhiteout() const;
    Qt::Alignment textAlignment() const { return m_textAlignment; }

    // --- Shape tab properties ---
    int shapeType() const { return m_shapeType; }
    int shapeStrokeWidth() const { return m_shapeStrokeWidth; }
    QColor shapeStrokeColor() const { return m_shapeStrokeColor; }
    QColor shapeFillColor() const { return m_shapeFillColor; }

    // --- Watermark tab properties ---
    QString watermarkText() const;
    qreal watermarkAngle() const;
    qreal watermarkOpacity() const;
    QColor watermarkColor() const { return m_watermarkColor; }
    int watermarkStyle() const;

    // --- Redaction tab properties ---
    QColor redactionColor() const { return m_redactionColor; }

signals:
    // Text signals
    void textFontChanged(const QFont &font);
    void textColorChanged(const QColor &color);
    void textWhiteoutChanged(bool whiteout);
    void textAlignmentChanged(Qt::Alignment alignment);
    void placeTextRequested();

    // Shape signals
    void shapeParamsChanged(int type, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);
    void drawShapeRequested();

    // Watermark signals
    void watermarkParamsChanged(const QString &text, qreal angle, qreal opacity, const QColor &color, int style);
    void placeWatermarkRequested();

    // Signature & Image signals
    void placeSignatureRequested(const QImage &image);
    void insertImageRequested();
    void openLargeSignaturePadRequested();
    void cryptoSignatureRequested();
    void verifyCryptoSignaturesRequested();

    // Redaction signals
    void redactionColorChanged(const QColor &color);
    void startRedactionRequested();

    // Page operations signals
    void rotatePageRequested(int angle);
    void deletePageRequested(int pageIndex);
    void insertBlankPageRequested(int afterPageIndex);
    void pageSelected(int pageIndex);
    void openPageOrganizerRequested();
    void saveCurrentPageAsPdfRequested();
    void saveCurrentPageAsImageRequested();

protected:
    void customizeTheme(QString &qss, bool isDark) override;

private:
    void setupPages();
    QWidget *createTextPage();
    QWidget *createShapesPage();
    QWidget *createWatermarkPage();
    QWidget *createSignaturePage();
    QWidget *createRedactionPage();
    QWidget *createPagesPage();

    void updateTextColorButtons();
    void updateShapeColorButtons();
    void updateWatermarkColorButtons();
    void updateRedactionColorButtons();

    // Text Tab controls
    QComboBox *m_comboFontFamily = nullptr;
    QSpinBox *m_spinFontSize = nullptr;
    QToolButton *m_btnBold = nullptr;
    QToolButton *m_btnItalic = nullptr;
    QToolButton *m_btnUnderline = nullptr;
    QList<QToolButton *> m_textColorButtons;
    QToolButton *m_btnTextCustomColor = nullptr;
    QCheckBox *m_chkWhiteout = nullptr;
    QToolButton *m_btnAlignLeft = nullptr;
    QToolButton *m_btnAlignCenter = nullptr;
    QToolButton *m_btnAlignRight = nullptr;
    QColor m_textColor = Qt::black;
    Qt::Alignment m_textAlignment = Qt::AlignLeft;

    // Shape Tab controls
    QComboBox *m_comboShapeType = nullptr;
    QSlider *m_sliderShapeWidth = nullptr;
    QLabel *m_lblShapeWidthVal = nullptr;
    QList<QToolButton *> m_shapeStrokeButtons;
    QToolButton *m_btnShapeCustomStroke = nullptr;
    QCheckBox *m_chkShapeNoFill = nullptr;
    QList<QToolButton *> m_shapeFillButtons;
    QToolButton *m_btnShapeCustomFill = nullptr;
    int m_shapeType = 0;
    int m_shapeStrokeWidth = 2;
    QColor m_shapeStrokeColor = QColor(QStringLiteral("#ef4444"));
    QColor m_shapeFillColor = Qt::transparent;

    // Watermark Tab controls
    QLineEdit *m_editWatermarkText = nullptr;
    QComboBox *m_comboWatermarkStyle = nullptr;
    QSlider *m_sliderWatermarkAngle = nullptr;
    QLabel *m_lblWatermarkAngleVal = nullptr;
    QSlider *m_sliderWatermarkOpacity = nullptr;
    QLabel *m_lblWatermarkOpacityVal = nullptr;
    QList<QToolButton *> m_watermarkColorButtons;
    QToolButton *m_btnWatermarkCustomColor = nullptr;
    QColor m_watermarkColor = QColor(QStringLiteral("#dc2626"));

    // Signature Tab controls
    SignatureCanvasWidget *m_sigCanvas = nullptr;
    QSlider *m_sliderSigWidth = nullptr;
    QList<QToolButton *> m_sigColorButtons;

    // Redaction Tab controls
    QList<QToolButton *> m_redactColorButtons;
    QToolButton *m_btnRedactCustomColor = nullptr;
    QColor m_redactionColor = Qt::black;

    // Pages Tab controls
    QLabel *m_lblPageStatus = nullptr;
    QSpinBox *m_spinCurrentPage = nullptr;
    int m_currentPage = 0;
    int m_totalPages = 0;
};
