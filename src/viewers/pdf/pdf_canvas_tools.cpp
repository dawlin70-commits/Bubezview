/**
 * @file pdf_canvas_tools.cpp
 * @brief Implementation of canvas visual rendering and tool helpers.
 */

#include "pdf_canvas_tools.h"
#include <poppler/qt6/poppler-qt6.h>
#include <poppler/qt6/poppler-form.h>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

QRect PdfCanvasTools::normalizeRect(const QPoint &p1, const QPoint &p2)
{
    return QRect(QPoint(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y())),
                 QPoint(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y())));
}

QImage PdfCanvasTools::renderShapeStamp(int shapeType,
                                        int strokeWidth,
                                        const QColor &strokeColor,
                                        const QColor &fillColor,
                                        const QSize &size,
                                        const QPoint &startPt,
                                        const QPoint &endPt)
{
    if (size.width() <= 0 || size.height() <= 0) {
        return QImage();
    }

    QImage shapeImg(size, QImage::Format_ARGB32_Premultiplied);
    shapeImg.fill(Qt::transparent);

    QPainter p(&shapeImg);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(strokeColor, strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    if (fillColor.isValid() && fillColor.alpha() > 0) {
        p.setBrush(fillColor);
    } else {
        p.setBrush(Qt::NoBrush);
    }

    int halfW = strokeWidth / 2;
    QRect localRect = shapeImg.rect().adjusted(halfW, halfW, -halfW, -halfW);

    if (shapeType == 0) { // Rectangle
        p.drawRect(localRect);
    } else if (shapeType == 1) { // Ellipse
        p.drawEllipse(localRect);
    } else if (shapeType == 2) { // Arrow
        QPoint p1 = (startPt != QPoint(-1, -1)) ? startPt : QPoint(halfW, halfW);
        QPoint p2 = (endPt != QPoint(-1, -1)) ? endPt : QPoint(localRect.right(), localRect.bottom());
        p.drawLine(p1, p2);

        double angle = std::atan2(static_cast<double>(p2.y() - p1.y()),
                                  static_cast<double>(p2.x() - p1.x()));
        double arrowSize = std::max(10.0, static_cast<double>(strokeWidth) * 3.0);
        QPointF head1 = QPointF(p2) - QPointF(arrowSize * std::cos(angle - M_PI / 6.0),
                                              arrowSize * std::sin(angle - M_PI / 6.0));
        QPointF head2 = QPointF(p2) - QPointF(arrowSize * std::cos(angle + M_PI / 6.0),
                                              arrowSize * std::sin(angle + M_PI / 6.0));
        QPolygonF arrowHead;
        arrowHead << QPointF(p2) << head1 << head2;
        p.setBrush(strokeColor);
        p.setPen(QPen(strokeColor, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolygon(arrowHead);
    } else if (shapeType == 3) { // Line
        QPoint p1 = (startPt != QPoint(-1, -1)) ? startPt : QPoint(halfW, halfW);
        QPoint p2 = (endPt != QPoint(-1, -1)) ? endPt : QPoint(localRect.right(), localRect.bottom());
        p.drawLine(p1, p2);
    }

    p.end();
    return shapeImg;
}

void PdfCanvasTools::drawRubberBand(QPainter &painter,
                                    const QRect &rect,
                                    int toolMode,
                                    int snapW300,
                                    int snapH300,
                                    int shapeType,
                                    int strokeWidth,
                                    const QColor &strokeColor,
                                    const QColor &fillColor,
                                    const QPoint &dragStart,
                                    const QPoint &dragCurrent)
{
    // 1 = Snapshot / SelectArea (rubber band with dimensions badge)
    if (toolMode == 1 || toolMode == 2) {
        painter.setPen(QPen(QColor(14, 165, 233), 2, Qt::DashLine));
        painter.setBrush(QColor(14, 165, 233, 40));
        painter.drawRect(rect);

        if (snapW300 > 0 && snapH300 > 0) {
            QString badge = QString("%1 × %2 px (300 DPI)").arg(snapW300).arg(snapH300);
            painter.setFont(QFont("sans-serif", 9, QFont::Bold));
            QFontMetrics fm = painter.fontMetrics();
            QRect badgeRect = fm.boundingRect(badge).adjusted(-6, -3, 6, 3);
            badgeRect.moveCenter(QPoint(rect.center().x(), std::max(badgeRect.height() / 2 + 4, rect.top() - 14)));

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(15, 23, 42, 220));
            painter.drawRoundedRect(badgeRect, 4, 4);

            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, badge);
        }
    } else if (toolMode == 3) { // Redaction
        painter.setPen(QPen(QColor(239, 68, 68), 2, Qt::DashLine));
        painter.setBrush(QColor(0, 0, 0, 200));
        painter.drawRect(rect);

        painter.setPen(Qt::white);
        painter.setFont(QFont("sans-serif", 9, QFont::Bold));
        painter.drawText(rect, Qt::AlignCenter, QObject::tr("REDACT"));
    } else if (toolMode == 4) { // Shape preview
        painter.setPen(QPen(strokeColor, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (fillColor.isValid() && fillColor.alpha() > 0) {
            painter.setBrush(fillColor);
        } else {
            painter.setBrush(Qt::NoBrush);
        }

        if (shapeType == 0) {
            painter.drawRect(rect);
        } else if (shapeType == 1) {
            painter.drawEllipse(rect);
        } else if (shapeType == 2) { // Arrow
            painter.drawLine(dragStart, dragCurrent);
            if ((dragCurrent - dragStart).manhattanLength() >= 4) {
                double angle = std::atan2(static_cast<double>(dragCurrent.y() - dragStart.y()),
                                          static_cast<double>(dragCurrent.x() - dragStart.x()));
                double arrowSize = std::max(10.0, static_cast<double>(strokeWidth) * 3.0);
                QPointF head1 = QPointF(dragCurrent) - QPointF(arrowSize * std::cos(angle - M_PI / 6.0),
                                                               arrowSize * std::sin(angle - M_PI / 6.0));
                QPointF head2 = QPointF(dragCurrent) - QPointF(arrowSize * std::cos(angle + M_PI / 6.0),
                                                               arrowSize * std::sin(angle + M_PI / 6.0));
                QPolygonF arrowHead;
                arrowHead << QPointF(dragCurrent) << head1 << head2;
                painter.setBrush(strokeColor);
                painter.setPen(QPen(strokeColor, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPolygon(arrowHead);
            }
        } else if (shapeType == 3) { // Line
            painter.drawLine(dragStart, dragCurrent);
        }
    } else if (toolMode == 5) { // Highlighter preview
        QColor hl = fillColor.isValid() ? fillColor : QColor(255, 235, 59, 100);
        painter.setPen(QPen(hl.darker(130), 1.5, Qt::DashLine));
        painter.setBrush(hl);
        painter.drawRect(rect);
    }
}

void PdfCanvasTools::drawTextSelection(QPainter &painter,
                                       const std::vector<std::unique_ptr<Poppler::TextBox>> &textBoxes,
                                       const std::vector<int> &selectedIndices,
                                       double scaleX,
                                       double scaleY)
{
    if (selectedIndices.empty()) return;

    painter.setBrush(QColor(0, 120, 215, 90));
    painter.setPen(Qt::NoPen);

    for (int idx : selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(textBoxes.size())) {
            const auto &box = textBoxes[idx];
            if (!box) continue;
            QRectF b = box->boundingBox();
            QRectF pixelRect(b.x() * scaleX,
                             b.y() * scaleY,
                             b.width() * scaleX,
                             b.height() * scaleY);
            painter.drawRect(pixelRect);
        }
    }
}

void PdfCanvasTools::drawAcroFormHighlights(QPainter &painter,
                                            const std::vector<std::unique_ptr<Poppler::FormField>> &formFields,
                                            double scaleX,
                                            double scaleY)
{
    if (formFields.empty()) return;

    for (const auto &field : formFields) {
        if (!field || !field->isVisible()) continue;
        QRectF f = field->rect();
        QRect pixelRect(qRound(f.x() * scaleX),
                         qRound(f.y() * scaleY),
                         qRound(f.width() * scaleX),
                         qRound(f.height() * scaleY));

        painter.setPen(QPen(QColor(59, 130, 246, 140), 1, Qt::DashLine));
        painter.setBrush(QColor(219, 234, 254, 40));
        painter.drawRect(pixelRect);
    }
}

int PdfCanvasTools::findClosestTextBox(const QPoint &pt,
                                       const std::vector<std::unique_ptr<Poppler::TextBox>> &textBoxes,
                                       double scaleX,
                                       double scaleY)
{
    if (textBoxes.empty()) return -1;

    for (int i = 0; i < static_cast<int>(textBoxes.size()); ++i) {
        const auto &box = textBoxes[i];
        if (!box) continue;
        QRectF b = box->boundingBox();
        QRectF pixelRect(b.x() * scaleX,
                         b.y() * scaleY,
                         b.width() * scaleX,
                         b.height() * scaleY);
        if (pixelRect.adjusted(-2, -2, 2, 2).contains(pt)) {
            return i;
        }
    }

    int closestOverall = -1;
    double minOverallDist = 1e9;
    for (int i = 0; i < static_cast<int>(textBoxes.size()); ++i) {
        const auto &box = textBoxes[i];
        if (!box) continue;
        QRectF b = box->boundingBox();
        QRectF pixelRect(b.x() * scaleX,
                         b.y() * scaleY,
                         b.width() * scaleX,
                         b.height() * scaleY);
        QPointF center = pixelRect.center();
        double dist = std::hypot(center.x() - pt.x(), center.y() - pt.y());
        if (dist < minOverallDist) {
            minOverallDist = dist;
            closestOverall = i;
        }
    }
    return closestOverall;
}
