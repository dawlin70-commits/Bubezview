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

namespace Poppler {
    class Document;
}

class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;

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
     * @brief Configures document, zoom, and viewport layout.
     * @param doc Poppler Document pointer (non-owning).
     * @param zoom Display scale factor (1.0 = 100%).
     * @param availWidth Total available viewport pixel width for centering.
     */
    void setDocument(Poppler::Document *doc, double zoom, int availWidth);

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
    QRect pageRect(int pageIndex) const;

    /**
     * @brief Finds which page corresponds to a vertical canvas coordinate.
     */
    int pageIndexAtY(int y) const;

    /**
     * @brief True if user has selected text on canvas.
     */
    bool hasTextSelection() const { return !m_selectedText.isEmpty(); }

    /**
     * @brief Currently selected text string.
     */
    QString selectedText() const { return m_selectedText; }

    /**
     * @brief Highlights a search match on the canvas.
     */
    void setSearchHighlight(int pageIndex, const QRectF &normRect);

    /**
     * @brief Clears any active search match highlight.
     */
    void clearSearchHighlight();

signals:
    void activePageChanged(int pageIndex);
    void selectionChanged(bool hasSelection);
    void zoomWheelRequested(double factor);
    void showInfoRequested();
    void printRequested();
    void fitWidthRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void layoutPages();
    QRect mapPageNormToCanvas(int pageIndex, const QRectF &normRect) const;

    Poppler::Document *m_doc = nullptr;
    double m_zoomFactor = 1.0;
    int m_availWidth = 600;

    QVector<QRect> m_pageRects;
    QMap<int, QImage> m_pageCache;

    bool m_isSelecting = false;
    QPoint m_selectStartPos;
    int m_selectionPage = -1;
    QRectF m_currentSelectionPageRect;
    QString m_selectedText;
    QVector<QRectF> m_selectionRects;

    int m_searchHighlightPage = -1;
    QRectF m_searchHighlightRect;
    QVector<QRectF> m_searchMatchRects;
};
