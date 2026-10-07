#ifndef OCR_SNIPPER_OVERLAY_H
#define OCR_SNIPPER_OVERLAY_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <functional>

/**
 * @brief Fullscreen/Target-covering translucent overlay for interactive rubber-band snippet capture.
 *
 * Temporarily covers the viewer window with a dimmed background and crosshair cursor.
 * When the user drags and releases the mouse, the selected rectangular area is captured
 * and passed back via signal or callback for high-fidelity OCR processing.
 */
class OcrSnipperOverlay : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the snipper overlay over the specified target widget.
     * @param targetWidget The widget (viewer or main window) to capture.
     * @param parent Optional parent widget.
     */
    explicit OcrSnipperOverlay(QWidget *targetWidget, QWidget *parent = nullptr);
    ~OcrSnipperOverlay() override = default;

    /**
     * @brief Sets an optional callback to map the relative selection rectangle
     * to a high-DPI source image (e.g. 300 DPI PDF rendering).
     * @param mapper Function taking (normalizedRectF) and returning high-res QImage snippet.
     */
    void setHighDpiCropper(std::function<QImage(const QRectF &)> cropper);

signals:
    /**
     * @brief Emitted when a valid snippet rectangle has been selected.
     * @param snippet The cropped image of the selected area.
     */
    void snippetCaptured(const QImage &snippet);

    /**
     * @brief Emitted when snipping is aborted by the user (via Esc, right click, or click outside).
     */
    void snippingCanceled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *m_targetWidget;
    QPixmap m_backgroundPixmap;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    bool m_isSelecting;
    std::function<QImage(const QRectF &)> m_highDpiCropper;
};

#endif // OCR_SNIPPER_OVERLAY_H
