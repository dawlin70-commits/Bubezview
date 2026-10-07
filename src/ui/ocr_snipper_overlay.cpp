#include "ocr_snipper_overlay.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>

OcrSnipperOverlay::OcrSnipperOverlay(QWidget *targetWidget, QWidget *parent)
    : QWidget(parent, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool)
    , m_targetWidget(targetWidget)
    , m_isSelecting(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    if (m_targetWidget) {
        m_backgroundPixmap = m_targetWidget->grab();
        QPoint globalPos = m_targetWidget->mapToGlobal(QPoint(0, 0));
        setGeometry(QRect(globalPos, m_targetWidget->size()));
    } else {
        setGeometry(0, 0, 800, 600);
    }
}

void OcrSnipperOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus();
    activateWindow();
}

void OcrSnipperOverlay::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

void OcrSnipperOverlay::setHighDpiCropper(std::function<QImage(const QRectF &)> cropper)
{
    m_highDpiCropper = cropper;
}

void OcrSnipperOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    // Draw base target background
    if (!m_backgroundPixmap.isNull()) {
        p.drawPixmap(0, 0, m_backgroundPixmap);
    } else {
        p.fillRect(rect(), Qt::black);
    }

    // Semi-transparent overlay to dim the surrounding background
    p.fillRect(rect(), QColor(0, 0, 0, 110));

    if (m_isSelecting) {
        QRect selRect = QRect(m_startPoint, m_currentPoint).normalized();

        // Reveal the unshaded image inside the selected area
        if (!m_backgroundPixmap.isNull()) {
            p.drawPixmap(selRect, m_backgroundPixmap, selRect);
        } else {
            p.fillRect(selRect, Qt::white);
        }

        // Draw crisp glowing accent boundary
        QPen pen(QColor("#3b82f6"), 2, Qt::SolidLine);
        p.setPen(pen);
        p.drawRect(selRect);

        // Small dimensions indicator
        if (selRect.width() > 30 && selRect.height() > 20) {
            QString dimStr = QString("%1 × %2").arg(selRect.width()).arg(selRect.height());
            p.setFont(QFont("sans-serif", 8, QFont::Bold));
            QRect badgeRect(selRect.x(), qMax(0, selRect.y() - 18), 75, 16);
            p.fillRect(badgeRect, QColor(0, 0, 0, 180));
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, dimStr);
        }
    }
}

void OcrSnipperOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        emit snippingCanceled();
        close();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_isSelecting = true;
        m_startPoint = event->pos();
        m_currentPoint = m_startPoint;
        update();
    }
}

void OcrSnipperOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting) {
        m_currentPoint = event->pos();
        update();
    }
}

void OcrSnipperOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        QRect selRect = QRect(m_startPoint, event->pos()).normalized();

        if (selRect.width() >= 8 && selRect.height() >= 8) {
            QImage snippet;
            if (m_highDpiCropper && width() > 0 && height() > 0) {
                QRectF normRect(selRect.x() / static_cast<double>(width()),
                                selRect.y() / static_cast<double>(height()),
                                selRect.width() / static_cast<double>(width()),
                                selRect.height() / static_cast<double>(height()));
                snippet = m_highDpiCropper(normRect);
            }

            if (snippet.isNull() && !m_backgroundPixmap.isNull()) {
                snippet = m_backgroundPixmap.copy(selRect).toImage();
            }

            if (!snippet.isNull()) {
                emit snippetCaptured(snippet);
            } else {
                emit snippingCanceled();
            }
        } else {
            emit snippingCanceled();
        }

        close();
    }
}

void OcrSnipperOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit snippingCanceled();
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}
