#include "smooth_waveform_scrubber.h"
#include "core/audio/audio_processor.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QThreadPool>
#include <QPointer>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

SmoothWaveformScrubber::SmoothWaveformScrubber(QWidget *parent)
    : QWidget(parent)
    , m_peaks(makeDefaultEnvelope(110))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(34);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

QVector<float> SmoothWaveformScrubber::makeDefaultEnvelope(int count)
{
    QVector<float> peaks(count);
    for (int i = 0; i < count; ++i) {
        double t = static_cast<double>(i) / std::max(1, count);
        double env = std::sin(t * M_PI);
        double v1 = std::abs(std::sin(i * 0.37) * std::cos(i * 0.19));
        double v2 = 0.4 * std::abs(std::sin(i * 1.13));
        double val = (0.18 + 0.82 * (v1 * 0.7 + v2 * 0.3)) * (0.25 + 0.75 * env);
        if (t > 0.32 && t < 0.38) val *= 0.35;
        if (t > 0.68 && t < 0.73) val *= 0.40;
        peaks[i] = std::clamp(static_cast<float>(val), 0.08f, 1.0f);
    }
    return peaks;
}

void SmoothWaveformScrubber::loadAudioWaveform(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_peaks = makeDefaultEnvelope(110);
    m_progress = 0.0;
    m_currentMs = 0;
    update();

    if (filePath.isEmpty()) return;

    QPointer<SmoothWaveformScrubber> guard(this);
    QString targetPath = filePath;
    QThreadPool::globalInstance()->start([guard, targetPath]() {
        if (!guard || guard->m_currentFilePath != targetPath) {
            return;
        }
        QVector<float> rawPeaks = AudioProcessor::generateWaveformPeaks(targetPath, 110);
        // Check if real non-flat peaks were extracted
        float maxPeak = 0.0f;
        for (float v : rawPeaks) {
            if (v > maxPeak) maxPeak = v;
        }
        if (maxPeak <= 0.06f || rawPeaks.size() < 10) {
            return;
        }

        // Apply perceptual power compression + local contrast + 3-tap smoothing
        const int n = rawPeaks.size();
        QVector<float> compressed(n);
        for (int i = 0; i < n; ++i) {
            float localMax = 0.01f;
            for (int k = std::max(0, i - 8); k <= std::min(n - 1, i + 8); ++k) {
                if (rawPeaks[k] > localMax) localMax = rawPeaks[k];
            }
            float globalNorm = std::clamp(rawPeaks[i] / maxPeak, 0.0f, 1.0f);
            float localNorm = std::clamp(rawPeaks[i] / localMax, 0.0f, 1.0f);
            float shaped = 0.55f * std::pow(globalNorm, 0.42f) + 0.45f * (localNorm * std::pow(localMax / maxPeak, 0.22f));
            compressed[i] = std::clamp(shaped, 0.12f, 1.0f);
        }

        QVector<float> smoothed(n);
        for (int i = 0; i < n; ++i) {
            float prev = compressed[std::max(0, i - 1)];
            float curr = compressed[i];
            float next = compressed[std::min(n - 1, i + 1)];
            float val = 0.22f * prev + 0.56f * curr + 0.22f * next;
            smoothed[i] = std::clamp(val, 0.12f, 1.0f);
        }

        QMetaObject::invokeMethod(qApp, [guard, targetPath, smoothed]() {
            if (guard && guard->m_currentFilePath == targetPath) {
                guard->m_peaks = smoothed;
                guard->update();
            }
        });
    });
}

void SmoothWaveformScrubber::setPlaybackPosition(qint64 currentMs, qint64 totalDurationMs)
{
    if (totalDurationMs > 0) {
        m_totalDurationMs = totalDurationMs;
    }
    if (m_isDragging) return;
    m_currentMs = currentMs;
    if (m_totalDurationMs > 0) {
        m_progress = std::clamp(static_cast<double>(m_currentMs) / static_cast<double>(m_totalDurationMs), 0.0, 1.0);
    } else {
        m_progress = 0.0;
    }
    update();
}

void SmoothWaveformScrubber::reset()
{
    m_currentFilePath.clear();
    m_peaks = makeDefaultEnvelope(110);
    m_currentMs = 0;
    m_totalDurationMs = 0;
    m_progress = 0.0;
    m_isDragging = false;
    update();
}

qint64 SmoothWaveformScrubber::calculateSeekMs(int mouseX) const
{
    if (m_totalDurationMs <= 0 || width() <= 0) return -1;
    double frac = std::clamp(static_cast<double>(mouseX) / static_cast<double>(width()), 0.0, 1.0);
    return static_cast<qint64>(frac * m_totalDurationMs);
}

void SmoothWaveformScrubber::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_totalDurationMs > 0) {
        m_isDragging = true;
        qint64 targetMs = calculateSeekMs(event->pos().x());
        if (targetMs >= 0) {
            m_currentMs = targetMs;
            m_progress = std::clamp(static_cast<double>(event->pos().x()) / std::max(1, width()), 0.0, 1.0);
            update();
            emit seekRequested(targetMs, false);
        }
    }
    QWidget::mousePressEvent(event);
}

void SmoothWaveformScrubber::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && m_isDragging && m_totalDurationMs > 0) {
        qint64 targetMs = calculateSeekMs(event->pos().x());
        if (targetMs >= 0) {
            m_currentMs = targetMs;
            m_progress = std::clamp(static_cast<double>(event->pos().x()) / std::max(1, width()), 0.0, 1.0);
            update();
            emit seekRequested(targetMs, false);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void SmoothWaveformScrubber::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        qint64 targetMs = calculateSeekMs(event->pos().x());
        if (targetMs >= 0) {
            m_currentMs = targetMs;
            m_progress = std::clamp(static_cast<double>(event->pos().x()) / std::max(1, width()), 0.0, 1.0);
            update();
            emit seekRequested(targetMs, true);
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void SmoothWaveformScrubber::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF rect(2.0, 2.0, std::max(10.0, width() - 4.0), std::max(10.0, height() - 4.0));
    const int n = m_peaks.size();
    if (n < 2) return;

    const double cy = rect.center().y();
    QPainterPath fullWave;
    fullWave.moveTo(rect.left(), cy);
    for (int i = 0; i < n; ++i) {
        double x = rect.left() + (static_cast<double>(i) / (n - 1)) * rect.width();
        double h = m_peaks[i] * (rect.height() * 0.46);
        fullWave.lineTo(x, cy - h);
    }
    for (int i = n - 1; i >= 0; --i) {
        double x = rect.left() + (static_cast<double>(i) / (n - 1)) * rect.width();
        double h = m_peaks[i] * (rect.height() * 0.46);
        fullWave.lineTo(x, cy + h);
    }
    fullWave.closeSubpath();

    // 1. Unplayed waveform envelope
    p.setPen(QPen(QColor(148, 163, 184, 70), 1.2));
    p.setBrush(QColor(148, 163, 184, 38));
    p.drawPath(fullWave);

    // 2. Played waveform envelope (clipped to current progress)
    const double px = rect.left() + m_progress * rect.width();
    p.save();
    p.setClipRect(QRectF(rect.left(), rect.top(), m_progress * rect.width(), rect.height()));
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0.0, QColor("#38bdf8"));
    grad.setColorAt(0.5, QColor("#0284c7"));
    grad.setColorAt(1.0, QColor("#38bdf8"));
    p.setBrush(grad);
    p.setPen(QPen(QColor("#7dd3fc"), 1.5));
    p.drawPath(fullWave);
    p.restore();

    // 3. Glowing vertical playhead cursor
    p.setPen(QPen(QColor(56, 189, 248, 100), 6.0));
    p.drawLine(QPointF(px, rect.top()), QPointF(px, rect.bottom()));
    p.setPen(QPen(Qt::white, 2.0));
    p.drawLine(QPointF(px, rect.top()), QPointF(px, rect.bottom()));
}
