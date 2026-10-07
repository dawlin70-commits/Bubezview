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

class QPushButton;
class QHBoxLayout;

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
     * @brief Activates the transform box for a text block.
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
     * @param text Stamp text (e.g. "COPIA", "BOZZA").
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

    QString text() const { return m_text; }
    QFont textFont() const { return m_font; }
    QColor textColor() const { return m_color; }
    bool isWhiteout() const { return m_whiteout; }
    QImage image() const { return m_image; }
    qreal watermarkAngle() const { return m_angle; }
    qreal watermarkOpacity() const { return m_opacity; }
    int watermarkStyle() const { return m_watermarkStyle; }
    QColor redactionColor() const { return m_redactionColor; }
    bool isTextModified() const { return m_isTextModified; }
    void setTextModified(bool modified) { m_isTextModified = modified; }

    QRect dragHandleRect() const;

signals:
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
     * @brief Emitted when the user requests copying the selected area.
     */
    void copyAreaRequested();

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
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateHandles();
    Handle handleAt(const QPoint &pos) const;
    void repositionToolbar();
    void updateCachedPixmap(bool fast = false);

    PdfTransformItemType m_itemType;
    QRect m_boxRect;
    Handle m_activeHandle;
    QPoint m_dragStartPos;
    QRect m_dragStartRect;

    // Item properties
    QString m_text;
    QFont m_font;
    QColor m_color;
    bool m_whiteout;
    QImage m_image;
    QPixmap m_cachedScaledPixmap;
    QSize m_lastCachedSize;
    bool m_wasResizing = false;
    qreal m_angle = 0.0;
    qreal m_opacity = 0.4;
    int m_watermarkStyle = 0;
    QColor m_redactionColor = Qt::black;
    bool m_isTextModified = false;

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
    QPushButton *m_btnCutArea = nullptr;
    QPushButton *m_btnCopyArea = nullptr;
    QPushButton *m_btnCancel = nullptr;
};
