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
 * @brief Helper class encapsulating visual rendering and coordinate math for canvas tools.
 */
class PdfCanvasTools {
public:
    /**
     * @brief Normalizes two arbitrary points into a top-left/bottom-right bounding QRect.
     */
    static QRect normalizeRect(const QPoint &p1, const QPoint &p2);

    /**
     * @brief Renders vector shapes (Rectangle, Ellipse, Arrow, Line) into an anti-aliased stamp image.
     * @param shapeType 0 = Rectangle, 1 = Ellipse, 2 = Arrow, 3 = Line.
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
};
