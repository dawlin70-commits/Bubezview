/**
 * @file pdf_page_widget.h
 * @brief Interactive PDF page canvas supporting rendering, text selection,
 * annotations, 300 DPI snapshots, redactions, and AcroForms form filling.
 */

#pragma once

#include <QWidget>
#include <QPixmap>
#include <QRectF>
#include <QImage>
#include <QFont>
#include <QColor>
#include <QMap>
#include <memory>
#include <vector>
#include "pdf_transform_box.h"

class QLineEdit;

namespace Poppler {
    class Document;
    class TextBox;
    class FormField;
}

/**
 * @brief Active interaction mode on the PDF canvas.
 */
enum class PdfToolMode {
    SelectText,      ///< Standard cursor, word drag/double-click selection
    SelectArea,      ///< Rectangular area selection for copying, cutting, or moving graphics and text
    Zoom,            ///< Zoom tool mode, directional mouse wheel and click zooming
    Typewriter,      ///< Interactive text insertion / whiteout replacement
    Signature,       ///< Signature placement and transformation
    InsertImage,     ///< Inserted picture / graphic placement and transformation
    Watermark,       ///< Watermark / stamp overlay placement
    Shape,           ///< Geometric shape / arrow drawing
    Snapshot,        ///< 300 DPI high-resolution area capture to clipboard
    Redaction,       ///< Privacy blackout box placement
    Highlight        ///< Semi-transparent color highlight markup
};

/**
 * @brief Interactive page widget that renders a rasterized PDF page and handles annotations,
 * text selection, snapshot clipping, and AcroForms.
 */
class PdfPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PdfPageWidget(QWidget *parent = nullptr);
    ~PdfPageWidget() override;

    struct PageLayout {
        int pageIndex = 0;
        QRect rect;
        double scaleX = 1.0;
        double scaleY = 1.0;
    };

    /**
     * @brief Configures continuous multi-page layout for the entire PDF document.
     */
    void setDocument(Poppler::Document *doc, double zoomFactor, bool fitToWidth, int rotationDegrees, int availWidth);

    /**
     * @brief Sets the active page index (0-based) and loads its text/form interaction data.
     */
    void setActivePage(int pageIndex);

    /**
     * @brief Returns current active page index.
     */
    int activePage() const { return m_activePageIndex; }

    /**
     * @brief Returns the 0-based page index intersecting vertical canvas coordinate y.
     */
    int pageIndexAtY(int y) const;

    /**
     * @brief Returns the 0-based page index containing point pos in widget coordinates.
     */
    int pageIndexAtPos(const QPoint &pos) const;

    /**
     * @brief Sets the path to the PDF document on disk for precise layout/font analysis.
     */
    void setFilePath(const QString &filePath) { m_filePath = filePath; }

    /**
     * @brief Returns current PDF file path on disk.
     */
    QString filePath() const { return m_filePath; }

    /**
     * @brief Returns the top Y position of the given page in canvas coordinates.
     */
    int pageTopY(int pageIndex) const;

    /**
     * @brief Returns the bounding rectangle of the given page in canvas coordinates.
     */
    QRect pageRect(int pageIndex) const;

    /**
     * @brief Clears page cache and forces a full re-render.
     */
    void invalidatePages();

    /**
     * @brief Sets the rendered page image, word bounding boxes, and AcroForms fields (single page mode).
     * @param pixmap Rendered raster pixmap.
     * @param textBoxes Vector of word bounding boxes in point coordinates (72 dpi).
     * @param formFields Vector of interactive AcroForms on this page.
     * @param scaleX Horizontal scaling factor from point coordinates to pixels.
     * @param scaleY Vertical scaling factor from point coordinates to pixels.
     * @param transitionDirection Direction for progressive page scroll (+1 next, -1 prev, 0 instant).
     */
    void setPageContent(const QPixmap &pixmap,
                        std::vector<std::unique_ptr<Poppler::TextBox>> &&textBoxes,
                        std::vector<std::unique_ptr<Poppler::FormField>> &&formFields,
                        double scaleX, double scaleY,
                        int transitionDirection = 0);

    /**
     * @brief Clears current pixmap, text boxes, and selection.
     */
    void clear();

    /**
     * @brief Sets the active interaction mode.
     * @param mode Target tool mode.
     */
    void setToolMode(PdfToolMode mode);

    /**
     * @brief Returns the active interaction mode.
     */
    PdfToolMode toolMode() const { return m_toolMode; }

    /**
     * @brief Activates the Typewriter tool with preconfigured text parameters.
     * @param text Text string to insert.
     * @param font Font family, style, and point size.
     * @param color Text color.
     * @param whiteout True to cover underlying content with an opaque white rectangle.
     * @param initialPos Center point in widget coordinates.
     */
    void startTypewriter(const QString &text, const QFont &font, const QColor &color,
                        bool whiteout, const QPoint &initialPos = QPoint());

    /**
     * @brief Activates the Signature tool with a transparent signature image.
     * @param image High-resolution signature image.
     * @param initialPos Center point in widget coordinates.
     */
    void startSignature(const QImage &image, const QPoint &initialPos = QPoint());

    /**
     * @brief Activates the Image insertion tool with a picture.
     * @param image High-resolution image (PNG, JPG, WebP, SVG, TIFF).
     * @param initialPos Center point in widget coordinates.
     */
    void startInsertImage(const QImage &image, const QPoint &initialPos = QPoint());

    /**
     * @brief Activates the rectangular Area Selection mode.
     */
    void startSelectArea();

    /**
     * @brief Cuts the active area selection: copies 300 DPI rendering to clipboard and applies whiteout.
     */
    void cutSelectedArea();

    /**
     * @brief Copies the active area selection to clipboard at 300 DPI without removing it.
     */
    void copySelectedArea();

    /**
     * @brief Checks if there is an active rectangular area selection.
     * @return True if an area is currently selected.
     */
    bool hasAreaSelection() const;

    /**
     * @brief Clears any active rectangular area selection overlay.
     */
    void clearAreaSelection();

    /**
     * @brief Cuts the selected text: copies text to clipboard and applies a clean whiteout annotation.
     */
    void cutSelection();

    /**
     * @brief Pastes content from clipboard (either image or text) at the given widget position.
     * @param targetPos Optional center point in widget coordinates.
     */
    void pasteFromClipboard(const QPoint &targetPos = QPoint());

    /**
     * @brief Pastes text or image from the system clipboard.
     * @param targetPos Optional center point in widget coordinates.
     */
    void pasteTextFromClipboard(const QPoint &targetPos = QPoint()) { pasteFromClipboard(targetPos); }

    /**
     * @brief Activates the Watermark tool with the specified watermark text.
     * @param text Watermark string (e.g. "COPY", "DRAFT").
     * @param angle Rotation angle in degrees.
     * @param opacity Translucency (0.1 to 1.0).
     * @param color Watermark color.
     * @param initialPos Center point in widget coordinates.
     */
    void startWatermark(const QString &text = QStringLiteral("COPY"), qreal angle = 45.0, qreal opacity = 0.35,
                       const QColor &color = QColor(QStringLiteral("#DC2626")), const QPoint &initialPos = QPoint(), int style = 0);

    /**
     * @brief Activates the Shape tool with drawing options.
     * @param shapeType 0: Rectangle, 1: Ellipse, 2: Arrow, 3: Line.
     * @param strokeWidth Pen width in pixels.
     * @param strokeColor Outline color.
     * @param fillColor Interior color (transparent if invalid).
     */
    void startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);

    /**
     * @brief Activates the 300 DPI high-resolution snapshot clipping tool.
     */
    void startSnapshot();

    /**
     * @brief Activates the Redaction blackout box tool.
     */
    void startRedaction();

    /**
     * @brief Activates the semi-transparent highlighter markup tool.
     * @param color Base color for the highlighter.
     * @param opacity Opacity factor from 0.05 to 1.0.
     */
    void startHighlighter(const QColor &color = QColor(255, 235, 59), double opacity = 0.40);

    /**
     * @brief Highlights the currently selected text using the specified color and opacity.
     * @param color Base color for the highlight.
     * @param opacity Opacity factor.
     */
    void highlightSelectedText(const QColor &color = QColor(255, 235, 59), double opacity = 0.40);

    /**
     * @brief Configures whether the active tool remains active for consecutive strokes.
     */
    void setContinuousMode(bool continuous) { m_continuousMode = continuous; }
    bool isContinuousMode() const { return m_continuousMode; }

    /**
     * @brief Dismisses any active transform box and resets tool mode to SelectText.
     */
    void cancelActiveTool();

    /**
     * @brief Returns pointer to the overlay transform box widget.
     * @return PdfTransformBox pointer.
     */
    PdfTransformBox *transformBox() const { return m_transformBox; }

    /**
     * @brief Computes the center of the currently visible viewport mapped into page coordinates.
     * Ensures newly spawned transform boxes (typewriter, signature, watermark) are directly visible.
     */
    QPoint visibleCenter() const;

    /**
     * @brief Checks if there is any active text selection.
     * @return True if one or more words are selected.
     */
    bool hasSelection() const;

    /**
     * @brief Returns the selected text joined with spaces.
     * @return Formatted selected text string.
     */
    QString selectedText() const;

    /**
     * @brief Refreshes widget styling and borders based on the active ThemePreset.
     */
    void updateTheme();

    /**
     * @brief Copies the current selected text to the system clipboard.
     */
    void copySelectionToClipboard();

    /**
     * @brief Copies all text on the current page to the system clipboard.
     */
    void copyPageTextToClipboard();

    /**
     * @brief Selects all words on the current page.
     */
    void selectAll();

signals:
    /**
     * @brief Emitted when text selection changes or is cleared.
     * @param hasSelection True if at least one character/word is highlighted.
     */
    void selectionChanged(bool hasSelection);

    /**
     * @brief Emitted when the user requests copying the entire document text.
     */
    void copyDocumentRequested();

    /**
     * @brief Emitted when an annotation item is confirmed via the transform box or shape drawer.
     * @param image Rendered raster image of the annotation (stamp).
     * @param normalizedRect Normalized page boundary (0.0 to 1.0 coordinates).
     */
    void addStampAnnotationRequested(const QImage &image, const QRectF &normalizedRect);

    /**
     * @brief Emitted when multiple stamp annotations are added in a single batch (e.g. text cut).
     * @param stamps List of image and normalized rect pairs.
     */
    void addStampAnnotationsRequested(const QList<QPair<QImage, QRectF>> &stamps);

    /**
     * @brief Emitted when a native vector text annotation is added to the page.
     * @param text String content.
     * @param font Font family, style, and point size.
     * @param color Text color.
     * @param normalizedRect Normalized page boundary (0.0 to 1.0 coordinates).
     * @param whiteout True if an opaque white background should be placed under the text.
     */
    void addTextAnnotationRequested(const QString &text, const QFont &font, const QColor &color,
                                    const QRectF &normalizedRect, bool whiteout);    /**
     * @brief Emitted when the user requests image insertion from the context menu.
     */
    void insertImageRequested();

    /**
     * @brief Emitted when a snapshot area has been selected by the user.
     * @param pointRect Selected bounding box in 72 DPI page point coordinates.
     */
    void snapshotAreaSelected(const QRectF &pointRect);

    /**
     * @brief Emitted when the user requests cutting an area selection.
     * @param pointRect Bounding rectangle in 72 DPI page point coordinates.
     */
    void cutAreaRequested(const QRectF &pointRect);

    /**
     * @brief Emitted when the user requests copying an area selection.
     * @param pointRect Bounding rectangle in 72 DPI page point coordinates.
     */
    void copyAreaRequested(const QRectF &pointRect);

    /**
     * @brief Emitted when a directional zoom event occurs at the given widget coordinate.
     * @param pagePos Point in widget coordinates where the zoom is centered.
     * @param factor Zoom factor multiplier (e.g. 1.25 for in, 0.8 for out).
     */
    void zoomWheelRequested(const QPoint &pagePos, double factor);

    /**
     * @brief Emitted when an AcroForms form field value has been modified by the user.
     */
    void formFieldModified();

    /**
     * @brief Emitted when the canvas tool mode changes.
     */
    void toolModeChanged(PdfToolMode mode);

    /**
     * @brief Emitted when the user exits continuous mode (e.g. via Escape key).
     */
    void continuousModeExited();

    /**
     * @brief Emitted when an undo command is requested (Ctrl+Z).
     */
    void undoRequested();

    /**
     * @brief Emitted when a redo command is requested (Ctrl+Y / Ctrl+Shift+Z).
     */
    void redoRequested();

    /**
     * @brief Emitted when the active page in view changes.
     */
    void activePageChanged(int pageIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onTransformBoxApplied();
    void onTransformBoxApplyAndContinue();
    void onTransformBoxCancelled();
    void onTransformBoxEditRequested();

private:
    void updateSelection(const QPoint &startPt, const QPoint &endPt);
    int findClosestBox(const QPoint &pt) const;
    void commitShapeDrawing(const QRect &rect, const QPoint &startPt = QPoint(-1, -1), const QPoint &endPt = QPoint(-1, -1));
    QRect normalizeRect(const QPoint &p1, const QPoint &p2) const;
    void cancelTransitionAnimation();

    QPixmap m_pixmap;
    std::vector<std::unique_ptr<Poppler::TextBox>> m_textBoxes;
    std::vector<std::unique_ptr<Poppler::FormField>> m_formFields;
    double m_scaleX;
    double m_scaleY;

    // Smooth progressive page scrolling transition
    QPixmap m_previousPixmap;
    double m_transitionProgress = 1.0;
    int m_transitionDirection = 0;
    class QVariantAnimation *m_transitionAnim = nullptr;

    PdfToolMode m_toolMode;
    PdfTransformBox *m_transformBox;

    // Text selection state
    bool m_isSelecting;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    std::vector<int> m_selectedIndices;

    // Middle-click panning
    bool m_isPanning;
    QPoint m_panStartPos;

    // Snapshot & shape drag state
    bool m_isDraggingTool;
    QPoint m_dragStart;
    QPoint m_dragCurrent;

    // Area selection state
    bool m_hasAreaSelection;
    QRect m_selectedAreaRect;

    // Shape drawing parameters
    int m_shapeType;
    int m_shapeStrokeWidth;
    QColor m_shapeStrokeColor;
    QColor m_shapeFillColor;

    // Highlighter parameters
    QColor m_highlighterColor;
    double m_highlighterOpacity;
    void commitHighlightDrawing(const QRect &rect);

    // Continuous tool placement mode
    bool m_continuousMode;

    // Inline AcroForms text editor
    QLineEdit *m_inlineEditor;

    // Captured clipboard tracking
    enum class LastClipType { None, Text, AreaImage };
    LastClipType m_lastClipType = LastClipType::None;

    struct CopiedTextFormat {
        bool valid = false;
        QFont font = QFont(QStringLiteral("Sans Serif"), 12);
        QColor color = Qt::black;
        bool hasMultipleStyles = false;
        QString sourceDocumentPath;
        bool isTextModified = false;
    };
    CopiedTextFormat m_lastCopiedTextFormat;
    void captureSelectedTextFormat();

    // XML-based precise layout & font detection
    struct PageFontSpec {
        QString family;
        double size = 12.0;
        QColor color = Qt::black;
        bool isBold = false;
        bool isItalic = false;
    };

    struct PageTextElement {
        QRectF rectPt;
        int fontId = -1;
        QString text;
        QString family;
        double size = 12.0;
        QColor color = Qt::black;
        bool isBold = false;
        bool isItalic = false;
    };

    struct CachedPageLayout {
        int pageIndex = -1;
        QString filePath;
        QMap<int, PageFontSpec> fonts;
        QList<PageTextElement> elements;
    };

    QString m_filePath;
    CachedPageLayout m_cachedPageLayout;
    bool parsePageXmlLayout(int pageIndex, double pageWidthPt, double pageHeightPt);

    // Multi-page continuous layout state
    void refreshActivePageData();
    Poppler::Document *m_doc = nullptr;
    QVector<PageLayout> m_pageLayouts;
    int m_activePageIndex = 0;
    double m_zoomFactor = 1.0;
    bool m_fitToWidth = true;
    int m_rotationDegrees = 0;
    int m_availWidth = 800;
    int m_maxCachePages = 8;
    mutable QMap<int, QPixmap> m_pageCache;
};
