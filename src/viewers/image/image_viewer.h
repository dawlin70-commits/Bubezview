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

    /**
     * @brief Zooms into or out of the image anchored at a specific viewport position.
     * Keeps the pixel beneath the viewport coordinate fixed after scaling.
     * @param viewportPos Target focus point in viewport coordinates.
     * @param factor Zoom multiplier factor (e.g. 1.25 for zoom-in, 0.8 for zoom-out).
     */
    void zoomAt(const QPoint &viewportPos, double factor);

    void setZoomToolActive(bool active);
    bool isZoomToolActive() const { return m_zoomToolActive; }

    /**
     * @brief Returns the inner QScrollArea holding the image canvas.
     * @return Pointer to scroll area.
     */
    QScrollArea *scrollArea() const { return m_scrollArea; }

    /**
     * @brief Returns the canvas widget rendering the image and crop selection.
     * @return Pointer to inner canvas widget.
     */
    QWidget *canvasWidget() const { return reinterpret_cast<QWidget*>(m_canvas); }

    /**
     * @brief Returns the coordinated background color matching the active theme preset.
     */
    QColor canvasBackgroundColor() const;

    /**
     * @brief Transparency background rendering mode for images with an alpha channel.
     */
    enum class TransparencyMode {
        White,        ///< Solid pure white background (Default, high contrast, removes transparency).
        Checkerboard, ///< Subtle checkerboard pattern (Photoshop / GIMP style).
        Theme,        ///< Theme coordinated background color (Dark Slate / Breeze Dark / Native).
        Black         ///< Solid black background.
    };

    /**
     * @brief Gets current transparency background mode from settings.
     */
    TransparencyMode transparencyMode() const;

    /**
     * @brief Sets transparency background mode and refreshes canvas.
     */
    void setTransparencyMode(TransparencyMode mode);

    /**
     * @brief Permanently flattens image transparency onto a solid color (with full Undo/Redo support).
     * @param fillColor Color to replace transparent pixels with (defaults to pure white).
     */
    void removeTransparency(const QColor &fillColor = Qt::white);

    /**
     * @brief Helper indicating if the currently active theme is a dark theme.
     */
    bool isDarkTheme() const;

    // Geometric & visual transformations
    void rotateClockwise();
    void rotateCounterClockwise();
    void flipHorizontal();
    void flipVertical();
    void invertColors();

    // Undo / Redo support
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    void undo();
    void redo();
    void clearUndoRedo();

    // Geometric cropping & visual tools
    /**
     * @brief Crops the original image to the specified rectangle.
     * @param cropRect Bounding rectangle in original image coordinates.
     */
    void applyCrop(const QRect &cropRect);

    /**
     * @brief Toggles interactive visual cropping mode on the canvas.
     * @param active True to activate crop selection; false to cancel.
     */
    void setCropModeActive(bool active);
    bool isCropModeActive() const;
    void confirmCrop();
    void cancelCropMode();

    bool isModified() const { return m_isModified; }
    void setModified(bool mod);

    /**
     * @brief Refreshes scrollbar styles and canvas background based on active theme preset.
     */
    void updateTheme();

    /**
     * @brief Handles wheel zoom, pan, and adjacent image file navigation.
     * @param event The wheel event.
     */
    void handleWheel(QWheelEvent *event);

signals:
    /**
     * @brief Emitted when the undo/redo availability changes.
     * @param canUndo True if undo actions are available.
     * @param canRedo True if redo actions are available.
     */
    void undoAvailabilityChanged(bool canUndo, bool canRedo);

    /**
     * @brief Emitted when interactive crop mode state changes.
     * @param active True if currently in crop selection mode.
     */
    void cropModeChanged(bool active);
    /**
     * @brief Emitted when the image dirty/modified state changes.
     * @param modified True if image has unsaved modifications; false otherwise.
     */
    void modificationChanged(bool modified);

    /**
     * @brief Emitted when the image zoom factor changes.
     * @param factor Current zoom scale factor.
     */
    void zoomChanged(double factor);

    /**
     * @brief Emitted when mouse wheel scrolling requests navigating to the next image file.
     */
    void nextFileRequested();

    /**
     * @brief Emitted when mouse wheel scrolling requests navigating to the previous image file.
     */
    void prevFileRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateScaledPixmap();
    void pushUndoState();

    QScrollArea *m_scrollArea;
    ImageCanvasWidget *m_canvas;

    QImage m_originalImage;
    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;
    QPixmap m_sourcePixmap;
    QPixmap m_scaledPixmap;
    double m_zoomFactor;
    bool m_fitToWindow;
    bool m_zoomToolActive;
    bool m_isModified;

    int m_wheelNavAccumulator = 0;
    qint64 m_lastNavTime = 0;
};
