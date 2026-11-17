/**
 * @file gallery_zoom_transition.cpp
 * @brief Implementation of the morphing card zoom transition engine.
 */

#include "gallery_zoom_transition.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

void drawAspectPreservedPixmap(QPainter &p,
                               const QRectF &targetRect,
                               const QPixmap &pix,
                               qreal cropWeight)
{
    if (pix.isNull() || targetRect.isEmpty()) return;

    const qreal dpr = pix.devicePixelRatio();
    const QSizeF srcLogical = QSizeF(pix.size()) / dpr;
    const qreal srcAspect = srcLogical.width() / qMax(1.0, srcLogical.height());
    const qreal dstAspect = targetRect.width() / qMax(1.0, targetRect.height());

    QRectF cropLogical(QPointF(0.0, 0.0), srcLogical);
    const qreal wMix = qBound(0.0, cropWeight, 1.0);
    if (srcAspect > dstAspect) {
        const qreal targetW = srcLogical.height() * dstAspect;
        const qreal curW = srcLogical.width() + (targetW - srcLogical.width()) * wMix;
        cropLogical = QRectF((srcLogical.width() - curW) * 0.5, 0.0, curW, srcLogical.height());
    } else if (dstAspect > srcAspect) {
        const qreal targetH = srcLogical.width() / qMax(0.01, dstAspect);
        const qreal curH = srcLogical.height() + (targetH - srcLogical.height()) * wMix;
        cropLogical = QRectF(0.0, (srcLogical.height() - curH) * 0.5, srcLogical.width(), curH);
    }

    const QRectF srcPhysical(
        cropLogical.x() * dpr,
        cropLogical.y() * dpr,
        cropLogical.width() * dpr,
        cropLogical.height() * dpr
    );
    p.drawPixmap(targetRect, pix, srcPhysical);
}

void drawTargetThumbnail(QPainter &p,
                         const QRectF &targetRect,
                         const QPixmap &thumbPixmap,
                         const QPixmap &cardThumbnail,
                         const QPixmap &fallbackPix,
                         const QRect &fallbackLogicalRect)
{
    if (!thumbPixmap.isNull()) {
        p.drawPixmap(targetRect, thumbPixmap, QRectF(thumbPixmap.rect()));
    } else if (!cardThumbnail.isNull()) {
        p.drawPixmap(targetRect, cardThumbnail, QRectF(cardThumbnail.rect()));
    } else if (!fallbackPix.isNull() && fallbackLogicalRect.isValid()) {
        const qreal dpr = fallbackPix.devicePixelRatio();
        const QRectF phys(
            fallbackLogicalRect.x() * dpr,
            fallbackLogicalRect.y() * dpr,
            fallbackLogicalRect.width() * dpr,
            fallbackLogicalRect.height() * dpr
        );
        p.drawPixmap(targetRect, fallbackPix, phys);
    }
}

} // namespace

GalleryZoomTransition::GalleryZoomTransition(QWidget *parent)
    : QWidget(parent)
    , m_anim(new QVariantAnimation(this))
    , m_direction(Direction::ZoomInToViewer)
    , m_progress(0.0)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();

    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_anim, &QVariantAnimation::valueChanged, this, &GalleryZoomTransition::onAnimationProgress);
    connect(m_anim, &QVariantAnimation::finished, this, &GalleryZoomTransition::onAnimationFinished);
}

GalleryZoomTransition::~GalleryZoomTransition()
{
    stop();
}

void GalleryZoomTransition::start(const QPixmap &sourcePix,
                                 const QPixmap &targetPix,
                                 const QRect &cardRect,
                                 Direction direction,
                                 int durationMs,
                                 const QPixmap &cardThumbnail,
                                 const QRect &thumbRect,
                                 const QPixmap &thumbPixmap,
                                 const QRect &viewerRect)
{
    stop();

    if (sourcePix.isNull() || targetPix.isNull()) {
        emit finished();
        return;
    }

    m_sourcePix = sourcePix;
    m_targetPix = targetPix;
    m_cardThumbnail = cardThumbnail;
    m_thumbPixmap = thumbPixmap;
    m_cardRect = cardRect;
    m_thumbRect = thumbRect;
    m_viewerRect = viewerRect;
    m_direction = direction;
    m_progress = 0.0;

    // Validate cardRect; fallback to pleasant centered card proportion if empty or offscreen
    if (!m_cardRect.isValid() || m_cardRect.width() <= 0 || m_cardRect.height() <= 0) {
        int cw = 148;
        int ch = 152;
        m_cardRect = QRect((width() - cw) / 2, (height() - ch) / 2, cw, ch);
    }

    m_anim->setDuration(qMax(80, durationMs));
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    // Keep caller-defined geometry (e.g. bounded below header bar); fallback to parent only if unset
    if (parentWidget() && geometry().isEmpty()) {
        setGeometry(parentWidget()->rect());
    }

    raise();
    show();
    m_anim->start();
    update();
}

void GalleryZoomTransition::stop()
{
    if (m_anim->state() == QAbstractAnimation::Running) {
        m_anim->stop();
    }
    m_sourcePix = QPixmap();
    m_targetPix = QPixmap();
    m_cardThumbnail = QPixmap();
    m_thumbPixmap = QPixmap();
    m_thumbRect = QRect();
    m_viewerRect = QRect();
    m_progress = 0.0;
    hide();
}

bool GalleryZoomTransition::isRunning() const
{
    return m_anim->state() == QAbstractAnimation::Running;
}

void GalleryZoomTransition::onAnimationProgress(const QVariant &value)
{
    m_progress = value.toReal();
    update();
}

void GalleryZoomTransition::onAnimationFinished()
{
    hide();
    m_sourcePix = QPixmap();
    m_targetPix = QPixmap();
    m_cardThumbnail = QPixmap();
    m_thumbPixmap = QPixmap();
    m_thumbRect = QRect();
    m_viewerRect = QRect();
    emit finished();
}

void GalleryZoomTransition::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (m_sourcePix.isNull() || m_targetPix.isNull() || width() <= 0 || height() <= 0) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal t = m_progress;
    const QRect fullRect = rect();
    const bool hasThumbTarget = m_thumbRect.isValid() && m_thumbRect.width() > 0 && m_thumbRect.height() > 0;
    const QRect effectiveTargetRect = hasThumbTarget ? m_thumbRect : m_cardRect;
    const QRect effectiveViewerRect = (m_viewerRect.isValid() && !m_viewerRect.isEmpty()) ? m_viewerRect : fullRect;
    const qreal targetRadius = hasThumbTarget ? 10.0 : 16.0;

    const QRect startRect = (m_direction == Direction::ZoomInToViewer) ? effectiveTargetRect : effectiveViewerRect;
    const QRect endRect   = (m_direction == Direction::ZoomInToViewer) ? effectiveViewerRect : effectiveTargetRect;

    // Smooth sub-pixel geometry interpolation from source rect to target rect
    const qreal curX = startRect.x() + (endRect.x() - startRect.x()) * t;
    const qreal curY = startRect.y() + (endRect.y() - startRect.y()) * t;
    const qreal curW = startRect.width() + (endRect.width() - startRect.width()) * t;
    const qreal curH = startRect.height() + (endRect.height() - startRect.height()) * t;
    const QRectF morphGeom(curX, curY, curW, curH);

    if (m_direction == Direction::ZoomInToViewer) {
        // 1. Background: Gallery view gently fades out
        p.setOpacity(qBound(0.0, 1.0 - t * 0.75, 1.0));
        p.drawPixmap(fullRect, m_sourcePix);

        // Card container softly dissolves as thumbnail launches
        if (!m_cardThumbnail.isNull() && m_cardRect.isValid() && t < 0.45) {
            p.setOpacity(1.0 - (t / 0.45));
            p.drawPixmap(m_cardRect, m_cardThumbnail);
        }

        // 2. Physical elevation shadow during flight (lands softly to 0 at t -> 1.0)
        const qreal shadowFactor = (t < 0.25) ? qBound(0.0, t / 0.25, 1.0)
                                              : qBound(0.0, (1.0 - t) / 0.75, 1.0);
        if (shadowFactor > 0.001) {
            p.save();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, static_cast<int>(48 * shadowFactor)));
            p.drawRoundedRect(morphGeom.adjusted(2.0, 3.0, -2.0, 3.0), targetRadius * (1.0 - t) + 2.0, targetRadius * (1.0 - t) + 2.0);
            p.restore();
        }

        // 3. Morphing target view: expands from thumbnail/card and resolves into full viewer
        const qreal cornerRadius = targetRadius * (1.0 - t);
        QPainterPath clip;
        clip.addRoundedRect(morphGeom, cornerRadius, cornerRadius);
        p.save();
        p.setClipPath(clip);

        p.setOpacity(qBound(0.0, t * 1.40, 1.0));
        drawAspectPreservedPixmap(p, morphGeom, m_targetPix, 1.0 - t);

        if (t < 0.42) {
            p.setOpacity(1.0 - (t / 0.42));
            drawTargetThumbnail(p, morphGeom, m_thumbPixmap, m_cardThumbnail, m_sourcePix, effectiveTargetRect);
        }
        p.restore();

    } else {
        // ZoomOutToGallery
        // 1. Background: Target gallery view (with target slot empty) is visible at 100% immediately
        p.setOpacity(1.0);
        p.drawPixmap(fullRect, m_targetPix);

        // Subtle spatial depth dimmer that clears quickly as the viewer pulls back
        if (t < 0.45) {
            const qreal dimAlpha = 36.0 * (1.0 - (t / 0.45));
            p.fillRect(fullRect, QColor(0, 0, 0, static_cast<int>(dimAlpha)));
        }

        // Card frame & filename label materialize smoothly in the empty slot
        if (!m_cardThumbnail.isNull() && m_cardRect.isValid() && t > 0.20) {
            const qreal cardFrameAlpha = qBound(0.0, (t - 0.20) / 0.70, 1.0);
            p.save();
            p.setOpacity(cardFrameAlpha);
            p.drawPixmap(m_cardRect, m_cardThumbnail);
            p.restore();
        }

        // 2. Elevation shadow during flight, landing smoothly to 0 before contact
        const qreal shadowFactor = (t < 0.20) ? qBound(0.0, t / 0.20, 1.0)
                                              : qBound(0.0, (1.0 - t) / 0.80, 1.0);
        if (shadowFactor > 0.001) {
            p.save();
            p.setPen(Qt::NoPen);
            const qreal spread = 6.0 * (1.0 - t);
            p.setBrush(QColor(0, 0, 0, static_cast<int>(52 * shadowFactor)));
            p.drawRoundedRect(morphGeom.adjusted(-spread * 0.3, 2.0, spread * 0.3, 4.0 + spread * 0.6),
                              targetRadius * t + 2.0, targetRadius * t + 2.0);
            p.restore();
        }

        // 3. Morphing viewer: shrinks cleanly into exact thumbnail/icon rect without aspect distortion
        const qreal cornerRadius = targetRadius * t;
        QPainterPath clip;
        clip.addRoundedRect(morphGeom, cornerRadius, cornerRadius);
        p.save();
        p.setClipPath(clip);

        const qreal viewerAlpha = qBound(0.0, 1.0 - (t - 0.30) / 0.60, 1.0);
        if (viewerAlpha > 0.001) {
            p.setOpacity(viewerAlpha);
            drawAspectPreservedPixmap(p, morphGeom, m_sourcePix, qMin(1.0, t * 1.15));
        }

        const qreal thumbAlpha = qBound(0.0, (t - 0.35) / 0.55, 1.0);
        if (thumbAlpha > 0.001) {
            p.setOpacity(thumbAlpha);
            drawTargetThumbnail(p, morphGeom, m_thumbPixmap, m_cardThumbnail, m_targetPix, effectiveTargetRect);
        }
        p.restore();
    }
}
