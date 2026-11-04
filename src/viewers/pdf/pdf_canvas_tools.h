/**
 * @file pdf_canvas_tools.h
 * @brief Interactive canvas rendering and tool helpers for PDF text selection,
 * shape rendering, rubber-band previews, and AcroForms highlights.
 */

#pragma once

#include <QRect>
#include <QPoint>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <memory>
#include <vector>

namespace Poppler {
    class TextBox;
    class FormField;
}

/**
 * @brief Canonical geometric shape types supported in PDF canvas and markup tools.
 */
enum class PdfShapeType {
    Rectangle = 0,
    Ellipse = 1,
    Arrow = 2,
    Line = 3,
    RoundedRectangle = 4
};

/**
 * @brief Helper class encapsulating visual rendering and coordinate math for canvas tools.
 */
class PdfCanvasTools {
public:
    /**
     * @brief Normalizes two arbitrary points into a top-left/bottom-right bounding QRect.
     */
    static QRect normalizeRect(const QPoint &p1, const QPoint &p2);

    /**
     * @brief Computes the shortest distance from point p to the line segment (a, b).
     */
    static double pointToLineSegmentDistance(const QPointF &p, const QPointF &a, const QPointF &b);

    /**
     * @brief Performs accurate vector hit-testing for geometric shapes.
     * @param shapeType 0: Rectangle, 1: Ellipse, 2: Arrow, 3: Line, 4: Rounded Rect.
     * @param shapeRect Bounding rect in canvas pixels.
     * @param localStart Local start point for directional shapes (or QPoint(-1, -1)).
     * @param localEnd Local end point for directional shapes (or QPoint(-1, -1)).
     * @param strokeWidth Line width in pixels.
     * @param fillColor Fill color (determines whether interior is clickable).
     * @param clickPoint Click point in canvas pixels.
     * @param tolerance Hit-test threshold in pixels.
     * @return True if clickPoint hits the shape's stroke or filled interior.
     */
    static bool hitTestShape(int shapeType,
                             const QRect &shapeRect,
                             const QPoint &localStart,
                             const QPoint &localEnd,
                             int strokeWidth,
                             const QColor &fillColor,
                             const QPoint &clickPoint,
                             int tolerance = 6);

    /**
     * @brief Renders vector shapes (Rectangle, Ellipse, Arrow, Line) into an anti-aliased stamp image.
     * @param shapeType 0 = Rectangle, 1 = Ellipse, 2 = Arrow, 3 = Line, 4 = Rounded Rect.
     * @param strokeWidth Stroke line width in pixels.
     * @param strokeColor Stroke color.
     * @param fillColor Fill brush color.
     * @param size Target stamp dimensions.
     * @param startPt Optional start point in stamp coordinates (for Line/Arrow direction).
     * @param endPt Optional end point in stamp coordinates (for Line/Arrow direction).
     * @return Transparent ARGB32 premultiplied image containing the rendered shape.
     */
    static QImage renderShapeStamp(int shapeType,
                                   int strokeWidth,
                                   const QColor &strokeColor,
                                   const QColor &fillColor,
                                   const QSize &size,
                                   const QPoint &startPt = QPoint(-1, -1),
                                   const QPoint &endPt = QPoint(-1, -1));

    /**
     * @brief Renders the rubber-band overlay during an active drag interaction
     * (Snapshot rectangle with DPI badge, Redaction overlay, or geometric Shape).
     */
    static void drawRubberBand(QPainter &painter,
                               const QRect &rect,
                               int toolMode,
                               int snapW300,
                               int snapH300,
                               int shapeType,
                               int strokeWidth,
                               const QColor &strokeColor,
                               const QColor &fillColor,
                               const QPoint &dragStart,
                               const QPoint &dragCurrent);

    /**
     * @brief Renders semi-transparent highlights over currently selected text words.
     */
    static void drawTextSelection(QPainter &painter,
                                  const std::vector<std::unique_ptr<Poppler::TextBox>> &textBoxes,
                                  const std::vector<int> &selectedIndices,
                                  double scaleX,
                                  double scaleY);

    /**
     * @brief Renders interactive visual dashed boundaries over interactive AcroForms fields.
     */
    static void drawAcroFormHighlights(QPainter &painter,
                                       const std::vector<std::unique_ptr<Poppler::FormField>> &formFields,
                                       double scaleX,
                                       double scaleY);

    /**
     * @brief Finds the text box closest to the specified canvas coordinate.
     */
    static int findClosestTextBox(const QPoint &pt,
                                 const std::vector<std::unique_ptr<Poppler::TextBox>> &textBoxes,
                                 double scaleX,
                                 double scaleY);

    /**
     * @brief Calculates bounding box for a rubber stamp given its text, angle, and font size.
     * @param text Stamp text.
     * @param angle Rotation angle in degrees.
     * @param fontSize Font size in points.
     * @param centerPos Center point on the page.
     * @param pageW Page width.
     * @param pageH Page height.
     * @return Bounding rectangle clamped to page bounds.
     */
    static QRect calculateStampRect(const QString &text, qreal angle, int fontSize,
                                    const QPoint &centerPos, int pageW, int pageH);

    /**
     * @brief Samples the dominant background color around the perimeter of a given rectangle in a page image.
     * @param image Rendered raster page image.
     * @param rect Target area (e.g. text bounding box) in pixel coordinates.
     * @param marginDistance Number of pixels outside rect to sample (default: 3).
     * @return Sampled dominant background color (default Qt::white if invalid).
     */
    static QColor sampleBackgroundColor(const QImage &image, const QRect &rect, int marginDistance = 3);

    /**
     * @brief Generates an alpha-feathered background patch image with softly blurred edges.
     * @param size Target patch dimensions.
     * @param color Background fill color.
     * @param featherRadius Feathering margin width in pixels (default: 6).
     * @return Alpha-feathered QImage patch.
     */
    static QImage createFeatheredPatch(const QSize &size, const QColor &color, int featherRadius = 6);
};
