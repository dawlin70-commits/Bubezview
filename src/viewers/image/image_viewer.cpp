#include "image_viewer.h"
#include "ui/photo_studio_panel.h"
#include "theme_manager.h"
#include "gpu_acceleration_manager.h"
#include "core/services/image_processor.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QImageReader>
#include <QMouseEvent>
#include <QTransform>
#include <QGuiApplication>
#include <QSettings>

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
        QPainter painter(this);
        if (m_pixmap.isNull()) {
            painter.fillRect(rect(), m_viewer ? m_viewer->canvasBackgroundColor() : palette().window().color());
            return;
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // 1. Fill background based on transparency mode if pixmap contains an alpha channel
        if (m_pixmap.hasAlphaChannel()) {
            ImageViewer::TransparencyMode mode = m_viewer ? m_viewer->transparencyMode() : ImageViewer::TransparencyMode::White;
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
                if (m_viewer) {
                    painter.fillRect(rect(), m_viewer->canvasBackgroundColor());
                } else {
                    painter.fillRect(rect(), palette().window());
                }
                break;
            case ImageViewer::TransparencyMode::Black:
                painter.fillRect(rect(), Qt::black);
                break;
            }
        } else {
            // Opaque pixmaps fill underlying canvas background in case of subpixel borders
            painter.fillRect(rect(), m_viewer ? m_viewer->canvasBackgroundColor() : palette().window().color());
        }

        // 2. Draw actual pixmap
        painter.drawPixmap(rect(), m_pixmap);

        // 3. Subtle 1px framing border for transparent images displayed on White or Checkerboard
        if (m_pixmap.hasAlphaChannel()) {
            ImageViewer::TransparencyMode mode = m_viewer ? m_viewer->transparencyMode() : ImageViewer::TransparencyMode::White;
            if (mode == ImageViewer::TransparencyMode::White || mode == ImageViewer::TransparencyMode::Checkerboard) {
                painter.setPen(QColor(0, 0, 0, 45));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(rect().adjusted(0, 0, -1, -1));
            }
        }

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
        if (m_viewer && m_viewer->isEyedropperActive() && event->button() == Qt::LeftButton) {
            if (width() > 0 && height() > 0 && !m_viewer->originalImage().isNull()) {
                double sx = double(m_viewer->originalImage().width()) / double(width());
                double sy = double(m_viewer->originalImage().height()) / double(height());
                int imgX = qBound(0, qRound(event->pos().x() * sx), m_viewer->originalImage().width() - 1);
                int imgY = qBound(0, qRound(event->pos().y() * sy), m_viewer->originalImage().height() - 1);
                if (m_viewer->studioPanel()) {
                    m_viewer->studioPanel()->setNeutralSamplePoint(QPoint(imgX, imgY));
                }
            }
            m_viewer->setEyedropperActive(false);
            event->accept();
            return;
        }

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
    , m_studioPanel(nullptr)
    , m_zoomFactor(1.0)
    , m_fitToWindow(true)
    , m_zoomToolActive(false)
    , m_isModified(false)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_studioSplitter = new QSplitter(Qt::Horizontal, this);
    m_studioSplitter->setHandleWidth(4);
    m_studioSplitter->setStyleSheet(
        "QSplitter::handle:horizontal {"
        "   background-color: rgba(255, 255, 255, 0.08);"
        "   width: 4px;"
        "}"
        "QSplitter::handle:horizontal:hover {"
        "   background-color: #38bdf8;"
        "}"
    );

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    // Create and set canvas widget into scroll area
    m_canvas = new ImageCanvasWidget(this, m_scrollArea);
    m_scrollArea->setWidget(m_canvas);

    if (m_scrollArea->viewport()) {
        m_scrollArea->viewport()->installEventFilter(this);
    }

    m_studioSplitter->addWidget(m_scrollArea);
    m_studioSplitter->setCollapsible(0, false);

    mainLayout->addWidget(m_studioSplitter);

    updateTheme();
}

void ImageViewer::ensureStudioPanelCreated()
{
    if (m_studioPanel) return;

    // Collapsible & Resizable Photo Studio Right Side Panel
    m_studioPanel = new PhotoStudioPanel(this);
    m_studioPanel->setVisible(false);
    connect(m_studioPanel, &PhotoStudioPanel::adjustmentsChanged, this, &ImageViewer::onStudioAdjustmentsChanged);
    connect(m_studioPanel, &PhotoStudioPanel::applyRequested, this, &ImageViewer::onStudioApply);
    connect(m_studioPanel, &PhotoStudioPanel::closeRequested, this, &ImageViewer::closeStudioPanel);
    connect(m_studioPanel, &PhotoStudioPanel::compareToggled, this, &ImageViewer::onStudioCompareToggled);
    connect(m_studioPanel, &PhotoStudioPanel::eyedropperToggled, this, &ImageViewer::onStudioEyedropperToggled);
    connect(m_studioPanel, &PhotoStudioPanel::privacyToolRequested, this, &ImageViewer::onStudioPrivacyToolRequested);
    connect(m_studioPanel, &PhotoStudioPanel::resetRequested, this, [this]() {
        if (!m_preStudioImage.isNull()) {
            m_studioAdjustedProxy = m_studioProxyImage;
            m_originalImage = m_studioProxyImage;
            m_sourcePixmap = QPixmap();
            m_scaledPixmap = QPixmap();
            updateScaledPixmap();
            if (m_studioPanel) {
                m_studioPanel->updateHistogram(m_studioProxyImage);
            }
        }
    });

    if (m_studioSplitter) {
        m_studioSplitter->addWidget(m_studioPanel);
        m_studioSplitter->setCollapsible(1, true);
    }
}

ImageViewer::~ImageViewer() = default;

QColor ImageViewer::canvasBackgroundColor() const
{
    ThemePreset preset = ThemeManager::currentPreset();
    switch (preset) {
    case ThemePreset::DarkSlate:   return QColor("#22262b");
    case ThemePreset::BreezeDark:  return QColor("#181b1d");
    case ThemePreset::ModernLight: return QColor("#f5f5f7");
    case ThemePreset::BreezeLight: return QColor("#eff0f1");
    }
    return QColor("#22262b");
}

bool ImageViewer::isDarkTheme() const
{
    return ThemeManager::isDark(ThemeManager::currentPreset());
}

ImageViewer::TransparencyMode ImageViewer::transparencyMode() const
{
    QSettings settings("Bubez", "BubezView");
    QString mode = settings.value("image/transparency_mode", "white").toString();
    if (mode == "checkerboard") return TransparencyMode::Checkerboard;
    if (mode == "theme") return TransparencyMode::Theme;
    if (mode == "black") return TransparencyMode::Black;
    return TransparencyMode::White;
}

void ImageViewer::setTransparencyMode(TransparencyMode mode)
{
    QSettings settings("Bubez", "BubezView");
    QString str = "white";
    if (mode == TransparencyMode::Checkerboard) str = "checkerboard";
    else if (mode == TransparencyMode::Theme) str = "theme";
    else if (mode == TransparencyMode::Black) str = "black";
    settings.setValue("image/transparency_mode", str);
    if (m_canvas) {
        m_canvas->update();
    }
    if (m_studioPanel) {
        m_studioPanel->updateTheme();
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
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();
    QString canvasBg = canvasBackgroundColor().name();

    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(
            "QScrollArea { background-color: " + canvasBg + "; border: none; }\n" +
            ThemeManager::scrollBarStyleSheet(preset)
        );
        if (m_scrollArea->viewport()) {
            QPalette pal = m_scrollArea->viewport()->palette();
            pal.setColor(QPalette::Window, canvasBackgroundColor());
            m_scrollArea->viewport()->setPalette(pal);
            m_scrollArea->viewport()->setStyleSheet("background-color: " + canvasBg + "; border: none;");
            m_scrollArea->viewport()->setAutoFillBackground(true);
            m_scrollArea->viewport()->update();
        }
    }
    if (m_canvas) {
        m_canvas->update();
    }
    if (m_studioPanel) {
        m_studioPanel->updateTheme();
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
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
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
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    m_zoomFactor = 1.0;
    m_fitToWindow = true;
    setModified(false);
    updateScaledPixmap();
    return true;
}

void ImageViewer::clear()
{
    closeStudioPanel();
    clearUndoRedo();
    m_originalImage = QImage();
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    if (m_canvas) {
        m_canvas->setPixmap(m_scaledPixmap, QSize(0, 0));
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

void ImageViewer::setEditMode(bool editMode)
{
    m_isEditMode = editMode;
    if (!editMode) {
        cancelCropMode();
    }
}

void ImageViewer::rotateClockwise()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(90);
    m_originalImage = m_originalImage.transformed(t);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::rotateCounterClockwise()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    QTransform t;
    t.rotate(-90);
    m_originalImage = m_originalImage.transformed(t);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::flipHorizontal()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(true, false);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::flipVertical()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage = m_originalImage.mirrored(false, true);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::invertColors()
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    pushUndoState();
    m_originalImage.invertPixels();
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::updateScaledPixmap()
{
    if (m_originalImage.isNull()) {
        m_scaledPixmap = QPixmap();
        m_sourcePixmap = QPixmap();
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
    // Never zoom or scroll main image when cursor is interacting with the Photo Studio panel
    if (m_studioPanel && m_studioPanel->isVisible()) {
        QPoint ptInPanel = m_studioPanel->mapFromGlobal(event->globalPosition().toPoint());
        if (m_studioPanel->rect().contains(ptInPanel)) {
            event->accept();
            return;
        }
    }

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
            // In Edit Mode, Crop Mode, or Photo Studio, mouse wheel file navigation is strictly disabled
            if (m_isEditMode || isCropModeActive() || isStudioPanelVisible()) {
                event->accept();
                return;
            }
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
                // In Edit Mode, hitting the bottom edge must never navigate away
                if (m_isEditMode || isCropModeActive() || isStudioPanelVisible()) {
                    event->accept();
                    return;
                }
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
                // In Edit Mode, hitting the top edge must never navigate away
                if (m_isEditMode || isCropModeActive() || isStudioPanelVisible()) {
                    event->accept();
                    return;
                }
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
    if (!m_isEditMode || m_undoStack.isEmpty()) return;
    m_redoStack.append(m_originalImage);
    m_originalImage = m_undoStack.takeLast();
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ImageViewer::redo()
{
    if (!m_isEditMode || m_redoStack.isEmpty()) return;
    m_undoStack.append(m_originalImage);
    m_originalImage = m_redoStack.takeLast();
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}


void ImageViewer::resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode)
{
    if (!m_isEditMode || m_originalImage.isNull() || newWidth <= 0 || newHeight <= 0) return;
    if (m_originalImage.width() == newWidth && m_originalImage.height() == newHeight) return;

    pushUndoState();
    m_originalImage = m_originalImage.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, mode);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();

    // Reset fit-to-window and switch to 100% (1:1) zoom so user immediately perceives the actual size change
    m_fitToWindow = false;
    m_zoomFactor = 1.0;

    setModified(true);
    updateScaledPixmap();
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::applyModifiedImage(const QImage &newImage)
{
    if (!m_isEditMode || newImage.isNull()) return;

    pushUndoState();
    m_originalImage = newImage;
    m_sourcePixmap = QPixmap();
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
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
}

void ImageViewer::applyCrop(const QRect &cropRect)
{
    if (!m_isEditMode || m_originalImage.isNull()) return;
    QRect validRect = cropRect.intersected(m_originalImage.rect());
    if (validRect.width() < 1 || validRect.height() < 1) return;

    pushUndoState();
    m_originalImage = m_originalImage.copy(validRect);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    setModified(true);
    updateScaledPixmap();
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

    if (!m_privacyMode.isEmpty()) {
        pushUndoState();

        QRect fullResRect = targetRect;
        if (!m_preStudioImage.isNull() && !m_originalImage.isNull() &&
            m_originalImage.width() > 0 && m_originalImage.height() > 0) {
            double px = double(m_preStudioImage.width()) / double(m_originalImage.width());
            double py = double(m_preStudioImage.height()) / double(m_originalImage.height());
            int ox = qBound(0, qRound(targetRect.x() * px), m_preStudioImage.width() - 1);
            int oy = qBound(0, qRound(targetRect.y() * py), m_preStudioImage.height() - 1);
            int ow = qBound(1, qRound(targetRect.width() * px), m_preStudioImage.width() - ox);
            int oh = qBound(1, qRound(targetRect.height() * py), m_preStudioImage.height() - oy);
            fullResRect = QRect(ox, oy, ow, oh);
        }

        if (m_privacyMode == "pixelate") {
            m_originalImage = ImageProcessor::pixelateRegion(m_originalImage, targetRect);
            if (!m_preStudioImage.isNull()) {
                m_preStudioImage = ImageProcessor::pixelateRegion(m_preStudioImage, fullResRect);
            }
            if (!m_studioProxyImage.isNull()) {
                m_studioProxyImage = ImageProcessor::pixelateRegion(m_studioProxyImage, targetRect);
            }
        } else if (m_privacyMode == "blur") {
            m_originalImage = ImageProcessor::blurRegion(m_originalImage, targetRect);
            if (!m_preStudioImage.isNull()) {
                m_preStudioImage = ImageProcessor::blurRegion(m_preStudioImage, fullResRect);
            }
            if (!m_studioProxyImage.isNull()) {
                m_studioProxyImage = ImageProcessor::blurRegion(m_studioProxyImage, targetRect);
            }
        }
        m_privacyMode.clear();
        setCropModeActive(false);
        setModified(true);
        m_sourcePixmap = QPixmap();
        m_scaledPixmap = QPixmap();
        updateScaledPixmap();
        return;
    }

    setCropModeActive(false);
    applyCrop(targetRect);
}

void ImageViewer::cancelCropMode()
{
    m_privacyMode.clear();
    setCropModeActive(false);
}

void ImageViewer::toggleStudioPanel()
{
    if (isStudioPanelVisible()) {
        closeStudioPanel();
    } else {
        openStudioPanel();
    }
}

void ImageViewer::openStudioPanel()
{
    ensureStudioPanelCreated();
    if (!m_studioPanel || m_originalImage.isNull()) return;
    m_preStudioImage = m_originalImage;

    int studioW = 340; // breathing room default
    int canvasW = qMax(200, width() - studioW);
    int canvasH = qMax(200, height() - 40);
    QSize targetDisplaySize = m_preStudioImage.size().scaled(QSize(canvasW, canvasH), Qt::KeepAspectRatio);
    if (targetDisplaySize.width() < 1) targetDisplaySize.setWidth(1);
    if (targetDisplaySize.height() < 1) targetDisplaySize.setHeight(1);

    // Create fast preview proxy sized to viewport for instantaneous 60fps adjustments
    if (m_preStudioImage.size() != targetDisplaySize) {
        m_studioProxyImage = m_preStudioImage.scaled(targetDisplaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_studioProxyImage = m_preStudioImage;
    }
    m_studioAdjustedProxy = m_studioProxyImage;
    m_autoProxyValid = false;
    m_compareShowingOriginal = false;
    m_eyedropperActive = false;
    m_privacyMode.clear();

    m_studioPanel->resetAll();
    m_studioPanel->updateHistogram(m_studioProxyImage);
    m_studioPanel->setVisible(true);

    if (m_studioSplitter) {
        m_studioSplitter->setSizes({canvasW, studioW});
    }
}

void ImageViewer::closeStudioPanel()
{
    if (!m_studioPanel || !m_studioPanel->isVisible()) return;
    m_studioPanel->setVisible(false);
    m_eyedropperActive = false;
    m_privacyMode.clear();
    if (m_canvas) {
        m_canvas->unsetCursor();
        m_canvas->setCropActive(false);
    }
    if (!m_preStudioImage.isNull()) {
        m_originalImage = m_preStudioImage;
        m_sourcePixmap = QPixmap();
        m_scaledPixmap = QPixmap();
        updateScaledPixmap();
        m_preStudioImage = QImage();
    }
    m_studioProxyImage = QImage();
    m_studioAdjustedProxy = QImage();
    m_autoEnhancedProxy = QImage();
    m_autoProxyValid = false;

    if (m_studioSplitter) {
        m_studioSplitter->setSizes({width(), 0});
    }
}

bool ImageViewer::isStudioPanelVisible() const
{
    return m_studioPanel && m_studioPanel->isVisible();
}

void ImageViewer::setEyedropperActive(bool a)
{
    onStudioEyedropperToggled(a);
}

void ImageViewer::onStudioAdjustmentsChanged()
{
    if (m_studioProxyImage.isNull() || !m_studioPanel) return;

    QImage processed = m_studioProxyImage;

    // 1. Auto-Enhance Stage
    if (m_studioPanel->isAutoEnhanceEnabled()) {
        ImageProcessor::AutoEnhanceOptions opts = m_studioPanel->autoEnhanceOptions();

        bool optionsMatch = m_autoProxyValid
            && (m_cachedAutoOpts.profile == opts.profile)
            && (m_cachedAutoOpts.protectHighlights == opts.protectHighlights)
            && (m_cachedAutoOpts.recoverShadows == opts.recoverShadows)
            && (m_cachedAutoOpts.correctWhiteBalance == opts.correctWhiteBalance)
            && (m_cachedAutoOpts.protectSkinTones == opts.protectSkinTones)
            && (m_cachedAutoOpts.autoDenoiseShadows == opts.autoDenoiseShadows)
            && (m_cachedAutoOpts.centerWeighted == opts.centerWeighted)
            && (m_cachedAutoOpts.neutralSamplePoint == opts.neutralSamplePoint);

        if (!optionsMatch) {
            ImageProcessor::AutoEnhanceOptions baseOpts = opts;
            baseOpts.intensity = 100;
            m_autoEnhancedProxy = ImageProcessor::autoEnhance(m_studioProxyImage, baseOpts);
            m_cachedAutoOpts = opts;
            m_autoProxyValid = true;
        }

        if (opts.intensity == 100) {
            processed = m_autoEnhancedProxy;
        } else if (opts.intensity > 0) {
            processed = ImageProcessor::blend(m_studioProxyImage, m_autoEnhancedProxy, opts.intensity / 100.0);
        }
    }

    // 2. Filter Preset Stage
    if (m_studioPanel->activePreset() != ImageProcessor::FilterPreset::Original) {
        QImage filtered = ImageProcessor::applyPreset(processed, m_studioPanel->activePreset());
        int strength = m_studioPanel->presetIntensity();
        if (strength < 100) {
            processed = ImageProcessor::blend(processed, filtered, strength / 100.0);
        } else {
            processed = filtered;
        }
    }

    // 3. Manual Adjustments Stage (Light, Color, Channels)
    int exp = m_studioPanel->exposure();
    int b = m_studioPanel->brightness();
    int c = m_studioPanel->contrast();
    int hi = m_studioPanel->highlights();
    int sh = m_studioPanel->shadows();
    int s = m_studioPanel->saturation();
    int t = m_studioPanel->temperature();
    int h = m_studioPanel->tint();
    int r = m_studioPanel->red();
    int g = m_studioPanel->green();
    int bl = m_studioPanel->blue();

    int totalBrightness = qBound(-100, b + exp, 100);

    if (totalBrightness != 0 || c != 0 || s != 0 || h != 0 || t != 0 || r != 0 || g != 0 || bl != 0 || hi != 0 || sh != 0) {
        processed = ImageProcessor::adjust(processed, totalBrightness, c, s, h, t, r, g, bl, hi, sh);
    }

    // 4. Detail, Vignette & Invert
    int sharp = m_studioPanel->sharpness();
    if (sharp > 0) {
        processed = ImageProcessor::applySharpen(processed, sharp);
    }

    int vig = m_studioPanel->vignette();
    if (vig > 0) {
        int vigRad = m_studioPanel->vignetteRadius();
        processed = ImageProcessor::applyVignette(processed, vig, vigRad);
    }

    if (m_studioPanel->isInvertColors()) {
        processed = ImageProcessor::applyInvert(processed);
    }

    m_studioAdjustedProxy = processed;

    // Real-time histogram update
    m_studioPanel->updateHistogram(processed);

    // Display on canvas
    if (!m_compareShowingOriginal) {
        m_originalImage = processed;
        m_sourcePixmap = QPixmap();
        m_scaledPixmap = QPixmap();
        updateScaledPixmap();
    }
}

void ImageViewer::onStudioApply()
{
    if (m_preStudioImage.isNull() || !m_studioPanel) return;

    // Apply entire pipeline to full-resolution original image
    QImage processed = m_preStudioImage;

    // 1. Auto-Enhance
    if (m_studioPanel->isAutoEnhanceEnabled()) {
        ImageProcessor::AutoEnhanceOptions opts = m_studioPanel->autoEnhanceOptions();
        if (opts.neutralSamplePoint.x() >= 0 && !m_studioProxyImage.isNull() &&
            m_studioProxyImage.width() > 0 && m_studioProxyImage.height() > 0) {
            double px = double(m_preStudioImage.width()) / double(m_studioProxyImage.width());
            double py = double(m_preStudioImage.height()) / double(m_studioProxyImage.height());
            opts.neutralSamplePoint = QPoint(
                qBound(0, qRound(opts.neutralSamplePoint.x() * px), m_preStudioImage.width() - 1),
                qBound(0, qRound(opts.neutralSamplePoint.y() * py), m_preStudioImage.height() - 1)
            );
        }
        QImage autoImg = ImageProcessor::autoEnhance(m_preStudioImage, opts);
        if (opts.intensity == 100) {
            processed = autoImg;
        } else if (opts.intensity > 0) {
            processed = ImageProcessor::blend(m_preStudioImage, autoImg, opts.intensity / 100.0);
        }
    }

    // 2. Filter Preset
    if (m_studioPanel->activePreset() != ImageProcessor::FilterPreset::Original) {
        QImage filtered = ImageProcessor::applyPreset(processed, m_studioPanel->activePreset());
        int strength = m_studioPanel->presetIntensity();
        if (strength < 100) {
            processed = ImageProcessor::blend(processed, filtered, strength / 100.0);
        } else {
            processed = filtered;
        }
    }

    // 3. Manual Adjustments
    int exp = m_studioPanel->exposure();
    int b = m_studioPanel->brightness();
    int c = m_studioPanel->contrast();
    int hi = m_studioPanel->highlights();
    int sh = m_studioPanel->shadows();
    int s = m_studioPanel->saturation();
    int t = m_studioPanel->temperature();
    int h = m_studioPanel->tint();
    int r = m_studioPanel->red();
    int g = m_studioPanel->green();
    int bl = m_studioPanel->blue();

    int totalBrightness = qBound(-100, b + exp, 100);
    if (totalBrightness != 0 || c != 0 || s != 0 || h != 0 || t != 0 || r != 0 || g != 0 || bl != 0 || hi != 0 || sh != 0) {
        processed = ImageProcessor::adjust(processed, totalBrightness, c, s, h, t, r, g, bl, hi, sh);
    }

    // 4. Detail, Vignette & Invert
    int sharp = m_studioPanel->sharpness();
    if (sharp > 0) {
        processed = ImageProcessor::applySharpen(processed, sharp);
    }

    int vig = m_studioPanel->vignette();
    if (vig > 0) {
        int vigRad = m_studioPanel->vignetteRadius();
        processed = ImageProcessor::applyVignette(processed, vig, vigRad);
    }

    if (m_studioPanel->isInvertColors()) {
        processed = ImageProcessor::applyInvert(processed);
    }

    pushUndoState();
    m_originalImage = processed;
    m_preStudioImage = QImage();
    m_studioProxyImage = QImage();
    m_studioAdjustedProxy = QImage();
    m_autoEnhancedProxy = QImage();
    m_autoProxyValid = false;

    setModified(true);
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    updateScaledPixmap();

    m_studioPanel->setVisible(false);
    if (m_studioSplitter) {
        m_studioSplitter->setSizes({width(), 0});
    }
}

void ImageViewer::onStudioCompareToggled(bool showOriginal)
{
    m_compareShowingOriginal = showOriginal;
    if (m_compareShowingOriginal) {
        m_originalImage = m_studioProxyImage.isNull() ? m_preStudioImage : m_studioProxyImage;
    } else {
        m_originalImage = m_studioAdjustedProxy.isNull() ? m_preStudioImage : m_studioAdjustedProxy;
    }
    m_sourcePixmap = QPixmap();
    m_scaledPixmap = QPixmap();
    updateScaledPixmap();
}

void ImageViewer::onStudioEyedropperToggled(bool active)
{
    m_eyedropperActive = active;
    if (m_canvas) {
        if (active) {
            m_canvas->setCursor(Qt::CrossCursor);
        } else {
            m_canvas->unsetCursor();
        }
    }
}

void ImageViewer::onStudioPrivacyToolRequested(const QString &mode)
{
    m_privacyMode = mode;
    if (m_canvas) {
        m_canvas->setCropActive(true);
    }
}


