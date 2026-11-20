/**
 * @file pdf_transform_box.h
 * @brief Interactive 8-handle transformation overlay for placing items on PDF pages.
 *
 * Allows users to move, scale, and visually preview text, signatures, watermarks,
 * shapes, and redaction boxes before committing them to the PDF document.
 */

#pragma once

#include <QWidget>
#include <QRectF>
#include <QImage>
#include <QFont>
#include <QColor>
#include "core/interfaces/i_pdf_viewer.h"

class QPushButton;
class QHBoxLayout;
class QTextEdit;

/**
 * @brief Types of items that can be placed and transformed via PdfTransformBox.
 */
enum class PdfTransformItemType {
    Text,           ///< Typed text block or typewriter entry
    Signature,      ///< Hand-drawn or imported transparent signature
    Image,          ///< Inserted photo, logo, or graphic
    Watermark,      ///< Angled text stamp / watermark
    Shape,          ///< Geometric shape or arrow
    Redaction,      ///< Black-out privacy redaction box
    AreaSelection   ///< Resizable and movable area selection box for Cut and Copy
};

/**
 * @brief Interactive bounding box widget featuring 8 resize handles and an action pill toolbar.
 */
class PdfTransformBox : public QWidget
{
    Q_OBJECT

public:
    enum Handle {
        None,
        TopLeft,
        TopCenter,
        TopRight,
        MidRight,
        BottomRight,
        BottomCenter,
        BottomLeft,
        MidLeft,
        Body
    };

    explicit PdfTransformBox(QWidget *parent = nullptr);
    ~PdfTransformBox() override;

    /**
     * @brief Activates the transform box for in-place text editing on canvas.
     * @param text Content string.
     * @param font Font family and size.
     * @param textColor Color of the text.
     * @param bgMode Background coverage mode (Auto, Transparent, Feathered, Custom).
     * @param bgColor Background tint color.
     * @param initialRect Exact target rectangle in canvas pixels (if valid), or empty to center at initialPos.
     * @param initialPos Fallback position if initialRect is invalid.
     */
    void showText(const QString &text, const QFont &font, const QColor &textColor,
                  PdfTextBgMode bgMode = PdfTextBgMode::Transparent, const QColor &bgColor = QColor(),
                  const QRect &initialRect = QRect(), const QPoint &initialPos = QPoint());

    /**
     * @brief Activates the transform box for a text block (backwards compatible).
     * @param text Content string.
     * @param font Font family and size.
     * @param textColor Color of the text.
     * @param whiteout True to render opaque white background to cover underlying text.
     * @param initialPos Center point in widget coordinates.
     */
    void showText(const QString &text, const QFont &font, const QColor &textColor,
                  bool whiteout, const QPoint &initialPos);

    /**
     * @brief Activates the transform box for an image or signature.
     * @param image High-resolution image (with transparency).
     * @param initialPos Center point in widget coordinates.
     * @param defaultSize Initial display size.
     * @param type Item type (defaults to Signature for backwards compatibility).
     */
    void showImage(const QImage &image, const QPoint &initialPos,
                   const QSize &defaultSize = QSize(200, 80),
                   PdfTransformItemType type = PdfTransformItemType::Signature);

    /**
     * @brief Activates the transform box for a watermark / stamp.
     * @param text Stamp text (e.g. "COPY", "DRAFT").
     * @param angle Rotation angle in degrees (e.g. 45.0).
     * @param opacity Translucency (0.1 to 1.0).
     * @param color Stamp color.
     * @param initialPos Center point in widget coordinates.
     * @param style Stamp graphic style (0: ClassicRounded, 1: OfficialDouble, 2: Dotted, 3: PlainText).
     */
    void showWatermark(const QString &text, qreal angle, qreal opacity,
                      const QColor &color, const QPoint &initialPos, int style = 0);

    /**
     * @brief Updates active watermark parameters in real-time.
     */
    void updateWatermark(const QString &text, qreal angle, qreal opacity, const QColor &color, int style);

    /**
     * @brief Activates the transform box for an interactive persistent geometric shape.
     * @param shapeType 0: Rectangle, 1: Rounded Rect, 2: Arrow, 3: Line, 4: Ellipse.
     * @param strokeWidth Stroke line width in pixels.
     * @param strokeColor Stroke border color.
     * @param fillColor Fill brush color.
     * @param initialRect Starting bounding rectangle on page.
     * @param localStart Start point relative to initialRect for Arrow/Line.
     * @param localEnd End point relative to initialRect for Arrow/Line.
     */
    void showShape(int shapeType, int strokeWidth, const QColor &strokeColor,
                   const QColor &fillColor, const QRect &initialRect,
                   const QPoint &localStart = QPoint(), const QPoint &localEnd = QPoint());

    /**
     * @brief Updates shape parameters in real-time.
     */
    void updateShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);
    void setShapeType(int shapeType);
    void setShapeStrokeWidth(int width);
    void setShapeStrokeColor(const QColor &color);
    void setShapeFillColor(const QColor &color);

    int shapeType() const { return m_shapeType; }
    int shapeStrokeWidth() const { return m_shapeStrokeWidth; }
    QColor shapeStrokeColor() const { return m_shapeStrokeColor; }
    QColor shapeFillColor() const { return m_shapeFillColor; }
    QPoint shapeLocalStart() const { return m_shapeLocalStart; }
    QPoint shapeLocalEnd() const { return m_shapeLocalEnd; }

    /**
     * @brief Updates text styling and content in real-time from the side panel.
     */
    void updateTextContent(const QString &text);
    void updateTextFont(const QFont &font);
    void updateTextColor(const QColor &color);
    void updateTextWhiteout(bool whiteout);
    void updateTextAlignment(Qt::Alignment alignment);

    Qt::Alignment textAlignment() const { return m_textAlignment; }
    void adjustHeightToFitText();

    /**
     * @brief Sets the page zoom scale factor (screen pixels per PDF point).
     */
    void setScaleFactor(double scaleFactor);
    double scaleFactor() const { return m_scaleFactor; }

    /**
     * @brief Computes font scaled to current screen zoom level for pixel-perfect on-screen preview.
     */
    QFont screenFont() const;



    /**
     * @brief Activates the transform box for a permanent redaction block.
     * @param initialRect Starting widget rectangle.
     */
    void showRedaction(const QRect &initialRect);

    /**
     * @brief Activates the transform box for an interactive, resizable area selection.
     * @param initialRect Starting widget rectangle.
     */
    void showAreaSelection(const QRect &initialRect);

    /**
     * @brief Sets the fill color for redaction/coverage.
     */
    void setRedactionColor(const QColor &color);

    /**
     * @brief Draws auto-fitted watermark stamp vector graphics cleanly inside target rect.
     */
    static void drawWatermarkStamp(QPainter &painter, const QRect &rect,
                                   const QString &text, qreal angle,
                                   qreal opacity, const QColor &color,
                                   int style);

    /**
     * @brief Hides the transform box and clears current item data.
     */
    void dismiss();

    PdfTransformItemType itemType() const { return m_itemType; }
    QRect boxRect() const { return m_boxRect; }
    void setBoxRect(const QRect &rect);
    QTextEdit *inlineTextEdit() const { return m_inlineTextEdit; }

    QString text() const;
    QFont textFont() const { return m_font; }
    QColor textColor() const { return m_color; }
    bool isWhiteout() const { return m_whiteout; }
    PdfTextBgMode textBgMode() const { return m_textBgMode; }
    void setTextBgMode(PdfTextBgMode mode);
    QColor textBgColor() const { return m_textBgColor; }
    void setTextBgColor(const QColor &color);
    QImage image() const { return m_image; }
    void fitToPageWidth();
    void fitToFullPage();
    qreal watermarkAngle() const { return m_angle; }
    qreal watermarkOpacity() const { return m_opacity; }
    int watermarkStyle() const { return m_watermarkStyle; }
    QColor redactionColor() const { return m_redactionColor; }
    bool isTextModified() const { return m_isTextModified; }
    void setTextModified(bool modified) { m_isTextModified = modified; }

    /**
     * @brief Switches text box to solid draggable placement mode (hides inline editor so user can drag freely).
     * @param placementOnly True to activate placement mode, false for editing mode.
     * @param centerOnPoint If valid and topLeftPos is null, centers the box around this point.
     * @param suggestedWidth Optional suggested width (e.g. from original text block or previous width).
     * @param topLeftPos If valid, positions the box's top-left corner at this point instead of centering.
     */
    void setPlacementMode(bool placementOnly, const QPoint &centerOnPoint = QPoint(),
                          int suggestedWidth = 0, const QPoint &topLeftPos = QPoint());
    bool isPlacementMode() const { return m_isPlacementMode; }

    /**
     * @brief Sets whether the current box geometry collides with existing page text.
     */
    void setHasCollision(bool collision);
    bool hasCollision() const { return m_hasCollision; }

    QRect dragHandleRect() const;

signals:
    /**
     * @brief Emitted when placement vs in-place edit mode toggles.
     */
    void placementModeChanged(bool isPlacement);

    /**
     * @brief Emitted whenever the box position or size changes during user interaction.
     */
    void geometryChanged(const QRect &newRect);

    /**
     * @brief Emitted when the user clicks 'Apply' or presses Enter.
     */
    void applied();

    /**
     * @brief Emitted when the user clicks 'Stamp Again' to commit the current item and keep another copy active.
     */
    void applyAndContinue();

    /**
     * @brief Emitted when the user clicks 'Cancel' or presses Escape.
     */
    void cancelled();

    /**
     * @brief Emitted when the user clicks 'Edit' on the floating toolbar to modify content.
     */
    void editRequested();

    /**
     * @brief Emitted when the user requests cutting the selected area.
     */
    void cutAreaRequested();

    /**
     * @brief Emitted when the user requests copying selectable vector text from the selected area.
     */
    void copyAreaTextRequested();

    /**
     * @brief Emitted when the user requests copying the selected area as an image.
     */
    void copyAreaRequested();

    /**
     * @brief Emitted when the user requests OCR recognition on the selected area.
     */
    void ocrAreaRequested();

    /**
     * @brief Emitted when the user presses Undo (Ctrl+Z).
     */
    void undoRequested();

    /**
     * @brief Emitted when the user presses Redo (Ctrl+Y / Ctrl+Shift+Z).
     */
    void redoRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateHandles();
    Handle handleAt(const QPoint &pos) const;
    void repositionToolbar();
    void updateCachedPixmap(bool fast = false);
    void updateRedactionColorButtons();
    void updateInlineTextEditStyle();

    PdfTransformItemType m_itemType;
    QRect m_boxRect;
    Handle m_activeHandle;
    QPoint m_dragStartPos;
    QRect m_dragStartRect;

    // Item properties
    QString m_text;
    QFont m_font;
    QColor m_color;
    bool m_whiteout = false;
    PdfTextBgMode m_textBgMode = PdfTextBgMode::Transparent;
    QColor m_textBgColor = Qt::white;
    Qt::Alignment m_textAlignment = Qt::AlignLeft;
    QImage m_image;
    QPixmap m_cachedScaledPixmap;
    QSize m_lastCachedSize;
    bool m_wasResizing = false;
    qreal m_angle = 0.0;
    qreal m_opacity = 0.4;
    int m_watermarkStyle = 0;
    QColor m_redactionColor = Qt::black;
    bool m_isTextModified = false;
    int m_minBoxHeight = 0;
    bool m_isPlacementMode = false;
    bool m_hasCollision = false;
    double m_scaleFactor = 1.0;

    // Shape properties
    int m_shapeType = 0;
    int m_shapeStrokeWidth = 2;
    QColor m_shapeStrokeColor = QColor(QStringLiteral("#ef4444"));
    QColor m_shapeFillColor = Qt::transparent;
    QPoint m_shapeLocalStart = QPoint(-1, -1);
    QPoint m_shapeLocalEnd = QPoint(-1, -1);
    QPoint m_dragStartShapeLocalStart = QPoint(-1, -1);
    QPoint m_dragStartShapeLocalEnd = QPoint(-1, -1);

    // Inline canvas text editor
    QTextEdit *m_inlineTextEdit = nullptr;

    // Handle geometries (8 control handles)
    static constexpr int HandleSize = 10;
    QRect m_handles[8];

    // Floating action pill toolbar
    QWidget *m_toolbar = nullptr;
    QPushButton *m_btnEdit = nullptr;
    QPushButton *m_btnRedactBlack = nullptr;
    QPushButton *m_btnRedactWhite = nullptr;
    QPushButton *m_btnRedactGray = nullptr;
    QPushButton *m_btnRedactCustom = nullptr;
    QPushButton *m_btnApply = nullptr;
    QPushButton *m_btnApplyAndContinue = nullptr;
    QPushButton *m_btnFitWidth = nullptr;
    QPushButton *m_btnFitPage = nullptr;
    QPushButton *m_btnCutArea = nullptr;
    QPushButton *m_btnCopyText = nullptr;
    QPushButton *m_btnCopyArea = nullptr;
    QPushButton *m_btnOcrArea = nullptr;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnFontStatus = nullptr;

    void updateFontStatusBadge();
    void onFontStatusClicked();
};
