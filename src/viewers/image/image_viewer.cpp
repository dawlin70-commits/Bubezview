#include "image_viewer.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QImageReader>
#include <QMouseEvent>
#include <QTransform>

#include <cmath>

/**
 * @brief Inner canvas widget responsible for rendering the scaled pixmap,
 * supporting interactive click-and-drag panning, and on-canvas cropping.
 */
class ImageCanvasWidget : public QWidget {
public:
    explicit ImageCanvasWidget(ImageViewer *viewer, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_viewer(viewer)
        , m_isDragging(false)
        , m_cropActive(false)
        , m_isSelectingCrop(false)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setPixmap(const QPixmap &pixmap)
    {
        m_pixmap = pixmap;
        setFixedSize(m_pixmap.isNull() ? QSize(0, 0) : m_pixmap.size());
        if (m_cropActive) {
            m_cropRect = m_cropRect.intersected(rect());
        }
        update();
    }

    const QPixmap &pixmap() const { return m_pixmap; }

    void setCropActive(bool active)
    {
        if (m_cropActive != active) {
            m_cropActive = active;
            m_isSelectingCrop = false;
            m_cropRect = QRect();
            if (m_cropActive) {
                setCursor(Qt::CrossCursor);
                setFocus();
            } else {
                unsetCursor();
            }
            update();
        }
    }

    bool isCropActive() const { return m_cropActive; }
    QRect cropRect() const { return m_cropRect; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_pixmap.isNull()) return;
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_pixmap);

        if (m_cropActive) {
            if (m_cropRect.isValid() && !m_cropRect.isEmpty()) {
                QColor dimColor(0, 0, 0, 140);
                // Top
                if (m_cropRect.top() > 0) {
                    painter.fillRect(0, 0, width(), m_cropRect.top(), dimColor);
                }
                // Bottom
                if (m_cropRect.bottom() < height() - 1) {
                    painter.fillRect(0, m_cropRect.bottom() + 1, width(), height() - m_cropRect.bottom() - 1, dimColor);
                }
                // Left
                if (m_cropRect.left() > 0) {
                    painter.fillRect(0, m_cropRect.top(), m_cropRect.left(), m_cropRect.height(), dimColor);
                }
                // Right
                if (m_cropRect.right() < width() - 1) {
                    painter.fillRect(m_cropRect.right() + 1, m_cropRect.top(), width() - m_cropRect.right() - 1, m_cropRect.height(), dimColor);
                }

                // White dashed selection border
                QPen pen(Qt::white, 1, Qt::DashLine);
                painter.setPen(pen);
                painter.drawRect(m_cropRect);

                // 4 Corner handles
                painter.setBrush(Qt::white);
                painter.setPen(QPen(Qt::black, 1));
                const int hsz = 6;
                painter.drawRect(m_cropRect.left() - hsz / 2, m_cropRect.top() - hsz / 2, hsz, hsz);
                painter.drawRect(m_cropRect.right() - hsz / 2, m_cropRect.top() - hsz / 2, hsz, hsz);
                painter.drawRect(m_cropRect.left() - hsz / 2, m_cropRect.bottom() - hsz / 2, hsz, hsz);
                painter.drawRect(m_cropRect.right() - hsz / 2, m_cropRect.bottom() - hsz / 2, hsz, hsz);

                // Resolution badge
                if (m_viewer && !m_viewer->originalImage().isNull() && width() > 0 && height() > 0) {
                    double sx = double(m_viewer->originalImage().width()) / double(width());
                    double sy = double(m_viewer->originalImage().height()) / double(height());
                    int origW = qMax(1, qRound(m_cropRect.width() * sx));
                    int origH = qMax(1, qRound(m_cropRect.height() * sy));
                    QString badgeText = QString("%1 × %2 px (Enter = Crop, Esc = Cancel)").arg(origW).arg(origH);

                    QFont f = painter.font();
                    f.setPointSize(9);
                    f.setBold(true);
                    painter.setFont(f);
                    QFontMetrics fm(f);
                    int tw = fm.horizontalAdvance(badgeText) + 16;
                    int th = fm.height() + 8;
                    int bx = m_cropRect.center().x() - tw / 2;
                    int by = m_cropRect.bottom() + 10;
                    if (by + th > height()) by = m_cropRect.top() - th - 10;
                    if (by < 0) by = m_cropRect.top() + 10;
                    bx = qBound(5, bx, qMax(5, width() - tw - 5));

                    QRect badgeRect(bx, by, tw, th);
                    painter.setBrush(QColor(15, 23, 42, 220));
                    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
                    painter.drawRoundedRect(badgeRect, 5, 5);
                    painter.setPen(Qt::white);
                    painter.drawText(badgeRect, Qt::AlignCenter, badgeText);
                }
            } else {
                // Dim entire canvas lightly when crop mode is waiting for first drag
                painter.fillRect(rect(), QColor(0, 0, 0, 75));
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_cropActive) {
            if (event->button() == Qt::LeftButton) {
                m_isSelectingCrop = true;
                m_cropStartPos = event->pos();
                m_cropRect = QRect(m_cropStartPos, m_cropStartPos);
                update();
                event->accept();
                return;
            } else if (event->button() == Qt::RightButton) {
                if (m_viewer) m_viewer->cancelCropMode();
                event->accept();
                return;
            }
        }

        if (m_viewer && m_viewer->isZoomToolActive()) {
            QPoint vpPos = (m_viewer->scrollArea() && m_viewer->scrollArea()->viewport())
                               ? m_viewer->scrollArea()->viewport()->mapFromGlobal(event->globalPosition().toPoint())
                               : event->pos();
            if (event->button() == Qt::LeftButton) {
                m_viewer->zoomAt(vpPos, 1.25);
                event->accept();
                return;
            } else if (event->button() == Qt::RightButton) {
                m_viewer->zoomAt(vpPos, 1.0 / 1.25);
                event->accept();
                return;
            }
        }

        if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
            m_isDragging = true;
            m_lastGlobalPos = event->globalPosition().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
        } else {
            QWidget::mousePressEvent(event);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_cropActive && m_isSelectingCrop) {
            m_cropRect = QRect(m_cropStartPos, event->pos()).normalized();
            m_cropRect = m_cropRect.intersected(rect());
            update();
            event->accept();
            return;
        }

        if (m_isDragging) {
            QPoint currentGlobalPos = event->globalPosition().toPoint();
            QPoint delta = currentGlobalPos - m_lastGlobalPos;
            m_lastGlobalPos = currentGlobalPos;

            // Find enclosing QScrollArea to adjust scrollbars
            QScrollArea *sa = nullptr;
            QWidget *p = parentWidget();
            while (p) {
                if ((sa = qobject_cast<QScrollArea *>(p))) break;
                p = p->parentWidget();
            }
            if (sa) {
                if (sa->horizontalScrollBar()) {
                    sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - delta.x());
                }
                if (sa->verticalScrollBar()) {
                    sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - delta.y());
                }
            }
            event->accept();
        } else {
            QWidget::mouseMoveEvent(event);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_cropActive && m_isSelectingCrop && event->button() == Qt::LeftButton) {
            m_isSelectingCrop = false;
            m_cropRect = QRect(m_cropStartPos, event->pos()).normalized();
            m_cropRect = m_cropRect.intersected(rect());
            update();
            event->accept();
            return;
        }

        if (m_isDragging && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
            m_isDragging = false;
            if (m_cropActive) {
                setCursor(Qt::CrossCursor);
            } else if (m_viewer && m_viewer->isZoomToolActive()) {
                QPixmap zoomPix = QIcon::fromTheme("zoom-in").pixmap(24, 24);
                setCursor(zoomPix.isNull() ? Qt::CrossCursor : QCursor(zoomPix, 8, 8));
            } else {
                unsetCursor();
            }
            event->accept();
        } else {
            QWidget::mouseReleaseEvent(event);
        }
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (m_cropActive && !m_cropRect.isEmpty() && m_cropRect.contains(event->pos())) {
            if (m_viewer) m_viewer->confirmCrop();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_cropActive) {
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                if (m_viewer) m_viewer->confirmCrop();
                event->accept();
                return;
            } else if (event->key() == Qt::Key_Escape) {
                if (m_viewer) m_viewer->cancelCropMode();
                event->accept();
                return;
            }
        }
        QWidget::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (m_viewer) {
            m_viewer->handleWheel(event);
        } else {
            event->ignore();
        }
    }

private:
    ImageViewer *m_viewer;
    QPixmap m_pixmap;
    bool m_isDragging;
    QPoint m_lastGlobalPos;
    bool m_cropActive;
    bool m_isSelectingCrop;
    QPoint m_cropStartPos;
    QRect m_cropRect;
};

ImageViewer::ImageViewer(QWidget *parent)
    : QWidget(parent)
    , m_scrollArea(nullptr)
    , m_canvas(nullptr)
    , m_zoomFactor(1.0)
    , m_fitToWindow(true)
    , m_zoomToolActive(false)
    , m_isModified(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_canvas = new ImageCanvasWidget(this, m_scrollArea);
    m_scrollArea->setWidget(m_canvas);
    if (m_scrollArea->viewport()) {
        m_scrollArea->viewport()->installEventFilter(this);
    }

    mainLayout->addWidget(m_scrollArea);

    updateTheme();
}

ImageViewer::~ImageViewer() = default;

void ImageViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();
    QString canvasBg;
    if (preset == ThemePreset::SystemNative) {
        canvasBg = "#e4e7eb";
    } else if (preset == ThemePreset::DarkSlate) {
        canvasBg = "#22262b";
    } else {
        canvasBg = "#181b1d";
    }

    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(
            "QScrollArea { background-color: " + canvasBg + "; border: none; }\n" +
            ThemeManager::scrollBarStyleSheet(preset)
        );
        if (m_scrollArea->viewport()) {
            m_scrollArea->viewport()->setStyleSheet("background-color: " + canvasBg + "; border: none;");
        }
    }
}

void ImageViewer::setModified(bool mod)
{
    if (m_isModified != mod) {
        m_isModified = mod;
        emit modificationChanged(m_isModified);
    }
}

bool ImageViewer::loadFile(const QString &filePath)
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    m_originalImage = reader.read();

    if (m_originalImage.isNull()) {
        return false;
    }

    clearUndoRedo();
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    setModified(false);
    updateScaledPixmap();
    return true;
}

bool ImageViewer::loadImage(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }

    clearUndoRedo();
    m_originalImage = image;
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    setModified(false);
    updateScaledPixmap();
    return true;
}

void ImageViewer::clear()
{
    clearUndoRedo();
    m_originalImage = QImage();
    m_scaledPixmap = QPixmap();
    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap);
    }
    setModified(false);
}

void ImageViewer::resetZoom()
{
    m_fitToWindow = false;
    m_zoomFactor = 1.0;
    updateScaledPixmap();
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::fitToWindow()
{
    m_fitToWindow = true;
    updateScaledPixmap();
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::setZoomToolActive(bool active)
{
    m_zoomToolActive = active;
    if (m_canvas) {
        if (m_zoomToolActive) {
            QPixmap zoomPix = QIcon::fromTheme("zoom-in").pixmap(24, 24);
            m_canvas->setCursor(zoomPix.isNull() ? Qt::CrossCursor : QCursor(zoomPix, 8, 8));
        } else {
            m_canvas->unsetCursor();
        }
    }
}

void ImageViewer::zoomIn()
{
    QPoint center = (m_scrollArea && m_scrollArea->viewport())
                        ? QPoint(m_scrollArea->viewport()->width() / 2, m_scrollArea->viewport()->height() / 2)
                        : QPoint(width() / 2, height() / 2);
    zoomAt(center, 1.25);
}

void ImageViewer::zoomOut()
{
    QPoint center = (m_scrollArea && m_scrollArea->viewport())
                        ? QPoint(m_scrollArea->viewport()->width() / 2, m_scrollArea->viewport()->height() / 2)
                        : QPoint(width() / 2, height() / 2);
    zoomAt(center, 1.0 / 1.25);
}

void ImageViewer::zoomAt(const QPoint &viewportPos, double factor)
{
    if (m_originalImage.isNull() || !m_canvas || !m_scrollArea) return;

    // 1. Calculate relative coordinate within current canvas
    QPoint widgetPos = m_canvas->mapFrom(m_scrollArea->viewport(), viewportPos);
    double relX = 0.5;
    double relY = 0.5;
    if (m_canvas->width() > 0 && m_canvas->height() > 0) {
        relX = qBound(0.0, static_cast<double>(widgetPos.x()) / m_canvas->width(), 1.0);
        relY = qBound(0.0, static_cast<double>(widgetPos.y()) / m_canvas->height(), 1.0);
    }

    // 2. Adjust zoom factor
    if (m_fitToWindow) {
        m_fitToWindow = false;
        if (!m_canvas->pixmap().isNull() && m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(m_canvas->pixmap().width()) / static_cast<double>(m_originalImage.width());
        }
    }

    m_zoomFactor *= factor;
    m_zoomFactor = qBound(0.05, m_zoomFactor, 25.0);
    updateScaledPixmap();

    // 3. Keep target coordinate pinned under cursor
    int newX = qRound(relX * m_canvas->width()) - viewportPos.x();
    int newY = qRound(relY * m_canvas->height()) - viewportPos.y();
    if (m_scrollArea->horizontalScrollBar()) {
        m_scrollArea->horizontalScrollBar()->setValue(newX);
    }
    if (m_scrollArea->verticalScrollBar()) {
        m_scrollArea->verticalScrollBar()->setValue(newY);
    }

    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::rotateClockwise()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(90);
    m_originalImage = m_originalImage.transformed(t);
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::rotateCounterClockwise()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(-90);
    m_originalImage = m_originalImage.transformed(t);
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::flipHorizontal()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(true, false);
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::flipVertical()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(false, true);
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::invertColors()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage.invertPixels();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::updateScaledPixmap()
{
    if (m_originalImage.isNull()) {
        m_scaledPixmap = QPixmap();
        if (m_canvas) m_canvas->setPixmap(m_scaledPixmap);
        return;
    }

    QSize viewportSize;
    if (m_scrollArea && m_scrollArea->viewport() &&
        m_scrollArea->viewport()->width() > 50 && m_scrollArea->viewport()->height() > 50) {
        viewportSize = m_scrollArea->viewport()->size();
    } else if (width() > 50 && height() > 50) {
        viewportSize = size();
    } else {
        viewportSize = QSize(800, 600);
    }

    QSize targetSize;
    if (m_fitToWindow) {
        targetSize = m_originalImage.size().scaled(viewportSize, Qt::KeepAspectRatio);
        if (m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(targetSize.width()) / static_cast<double>(m_originalImage.width());
        }
    } else {
        targetSize = m_originalImage.size() * m_zoomFactor;
    }

    if (targetSize.width() < 1) targetSize.setWidth(1);
    if (targetSize.height() < 1) targetSize.setHeight(1);

    // Guard against memory exhaustion / X11 BadAlloc crashes on extreme zoom
    constexpr int MAX_SAFE_DIM = 16384;
    if (targetSize.width() > MAX_SAFE_DIM || targetSize.height() > MAX_SAFE_DIM) {
        targetSize.scale(MAX_SAFE_DIM, MAX_SAFE_DIM, Qt::KeepAspectRatio);
    }

    m_scaledPixmap = QPixmap::fromImage(
        m_originalImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );

    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap);
    }
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (layout()) {
        layout()->activate();
    }
    if (m_fitToWindow) {
        updateScaledPixmap();
    }
}

void ImageViewer::handleWheel(QWheelEvent *event)
{
    // 1. Zoom with Mouse Wheel: when Zoom Tool is active or Ctrl key is pressed
    if (m_zoomToolActive || (event->modifiers() & Qt::ControlModifier)) {
        int deltaY = event->angleDelta().y();
        if (deltaY != 0) {
            double factor = (deltaY > 0) ? 1.25 : (1.0 / 1.25);
            QPoint vpPos = (m_scrollArea && m_scrollArea->viewport())
                               ? m_scrollArea->viewport()->mapFromGlobal(event->globalPosition().toPoint())
                               : QPoint(width() / 2, height() / 2);
            zoomAt(vpPos, factor);
            event->accept();
            return;
        }
        return;
    }

    // 2. Horizontal Scrolling: Shift + Wheel or horizontal trackpad/wheel tilt
    if ((event->modifiers() & Qt::ShiftModifier) || event->angleDelta().x() != 0) {
        int delta = (event->modifiers() & Qt::ShiftModifier) ? event->angleDelta().y() : event->angleDelta().x();
        QScrollBar *hbar = m_scrollArea ? m_scrollArea->horizontalScrollBar() : nullptr;
        if (hbar && hbar->maximum() > hbar->minimum()) {
            int step = (event->pixelDelta().x() != 0) ? event->pixelDelta().x() : qRound(delta * 0.75);
            if (step == 0 && delta != 0) step = (delta > 0 ? 1 : -1);
            hbar->setValue(hbar->value() - step);
            event->accept();
            return;
        }
    }

    // 3. Normal Vertical Scrolling & Edge/Fitted Sibling File Navigation
    int deltaY = event->angleDelta().y();
    if (deltaY != 0) {
        QScrollBar *vbar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
        bool canScrollV = (vbar && vbar->maximum() > vbar->minimum());
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (!canScrollV) {
            // Whole image fits vertically: navigate sibling files on mouse wheel
            if (now - m_lastNavTime < 250) {
                event->accept();
                return;
            }
            m_wheelNavAccumulator += deltaY;
            if (m_wheelNavAccumulator <= -120) {
                m_wheelNavAccumulator = 0;
                m_lastNavTime = now;
                emit nextFileRequested();
            } else if (m_wheelNavAccumulator >= 120) {
                m_wheelNavAccumulator = 0;
                m_lastNavTime = now;
                emit prevFileRequested();
            }
            event->accept();
            return;
        } else {
            // Content is scrollable
            if (deltaY < 0 && vbar->value() >= vbar->maximum()) {
                // At bottom edge: deliberate overscroll navigates to next file
                if (now - m_lastNavTime < 250) {
                    event->accept();
                    return;
                }
                m_wheelNavAccumulator += deltaY;
                if (m_wheelNavAccumulator <= -180) {
                    m_wheelNavAccumulator = 0;
                    m_lastNavTime = now;
                    emit nextFileRequested();
                }
                event->accept();
                return;
            } else if (deltaY > 0 && vbar->value() <= vbar->minimum()) {
                // At top edge: deliberate overscroll navigates to previous file
                if (now - m_lastNavTime < 250) {
                    event->accept();
                    return;
                }
                m_wheelNavAccumulator += deltaY;
                if (m_wheelNavAccumulator >= 180) {
                    m_wheelNavAccumulator = 0;
                    m_lastNavTime = now;
                    emit prevFileRequested();
                }
                event->accept();
                return;
            } else {
                m_wheelNavAccumulator = 0;
                int step = (event->pixelDelta().y() != 0) ? event->pixelDelta().y() : qRound(deltaY * 0.75);
                if (step == 0 && deltaY != 0) step = (deltaY > 0 ? 1 : -1);
                vbar->setValue(vbar->value() - step);
                event->accept();
                return;
            }
        }
    }

    event->ignore();
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    handleWheel(event);
    if (!event->isAccepted()) {
        QWidget::wheelEvent(event);
    }
}

bool ImageViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (m_scrollArea && watched == m_scrollArea->viewport() && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent*>(event);
        handleWheel(we);
        return we->isAccepted();
    }
    return QWidget::eventFilter(watched, event);
}

void ImageViewer::clearUndoRedo()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoAvailabilityChanged(false, false);
}

void ImageViewer::pushUndoState()
{
    if (m_originalImage.isNull()) return;
    if (m_undoStack.size() >= 20) {
        m_undoStack.removeFirst();
    }
    m_undoStack.append(m_originalImage);
    m_redoStack.clear();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::undo()
{
    if (m_undoStack.isEmpty()) return;
    m_redoStack.append(m_originalImage);
    m_originalImage = m_undoStack.takeLast();
    setModified(true);
    updateScaledPixmap();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::redo()
{
    if (m_redoStack.isEmpty()) return;
    m_undoStack.append(m_originalImage);
    m_originalImage = m_redoStack.takeLast();
    setModified(true);
    updateScaledPixmap();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}


void ImageViewer::applyCrop(const QRect &cropRect)
{
    if (m_originalImage.isNull()) return;
    QRect validRect = cropRect.intersected(m_originalImage.rect());
    if (validRect.width() < 1 || validRect.height() < 1) return;

    pushUndoState();
    m_originalImage = m_originalImage.copy(validRect);
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::setCropModeActive(bool active)
{
    if (m_canvas) {
        m_canvas->setCropActive(active);
    }
    emit cropModeChanged(active);
}

bool ImageViewer::isCropModeActive() const
{
    return m_canvas && m_canvas->isCropActive();
}

void ImageViewer::confirmCrop()
{
    if (!m_canvas || !m_canvas->isCropActive() || m_canvas->cropRect().isEmpty()) {
        setCropModeActive(false);
        return;
    }
    QRect cr = m_canvas->cropRect();
    if (m_canvas->width() <= 0 || m_canvas->height() <= 0 || m_originalImage.isNull()) {
        setCropModeActive(false);
        return;
    }
    double sx = double(m_originalImage.width()) / double(m_canvas->width());
    double sy = double(m_originalImage.height()) / double(m_canvas->height());
    int ix = qBound(0, qRound(cr.x() * sx), m_originalImage.width() - 1);
    int iy = qBound(0, qRound(cr.y() * sy), m_originalImage.height() - 1);
    int iw = qBound(1, qRound(cr.width() * sx), m_originalImage.width() - ix);
    int ih = qBound(1, qRound(cr.height() * sy), m_originalImage.height() - iy);

    setCropModeActive(false);
    applyCrop(QRect(ix, iy, iw, ih));
}

void ImageViewer::cancelCropMode()
{
    setCropModeActive(false);
}

