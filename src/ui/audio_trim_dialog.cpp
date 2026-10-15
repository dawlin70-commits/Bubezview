#include "audio_trim_dialog.h"
#include "audio_tag_dialog.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QApplication>
#include <clocale>
#include <cmath>
#include <algorithm>

// ============================================================================
// WaveformWidget Implementation
// ============================================================================
WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
    , m_startMs(0)
    , m_endMs(0)
    , m_totalMs(1)
    , m_currentMs(0)
    , m_draggingStart(false)
    , m_draggingEnd(false)
    , m_draggingRange(false)
    , m_dragOriginX(0)
{
    setMinimumHeight(130);
    setCursor(Qt::PointingHandCursor);
}

void WaveformWidget::setPeaks(const QVector<float> &peaks)
{
    m_peaks = peaks;
    update();
}

void WaveformWidget::setSelection(qint64 startMs, qint64 endMs, qint64 totalMs)
{
    m_startMs = qBound<qint64>(0, startMs, totalMs);
    m_endMs = qBound<qint64>(m_startMs, endMs, totalMs);
    m_totalMs = qMax<qint64>(1, totalMs);
    update();
}

void WaveformWidget::setCurrentPlayPosition(qint64 currentMs, qint64 totalMs)
{
    m_currentMs = currentMs;
    m_totalMs = qMax<qint64>(1, totalMs);
    update();
}

qint64 WaveformWidget::pixelToMs(int x) const
{
    if (width() <= 0) return 0;
    double fraction = static_cast<double>(qBound(0, x, width())) / static_cast<double>(width());
    return static_cast<qint64>(fraction * m_totalMs);
}

int WaveformWidget::msToPixel(qint64 ms) const
{
    if (m_totalMs <= 0) return 0;
    double fraction = static_cast<double>(ms) / static_cast<double>(m_totalMs);
    return static_cast<int>(fraction * width());
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int cy = h / 2;

    ThemePreset preset = ThemeManager::currentPreset();
    QColor canvasBg("#1a1d21");
    QColor canvasBorder("#2a2f35");
    QColor baselineColor("#2a2f35");
    QColor barInactive("#47515f");
    QColor barActive("#61afef");
    QColor regionColor(97, 175, 239, 40);

    if (preset == ThemePreset::BreezeDark) {
        canvasBg = QColor("#151719");
        canvasBorder = QColor("#23272b");
        baselineColor = QColor("#23272b");
        barInactive = QColor("#3e444b");
        barActive = QColor("#3daee9");
        regionColor = QColor(61, 174, 233, 40);
    } else if (preset == ThemePreset::SystemNative) {
        QPalette pal = palette();
        canvasBg = pal.base().color();
        canvasBorder = pal.mid().color();
        baselineColor = pal.mid().color();
        barInactive = pal.mid().color();
        barActive = pal.highlight().color();
        regionColor = pal.highlight().color();
        regionColor.setAlpha(40);
    }

    // Background canvas with subtle rounded border
    p.setPen(QPen(canvasBorder, 1));
    p.setBrush(canvasBg);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);

    int startX = msToPixel(m_startMs);
    int endX = msToPixel(m_endMs);

    // Selected region highlight with gradient
    if (endX > startX) {
        p.fillRect(QRect(startX, 1, endX - startX, h - 2), regionColor);
    }

    // Center baseline
    p.setPen(QPen(baselineColor, 1));
    p.drawLine(0, cy, w, cy);

    // Render waveform bars
    if (!m_peaks.isEmpty()) {
        int count = m_peaks.size();
        double barWidth = static_cast<double>(w) / static_cast<double>(count);
        for (int i = 0; i < count; ++i) {
            double bx = i * barWidth;
            double barH = m_peaks[i] * (h - 24);
            double y1 = cy - barH / 2.0;
            double y2 = cy + barH / 2.0;

            bool inRange = (bx >= startX && bx <= endX);
            p.setPen(QPen(inRange ? barActive : barInactive, qMax(1.0, barWidth - 1.0)));
            p.drawLine(QPointF(bx + barWidth / 2.0, y1), QPointF(bx + barWidth / 2.0, y2));
        }
    }

    // Current playhead line
    if (m_currentMs > 0 && m_currentMs <= m_totalMs) {
        int playX = msToPixel(m_currentMs);
        p.setPen(QPen(QColor("#ef4444"), 2));
        p.drawLine(playX, 2, playX, h - 2);
    }

    // Marker [A
    p.setPen(QPen(QColor("#10b981"), 2));
    p.drawLine(startX, 0, startX, h);
    p.setBrush(QColor("#10b981"));
    QPolygon headA;
    headA << QPoint(startX, 0) << QPoint(startX + 12, 0) << QPoint(startX, 16);
    p.drawPolygon(headA);

    // Marker B]
    p.setPen(QPen(QColor("#f59e0b"), 2));
    p.drawLine(endX, 0, endX, h);
    p.setBrush(QColor("#f59e0b"));
    QPolygon headB;
    headB << QPoint(endX, 0) << QPoint(endX - 12, 0) << QPoint(endX, 16);
    p.drawPolygon(headB);
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    int x = event->pos().x();
    int startX = msToPixel(m_startMs);
    int endX = msToPixel(m_endMs);

    if (std::abs(x - startX) <= 10) {
        m_draggingStart = true;
    } else if (std::abs(x - endX) <= 10) {
        m_draggingEnd = true;
    } else if (event->button() == Qt::LeftButton) {
        qint64 clickedMs = pixelToMs(x);
        emit seekRequested(clickedMs);
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    int x = event->pos().x();
    int startX = msToPixel(m_startMs);
    int endX = msToPixel(m_endMs);

    if (m_draggingStart) {
        qint64 newStart = qBound<qint64>(0, pixelToMs(x), m_endMs - 100);
        m_startMs = newStart;
        emit rangeSelected(m_startMs, m_endMs);
        update();
    } else if (m_draggingEnd) {
        qint64 newEnd = qBound<qint64>(m_startMs + 100, pixelToMs(x), m_totalMs);
        m_endMs = newEnd;
        emit rangeSelected(m_startMs, m_endMs);
        update();
    } else {
        if (std::abs(x - startX) <= 10 || std::abs(x - endX) <= 10) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_draggingStart = false;
    m_draggingEnd = false;
    m_draggingRange = false;
}

// ============================================================================
// SpectrumWidget Implementation
// ============================================================================
SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
    , m_lowCutHz(0)
    , m_denoiseDb(0)
    , m_dehumHz(0)
    , m_voiceEnhance(false)
    , m_deesser(false)
    , m_voiceFreqHz(3000)
    , m_voiceGainDb(3.5)
    , m_deessFreqHz(6500)
    , m_deessDepthDb(-6.0)
    , m_highCutHz(0)
    , m_isPlaying(false)
    , m_activeDragNode(DragNode::None)
{
    m_realBands.fill(0.0f, 36);
    setFixedHeight(150);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setToolTip(tr("Interactive 4-Node Acoustic Curve:\n• Drag Low-Cut (Blue) / High-Cut (Purple) horizontally\n• Drag Voice Presence (Green) and De-Esser (Yellow) in 2D (Frequency & Gain/Cut)"));
}

void SpectrumWidget::setFilterParameters(int lowCutHz, int denoiseDb, int dehumHz,
                                         bool voiceEnhance, bool deesser,
                                         int voiceFreqHz, double voiceGainDb,
                                         int deessFreqHz, double deessDepthDb,
                                         int highCutHz)
{
    m_lowCutHz = lowCutHz;
    m_denoiseDb = denoiseDb;
    m_dehumHz = dehumHz;
    m_voiceEnhance = voiceEnhance;
    m_deesser = deesser;
    m_voiceFreqHz = voiceFreqHz;
    m_voiceGainDb = voiceGainDb;
    m_deessFreqHz = deessFreqHz;
    m_deessDepthDb = deessDepthDb;
    m_highCutHz = highCutHz;
    update();
}

void SpectrumWidget::setSpectrumFftData(const QVector<float> &bands)
{
    if (bands.size() != m_realBands.size()) {
        m_realBands.resize(bands.size());
    }

    // Apply smooth attack and gentle decay per band
    for (int i = 0; i < bands.size(); ++i) {
        float target = bands[i];
        if (target > m_realBands[i]) {
            m_realBands[i] = m_realBands[i] * 0.35f + target * 0.65f; // Fast attack
        } else {
            m_realBands[i] = m_realBands[i] * 0.72f; // Smooth decay
        }
    }
    update();
}

void SpectrumWidget::setPlaying(bool playing)
{
    m_isPlaying = playing;
    if (!m_isPlaying) {
        m_realBands.fill(0.0f);
    }
    update();
}

double SpectrumWidget::freqToX(double freq) const
{
    if (freq <= 20.0) return 4.0;
    if (freq >= 20000.0) return static_cast<double>(width() - 4);
    double minLog = std::log10(20.0);
    double maxLog = std::log10(20000.0);
    double curLog = std::log10(freq);
    double frac = (curLog - minLog) / (maxLog - minLog);
    return 4.0 + frac * static_cast<double>(width() - 8);
}

double SpectrumWidget::xToFreq(double x) const
{
    if (x <= 4.0) return 20.0;
    if (x >= width() - 4) return 20000.0;
    double frac = (x - 4.0) / static_cast<double>(width() - 8);
    double minLog = std::log10(20.0);
    double maxLog = std::log10(20000.0);
    double curLog = minLog + frac * (maxLog - minLog);
    return std::pow(10.0, curLog);
}

void SpectrumWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    bool isDark = palette().window().color().lightness() < 128;

    // Theme palette colors
    QColor canvasBg = isDark ? QColor("#181b1f") : QColor("#ffffff");
    QColor canvasBorder = isDark ? QColor("#2a2f35") : QColor("#cbd5e1");
    QColor gridLineColor = isDark ? QColor("#24282f") : QColor("#e2e8f0");
    QColor gridTextColor = isDark ? QColor("#64748b") : QColor("#475569");
    QColor zeroDbLine = isDark ? QColor("#2d333b") : QColor("#cbd5e1");
    QColor zeroDbText = isDark ? QColor("#475569") : QColor("#64748b");
    QColor curveColor = isDark ? QColor("#61afef") : QColor("#0284c7");
    QColor curveFillStart = isDark ? QColor(97, 175, 239, 45) : QColor(2, 132, 199, 45);
    QColor curveFillEnd = isDark ? QColor(97, 175, 239, 5) : QColor(2, 132, 199, 5);
    QColor barGradTop = isDark ? QColor(97, 175, 239, 90) : QColor(2, 132, 199, 95);
    QColor barGradBottom = isDark ? QColor(16, 185, 129, 25) : QColor(16, 185, 129, 30);

    // 1. Canvas background
    QRect bgRect(0, 0, w, h);
    p.setPen(QPen(canvasBorder, 1));
    p.setBrush(canvasBg);
    p.drawRoundedRect(bgRect.adjusted(0, 0, -1, -1), 6, 6);

    // 2. Frequency grid lines (logarithmic scale)
    const struct GridFreq {
        double freq;
        const char *label;
    } gridFreqs[] = {
        {50.0, "50"}, {100.0, "100"}, {250.0, "250"}, {500.0, "500"},
        {1000.0, "1k"}, {2000.0, "2k"}, {5000.0, "5k"}, {10000.0, "10k"}
    };

    p.setFont(QFont("sans-serif", 8));
    for (const auto &gf : gridFreqs) {
        int x = static_cast<int>(freqToX(gf.freq));
        p.setPen(QPen(gridLineColor, 1, Qt::DotLine));
        p.drawLine(x, 4, x, h - 18);
        p.setPen(gridTextColor);
        p.drawText(x - 14, h - 4, QString::fromLatin1(gf.label));
    }

    // 3. dB Reference Lines (0 dB at y = 48)
    int y0db = 48;
    p.setPen(QPen(zeroDbLine, 1, Qt::DashLine));
    p.drawLine(4, y0db, w - 4, y0db);
    p.setPen(zeroDbText);
    p.drawText(6, y0db - 3, "0 dB");

    // 4. Real FFT Spectrum Bars
    if (m_isPlaying && !m_realBands.isEmpty()) {
        int numBars = m_realBands.size();
        double barWidth = static_cast<double>(w - 8) / numBars;
        for (int i = 0; i < numBars; ++i) {
            double barX = 4.0 + i * barWidth;
            float energy = m_realBands[i];
            if (energy > 0.015f) {
                int barH = static_cast<int>(energy * (h - 52));
                barH = std::clamp(barH, 2, h - 52);
                QRectF barRect(barX + 1, h - 18 - barH, barWidth - 2, barH);

                QLinearGradient grad(barRect.topLeft(), barRect.bottomLeft());
                grad.setColorAt(0.0, barGradTop);
                grad.setColorAt(1.0, barGradBottom);
                p.fillRect(barRect, grad);
            }
        }
    }

    // 5. Frequency Response Curve
    QPainterPath curvePath;
    QPainterPath fillPath;
    bool pathStarted = false;

    for (int x = 4; x <= w - 4; x += 2) {
        double freq = xToFreq(x);
        double gainDb = 0.0;

        // Low-cut rolloff (-24 dB / octave below cutoff)
        if (m_lowCutHz > 0 && freq < m_lowCutHz) {
            double octaves = std::log2(static_cast<double>(m_lowCutHz) / std::max(10.0, freq));
            gainDb -= octaves * 24.0;
        }

        // High-cut rolloff (-24 dB / octave above cutoff)
        if (m_highCutHz > 0 && m_highCutHz < 20000 && freq > m_highCutHz) {
            double octaves = std::log2(freq / static_cast<double>(m_highCutHz));
            gainDb -= octaves * 24.0;
        }

        // De-hum notch at 50Hz / 60Hz
        if (m_dehumHz > 0) {
            double diff = std::abs(freq - m_dehumHz);
            if (diff < 15.0) {
                gainDb -= 20.0 * (1.0 - diff / 15.0);
            }
        }

        // Parametric Voice presence boost (bell curve)
        if (m_voiceEnhance && m_voiceGainDb > 0.05) {
            double octDist = std::log2(freq / std::max(20, m_voiceFreqHz));
            gainDb += m_voiceGainDb * std::exp(-0.5 * std::pow(octDist / 0.5, 2));
        }

        // Parametric De-esser sibilance attenuation (bell dip)
        if (m_deesser && m_deessDepthDb < -0.05) {
            double octDist = std::log2(freq / std::max(20, m_deessFreqHz));
            gainDb += m_deessDepthDb * std::exp(-0.5 * std::pow(octDist / 0.45, 2));
        }

        // Noise floor attenuation indicator
        if (m_denoiseDb > 0) {
            gainDb -= (m_denoiseDb * 0.1);
        }

        gainDb = std::max(-30.0, std::min(14.0, gainDb));
        int y = y0db - static_cast<int>(gainDb * 2.6);
        y = std::max(6, std::min(h - 20, y));

        if (!pathStarted) {
            curvePath.moveTo(x, y);
            fillPath.moveTo(x, h - 18);
            fillPath.lineTo(x, y);
            pathStarted = true;
        } else {
            curvePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }

    fillPath.lineTo(w - 4, h - 18);
    fillPath.closeSubpath();

    // Fill under curve
    QLinearGradient fillGrad(0, 0, 0, h);
    fillGrad.setColorAt(0.0, curveFillStart);
    fillGrad.setColorAt(1.0, curveFillEnd);
    p.fillPath(fillPath, fillGrad);

    // Draw main curve line
    p.setPen(QPen(curveColor, 2.2));
    p.drawPath(curvePath);

    // 6. Draw 4 Interactive Parametric Nodes

    // --- Node 1: Low-Cut Filter ---
    if (m_lowCutHz > 0) {
        int cutX = static_cast<int>(freqToX(m_lowCutHz));
        p.setPen(QPen(QColor("#38bdf8"), 1.8, Qt::DashLine));
        p.drawLine(cutX, 4, cutX, h - 18);

        // Handle
        p.setBrush(QColor("#0284c7"));
        p.setPen(QPen(QColor("#ffffff"), 1.8));
        p.drawEllipse(QPoint(cutX, y0db), 6, 6);

        QRect markerRect(cutX - 28, 6, 56, 16);
        p.setPen(QColor("#0284c7"));
        p.setBrush(isDark ? QColor("#0f172a") : QColor("#e0f2fe"));
        p.drawRoundedRect(markerRect, 3, 3);
        p.setPen(QColor("#38bdf8"));
        p.setFont(QFont("sans-serif", 7, QFont::Bold));
        p.drawText(markerRect, Qt::AlignCenter, QString("%1 Hz").arg(m_lowCutHz));
    }

    // --- Node 2: Voice Presence Bell ---
    if (m_voiceEnhance) {
        int vx = static_cast<int>(freqToX(m_voiceFreqHz));
        int vy = y0db - static_cast<int>(m_voiceGainDb * 2.6);
        vy = std::clamp(vy, 10, h - 24);

        // Glowing outer halo
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(16, 185, 129, 60));
        p.drawEllipse(QPoint(vx, vy), 10, 10);

        // Node handle
        p.setPen(QPen(QColor("#ffffff"), 2));
        p.setBrush(QColor("#10b981"));
        p.drawEllipse(QPoint(vx, vy), 6, 6);

        // Floating badge
        QRect voiceBadge(vx - 36, vy - 20, 72, 16);
        p.setPen(QColor("#059669"));
        p.setBrush(isDark ? QColor("#064e3b") : QColor("#d1fae5"));
        p.drawRoundedRect(voiceBadge, 3, 3);
        p.setPen(QColor("#10b981"));
        p.setFont(QFont("sans-serif", 7, QFont::Bold));
        p.drawText(voiceBadge, Qt::AlignCenter, QString("+%1dB @ %2k").arg(m_voiceGainDb, 0, 'f', 1).arg(m_voiceFreqHz / 1000.0, 0, 'f', 1));
    }

    // --- Node 3: De-Esser Notch ---
    if (m_deesser) {
        int dx = static_cast<int>(freqToX(m_deessFreqHz));
        int dy = y0db - static_cast<int>(m_deessDepthDb * 2.6);
        dy = std::clamp(dy, 10, h - 24);

        // Glowing outer halo
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(245, 158, 11, 60));
        p.drawEllipse(QPoint(dx, dy), 10, 10);

        // Node handle
        p.setPen(QPen(QColor("#ffffff"), 2));
        p.setBrush(QColor("#f59e0b"));
        p.drawEllipse(QPoint(dx, dy), 6, 6);

        // Floating badge
        QRect deessBadge(dx - 36, dy + 10, 72, 16);
        p.setPen(QColor("#d97706"));
        p.setBrush(isDark ? QColor("#451a03") : QColor("#fef3c7"));
        p.drawRoundedRect(deessBadge, 3, 3);
        p.setPen(QColor("#f59e0b"));
        p.setFont(QFont("sans-serif", 7, QFont::Bold));
        p.drawText(deessBadge, Qt::AlignCenter, QString("%1dB @ %2k").arg(m_deessDepthDb, 0, 'f', 1).arg(m_deessFreqHz / 1000.0, 0, 'f', 1));
    }

    // --- Node 4: High-Cut Filter ---
    if (m_highCutHz > 0 && m_highCutHz < 20000) {
        int hx = static_cast<int>(freqToX(m_highCutHz));
        p.setPen(QPen(QColor("#c084fc"), 1.8, Qt::DashLine));
        p.drawLine(hx, 4, hx, h - 18);

        // Handle
        p.setBrush(QColor("#9333ea"));
        p.setPen(QPen(QColor("#ffffff"), 1.8));
        p.drawEllipse(QPoint(hx, y0db), 6, 6);

        QRect markerRect(hx - 28, 6, 56, 16);
        p.setPen(QColor("#9333ea"));
        p.setBrush(isDark ? QColor("#3b0764") : QColor("#f3e8ff"));
        p.drawRoundedRect(markerRect, 3, 3);
        p.setPen(QColor("#c084fc"));
        p.setFont(QFont("sans-serif", 7, QFont::Bold));
        p.drawText(markerRect, Qt::AlignCenter, QString("%1 kHz").arg(m_highCutHz / 1000.0, 0, 'f', 1));
    }
}

void SpectrumWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    QPoint pos = event->pos();
    int y0db = 48;

    // Check hit tests for the 4 nodes
    QPoint voicePt(static_cast<int>(freqToX(m_voiceFreqHz)), y0db - static_cast<int>(m_voiceGainDb * 2.6));
    QPoint deessPt(static_cast<int>(freqToX(m_deessFreqHz)), y0db - static_cast<int>(m_deessDepthDb * 2.6));
    int lowCutX = (m_lowCutHz > 0) ? static_cast<int>(freqToX(m_lowCutHz)) : static_cast<int>(freqToX(40));
    int highCutX = (m_highCutHz > 0 && m_highCutHz < 20000) ? static_cast<int>(freqToX(m_highCutHz)) : static_cast<int>(freqToX(18000));

    auto dist = [](const QPoint &a, const QPoint &b) {
        return std::sqrt(std::pow(a.x() - b.x(), 2) + std::pow(a.y() - b.y(), 2));
    };

    if (m_voiceEnhance && dist(pos, voicePt) <= 16.0) {
        m_activeDragNode = DragNode::Voice;
    } else if (m_deesser && dist(pos, deessPt) <= 16.0) {
        m_activeDragNode = DragNode::DeEsser;
    } else if (std::abs(pos.x() - lowCutX) <= 14) {
        m_activeDragNode = DragNode::LowCut;
    } else if (m_highCutHz > 0 && std::abs(pos.x() - highCutX) <= 14) {
        m_activeDragNode = DragNode::HighCut;
    } else {
        // Quick assign by region clicked
        double clickedFreq = xToFreq(pos.x());
        if (clickedFreq < 400.0) {
            m_activeDragNode = DragNode::LowCut;
            m_lowCutHz = std::clamp(static_cast<int>(clickedFreq), 0, 500);
            emit lowCutChanged(m_lowCutHz);
            update();
        } else if (clickedFreq > 12000.0) {
            m_activeDragNode = DragNode::HighCut;
            m_highCutHz = std::clamp(static_cast<int>(clickedFreq), 4000, 20000);
            emit highCutChanged(m_highCutHz);
            update();
        } else if (clickedFreq >= 800.0 && clickedFreq <= 4500.0) {
            m_activeDragNode = DragNode::Voice;
            m_voiceFreqHz = std::clamp(static_cast<int>(clickedFreq), 1000, 5000);
            m_voiceGainDb = std::clamp((y0db - pos.y()) / 2.6, 1.0, 12.0);
            m_voiceEnhance = true;
            emit voiceParamChanged(m_voiceFreqHz, m_voiceGainDb);
            update();
        } else {
            m_activeDragNode = DragNode::DeEsser;
            m_deessFreqHz = std::clamp(static_cast<int>(clickedFreq), 4000, 10000);
            m_deessDepthDb = std::clamp((y0db - pos.y()) / 2.6, -18.0, -1.0);
            m_deesser = true;
            emit deessParamChanged(m_deessFreqHz, m_deessDepthDb);
            update();
        }
    }
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    int y0db = 48;

    if (m_activeDragNode == DragNode::None) {
        // Update cursor icon on hover over nodes
        QPoint voicePt(static_cast<int>(freqToX(m_voiceFreqHz)), y0db - static_cast<int>(m_voiceGainDb * 2.6));
        QPoint deessPt(static_cast<int>(freqToX(m_deessFreqHz)), y0db - static_cast<int>(m_deessDepthDb * 2.6));
        int lowCutX = (m_lowCutHz > 0) ? static_cast<int>(freqToX(m_lowCutHz)) : -100;
        int highCutX = (m_highCutHz > 0 && m_highCutHz < 20000) ? static_cast<int>(freqToX(m_highCutHz)) : -100;

        auto dist = [](const QPoint &a, const QPoint &b) {
            return std::sqrt(std::pow(a.x() - b.x(), 2) + std::pow(a.y() - b.y(), 2));
        };

        if ((m_voiceEnhance && dist(pos, voicePt) <= 16.0) ||
            (m_deesser && dist(pos, deessPt) <= 16.0)) {
            setCursor(Qt::SizeAllCursor);
        } else if (std::abs(pos.x() - lowCutX) <= 12 || std::abs(pos.x() - highCutX) <= 12) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        return;
    }

    if (m_activeDragNode == DragNode::LowCut) {
        int hz = std::clamp(static_cast<int>(xToFreq(pos.x())), 0, 500);
        m_lowCutHz = hz;
        emit lowCutChanged(hz);
        update();
    } else if (m_activeDragNode == DragNode::HighCut) {
        int hz = std::clamp(static_cast<int>(xToFreq(pos.x())), 4000, 20000);
        m_highCutHz = hz;
        emit highCutChanged(hz);
        update();
    } else if (m_activeDragNode == DragNode::Voice) {
        int freq = std::clamp(static_cast<int>(xToFreq(pos.x())), 1000, 5000);
        double gain = std::clamp((y0db - pos.y()) / 2.6, 0.5, 12.0);
        m_voiceFreqHz = freq;
        m_voiceGainDb = gain;
        m_voiceEnhance = true;
        emit voiceParamChanged(m_voiceFreqHz, m_voiceGainDb);
        update();
    } else if (m_activeDragNode == DragNode::DeEsser) {
        int freq = std::clamp(static_cast<int>(xToFreq(pos.x())), 4000, 10000);
        double depth = std::clamp((y0db - pos.y()) / 2.6, -18.0, -1.0);
        m_deessFreqHz = freq;
        m_deessDepthDb = depth;
        m_deesser = true;
        emit deessParamChanged(m_deessFreqHz, m_deessDepthDb);
        update();
    }
}

void SpectrumWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_activeDragNode = DragNode::None;
    setCursor(Qt::CrossCursor);
}

// ============================================================================
// AudioTrimDialog Implementation
// ============================================================================
AudioTrimDialog::AudioTrimDialog(const QString &audioFilePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(audioFilePath)
    , m_lblInfo(nullptr)
    , m_tabWidget(nullptr)
    , m_waveform(nullptr)
    , m_lblStart(nullptr)
    , m_lblEnd(nullptr)
    , m_lblDuration(nullptr)
    , m_btnPlayPreview(nullptr)
    , m_btnSetStart(nullptr)
    , m_btnSetEnd(nullptr)
    , m_spinFadeIn(nullptr)
    , m_spinFadeOut(nullptr)
    , m_chkLossless(nullptr)
    , m_comboPresets(nullptr)
    , m_btnPlayFilters(nullptr)
    , m_btnBypass(nullptr)
    , m_sliderSeekFilters(nullptr)
    , m_lblTimeFilters(nullptr)
    , m_spectrumWidget(nullptr)
    , m_sliderLowCut(nullptr)
    , m_lblLowCutVal(nullptr)
    , m_sliderDenoise(nullptr)
    , m_lblDenoiseVal(nullptr)
    , m_comboDehum(nullptr)
    , m_chkDeclick(nullptr)
    , m_chkDeesser(nullptr)
    , m_chkVoiceEnhance(nullptr)
    , m_chkNormalize(nullptr)
    , m_chkNoiseGate(nullptr)
    , m_comboFormat(nullptr)
    , m_lblStatusBadge(nullptr)
    , m_progressBar(nullptr)
    , m_btnSave(nullptr)
    , m_btnCancel(nullptr)
    , m_previewMpv(nullptr)
    , m_playbackTimer(new QTimer(this))
    , m_startMs(0)
    , m_endMs(0)
    , m_isPreviewPlaying(false)
    , m_updatingPresetUI(false)
    , m_isFiltersPlaying(false)
    , m_isBypassActive(false)
{
    setWindowTitle(tr("Audio Suite & Tools - BubezView"));
    setMinimumSize(820, 580);
    resize(860, 620);

    applyTheme();

    // Initialize libmpv instance for instant sub-clip looping
    std::setlocale(LC_NUMERIC, "C");
    m_previewMpv = mpv_create();
    if (m_previewMpv) {
        mpv_set_option_string(m_previewMpv, "terminal", "no");
        mpv_set_option_string(m_previewMpv, "msg-level", "all=no");
        mpv_set_option_string(m_previewMpv, "audio-fallback-to-null", "yes");
        mpv_initialize(m_previewMpv);
    }

    m_meta = AudioProcessor::readMetadata(m_filePath);
    m_startMs = 0;
    m_endMs = m_meta.durationMs > 0 ? m_meta.durationMs : 10000;

    m_pcmSampleRate = 22050;

    setupUi();

    // Async peak generation
    QVector<float> peaks = AudioProcessor::generateWaveformPeaks(m_filePath, 260);
    m_waveform->setPeaks(peaks);
    m_waveform->setSelection(m_startMs, m_endMs, m_meta.durationMs);

    // Extract PCM data for accurate real-time FFT spectrum analysis
    m_pcmData = AudioProcessor::extractPcmData(m_filePath, m_pcmSampleRate, 360);

    connect(m_playbackTimer, &QTimer::timeout, this, &AudioTrimDialog::updatePreviewTimer);
    applyPreviewFilters();
}

AudioTrimDialog::~AudioTrimDialog()
{
    stopPreview();
    if (m_previewMpv) {
        mpv_terminate_destroy(m_previewMpv);
        m_previewMpv = nullptr;
    }
}

void AudioTrimDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    // 1. Shared Header Information
    QFileInfo fi(m_filePath);
    m_lblInfo = new QLabel(this);
    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);
    QString titleColor = isDark ? "#eff0f1" : "#1e293b";
    QString subColor = isDark ? "#959ca7" : "#64748b";
    QString infoText = QString("<div style='font-size: 14px; font-weight: bold; color: %1;'>%2</div>"
                               "<div style='font-size: 11px; color: %3; margin-top: 2px;'>%4 &nbsp;|&nbsp; %5 kHz &nbsp;|&nbsp; %6 %7 &nbsp;|&nbsp; %8 kbps</div>")
                           .arg(titleColor, fi.fileName(),
                                subColor,
                                m_meta.codecName.isEmpty() ? fi.suffix().toUpper() : m_meta.codecName,
                                QString::number(m_meta.sampleRate / 1000.0, 'f', 1),
                                QString::number(m_meta.channels),
                                (m_meta.channels == 2 ? tr("channels (Stereo)") : tr("channel (Mono)")),
                                QString::number(m_meta.bitRate));
    m_lblInfo->setText(infoText);
    mainLayout->addWidget(m_lblInfo);

    // 2. Four-Tab Suite: Trimming, Acoustic Restoration, Channels & Pitch/Tempo, Equalizer
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createTrimTab(), tr("✂️ Audio Trimming"));
    m_tabWidget->addTab(createFiltersTab(), tr("🎛️ Filters & Noise Reduction"));
    m_tabWidget->addTab(createDspTab(), tr("🔊 Channels, Pitch & Tempo"));
    m_tabWidget->addTab(createEqualizerTab(), tr("🎚️ Track Equalizer"));
    mainLayout->addWidget(m_tabWidget, 1);

    // 3. Shared Footer: Format, Bitrate, Sample Rate, Tag Editor, Action Buttons
    QHBoxLayout *footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(8);

    QLabel *lblOutFmt = new QLabel(tr("Format:"), this);
    footerLayout->addWidget(lblOutFmt);

    m_comboFormat = new QComboBox(this);
    m_comboFormat->addItem(tr("Original Format (Lossless Copy)"), "copy");
    m_comboFormat->addItem("MP3", "mp3");
    m_comboFormat->addItem("FLAC", "flac");
    m_comboFormat->addItem("WAV", "wav");
    m_comboFormat->addItem("AAC / M4A", "aac");
    m_comboFormat->addItem("OGG Vorbis", "ogg");
    connect(m_comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onFormatChanged);
    footerLayout->addWidget(m_comboFormat);

    QLabel *lblBitrate = new QLabel(tr("Bitrate:"), this);
    footerLayout->addWidget(lblBitrate);

    m_comboBitrate = new QComboBox(this);
    m_comboBitrate->addItem(tr("Auto"), 0);
    m_comboBitrate->addItem("128 kbps", 128);
    m_comboBitrate->addItem("192 kbps", 192);
    m_comboBitrate->addItem("256 kbps", 256);
    m_comboBitrate->addItem("320 kbps", 320);
    m_comboBitrate->setCurrentIndex(4); // default 320 kbps
    connect(m_comboBitrate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onFilterChanged);
    footerLayout->addWidget(m_comboBitrate);

    QLabel *lblSamplerate = new QLabel(tr("Sample Rate:"), this);
    footerLayout->addWidget(lblSamplerate);

    m_comboSampleRate = new QComboBox(this);
    m_comboSampleRate->addItem(tr("Original"), 0);
    m_comboSampleRate->addItem("44.1 kHz", 44100);
    m_comboSampleRate->addItem("48.0 kHz", 48000);
    m_comboSampleRate->addItem("96.0 kHz", 96000);
    m_comboSampleRate->addItem("192.0 kHz", 192000);
    connect(m_comboSampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onFilterChanged);
    footerLayout->addWidget(m_comboSampleRate);

    m_btnOpenTagEditor = new QPushButton(tr("🏷️ Tag Editor..."), this);
    m_btnOpenTagEditor->setToolTip(tr("Edit metadata, titles, and album art for this file"));
    connect(m_btnOpenTagEditor, &QPushButton::clicked, this, &AudioTrimDialog::onOpenTagEditorClicked);
    footerLayout->addWidget(m_btnOpenTagEditor);

    m_lblStatusBadge = new QLabel(this);
    footerLayout->addWidget(m_lblStatusBadge);
    footerLayout->addStretch();

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    footerLayout->addWidget(m_btnCancel);

    m_btnSave = new QPushButton(tr("Save Audio..."), this);
    m_btnSave->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   padding: 7px 20px;"
        "   border-radius: 6px;"
        "   border: 1px solid #0369a1;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnSave, &QPushButton::clicked, this, &AudioTrimDialog::onSaveClicked);
    footerLayout->addWidget(m_btnSave);

    mainLayout->addLayout(footerLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet("QProgressBar { border-radius: 4px; height: 6px; text-align: center; } QProgressBar::chunk { background-color: #0284c7; border-radius: 4px; }");
    mainLayout->addWidget(m_progressBar);

    updateTimeLabels();
}

QWidget *AudioTrimDialog::createTrimTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 12, 14, 12);
    tabLayout->setSpacing(12);

    // 1. Waveform Visualizer
    m_waveform = new WaveformWidget(tab);
    connect(m_waveform, &WaveformWidget::rangeSelected, this, &AudioTrimDialog::onRangeChangedFromWaveform);
    connect(m_waveform, &WaveformWidget::seekRequested, this, &AudioTrimDialog::onWaveformSeek);
    tabLayout->addWidget(m_waveform, 1);

    // 2. Selection Timestamps
    QHBoxLayout *timeLayout = new QHBoxLayout();
    m_lblStart = new QLabel(tab);
    m_lblEnd = new QLabel(tab);
    m_lblDuration = new QLabel(tab);
    m_lblStart->setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;");
    m_lblEnd->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 13px;");
    m_lblDuration->setStyleSheet("color: #0284c7; font-weight: bold; font-size: 13px;");
    timeLayout->addWidget(m_lblStart);
    timeLayout->addStretch();
    timeLayout->addWidget(m_lblDuration);
    timeLayout->addStretch();
    timeLayout->addWidget(m_lblEnd);
    tabLayout->addLayout(timeLayout);

    // 3. Quick Markers & Preview Controls
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_btnPlayPreview = new QPushButton(tr("▶ Play Selection (A-B Loop)"), tab);
    m_btnPlayPreview->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   padding: 7px 16px;"
        "   font-weight: bold;"
        "   border-radius: 6px;"
        "   border: 1px solid #0369a1;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnPlayPreview, &QPushButton::clicked, this, &AudioTrimDialog::onPlayPreviewClicked);

    m_btnSetStart = new QPushButton(tr("Set Start [A] Here"), tab);
    m_btnSetStart->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(16, 185, 129, 0.15);"
        "   color: #10b981;"
        "   padding: 7px 14px;"
        "   font-weight: bold;"
        "   border: 1px solid #10b981;"
        "   border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: rgba(16, 185, 129, 0.25); }"
    );
    connect(m_btnSetStart, &QPushButton::clicked, this, &AudioTrimDialog::onSetStartCurrent);

    m_btnSetEnd = new QPushButton(tr("Set End [B] Here"), tab);
    m_btnSetEnd->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(245, 158, 11, 0.15);"
        "   color: #f59e0b;"
        "   padding: 7px 14px;"
        "   font-weight: bold;"
        "   border: 1px solid #f59e0b;"
        "   border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: rgba(245, 158, 11, 0.25); }"
    );
    connect(m_btnSetEnd, &QPushButton::clicked, this, &AudioTrimDialog::onSetEndCurrent);

    m_btnAutoDetectSilence = new QPushButton(tr("⚡ Auto-Snap Silence"), tab);
    m_btnAutoDetectSilence->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(97, 175, 239, 0.15);"
        "   color: #38bdf8;"
        "   padding: 7px 14px;"
        "   font-weight: bold;"
        "   border: 1px solid #0284c7;"
        "   border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: rgba(97, 175, 239, 0.25); }"
    );
    m_btnAutoDetectSilence->setToolTip(tr("Automatically detect start and end of audio, trimming leading and trailing silence."));
    connect(m_btnAutoDetectSilence, &QPushButton::clicked, this, &AudioTrimDialog::onAutoDetectSilenceClicked);

    m_btnSplitCue = new QPushButton(tr("📑 Split by CUE..."), tab);
    m_btnSplitCue->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(168, 85, 247, 0.15);"
        "   color: #c084fc;"
        "   padding: 7px 14px;"
        "   font-weight: bold;"
        "   border: 1px solid #a855f7;"
        "   border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: rgba(168, 85, 247, 0.25); }"
    );
    m_btnSplitCue->setToolTip(tr("Automatically split audio into individual tracks using a CUE sheet file."));
    connect(m_btnSplitCue, &QPushButton::clicked, this, &AudioTrimDialog::onSplitCueClicked);

    btnLayout->addWidget(m_btnPlayPreview);
    btnLayout->addWidget(m_btnSetStart);
    btnLayout->addWidget(m_btnSetEnd);
    btnLayout->addWidget(m_btnAutoDetectSilence);
    btnLayout->addWidget(m_btnSplitCue);
    btnLayout->addStretch();
    tabLayout->addLayout(btnLayout);

    // 4. Trimming Options (Fades & Lossless)
    QGroupBox *grpTrim = new QGroupBox(tr("Trimming & Fades Options"), tab);
    QGridLayout *grid = new QGridLayout(grpTrim);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(10);

    QLabel *lblFadeIn = new QLabel(tr("Fade-In (sec):"), grpTrim);
    grid->addWidget(lblFadeIn, 0, 0);

    m_spinFadeIn = new QDoubleSpinBox(grpTrim);
    m_spinFadeIn->setRange(0.0, 10.0);
    m_spinFadeIn->setSingleStep(0.5);
    m_spinFadeIn->setValue(0.0);
    m_spinFadeIn->setFixedWidth(90);
    connect(m_spinFadeIn, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AudioTrimDialog::onFilterChanged);
    grid->addWidget(m_spinFadeIn, 0, 1);

    QLabel *lblFadeOut = new QLabel(tr("Fade-Out (sec):"), grpTrim);
    grid->addWidget(lblFadeOut, 0, 2);

    m_spinFadeOut = new QDoubleSpinBox(grpTrim);
    m_spinFadeOut->setRange(0.0, 10.0);
    m_spinFadeOut->setSingleStep(0.5);
    m_spinFadeOut->setValue(0.0);
    m_spinFadeOut->setFixedWidth(90);
    connect(m_spinFadeOut, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &AudioTrimDialog::onFilterChanged);
    grid->addWidget(m_spinFadeOut, 0, 3);

    m_chkLossless = new QCheckBox(tr("Lightning Lossless Mode (Instant copy, no re-encode)"), grpTrim);
    m_chkLossless->setChecked(true);
    connect(m_chkLossless, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_spinFadeIn->setValue(0.0);
            m_spinFadeOut->setValue(0.0);
            m_comboPresets->setCurrentIndex(0);
            m_sliderLowCut->setValue(0);
            m_sliderDenoise->setValue(0);
            m_comboDehum->setCurrentIndex(0);
            m_chkDeclick->setChecked(false);
            m_chkDeesser->setChecked(false);
            m_chkVoiceEnhance->setChecked(false);
            m_chkNormalize->setChecked(false);
            m_chkNoiseGate->setChecked(false);
            applyPreviewFilters();
        }
    });
    grid->addWidget(m_chkLossless, 1, 0, 1, 4);

    tabLayout->addWidget(grpTrim);

    QLabel *lblTip = new QLabel(tr("⚡ <b>Tip:</b> When no fades or audio filters are active, Lossless Mode cuts the audio in ~0.1s preserving 100% original quality without recompression."), tab);
    ThemePreset pr = ThemeManager::currentPreset();
    bool isDarkPr = (pr == ThemePreset::DarkSlate || pr == ThemePreset::BreezeDark);
    lblTip->setStyleSheet(QString("color: %1; font-size: 12px; margin-top: 4px;").arg(isDarkPr ? "#959ca7" : "#64748b"));
    lblTip->setWordWrap(true);
    tabLayout->addWidget(lblTip);

    return tab;
}

QWidget *AudioTrimDialog::createFiltersTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 12, 14, 12);
    tabLayout->setSpacing(12);

    // 1. Acoustic Presets Header Bar
    QHBoxLayout *presetLayout = new QHBoxLayout();
    QLabel *lblPreset = new QLabel(tr("Acoustic Scenario Preset:"), tab);
    lblPreset->setStyleSheet("font-weight: bold; font-size: 13px;");
    presetLayout->addWidget(lblPreset);

    m_comboPresets = new QComboBox(tab);
    m_comboPresets->addItem(tr("No Filter (Original Audio)"), 0);
    m_comboPresets->addItem(tr("🚗 Traffic, Wind & Low Rumble (Cut < 110Hz)"), 1);
    m_comboPresets->addItem(tr("💨 PC Fan & Microphone Hiss (Adaptive FFT Denoise)"), 2);
    m_comboPresets->addItem(tr("⚡ Electrical Mains Hum (50Hz / 60Hz Notch)"), 3);
    m_comboPresets->addItem(tr("🎙️ Voice & Clear Podcast (Anti-rumble + Denoise + De-Esser + Gate)"), 4);
    m_comboPresets->addItem(tr("📻 Vinyl Restoration & Clean (De-Click + De-Hum + Denoise)"), 5);
    m_comboPresets->addItem(tr("🎚️ Manual & Custom Tuning..."), 6);
    connect(m_comboPresets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onPresetSelected);
    presetLayout->addWidget(m_comboPresets, 1);
    tabLayout->addLayout(presetLayout);

    // 2. Dedicated Mini-Player Bar for Filters + A/B Bypass
    QHBoxLayout *playBar = new QHBoxLayout();
    playBar->setSpacing(10);

    m_btnPlayFilters = new QPushButton(tr("▶ Play Preview"), tab);
    m_btnPlayFilters->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   padding: 6px 14px;"
        "   border-radius: 6px;"
        "   border: 1px solid #0369a1;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnPlayFilters, &QPushButton::clicked, this, &AudioTrimDialog::onPlayFiltersClicked);
    playBar->addWidget(m_btnPlayFilters);

    m_btnBypass = new QPushButton(tr("🎧 A/B Bypass (Compare Original)"), tab);
    m_btnBypass->setCheckable(true);
    m_btnBypass->setToolTip(tr("Temporarily disable all active filters to compare with original audio in real-time."));
    m_btnBypass->setStyleSheet(
        "QPushButton {"
        "   background-color: palette(button);"
        "   color: palette(button-text);"
        "   font-weight: bold;"
        "   padding: 6px 14px;"
        "   border-radius: 6px;"
        "   border: 1px solid #cbd5e1;"
        "}"
        "QPushButton:checked {"
        "   background-color: #eab308;"
        "   color: #000000;"
        "   border: 1px solid #ca8a04;"
        "}"
    );
    connect(m_btnBypass, &QPushButton::toggled, this, &AudioTrimDialog::onBypassToggled);
    playBar->addWidget(m_btnBypass);

    m_sliderSeekFilters = new QSlider(Qt::Horizontal, tab);
    m_sliderSeekFilters->setRange(0, 1000);
    m_sliderSeekFilters->setValue(0);
    connect(m_sliderSeekFilters, &QSlider::sliderMoved, this, &AudioTrimDialog::onFiltersSeek);
    playBar->addWidget(m_sliderSeekFilters, 1);

    m_lblTimeFilters = new QLabel(QString("00:00.0 / %1").arg(formatTime(m_meta.durationMs)), tab);
    m_lblTimeFilters->setStyleSheet("font-family: monospace; font-size: 12px; font-weight: bold;");
    playBar->addWidget(m_lblTimeFilters);

    tabLayout->addLayout(playBar);

    // 3. Interactive Spectrum Analyzer & Curve Widget
    m_spectrumWidget = new SpectrumWidget(tab);
    m_spectrumWidget->setFixedHeight(150);
    connect(m_spectrumWidget, &SpectrumWidget::lowCutChanged, this, [this](int hz) {
        if (m_sliderLowCut) m_sliderLowCut->setValue(hz);
    });
    connect(m_spectrumWidget, &SpectrumWidget::highCutChanged, this, [this](int hz) {
        if (m_sliderHighCut) m_sliderHighCut->setValue(hz);
    });
    connect(m_spectrumWidget, &SpectrumWidget::voiceParamChanged, this, [this](int freq, double gain) {
        if (m_chkVoiceEnhance) m_chkVoiceEnhance->setChecked(true);
        if (m_sliderVoiceFreq) m_sliderVoiceFreq->setValue(freq);
        if (m_sliderVoiceGain) m_sliderVoiceGain->setValue(static_cast<int>(gain * 10));
    });
    connect(m_spectrumWidget, &SpectrumWidget::deessParamChanged, this, [this](int freq, double depth) {
        if (m_chkDeesser) m_chkDeesser->setChecked(true);
        if (m_sliderDeessFreq) m_sliderDeessFreq->setValue(freq);
        if (m_sliderDeessDepth) m_sliderDeessDepth->setValue(static_cast<int>(depth * 10));
    });
    tabLayout->addWidget(m_spectrumWidget);

    // 4. Acoustic Cleaning & Enhancement GroupBoxes (Side-by-side or neatly grouped)
    QHBoxLayout *groupsLayout = new QHBoxLayout();
    groupsLayout->setSpacing(12);

    // Left Box: Noise & Rumble Removal
    QGroupBox *grpNoise = new QGroupBox(tr("Noise, Rumble & Cutoff Filters"), tab);
    QGridLayout *gridNoise = new QGridLayout(grpNoise);
    gridNoise->setHorizontalSpacing(10);
    gridNoise->setVerticalSpacing(6);

    QLabel *lblLowCut = new QLabel(tr("Low-Cut (High-Pass):"), grpNoise);
    gridNoise->addWidget(lblLowCut, 0, 0);

    m_sliderLowCut = new QSlider(Qt::Horizontal, grpNoise);
    m_sliderLowCut->setRange(0, 500);
    m_sliderLowCut->setValue(0);
    connect(m_sliderLowCut, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    gridNoise->addWidget(m_sliderLowCut, 0, 1);

    m_lblLowCutVal = new QLabel(tr("Off"), grpNoise);
    m_lblLowCutVal->setFixedWidth(50);
    gridNoise->addWidget(m_lblLowCutVal, 0, 2);

    QLabel *lblHighCut = new QLabel(tr("High-Cut (Low-Pass):"), grpNoise);
    gridNoise->addWidget(lblHighCut, 1, 0);

    m_sliderHighCut = new QSlider(Qt::Horizontal, grpNoise);
    m_sliderHighCut->setRange(4000, 20000);
    m_sliderHighCut->setValue(20000);
    connect(m_sliderHighCut, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    gridNoise->addWidget(m_sliderHighCut, 1, 1);

    m_lblHighCutVal = new QLabel(tr("Off"), grpNoise);
    m_lblHighCutVal->setFixedWidth(50);
    gridNoise->addWidget(m_lblHighCutVal, 1, 2);

    QLabel *lblDenoise = new QLabel(tr("FFT Denoise:"), grpNoise);
    gridNoise->addWidget(lblDenoise, 2, 0);

    m_sliderDenoise = new QSlider(Qt::Horizontal, grpNoise);
    m_sliderDenoise->setRange(0, 25);
    m_sliderDenoise->setValue(0);
    connect(m_sliderDenoise, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    gridNoise->addWidget(m_sliderDenoise, 2, 1);

    m_lblDenoiseVal = new QLabel(tr("Off"), grpNoise);
    m_lblDenoiseVal->setFixedWidth(50);
    gridNoise->addWidget(m_lblDenoiseVal, 2, 2);

    QLabel *lblDehum = new QLabel(tr("De-Hum Notch:"), grpNoise);
    gridNoise->addWidget(lblDehum, 3, 0);

    m_comboDehum = new QComboBox(grpNoise);
    m_comboDehum->addItem(tr("Disabled"), 0);
    m_comboDehum->addItem(tr("50 Hz (EU / Asia)"), 50);
    m_comboDehum->addItem(tr("60 Hz (US / Americas)"), 60);
    connect(m_comboDehum, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onManualControlChanged);
    gridNoise->addWidget(m_comboDehum, 3, 1, 1, 2);

    m_chkDeclick = new QCheckBox(tr("Vinyl De-Clicker & De-Pop (Scratches & clicks)"), grpNoise);
    connect(m_chkDeclick, &QCheckBox::toggled, this, &AudioTrimDialog::onManualControlChanged);
    gridNoise->addWidget(m_chkDeclick, 4, 0, 1, 3);

    groupsLayout->addWidget(grpNoise, 1);

    // Right Box: Voice & Dynamics Enhancement
    QGroupBox *grpVoice = new QGroupBox(tr("Voice Clarity & Dynamics"), tab);
    QVBoxLayout *vboxVoice = new QVBoxLayout(grpVoice);
    vboxVoice->setSpacing(6);

    // Parametric Voice Presence
    QHBoxLayout *rowVoiceHdr = new QHBoxLayout();
    m_chkVoiceEnhance = new QCheckBox(tr("Voice Presence & Clarity"), grpVoice);
    connect(m_chkVoiceEnhance, &QCheckBox::toggled, this, &AudioTrimDialog::onManualControlChanged);
    rowVoiceHdr->addWidget(m_chkVoiceEnhance);

    m_lblVoiceVal = new QLabel("+3.5 dB @ 3.0 kHz", grpVoice);
    m_lblVoiceVal->setStyleSheet("font-size: 11px; color: #10b981; font-weight: bold;");
    rowVoiceHdr->addWidget(m_lblVoiceVal, 0, Qt::AlignRight);
    vboxVoice->addLayout(rowVoiceHdr);

    QHBoxLayout *rowVoiceSliders = new QHBoxLayout();
    rowVoiceSliders->addWidget(new QLabel(tr("Freq:"), grpVoice));
    m_sliderVoiceFreq = new QSlider(Qt::Horizontal, grpVoice);
    m_sliderVoiceFreq->setRange(1000, 5000);
    m_sliderVoiceFreq->setValue(3000);
    connect(m_sliderVoiceFreq, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    rowVoiceSliders->addWidget(m_sliderVoiceFreq, 1);

    rowVoiceSliders->addWidget(new QLabel(tr("Gain:"), grpVoice));
    m_sliderVoiceGain = new QSlider(Qt::Horizontal, grpVoice);
    m_sliderVoiceGain->setRange(5, 120);
    m_sliderVoiceGain->setValue(35);
    connect(m_sliderVoiceGain, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    rowVoiceSliders->addWidget(m_sliderVoiceGain, 1);
    vboxVoice->addLayout(rowVoiceSliders);

    // Parametric De-Esser
    QHBoxLayout *rowDeessHdr = new QHBoxLayout();
    m_chkDeesser = new QCheckBox(tr("De-Esser (Harsh Sibilance Dip)"), grpVoice);
    connect(m_chkDeesser, &QCheckBox::toggled, this, &AudioTrimDialog::onManualControlChanged);
    rowDeessHdr->addWidget(m_chkDeesser);

    m_lblDeessVal = new QLabel("-6.0 dB @ 6.5 kHz", grpVoice);
    m_lblDeessVal->setStyleSheet("font-size: 11px; color: #f59e0b; font-weight: bold;");
    rowDeessHdr->addWidget(m_lblDeessVal, 0, Qt::AlignRight);
    vboxVoice->addLayout(rowDeessHdr);

    QHBoxLayout *rowDeessSliders = new QHBoxLayout();
    rowDeessSliders->addWidget(new QLabel(tr("Freq:"), grpVoice));
    m_sliderDeessFreq = new QSlider(Qt::Horizontal, grpVoice);
    m_sliderDeessFreq->setRange(4000, 10000);
    m_sliderDeessFreq->setValue(6500);
    connect(m_sliderDeessFreq, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    rowDeessSliders->addWidget(m_sliderDeessFreq, 1);

    rowDeessSliders->addWidget(new QLabel(tr("Cut:"), grpVoice));
    m_sliderDeessDepth = new QSlider(Qt::Horizontal, grpVoice);
    m_sliderDeessDepth->setRange(-180, -10);
    m_sliderDeessDepth->setValue(-60);
    connect(m_sliderDeessDepth, &QSlider::valueChanged, this, &AudioTrimDialog::onManualControlChanged);
    rowDeessSliders->addWidget(m_sliderDeessDepth, 1);
    vboxVoice->addLayout(rowDeessSliders);

    m_chkNoiseGate = new QCheckBox(tr("Noise Gate (Mute background hiss during pauses)"), grpVoice);
    connect(m_chkNoiseGate, &QCheckBox::toggled, this, &AudioTrimDialog::onManualControlChanged);
    vboxVoice->addWidget(m_chkNoiseGate);

    m_chkNormalize = new QCheckBox(tr("Loudness Normalization (Broadcast EBU R128)"), grpVoice);
    connect(m_chkNormalize, &QCheckBox::toggled, this, &AudioTrimDialog::onManualControlChanged);
    vboxVoice->addWidget(m_chkNormalize);

    vboxVoice->addStretch();
    groupsLayout->addWidget(grpVoice, 1);

    tabLayout->addLayout(groupsLayout);

    return tab;
}

QWidget *AudioTrimDialog::createEqualizerTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(12, 10, 12, 10);

    m_equalizerWidget = new EqualizerWidget(tab);
    connect(m_equalizerWidget, &EqualizerWidget::configurationChanged, this, [this](const EqualizerPreset &, bool) {
        onFilterChanged();
    });
    tabLayout->addWidget(m_equalizerWidget);

    return tab;
}

QWidget *AudioTrimDialog::createDspTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(16, 14, 16, 14);
    tabLayout->setSpacing(14);

    // 1. Canali & Spazialità Group
    QGroupBox *grpChannels = new QGroupBox(tr("Gestione Canali & Spazialità"), tab);
    QVBoxLayout *channelsLayout = new QVBoxLayout(grpChannels);
    channelsLayout->setSpacing(10);

    QHBoxLayout *rowChannel = new QHBoxLayout();
    QLabel *lblChannelMode = new QLabel(tr("Modalità Canali:"), grpChannels);
    lblChannelMode->setStyleSheet("font-weight: bold;");
    rowChannel->addWidget(lblChannelMode);

    m_comboChannels = new QComboBox(grpChannels);
    m_comboChannels->addItem(tr("Original (Stereo / Multichannel)"), static_cast<int>(ChannelMode::Original));
    m_comboChannels->addItem(tr("Stereo ➔ Mono (Balanced Downmix)"), static_cast<int>(ChannelMode::StereoToMono));
    m_comboChannels->addItem(tr("Swap Channels (L ⇄ R)"), static_cast<int>(ChannelMode::SwapChannels));
    m_comboChannels->addItem(tr("Left Channel Only (L) on both sides"), static_cast<int>(ChannelMode::LeftOnly));
    m_comboChannels->addItem(tr("Right Channel Only (R) on both sides"), static_cast<int>(ChannelMode::RightOnly));
    m_comboChannels->addItem(tr("🎤 Vocal Remover / Karaoke (Center Channel Cancellation)"), static_cast<int>(ChannelMode::VocalRemover));
    connect(m_comboChannels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioTrimDialog::onFilterChanged);
    rowChannel->addWidget(m_comboChannels, 1);
    channelsLayout->addLayout(rowChannel);

    tabLayout->addWidget(grpChannels);

    // 2. Pitch Shift & Time Stretch Group
    QGroupBox *grpPitchTempo = new QGroupBox(tr("Pitch Shift & Time Stretch"), tab);
    QGridLayout *pitchGrid = new QGridLayout(grpPitchTempo);
    pitchGrid->setHorizontalSpacing(14);
    pitchGrid->setVerticalSpacing(12);

    // Pitch Shift
    QLabel *lblPitch = new QLabel(tr("Pitch Shift:"), grpPitchTempo);
    lblPitch->setStyleSheet("font-weight: bold;");
    pitchGrid->addWidget(lblPitch, 0, 0);

    m_sliderPitch = new QSlider(Qt::Horizontal, grpPitchTempo);
    m_sliderPitch->setRange(-12, 12);
    m_sliderPitch->setValue(0);
    m_sliderPitch->setTickInterval(1);
    m_sliderPitch->setTickPosition(QSlider::TicksBelow);
    pitchGrid->addWidget(m_sliderPitch, 0, 1);

    m_lblPitchVal = new QLabel(tr("0 semitones (Original)"), grpPitchTempo);
    m_lblPitchVal->setFixedWidth(160);
    pitchGrid->addWidget(m_lblPitchVal, 0, 2);

    m_btnResetPitch = new QPushButton(tr("Reset"), grpPitchTempo);
    connect(m_btnResetPitch, &QPushButton::clicked, this, &AudioTrimDialog::onResetPitchClicked);
    pitchGrid->addWidget(m_btnResetPitch, 0, 3);

    connect(m_sliderPitch, &QSlider::valueChanged, this, [this](int val) {
        if (val == 0) {
            m_lblPitchVal->setText(tr("0 semitones (Original)"));
        } else if (val > 0) {
            m_lblPitchVal->setText(QString("+%1 %2").arg(val).arg(val == 1 ? tr("semitone") : tr("semitones")));
        } else {
            m_lblPitchVal->setText(QString("%1 %2").arg(val).arg(val == -1 ? tr("semitone") : tr("semitones")));
        }
        onFilterChanged();
    });

    // Time Stretch / Speed
    QLabel *lblSpeed = new QLabel(tr("Time Stretch (Speed):"), grpPitchTempo);
    lblSpeed->setStyleSheet("font-weight: bold;");
    pitchGrid->addWidget(lblSpeed, 1, 0);

    m_sliderSpeed = new QSlider(Qt::Horizontal, grpPitchTempo);
    m_sliderSpeed->setRange(50, 200); // 0.50x to 2.00x
    m_sliderSpeed->setValue(100);
    m_sliderSpeed->setTickInterval(25);
    m_sliderSpeed->setTickPosition(QSlider::TicksBelow);
    pitchGrid->addWidget(m_sliderSpeed, 1, 1);

    m_lblSpeedVal = new QLabel(tr("1.00x (Original)"), grpPitchTempo);
    m_lblSpeedVal->setFixedWidth(160);
    pitchGrid->addWidget(m_lblSpeedVal, 1, 2);

    m_btnResetSpeed = new QPushButton(tr("Reset"), grpPitchTempo);
    connect(m_btnResetSpeed, &QPushButton::clicked, this, &AudioTrimDialog::onResetSpeedClicked);
    pitchGrid->addWidget(m_btnResetSpeed, 1, 3);

    connect(m_sliderSpeed, &QSlider::valueChanged, this, [this](int val) {
        double factor = val / 100.0;
        m_lblSpeedVal->setText(QString("%1x %2").arg(QString::number(factor, 'f', 2), (val == 100 ? tr("(Original)") : QString())));
        onFilterChanged();
    });

    // Reverse checkbox
    m_chkReverse = new QCheckBox(tr("🔄 Reverse Audio Playback"), grpPitchTempo);
    connect(m_chkReverse, &QCheckBox::toggled, this, &AudioTrimDialog::onFilterChanged);
    pitchGrid->addWidget(m_chkReverse, 2, 0, 1, 4);

    // Remove Silence checkbox
    m_chkRemoveSilence = new QCheckBox(tr("✂️ Auto Silence Removal (Trim long gaps between sounds)"), grpPitchTempo);
    connect(m_chkRemoveSilence, &QCheckBox::toggled, this, &AudioTrimDialog::onFilterChanged);
    pitchGrid->addWidget(m_chkRemoveSilence, 3, 0, 1, 4);

    tabLayout->addWidget(grpPitchTempo);
    tabLayout->addStretch();

    return tab;
}

void AudioTrimDialog::onPresetSelected(int index)
{
    m_updatingPresetUI = true;

    switch (index) {
    case 0: // None
        m_sliderLowCut->setValue(0);
        m_sliderDenoise->setValue(0);
        m_comboDehum->setCurrentIndex(0);
        m_chkDeclick->setChecked(false);
        m_chkDeesser->setChecked(false);
        m_chkVoiceEnhance->setChecked(false);
        m_chkNoiseGate->setChecked(false);
        break;
    case 1: // Traffic, Wind & Low Rumble
        m_sliderLowCut->setValue(110);
        m_sliderDenoise->setValue(0);
        m_comboDehum->setCurrentIndex(0);
        m_chkDeclick->setChecked(false);
        m_chkDeesser->setChecked(false);
        m_chkVoiceEnhance->setChecked(false);
        m_chkNoiseGate->setChecked(false);
        break;
    case 2: // PC Fan & Microphone Hiss
        m_sliderLowCut->setValue(80);
        m_sliderDenoise->setValue(14);
        m_comboDehum->setCurrentIndex(0);
        m_chkDeclick->setChecked(false);
        m_chkDeesser->setChecked(false);
        m_chkVoiceEnhance->setChecked(false);
        m_chkNoiseGate->setChecked(false);
        break;
    case 3: // Electrical Mains Hum
        m_sliderLowCut->setValue(60);
        m_sliderDenoise->setValue(0);
        m_comboDehum->setCurrentIndex(1); // 50 Hz
        m_chkDeclick->setChecked(false);
        m_chkDeesser->setChecked(false);
        m_chkVoiceEnhance->setChecked(false);
        m_chkNoiseGate->setChecked(false);
        break;
    case 4: // Voice & Clear Podcast
        m_sliderLowCut->setValue(100);
        m_sliderDenoise->setValue(12);
        m_comboDehum->setCurrentIndex(1); // 50 Hz
        m_chkDeclick->setChecked(false);
        m_chkDeesser->setChecked(true);
        m_chkVoiceEnhance->setChecked(true);
        m_chkNoiseGate->setChecked(true);
        break;
    case 5: // Vinyl Restoration & Clean
        m_sliderLowCut->setValue(50);
        m_sliderDenoise->setValue(10);
        m_comboDehum->setCurrentIndex(1); // 50 Hz
        m_chkDeclick->setChecked(true);
        m_chkDeesser->setChecked(false);
        m_chkVoiceEnhance->setChecked(false);
        m_chkNoiseGate->setChecked(false);
        break;
    case 6: // Manual
    default:
        break;
    }

    m_updatingPresetUI = false;

    // Update readout labels
    int lowCut = m_sliderLowCut->value();
    m_lblLowCutVal->setText(lowCut > 0 ? QString("%1 Hz").arg(lowCut) : tr("Off"));
    int denoise = m_sliderDenoise->value();
    m_lblDenoiseVal->setText(denoise > 0 ? QString("%1 dB").arg(denoise) : tr("Off"));

    applyPreviewFilters();
}

void AudioTrimDialog::onManualControlChanged()
{
    int lowCut = m_sliderLowCut ? m_sliderLowCut->value() : 0;
    if (m_lblLowCutVal) m_lblLowCutVal->setText(lowCut > 0 ? QString("%1 Hz").arg(lowCut) : tr("Off"));

    int highCut = m_sliderHighCut ? m_sliderHighCut->value() : 20000;
    if (m_lblHighCutVal) m_lblHighCutVal->setText((highCut > 0 && highCut < 20000) ? QString("%1 kHz").arg(highCut / 1000.0, 0, 'f', 1) : tr("Off"));

    int denoise = m_sliderDenoise ? m_sliderDenoise->value() : 0;
    if (m_lblDenoiseVal) m_lblDenoiseVal->setText(denoise > 0 ? QString("%1 dB").arg(denoise) : tr("Off"));

    int vFreq = m_sliderVoiceFreq ? m_sliderVoiceFreq->value() : 3000;
    double vGain = m_sliderVoiceGain ? (m_sliderVoiceGain->value() / 10.0) : 3.5;
    if (m_lblVoiceVal) m_lblVoiceVal->setText(QString("+%1 dB @ %2 kHz").arg(vGain, 0, 'f', 1).arg(vFreq / 1000.0, 0, 'f', 1));

    int dFreq = m_sliderDeessFreq ? m_sliderDeessFreq->value() : 6500;
    double dDepth = m_sliderDeessDepth ? (m_sliderDeessDepth->value() / 10.0) : -6.0;
    if (m_lblDeessVal) m_lblDeessVal->setText(QString("%1 dB @ %2 kHz").arg(dDepth, 0, 'f', 1).arg(dFreq / 1000.0, 0, 'f', 1));

    if (!m_updatingPresetUI && m_comboPresets) {
        m_comboPresets->blockSignals(true);
        m_comboPresets->setCurrentIndex(6); // Manual
        m_comboPresets->blockSignals(false);
    }

    applyPreviewFilters();
}

void AudioTrimDialog::onBypassToggled(bool checked)
{
    m_isBypassActive = checked;
    if (m_btnBypass) {
        m_btnBypass->setText(checked ? tr("⚡ BYPASS ATTIVO (Suono Originale)") : tr("🎧 A/B Bypass (Compare Original)"));
    }
    applyPreviewFilters();
}

void AudioTrimDialog::onPlayFiltersClicked()
{
    if (m_isPreviewPlaying) {
        stopPreview();
    } else {
        startPreview();
    }
}

void AudioTrimDialog::onFiltersSeek(int val)
{
    if (m_previewMpv && m_meta.durationMs > 0) {
        double frac = static_cast<double>(val) / 1000.0;
        double sec = (frac * m_meta.durationMs) / 1000.0;
        mpv_command_string(m_previewMpv, QString("seek %1 absolute").arg(sec).toUtf8().constData());
    }
}

void AudioTrimDialog::applyTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::DarkSlate) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #22262b;"
            "   color: #eff0f1;"
            "   font-family: sans-serif;"
            "}"
            "QLabel {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QGroupBox {"
            "   border: 1px solid #2a2f35;"
            "   border-radius: 8px;"
            "   margin-top: 12px;"
            "   padding-top: 18px;"
            "   padding-bottom: 12px;"
            "   padding-left: 14px;"
            "   padding-right: 14px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "   color: #eff0f1;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 12px;"
            "   padding: 0 4px;"
            "   color: #61afef;"
            "}"
            "QDoubleSpinBox, QSpinBox {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   font-size: 13px;"
            "   selection-background-color: #0284c7;"
            "}"
            "QDoubleSpinBox:focus, QSpinBox:focus {"
            "   border-color: #61afef;"
            "}"
            "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
            "   background-color: #22262b;"
            "   border: none;"
            "   width: 18px;"
            "}"
            "QComboBox {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 6px 12px;"
            "   font-size: 13px;"
            "   min-height: 20px;"
            "}"
            "QComboBox:focus {"
            "   border-color: #61afef;"
            "}"
            "QComboBox::drop-down {"
            "   border: none;"
            "   background-color: transparent;"
            "   width: 24px;"
            "}"
            "QComboBox QAbstractItemView {"
            "   background-color: #1e2226;"
            "   color: #eff0f1;"
            "   border: 1px solid #2d333b;"
            "   selection-background-color: #383c42;"
            "   selection-color: #ffffff;"
            "   padding: 4px;"
            "}"
            "QCheckBox {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "   spacing: 8px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 18px;"
            "   height: 18px;"
            "   background-color: #282c32;"
            "   border: 1px solid #353b44;"
            "   border-radius: 4px;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0284c7;"
            "   border-color: #61afef;"
            "}"
            "QTabWidget::pane {"
            "   border: 1px solid #2a2f35;"
            "   border-radius: 8px;"
            "   background-color: #22262b;"
            "   top: -1px;"
            "}"
            "QTabBar::tab {"
            "   background-color: #1a1d21;"
            "   color: #959ca7;"
            "   border: 1px solid #2a2f35;"
            "   border-bottom: none;"
            "   border-top-left-radius: 6px;"
            "   border-top-right-radius: 6px;"
            "   padding: 7px 18px;"
            "   margin-right: 4px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: #22262b;"
            "   color: #61afef;"
            "   border-color: #2a2f35;"
            "   border-bottom: 1px solid #22262b;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "}"
            "QSlider::groove:horizontal {"
            "   height: 6px;"
            "   background: #282c32;"
            "   border-radius: 3px;"
            "}"
            "QSlider::sub-page:horizontal {"
            "   background: #0284c7;"
            "   border-radius: 3px;"
            "}"
            "QSlider::handle:horizontal {"
            "   background: #eff0f1;"
            "   border: 2px solid #0284c7;"
            "   width: 16px;"
            "   margin-top: -5px;"
            "   margin-bottom: -5px;"
            "   border-radius: 8px;"
            "}"
            "QSlider::handle:horizontal:hover {"
            "   background: #ffffff;"
            "   border-color: #61afef;"
            "}"
            "QPushButton {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 7px 16px;"
            "   font-size: 13px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #353b44;"
            "   color: #ffffff;"
            "}"
        );
    } else if (preset == ThemePreset::BreezeDark) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #181b1d;"
            "   color: #eff0f1;"
            "   font-family: sans-serif;"
            "}"
            "QLabel {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QGroupBox {"
            "   border: 1px solid #23272b;"
            "   border-radius: 8px;"
            "   margin-top: 12px;"
            "   padding-top: 18px;"
            "   padding-bottom: 12px;"
            "   padding-left: 14px;"
            "   padding-right: 14px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "   color: #eff0f1;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 12px;"
            "   padding: 0 4px;"
            "   color: #3daee9;"
            "}"
            "QDoubleSpinBox, QSpinBox {"
            "   background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   font-size: 13px;"
            "   selection-background-color: #3daee9;"
            "}"
            "QDoubleSpinBox:focus, QSpinBox:focus {"
            "   border-color: #3daee9;"
            "}"
            "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
            "   background-color: #181b1d;"
            "   border: none;"
            "   width: 18px;"
            "}"
            "QComboBox {"
            "   background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 6px 12px;"
            "   font-size: 13px;"
            "   min-height: 20px;"
            "}"
            "QComboBox:focus {"
            "   border-color: #3daee9;"
            "}"
            "QComboBox::drop-down {"
            "   border: none;"
            "   background-color: transparent;"
            "   width: 24px;"
            "}"
            "QComboBox QAbstractItemView {"
            "   background-color: #1e2226;"
            "   color: #eff0f1;"
            "   border: 1px solid #2d333b;"
            "   selection-background-color: #2a3037;"
            "   selection-color: #ffffff;"
            "   padding: 4px;"
            "}"
            "QCheckBox {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "   spacing: 8px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 18px;"
            "   height: 18px;"
            "   background-color: #22262a;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 4px;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #3daee9;"
            "   border-color: #3daee9;"
            "}"
            "QTabWidget::pane {"
            "   border: 1px solid #31363b;"
            "   border-radius: 6px;"
            "   background-color: #232629;"
            "   top: -1px;"
            "}"
            "QTabBar::tab {"
            "   background-color: #1b1e20;"
            "   color: #bdc3c7;"
            "   border: 1px solid #31363b;"
            "   border-bottom: none;"
            "   border-top-left-radius: 5px;"
            "   border-top-right-radius: 5px;"
            "   padding: 7px 18px;"
            "   margin-right: 3px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: #232629;"
            "   color: #3daee9;"
            "   border-color: #31363b;"
            "   border-bottom: 1px solid #232629;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "   background-color: #2a2e32;"
            "   color: #eff0f1;"
            "}"
            "QSlider::groove:horizontal {"
            "   height: 6px;"
            "   background: #2a2e32;"
            "   border-radius: 3px;"
            "}"
            "QSlider::sub-page:horizontal {"
            "   background: #3daee9;"
            "   border-radius: 3px;"
            "}"
            "QSlider::handle:horizontal {"
            "   background: #eff0f1;"
            "   border: 2px solid #3daee9;"
            "   width: 16px;"
            "   margin-top: -5px;"
            "   margin-bottom: -5px;"
            "   border-radius: 8px;"
            "}"
            "QPushButton {"
            "   background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 7px 16px;"
            "   font-size: 13px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2a3037;"
            "   color: #ffffff;"
            "}"
        );
    } else {
        setStyleSheet(
            "QDialog {"
            "   background-color: palette(window);"
            "   color: palette(window-text);"
            "   font-family: sans-serif;"
            "}"
            "QGroupBox {"
            "   border: 1px solid palette(mid);"
            "   border-radius: 8px;"
            "   margin-top: 12px;"
            "   padding-top: 18px;"
            "   padding-bottom: 12px;"
            "   padding-left: 14px;"
            "   padding-right: 14px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "   color: palette(window-text);"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 12px;"
            "   padding: 0 4px;"
            "   color: palette(highlight);"
            "}"
            "QDoubleSpinBox, QSpinBox {"
            "   background-color: palette(base);"
            "   color: palette(window-text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   font-size: 13px;"
            "}"
            "QComboBox {"
            "   background-color: palette(base);"
            "   color: palette(window-text);"
            "   border: 1px solid palette(mid);"
            "   border-radius: 6px;"
            "   padding: 6px 12px;"
            "   font-size: 13px;"
            "   min-height: 20px;"
            "}"
            "QCheckBox {"
            "   color: palette(window-text);"
            "   font-size: 13px;"
            "   spacing: 8px;"
            "}"
            "QTabWidget::pane {"
            "   border: 1px solid #cbd5e1;"
            "   border-radius: 6px;"
            "   background-color: palette(window);"
            "   top: -1px;"
            "}"
            "QTabBar::tab {"
            "   background-color: #f1f5f9;"
            "   color: #475569;"
            "   border: 1px solid #cbd5e1;"
            "   border-bottom: none;"
            "   border-top-left-radius: 6px;"
            "   border-top-right-radius: 6px;"
            "   padding: 7px 18px;"
            "   margin-right: 4px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: palette(window);"
            "   color: #0284c7;"
            "   border-color: #cbd5e1;"
            "   border-bottom: 1px solid palette(window);"
            "}"
            "QTabBar::tab:hover:!selected {"
            "   background-color: #e2e8f0;"
            "   color: #1e293b;"
            "}"
            "QSlider::groove:horizontal {"
            "   height: 6px;"
            "   background: #e2e8f0;"
            "   border-radius: 3px;"
            "}"
            "QSlider::sub-page:horizontal {"
            "   background: #0284c7;"
            "   border-radius: 3px;"
            "}"
            "QSlider::handle:horizontal {"
            "   background: #ffffff;"
            "   border: 2px solid #0284c7;"
            "   width: 16px;"
            "   margin-top: -5px;"
            "   margin-bottom: -5px;"
            "   border-radius: 8px;"
            "}"
            "QSlider::handle:horizontal:hover {"
            "   background: #0284c7;"
            "}"
            "QPushButton {"
            "   background-color: palette(button);"
            "   color: palette(button-text);"
            "   border: 1px solid #cbd5e1;"
            "   border-radius: 6px;"
            "   padding: 7px 16px;"
            "   font-size: 13px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #f1f5f9;"
            "}"
        );
    }
}

void AudioTrimDialog::updateTimeLabels()
{
    m_lblStart->setText(tr("Start [A]: %1").arg(formatTime(m_startMs)));
    m_lblEnd->setText(tr("End [B]: %1").arg(formatTime(m_endMs)));
    qint64 diff = qMax<qint64>(0, m_endMs - m_startMs);
    m_lblDuration->setText(tr("Clip Duration: %1").arg(formatTime(diff)));
}

QString AudioTrimDialog::formatTime(qint64 ms) const
{
    qint64 totalSeconds = ms / 1000;
    qint64 minutes = totalSeconds / 60;
    qint64 seconds = totalSeconds % 60;
    qint64 milli = (ms % 1000) / 100;
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milli);
}

void AudioTrimDialog::onRangeChangedFromWaveform(qint64 startMs, qint64 endMs)
{
    m_startMs = startMs;
    m_endMs = endMs;
    updateTimeLabels();
    if (m_isPreviewPlaying) {
        startPreview();
    } else {
        applyPreviewFilters();
    }
}

void AudioTrimDialog::onWaveformSeek(qint64 posMs)
{
    if (m_previewMpv) {
        double sec = static_cast<double>(posMs) / 1000.0;
        mpv_command_string(m_previewMpv, QString("seek %1 absolute").arg(sec).toUtf8().constData());
    }
}

void AudioTrimDialog::onSetStartCurrent()
{
    if (m_previewMpv) {
        double pos = 0.0;
        mpv_get_property(m_previewMpv, "time-pos", MPV_FORMAT_DOUBLE, &pos);
        qint64 currentMs = static_cast<qint64>(pos * 1000.0);
        if (currentMs < m_endMs) {
            m_startMs = currentMs;
            m_waveform->setSelection(m_startMs, m_endMs, m_meta.durationMs);
            updateTimeLabels();
            applyPreviewFilters();
        }
    }
}

void AudioTrimDialog::onSetEndCurrent()
{
    if (m_previewMpv) {
        double pos = 0.0;
        mpv_get_property(m_previewMpv, "time-pos", MPV_FORMAT_DOUBLE, &pos);
        qint64 currentMs = static_cast<qint64>(pos * 1000.0);
        if (currentMs > m_startMs) {
            m_endMs = currentMs;
            m_waveform->setSelection(m_startMs, m_endMs, m_meta.durationMs);
            updateTimeLabels();
            applyPreviewFilters();
        }
    }
}

void AudioTrimDialog::onPlayPreviewClicked()
{
    if (m_isPreviewPlaying) {
        stopPreview();
    } else {
        startPreview();
    }
}

void AudioTrimDialog::applyPreviewFilters()
{
    if (!m_previewMpv) return;

    QStringList filters;
    double startSec = static_cast<double>(m_startMs) / 1000.0;
    double durationSec = (m_endMs > m_startMs) ? static_cast<double>(m_endMs - m_startMs) / 1000.0 : 0.0;
    double fadeIn = m_spinFadeIn ? m_spinFadeIn->value() : 0.0;
    double fadeOut = m_spinFadeOut ? m_spinFadeOut->value() : 0.0;

    if (fadeIn > 0.0) {
        filters << QString("lavfi=[afade=t=in:st=%1:d=%2]").arg(QString::number(startSec, 'f', 3), QString::number(fadeIn, 'f', 2));
    }
    if (fadeOut > 0.0 && durationSec > fadeOut) {
        double fadeStart = startSec + durationSec - fadeOut;
        filters << QString("lavfi=[afade=t=out:st=%1:d=%2]").arg(QString::number(fadeStart, 'f', 3), QString::number(fadeOut, 'f', 2));
    }

    // If bypass is active, do not apply any acoustic filters so user hears raw original audio
    if (m_isBypassActive) {
        mpv_set_property_string(m_previewMpv, "af", "");
        if (m_spectrumWidget) {
            m_spectrumWidget->setFilterParameters(0, 0, 0, false, false);
        }
        return;
    }

    int lowCut = m_sliderLowCut ? m_sliderLowCut->value() : 0;
    if (lowCut > 0) {
        filters << QString("lavfi=[highpass=f=%1]").arg(lowCut);
    }

    int highCut = m_sliderHighCut ? m_sliderHighCut->value() : 20000;
    if (highCut > 0 && highCut < 20000) {
        filters << QString("lavfi=[lowpass=f=%1]").arg(highCut);
    }

    int dehum = m_comboDehum ? m_comboDehum->currentData().toInt() : 0;
    if (dehum > 0) {
        filters << QString("lavfi=[bandreject=f=%1:w=5]").arg(dehum);
    }

    if (m_chkDeclick && m_chkDeclick->isChecked()) {
        filters << "lavfi=[adeclick=w=55:o=75:a=2:m=a]";
    }

    int denoise = m_sliderDenoise ? m_sliderDenoise->value() : 0;
    if (denoise > 0) {
        filters << QString("lavfi=[afftdn=nr=%1:nf=-25:tn=1]").arg(denoise);
    }

    int deessFreq = m_sliderDeessFreq ? m_sliderDeessFreq->value() : 6500;
    double deessDepth = m_sliderDeessDepth ? (m_sliderDeessDepth->value() / 10.0) : -6.0;
    if (m_chkDeesser && m_chkDeesser->isChecked()) {
        filters << QString("lavfi=[equalizer=f=%1:width_type=q:w=1.5:g=%2]").arg(deessFreq).arg(deessDepth, 0, 'f', 1);
    }

    if (m_chkNoiseGate && m_chkNoiseGate->isChecked()) {
        filters << "lavfi=[agate=threshold=-35dB:ratio=3:range=-40dB:attack=10:release=150]";
    }

    int voiceFreq = m_sliderVoiceFreq ? m_sliderVoiceFreq->value() : 3000;
    double voiceGain = m_sliderVoiceGain ? (m_sliderVoiceGain->value() / 10.0) : 3.5;
    if (m_chkVoiceEnhance && m_chkVoiceEnhance->isChecked()) {
        filters << QString("lavfi=[speechnorm=e=4:r=0.0001:l=1,equalizer=f=%1:width_type=q:w=1:g=%2]").arg(voiceFreq).arg(voiceGain, 0, 'f', 1);
    }

    if (m_chkNormalize && m_chkNormalize->isChecked()) {
        filters << "lavfi=[loudnorm=I=-16:TP=-1.5:LRA=11]";
    }

    // Spatial & Channel Modes
    int chMode = m_comboChannels ? m_comboChannels->currentData().toInt() : 0;
    switch (static_cast<ChannelMode>(chMode)) {
    case ChannelMode::StereoToMono:
        filters << "lavfi=[pan=mono|c0=0.5*c0+0.5*c1]";
        break;
    case ChannelMode::SwapChannels:
        filters << "lavfi=[pan=stereo|c0=c1|c1=c0]";
        break;
    case ChannelMode::LeftOnly:
        filters << "lavfi=[pan=stereo|c0=c0|c1=c0]";
        break;
    case ChannelMode::RightOnly:
        filters << "lavfi=[pan=stereo|c0=c1|c1=c1]";
        break;
    case ChannelMode::VocalRemover:
        filters << "lavfi=[pan=stereo|c0=c0-c1|c1=c1-c0]";
        break;
    default:
        break;
    }

    // Pitch Shift in semitones
    int pitchSemitones = m_sliderPitch ? m_sliderPitch->value() : 0;
    if (pitchSemitones != 0) {
        double scale = std::pow(2.0, static_cast<double>(pitchSemitones) / 12.0);
        filters << QString("lavfi=[rubberband=pitch=%1]").arg(scale, 0, 'f', 4);
    }

    // Time Stretch / Speed
    int speedVal = m_sliderSpeed ? m_sliderSpeed->value() : 100;
    if (speedVal != 100) {
        double speed = std::clamp(speedVal / 100.0, 0.5, 2.0);
        filters << QString("lavfi=[atempo=%1]").arg(speed, 0, 'f', 2);
    }

    // Audio Reverse
    if (m_chkReverse && m_chkReverse->isChecked()) {
        filters << "lavfi=[areverse]";
    }

    // Intelligent silence removal
    if (m_chkRemoveSilence && m_chkRemoveSilence->isChecked()) {
        filters << "lavfi=[silenceremove=start_periods=1:start_duration=0.1:start_threshold=-50dB:stop_periods=1:stop_duration=0.5:stop_threshold=-50dB]";
    }

    // 10-Band Graphic Equalizer, Pre-Amp, Limiter, Stereo Width
    if (m_equalizerWidget && !m_equalizerWidget->isBypassed()) {
        QString eqMpv = EqualizerManager::buildMpvAfString(m_equalizerWidget->currentPreset());
        if (!eqMpv.isEmpty()) {
            filters << eqMpv;
        }
    }

    QString afStr = filters.join(",");
    mpv_set_property_string(m_previewMpv, "af", afStr.toUtf8().constData());

    if (m_spectrumWidget) {
        m_spectrumWidget->setFilterParameters(lowCut, denoise, dehum,
                                              m_chkVoiceEnhance && m_chkVoiceEnhance->isChecked(),
                                              m_chkDeesser && m_chkDeesser->isChecked(),
                                              voiceFreq, voiceGain,
                                              deessFreq, deessDepth,
                                              highCut);
    }

    int targetSr = m_comboSampleRate ? m_comboSampleRate->currentData().toInt() : 0;
    int targetBr = m_comboBitrate ? m_comboBitrate->currentData().toInt() : 0;

    bool hasAudioFilters = (lowCut > 0) || (highCut < 20000) || (dehum > 0) || (denoise > 0) ||
                           (m_chkDeclick && m_chkDeclick->isChecked()) ||
                           (m_chkDeesser && m_chkDeesser->isChecked()) ||
                           (m_chkNoiseGate && m_chkNoiseGate->isChecked()) ||
                           (m_chkVoiceEnhance && m_chkVoiceEnhance->isChecked()) ||
                           (m_chkNormalize && m_chkNormalize->isChecked()) ||
                           (fadeIn > 0.0) || (fadeOut > 0.0) ||
                           (chMode != 0) || (pitchSemitones != 0) || (speedVal != 100) ||
                           (m_chkReverse && m_chkReverse->isChecked()) ||
                           (m_chkRemoveSilence && m_chkRemoveSilence->isChecked()) ||
                           (m_equalizerWidget && !m_equalizerWidget->isBypassed()) ||
                           (targetSr > 0) || (targetBr > 0);

    if (m_chkLossless) {
        if (hasAudioFilters) {
            m_chkLossless->blockSignals(true);
            m_chkLossless->setChecked(false);
            m_chkLossless->setEnabled(false);
            m_chkLossless->setToolTip(tr("Lossless copy is disabled because audio filters or fades require re-encoding."));
            m_chkLossless->blockSignals(false);
            if (m_lblStatusBadge) {
                m_lblStatusBadge->setStyleSheet("color: #f59e0b; font-size: 11px; font-weight: bold; padding: 3px 8px; background: rgba(245, 158, 11, 0.12); border-radius: 4px; border: 1px solid rgba(245, 158, 11, 0.25);");
                m_lblStatusBadge->setText(tr("🎛️ Re-encoding (Audio Filters Active)"));
            }
        } else {
            QString fmt = m_comboFormat ? m_comboFormat->currentData().toString() : "copy";
            bool canBeLossless = (fmt == "copy");
            m_chkLossless->setEnabled(canBeLossless);
            if (canBeLossless) {
                m_chkLossless->setChecked(true);
                m_chkLossless->setToolTip(QString());
            }
            if (m_lblStatusBadge) {
                m_lblStatusBadge->setStyleSheet("color: #10b981; font-size: 11px; font-weight: bold; padding: 3px 8px; background: rgba(16, 185, 129, 0.12); border-radius: 4px; border: 1px solid rgba(16, 185, 129, 0.25);");
                m_lblStatusBadge->setText(tr("⚡ Instant Lossless Ready"));
            }
        }
    }
}

void AudioTrimDialog::onFilterChanged()
{
    applyPreviewFilters();
}

void AudioTrimDialog::startPreview()
{
    if (!m_previewMpv) return;

    applyPreviewFilters();

    QByteArray pathBytes = m_filePath.toUtf8();
    const char *cmd[] = {"loadfile", pathBytes.constData(), "replace", nullptr};
    mpv_command(m_previewMpv, cmd);

    // If Tab 1 is active, seek to startMs; if Tab 2 is active and user seeked, keep position
    double startSec = static_cast<double>(m_startMs) / 1000.0;
    if (m_tabWidget && m_tabWidget->currentIndex() == 1 && m_sliderSeekFilters && m_meta.durationMs > 0) {
        startSec = (static_cast<double>(m_sliderSeekFilters->value()) / 1000.0) * (static_cast<double>(m_meta.durationMs) / 1000.0);
    }
    mpv_command_string(m_previewMpv, QString("seek %1 absolute").arg(startSec).toUtf8().constData());

    applyPreviewFilters();

    int pause = 0;
    mpv_set_property(m_previewMpv, "pause", MPV_FORMAT_FLAG, &pause);

    m_isPreviewPlaying = true;
    if (m_btnPlayPreview) m_btnPlayPreview->setText(tr("⏸ Pause Preview"));
    if (m_btnPlayFilters) m_btnPlayFilters->setText(tr("⏸ Pause"));
    if (m_spectrumWidget) m_spectrumWidget->setPlaying(true);
    m_playbackTimer->start(50);
}

void AudioTrimDialog::stopPreview()
{
    if (!m_previewMpv) return;
    const char *cmd[] = {"stop", nullptr};
    mpv_command(m_previewMpv, cmd);
    m_isPreviewPlaying = false;
    if (m_btnPlayPreview) m_btnPlayPreview->setText(tr("▶ Play Selection (A-B Loop)"));
    if (m_btnPlayFilters) m_btnPlayFilters->setText(tr("▶ Play Preview"));
    if (m_spectrumWidget) m_spectrumWidget->setPlaying(false);
    if (m_equalizerWidget) m_equalizerWidget->updateVuMeter(0.0f, 0.0f);
    m_playbackTimer->stop();
    if (m_waveform) m_waveform->setCurrentPlayPosition(0, m_meta.durationMs);
}

void AudioTrimDialog::updatePreviewTimer()
{
    if (!m_previewMpv || !m_isPreviewPlaying) return;

    double pos = 0.0;
    if (mpv_get_property(m_previewMpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) == 0) {
        qint64 currentMs = static_cast<qint64>(pos * 1000.0);
        if (m_waveform) m_waveform->setCurrentPlayPosition(currentMs, m_meta.durationMs);

        // Compute and pass real FFT magnitudes to SpectrumWidget and peaks to Equalizer VU meter
        if (!m_pcmData.isEmpty() && m_pcmSampleRate > 0) {
            int centerIdx = static_cast<int>((static_cast<double>(currentMs) / 1000.0) * m_pcmSampleRate);
            int windowSize = 1024;
            int startIdx = std::max(0, centerIdx - windowSize / 2);
            int endIdx = std::min(static_cast<int>(m_pcmData.size()), startIdx + windowSize);

            if (endIdx - startIdx >= 256) {
                QVector<float> slice;
                slice.reserve(endIdx - startIdx);
                for (int i = startIdx; i < endIdx; ++i) {
                    slice.append(m_pcmData[i]);
                }

                // Feed real-time peaks to Equalizer VU meter
                if (m_equalizerWidget) {
                    float peak = 0.0f;
                    for (float s : slice) {
                        float a = std::abs(s);
                        if (a > peak) peak = a;
                    }
                    float eqGain = 1.0f;
                    if (!m_equalizerWidget->isBypassed()) {
                        eqGain = std::pow(10.0f, m_equalizerWidget->currentPreset().preAmpDb / 20.0f);
                    }
                    float peakL = std::clamp(peak * eqGain, 0.0f, 1.0f);
                    float peakR = std::clamp(peak * eqGain * 0.95f, 0.0f, 1.0f);
                    m_equalizerWidget->updateVuMeter(peakL, peakR);
                }

                if (m_spectrumWidget) {
                    QVector<float> fftBands = AudioProcessor::computeFftMagnitudes(slice, m_pcmSampleRate, 36);

                    // Modulate FFT bands by active filters (if not bypassed)
                    if (!m_isBypassActive) {
                        int lowCut = m_sliderLowCut ? m_sliderLowCut->value() : 0;
                        int denoise = m_sliderDenoise ? m_sliderDenoise->value() : 0;
                        bool voiceEnhance = m_chkVoiceEnhance && m_chkVoiceEnhance->isChecked();
                        bool deesser = m_chkDeesser && m_chkDeesser->isChecked();

                        // Apply filter attenuation/boost directly onto display bands
                        double minLog = std::log10(20.0);
                        double maxLog = std::log10(20000.0);
                        for (int b = 0; b < fftBands.size(); ++b) {
                            double frac = (static_cast<double>(b) + 0.5) / fftBands.size();
                            double freq = std::pow(10.0, minLog + frac * (maxLog - minLog));

                            if (lowCut > 0 && freq < lowCut) {
                                double att = std::pow(freq / lowCut, 2.0);
                                fftBands[b] *= static_cast<float>(att);
                            }
                            if (denoise > 0) {
                                fftBands[b] = std::max(0.0f, fftBands[b] - (denoise * 0.012f));
                            }
                            if (deesser && std::abs(freq - 6500.0) < 2500.0) {
                                fftBands[b] *= 0.65f;
                            }
                            if (voiceEnhance && std::abs(freq - 3000.0) < 1500.0) {
                                fftBands[b] = std::min(1.0f, fftBands[b] * 1.35f);
                            }
                        }
                    }

                    m_spectrumWidget->setSpectrumFftData(fftBands);
                }
            }
        }

        // Update Filters Tab mini-player
        if (m_sliderSeekFilters && m_meta.durationMs > 0) {
            int sliderVal = static_cast<int>((static_cast<double>(currentMs) / m_meta.durationMs) * 1000.0);
            m_sliderSeekFilters->blockSignals(true);
            m_sliderSeekFilters->setValue(std::min(1000, std::max(0, sliderVal)));
            m_sliderSeekFilters->blockSignals(false);
        }
        if (m_lblTimeFilters) {
            m_lblTimeFilters->setText(QString("%1 / %2").arg(formatTime(currentMs), formatTime(m_meta.durationMs)));
        }

        // Loop behavior: in Tab 0 loop selection [startMs - endMs], in Tab 1 loop whole file
        if (m_tabWidget && m_tabWidget->currentIndex() == 0) {
            if (currentMs >= m_endMs || currentMs < m_startMs - 500) {
                double startSec = static_cast<double>(m_startMs) / 1000.0;
                mpv_command_string(m_previewMpv, QString("seek %1 absolute").arg(startSec).toUtf8().constData());
                applyPreviewFilters();
            }
        } else {
            if (m_meta.durationMs > 0 && currentMs >= m_meta.durationMs) {
                mpv_command_string(m_previewMpv, "seek 0 absolute");
                applyPreviewFilters();
            }
        }
    }
}

void AudioTrimDialog::onFormatChanged(int)
{
    QString fmt = m_comboFormat->currentData().toString();
    if (fmt != "copy") {
        m_chkLossless->blockSignals(true);
        m_chkLossless->setChecked(false);
        m_chkLossless->setEnabled(false);
        m_chkLossless->setToolTip(tr("Lossless mode is only available when keeping original format."));
        m_chkLossless->blockSignals(false);
        if (m_lblStatusBadge) {
            m_lblStatusBadge->setStyleSheet("color: #0284c7; font-size: 11px; font-weight: bold; padding: 3px 8px; background: rgba(2, 132, 199, 0.12); border-radius: 4px; border: 1px solid rgba(2, 132, 199, 0.25);");
            m_lblStatusBadge->setText(tr("🔄 Transcoding to %1").arg(fmt.toUpper()));
        }
    } else {
        applyPreviewFilters();
    }
}

void AudioTrimDialog::onSaveClicked()
{
    stopPreview();

    QFileInfo fi(m_filePath);
    QString ext = m_comboFormat->currentData().toString();
    if (ext == "copy") {
        ext = fi.suffix();
    }

    QString defaultName = fi.dir().filePath(QString("%1_processed.%2").arg(fi.baseName(), ext));
    QString dest = QFileDialog::getSaveFileName(this, tr("Save Audio"), defaultName, QString("Audio (*.%1)").arg(ext));
    if (dest.isEmpty()) return;

    AudioTrimOptions opts;
    opts.startMs = m_startMs;
    opts.endMs = m_endMs;
    opts.fadeInSec = m_spinFadeIn->value();
    opts.fadeOutSec = m_spinFadeOut->value();
    opts.lowCutHz = m_sliderLowCut->value();
    opts.highCutHz = m_sliderHighCut ? m_sliderHighCut->value() : 20000;
    opts.denoiseDb = m_sliderDenoise->value();
    opts.dehumHz = m_comboDehum->currentData().toInt();
    opts.declick = m_chkDeclick->isChecked();
    opts.deesser = m_chkDeesser->isChecked();
    opts.deessFreqHz = m_sliderDeessFreq ? m_sliderDeessFreq->value() : 6500;
    opts.deessDepthDb = m_sliderDeessDepth ? (m_sliderDeessDepth->value() / 10.0) : -6.0;
    opts.voiceEnhancer = m_chkVoiceEnhance->isChecked();
    opts.voiceFreqHz = m_sliderVoiceFreq ? m_sliderVoiceFreq->value() : 3000;
    opts.voiceGainDb = m_sliderVoiceGain ? (m_sliderVoiceGain->value() / 10.0) : 3.5;
    opts.normalizeLoudness = m_chkNormalize->isChecked();
    opts.noiseGate = m_chkNoiseGate->isChecked();
    opts.outputFormat = m_comboFormat->currentData().toString();
    opts.channelMode = static_cast<ChannelMode>(m_comboChannels ? m_comboChannels->currentData().toInt() : 0);
    opts.pitchSemitones = m_sliderPitch ? m_sliderPitch->value() : 0;
    opts.timeStretch = m_sliderSpeed ? (m_sliderSpeed->value() / 100.0) : 1.0;
    opts.reverseAudio = m_chkReverse && m_chkReverse->isChecked();
    opts.removeSilence = m_chkRemoveSilence && m_chkRemoveSilence->isChecked();
    opts.targetBitrateKbps = m_comboBitrate ? m_comboBitrate->currentData().toInt() : 0;
    opts.targetSampleRate = m_comboSampleRate ? m_comboSampleRate->currentData().toInt() : 0;

    if (m_equalizerWidget && !m_equalizerWidget->isBypassed()) {
        opts.equalizerEnabled = true;
        opts.equalizerPreset = m_equalizerWidget->currentPreset();
    }

    opts.losslessCopy = m_chkLossless->isChecked() &&
                        (opts.fadeInSec == 0.0) && (opts.fadeOutSec == 0.0) &&
                        (opts.lowCutHz == 0) && (opts.highCutHz >= 20000) &&
                        (opts.denoiseDb == 0) && (opts.dehumHz == 0) &&
                        !opts.declick && !opts.deesser &&
                        !opts.voiceEnhancer && !opts.normalizeLoudness && !opts.noiseGate &&
                        (opts.channelMode == ChannelMode::Original) &&
                        (opts.pitchSemitones == 0) && (opts.timeStretch == 1.0) &&
                        !opts.reverseAudio && !opts.removeSilence &&
                        !opts.equalizerEnabled &&
                        (opts.targetSampleRate == 0) && (opts.targetBitrateKbps == 0);

    m_progressBar->setVisible(true);
    m_progressBar->setValue(30);
    m_btnSave->setEnabled(false);
    qApp->processEvents();

    bool success = AudioProcessor::trimAudio(m_filePath, dest, opts, [this](int prog) {
        m_progressBar->setValue(prog);
        qApp->processEvents();
    });

    m_progressBar->setVisible(false);
    m_btnSave->setEnabled(true);

    if (success) {
        m_savedPath = dest;
        QMessageBox::information(this, tr("Success"), tr("Audio saved successfully!\n%1").arg(dest));
        accept();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to process or export audio file. Please check permissions or format compatibility."));
    }
}

void AudioTrimDialog::onAutoDetectSilenceClicked()
{
    qint64 startMs = 0;
    qint64 endMs = m_meta.durationMs;
    bool ok = AudioProcessor::detectAudioBoundaries(m_filePath, startMs, endMs, -45);
    if (ok) {
        m_startMs = startMs;
        m_endMs = endMs;
        if (m_waveform) {
            m_waveform->setSelection(m_startMs, m_endMs, m_meta.durationMs);
        }
        updateTimeLabels();
        applyPreviewFilters();
        QMessageBox::information(this, tr("Auto-Snap Silence"),
                                tr("Silence detected successfully!\nAudio start: %1\nAudio end: %2")
                                .arg(formatTime(m_startMs), formatTime(m_endMs)));
    } else {
        QMessageBox::warning(this, tr("Silence Detection"), tr("Could not detect silence or no silent intervals found."));
    }
}

void AudioTrimDialog::onSplitCueClicked()
{
    stopPreview();
    QFileInfo fi(m_filePath);
    QString autoCuePath = fi.dir().filePath(fi.completeBaseName() + ".cue");
    QString cuePath = autoCuePath;

    if (!QFileInfo::exists(cuePath)) {
        cuePath = QFileDialog::getOpenFileName(this, tr("Select CUE Sheet File"), fi.dir().path(), tr("CUE Sheet (*.cue)"));
        if (cuePath.isEmpty()) return;
    }

    QList<CueTrack> tracks = AudioProcessor::parseCueSheet(cuePath, m_meta.durationMs);
    if (tracks.isEmpty()) {
        QMessageBox::warning(this, tr("CUE Sheet Error"), tr("No valid tracks found in CUE file:\n%1").arg(cuePath));
        return;
    }

    int ret = QMessageBox::question(this, tr("Split CUE Tracks"),
                                    tr("Found %1 tracks in CUE file.\nProceed with splitting into audio folder?")
                                    .arg(tracks.size()));
    if (ret != QMessageBox::Yes) return;

    QString outputDir = QFileDialog::getExistingDirectory(this, tr("Destination Directory"), fi.dir().path());
    if (outputDir.isEmpty()) return;

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    qApp->processEvents();

    bool success = AudioProcessor::splitCueSheet(m_filePath, cuePath, outputDir, [this](int cur, int tot, const QString &title) {
        int pct = static_cast<int>((static_cast<double>(cur) / tot) * 100.0);
        m_progressBar->setValue(pct);
        if (m_lblStatusBadge) {
            m_lblStatusBadge->setText(tr("Track %1/%2: %3").arg(cur).arg(tot).arg(title));
        }
        qApp->processEvents();
    });

    m_progressBar->setVisible(false);

    if (success) {
        QMessageBox::information(this, tr("Success"), tr("Successfully split %1 tracks into destination folder:\n%2").arg(tracks.size()).arg(outputDir));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("An error occurred while splitting tracks."));
    }
}

void AudioTrimDialog::onOpenTagEditorClicked()
{
    AudioTagDialog tagDlg(m_filePath, QStringList(), this);
    if (tagDlg.exec() == QDialog::Accepted) {
        // Refresh metadata
        m_meta = AudioProcessor::readMetadata(m_filePath);
        if (m_lblInfo) {
            QFileInfo fi(m_filePath);
            ThemePreset preset = ThemeManager::currentPreset();
            bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);
            QString titleColor = isDark ? "#eff0f1" : "#1e293b";
            QString subColor = isDark ? "#959ca7" : "#64748b";
            QString infoText = QString("<div style='font-size: 14px; font-weight: bold; color: %1;'>%2</div>"
                                       "<div style='font-size: 11px; color: %3; margin-top: 2px;'>%4 &nbsp;|&nbsp; %5 kHz &nbsp;|&nbsp; %6 %7 &nbsp;|&nbsp; %8 kbps</div>")
                                   .arg(titleColor, fi.fileName(),
                                        subColor,
                                        m_meta.codecName.isEmpty() ? fi.suffix().toUpper() : m_meta.codecName,
                                        QString::number(m_meta.sampleRate / 1000.0, 'f', 1),
                                        QString::number(m_meta.channels),
                                        (m_meta.channels == 2 ? tr("channels (Stereo)") : tr("channel (Mono)")),
                                        QString::number(m_meta.bitRate));
            m_lblInfo->setText(infoText);
        }
    }
}

void AudioTrimDialog::onResetPitchClicked()
{
    if (m_sliderPitch) {
        m_sliderPitch->setValue(0);
    }
}

void AudioTrimDialog::onResetSpeedClicked()
{
    if (m_sliderSpeed) {
        m_sliderSpeed->setValue(100);
    }
}
