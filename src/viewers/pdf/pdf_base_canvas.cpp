/**
 * @file pdf_base_canvas.cpp
 * @brief Implementation of PdfBaseCanvas continuous PDF rendering widget.
 */

#include "pdf_base_canvas.h"
#include "pdf_canvas_tools.h"
#include "theme_manager.h"
#include <poppler/qt6/poppler-qt6.h>

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <cmath>
#include <algorithm>

PdfBaseCanvas::PdfBaseCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setAcceptDrops(true);

    QSettings settings(QStringLiteral("Bubez"), QStringLiteral("BubezView"));
    m_maxCachePages = (settings.value(QStringLiteral("hardware_profile"), QStringLiteral("balanced")).toString() == QStringLiteral("desktop")) ? 24 : 12;

    m_renderDebounceTimer = new QTimer(this);
    m_renderDebounceTimer->setSingleShot(true);
    m_renderDebounceTimer->setInterval(80);
    connect(m_renderDebounceTimer, &QTimer::timeout, this, [this]() {
        bool needsRepaint = false;
        for (auto it = m_cachedScales.begin(); it != m_cachedScales.end();) {
            int pageIdx = it.key();
            double cachedScale = it.value();
            double targetScale = pageScale(pageIdx);
            if (std::abs(cachedScale - targetScale) > 0.01) {
                m_pageCache.remove(pageIdx);
                it = m_cachedScales.erase(it);
                needsRepaint = true;
            } else {
                ++it;
            }
        }
        if (needsRepaint) {
            update();
        }
    });
}

void PdfBaseCanvas::setHandToolActive(bool active)
{
    m_handToolActive = active;
    m_isPanning = false;
    setCursor(m_handToolActive ? Qt::OpenHandCursor : Qt::IBeamCursor);
    update();
}

void PdfBaseCanvas::setDocument(Poppler::Document *doc, double zoom, int rotation, int availWidth, int availHeight, bool fitToPage, bool fitToWidth)
{
    bool docChanged = (m_doc != doc);
    bool rotChanged = (m_rotation != rotation);

    m_doc = doc;
    m_zoomFactor = zoom;
    m_rotation = rotation;
    m_availWidth = availWidth;
    m_availHeight = availHeight;
    m_fitToPage = fitToPage;
    m_fitToWidth = fitToWidth;

    if (docChanged || rotChanged) {
        m_pageCache.clear();
        m_cachedScales.clear();
        m_activeTextBoxes.clear();
        m_selectedIndices.clear();
        m_selectedPageIndex = -1;
        m_selectedText.clear();
    }

    layoutPages();

    if (m_renderDebounceTimer) {
        m_renderDebounceTimer->start();
    }
    update();
}

double PdfBaseCanvas::pageScale(int pageIndex) const
{
    if (pageIndex >= 0 && pageIndex < m_pageScales.size()) {
        return m_pageScales[pageIndex];
    }
    return m_zoomFactor;
}

QSizeF PdfBaseCanvas::getRotatedPageSize(int pageIndex) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_doc->numPages()) {
        return QSizeF(595, 842);
    }
    std::unique_ptr<Poppler::Page> p(m_doc->page(pageIndex));
    if (!p) return QSizeF(595, 842);

    QSizeF sz = p->pageSizeF();
    if (m_rotation == 90 || m_rotation == 270) {
        sz.transpose();
    }
    return sz;
}

void PdfBaseCanvas::clear()
{
    m_doc = nullptr;
    m_pageRects.clear();
    m_pageScales.clear();
    m_pageCache.clear();
    m_cachedScales.clear();
    if (m_renderDebounceTimer) {
        m_renderDebounceTimer->stop();
    }
    m_activeTextBoxes.clear();
    m_selectedIndices.clear();
    m_selectedPageIndex = -1;
    m_selectedText.clear();
    m_searchHighlightPage = -1;
    m_searchHighlightPointRect = QRectF();
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
    if (m_pageRects.isEmpty()) return 0;
    if (y < m_pageRects.first().top()) return 0;
    if (y > m_pageRects.last().bottom()) return m_pageRects.size() - 1;

    for (int i = 0; i < m_pageRects.size(); ++i) {
        const QRect &r = m_pageRects[i];
        if (y >= r.top() - 8 && y <= r.bottom() + 8) {
            return i;
        }
    }
    for (int i = 0; i < m_pageRects.size() - 1; ++i) {
        if (y > m_pageRects[i].bottom() && y < m_pageRects[i + 1].top()) {
            int distToCurrent = y - m_pageRects[i].bottom();
            int distToNext = m_pageRects[i + 1].top() - y;
            return (distToNext < distToCurrent) ? (i + 1) : i;
        }
    }
    return 0;
}

void PdfBaseCanvas::setSearchHighlight(int pageIndex, const QRectF &pointRect)
{
    m_searchHighlightPage = pageIndex;
    m_searchHighlightPointRect = pointRect;
    update();
}

void PdfBaseCanvas::clearSearchHighlight()
{
    m_searchHighlightPage = -1;
    m_searchHighlightPointRect = QRectF();
    update();
}

int PdfBaseCanvas::pageAtPos(const QPoint &pos) const
{
    for (int i = 0; i < m_pageRects.size(); ++i) {
        if (m_pageRects[i].contains(pos)) {
            return i;
        }
    }
    return -1;
}

QPointF PdfBaseCanvas::mapToPagePoint(int pageIndex, const QPoint &canvasPos) const
{
    if (pageIndex < 0 || pageIndex >= m_pageRects.size()) {
        return QPointF();
    }
    const QRect &pRect = m_pageRects[pageIndex];
    QSizeF pSize = getRotatedPageSize(pageIndex);
    double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
    double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());

    double localX = canvasPos.x() - pRect.left();
    double localY = canvasPos.y() - pRect.top();

    return QPointF(localX / scaleX, localY / scaleY);
}

QPoint PdfBaseCanvas::mapFromPagePoint(int pageIndex, const QPointF &pagePt) const
{
    if (pageIndex < 0 || pageIndex >= m_pageRects.size()) {
        return QPoint();
    }
    const QRect &pRect = m_pageRects[pageIndex];
    QSizeF pSize = getRotatedPageSize(pageIndex);
    double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
    double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());

    int x = pRect.left() + qRound(pagePt.x() * scaleX);
    int y = pRect.top() + qRound(pagePt.y() * scaleY);
    return QPoint(x, y);
}

QRect PdfBaseCanvas::mapFromPageRect(int pageIndex, const QRectF &pointRect) const
{
    if (pageIndex < 0 || pageIndex >= m_pageRects.size()) {
        return QRect();
    }
    const QRect &pRect = m_pageRects[pageIndex];
    QSizeF pSize = getRotatedPageSize(pageIndex);
    double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
    double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());

    int x = pRect.left() + qRound(pointRect.left() * scaleX);
    int y = pRect.top() + qRound(pointRect.top() * scaleY);
    int w = qRound(pointRect.width() * scaleX);
    int h = qRound(pointRect.height() * scaleY);
    return QRect(x, y, w, h);
}

void PdfBaseCanvas::setEditMode(bool editMode)
{
    m_editMode = editMode;
    if (m_editorOverlay) {
        m_editorOverlay->setVisible(editMode);
        if (editMode) {
            m_editorOverlay->setGeometry(rect());
            m_editorOverlay->raise();
        }
    }
    if (!editMode) {
        setHandToolActive(true);
    } else {
        setHandToolActive(false);
    }
    update();
}

void PdfBaseCanvas::setEditorOverlay(QWidget *overlay)
{
    if (m_editorOverlay == overlay) return;
    if (m_editorOverlay && m_editorOverlay->parent() == this) {
        m_editorOverlay->hide();
    }
    m_editorOverlay = overlay;
    if (m_editorOverlay) {
        m_editorOverlay->setParent(this);
        m_editorOverlay->setGeometry(rect());
        m_editorOverlay->setVisible(m_editMode);
        if (m_editMode) {
            m_editorOverlay->raise();
        }
    }
}

void PdfBaseCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_editorOverlay) {
        m_editorOverlay->setGeometry(rect());
    }
}

void PdfBaseCanvas::loadTextBoxesForPage(int pageIndex)
{
    if (pageIndex == m_selectedPageIndex && !m_activeTextBoxes.empty()) return;

    m_selectedPageIndex = pageIndex;
    m_activeTextBoxes.clear();
    m_selectedIndices.clear();

    if (!m_doc || pageIndex < 0 || pageIndex >= m_doc->numPages()) return;

    std::unique_ptr<Poppler::Page> p(m_doc->page(pageIndex));
    if (p) {
        Poppler::Page::Rotation popplerRot = Poppler::Page::Rotate0;
        if (m_rotation == 90) popplerRot = Poppler::Page::Rotate90;
        else if (m_rotation == 180) popplerRot = Poppler::Page::Rotate180;
        else if (m_rotation == 270) popplerRot = Poppler::Page::Rotate270;

        m_activeTextBoxes = p->textList(popplerRot);
    }
}

void PdfBaseCanvas::updateSelection(const QPoint &startPt, const QPoint &endPt)
{
    if (m_activeTextBoxes.empty() || m_selectedPageIndex < 0 || m_selectedPageIndex >= m_pageRects.size()) {
        return;
    }

    QRect pRect = m_pageRects[m_selectedPageIndex];
    QSizeF pSize = getRotatedPageSize(m_selectedPageIndex);
    double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
    double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());

    int startIdx = PdfCanvasTools::findClosestTextBox(startPt, m_activeTextBoxes, scaleX, scaleY);
    int endIdx = PdfCanvasTools::findClosestTextBox(endPt, m_activeTextBoxes, scaleX, scaleY);

    if (startIdx >= 0 && endIdx >= 0) {
        int fromIdx = std::min(startIdx, endIdx);
        int toIdx = std::max(startIdx, endIdx);

        std::vector<int> newSelection;
        newSelection.reserve(toIdx - fromIdx + 1);
        for (int i = fromIdx; i <= toIdx; ++i) {
            newSelection.push_back(i);
        }

        if (newSelection != m_selectedIndices) {
            m_selectedIndices = std::move(newSelection);

            QString result;
            for (size_t i = 0; i < m_selectedIndices.size(); ++i) {
                int idx = m_selectedIndices[i];
                if (idx >= 0 && idx < static_cast<int>(m_activeTextBoxes.size())) {
                    if (i > 0) result += QStringLiteral(" ");
                    result += m_activeTextBoxes[idx]->text();
                }
            }
            m_selectedText = result;
            emit selectionChanged(!m_selectedText.isEmpty());
            update();
        }
    }
}

void PdfBaseCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp" || ext == "svg" || ext == "tiff" || ext == "tif") {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void PdfBaseCanvas::dropEvent(QDropEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString path = url.toLocalFile();
            QImage img(path);
            if (!img.isNull()) {
                emit requestStudio();
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dropEvent(event);
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

    Poppler::Page::Rotation popplerRot = Poppler::Page::Rotate0;
    if (m_rotation == 90) popplerRot = Poppler::Page::Rotate90;
    else if (m_rotation == 180) popplerRot = Poppler::Page::Rotate180;
    else if (m_rotation == 270) popplerRot = Poppler::Page::Rotate270;

    for (int i = 0; i < m_pageRects.size(); ++i) {
        const QRect &pRect = m_pageRects[i];
        if (!pRect.intersects(visibleRect)) {
            continue;
        }

        // Draw soft multi-layer rounded drop shadow around page
        const qreal pageRadius = 12.0;
        const QRectF pageF(pRect);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 14));
        painter.drawRoundedRect(pageF.adjusted(-4, -1, 4, 7), pageRadius + 3.0, pageRadius + 3.0);
        painter.setBrush(QColor(0, 0, 0, 26));
        painter.drawRoundedRect(pageF.adjusted(-2, 1, 2, 4), pageRadius + 1.5, pageRadius + 1.5);

        // Render page image with bounded LRU caching
        QImage img = m_pageCache.value(i);
        const double effScale = pageScale(i);

        if (img.isNull()) {
            std::unique_ptr<Poppler::Page> page(m_doc->page(i));
            if (page) {
                const double baseDpi = 72.0 * effScale;
                const QSizeF ptSize = page->pageSizeF();
                const double maxPt = std::max(ptSize.width(), ptSize.height());
                const double maxDpiForPage = (maxPt > 1.0) ? (4096.0 * 72.0 / maxPt) : 500.0;
                const double targetDpi = std::clamp(std::max(192.0, baseDpi * 1.75) * dpr, 72.0, std::min(500.0, maxDpiForPage));
                img = page->renderToImage(targetDpi, targetDpi, -1, -1, -1, -1, popplerRot);
                if (!img.isNull()) {
                    m_pageCache.insert(i, img);
                    m_cachedScales.insert(i, effScale);

                    // Bounded LRU + RAM budget cache eviction (never exceeding 256 MB or m_maxCachePages)
                    constexpr qsizetype MAX_CACHE_BYTES = 256LL * 1024 * 1024;
                    auto totalCacheBytes = [this]() -> qsizetype {
                        qsizetype total = 0;
                        for (auto it = m_pageCache.constBegin(); it != m_pageCache.constEnd(); ++it) {
                            total += it.value().sizeInBytes();
                        }
                        return total;
                    };
                    const int activePage = pageIndexAtY(visibleRect.center().y());
                    while (m_pageCache.size() > 1 &&
                           (m_pageCache.size() > m_maxCachePages || totalCacheBytes() > MAX_CACHE_BYTES)) {
                        int furthest = -1;
                        int maxDist = -1;
                        for (auto it = m_pageCache.constBegin(); it != m_pageCache.constEnd(); ++it) {
                            if (it.key() == i) continue;
                            int dist = std::abs(it.key() - activePage);
                            if (dist > maxDist) {
                                maxDist = dist;
                                furthest = it.key();
                            }
                        }
                        if (furthest < 0) break;
                        m_pageCache.remove(furthest);
                        m_cachedScales.remove(furthest);
                    }
                }
            }
        }

        // Clip page paper and rendered content to 12px rounded corners
        painter.save();
        QPainterPath pageClipPath;
        pageClipPath.addRoundedRect(pageF, pageRadius, pageRadius);
        painter.setClipPath(pageClipPath);
        painter.fillRect(pRect, Qt::white);
        if (!img.isNull()) {
            painter.drawImage(pRect, img);
        }
        painter.restore();

        // Crisp rounded inner rim
        painter.setPen(QPen(QColor(148, 163, 184, 75), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(pageF.adjusted(0.5, 0.5, -0.5, -0.5), pageRadius, pageRadius);

        QSizeF pSize = getRotatedPageSize(i);
        double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
        double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());

        // Draw character-level text selection (natural Poppler textList highlights)
        if (m_selectedPageIndex == i && !m_selectedIndices.empty() && !m_activeTextBoxes.empty()) {
            painter.save();
            painter.translate(pRect.topLeft());
            PdfCanvasTools::drawTextSelection(painter, m_activeTextBoxes, m_selectedIndices, scaleX, scaleY);
            painter.restore();
        }

        // Draw exact search match highlight (semi-transparent yellow)
        if (m_searchHighlightPage == i && !m_searchHighlightPointRect.isEmpty()) {
            QRect matchPix(pRect.left() + qRound(m_searchHighlightPointRect.x() * scaleX),
                           pRect.top() + qRound(m_searchHighlightPointRect.y() * scaleY),
                           std::max(4, qRound(m_searchHighlightPointRect.width() * scaleX)),
                           std::max(4, qRound(m_searchHighlightPointRect.height() * scaleY)));

            painter.fillRect(matchPix.adjusted(-2, -2, 2, 2), QColor(250, 204, 21, 160));
            painter.setPen(QPen(QColor(234, 179, 8), 1.5));
            painter.drawRect(matchPix.adjusted(-2, -2, 2, 2));
        }
    }
}

void PdfBaseCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_handToolActive || m_spacePressed) {
        if (event->button() == Qt::LeftButton) {
            m_isPanning = true;
            m_panStartPos = event->globalPosition().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        } else if (event->button() == Qt::RightButton && m_handToolActive) {
            setHandToolActive(false);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panStartPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        int clickedPage = pageIndexAtY(event->pos().y());
        if (clickedPage >= 0 && clickedPage < m_pageRects.size()) {
            QRect pRect = m_pageRects[clickedPage];
            if (pRect.contains(event->pos())) {
                m_isSelecting = true;
                m_selectStartPos = event->pos();
                loadTextBoxesForPage(clickedPage);
                m_selectedIndices.clear();
                m_selectedText.clear();
                emit selectionChanged(false);
                update();
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PdfBaseCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPoint delta = event->globalPosition().toPoint() - m_panStartPos;
        m_panStartPos = event->globalPosition().toPoint();
        if (m_scrollArea) {
            if (m_scrollArea->horizontalScrollBar()) {
                m_scrollArea->horizontalScrollBar()->setValue(
                    m_scrollArea->horizontalScrollBar()->value() - delta.x()
                );
            }
            if (m_scrollArea->verticalScrollBar()) {
                m_scrollArea->verticalScrollBar()->setValue(
                    m_scrollArea->verticalScrollBar()->value() - delta.y()
                );
            }
        }
        event->accept();
        return;
    }

    if (m_isSelecting && m_selectedPageIndex >= 0 && m_selectedPageIndex < m_pageRects.size()) {
        QRect pageR = m_pageRects[m_selectedPageIndex];
        QPoint localStart = m_selectStartPos - pageR.topLeft();
        QPoint localCurrent = event->pos() - pageR.topLeft();
        updateSelection(localStart, localCurrent);
    }
    QWidget::mouseMoveEvent(event);
}

void PdfBaseCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isPanning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_isPanning = false;
        setCursor((m_handToolActive || m_spacePressed) ? Qt::OpenHandCursor : Qt::IBeamCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void PdfBaseCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setHandToolActive(!m_handToolActive);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        int clickedPage = pageIndexAtY(event->pos().y());
        if (clickedPage >= 0 && clickedPage < m_pageRects.size()) {
            QRect pRect = m_pageRects[clickedPage];
            if (pRect.contains(event->pos())) {
                loadTextBoxesForPage(clickedPage);
                QSizeF pSize = getRotatedPageSize(clickedPage);
                double scaleX = (double)pRect.width() / std::max(1.0, pSize.width());
                double scaleY = (double)pRect.height() / std::max(1.0, pSize.height());
                QPoint localPt = event->pos() - pRect.topLeft();
                int idx = PdfCanvasTools::findClosestTextBox(localPt, m_activeTextBoxes, scaleX, scaleY);
                if (idx >= 0 && idx < static_cast<int>(m_activeTextBoxes.size())) {
                    m_selectedIndices = {idx};
                    m_selectedText = m_activeTextBoxes[idx]->text();
                    emit selectionChanged(true);
                    update();
                    event->accept();
                    return;
                }
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PdfBaseCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    } else if (event->key() == Qt::Key_H) {
        setHandToolActive(!m_handToolActive);
        event->accept();
        return;
    } else if (event->key() == Qt::Key_V || event->key() == Qt::Key_Escape) {
        setHandToolActive(false);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PdfBaseCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        if (!m_isPanning) {
            setCursor(m_handToolActive ? Qt::OpenHandCursor : Qt::IBeamCursor);
        }
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void PdfBaseCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int numDegrees = event->angleDelta().y();
        if (numDegrees != 0) {
            const double factor = (numDegrees > 0) ? 1.15 : (1.0 / 1.15);
            QPoint vpPos = (m_scrollArea && m_scrollArea->viewport())
                               ? m_scrollArea->viewport()->mapFromGlobal(event->globalPosition().toPoint())
                               : event->position().toPoint();
            emit zoomAtRequested(vpPos, factor);
            emit zoomWheelRequested(factor);
            event->accept();
            return;
        }
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        if (m_scrollArea && m_scrollArea->horizontalScrollBar()) {
            int step = m_scrollArea->horizontalScrollBar()->singleStep() * 3;
            if (step <= 0) step = 40;
            int delta = (event->angleDelta().y() != 0) ? event->angleDelta().y() : event->angleDelta().x();
            m_scrollArea->horizontalScrollBar()->setValue(
                m_scrollArea->horizontalScrollBar()->value() - (delta > 0 ? step : -step)
            );
            event->accept();
            return;
        }
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

    QAction *actHand = menu.addAction(tr("✋ Pan / Hand Tool (H)"));
    actHand->setCheckable(true);
    actHand->setChecked(m_handToolActive);
    connect(actHand, &QAction::triggered, this, [this]() {
        setHandToolActive(!m_handToolActive);
    });

    menu.addSeparator();
    QAction *actFitWidth = menu.addAction(tr("↔ Fit to Width"));
    connect(actFitWidth, &QAction::triggered, this, &PdfBaseCanvas::fitWidthRequested);

    QAction *actFitPage = menu.addAction(tr("🔍 Fit to Window / Page"));
    connect(actFitPage, &QAction::triggered, this, &PdfBaseCanvas::fitPageRequested);

    menu.addSeparator();
    QAction *actScan = menu.addAction(tr("🔍 Scansiona testo..."));
    connect(actScan, &QAction::triggered, this, &PdfBaseCanvas::scanTextRequested);

    QAction *actSaveAs = menu.addAction(tr("💾 Save As... (Ctrl+Shift+S)"));
    connect(actSaveAs, &QAction::triggered, this, &PdfBaseCanvas::saveAsRequested);

    QAction *actExport = menu.addAction(tr("↗ Convert / Export in Other Formats... (Ctrl+E)"));
    connect(actExport, &QAction::triggered, this, &PdfBaseCanvas::exportRequested);

    menu.addSeparator();
    QAction *actPrint = menu.addAction(tr("🖨️ Print Document... (Ctrl+P)"));
    connect(actPrint, &QAction::triggered, this, &PdfBaseCanvas::printRequested);

    menu.exec(event->globalPos());
}

void PdfBaseCanvas::layoutPages()
{
    m_pageRects.clear();
    m_pageScales.clear();
    if (!m_doc) {
        setFixedSize(0, 0);
        return;
    }

    const int numPages = m_doc->numPages();
    const int spacing = 16;
    int currentY = 16;
    int maxWidth = 0;

    const int targetViewportW = std::max(200, m_availWidth - 32);
    const int targetViewportH = std::max(200, m_availHeight - 32);

    for (int i = 0; i < numPages; ++i) {
        QSizeF pSize = getRotatedPageSize(i);
        double pageScale = m_zoomFactor;

        if (m_fitToPage) {
            double scaleW = (double)targetViewportW / std::max(10.0, pSize.width());
            double scaleH = (double)targetViewportH / std::max(10.0, pSize.height());
            pageScale = std::min(scaleW, scaleH);
        } else if (m_fitToWidth) {
            pageScale = (double)targetViewportW / std::max(10.0, pSize.width());
        }

        pageScale = std::clamp(pageScale, 0.15, 6.0);
        m_pageScales.append(pageScale);

        int w = std::max(100, (int)std::round(pSize.width() * pageScale));
        int h = std::max(100, (int)std::round(pSize.height() * pageScale));

        maxWidth = std::max(maxWidth, w);
        m_pageRects.append(QRect(0, currentY, w, h));
        currentY += h + spacing;
    }

    int canvasW = std::max(maxWidth + 32, m_availWidth);
    for (int i = 0; i < m_pageRects.size(); ++i) {
        int x = std::max(16, (canvasW - m_pageRects[i].width()) / 2);
        m_pageRects[i].moveLeft(x);
    }

    setFixedSize(canvasW, currentY + 32);
    if (m_editorOverlay) {
        m_editorOverlay->setGeometry(0, 0, canvasW, currentY + 32);
    }
}
