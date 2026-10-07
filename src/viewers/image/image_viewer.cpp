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
            unsetCursor();
            event->accept();
        } else {
            QWidget::mouseReleaseEvent(event);
        }
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            if (m_viewer) {
                m_viewer->handleWheel(event);
            }
        } else {
            // Forward unhandled wheel scroll to enclosing scroll area
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
}

void ImageViewer::fitToWindow()
{
    m_fitToWindow = true;
    updateScaledPixmap();
}

void ImageViewer::setZoomToolActive(bool active)
{
    m_zoomToolActive = active;
}

void ImageViewer::zoomIn()
{
    if (m_fitToWindow) {
        m_fitToWindow = false;
        if (!m_originalImage.isNull() && m_canvas && !m_canvas->pixmap().isNull() && m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(m_canvas->pixmap().width()) / static_cast<double>(m_originalImage.width());
        }
    }
    m_zoomFactor *= 1.25;
    if (m_zoomFactor > 10.0) m_zoomFactor = 10.0;
    updateScaledPixmap();
}

void ImageViewer::zoomOut()
{
    if (m_fitToWindow) {
        m_fitToWindow = false;
        if (!m_originalImage.isNull() && m_canvas && !m_canvas->pixmap().isNull() && m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(m_canvas->pixmap().width()) / static_cast<double>(m_originalImage.width());
        }
    }
    m_zoomFactor /= 1.25;
    if (m_zoomFactor < 0.1) m_zoomFactor = 0.1;
    updateScaledPixmap();
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
    if (m_zoomToolActive || (event->modifiers() & Qt::ControlModifier)) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else if (event->angleDelta().y() < 0) {
            zoomOut();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    handleWheel(event);
    if (!event->isAccepted()) {
        QWidget::wheelEvent(event);
    }
}
