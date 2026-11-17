/**
 * @file pdf_base_canvas.h
 * @brief Continuous interactive canvas widget for rendering PDF pages in PdfBaseViewer.
 *
 * Handles high-DPI rasterization with Poppler, smooth scrolling, text selection,
 * search term visual highlighting, and contextual canvas actions.
 */

#pragma once

#include <QWidget>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <QMap>
#include <QImage>
#include <QTimer>
#include <memory>
#include <vector>

#include <poppler/qt6/poppler-qt6.h>

class QScrollArea;
class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class QKeyEvent;

/**
 * @class PdfBaseCanvas
 * @brief Interactive drawing canvas rendering vertical continuous PDF pages.
 */
class PdfBaseCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PdfBaseCanvas(QWidget *parent = nullptr);
    ~PdfBaseCanvas() override = default;

    /**
     * @brief Configures document, zoom, rotation, and viewport layout.
     * @param doc Poppler Document pointer (non-owning).
     * @param zoom Display scale factor (1.0 = 100%).
     * @param rotation Clockwise view rotation degrees (0, 90, 180, 270).
     * @param availWidth Total available viewport pixel width for centering.
     * @param availHeight Total available viewport pixel height for fitting.
     * @param fitToPage If true, each page scales to fit viewport width and height.
     * @param fitToWidth If true, each page scales to fit viewport width.
     */
    void setDocument(Poppler::Document *doc, double zoom, int rotation, int availWidth, int availHeight = 600, bool fitToPage = false, bool fitToWidth = false);

    /**
     * @brief Effective scale factor for a specific page.
     */
    double pageScale(int pageIndex) const;

    /**
     * @brief Returns rotated dimensions for a specific page in points.
     */
    QSizeF getRotatedPageSize(int pageIndex) const;

    /**
     * @brief Clears document layout and frees cached rendered page bitmaps.
     */
    void clear();

    /**
     * @brief Total number of pages laid out.
     */
    int pageCount() const { return m_pageRects.size(); }

    /**
     * @brief Canvas coordinate bounds for a specific page.
     */
    Q_INVOKABLE QRect pageRect(int pageIndex) const;

    /**
     * @brief Finds which page corresponds to a vertical canvas coordinate.
     */
    int pageIndexAtY(int y) const;
    Q_INVOKABLE int pageIndexAtPos(const QPoint &pos) const { return pageIndexAtY(pos.y()); }
    Q_INVOKABLE int activePage() const { return m_selectedPageIndex >= 0 ? m_selectedPageIndex : 0; }

    /**
     * @brief True if user has selected text on canvas.
     */
    bool hasTextSelection() const { return !m_selectedText.isEmpty(); }

    /**
     * @brief Currently selected text string.
     */
    QString selectedText() const { return m_selectedText; }

    /**
     * @brief Highlights a search match on the canvas with point coordinates.
     */
    void setSearchHighlight(int pageIndex, const QRectF &pointRect);

    /**
     * @brief Clears any active search match highlight.
     */
    void clearSearchHighlight();

    /**
     * @brief Assigns parent scroll area for smooth panning and viewport zooming.
     */
    void setScrollArea(QScrollArea *sa) { m_scrollArea = sa; }

    /**
     * @brief Toggles freehand pan/drag mode.
     */
    void setHandToolActive(bool active);
    bool isHandToolActive() const { return m_handToolActive; }

    /**
     * @brief Current view rotation in degrees (0, 90, 180, 270).
     */
    int rotation() const { return m_rotation; }

    /**
     * @brief Resolves page index at a given canvas position.
     * @return 0-based page index, or -1 if not inside any page.
     */
    int pageAtPos(const QPoint &pos) const;

    /**
     * @brief Maps canvas pixel coordinate to PDF page coordinate in typographic points (72 DPI).
     */
    QPointF mapToPagePoint(int pageIndex, const QPoint &canvasPos) const;

    /**
     * @brief Maps PDF page coordinate in points (72 DPI) to canvas pixel coordinate.
     */
    QPoint mapFromPagePoint(int pageIndex, const QPointF &pagePt) const;

    /**
     * @brief Maps a PDF page rectangle in points to a canvas pixel rectangle.
     */
    QRect mapFromPageRect(int pageIndex, const QRectF &pointRect) const;

    /**
     * @brief Configures editor overlay mode ("carta velina").
     */
    void setEditMode(bool editMode);
    bool isEditMode() const { return m_editMode; }

    /**
     * @brief Attaches a transparent editor overlay widget over the canvas.
     */
    void setEditorOverlay(QWidget *overlay);
    QWidget *editorOverlay() const { return m_editorOverlay; }

signals:
    void activePageChanged(int pageIndex);
    void selectionChanged(bool hasSelection);
    void zoomWheelRequested(double factor);
    void zoomAtRequested(const QPoint &viewportPos, double factor);
    void showInfoRequested();
    void printRequested();
    void fitWidthRequested();
    void fitPageRequested();
    void savePageAsPdfRequested();
    void savePageAsImageRequested();
    void saveAsRequested();
    void exportRequested();
    void scanTextRequested();
    void requestStudio();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void layoutPages();
    void loadTextBoxesForPage(int pageIndex);
    void updateSelection(const QPoint &startPt, const QPoint &endPt);

    Poppler::Document *m_doc = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    double m_zoomFactor = 1.0;
    int m_rotation = 0;
    int m_availWidth = 600;
    int m_availHeight = 600;
    bool m_fitToPage = true;
    bool m_fitToWidth = false;

    bool m_handToolActive = true;
    bool m_spacePressed = false;
    bool m_isPanning = false;
    QPoint m_panStartPos;

    QVector<QRect> m_pageRects;
    QVector<double> m_pageScales;
    QMap<int, QImage> m_pageCache;
    QMap<int, double> m_cachedScales;
    int m_maxCachePages = 16;
    QTimer *m_renderDebounceTimer = nullptr;

    bool m_isSelecting = false;
    QPoint m_selectStartPos;
    int m_selectedPageIndex = -1;
    std::vector<std::unique_ptr<Poppler::TextBox>> m_activeTextBoxes;
    std::vector<int> m_selectedIndices;
    QString m_selectedText;

    int m_searchHighlightPage = -1;
    QRectF m_searchHighlightPointRect;

    bool m_editMode = false;
    QWidget *m_editorOverlay = nullptr;
};
