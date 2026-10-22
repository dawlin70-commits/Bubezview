/**
 * @file photo_studio_dialog.cpp
 * @brief Implementation of the modern Photo Studio & Filter Studio dialog.
 */

#include "photo_studio_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QApplication>
#include <cmath>
#include "core/services/theme_manager.h"

// ============================================================================
// 1. HistogramWidget Implementation
// ============================================================================

HistogramWidget::HistogramWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(110);
    setMinimumWidth(260);
    m_histR.fill(0, 256);
    m_histG.fill(0, 256);
    m_histB.fill(0, 256);
    m_histLuma.fill(0, 256);
}

void HistogramWidget::updateHistogram(const QImage &image)
{
    ImageProcessor::computeHistogram(image, m_histR, m_histG, m_histB, m_histLuma,
                                    m_clippedShadows, m_clippedHighlights);

    m_maxCount = 1;
    for (int i = 0; i < 256; ++i) {
        m_maxCount = std::max({m_maxCount, m_histR[i], m_histG[i], m_histB[i], m_histLuma[i]});
    }

    update();
}

void HistogramWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(1, 1, -1, -1);

    // Theme-adaptive background and border
    bool isDark = palette().windowText().color().value() > palette().window().color().value();
    QColor bg = isDark ? QColor(24, 26, 30) : palette().base().color();
    QColor border = isDark ? QColor(55, 60, 68) : palette().mid().color();

    p.fillRect(rect(), bg);
    p.setPen(border);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    if (m_maxCount <= 1) return;

    const double w = r.width();
    const double h = r.height() - 8;
    const double baseBottom = r.bottom() - 4;

    // Helper lambda to draw curve
    auto drawCurve = [&](const QVector<int> &hist, const QColor &stroke, const QColor &fill) {
        QPainterPath path;
        path.moveTo(r.left(), baseBottom);

        for (int i = 0; i < 256; ++i) {
            double px = r.left() + (i * w) / 255.0;
            double py = baseBottom - (static_cast<double>(hist[i]) / m_maxCount) * h;
            path.lineTo(px, py);
        }
        path.lineTo(r.right(), baseBottom);
        path.closeSubpath();

        if (fill.isValid()) {
            p.fillPath(path, fill);
        }
        p.setPen(QPen(stroke, 1.2));
        p.drawPath(path);
    };

    // Draw RGB & Luminance curves according to theme contrast
    if (isDark) {
        p.setCompositionMode(QPainter::CompositionMode_Screen);
        drawCurve(m_histR, QColor(255, 75, 75, 200), QColor(255, 60, 60, 45));
        drawCurve(m_histG, QColor(75, 220, 95, 200), QColor(60, 220, 80, 45));
        drawCurve(m_histB, QColor(70, 150, 255, 200), QColor(60, 140, 255, 45));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawCurve(m_histLuma, QColor(230, 230, 230, 180), QColor());
    } else {
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawCurve(m_histR, QColor(220, 38, 38, 200), QColor(220, 38, 38, 35));
        drawCurve(m_histG, QColor(22, 163, 74, 200), QColor(22, 163, 74, 35));
        drawCurve(m_histB, QColor(2, 132, 199, 200), QColor(2, 132, 199, 35));
        drawCurve(m_histLuma, QColor(71, 85, 105, 200), QColor());
    }

    // Badges for clipped shadows / highlights with dynamic text metrics
    QFont f = font();
    f.setPointSize(8);
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);

    if (m_clippedShadows) {
        QString text = tr("Shadows Clipped");
        int badgeW = fm.horizontalAdvance(text) + 14;
        QRect shadowBadge(r.left() + 8, r.top() + 7, badgeW, 19);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(2, 132, 199, 230)); // Modern cyan/blue
        p.drawRoundedRect(shadowBadge, 4, 4);
        p.setPen(Qt::white);
        p.drawText(shadowBadge, Qt::AlignCenter, text);
    }

    if (m_clippedHighlights) {
        QString text = tr("Highlights Clipped");
        int badgeW = fm.horizontalAdvance(text) + 14;
        QRect hlBadge(r.right() - badgeW - 8, r.top() + 7, badgeW, 19);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(225, 29, 72, 230)); // Rose red
        p.drawRoundedRect(hlBadge, 4, 4);
        p.setPen(Qt::white);
        p.drawText(hlBadge, Qt::AlignCenter, text);
    }
}

// ============================================================================
// 2. PhotoStudioViewport Implementation
// ============================================================================

PhotoStudioViewport::PhotoStudioViewport(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PhotoStudioViewport::setOriginalImage(const QImage &image)
{
    m_originalImage = image;
    update();
}

void PhotoStudioViewport::setModifiedImage(const QImage &image)
{
    m_modifiedImage = image;
    update();
}

void PhotoStudioViewport::setHoverImage(const QImage &image)
{
    m_hoverImage = image;
    m_hasHoverImage = true;
    update();
}

void PhotoStudioViewport::clearHoverImage()
{
    m_hasHoverImage = false;
    update();
}

void PhotoStudioViewport::setSplitViewEnabled(bool enabled)
{
    m_splitViewEnabled = enabled;
    update();
}

void PhotoStudioViewport::setHoldCompareActive(bool active)
{
    m_holdCompareActive = active;
    update();
}

void PhotoStudioViewport::setPrivacyToolActive(bool active)
{
    m_privacyToolActive = active;
    if (m_privacyToolActive) {
        m_eyedropperActive = false;
        m_fourWayActive = false;
    } else {
        clearSelection();
    }
    setCursor(m_privacyToolActive ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void PhotoStudioViewport::setEyedropperActive(bool active)
{
    m_eyedropperActive = active;
    if (m_eyedropperActive) {
        m_privacyToolActive = false;
        m_fourWayActive = false;
        clearSelection();
    }
    setCursor(m_eyedropperActive ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void PhotoStudioViewport::setFourWayCompare(bool active,
                                            const QImage &orig,
                                            const QImage &natural,
                                            const QImage &vivid,
                                            const QImage &shadows)
{
    m_fourWayActive = active;
    m_fourWayOrig = orig;
    m_fourWayNatural = natural;
    m_fourWayVivid = vivid;
    m_fourWayShadows = shadows;
    if (m_fourWayActive) {
        m_privacyToolActive = false;
        m_eyedropperActive = false;
        m_splitViewEnabled = false;
    }
    update();
}

void PhotoStudioViewport::clearSelection()
{
    m_currentSelectionRect = QRect();
    m_selectingRect = false;
    update();
}

QRect PhotoStudioViewport::imageViewportRect() const
{
    const QImage &ref = m_modifiedImage.isNull() ? m_originalImage : m_modifiedImage;
    if (ref.isNull() || width() <= 0 || height() <= 0) return QRect();

    QSize scaledSize = ref.size().scaled(size() - QSize(16, 16), Qt::KeepAspectRatio);
    int x = (width() - scaledSize.width()) / 2;
    int y = (height() - scaledSize.height()) / 2;
    return QRect(x, y, scaledSize.width(), scaledSize.height());
}

QPoint PhotoStudioViewport::widgetToImage(const QPoint &pt) const
{
    QRect viewRect = imageViewportRect();
    if (viewRect.isEmpty() || !viewRect.contains(pt)) return QPoint(-1, -1);

    const QImage &ref = m_modifiedImage.isNull() ? m_originalImage : m_modifiedImage;
    double scaleX = static_cast<double>(ref.width()) / viewRect.width();
    double scaleY = static_cast<double>(ref.height()) / viewRect.height();

    int ix = static_cast<int>((pt.x() - viewRect.left()) * scaleX);
    int iy = static_cast<int>((pt.y() - viewRect.top()) * scaleY);
    return QPoint(qBound(0, ix, ref.width() - 1), qBound(0, iy, ref.height() - 1));
}

QRect PhotoStudioViewport::widgetToImage(const QRect &rect) const
{
    QRect viewRect = imageViewportRect();
    if (viewRect.isEmpty()) return QRect();

    QRect clipped = rect.normalized().intersected(viewRect);
    if (clipped.isEmpty()) return QRect();

    const QImage &ref = m_modifiedImage.isNull() ? m_originalImage : m_modifiedImage;
    if (ref.isNull() || viewRect.width() <= 0 || viewRect.height() <= 0) return QRect();

    double scaleX = static_cast<double>(ref.width()) / viewRect.width();
    double scaleY = static_cast<double>(ref.height()) / viewRect.height();

    int ix1 = static_cast<int>((clipped.left() - viewRect.left()) * scaleX);
    int iy1 = static_cast<int>((clipped.top() - viewRect.top()) * scaleY);
    int ix2 = static_cast<int>((clipped.right() - viewRect.left()) * scaleX);
    int iy2 = static_cast<int>((clipped.bottom() - viewRect.top()) * scaleY);

    ix1 = qBound(0, ix1, ref.width() - 1);
    iy1 = qBound(0, iy1, ref.height() - 1);
    ix2 = qBound(0, ix2, ref.width() - 1);
    iy2 = qBound(0, iy2, ref.height() - 1);

    return QRect(QPoint(ix1, iy1), QPoint(ix2, iy2)).normalized();
}

QRect PhotoStudioViewport::selectedImageRect() const
{
    return widgetToImage(m_currentSelectionRect);
}

void PhotoStudioViewport::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    ThemePreset preset = ThemeManager::currentPreset();
    QColor canvasBg;
    if (preset == ThemePreset::SystemNative) {
        bool isDark = palette().windowText().color().value() > palette().window().color().value();
        canvasBg = isDark ? QColor("#1e2226") : QColor("#e4e7eb");
    } else if (preset == ThemePreset::DarkSlate) {
        canvasBg = QColor("#22262b");
    } else {
        canvasBg = QColor("#181b1d");
    }
    p.fillRect(rect(), canvasBg);

    QRect viewRect = imageViewportRect();
    if (viewRect.isEmpty()) return;

    const QImage &displayMod = m_hasHoverImage ? m_hoverImage : m_modifiedImage;

    // Draw image shadow / border
    bool isDark = canvasBg.value() < 128;
    p.setPen(isDark ? QColor(40, 44, 52) : QColor(185, 190, 200));
    p.drawRect(viewRect.adjusted(-1, -1, 0, 0));

    if (m_fourWayActive && !m_fourWayOrig.isNull()) {
        int halfW = viewRect.width() / 2;
        int halfH = viewRect.height() / 2;
        QRect q1(viewRect.left(), viewRect.top(), halfW, halfH);
        QRect q2(viewRect.left() + halfW, viewRect.top(), viewRect.width() - halfW, halfH);
        QRect q3(viewRect.left(), viewRect.top() + halfH, halfW, viewRect.height() - halfH);
        QRect q4(viewRect.left() + halfW, viewRect.top() + halfH, viewRect.width() - halfW, viewRect.height() - halfH);

        p.drawImage(q1, m_fourWayOrig);
        p.drawImage(q2, m_fourWayNatural);
        p.drawImage(q3, m_fourWayVivid);
        p.drawImage(q4, m_fourWayShadows);

        // Divider lines
        p.setPen(QPen(QColor(0, 160, 255, 200), 2));
        p.drawLine(viewRect.left() + halfW, viewRect.top(), viewRect.left() + halfW, viewRect.bottom());
        p.drawLine(viewRect.left(), viewRect.top() + halfH, viewRect.right(), viewRect.top() + halfH);

        // Labels
        QFont f = font();
        f.setPointSize(8);
        f.setBold(true);
        p.setFont(f);

        auto drawBadge = [&](const QRect &qr, const QString &text, const QColor &col) {
            QRect b(qr.left() + 6, qr.top() + 6, 130, 20);
            p.fillRect(b, col);
            p.setPen(Qt::white);
            p.drawText(b, Qt::AlignCenter, text);
        };

        drawBadge(q1, tr("1. Original"), QColor(40, 110, 220, 210));
        drawBadge(q2, tr("2. Natural"), QColor(40, 160, 80, 210));
        drawBadge(q3, tr("3. Vivid & Scene"), QColor(210, 120, 20, 210));
        drawBadge(q4, tr("4. Backlight / Shadows"), QColor(140, 50, 200, 210));

        // Instruction banner
        QRect banner(viewRect.left(), viewRect.bottom() - 22, viewRect.width(), 22);
        p.fillRect(banner, QColor(0, 0, 0, 180));
        p.setPen(QColor(220, 230, 245));
        p.drawText(banner, Qt::AlignCenter, tr("Click any quadrant to choose that Auto-Fix profile"));
        return;
    }

    if (m_holdCompareActive) {
        // Hold to compare: Show 100% Original
        if (!m_originalImage.isNull()) {
            p.drawImage(viewRect, m_originalImage);
        }
        // Badge
        QFont f = font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        QRect badge(viewRect.left() + 10, viewRect.top() + 10, 100, 24);
        p.fillRect(badge, QColor(30, 130, 240, 220));
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, tr("ORIGINAL"));
    }
    else if (m_splitViewEnabled && !m_originalImage.isNull() && !displayMod.isNull()) {
        // Split-Screen mode (Draggable vertical line)
        int splitX = viewRect.left() + static_cast<int>(viewRect.width() * m_splitRatio);

        // 1. Draw Original on Left
        p.save();
        p.setClipRect(QRect(viewRect.left(), viewRect.top(), splitX - viewRect.left(), viewRect.height()));
        p.drawImage(viewRect, m_originalImage);
        p.restore();

        // 2. Draw Modified/Hover on Right
        p.save();
        p.setClipRect(QRect(splitX, viewRect.top(), viewRect.right() - splitX + 1, viewRect.height()));
        p.drawImage(viewRect, displayMod);
        p.restore();

        // 3. Draw Split Line & Handle
        p.setPen(QPen(QColor(255, 255, 255, 230), 2));
        p.drawLine(splitX, viewRect.top(), splitX, viewRect.bottom());

        // Handle knob
        int knobY = viewRect.top() + viewRect.height() / 2;
        QRect knobRect(splitX - 16, knobY - 16, 32, 32);
        p.setBrush(QColor(30, 140, 240));
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(knobRect);
        p.setPen(Qt::white);
        QFont fKnob = font();
        fKnob.setPointSize(8);
        fKnob.setBold(true);
        p.setFont(fKnob);
        p.drawText(knobRect, Qt::AlignCenter, "◀ ▶");

        // Before / After Labels
        QFont f = font();
        f.setPointSize(8);
        f.setBold(true);
        p.setFont(f);

        QRect lblBefore(viewRect.left() + 8, viewRect.top() + 8, 70, 20);
        p.fillRect(lblBefore, QColor(0, 0, 0, 160));
        p.setPen(Qt::white);
        p.drawText(lblBefore, Qt::AlignCenter, tr("BEFORE"));

        QRect lblAfter(viewRect.right() - 78, viewRect.top() + 8, 70, 20);
        p.fillRect(lblAfter, QColor(0, 0, 0, 160));
        p.setPen(Qt::white);
        p.drawText(lblAfter, Qt::AlignCenter, tr("AFTER"));
    }
    else {
        // Standard full viewport render
        if (!displayMod.isNull()) {
            p.drawImage(viewRect, displayMod);
        } else if (!m_originalImage.isNull()) {
            p.drawImage(viewRect, m_originalImage);
        }
    }

    // Draw Privacy selection rectangle if active
    if (!m_currentSelectionRect.isEmpty()) {
        p.setPen(QPen(QColor(0, 180, 255), 2, Qt::DashLine));
        p.setBrush(QColor(0, 180, 255, 45));
        p.drawRect(m_currentSelectionRect);
    }
}

void PhotoStudioViewport::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_eyedropperActive) {
            QPoint imgPt = widgetToImage(event->pos());
            if (imgPt.x() >= 0) {
                emit eyedropperSampled(imgPt);
            }
            return;
        }

        if (m_fourWayActive) {
            QRect viewRect = imageViewportRect();
            if (viewRect.contains(event->pos())) {
                int halfW = viewRect.width() / 2;
                int halfH = viewRect.height() / 2;
                bool isRight = (event->pos().x() >= viewRect.left() + halfW);
                bool isBottom = (event->pos().y() >= viewRect.top() + halfH);
                int quadrant = (isBottom ? 2 : 0) + (isRight ? 1 : 0);
                emit fourWayQuadrantSelected(quadrant);
            }
            return;
        }

        if (m_privacyToolActive) {
            m_selectingRect = true;
            m_selectionStart = event->pos();
            m_currentSelectionRect = QRect(m_selectionStart, QSize());
            update();
            return;
        }

        if (m_splitViewEnabled) {
            QRect viewRect = imageViewportRect();
            int splitX = viewRect.left() + static_cast<int>(viewRect.width() * m_splitRatio);
            if (std::abs(event->pos().x() - splitX) < 18) {
                m_draggingSplit = true;
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PhotoStudioViewport::mouseMoveEvent(QMouseEvent *event)
{
    QRect viewRect = imageViewportRect();

    if (m_selectingRect && m_privacyToolActive) {
        m_currentSelectionRect = QRect(m_selectionStart, event->pos()).normalized();
        m_currentSelectionRect = m_currentSelectionRect.intersected(viewRect);
        update();
        return;
    }

    if (m_draggingSplit && m_splitViewEnabled && viewRect.width() > 0) {
        double ratio = static_cast<double>(event->pos().x() - viewRect.left()) / viewRect.width();
        m_splitRatio = qBound(0.05, ratio, 0.95);
        emit splitRatioChanged(m_splitRatio);
        update();
        return;
    }

    if (m_splitViewEnabled && !m_privacyToolActive) {
        int splitX = viewRect.left() + static_cast<int>(viewRect.width() * m_splitRatio);
        if (std::abs(event->pos().x() - splitX) < 18) {
            setCursor(Qt::SplitHCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PhotoStudioViewport::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_selectingRect) {
            m_selectingRect = false;
            QRect imgRect = selectedImageRect();
            if (!imgRect.isEmpty()) {
                emit selectionCompleted(imgRect);
            }
            update();
            return;
        }
        if (m_draggingSplit) {
            m_draggingSplit = false;
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void PhotoStudioViewport::resizeEvent(QResizeEvent *)
{
    update();
}

// ============================================================================
// 3. FilterCardWidget Implementation
// ============================================================================

FilterCardWidget::FilterCardWidget(ImageProcessor::FilterPreset preset,
                                   const QString &title,
                                   const QImage &thumbnail,
                                   QWidget *parent)
    : QWidget(parent)
    , m_preset(preset)
    , m_title(title)
    , m_thumbnail(thumbnail)
{
    setFixedSize(96, 92);
    setCursor(Qt::PointingHandCursor);
}

void FilterCardWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void FilterCardWidget::enterEvent(QEnterEvent *)
{
    m_hovered = true;
    emit hovered(m_preset);
    update();
}

void FilterCardWidget::leaveEvent(QEvent *)
{
    m_hovered = false;
    emit unhovered();
    update();
}

void FilterCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_preset);
    }
}

void FilterCardWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(2, 2, -2, -2);
    bool isDark = palette().windowText().color().value() > palette().window().color().value();

    // Background & Card Border
    QColor cardBg = isDark ? QColor(28, 31, 37) : palette().button().color();
    QColor borderColor = m_selected
        ? QColor(2, 132, 199)
        : (m_hovered ? QColor(2, 132, 199) : (isDark ? QColor(48, 52, 60) : palette().mid().color()));

    p.setPen(QPen(borderColor, m_selected ? 2 : 1));
    p.setBrush(cardBg);
    p.drawRoundedRect(r, 6, 6);

    // Thumbnail
    if (!m_thumbnail.isNull()) {
        QRect thumbRect(r.left() + 4, r.top() + 4, r.width() - 8, 58);
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(thumbRect, 4, 4);
        p.setClipPath(clip);
        p.drawImage(thumbRect, m_thumbnail);
        p.restore();
    }

    // Title
    QFont f = font();
    f.setPointSize(8);
    f.setBold(m_selected);
    p.setFont(f);
    p.setPen(m_selected ? QColor(2, 132, 199) : palette().text().color());
    QRect textRect(r.left() + 2, r.bottom() - 20, r.width() - 4, 18);
    p.drawText(textRect, Qt::AlignCenter, p.fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width()));
}

// ============================================================================
// 3b. PhotoSlider Implementation
// ============================================================================

PhotoSlider::PhotoSlider(Qt::Orientation orientation, int min, int max, int defaultVal, QWidget *parent)
    : QSlider(orientation, parent)
    , m_defaultVal(defaultVal)
{
    setRange(min, max);
    setValue(defaultVal);
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(22);
    setCursor(Qt::PointingHandCursor);
}

void PhotoSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int cy = height() / 2;
    const int handleRadius = 7;
    const int grooveLeft = handleRadius + 2;
    const int grooveRight = width() - (handleRadius + 2);
    const int grooveWidth = std::max(1, grooveRight - grooveLeft);

    const int range = maximum() - minimum();
    if (range <= 0) return;

    bool isDark = palette().windowText().color().value() > palette().window().color().value();
    QColor trackColor = isDark ? QColor(48, 54, 65) : QColor(210, 215, 222);
    QColor zeroTickColor = isDark ? QColor(110, 125, 145) : QColor(150, 160, 175);
    QColor activeColor = QColor(2, 132, 199); // Crisp blue that fits both light and dark

    // 1. Base groove track
    QRectF grooveRect(grooveLeft, cy - 2, grooveWidth, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(grooveRect, 2, 2);

    // Current handle X position
    const double valRatio = qBound(0.0, static_cast<double>(value() - minimum()) / range, 1.0);
    const int handleX = grooveLeft + static_cast<int>(std::round(valRatio * grooveWidth));

    // 2. Active fill & center tick notch
    if (minimum() < 0 && maximum() > 0) {
        // Bipolar slider: center zero reference
        const double zeroRatio = qBound(0.0, static_cast<double>(0 - minimum()) / range, 1.0);
        const int zeroX = grooveLeft + static_cast<int>(std::round(zeroRatio * grooveWidth));

        // Draw center tick notch (1.5px vertical line)
        p.setPen(QPen(zeroTickColor, 1.5));
        p.drawLine(zeroX, cy - 5, zeroX, cy + 5);

        // Active fill strictly from center to handle
        if (handleX > zeroX) {
            QRectF fillRect(zeroX, cy - 2, handleX - zeroX, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(activeColor);
            p.drawRoundedRect(fillRect, 1.5, 1.5);
        } else if (handleX < zeroX) {
            QRectF fillRect(handleX, cy - 2, zeroX - handleX, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(activeColor);
            p.drawRoundedRect(fillRect, 1.5, 1.5);
        }
    } else {
        // Unipolar slider: fill from left (0) to handle
        if (handleX > grooveLeft) {
            QRectF fillRect(grooveLeft, cy - 2, handleX - grooveLeft, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(activeColor);
            p.drawRoundedRect(fillRect, 1.5, 1.5);
        }
    }

    // 3. Draw Handle / Thumb
    if (isSliderDown() || m_hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(2, 132, 199, isSliderDown() ? 65 : 35));
        p.drawEllipse(QPoint(handleX, cy), 10, 10);
    }

    // Handle outer body
    QColor handleBorder = isSliderDown() ? activeColor : (m_hovered ? activeColor : (isDark ? QColor(148, 163, 184) : QColor(140, 150, 165)));
    QColor handleFill = isSliderDown() ? activeColor : (m_hovered ? Qt::white : (isDark ? QColor(241, 245, 249) : Qt::white));
    p.setPen(QPen(handleBorder, 1.5));
    p.setBrush(handleFill);
    p.drawEllipse(QPoint(handleX, cy), handleRadius - 1, handleRadius - 1);

    // Handle inner accent dot if value is non-default
    if (value() != m_defaultVal) {
        p.setPen(Qt::NoPen);
        p.setBrush(isSliderDown() ? Qt::white : activeColor);
        p.drawEllipse(QPoint(handleX, cy), 2, 2);
    }
}

void PhotoSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int handleRadius = 7;
        const int grooveLeft = handleRadius + 2;
        const int grooveRight = width() - (handleRadius + 2);
        const int grooveWidth = std::max(1, grooveRight - grooveLeft);
        const double ratio = qBound(0.0, static_cast<double>(event->pos().x() - grooveLeft) / grooveWidth, 1.0);
        const int newVal = minimum() + static_cast<int>(std::round(ratio * (maximum() - minimum())));
        setValue(newVal);
        setSliderDown(true);
        update();
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void PhotoSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (isSliderDown()) {
        const int handleRadius = 7;
        const int grooveLeft = handleRadius + 2;
        const int grooveRight = width() - (handleRadius + 2);
        const int grooveWidth = std::max(1, grooveRight - grooveLeft);
        const double ratio = qBound(0.0, static_cast<double>(event->pos().x() - grooveLeft) / grooveWidth, 1.0);
        const int newVal = minimum() + static_cast<int>(std::round(ratio * (maximum() - minimum())));
        setValue(newVal);
        update();
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void PhotoSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (isSliderDown()) {
        setSliderDown(false);
        update();
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

void PhotoSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setValue(m_defaultVal);
        update();
        event->accept();
        return;
    }
    QSlider::mouseDoubleClickEvent(event);
}

void PhotoSlider::wheelEvent(QWheelEvent *event)
{
    // Ignore wheel events on slider so mouse-wheel scrolls the parent panel smoothly
    event->ignore();
}

void PhotoSlider::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QSlider::enterEvent(event);
}

void PhotoSlider::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QSlider::leaveEvent(event);
}

// ============================================================================
// 4. PhotoStudioDialog Implementation
// ============================================================================

PhotoStudioDialog::PhotoStudioDialog(const QImage &originalImage, QWidget *parent)
    : QDialog(parent)
    , m_sourceImage(originalImage)
{
    setWindowTitle(tr("Photo Studio & Filters"));
    resize(1200, 780);
    setMinimumSize(940, 600);

    // Create fast preview base (scaled down to max 960px) for silky smooth 60fps adjustments
    if (!m_sourceImage.isNull()) {
        m_previewBase = (m_sourceImage.width() > 960 || m_sourceImage.height() > 960)
            ? m_sourceImage.scaled(960, 960, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : m_sourceImage;
    }

    setupUi();
    generateFilterThumbnails();
    updateProcessing(true);
}

PhotoStudioDialog::~PhotoStudioDialog() = default;

void PhotoStudioDialog::setupUi()
{
    setStyleSheet(QStringLiteral(R"(
        /* Primary Apply Button */
        QPushButton#btnApply {
            background-color: #0284c7;
            color: #ffffff;
            border-radius: 6px;
            padding: 6px 20px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton#btnApply:hover {
            background-color: #0369a1;
        }
        QPushButton#btnApply:pressed {
            background-color: #075985;
        }

        /* Standard Cancel Button */
        QPushButton#btnCancel {
            border-radius: 6px;
            padding: 6px 18px;
            font-size: 12px;
            font-weight: 500;
        }

        /* Reset Button */
        QPushButton#btnResetAll {
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton#btnResetAll:hover {
            color: #dc2626;
        }

        /* Active ToolButtons (Split, Compare, Eyedropper, 4-Way, AutoFix) */
        QToolButton:checked {
            background-color: #0284c7;
            color: #ffffff;
            font-weight: bold;
        }
    )"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Top Control Bar (Auto-Fix, Split-View, Compare, Reset)
    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_btnAutoFix = new QToolButton(this);
    m_btnAutoFix->setText(tr("🪄 Auto Fix"));
    m_btnAutoFix->setCheckable(true);
    m_btnAutoFix->setToolTip(tr("Toggle intelligent automatic scene optimization (non-destructive)"));
    m_btnAutoFix->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnAutoFix, &QToolButton::clicked, this, &PhotoStudioDialog::onAutoEnhanceClicked);
    topBar->addWidget(m_btnAutoFix);

    m_btnSplitView = new QToolButton(this);
    m_btnSplitView->setText(tr("↔ Before / After"));
    m_btnSplitView->setCheckable(true);
    m_btnSplitView->setToolTip(tr("Toggle split-screen slider to compare before and after"));
    m_btnSplitView->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnSplitView, &QToolButton::toggled, this, &PhotoStudioDialog::onSplitViewToggled);
    topBar->addWidget(m_btnSplitView);

    m_btnCompare = new QToolButton(this);
    m_btnCompare->setText(tr("👁 Hold to Compare"));
    m_btnCompare->setToolTip(tr("Press and hold (or hold Spacebar) to view the original image"));
    connect(m_btnCompare, &QToolButton::pressed, this, [this]() {
        if (m_viewport) m_viewport->setHoldCompareActive(true);
    });
    connect(m_btnCompare, &QToolButton::released, this, [this]() {
        if (m_viewport) m_viewport->setHoldCompareActive(false);
    });
    topBar->addWidget(m_btnCompare);

    topBar->addStretch(1);

    m_btnReset = new QPushButton(tr("↺ Reset All"), this);
    m_btnReset->setObjectName("btnResetAll");
    m_btnReset->setToolTip(tr("Reset all sliders, filters, and auto-enhancements to original"));
    connect(m_btnReset, &QPushButton::clicked, this, &PhotoStudioDialog::onResetAllClicked);
    topBar->addWidget(m_btnReset);

    mainLayout->addLayout(topBar);

    // 2. Central Area: Viewport on Left (70%) + Sidebar on Right (30%)
    auto *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(10);

    m_viewport = new PhotoStudioViewport(this);
    m_viewport->setOriginalImage(m_previewBase);
    connect(m_viewport, &PhotoStudioViewport::eyedropperSampled, this, &PhotoStudioDialog::onEyedropperSampled);
    connect(m_viewport, &PhotoStudioViewport::fourWayQuadrantSelected, this, &PhotoStudioDialog::onFourWayQuadrantSelected);
    centerLayout->addWidget(m_viewport, 7);

    // Right Sidebar (415px width ensures all 6 tabs fit seamlessly without truncation or scroll buttons)
    auto *sidebarWidget = new QWidget(this);
    sidebarWidget->setFixedWidth(415);
    auto *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(8);

    // A. Histogram
    m_histogram = new HistogramWidget(this);
    sidebarLayout->addWidget(m_histogram);

    // B. Tabbed Tools
    m_tabs = new QTabWidget(this);
    m_tabs->setUsesScrollButtons(false);

    // Tab 0: Auto-Fix (Clean compact layout that fits completely without forced scrolling)
    auto *scrollAuto = new QScrollArea(m_tabs);
    scrollAuto->setWidgetResizable(true);
    scrollAuto->setFrameShape(QFrame::NoFrame);
    scrollAuto->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *tabAuto = new QWidget();
    auto *autoLayout = new QVBoxLayout(tabAuto);
    autoLayout->setContentsMargins(4, 4, 4, 4);
    autoLayout->setSpacing(8);

    // Group 1: Master Enable, Intensity & Profile
    auto *boxMaster = new QGroupBox(tr("Intelligent Scene Optimization"), tabAuto);
    auto *masterLay = new QVBoxLayout(boxMaster);
    masterLay->setContentsMargins(10, 12, 10, 10);
    masterLay->setSpacing(8);

    m_chkAutoFixEnable = new QCheckBox(tr("Enable Intelligent Auto-Enhance"), boxMaster);
    m_chkAutoFixEnable->setStyleSheet("font-weight: bold; color: #0284c7; font-size: 12px;");
    connect(m_chkAutoFixEnable, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_btnAutoFix) m_btnAutoFix->setChecked(checked);
        m_autoCacheValid = false;
        updateProcessing(true);
    });
    masterLay->addWidget(m_chkAutoFixEnable);

    auto *autoIntLay = new QHBoxLayout();
    autoIntLay->setContentsMargins(0, 0, 0, 0);
    autoIntLay->setSpacing(8);
    auto *lblAutoEff = new QLabel(tr("Effect Strength:"), boxMaster);
    lblAutoEff->setFixedWidth(112);
    lblAutoEff->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lblAutoEff->setStyleSheet("font-size: 12px; font-weight: 500;");

    m_lblAutoIntensity = new QLabel("100%", boxMaster);
    m_lblAutoIntensity->setFixedWidth(38);
    m_lblAutoIntensity->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblAutoIntensity->setStyleSheet("color: #0284c7; font-size: 12px; font-family: monospace; font-weight: bold;");

    m_sliderAutoIntensity = new PhotoSlider(Qt::Horizontal, 0, 150, 100, boxMaster);
    m_sliderAutoIntensity->setToolTip(tr("Double-click to reset to default (100%)"));
    connect(m_sliderAutoIntensity, &QSlider::valueChanged, this, [this](int val) {
        if (m_lblAutoIntensity) m_lblAutoIntensity->setText(QString::number(val) + "%");
        updateProcessing(false);
    });
    autoIntLay->addWidget(lblAutoEff);
    autoIntLay->addWidget(m_sliderAutoIntensity, 1);
    autoIntLay->addWidget(m_lblAutoIntensity);
    masterLay->addLayout(autoIntLay);

    auto *profLay = new QHBoxLayout();
    profLay->setContentsMargins(0, 0, 0, 0);
    profLay->setSpacing(8);
    auto *lblSceneProf = new QLabel(tr("Scene Profile:"), boxMaster);
    lblSceneProf->setFixedWidth(112);
    lblSceneProf->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lblSceneProf->setStyleSheet("font-size: 12px; font-weight: 500;");

    m_comboAutoProfile = new QComboBox(boxMaster);
    m_comboAutoProfile->addItem(tr("🌿 Natural & Balanced"), 0);
    m_comboAutoProfile->addItem(tr("🌅 Vivid & Landscapes"), 1);
    m_comboAutoProfile->addItem(tr("👥 Backlight & Shadows"), 2);
    m_comboAutoProfile->addItem(tr("🌫️ Dehaze & Atmospheric"), 3);
    connect(m_comboAutoProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_autoCacheValid = false;
        updateProcessing(true);
    });
    auto *spacerProf = new QWidget(boxMaster);
    spacerProf->setFixedWidth(38);
    profLay->addWidget(lblSceneProf);
    profLay->addWidget(m_comboAutoProfile, 1);
    profLay->addWidget(spacerProf);
    masterLay->addLayout(profLay);

    autoLayout->addWidget(boxMaster);

    // Group 2: Algorithm Protections and Tuning (Compact 2-Column Grid)
    auto *boxTuning = new QGroupBox(tr("Algorithm Protections and Tuning"), tabAuto);
    auto *tuningGrid = new QGridLayout(boxTuning);
    tuningGrid->setContentsMargins(10, 12, 10, 10);
    tuningGrid->setHorizontalSpacing(14);
    tuningGrid->setVerticalSpacing(6);

    auto addTuningCheck = [&](const QString &label, const QString &tip, bool defChecked, int row, int col) -> QCheckBox* {
        auto *chk = new QCheckBox(label, boxTuning);
        chk->setChecked(defChecked);
        if (!tip.isEmpty()) chk->setToolTip(tip);
        connect(chk, &QCheckBox::toggled, this, [this](bool) {
            m_autoCacheValid = false;
            updateProcessing(false);
        });
        tuningGrid->addWidget(chk, row, col);
        return chk;
    };

    m_chkProtectHighlights = addTuningCheck(tr("Protect Highlights"),
                                            tr("Excludes bright light fixtures and applies smooth compression so blown lights never clip"), true, 0, 0);
    m_chkProtectSkin       = addTuningCheck(tr("Protect Skin Tones"),
                                            tr("Ensures human skin retains healthy peach vitality without turning cold or cyan"), true, 0, 1);

    m_chkRecoverShadows    = addTuningCheck(tr("Adaptive Shadows"),
                                            tr("Recovers shadow details for backlit portraits and dark scenes"), true, 1, 0);
    m_chkDenoiseShadows    = addTuningCheck(tr("Clean Shadow Noise"),
                                            tr("Subtle noise reduction in deep lifted dark areas"), true, 1, 1);

    m_chkNeutralizeWb      = addTuningCheck(tr("Neutralize Casts"),
                                            tr("Neutralizes tungsten, yellow, or cold color casts"), true, 2, 0);
    m_chkCenterWeighted    = addTuningCheck(tr("Center-Weighted"),
                                            tr("Gives higher exposure priority to the center subject"), true, 2, 1);

    autoLayout->addWidget(boxTuning);

    // Group 3: Interactive Calibration and Compare
    auto *boxTools = new QGroupBox(tr("Interactive Calibration and Compare"), tabAuto);
    auto *toolsLay = new QVBoxLayout(boxTools);
    toolsLay->setContentsMargins(10, 12, 10, 10);
    toolsLay->setSpacing(6);

    auto *toolBtnLay = new QHBoxLayout();
    toolBtnLay->setSpacing(8);

    m_btnEyedropper = new QToolButton(boxTools);
    m_btnEyedropper->setObjectName("btnStudioTool");
    m_btnEyedropper->setText(tr("💧 Neutral Eyedropper"));
    m_btnEyedropper->setCheckable(true);
    m_btnEyedropper->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnEyedropper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnEyedropper->setToolTip(tr("Click, then sample a white or gray area in the photo to calibrate color temperature"));
    connect(m_btnEyedropper, &QToolButton::toggled, this, &PhotoStudioDialog::onEyedropperToggled);
    toolBtnLay->addWidget(m_btnEyedropper);

    m_btnFourWay = new QToolButton(boxTools);
    m_btnFourWay->setObjectName("btnStudioTool");
    m_btnFourWay->setText(tr("⊞ 4-Way Compare"));
    m_btnFourWay->setCheckable(true);
    m_btnFourWay->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnFourWay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnFourWay->setToolTip(tr("Simultaneously compare Original, Natural, Vivid, and Backlight profiles in 4 quadrants"));
    connect(m_btnFourWay, &QToolButton::toggled, this, &PhotoStudioDialog::onFourWayCompareToggled);
    toolBtnLay->addWidget(m_btnFourWay);
    toolsLay->addLayout(toolBtnLay);

    m_lblEyedropperStatus = new QLabel(tr("Tip: Use Eyedropper to click on a white/grey object for instant neutral point calibration."), boxTools);
    m_lblEyedropperStatus->setWordWrap(true);
    m_lblEyedropperStatus->setStyleSheet("font-size: 11px; color: gray;");
    toolsLay->addWidget(m_lblEyedropperStatus);

    autoLayout->addWidget(boxTools);
    autoLayout->addStretch(1);

    scrollAuto->setWidget(tabAuto);
    m_tabs->addTab(scrollAuto, tr("Auto-Fix"));
    m_tabs->setTabToolTip(0, tr("Intelligent Auto-Enhance & Scene Optimizer"));

    // Tab 1: 🎨 Filters & Presets
    auto *tabFilters = new QWidget();
    auto *filtersLayout = new QVBoxLayout(tabFilters);
    filtersLayout->setContentsMargins(6, 6, 6, 6);
    filtersLayout->setSpacing(8);

    // Intensity Slider
    auto *intensityLayout = new QHBoxLayout();
    intensityLayout->setContentsMargins(0, 0, 0, 0);
    intensityLayout->setSpacing(8);
    auto *lblFilterEff = new QLabel(tr("Filter Strength:"), tabFilters);
    lblFilterEff->setFixedWidth(112);
    lblFilterEff->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lblFilterEff->setStyleSheet("font-size: 12px; font-weight: 500;");

    m_lblIntensity = new QLabel("100%", tabFilters);
    m_lblIntensity->setFixedWidth(38);
    m_lblIntensity->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblIntensity->setStyleSheet("color: #0284c7; font-size: 12px; font-family: monospace; font-weight: bold;");

    m_sliderIntensity = new PhotoSlider(Qt::Horizontal, 0, 100, 100, tabFilters);
    m_sliderIntensity->setToolTip(tr("Double-click to reset to default (100%)"));
    connect(m_sliderIntensity, &QSlider::valueChanged, this, &PhotoStudioDialog::onFilterIntensityChanged);
    intensityLayout->addWidget(lblFilterEff);
    intensityLayout->addWidget(m_sliderIntensity, 1);
    intensityLayout->addWidget(m_lblIntensity);
    filtersLayout->addLayout(intensityLayout);

    // Scrollable Filter Cards Grid
    auto *scrollFilters = new QScrollArea(tabFilters);
    scrollFilters->setWidgetResizable(true);
    scrollFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *cardsContainer = new QWidget();
    auto *cardsGrid = new QGridLayout(cardsContainer);
    cardsGrid->setContentsMargins(4, 4, 4, 4);
    cardsGrid->setSpacing(6);

    const QVector<ImageProcessor::FilterPreset> presets = {
        ImageProcessor::FilterPreset::Original,
        ImageProcessor::FilterPreset::TealAndOrange,
        ImageProcessor::FilterPreset::VintageSepia,
        ImageProcessor::FilterPreset::RetroFilm,
        ImageProcessor::FilterPreset::FilmNoir,
        ImageProcessor::FilterPreset::RedFilterBW,
        ImageProcessor::FilterPreset::OrangeFilterBW,
        ImageProcessor::FilterPreset::GoldenHour,
        ImageProcessor::FilterPreset::NordicCool,
        ImageProcessor::FilterPreset::CrossProcess,
        ImageProcessor::FilterPreset::Cyberpunk,
        ImageProcessor::FilterPreset::DocumentEnhance
    };

    int row = 0, col = 0;
    for (auto preset : presets) {
        auto *card = new FilterCardWidget(preset, ImageProcessor::presetDisplayName(preset), QImage(), cardsContainer);
        connect(card, &FilterCardWidget::clicked, this, &PhotoStudioDialog::onFilterCardClicked);
        connect(card, &FilterCardWidget::hovered, this, &PhotoStudioDialog::onFilterCardHovered);
        connect(card, &FilterCardWidget::unhovered, this, &PhotoStudioDialog::onFilterCardUnhovered);

        if (preset == ImageProcessor::FilterPreset::Original) {
            card->setSelected(true);
        }

        cardsGrid->addWidget(card, row, col);
        m_filterCards.append(card);

        if (++col >= 3) {
            col = 0;
            ++row;
        }
    }
    scrollFilters->setWidget(cardsContainer);
    filtersLayout->addWidget(scrollFilters);
    m_tabs->addTab(tabFilters, tr("Filters"));
    m_tabs->setTabToolTip(1, tr("Photo Color & Film Presets"));

    // Helper lambda to make slider rows with guaranteed pixel-perfect alignment
    auto addSliderRow = [&](QVBoxLayout *parentLayout, const QString &label, int min, int max, int def) -> PhotoSlider* {
        auto *hl = new QHBoxLayout();
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);

        auto *lblTitle = new QLabel(label);
        lblTitle->setFixedWidth(112);
        lblTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lblTitle->setStyleSheet("font-size: 12px; font-weight: 500;");

        auto *lblVal = new QLabel(QString::number(def));
        lblVal->setFixedWidth(38);
        lblVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblVal->setStyleSheet(def == 0
            ? "color: gray; font-size: 12px; font-family: monospace;"
            : "color: #0284c7; font-size: 12px; font-family: monospace; font-weight: bold;");

        auto *slider = new PhotoSlider(Qt::Horizontal, min, max, def);
        slider->setToolTip(tr("Double-click to reset to default"));
        connect(slider, &QSlider::valueChanged, this, [this, lblVal](int val) {
            lblVal->setText(QString::number(val));
            lblVal->setStyleSheet(val == 0
                ? "color: gray; font-size: 12px; font-family: monospace;"
                : "color: #0284c7; font-size: 12px; font-family: monospace; font-weight: bold;");
            onManualValuesChanged();
        });
        hl->addWidget(lblTitle);
        hl->addWidget(slider, 1);
        hl->addWidget(lblVal);
        parentLayout->addLayout(hl);
        return slider;
    };

    // Tab 2: Light & Tone
    auto *tabLight = new QWidget();
    auto *lightLayout = new QVBoxLayout(tabLight);
    lightLayout->setContentsMargins(14, 16, 14, 14);
    lightLayout->setSpacing(14);
    m_sliderBrightness = addSliderRow(lightLayout, tr("Brightness"), -100, 100, 0);
    m_sliderContrast   = addSliderRow(lightLayout, tr("Contrast"), -100, 100, 0);
    m_sliderHighlights = addSliderRow(lightLayout, tr("Highlights"), -100, 100, 0);
    m_sliderShadows    = addSliderRow(lightLayout, tr("Shadows"), -100, 100, 0);
    lightLayout->addStretch(1);
    m_tabs->addTab(tabLight, tr("Light"));
    m_tabs->setTabToolTip(2, tr("Light, Brightness & Contrast"));

    // Tab 3: Color Balance
    auto *tabColor = new QWidget();
    auto *colorLayout = new QVBoxLayout(tabColor);
    colorLayout->setContentsMargins(14, 16, 14, 14);
    colorLayout->setSpacing(12);
    m_sliderTemperature = addSliderRow(colorLayout, tr("Temp (Kelvin)"), -100, 100, 0);
    m_sliderSaturation  = addSliderRow(colorLayout, tr("Saturation"), -100, 100, 0);
    m_sliderHue         = addSliderRow(colorLayout, tr("Hue Shift"), -180, 180, 0);
    m_sliderRed         = addSliderRow(colorLayout, tr("Red Gain"), -100, 100, 0);
    m_sliderGreen       = addSliderRow(colorLayout, tr("Green Gain"), -100, 100, 0);
    m_sliderBlue        = addSliderRow(colorLayout, tr("Blue Gain"), -100, 100, 0);
    colorLayout->addStretch(1);
    m_tabs->addTab(tabColor, tr("Color"));
    m_tabs->setTabToolTip(3, tr("Color Balance & Temperature"));

    // Tab 4: Detail & Effects
    auto *tabDetail = new QWidget();
    auto *detailLayout = new QVBoxLayout(tabDetail);
    detailLayout->setContentsMargins(14, 16, 14, 14);
    detailLayout->setSpacing(12);
    m_sliderSharpen  = addSliderRow(detailLayout, tr("Sharpen"), 0, 100, 0);
    m_sliderBlur     = addSliderRow(detailLayout, tr("Soft Blur"), 0, 30, 0);
    m_sliderVignette = addSliderRow(detailLayout, tr("Vignette"), -100, 100, 0);
    m_chkInvert      = new QCheckBox(tr("Invert Colors (Negative)"), tabDetail);
    m_chkInvert->setStyleSheet("font-size: 12px; font-weight: 500; padding: 4px 0;");
    connect(m_chkInvert, &QCheckBox::toggled, this, &PhotoStudioDialog::onManualValuesChanged);
    detailLayout->addWidget(m_chkInvert);
    detailLayout->addStretch(1);
    m_tabs->addTab(tabDetail, tr("Detail"));
    m_tabs->setTabToolTip(4, tr("Sharpness, Blur & Effects"));

    // Tab 5: Privacy Redaction
    auto *tabPrivacy = new QWidget();
    auto *privacyLayout = new QVBoxLayout(tabPrivacy);
    privacyLayout->setContentsMargins(14, 16, 14, 14);
    privacyLayout->setSpacing(12);

    auto *lblPrivacyDesc = new QLabel(tr("Drag a selection rectangle on the image to redact sensitive data (e.g. license plates, faces, document text):"), tabPrivacy);
    lblPrivacyDesc->setWordWrap(true);
    lblPrivacyDesc->setStyleSheet("color: gray; font-size: 12px; line-height: 1.4;");
    privacyLayout->addWidget(lblPrivacyDesc);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    m_btnPrivacyPixelate = new QToolButton(tabPrivacy);
    m_btnPrivacyPixelate->setText(tr("▦ Pixelate"));
    m_btnPrivacyPixelate->setCheckable(true);
    m_btnPrivacyPixelate->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnPrivacyPixelate, &QToolButton::toggled, this, [this](bool checked) {
        if (checked && m_btnPrivacyBlur) m_btnPrivacyBlur->setChecked(false);
        if (m_viewport) m_viewport->setPrivacyToolActive(checked);
    });
    btnRow->addWidget(m_btnPrivacyPixelate);

    m_btnPrivacyBlur = new QToolButton(tabPrivacy);
    m_btnPrivacyBlur->setText(tr("💧 Blur"));
    m_btnPrivacyBlur->setCheckable(true);
    m_btnPrivacyBlur->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnPrivacyBlur, &QToolButton::toggled, this, [this](bool checked) {
        if (checked && m_btnPrivacyPixelate) m_btnPrivacyPixelate->setChecked(false);
        if (m_viewport) m_viewport->setPrivacyToolActive(checked);
    });
    btnRow->addWidget(m_btnPrivacyBlur);
    privacyLayout->addLayout(btnRow);

    m_lblPrivacyStatus = new QLabel(tabPrivacy);
    m_lblPrivacyStatus->setStyleSheet("color: #0284c7; font-size: 11px; font-weight: 500;");
    privacyLayout->addWidget(m_lblPrivacyStatus);

    connect(m_viewport, &PhotoStudioViewport::selectionCompleted, this, [this](const QRect &r) {
        if (m_previewBase.isNull() || m_sourceImage.isNull()) return;

        double sx = static_cast<double>(m_sourceImage.width()) / m_previewBase.width();
        double sy = static_cast<double>(m_sourceImage.height()) / m_previewBase.height();
        QRect fullRect(static_cast<int>(r.x() * sx),
                       static_cast<int>(r.y() * sy),
                       static_cast<int>(r.width() * sx),
                       static_cast<int>(r.height() * sy));

        if (m_btnPrivacyPixelate && m_btnPrivacyPixelate->isChecked()) {
            m_previewBase = ImageProcessor::pixelateRegion(m_previewBase, r, 16);
            m_sourceImage = ImageProcessor::pixelateRegion(m_sourceImage, fullRect, static_cast<int>(16 * sx));
            if (m_viewport) m_viewport->setOriginalImage(m_previewBase);
            generateFilterThumbnails();
            m_autoCacheValid = false;
            updateProcessing(true);
        } else if (m_btnPrivacyBlur && m_btnPrivacyBlur->isChecked()) {
            m_previewBase = ImageProcessor::blurRegion(m_previewBase, r, 14);
            m_sourceImage = ImageProcessor::blurRegion(m_sourceImage, fullRect, static_cast<int>(14 * sx));
            if (m_viewport) m_viewport->setOriginalImage(m_previewBase);
            generateFilterThumbnails();
            m_autoCacheValid = false;
            updateProcessing(true);
        }
    });

    privacyLayout->addStretch(1);
    m_tabs->addTab(tabPrivacy, tr("Privacy"));
    m_tabs->setTabToolTip(5, tr("Privacy Redaction (Blur / Mosaic)"));

    sidebarLayout->addWidget(m_tabs, 1);
    centerLayout->addWidget(sidebarWidget, 3);
    mainLayout->addLayout(centerLayout, 1);

    // 3. Bottom Dialog Buttons (Cancel, Apply Changes)
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);
    bottomRow->setSpacing(10);
    bottomRow->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setObjectName("btnCancel");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(m_btnCancel);

    m_btnApply = new QPushButton(tr("✓ Apply Changes"), this);
    m_btnApply->setObjectName("btnApply");
    m_btnApply->setDefault(true);
    connect(m_btnApply, &QPushButton::clicked, this, [this]() {
        // Render at full resolution using exact committed settings
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QImage full = m_sourceImage;

        // 0. Auto-Fix (non-destructive)
        if (m_chkAutoFixEnable && m_chkAutoFixEnable->isChecked()) {
            full = ImageProcessor::autoEnhance(full, currentAutoOptions());
        }

        // 1. Preset filter + intensity
        if (m_currentPreset != ImageProcessor::FilterPreset::Original) {
            QImage filtered = ImageProcessor::applyPreset(full, m_currentPreset);
            double intensity = m_sliderIntensity ? (m_sliderIntensity->value() / 100.0) : 1.0;
            full = ImageProcessor::blend(full, filtered, intensity);
        }

        // 2. Light & Color adjustments
        full = ImageProcessor::adjust(full,
                                      m_sliderBrightness ? m_sliderBrightness->value() : 0,
                                      m_sliderContrast ? m_sliderContrast->value() : 0,
                                      m_sliderSaturation ? m_sliderSaturation->value() : 0,
                                      m_sliderHue ? m_sliderHue->value() : 0,
                                      m_sliderTemperature ? m_sliderTemperature->value() : 0,
                                      m_sliderRed ? m_sliderRed->value() : 0,
                                      m_sliderGreen ? m_sliderGreen->value() : 0,
                                      m_sliderBlue ? m_sliderBlue->value() : 0,
                                      m_sliderHighlights ? m_sliderHighlights->value() : 0,
                                      m_sliderShadows ? m_sliderShadows->value() : 0);

        // 3. Sharpen, Blur, Vignette, Invert
        if (m_sliderSharpen && m_sliderSharpen->value() > 0) {
            full = ImageProcessor::applySharpen(full, m_sliderSharpen->value());
        }
        if (m_sliderBlur && m_sliderBlur->value() > 0) {
            full = ImageProcessor::applyBlur(full, m_sliderBlur->value());
        }
        if (m_sliderVignette && m_sliderVignette->value() != 0) {
            full = ImageProcessor::applyVignette(full, m_sliderVignette->value(), 80);
        }
        if (m_chkInvert && m_chkInvert->isChecked()) {
            full = ImageProcessor::applyInvert(full);
        }

        m_finalImage = full;
        QApplication::restoreOverrideCursor();
        accept();
    });
    bottomRow->addWidget(m_btnApply);

    mainLayout->addLayout(bottomRow);
}

void PhotoStudioDialog::generateFilterThumbnails()
{
    if (m_previewBase.isNull()) return;

    // Fast tiny thumbnail (88x58) for cards
    QImage tiny = m_previewBase.scaled(88, 58, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QRect cropRect((tiny.width() - 88) / 2, (tiny.height() - 58) / 2, 88, 58);
    tiny = tiny.copy(cropRect);

    for (auto *card : m_filterCards) {
        QImage thumb = ImageProcessor::applyPreset(tiny, card->preset());
        card->setThumbnail(thumb);
        card->setSelected(card->preset() == m_currentPreset);
    }
}

void PhotoStudioDialog::onFilterCardClicked(ImageProcessor::FilterPreset preset)
{
    m_currentPreset = preset;
    for (auto *card : m_filterCards) {
        card->setSelected(card->preset() == preset);
    }
    if (m_viewport) {
        m_viewport->clearHoverImage();
    }
    updateProcessing(true);
}

void PhotoStudioDialog::onFilterCardHovered(ImageProcessor::FilterPreset preset)
{
    if (!m_viewport || preset == m_currentPreset) return;

    // Instant hover preview on viewport
    QImage hoverImg = ImageProcessor::applyPreset(m_previewBase, preset);
    double intensity = m_sliderIntensity ? (m_sliderIntensity->value() / 100.0) : 1.0;
    hoverImg = ImageProcessor::blend(m_previewBase, hoverImg, intensity);

    m_viewport->setHoverImage(hoverImg);
}

void PhotoStudioDialog::onFilterCardUnhovered()
{
    if (m_viewport) {
        m_viewport->clearHoverImage();
    }
}

void PhotoStudioDialog::onFilterIntensityChanged(int value)
{
    if (m_lblIntensity) {
        m_lblIntensity->setText(QString::number(value) + "%");
    }
    updateProcessing(false);
}

void PhotoStudioDialog::onManualValuesChanged()
{
    if (m_updatingControls) return;
    updateProcessing(false);
}

ImageProcessor::AutoEnhanceOptions PhotoStudioDialog::currentAutoOptions() const
{
    ImageProcessor::AutoEnhanceOptions opts;
    if (m_sliderAutoIntensity) {
        opts.intensity = m_sliderAutoIntensity->value();
    }
    if (m_comboAutoProfile) {
        opts.profile = static_cast<ImageProcessor::AutoEnhanceOptions::Profile>(m_comboAutoProfile->currentIndex());
    }
    if (m_chkProtectHighlights) {
        opts.protectHighlights = m_chkProtectHighlights->isChecked();
    }
    if (m_chkRecoverShadows) {
        opts.recoverShadows = m_chkRecoverShadows->isChecked();
    }
    if (m_chkNeutralizeWb) {
        opts.correctWhiteBalance = m_chkNeutralizeWb->isChecked();
    }
    if (m_chkProtectSkin) {
        opts.protectSkinTones = m_chkProtectSkin->isChecked();
    }
    if (m_chkDenoiseShadows) {
        opts.autoDenoiseShadows = m_chkDenoiseShadows->isChecked();
    }
    if (m_chkCenterWeighted) {
        opts.centerWeighted = m_chkCenterWeighted->isChecked();
    }
    opts.neutralSamplePoint = m_neutralSamplePoint;
    return opts;
}

void PhotoStudioDialog::onAutoEnhanceClicked()
{
    // Toggle state: clicking Auto Fix toggles enable/disable non-destructively
    bool enabled = m_btnAutoFix && m_btnAutoFix->isChecked();
    if (m_chkAutoFixEnable) {
        m_chkAutoFixEnable->setChecked(enabled);
    }
    if (enabled && m_tabs) {
        m_tabs->setCurrentIndex(0); // Switch to Auto-Fix tab for fine-tuning
    }
    m_autoCacheValid = false;
    updateProcessing(true);
}

void PhotoStudioDialog::onEyedropperToggled(bool checked)
{
    if (m_viewport) {
        m_viewport->setEyedropperActive(checked);
    }
    if (m_lblEyedropperStatus) {
        m_lblEyedropperStatus->setText(checked
            ? tr("Eyedropper active: click on a neutral white or gray spot in the photo...")
            : (m_neutralSamplePoint.x() >= 0
                ? tr("Neutral sample point calibrated at (%1, %2)").arg(m_neutralSamplePoint.x()).arg(m_neutralSamplePoint.y())
                : tr("Tip: Use Eyedropper to click on a white or gray object for manual neutral point calibration.")));
    }
}

void PhotoStudioDialog::onEyedropperSampled(const QPoint &pt)
{
    m_neutralSamplePoint = pt;
    m_autoCacheValid = false;
    if (m_btnEyedropper) {
        m_btnEyedropper->setChecked(false);
    }
    if (m_viewport) {
        m_viewport->setEyedropperActive(false);
    }
    if (m_lblEyedropperStatus) {
        m_lblEyedropperStatus->setText(tr("Calibrated to sample point (%1, %2). White balance neutral point locked.").arg(pt.x()).arg(pt.y()));
    }
    if (m_chkAutoFixEnable && !m_chkAutoFixEnable->isChecked()) {
        m_chkAutoFixEnable->setChecked(true);
    }
    updateProcessing(true);
}

void PhotoStudioDialog::onFourWayCompareToggled(bool checked)
{
    if (!m_viewport) return;
    if (checked && !m_previewBase.isNull()) {
        ImageProcessor::AutoEnhanceOptions optNat = currentAutoOptions();
        optNat.profile = ImageProcessor::AutoEnhanceOptions::Profile::Natural;
        QImage imgNat = ImageProcessor::autoEnhance(m_previewBase, optNat);

        ImageProcessor::AutoEnhanceOptions optViv = currentAutoOptions();
        optViv.profile = ImageProcessor::AutoEnhanceOptions::Profile::Vivid;
        QImage imgViv = ImageProcessor::autoEnhance(m_previewBase, optViv);

        ImageProcessor::AutoEnhanceOptions optShad = currentAutoOptions();
        optShad.profile = ImageProcessor::AutoEnhanceOptions::Profile::BacklightShadows;
        QImage imgShad = ImageProcessor::autoEnhance(m_previewBase, optShad);

        m_viewport->setFourWayCompare(true, m_previewBase, imgNat, imgViv, imgShad);
    } else {
        m_viewport->setFourWayCompare(false);
        updateProcessing(true);
    }
}

void PhotoStudioDialog::onFourWayQuadrantSelected(int quadrantIndex)
{
    if (m_btnFourWay) {
        m_btnFourWay->setChecked(false);
    }
    if (m_viewport) {
        m_viewport->setFourWayCompare(false);
    }
    m_autoCacheValid = false;
    if (quadrantIndex == 0) {
        // Original chosen
        if (m_chkAutoFixEnable) m_chkAutoFixEnable->setChecked(false);
    } else {
        if (m_chkAutoFixEnable) m_chkAutoFixEnable->setChecked(true);
        if (m_comboAutoProfile) {
            if (quadrantIndex == 1) m_comboAutoProfile->setCurrentIndex(0); // Natural
            else if (quadrantIndex == 2) m_comboAutoProfile->setCurrentIndex(1); // Vivid
            else if (quadrantIndex == 3) m_comboAutoProfile->setCurrentIndex(2); // Backlight & Shadows
        }
    }
    updateProcessing(true);
}

void PhotoStudioDialog::onSplitViewToggled(bool checked)
{
    if (m_viewport) {
        m_viewport->setSplitViewEnabled(checked);
    }
}

void PhotoStudioDialog::onResetAllClicked()
{
    m_updatingControls = true;
    m_autoCacheValid = false;

    // Reset Auto-Fix
    if (m_chkAutoFixEnable) m_chkAutoFixEnable->setChecked(false);
    if (m_btnAutoFix) m_btnAutoFix->setChecked(false);
    if (m_sliderAutoIntensity) m_sliderAutoIntensity->setValue(100);
    if (m_lblAutoIntensity) m_lblAutoIntensity->setText("100%");
    if (m_comboAutoProfile) m_comboAutoProfile->setCurrentIndex(0);
    if (m_chkProtectHighlights) m_chkProtectHighlights->setChecked(true);
    if (m_chkRecoverShadows) m_chkRecoverShadows->setChecked(true);
    if (m_chkNeutralizeWb) m_chkNeutralizeWb->setChecked(true);
    if (m_chkProtectSkin) m_chkProtectSkin->setChecked(true);
    if (m_chkDenoiseShadows) m_chkDenoiseShadows->setChecked(true);
    if (m_chkCenterWeighted) m_chkCenterWeighted->setChecked(true);
    m_neutralSamplePoint = QPoint(-1, -1);
    if (m_btnEyedropper) m_btnEyedropper->setChecked(false);
    if (m_btnFourWay) m_btnFourWay->setChecked(false);
    if (m_lblEyedropperStatus) {
        m_lblEyedropperStatus->setText(tr("Tip: Use Eyedropper to click on a white or gray object for manual neutral point calibration."));
    }

    // Reset Filter Preset
    m_currentPreset = ImageProcessor::FilterPreset::Original;
    for (auto *card : m_filterCards) {
        card->setSelected(card->preset() == m_currentPreset);
    }

    // Reset Sliders
    if (m_sliderIntensity) m_sliderIntensity->setValue(100);
    if (m_sliderBrightness) m_sliderBrightness->setValue(0);
    if (m_sliderContrast) m_sliderContrast->setValue(0);
    if (m_sliderHighlights) m_sliderHighlights->setValue(0);
    if (m_sliderShadows) m_sliderShadows->setValue(0);

    if (m_sliderTemperature) m_sliderTemperature->setValue(0);
    if (m_sliderSaturation) m_sliderSaturation->setValue(0);
    if (m_sliderHue) m_sliderHue->setValue(0);
    if (m_sliderRed) m_sliderRed->setValue(0);
    if (m_sliderGreen) m_sliderGreen->setValue(0);
    if (m_sliderBlue) m_sliderBlue->setValue(0);

    if (m_sliderSharpen) m_sliderSharpen->setValue(0);
    if (m_sliderBlur) m_sliderBlur->setValue(0);
    if (m_sliderVignette) m_sliderVignette->setValue(0);
    if (m_chkInvert) m_chkInvert->setChecked(false);

    if (m_viewport) {
        m_viewport->setFourWayCompare(false);
        m_viewport->setEyedropperActive(false);
    }

    m_updatingControls = false;
    updateProcessing(true);
}

void PhotoStudioDialog::onApplyPrivacyRedaction(const QString &)
{
}

void PhotoStudioDialog::updateProcessing(bool updateHist)
{
    if (m_previewBase.isNull() || !m_viewport) return;

    QImage processed = m_previewBase;

    // 0. Auto-Fix Stage (non-destructive with instant 60fps caching)
    if (m_chkAutoFixEnable && m_chkAutoFixEnable->isChecked()) {
        ImageProcessor::AutoEnhanceOptions opts = currentAutoOptions();

        bool optionsMatch = m_autoCacheValid
            && (m_cachedAutoOptions.profile == opts.profile)
            && (m_cachedAutoOptions.protectHighlights == opts.protectHighlights)
            && (m_cachedAutoOptions.recoverShadows == opts.recoverShadows)
            && (m_cachedAutoOptions.correctWhiteBalance == opts.correctWhiteBalance)
            && (m_cachedAutoOptions.protectSkinTones == opts.protectSkinTones)
            && (m_cachedAutoOptions.autoDenoiseShadows == opts.autoDenoiseShadows)
            && (m_cachedAutoOptions.centerWeighted == opts.centerWeighted)
            && (m_cachedAutoOptions.neutralSamplePoint == opts.neutralSamplePoint);

        if (!optionsMatch) {
            ImageProcessor::AutoEnhanceOptions baseOpts = opts;
            baseOpts.intensity = 100;
            m_autoEnhancedCache = ImageProcessor::autoEnhance(m_previewBase, baseOpts);
            m_cachedAutoOptions = opts;
            m_autoCacheValid = true;
        }

        if (opts.intensity == 100) {
            processed = m_autoEnhancedCache;
        } else {
            processed = ImageProcessor::blend(m_previewBase, m_autoEnhancedCache, opts.intensity / 100.0);
        }
    }

    // 1. Preset filter & intensity blend
    if (m_currentPreset != ImageProcessor::FilterPreset::Original) {
        QImage filtered = ImageProcessor::applyPreset(processed, m_currentPreset);
        double intensity = m_sliderIntensity ? (m_sliderIntensity->value() / 100.0) : 1.0;
        processed = ImageProcessor::blend(processed, filtered, intensity);
    }

    // 2. Light & Color adjustments
    int b = m_sliderBrightness ? m_sliderBrightness->value() : 0;
    int c = m_sliderContrast ? m_sliderContrast->value() : 0;
    int s = m_sliderSaturation ? m_sliderSaturation->value() : 0;
    int h = m_sliderHue ? m_sliderHue->value() : 0;
    int temp = m_sliderTemperature ? m_sliderTemperature->value() : 0;
    int r = m_sliderRed ? m_sliderRed->value() : 0;
    int g = m_sliderGreen ? m_sliderGreen->value() : 0;
    int bl = m_sliderBlue ? m_sliderBlue->value() : 0;
    int hl = m_sliderHighlights ? m_sliderHighlights->value() : 0;
    int sh = m_sliderShadows ? m_sliderShadows->value() : 0;

    processed = ImageProcessor::adjust(processed, b, c, s, h, temp, r, g, bl, hl, sh);

    // 3. Detail & effects
    if (m_sliderSharpen && m_sliderSharpen->value() > 0) {
        processed = ImageProcessor::applySharpen(processed, m_sliderSharpen->value());
    }
    if (m_sliderBlur && m_sliderBlur->value() > 0) {
        processed = ImageProcessor::applyBlur(processed, m_sliderBlur->value());
    }
    if (m_sliderVignette && m_sliderVignette->value() != 0) {
        processed = ImageProcessor::applyVignette(processed, m_sliderVignette->value(), 80);
    }
    if (m_chkInvert && m_chkInvert->isChecked()) {
        processed = ImageProcessor::applyInvert(processed);
    }

    m_viewport->setModifiedImage(processed);

    if (updateHist && m_histogram) {
        m_histogram->updateHistogram(processed);
    }
}

void PhotoStudioDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        if (m_viewport) m_viewport->setHoldCompareActive(true);
        return;
    }
    QDialog::keyPressEvent(event);
}

void PhotoStudioDialog::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        if (m_viewport) m_viewport->setHoldCompareActive(false);
        return;
    }
    QDialog::keyReleaseEvent(event);
}
