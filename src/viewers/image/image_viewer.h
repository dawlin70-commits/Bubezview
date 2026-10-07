#pragma once

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QScrollArea>
#include <QResizeEvent>
#include <QWheelEvent>

class ImageCanvasWidget;

/**
 * @brief High-performance hardware-accelerated image viewer widget.
 *
 * Supports smooth rendering, panning, zooming, 90-degree rotations,
 * horizontal/vertical flipping, and color inversion.
 * Features full QScrollArea support with high-contrast visible scrollbars
 * and mouse click-and-drag panning.
 */
class ImageViewer : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);
    ~ImageViewer() override;

    /**
     * @brief Loads an image from disk into memory.
     * @param filePath Absolute path of the image file.
     * @return True if loaded successfully; false otherwise.
     */
    bool loadFile(const QString &filePath);

    /**
     * @brief Loads an already decoded QImage directly (e.g. from prefetch cache).
     * @param image Decoded QImage.
     * @return True if image is valid; false otherwise.
     */
    bool loadImage(const QImage &image);

    /**
     * @brief Clears currently loaded image and frees backing pixmap cache.
     */
    void clear();
    
    QSize imageSize() const { return m_originalImage.size(); }
    const QImage &originalImage() const { return m_originalImage; }
    double zoomFactor() const { return m_zoomFactor; }

    void resetZoom();
    void fitToWindow();
    bool isFitToWindow() const { return m_fitToWindow; }
    void zoomIn();
    void zoomOut();

    void setZoomToolActive(bool active);
    bool isZoomToolActive() const { return m_zoomToolActive; }

    // Geometric & visual transformations
    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void invertColors();

    bool isModified() const { return m_isModified; }
    void setModified(bool mod) { m_isModified = mod; }

    /**
     * @brief Refreshes scrollbar styles and canvas background based on active theme preset.
     */
    void updateTheme();

    /**
     * @brief Handles wheel zoom and scroll delegation.
     * @param event The wheel event.
     */
    void handleWheel(QWheelEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateScaledPixmap();

    QScrollArea *m_scrollArea;
    ImageCanvasWidget *m_canvas;

    QImage m_originalImage;
    QPixmap m_scaledPixmap;
    double m_zoomFactor;
    bool m_fitToWindow;
    bool m_zoomToolActive;
    bool m_isModified;
};
