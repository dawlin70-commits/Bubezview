#include "image_viewer.h"
#include "core/interfaces/i_image_studio_plugin.h"
#include "theme_manager.h"
#include "gpu_acceleration_manager.h"
#include "core/services/image_processor.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QMouseEvent>
#include <QTransform>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>

/**
 * @brief Inner canvas widget responsible for rendering the image pixmap,
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
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::OpenHandCursor);
    }

    void setPixmap(const QPixmap &pixmap, const QSize &targetSize = QSize())
    {
        m_pixmap = pixmap;
        setFixedSize(targetSize.isValid() ? targetSize : (m_pixmap.isNull() ? QSize(0, 0) : m_pixmap.size()));
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
                setCursor(Qt::OpenHandCursor);
            }
            update();
        }
    }

    bool isCropActive() const { return m_cropActive; }
    QRect cropRect() const { return m_cropRect; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QColor stageBg = m_viewer ? m_viewer->canvasBackgroundColor() : palette().window().color();
        painter.fillRect(rect(), stageBg);
        if (m_pixmap.isNull()) {
            return;
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const qreal cornerRadius = 12.0;
        painter.save();
        QPainterPath clipPath;
        clipPath.addRoundedRect(QRectF(rect()), cornerRadius, cornerRadius);
        painter.setClipPath(clipPath);

        // 1. Fill background based on transparency mode if pixmap contains an alpha channel
        if (m_pixmap.hasAlphaChannel()) {
            ImageViewer::TransparencyMode mode = m_viewer ? m_viewer->transparencyMode() : ImageViewer::TransparencyMode::Checkerboard;
            switch (mode) {
            case ImageViewer::TransparencyMode::White:
                painter.fillRect(rect(), Qt::white);
                break;
            case ImageViewer::TransparencyMode::Checkerboard: {
                static QPixmap s_checkerTile;
                if (s_checkerTile.isNull()) {
                    QImage tile(16, 16, QImage::Format_RGB32);
                    tile.fill(QColor(240, 240, 240));
                    QPainter pt(&tile);
                    pt.fillRect(0, 0, 8, 8, QColor(205, 205, 205));
                    pt.fillRect(8, 8, 8, 8, QColor(205, 205, 205));
                    pt.end();
                    s_checkerTile = QPixmap::fromImage(tile);
                }
                painter.drawTiledPixmap(rect(), s_checkerTile);
                break;
            }
            case ImageViewer::TransparencyMode::Theme:
                painter.fillRect(rect(), stageBg);
                break;
            case ImageViewer::TransparencyMode::Black:
                painter.fillRect(rect(), Qt::black);
                break;
            }
        } else {
            painter.fillRect(rect(), stageBg);
        }

        // 2. Draw actual pixmap
        painter.drawPixmap(rect(), m_pixmap);
        painter.restore();

        // 3. Subtle 1px rounded framing border
        bool isDark = m_viewer ? m_viewer->isDarkTheme() : false;
        painter.setPen(QPen(isDark ? QColor(255, 255, 255, 32) : QColor(15, 23, 42, 32), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), cornerRadius, cornerRadius);

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
                setCursor(Qt::OpenHandCursor);
            }
            event->accept();
        } else {
            QWidget::mouseReleaseEvent(event);
        }
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        if (m_cropActive || (m_viewer && m_viewer->isZoomToolActive())) {
            event->ignore();
            return;
        }
        if (m_viewer) {
            emit m_viewer->contextMenuRequested(event->globalPos());
            event->accept();
            return;
        }
        QWidget::contextMenuEvent(event);
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
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    if (m_scrollArea->viewport()) {
        m_scrollArea->viewport()->setCursor(Qt::OpenHandCursor);
    }

    // Create and set canvas widget into scroll area
    m_canvas = new ImageCanvasWidget(this, m_scrollArea);
    m_scrollArea->setWidget(m_canvas);

    if (m_scrollArea->viewport()) {
        m_scrollArea->viewport()->installEventFilter(this);
    }

    mainLayout->addWidget(m_scrollArea, 1);

    m_smoothResizeTimer = new QTimer(this);
    m_smoothResizeTimer->setSingleShot(true);
    connect(m_smoothResizeTimer, &QTimer::timeout, this, [this]() {
        if (m_fitToWindow) {
            updateScaledPixmap();
        }
    });

    updateTheme();
}

ImageViewer::~ImageViewer()
{
    clear();
}

QColor ImageViewer::canvasBackgroundColor() const
{
    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    switch (preset) {
    case ThemePreset::System:
    case ThemePreset::DarkSlate:   return QColor("#151a28");
    case ThemePreset::ZincGray:    return QColor("#36373b");
    case ThemePreset::BreezeDark:  return QColor("#131824");
    case ThemePreset::ModernLight: return QColor("#e2e5ea");
    case ThemePreset::BreezeLight: return QColor("#eaeff5");
    }
    return QColor("#151a28");
}

bool ImageViewer::isDarkTheme() const
{
    return ThemeManager::isDark(ThemeManager::currentPreset());
}

ImageViewer::TransparencyMode ImageViewer::transparencyMode() const
{
    // Fixed default (Checkerboard). The former "image/transparency_mode" setting
    // is intentionally ignored so stale saved values cannot override it.
    return m_transparencyMode;
}

void ImageViewer::setTransparencyMode(TransparencyMode mode)
{
    m_transparencyMode = mode;
    if (m_canvas) {
        m_canvas->update();
    }
}

void ImageViewer::removeTransparency(const QColor &fillColor)
{
    if (m_originalImage.isNull() || !m_originalImage.hasAlphaChannel()) {
        return;
    }
    pushUndoState();
    QImage opaqueImg(m_originalImage.size(), QImage::Format_RGB32);
    opaqueImg.fill(fillColor);
    QPainter p(&opaqueImg);
    p.drawImage(0, 0, m_originalImage);
    p.end();

    m_originalImage = opaqueImg;
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) {
        m_studioWidget->setSourceImage(m_originalImage);
    }
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    bool isDark = ThemeManager::isDark(preset);
    QString canvasGrad;
    if (preset == ThemePreset::ZincGray) {
        canvasGrad = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5a5b61, stop:0.5 #505156, stop:1 #444549)");
    } else if (isDark) {
        canvasGrad = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e2538, stop:0.5 #151a28, stop:1 #0e121c)");
    } else {
        canvasGrad = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f8fafc, stop:0.5 #eef2f6, stop:1 #e2e8f0)");
    }

    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(
            "QScrollArea { background: " + canvasGrad + "; border: none; }\n" +
            ThemeManager::scrollBarStyleSheet(preset)
        );
        if (m_scrollArea->viewport()) {
            QPalette pal = m_scrollArea->viewport()->palette();
            pal.setColor(QPalette::Window, canvasBackgroundColor());
            m_scrollArea->viewport()->setPalette(pal);
            m_scrollArea->viewport()->setStyleSheet("background: " + canvasGrad + "; border: none;");
            m_scrollArea->viewport()->setAutoFillBackground(true);
            m_scrollArea->viewport()->update();
        }
    }
    if (m_canvas) {
        m_canvas->update();
    }
    if (m_studioWidget) {
        m_studioWidget->updateTheme();
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
    if (QImageReader::allocationLimit() > 0 && QImageReader::allocationLimit() < 512) {
        QImageReader::setAllocationLimit(512);
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    m_originalImage = reader.read();

    if (m_originalImage.isNull()) {
        return false;
    }

    clearUndoRedo();
    m_scaledPixmap = QPixmap();
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    setModified(false);
    updateScaledPixmap();
    if (m_studioWidget) {
        m_studioWidget->setSourceImage(m_originalImage);
    }
    return true;
}

bool ImageViewer::loadImage(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }

    clearUndoRedo();
    m_originalImage = image;
    m_scaledPixmap = QPixmap();
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    setModified(false);
    updateScaledPixmap();
    if (m_studioWidget) {
        m_studioWidget->setSourceImage(m_originalImage);
    }
    return true;
}

void ImageViewer::clear()
{
    clearUndoRedo();
    m_originalImage = QImage();
    m_scaledPixmap = QPixmap();
    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap, QSize(0, 0));
    }
    if (m_studioWidget) {
        // Release the cached QImage inside the studio panel to avoid retaining memory
        m_studioWidget->setSourceImage(QImage());
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
            m_canvas->setCursor(Qt::OpenHandCursor);
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

void ImageViewer::setZoomFactor(double factor)
{
    if (m_originalImage.isNull() || !m_canvas || !m_scrollArea) return;
    m_fitToWindow = false;
    m_zoomFactor = qBound(0.05, factor, 25.0);
    updateScaledPixmap();
    emit zoomChanged(m_zoomFactor);
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
        if (m_canvas->width() > 0 && m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(m_canvas->width()) / static_cast<double>(m_originalImage.width());
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

void ImageViewer::attachStudioWidget(IImageStudioWidget *studioWidget)
{
    if (!studioWidget || m_studioWidget == studioWidget) return;

    // Cleanly detach and dispose any previously attached studio panel before replacing it
    if (m_studioWidget) {
        if (auto *hLay = qobject_cast<QHBoxLayout*>(layout())) {
            hLay->removeWidget(m_studioWidget);
        }
        m_studioWidget->hide();
        m_studioWidget->deleteLater();
        m_studioWidget = nullptr;
    }

    m_studioWidget = studioWidget;
    m_studioWidget->setParent(this);
    m_studioWidget->setOnApplyImage([this](const QImage &img) {
        applyModifiedImage(img);
    });
    m_studioWidget->setOnRequestCrop([this]() {
        setCropModeActive(!isCropModeActive());
    });
    m_studioWidget->setOnRequestRotate([this](bool cw) {
        if (cw) rotateClockwise();
        else rotateCounterClockwise();
    });
    m_studioWidget->setOnRequestFlip([this](bool horizontal) {
        if (horizontal) flipHorizontal();
        else flipVertical();
    });
    m_studioWidget->setOnRequestRemoveTransparency([this]() {
        removeTransparency(Qt::white);
    });
    if (auto *hLay = qobject_cast<QHBoxLayout*>(layout())) {
        hLay->addWidget(m_studioWidget);
    }
    m_studioWidget->updateTheme();
    m_studioWidget->setSourceImage(m_originalImage);
    m_studioWidget->setVisible(m_isEditMode);
}

void ImageViewer::detachStudioWidget()
{
    if (!m_studioWidget) return;
    if (auto *hLay = qobject_cast<QHBoxLayout*>(layout())) {
        hLay->removeWidget(m_studioWidget);
    }
    m_studioWidget->hide();
    // Synchronous delete is required so all plugin vtables are destroyed before dlclose()
    delete m_studioWidget;
    m_studioWidget = nullptr;
}

void ImageViewer::setEditMode(bool editMode)
{
    m_isEditMode = editMode;
    if (m_studioWidget) {
        if (editMode) {
            m_studioWidget->setSourceImage(m_originalImage);
            m_studioWidget->updateTheme();
        }
        m_studioWidget->setVisible(editMode);
    }
    if (!editMode) {
        cancelCropMode();
        if (m_canvas) {
            m_canvas->setCursor(Qt::OpenHandCursor);
        }
        if (m_scrollArea && m_scrollArea->viewport()) {
            m_scrollArea->viewport()->setCursor(Qt::OpenHandCursor);
        }
    }
}

void ImageViewer::rotateClockwise()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(90);
    m_originalImage = m_originalImage.transformed(t);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::rotateCounterClockwise()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(-90);
    m_originalImage = m_originalImage.transformed(t);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::flipHorizontal()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(true, false);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::flipVertical()
{
    if (m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(false, true);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::invertColors()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage.invertPixels();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::updateScaledPixmap()
{
    if (m_originalImage.isNull()) {
        m_scaledPixmap = QPixmap();
        if (m_canvas) m_canvas->setPixmap(m_scaledPixmap, QSize(0, 0));
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
        if (!m_fitToWindow && m_originalImage.width() > 0) {
            m_zoomFactor = static_cast<double>(targetSize.width()) / static_cast<double>(m_originalImage.width());
        }
    }

    if (m_scaledPixmap.isNull() || m_scaledPixmap.size() != targetSize) {
        if (m_originalImage.size() == targetSize) {
            m_scaledPixmap = QPixmap::fromImage(m_originalImage);
        } else {
            m_scaledPixmap = QPixmap::fromImage(
                m_originalImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
        }
    }

    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap, m_scaledPixmap.size());
    }
}

void ImageViewer::updateScaledPixmapFast()
{
    if (m_originalImage.isNull()) return;

    QSize viewportSize;
    if (m_scrollArea && m_scrollArea->viewport() &&
        m_scrollArea->viewport()->width() > 50 && m_scrollArea->viewport()->height() > 50) {
        viewportSize = m_scrollArea->viewport()->size();
    } else if (width() > 50 && height() > 50) {
        viewportSize = size();
    } else {
        viewportSize = QSize(800, 600);
    }

    QSize targetSize = m_originalImage.size().scaled(viewportSize, Qt::KeepAspectRatio);
    if (m_originalImage.width() > 0) {
        m_zoomFactor = static_cast<double>(targetSize.width()) / static_cast<double>(m_originalImage.width());
    }
    if (targetSize.width() < 1) targetSize.setWidth(1);
    if (targetSize.height() < 1) targetSize.setHeight(1);

    m_scaledPixmap = QPixmap::fromImage(
        m_originalImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation)
    );
    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap, m_scaledPixmap.size());
    }
}

void ImageViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_fitToWindow) {
        updateScaledPixmap();
    }
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (layout()) {
        layout()->activate();
    }
    if (m_fitToWindow) {
        if (m_originalImage.width() * m_originalImage.height() > 1920 * 1080) {
            updateScaledPixmapFast();
            if (m_smoothResizeTimer) {
                m_smoothResizeTimer->start(35);
            }
        } else {
            updateScaledPixmap();
        }
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

    // 3. Normal Vertical Scrolling (File navigation via mouse wheel is disabled)
    int deltaY = event->angleDelta().y();
    if (deltaY != 0) {
        QScrollBar *vbar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
        if (vbar && vbar->maximum() > vbar->minimum()) {
            int step = (event->pixelDelta().y() != 0) ? event->pixelDelta().y() : qRound(deltaY * 0.75);
            if (step == 0 && deltaY != 0) step = (deltaY > 0 ? 1 : -1);
            vbar->setValue(vbar->value() - step);
            event->accept();
            return;
        } else {
            // Whole image fits vertically: consume event without navigating files
            event->accept();
            return;
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
    if (m_scrollArea && watched == m_scrollArea->viewport()) {
        if (event->type() == QEvent::Wheel) {
            auto *we = static_cast<QWheelEvent*>(event);
            handleWheel(we);
            return we->isAccepted();
        }
        if (event->type() == QEvent::ContextMenu) {
            auto *ce = static_cast<QContextMenuEvent*>(event);
            emit contextMenuRequested(ce->globalPos());
            return true;
        }
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
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::redo()
{
    if (m_redoStack.isEmpty()) return;
    m_undoStack.append(m_originalImage);
    m_originalImage = m_redoStack.takeLast();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

QList<PdfHistoryStep> ImageViewer::historySteps() const
{
    QList<PdfHistoryStep> steps;
    if (m_originalImage.isNull()) return steps;

    auto makeThumb = [](const QImage &img) -> QPixmap {
        if (img.isNull()) return QPixmap();
        return QPixmap::fromImage(img.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    };

    const QDateTime now = QDateTime::currentDateTime();
    int idx = 0;

    // 1. Past undo states (chronological oldest -> newest)
    for (int i = 0; i < m_undoStack.size(); ++i) {
        PdfHistoryStep st;
        st.index = idx++;
        st.description = (i == 0) ? tr("Original Image") : tr("Edit #%1").arg(i);
        st.timestamp = now;
        st.thumbnail = makeThumb(m_undoStack.at(i));
        st.isCurrent = false;
        steps.append(st);
    }

    // 2. Current active image state
    {
        PdfHistoryStep cur;
        cur.index = idx++;
        cur.description = m_undoStack.isEmpty() ? tr("Original Image") : tr("Current State");
        cur.timestamp = now;
        cur.thumbnail = makeThumb(m_originalImage);
        cur.isCurrent = true;
        steps.append(cur);
    }

    // 3. Future redo states (top of redoStack is next step)
    for (int i = m_redoStack.size() - 1; i >= 0; --i) {
        PdfHistoryStep st;
        st.index = idx++;
        st.description = tr("Redo #%1").arg(m_undoStack.size() + (m_redoStack.size() - i));
        st.timestamp = now;
        st.thumbnail = makeThumb(m_redoStack.at(i));
        st.isCurrent = false;
        steps.append(st);
    }

    return steps;
}

void ImageViewer::restoreHistoryStep(int index)
{
    if (m_originalImage.isNull()) return;
    const int curIdx = m_undoStack.size();
    const int total = m_undoStack.size() + 1 + m_redoStack.size();
    if (index < 0 || index >= total || index == curIdx) return;

    // Build unified chronological list, then split around target index
    QList<QImage> timeline;
    timeline.reserve(total);
    for (const QImage &img : m_undoStack) timeline.append(img);
    timeline.append(m_originalImage);
    for (int i = m_redoStack.size() - 1; i >= 0; --i) timeline.append(m_redoStack.at(i));

    m_undoStack.clear();
    for (int i = 0; i < index; ++i) {
        m_undoStack.append(timeline.at(i));
    }
    m_originalImage = timeline.at(index);
    m_redoStack.clear();
    for (int i = total - 1; i > index; --i) {
        m_redoStack.append(timeline.at(i));
    }

    m_scaledPixmap = QPixmap();
    setModified(index != 0);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
    emit undoAvailabilityChanged(canUndo(), canRedo());
}


void ImageViewer::resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode)
{
    if (!m_isEditMode || m_originalImage.isNull() || newWidth <= 0 || newHeight <= 0) return;
    if (m_originalImage.width() == newWidth && m_originalImage.height() == newHeight) return;

    pushUndoState();
    m_originalImage = m_originalImage.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, mode);
    m_scaledPixmap = QPixmap();

    // Reset fit-to-window and switch to 100% (1:1) zoom so user immediately perceives the actual size change
    m_fitToWindow = false;
    m_zoomFactor = 1.0;

    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::applyModifiedImage(const QImage &newImage)
{
    if (!m_isEditMode || newImage.isNull()) return;

    pushUndoState();
    m_originalImage = newImage;
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::adjustImage(int brightness, int contrast, int saturation,
                             int hue, int temperature,
                             int red, int green, int blue)
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    if (brightness == 0 && contrast == 0 && saturation == 0 &&
        hue == 0 && temperature == 0 && red == 0 && green == 0 && blue == 0) {
        return;
    }

    pushUndoState();
    m_originalImage = ImageProcessor::adjust(m_originalImage, brightness, contrast, saturation,
                                            hue, temperature, red, green, blue);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::applyCrop(const QRect &cropRect)
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    QRect validRect = cropRect.intersected(m_originalImage.rect());
    if (validRect.width() < 1 || validRect.height() < 1) return;

    pushUndoState();
    m_originalImage = m_originalImage.copy(validRect);
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    if (m_studioWidget) m_studioWidget->setSourceImage(m_originalImage);
}

void ImageViewer::setCropModeActive(bool active)
{
    if (active && !m_isEditMode) {
        return;
    }
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
    if (!m_isEditMode) {
        cancelCropMode();
        return;
    }
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
    QRect targetRect(ix, iy, iw, ih);

    setCropModeActive(false);
    applyCrop(targetRect);
}

void ImageViewer::cancelCropMode()
{
    setCropModeActive(false);
}



