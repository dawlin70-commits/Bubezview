/**
 * @file pdf_canvas_tools.cpp
 * @brief Implementation of canvas visual rendering and tool helpers.
 */

#include "pdf_canvas_tools.h"
#include <poppler/qt6/poppler-qt6.h>
#include <poppler/qt6/poppler-form.h>
#include <QFontMetrics>
#include <QMap>
#include <cmath>
#include <algorithm>

QRect PdfCanvasTools::normalizeRect(const QPoint &p1, const QPoint &p2)
{
    return QRect(QPoint(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y())),
                 QPoint(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y())));
}

double PdfCanvasTools::pointToLineSegmentDistance(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double vx = b.x() - a.x();
    double vy = b.y() - a.y();
    double lenSq = vx * vx + vy * vy;
    if (lenSq <= 1e-6) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    double wx = p.x() - a.x();
    double wy = p.y() - a.y();
    double t = std::clamp((wx * vx + wy * vy) / lenSq, 0.0, 1.0);
    double projX = a.x() + t * vx;
    double projY = a.y() + t * vy;
    return std::hypot(p.x() - projX, p.y() - projY);
}

bool PdfCanvasTools::hitTestShape(int shapeType,
                                  const QRect &shapeRect,
                                  const QPoint &localStart,
                                  const QPoint &localEnd,
                                  int strokeWidth,
                                  const QColor &fillColor,
                                  const QPoint &clickPoint,
                                  int tolerance)
{
    if (shapeRect.isNull() || !shapeRect.isValid()) return false;

    int effTol = std::max(tolerance, (strokeWidth + 1) / 2 + 4);

    // 1. Arrow (2) or Line (3)
    if (shapeType == 2 || shapeType == 3) {
        QPoint p1 = (localStart != QPoint(-1, -1)) ? (shapeRect.topLeft() + localStart) : shapeRect.topLeft();
        QPoint p2 = (localEnd != QPoint(-1, -1)) ? (shapeRect.topLeft() + localEnd) : shapeRect.bottomRight();

        double dist = pointToLineSegmentDistance(QPointF(clickPoint), QPointF(p1), QPointF(p2));
        if (dist <= effTol) return true;

        if (shapeType == 2) { // Arrowhead check
            double arrowSize = std::max(12.0, static_cast<double>(strokeWidth) * 3.5);
            if (std::hypot(clickPoint.x() - p2.x(), clickPoint.y() - p2.y()) <= arrowSize + 4.0) {
                return true;
            }
        }
        return false;
    }

    // 2. Rectangle (0) or Rounded Rectangle (4)
    if (shapeType == 0 || shapeType == 4) {
        bool isFilled = fillColor.isValid() && fillColor.alpha() > 20;
        if (isFilled) {
            return shapeRect.adjusted(-effTol, -effTol, effTol, effTol).contains(clickPoint);
        }
        // Hollow: test proximity to the 4 edges
        QRect inner = shapeRect.adjusted(effTol, effTol, -effTol, -effTol);
        QRect outer = shapeRect.adjusted(-effTol, -effTol, effTol, effTol);
        return outer.contains(clickPoint) && (!inner.isValid() || !inner.contains(clickPoint));
    }

    // 3. Ellipse / Circle (1)
    if (shapeType == 1) {
        double rx = shapeRect.width() / 2.0;
        double ry = shapeRect.height() / 2.0;
        if (rx <= 1e-4 || ry <= 1e-4) return false;
        double cx = shapeRect.center().x();
        double cy = shapeRect.center().y();
        double dx = (clickPoint.x() - cx) / rx;
        double dy = (clickPoint.y() - cy) / ry;
        double dNormSq = dx * dx + dy * dy;

        bool isFilled = fillColor.isValid() && fillColor.alpha() > 20;
        if (isFilled) {
            return dNormSq <= 1.0 + (effTol / std::min(rx, ry));
        }
        // Hollow: distance to ellipse perimeter
        double dNorm = std::sqrt(dNormSq);
        double distPx = std::abs(dNorm - 1.0) * std::min(rx, ry);
        return distPx <= effTol;
    }

    return shapeRect.contains(clickPoint);
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
    } else if (shapeType == 1) { // Circle / Ellipse
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
    } else if (shapeType == 3) { // Straight Line
        QPoint p1 = (startPt != QPoint(-1, -1)) ? startPt : QPoint(halfW, halfW);
        QPoint p2 = (endPt != QPoint(-1, -1)) ? endPt : QPoint(localRect.right(), localRect.bottom());
        p.drawLine(p1, p2);
    } else if (shapeType == 4) { // Rounded Rectangle
        p.drawRoundedRect(localRect, 8, 8);
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

        if (shapeType == 0) { // Rectangle
            painter.drawRect(rect);
        } else if (shapeType == 1) { // Circle / Ellipse
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
        } else if (shapeType == 3) { // Straight Line
            painter.drawLine(dragStart, dragCurrent);
        } else if (shapeType == 4) { // Rounded Rectangle
            painter.drawRoundedRect(rect, 8, 8);
        }
    } else if (toolMode == 5) { // Highlighter preview
        QColor hl = fillColor.isValid() ? fillColor : QColor(255, 235, 59, 100);
        painter.setPen(QPen(hl.darker(130), 1.5, Qt::DashLine));
        painter.setBrush(hl);
        painter.drawRect(rect);
    } else if (toolMode == 6) { // Spotlight preview
        painter.setPen(QPen(QColor(255, 235, 59), 2, Qt::SolidLine));
        painter.setBrush(QColor(255, 255, 255, 30));
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

QRect PdfCanvasTools::calculateStampRect(const QString &text, qreal angle, int fontSize,
                                         const QPoint &centerPos, int pageW, int pageH)
{
    QFont baseFont(QStringLiteral("Sans Serif"), 36, QFont::Bold);
    QFontMetrics fm(baseFont);
    QRect textBounds = fm.boundingRect(text.isEmpty() ? QStringLiteral("APPROVED") : text);
    qreal rawW = std::max(120.0, static_cast<double>(textBounds.width()) + 40.0);
    qreal rawH = std::max(50.0, static_cast<double>(textBounds.height()) + 24.0);

    qreal fontSizeScale = static_cast<double>(fontSize > 0 ? fontSize : 28) / 36.0;
    qreal scaledW = rawW * fontSizeScale;
    qreal scaledH = rawH * fontSizeScale;

    qreal rad = qDegreesToRadians(std::abs(angle));
    qreal cosVal = std::abs(std::cos(rad));
    qreal sinVal = std::abs(std::sin(rad));
    int stampW = std::max(80, qRound(scaledW * cosVal + scaledH * sinVal) + 16);
    int stampH = std::max(36, qRound(scaledW * sinVal + scaledH * cosVal) + 16);

    QRect rect(centerPos.x() - stampW / 2, centerPos.y() - stampH / 2, stampW, stampH);
    if (pageW > 0 && pageH > 0) {
        if (rect.left() < 4) rect.moveLeft(4);
        if (rect.top() < 4) rect.moveTop(4);
        if (rect.right() > pageW - 4) rect.moveRight(pageW - 4);
        if (rect.bottom() > pageH - 4) rect.moveBottom(pageH - 4);
    }
    return rect;
}

QColor PdfCanvasTools::sampleBackgroundColor(const QImage &image, const QRect &rect, int marginDistance)
{
    if (image.isNull() || !rect.isValid() || rect.isEmpty()) {
        return Qt::white;
    }

    int imgW = image.width();
    int imgH = image.height();
    if (imgW <= 0 || imgH <= 0) return Qt::white;

    // Build perimeter sampling ring just outside the rect
    int pad = std::max(1, marginDistance);
    int left = std::clamp(rect.left() - pad, 0, imgW - 1);
    int right = std::clamp(rect.right() + pad, 0, imgW - 1);
    int top = std::clamp(rect.top() - pad, 0, imgH - 1);
    int bottom = std::clamp(rect.bottom() + pad, 0, imgH - 1);

    // Quantize colors into 12-bit buckets (4 bits per channel) to group anti-aliased or compressed noise
    // and collect pixel colors in each bucket to compute precise average
    struct ColorBucket {
        quint64 rSum = 0;
        quint64 gSum = 0;
        quint64 bSum = 0;
        int count = 0;
    };
    QMap<quint32, ColorBucket> buckets;

    auto samplePoint = [&](int x, int y) {
        if (x < 0 || x >= imgW || y < 0 || y >= imgH) return;
        QRgb rgb = image.pixel(x, y);
        int r = qRed(rgb);
        int g = qGreen(rgb);
        int b = qBlue(rgb);

        // Quantized 4-bit per channel key: [R:4][G:4][B:4]
        quint32 key = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
        ColorBucket &bucket = buckets[key];
        bucket.rSum += r;
        bucket.gSum += g;
        bucket.bSum += b;
        bucket.count++;
    };

    // Sample horizontal outer borders (top and bottom)
    for (int x = left; x <= right; ++x) {
        samplePoint(x, top);
        samplePoint(x, bottom);
        if (pad > 1) {
            samplePoint(x, std::clamp(top - 1, 0, imgH - 1));
            samplePoint(x, std::clamp(bottom + 1, 0, imgH - 1));
        }
    }

    // Sample vertical outer borders (left and right)
    for (int y = top; y <= bottom; ++y) {
        samplePoint(left, y);
        samplePoint(right, y);
        if (pad > 1) {
            samplePoint(std::clamp(left - 1, 0, imgW - 1), y);
            samplePoint(std::clamp(right + 1, 0, imgW - 1), y);
        }
    }

    if (buckets.isEmpty()) {
        return Qt::white;
    }

    // Find bucket with highest pixel count (mode / dominant color)
    int maxCount = -1;
    quint32 bestKey = 0;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        if (it.value().count > maxCount) {
            maxCount = it.value().count;
            bestKey = it.key();
        }
    }

    const ColorBucket &best = buckets[bestKey];
    if (best.count <= 0) return Qt::white;

    int finalR = static_cast<int>(best.rSum / best.count);
    int finalG = static_cast<int>(best.gSum / best.count);
    int finalB = static_cast<int>(best.bSum / best.count);

    return QColor(finalR, finalG, finalB);
}

QImage PdfCanvasTools::createFeatheredPatch(const QSize &size, const QColor &color, int featherRadius)
{
    if (size.width() <= 0 || size.height() <= 0) {
        return QImage();
    }

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    int w = size.width();
    int h = size.height();
    int feather = std::clamp(featherRadius > 0 ? featherRadius : 6, 2, std::max(2, std::min(w, h) / 3));

    for (int y = 0; y < h; ++y) {
        qreal alphaY = 1.0;
        if (y < feather) {
            alphaY = static_cast<qreal>(y) / feather;
        } else if (y >= h - feather) {
            alphaY = static_cast<qreal>(h - 1 - y) / feather;
        }

        QRgb *scanLine = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            qreal alphaX = 1.0;
            if (x < feather) {
                alphaX = static_cast<qreal>(x) / feather;
            } else if (x >= w - feather) {
                alphaX = static_cast<qreal>(w - 1 - x) / feather;
            }

            qreal totalAlpha = alphaX * alphaY;
            QColor c = color;
            c.setAlphaF(totalAlpha * (color.alphaF() > 0.0 ? color.alphaF() : 1.0));
            scanLine[x] = c.rgba();
        }
    }

    return img;
}
