#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QColor>

/**
 * @class VinylSleeveWidget
 * @brief Analog studio artwork widget rendering the front album sleeve on the left
 * with a realistic grooved vinyl record and circular center label peeking out on the right.
 */
class VinylSleeveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VinylSleeveWidget(QWidget *parent = nullptr);

    /**
     * @brief Updates the front sleeve artwork and the center vinyl disc label artwork.
     * @param sleeveCover Primary album front cover pixmap.
     * @param vinylLabel Secondary disc/vinyl center label pixmap (falls back to sleeveCover if null).
     */
    void setArtwork(const QPixmap &sleeveCover, const QPixmap &vinylLabel = QPixmap());

    /**
     * @brief Clears current artwork and restores the default studio vinyl placeholder.
     */
    void clearArtwork();

    /**
     * @brief Toggles compact layout mode (centered square album cover without peeking vinyl disc).
     * @param compact True for compact mode, false for full vinyl sleeve + disc mode.
     */
    void setCompactMode(bool compact);
    bool isCompactMode() const { return m_compactMode; }

    QSize sizeHint() const override { return m_compactMode ? QSize(180, 180) : QSize(360, 240); }
    QSize minimumSizeHint() const override { return m_compactMode ? QSize(70, 70) : QSize(200, 130); }

    /**
     * @brief Renders a multi-pass soft drop shadow behind a rounded rectangle or ellipse.
     */
    static void drawSoftDropShadow(QPainter &p, const QRectF &rect, double radius, int blurSteps, const QColor &baseColor);

    /**
     * @brief Crops and scales a pixmap into a rounded rectangle with a subtle 1px highlight rim,
     * centered within the destination rectangle to preserve central framing of rectangular artwork.
     */
    static QPixmap makeRoundedPixmap(const QPixmap &src, int w, int h, int radius);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_sleeveCover;
    QPixmap m_vinylLabel;
    bool m_compactMode = false;
};

// Inline convenience helper for compatibility with existing code
inline QPixmap makeRoundedPixmap(const QPixmap &src, int w, int h, int radius)
{
    return VinylSleeveWidget::makeRoundedPixmap(src, w, h, radius);
}
