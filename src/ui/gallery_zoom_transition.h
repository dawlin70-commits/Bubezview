/**
 * @file gallery_zoom_transition.h
 * @brief Morphing card zoom transition engine for the Folder Gallery.
 */

#pragma once

#include <QWidget>
#include <QPixmap>
#include <QRect>
#include <QVariantAnimation>
#include <QEasingCurve>

/**
 * @class GalleryZoomTransition
 * @brief Smooth morphing zoom transition overlay between folder cards and full viewers.
 *
 * Implements fluid geometry interpolation from the exact thumbnail card position
 * in the gallery grid up to the full-window viewer and vice-versa, matching the
 * macOS Quick Look and iOS Photos card expansion physics.
 */
class GalleryZoomTransition : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Transition direction.
     */
    enum class Direction {
        ZoomInToViewer,   ///< Expanding from card rect in gallery to full viewer
        ZoomOutToGallery  ///< Shrinking from full viewer down into card rect in gallery
    };

    /**
     * @brief Constructs the gallery zoom transition overlay.
     * @param parent Stacked widget or viewer container.
     */
    explicit GalleryZoomTransition(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~GalleryZoomTransition() override;

    /**
     * @brief Starts the morphing zoom animation.
     * @param sourcePix Snapshot of the view before the transition.
     * @param targetPix Snapshot of the view after the transition.
     * @param cardRect Geometry of the clicked or target card in coordinates of this overlay.
     * @param direction ZoomInToViewer or ZoomOutToGallery.
     * @param durationMs Animation duration in milliseconds (default 360ms).
     * @param cardThumbnail Optional isolated pixmap of the full card for pixel-perfect slot landing.
     * @param thumbRect Optional exact geometry of the thumbnail/icon within cardRect.
     * @param thumbPixmap Optional isolated pixmap of the thumbnail/icon region.
     * @param viewerRect Optional source/target viewer geometry within overlay when layout height shifts.
     */
    void start(const QPixmap &sourcePix,
               const QPixmap &targetPix,
               const QRect &cardRect,
               Direction direction,
               int durationMs = 360,
               const QPixmap &cardThumbnail = QPixmap(),
               const QRect &thumbRect = QRect(),
               const QPixmap &thumbPixmap = QPixmap(),
               const QRect &viewerRect = QRect());

    /**
     * @brief Immediately stops any active animation and hides the overlay.
     */
    void stop();

    /**
     * @brief Returns whether a transition animation is actively running.
     * @return True if animating; false otherwise.
     */
    bool isRunning() const;

signals:
    /**
     * @brief Emitted when the zoom animation has completed.
     */
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAnimationProgress(const QVariant &value);
    void onAnimationFinished();

private:
    QVariantAnimation *m_anim;
    QPixmap m_sourcePix;
    QPixmap m_targetPix;
    QPixmap m_cardThumbnail;
    QPixmap m_thumbPixmap;
    QRect m_cardRect;
    QRect m_thumbRect;
    QRect m_viewerRect;
    Direction m_direction;
    qreal m_progress;
};
