#include "vinyl_sleeve_widget.h"
#include <QLinearGradient>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

void VinylSleeveWidget::drawSoftDropShadow(QPainter &p, const QRectF &rect, double radius, int blurSteps, const QColor &baseColor)
{
    p.save();
    p.setBrush(Qt::NoBrush);
    for (int i = 1; i <= blurSteps; ++i) {
        double expand = i * 1.4;
        int alpha = static_cast<int>(baseColor.alpha() * (1.0 - static_cast<double>(i) / (blurSteps + 1)) * 0.35);
        QColor c = baseColor;
        c.setAlpha(alpha);
        p.setPen(QPen(c, 2.2));
        p.drawRoundedRect(rect.adjusted(-expand, -expand + 4, expand, expand + 4), radius + expand, radius + expand);
    }
    p.restore();
}

QPixmap VinylSleeveWidget::makeRoundedPixmap(const QPixmap &src, int w, int h, int radius)
{
    QPixmap dst(w, h);
    dst.fill(Qt::transparent);
    QPainter p(&dst);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0, 0, w, h, radius, radius);
    p.setClipPath(path);
    QPixmap scaled = src.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int ox = (w - scaled.width()) / 2;
    int oy = (h - scaled.height()) / 2;
    p.drawPixmap(ox, oy, scaled);
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 35), 1.0));
    p.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), radius, radius);
    return dst;
}

VinylSleeveWidget::VinylSleeveWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumSize(200, 130);
    setMaximumSize(400, 280);
    setAttribute(Qt::WA_TranslucentBackground);
}

void VinylSleeveWidget::setArtwork(const QPixmap &sleeveCover, const QPixmap &vinylLabel)
{
    m_sleeveCover = sleeveCover;
    m_vinylLabel = vinylLabel.isNull() ? sleeveCover : vinylLabel;
    update();
}

void VinylSleeveWidget::setCompactMode(bool compact)
{
    if (m_compactMode == compact) return;
    m_compactMode = compact;
    updateGeometry();
    update();
}

void VinylSleeveWidget::clearArtwork()
{
    m_sleeveCover = QPixmap();
    m_vinylLabel = QPixmap();
    update();
}

void VinylSleeveWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_compactMode) {
        // Centered square album cover without peeking vinyl disc
        const int availH = height() - 8;
        const int availW = width() - 16;
        const int covSize = std::clamp(std::min(availH, availW), 60, 140);
        const int covX = (width() - covSize) / 2;
        const int covY = (height() - covSize) / 2;
        const QRectF sleeveRect(covX, covY, covSize, covSize);

        drawSoftDropShadow(p, sleeveRect, 10.0, 6, QColor(0, 0, 0, 190));

        if (!m_sleeveCover.isNull()) {
            p.drawPixmap(covX, covY, makeRoundedPixmap(m_sleeveCover, covSize, covSize, 8));
        } else {
            QLinearGradient sleeveGrad(sleeveRect.topLeft(), sleeveRect.bottomRight());
            sleeveGrad.setColorAt(0.0, QColor("#1c222d"));
            sleeveGrad.setColorAt(1.0, QColor("#12161f"));
            p.setBrush(sleeveGrad);
            p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
            p.drawRoundedRect(sleeveRect, 8.0, 8.0);

            QFont noteFont = font();
            noteFont.setPixelSize(std::max(20, static_cast<int>(covSize * 0.28)));
            p.setFont(noteFont);
            p.setPen(QColor("#38bdf8"));
            p.drawText(sleeveRect, Qt::AlignCenter, QStringLiteral("🎵"));
        }
        return;
    }

    // Dynamically fit sleeve size to both available height and width
    // (full size 236x236 in wide preview, scales smoothly down to 110x110 in narrow windows)
    const int maxCovFromWidth = std::max(100, static_cast<int>((width() - 32) / 1.55));
    const int availableHeight = height() - 20;
    const int covSize = std::clamp(std::min(availableHeight, maxCovFromWidth), 110, 236);
    const int covX = 10;
    const int covY = (height() - covSize) / 2;

    const double scaleRatio = static_cast<double>(covSize) / 236.0;
    const QPointF vinylCenter(covX + covSize + 24.0 * scaleRatio, covY + covSize / 2.0);
    const double vinylR = covSize * 0.48;

    // 1. Vinyl disc drop shadow and grooved black body peeking out on the right
    drawSoftDropShadow(p, QRectF(vinylCenter.x() - vinylR, vinylCenter.y() - vinylR, vinylR * 2.0, vinylR * 2.0),
                       vinylR, 8, QColor(0, 0, 0, 180));

    QRadialGradient discGrad(vinylCenter, vinylR);
    discGrad.setColorAt(0.0, QColor("#1e242d"));
    discGrad.setColorAt(0.7, QColor("#0d1015"));
    discGrad.setColorAt(0.95, QColor("#1b2029"));
    discGrad.setColorAt(1.0, QColor("#090b0e"));
    p.setBrush(discGrad);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawEllipse(vinylCenter, vinylR, vinylR);

    // Concentric micro-grooves on vinyl surface
    p.setBrush(Qt::NoBrush);
    const int grooveStart = static_cast<int>(48.0 * scaleRatio);
    const int grooveStep = std::max(5, static_cast<int>(7.0 * scaleRatio));
    for (int r = grooveStart; r < static_cast<int>(vinylR) - 6; r += grooveStep) {
        p.setPen(QPen(QColor(255, 255, 255, 14), 0.8));
        p.drawEllipse(vinylCenter, r, r);
    }

    // 2. Center circular vinyl record label (76x76 at 236px sleeve scale)
    const int labelDiam = std::max(44, static_cast<int>(76.0 * scaleRatio));
    const int labelRadius = labelDiam / 2;
    if (!m_vinylLabel.isNull()) {
        QPixmap labelPix = makeRoundedPixmap(m_vinylLabel, labelDiam, labelDiam, labelRadius);
        p.drawPixmap(static_cast<int>(vinylCenter.x() - labelRadius),
                     static_cast<int>(vinylCenter.y() - labelRadius),
                     labelPix);
    } else {
        // Fallback studio gold/cyan center label when audio has no artwork
        QRadialGradient fallbackLabel(vinylCenter, labelRadius);
        fallbackLabel.setColorAt(0.0, QColor("#38bdf8"));
        fallbackLabel.setColorAt(0.75, QColor("#0284c7"));
        fallbackLabel.setColorAt(1.0, QColor("#0369a1"));
        p.setBrush(fallbackLabel);
        p.setPen(QPen(QColor(255, 255, 255, 45), 1.0));
        p.drawEllipse(vinylCenter, labelRadius, labelRadius);
    }

    // Center spindle hole
    const double holeR = std::max(4.5, 7.0 * scaleRatio);
    p.setBrush(QColor("#0b0e14"));
    p.setPen(QPen(QColor(255, 255, 255, 35), 0.8));
    p.drawEllipse(vinylCenter, holeR, holeR);

    // 3. Album front sleeve on the left
    const QRectF sleeveRect(covX, covY, covSize, covSize);
    drawSoftDropShadow(p, sleeveRect, 14.0, 10, QColor(0, 0, 0, 210));

    if (!m_sleeveCover.isNull()) {
        p.drawPixmap(covX, covY, makeRoundedPixmap(m_sleeveCover, covSize, covSize, 12));
    } else {
        // Fallback dark studio sleeve with musical emblem
        QLinearGradient sleeveGrad(sleeveRect.topLeft(), sleeveRect.bottomRight());
        sleeveGrad.setColorAt(0.0, QColor("#1c222d"));
        sleeveGrad.setColorAt(1.0, QColor("#12161f"));
        p.setBrush(sleeveGrad);
        p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
        p.drawRoundedRect(sleeveRect, 12.0, 12.0);

        QFont noteFont = font();
        noteFont.setPixelSize(static_cast<int>(54 * scaleRatio));
        p.setFont(noteFont);
        p.setPen(QColor("#38bdf8"));
        p.drawText(sleeveRect, Qt::AlignCenter, QStringLiteral("🎵"));
    }
}
