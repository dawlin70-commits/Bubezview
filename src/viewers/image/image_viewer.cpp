#include "image_viewer.h"
#include "../../core/theme_manager.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QImageReader>
#include <QMouseEvent>
#include <QTransform>

/**
 * @brief Inner canvas widget responsible for rendering the scaled pixmap
 * and supporting interactive click-and-drag panning.
 */
class ImageCanvasWidget : public QWidget {
public:
    explicit ImageCanvasWidget(ImageViewer *viewer, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_viewer(viewer)
        , m_isDragging(false)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    void setPixmap(const QPixmap &pixmap)
    {
        m_pixmap = pixmap;
        setFixedSize(m_pixmap.isNull() ? QSize(0, 0) : m_pixmap.size());
        update();
    }

    const QPixmap &pixmap() const { return m_pixmap; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_pixmap.isNull()) return;
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_pixmap);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
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
        if (m_isDragging && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
            m_isDragging = false;
            if (m_viewer && m_viewer->isZoomToolActive()) {
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

bool ImageViewer::loadFile(const QString &filePath)
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    m_originalImage = reader.read();

    if (m_originalImage.isNull()) {
        return false;
    }

    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    m_isModified = false;
    updateScaledPixmap();
    return true;
}

bool ImageViewer::loadImage(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }

    m_originalImage = image;
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    m_isModified = false;
    updateScaledPixmap();
    return true;
}

void ImageViewer::clear()
{
    m_originalImage = QImage();
    m_scaledPixmap = QPixmap();
    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap);
    }
    m_isModified = false;
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
    QTransform t;
    t.rotate(90);
    m_originalImage = m_originalImage.transformed(t);
    m_isModified = true;
    updateScaledPixmap();
}

void ImageViewer::rotateCounterClockwise()
{
    if (m_originalImage.isNull()) return;
    QTransform t;
    t.rotate(-90);
    m_originalImage = m_originalImage.transformed(t);
    m_isModified = true;
    updateScaledPixmap();
}

void ImageViewer::flipHorizontal()
{
    if (m_originalImage.isNull()) return;
    m_originalImage = m_originalImage.mirrored(true, false);
    m_isModified = true;
    updateScaledPixmap();
}

void ImageViewer::flipVertical()
{
    if (m_originalImage.isNull()) return;
    m_originalImage = m_originalImage.mirrored(false, true);
    m_isModified = true;
    updateScaledPixmap();
}

void ImageViewer::invertColors()
{
    if (m_originalImage.isNull()) return;
    m_originalImage.invertPixels();
    m_isModified = true;
    updateScaledPixmap();
}

void ImageViewer::updateScaledPixmap()
{
    if (m_originalImage.isNull()) {
        m_scaledPixmap = QPixmap();
        if (m_canvas) m_canvas->setPixmap(m_scaledPixmap);
        return;
    }

    QSize viewportSize = (m_scrollArea && m_scrollArea->viewport()->width() > 10 && m_scrollArea->viewport()->height() > 10)
                             ? m_scrollArea->viewport()->size()
                             : (size().width() > 10 && size().height() > 10 ? size() : QSize(800, 600));

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
