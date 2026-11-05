/**
 * @file pdf_base_canvas.cpp
 * @brief Implementation of PdfBaseCanvas continuous PDF rendering widget.
 */

#include "pdf_base_canvas.h"
#include <poppler/qt6/poppler-qt6.h>

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <cmath>
#include <algorithm>

PdfBaseCanvas::PdfBaseCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PdfBaseCanvas::setDocument(Poppler::Document *doc, double zoom, int availWidth)
{
    m_doc = doc;
    m_zoomFactor = zoom;
    m_availWidth = availWidth;
    m_pageCache.clear();
    layoutPages();
    update();
}

void PdfBaseCanvas::clear()
{
    m_doc = nullptr;
    m_pageRects.clear();
    m_pageCache.clear();
    m_selectedText.clear();
    m_selectionRects.clear();
    m_searchMatchRects.clear();
    m_searchHighlightPage = -1;
    m_searchHighlightRect = QRectF();
    updateGeometry();
    update();
}

QRect PdfBaseCanvas::pageRect(int pageIndex) const
{
    if (pageIndex >= 0 && pageIndex < m_pageRects.size()) {
        return m_pageRects[pageIndex];
    }
    return QRect();
}

int PdfBaseCanvas::pageIndexAtY(int y) const
{
    for (int i = 0; i < m_pageRects.size(); ++i) {
        if (y >= m_pageRects[i].top() - 6 && y <= m_pageRects[i].bottom() + 6) {
            return i;
        }
    }
    return 0;
}

void PdfBaseCanvas::setSearchHighlight(int pageIndex, const QRectF &normRect)
{
    m_searchHighlightPage = pageIndex;
    m_searchHighlightRect = normRect;
    update();
}

void PdfBaseCanvas::clearSearchHighlight()
{
    m_searchHighlightPage = -1;
    m_searchHighlightRect = QRectF();
    update();
}

void PdfBaseCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (!m_doc || m_pageRects.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect visibleRect = visibleRegion().boundingRect();
    const double dpr = devicePixelRatioF();

    for (int i = 0; i < m_pageRects.size(); ++i) {
        const QRect &pRect = m_pageRects[i];
        if (!pRect.intersects(visibleRect)) {
            continue;
        }

        // Draw subtle page shadow and crisp border
        painter.fillRect(pRect.adjusted(3, 3, 3, 3), QColor(0, 0, 0, 45));
        painter.fillRect(pRect, Qt::white);

        // Render page image
        QImage img = m_pageCache.value(i);
        if (img.isNull()) {
            std::unique_ptr<Poppler::Page> page(m_doc->page(i));
            if (page) {
                const double dpi = std::clamp(72.0 * m_zoomFactor * dpr, 36.0, 450.0);
                img = page->renderToImage(dpi, dpi, -1, -1, -1, -1, Poppler::Page::Rotate0);
                if (!img.isNull()) {
                    m_pageCache.insert(i, img);
                }
            }
        }

        if (!img.isNull()) {
            painter.drawImage(pRect, img);
        }

        painter.setPen(QColor(100, 116, 139, 90));
        painter.drawRect(pRect);

        // Draw active text selection highlight (semi-transparent blue)
        if (m_selectionPage == i && !m_currentSelectionPageRect.isEmpty()) {
            QRect selRectOnCanvas = mapPageNormToCanvas(i, m_currentSelectionPageRect);
            painter.fillRect(selRectOnCanvas, QColor(56, 189, 248, 90));
        }

        // Draw active search match highlight (semi-transparent yellow)
        if (m_searchHighlightPage == i && !m_searchHighlightRect.isEmpty()) {
            QRect matchRect = mapPageNormToCanvas(i, m_searchHighlightRect);
            painter.fillRect(matchRect.adjusted(-2, -2, 2, 2), QColor(250, 204, 21, 150));
            painter.setPen(QPen(QColor(234, 179, 8), 1.5));
            painter.drawRect(matchRect.adjusted(-2, -2, 2, 2));
        }
    }
}

void PdfBaseCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_selectStartPos = event->pos();
        m_selectionPage = pageIndexAtY(event->pos().y());
        m_currentSelectionPageRect = QRectF();
        m_selectedText.clear();
        emit selectionChanged(false);
        update();
    }
    QWidget::mousePressEvent(event);
}

void PdfBaseCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting && m_doc && m_selectionPage >= 0 && m_selectionPage < m_doc->numPages()) {
        QRect curDragRect(m_selectStartPos, event->pos());
        QRect pageR = m_pageRects.value(m_selectionPage);
        if (!pageR.isEmpty()) {
            QRect inter = curDragRect.intersected(pageR);
            if (inter.width() > 3 && inter.height() > 3) {
                double normX = (double)(inter.left() - pageR.left()) / pageR.width();
                double normY = (double)(inter.top() - pageR.top()) / pageR.height();
                double normW = (double)inter.width() / pageR.width();
                double normH = (double)inter.height() / pageR.height();
                m_currentSelectionPageRect = QRectF(normX, normY, normW, normH);

                std::unique_ptr<Poppler::Page> page(m_doc->page(m_selectionPage));
                if (page) {
                    QSizeF pSize = page->pageSize();
                    QRectF textQueryRect(normX * pSize.width(), normY * pSize.height(),
                                         normW * pSize.width(), normH * pSize.height());
                    m_selectedText = page->text(textQueryRect);
                    emit selectionChanged(!m_selectedText.isEmpty());
                }
                update();
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PdfBaseCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void PdfBaseCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int numDegrees = event->angleDelta().y();
        const double factor = (numDegrees > 0) ? 1.15 : (1.0 / 1.15);
        emit zoomWheelRequested(factor);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void PdfBaseCanvas::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    if (!m_selectedText.isEmpty()) {
        QAction *actCopy = menu.addAction(tr("📋 Copy Selected Text"));
        connect(actCopy, &QAction::triggered, this, [this]() {
            if (QClipboard *cb = QApplication::clipboard()) {
                cb->setText(m_selectedText);
            }
        });
        menu.addSeparator();
    }

    QAction *actFitWidth = menu.addAction(tr("↔ Fit to Width"));
    connect(actFitWidth, &QAction::triggered, this, &PdfBaseCanvas::fitWidthRequested);

    menu.addSeparator();
    QAction *actInfo = menu.addAction(tr("ℹ️ Document Info..."));
    connect(actInfo, &QAction::triggered, this, &PdfBaseCanvas::showInfoRequested);

    QAction *actPrint = menu.addAction(tr("🖨️ Print Document... (Ctrl+P)"));
    connect(actPrint, &QAction::triggered, this, &PdfBaseCanvas::printRequested);

    menu.exec(event->globalPos());
}

void PdfBaseCanvas::layoutPages()
{
    m_pageRects.clear();
    if (!m_doc) {
        setFixedSize(0, 0);
        return;
    }

    const int numPages = m_doc->numPages();
    const int spacing = 16;
    int currentY = 16;
    int maxWidth = 0;

    for (int i = 0; i < numPages; ++i) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(i));
        if (!p) continue;

        QSizeF pSize = p->pageSize();
        int w = std::max(100, (int)std::round(pSize.width() * m_zoomFactor));
        int h = std::max(100, (int)std::round(pSize.height() * m_zoomFactor));

        maxWidth = std::max(maxWidth, w);
        m_pageRects.append(QRect(0, currentY, w, h));
        currentY += h + spacing;
    }

    int canvasW = std::max(maxWidth + 40, m_availWidth);
    for (int i = 0; i < m_pageRects.size(); ++i) {
        int x = std::max(20, (canvasW - m_pageRects[i].width()) / 2);
        m_pageRects[i].moveLeft(x);
    }

    setFixedSize(canvasW, currentY + 40);
}

QRect PdfBaseCanvas::mapPageNormToCanvas(int pageIndex, const QRectF &normRect) const
{
    QRect pRect = m_pageRects.value(pageIndex);
    if (pRect.isEmpty()) return QRect();

    int x = pRect.left() + (int)std::round(normRect.x() * pRect.width());
    int y = pRect.top() + (int)std::round(normRect.y() * pRect.height());
    int w = (int)std::round(normRect.width() * pRect.width());
    int h = (int)std::round(normRect.height() * pRect.height());
    return QRect(x, y, w, h);
}
