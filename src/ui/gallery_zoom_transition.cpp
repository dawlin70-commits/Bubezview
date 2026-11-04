/**
 * @file gallery_zoom_transition.cpp
 * @brief Implementation of the morphing card zoom transition engine.
 */

#include "gallery_zoom_transition.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

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
                                 int durationMs)
{
    stop();

    if (sourcePix.isNull() || targetPix.isNull()) {
        emit finished();
        return;
    }

    m_sourcePix = sourcePix;
    m_targetPix = targetPix;
    m_cardRect = cardRect;
    m_direction = direction;
    m_progress = 0.0;

    // Validate cardRect; fallback to pleasant centered card proportion if empty or offscreen
    if (!m_cardRect.isValid() || m_cardRect.width() <= 0 || m_cardRect.height() <= 0) {
        int cw = 154;
        int ch = 168;
        m_cardRect = QRect((width() - cw) / 2, (height() - ch) / 2, cw, ch);
    }

    m_anim->setDuration(qMax(80, durationMs));
    m_anim->setEasingCurve(direction == Direction::ZoomInToViewer ? QEasingCurve::OutCubic : QEasingCurve::InOutQuad);

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
    const QRect startRect = (m_direction == Direction::ZoomInToViewer) ? m_cardRect : fullRect;
    const QRect endRect   = (m_direction == Direction::ZoomInToViewer) ? fullRect  : m_cardRect;

    // Smooth geometry interpolation from source rect to target rect
    const qreal curX = startRect.x() + (endRect.x() - startRect.x()) * t;
    const qreal curY = startRect.y() + (endRect.y() - startRect.y()) * t;
    const qreal curW = startRect.width() + (endRect.width() - startRect.width()) * t;
    const qreal curH = startRect.height() + (endRect.height() - startRect.height()) * t;
    const QRectF morphGeom(curX, curY, curW, curH);

    if (m_direction == Direction::ZoomInToViewer) {
        // 1. Background: Gallery view gently fades out
        p.setOpacity(qBound(0.0, 1.0 - t * 0.75, 1.0));
        p.drawPixmap(fullRect, m_sourcePix);

        // 2. Physical elevation shadow during flight
        if (t < 0.96) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, static_cast<int>(55 * (1.0 - t))));
            p.drawRoundedRect(morphGeom.adjusted(2, 4, 3, 5), 10, 10);
        }

        // 3. Morphing target view: expands from card and resolves into full viewer
        const qreal cornerRadius = 9.0 * (1.0 - t);
        QPainterPath clip;
        clip.addRoundedRect(morphGeom, cornerRadius, cornerRadius);
        p.save();
        p.setClipPath(clip);

        // Crossfade: at very beginning, blend card thumbnail into the resolving full viewer
        p.setOpacity(qBound(0.0, t * 1.25, 1.0));
        p.drawPixmap(morphGeom.toRect(), m_targetPix);

        if (t < 0.40) {
            p.setOpacity(1.0 - (t / 0.40));
            p.drawPixmap(morphGeom.toRect(), m_sourcePix, m_cardRect);
        }
        p.restore();

    } else {
        // ZoomOutToGallery
        // 1. Background: Target gallery view gently reveals
        p.setOpacity(qBound(0.0, t * 1.1, 1.0));
        p.drawPixmap(fullRect, m_targetPix);

        // 2. Elevation shadow as the viewer shrinks into the grid
        if (t > 0.05) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, static_cast<int>(50 * t)));
            p.drawRoundedRect(morphGeom.adjusted(2, 4, 3, 5), 10, 10);
        }

        // 3. Morphing viewer: collapses from full window into target card
        const qreal cornerRadius = 9.0 * t;
        QPainterPath clip;
        clip.addRoundedRect(morphGeom, cornerRadius, cornerRadius);
        p.save();
        p.setClipPath(clip);

        p.setOpacity(qBound(0.0, 1.0 - t * 0.45, 1.0));
        p.drawPixmap(morphGeom.toRect(), m_sourcePix);

        if (t > 0.60) {
            p.setOpacity((t - 0.60) / 0.40);
            p.drawPixmap(morphGeom.toRect(), m_targetPix, m_cardRect);
        }
        p.restore();
    }
}
