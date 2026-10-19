#include "stereo_oscilloscope_widget.h"
#include "core/audio/audio_processor.h"
#include "theme_manager.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QThreadPool>
#include <QPointer>
#include <cmath>
#include <algorithm>

StereoOscilloscopeWidget::StereoOscilloscopeWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_isPlaying(false)
    , m_volume(1.0)
    , m_currentMs(0)
    , m_totalDurationMs(0)
    , m_progress(0.0)
    , m_levelL(0.0f)
    , m_levelR(0.0f)
    , m_peakL(0.0f)
    , m_peakR(0.0f)
    , m_phase(0.0)
    , m_isMouseSeeking(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(115);
    setMinimumWidth(500);
    setMouseTracking(true);

    connect(m_timer, &QTimer::timeout, this, &StereoOscilloscopeWidget::onAnimationTick);
}

StereoOscilloscopeWidget::~StereoOscilloscopeWidget() = default;

void StereoOscilloscopeWidget::loadAudioWaveform(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_peaksL.clear();
    m_peaksR.clear();
    m_progress = 0.0;
    m_currentMs = 0;
    update();

    if (filePath.isEmpty()) return;

    // Asynchronously decode and compute stereo peaks in background to prevent GUI freeze
    QPointer<StereoOscilloscopeWidget> guard(this);
    QString targetPath = filePath;
    QThreadPool::globalInstance()->start([guard, targetPath]() {
        auto pair = AudioProcessor::generateStereoWaveformPeaks(targetPath, 400);
        QMetaObject::invokeMethod(qApp, [guard, targetPath, pair]() {
            if (guard && guard->m_currentFilePath == targetPath) {
                guard->m_peaksL = pair.first;
                guard->m_peaksR = pair.second;
                guard->update();
            }
        });
    });
}

void StereoOscilloscopeWidget::setPlaying(bool playing)
{
    m_isPlaying = playing;
    if (m_isPlaying) {
        if (!m_timer->isActive()) {
            m_timer->start(30); // ~33 fps
        }
    }
}

void StereoOscilloscopeWidget::setPlaybackPosition(qint64 currentMs, qint64 totalDurationMs)
{
    m_currentMs = currentMs;
    m_totalDurationMs = totalDurationMs;
    if (m_totalDurationMs > 0) {
        m_progress = static_cast<double>(m_currentMs) / static_cast<double>(m_totalDurationMs);
        m_progress = std::clamp(m_progress, 0.0, 1.0);
    } else {
        m_progress = 0.0;
    }
    update();
}

void StereoOscilloscopeWidget::setVolume(double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
}

void StereoOscilloscopeWidget::reset()
{
    m_isPlaying = false;
    m_currentFilePath.clear();
    m_progress = 0.0;
    m_currentMs = 0;
    m_totalDurationMs = 0;
    m_levelL = 0.0f;
    m_levelR = 0.0f;
    m_peakL = 0.0f;
    m_peakR = 0.0f;
    m_peaksL.clear();
    m_peaksR.clear();
    m_timer->stop();
    emit levelsUpdated(0.0f, 0.0f);
    update();
}

void StereoOscilloscopeWidget::onAnimationTick()
{
    if (m_isPlaying) {
        // Smoothly interpolate playhead between mpv time-pos updates
        if (m_totalDurationMs > 0 && m_currentMs < m_totalDurationMs) {
            m_currentMs += 30;
            m_progress = static_cast<double>(m_currentMs) / static_cast<double>(m_totalDurationMs);
            m_progress = std::clamp(m_progress, 0.0, 1.0);
        }

        // Extract real amplitude from Left and Right waveforms at current progress
        float realAmpL = 0.0f;
        float realAmpR = 0.0f;

        if (!m_peaksL.isEmpty()) {
            int idxL = static_cast<int>(m_progress * (m_peaksL.size() - 1));
            idxL = std::clamp(idxL, 0, static_cast<int>(m_peaksL.size() - 1));
            realAmpL = m_peaksL[idxL];
        }

        if (!m_peaksR.isEmpty()) {
            int idxR = static_cast<int>(m_progress * (m_peaksR.size() - 1));
            idxR = std::clamp(idxR, 0, static_cast<int>(m_peaksR.size() - 1));
            realAmpR = m_peaksR[idxR];
        }

        float baseVol = static_cast<float>(m_volume);
        float targetL = realAmpL * baseVol;
        float targetR = realAmpR * baseVol;

        // If waveform peaks are not yet loaded (e.g. background analysis in progress), provide a lively response
        if (m_peaksL.isEmpty() && m_peaksR.isEmpty()) {
            float t = static_cast<float>(m_currentMs) * 0.005f;
            targetL = (0.35f + 0.15f * std::sin(t) + 0.10f * std::sin(t * 2.3f)) * baseVol;
            targetR = (0.35f + 0.15f * std::cos(t * 1.1f) + 0.10f * std::sin(t * 1.9f)) * baseVol;
        }

        targetL = std::clamp(targetL, 0.0f, 1.0f);
        targetR = std::clamp(targetR, 0.0f, 1.0f);

        // Studio VU ballistic response: fast attack on peaks, smooth natural decay
        if (targetL >= m_levelL) {
            m_levelL = m_levelL * 0.25f + targetL * 0.75f;
        } else {
            m_levelL = std::max(0.0f, m_levelL * 0.84f);
        }

        if (targetR >= m_levelR) {
            m_levelR = m_levelR * 0.25f + targetR * 0.75f;
        } else {
            m_levelR = std::max(0.0f, m_levelR * 0.84f);
        }

        // Peak hold segment falloff
        if (m_levelL > m_peakL) {
            m_peakL = m_levelL;
        } else {
            m_peakL = std::max(0.0f, m_peakL - 0.022f);
        }

        if (m_levelR > m_peakR) {
            m_peakR = m_levelR;
        } else {
            m_peakR = std::max(0.0f, m_peakR - 0.022f);
        }
    } else {
        // Smoothly decay to baseline when paused or stopped
        m_levelL *= 0.80f;
        m_levelR *= 0.80f;
        m_peakL *= 0.82f;
        m_peakR *= 0.82f;

        if (m_levelL < 0.008f && m_levelR < 0.008f) {
            m_levelL = 0.0f;
            m_levelR = 0.0f;
            m_peakL = 0.0f;
            m_peakR = 0.0f;
            m_timer->stop();
        }
    }

    emit levelsUpdated(m_levelL, m_levelR);
    update();
}

qint64 StereoOscilloscopeWidget::calculateSeekMs(const QPoint &pos) const
{
    if (m_totalDurationMs <= 0) return -1;

    const int masterW = 32;
    const int margin = 8;
    const int gap = 10;
    const int availW = width() - margin * 2 - masterW - 12;
    const int chW = (availW - gap) / 2;

    int leftWaveX = margin + 8;
    int leftWaveW = chW - 16;
    int rightWaveX = margin + chW + gap + 8;
    int rightWaveW = chW - 16;

    double frac = -1.0;
    if (pos.x() >= leftWaveX && pos.x() <= leftWaveX + leftWaveW) {
        frac = static_cast<double>(pos.x() - leftWaveX) / static_cast<double>(leftWaveW);
    } else if (pos.x() >= rightWaveX && pos.x() <= rightWaveX + rightWaveW) {
        frac = static_cast<double>(pos.x() - rightWaveX) / static_cast<double>(rightWaveW);
    }

    if (frac >= 0.0) {
        frac = std::clamp(frac, 0.0, 1.0);
        return static_cast<qint64>(frac * m_totalDurationMs);
    }
    return -1;
}

void StereoOscilloscopeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        qint64 targetMs = calculateSeekMs(event->pos());
        if (targetMs >= 0) {
            m_isMouseSeeking = true;
            setPlaybackPosition(targetMs, m_totalDurationMs);
            emit seekRequested(targetMs, false);
        }
    }
    QWidget::mousePressEvent(event);
}

void StereoOscilloscopeWidget::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && m_isMouseSeeking) {
        qint64 targetMs = calculateSeekMs(event->pos());
        if (targetMs >= 0) {
            setPlaybackPosition(targetMs, m_totalDurationMs);
            emit seekRequested(targetMs, false);
        }
    }

    // Update cursor icon
    const int masterW = 32;
    const int margin = 8;
    const int gap = 10;
    const int availW = width() - margin * 2 - masterW - 12;
    const int chW = (availW - gap) / 2;

    int leftWaveX = margin + 8;
    int leftWaveW = chW - 16;
    int rightWaveX = margin + chW + gap + 8;
    int rightWaveW = chW - 16;

    if (event->pos().y() <= 100 &&
        ((event->pos().x() >= leftWaveX && event->pos().x() <= leftWaveX + leftWaveW) ||
         (event->pos().x() >= rightWaveX && event->pos().x() <= rightWaveX + rightWaveW))) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void StereoOscilloscopeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isMouseSeeking) {
        m_isMouseSeeking = false;
        qint64 targetMs = calculateSeekMs(event->pos());
        if (targetMs >= 0) {
            setPlaybackPosition(targetMs, m_totalDurationMs);
            emit seekRequested(targetMs, true);
        }
    }
    QWidget::mouseReleaseEvent(event);
}


void StereoOscilloscopeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark);

    // Box container styling: dark audio visualizer styling
    QColor boxBg = isDark ? QColor(16, 20, 26, 240) : QColor(22, 27, 34, 240);
    QColor boxBorder = isDark ? QColor(38, 44, 54, 200) : QColor(48, 56, 68, 200);

    const int masterW = 32;
    const int margin = 6;
    const int gap = 8;
    const int availW = w - margin * 2 - masterW - 10;
    const int chW = (availW - gap) / 2;
    const int boxH = h - margin * 2;

    QRectF leftRect(margin, margin, chW, boxH);
    QRectF rightRect(margin + chW + gap, margin, chW, boxH);
    QRectF masterRect(w - margin - masterW, margin, masterW, boxH);

    // 1. Draw Channel Boxes Backgrounds
    p.setPen(QPen(boxBorder, 1));
    p.setBrush(boxBg);
    p.drawRoundedRect(leftRect, 8, 8);
    p.drawRoundedRect(rightRect, 8, 8);
    p.drawRoundedRect(masterRect, 8, 8);

    // Helper: Draw Channel Waveform + VU Meter
    auto drawChannel = [&](const QRectF &rect,
                           const QString &title,
                           const QColor &titleColor,
                           const QVector<float> &peaks,
                           const QLinearGradient &gradient,
                           float level,
                           float peak,
                           bool isRight)
    {
        // 1. Channel Title Header ("Left" / "Right")
        p.setFont(QFont("sans-serif", 8, QFont::Bold));
        p.setPen(titleColor);
        p.drawText(rect.x() + 8, rect.y() + 13, title);

        // 2. Waveform Canvas Area
        float waveX = rect.x() + 8;
        float waveY = rect.y() + 16;
        float waveW = rect.width() - 16;
        float waveH = rect.height() - 44;
        float midY = waveY + waveH / 2.0f;
        float maxAmpH = (waveH / 2.0f) - 2.0f;

        // Subtle zero baseline line
        p.setPen(QPen(QColor(255, 255, 255, 22), 1, Qt::DashLine));
        p.drawLine(QPointF(waveX, midY), QPointF(waveX + waveW, midY));

        // Continuous Mirrored Waveform Path
        int numSamples = peaks.isEmpty() ? 100 : peaks.size();
        QPainterPath wavePath;
        wavePath.moveTo(waveX, midY);

        // Upper envelope
        for (int i = 0; i < numSamples; ++i) {
            float frac = static_cast<float>(i) / (numSamples - 1);
            float amp = peaks.isEmpty() ? 0.20f : peaks[i];
            float px = waveX + frac * waveW;
            float py = midY - amp * maxAmpH;
            wavePath.lineTo(px, py);
        }

        wavePath.lineTo(waveX + waveW, midY);

        // Lower envelope (mirrored)
        for (int i = numSamples - 1; i >= 0; --i) {
            float frac = static_cast<float>(i) / (numSamples - 1);
            float amp = peaks.isEmpty() ? 0.20f : peaks[i];
            float px = waveX + frac * waveW;
            float py = midY + amp * maxAmpH;
            wavePath.lineTo(px, py);
        }

        wavePath.closeSubpath();

        // Fill waveform with rich gradient
        p.setPen(Qt::NoPen);
        p.setBrush(gradient);
        p.drawPath(wavePath);

        // Playhead Line across waveform
        if (m_progress > 0.0) {
            float playheadX = waveX + static_cast<float>(m_progress * waveW);
            // Subtle glow
            p.setPen(QPen(QColor(255, 255, 255, 70), 3));
            p.drawLine(QPointF(playheadX, waveY), QPointF(playheadX, waveY + waveH));
            // Crisp center line
            p.setPen(QPen(QColor(255, 255, 255, 240), 1.5));
            p.drawLine(QPointF(playheadX, waveY), QPointF(playheadX, waveY + waveH));
        }

        // 3. Horizontal High-Density Stereo VU-Meter & dB Scale
        float meterY = waveY + waveH + 4;
        float meterH = 6;

        // dB Text Readouts
        float dbVal = (level > 0.001f) ? (20.0f * std::log10(level)) : -60.0f;
        p.setFont(QFont("monospace", 7, QFont::Bold));
        p.setPen(QColor(148, 163, 184));
        p.drawText(QRectF(rect.x() + 8, meterY - 4, 48, 14), Qt::AlignLeft | Qt::AlignVCenter, isRight ? QString("R:%1dB").arg(dbVal, 2, 'f', 0) : QString("L:%1dB").arg(dbVal, 2, 'f', 0));

        // 32-Segment LED Ladder
        float barStartX = rect.x() + 56;
        float barW = rect.width() - 64;
        const int numSegments = 32;
        float segW = (barW - (numSegments - 1) * 2.0f) / static_cast<float>(numSegments);
        int activeSegs = static_cast<int>(level * numSegments);
        int peakSeg = static_cast<int>(peak * numSegments);

        for (int s = 0; s < numSegments; ++s) {
            float sx = barStartX + s * (segW + 2.0f);
            QColor onColor;
            QColor offColor = QColor(28, 34, 44);

            if (s < 20) {
                onColor = QColor(16, 185, 129); // Green
            } else if (s < 26) {
                onColor = QColor(245, 158, 11); // Yellow / Amber
            } else {
                onColor = QColor(239, 68, 68);  // Red
            }

            p.fillRect(QRectF(sx, meterY, segW, meterH), (s < activeSegs || (s == peakSeg && peak > 0.05f)) ? onColor : offColor);
        }

        // Serigraphed dB Scale Underneath
        float scaleY = meterY + meterH + 2;
        p.setFont(QFont("sans-serif", 6));
        p.setPen(QColor(100, 116, 139));
        const QStringList ticks = {"-60dB", "-50", "-40", "-30", "-20", "-10", "0", "+6dB"};
        for (int t = 0; t < ticks.size(); ++t) {
            float tx = barStartX + (static_cast<float>(t) / (ticks.size() - 1)) * barW - 10.0f;
            p.drawText(QRectF(tx, scaleY, 24, 9), Qt::AlignCenter, ticks[t]);
        }
    };

    // 2. Render Left Channel (Electric Blue ➔ Vivid Cyan ➔ Bright Green)
    QLinearGradient gradL(leftRect.x(), 0, leftRect.right(), 0);
    gradL.setColorAt(0.0, QColor(2, 132, 199, 230));   // Electric Sky Blue
    gradL.setColorAt(0.48, QColor(6, 182, 212, 240));  // Cyan
    gradL.setColorAt(1.0, QColor(16, 185, 129, 230));  // Bright Green
    drawChannel(leftRect, tr("Left"), QColor(56, 189, 248), m_peaksL, gradL, m_levelL, m_peakL, false);

    // 3. Render Right Channel (Crimson Red ➔ Vivid Orange ➔ Golden Amber)
    QLinearGradient gradR(rightRect.x(), 0, rightRect.right(), 0);
    gradR.setColorAt(0.0, QColor(239, 68, 68, 230));   // Crimson Red
    gradR.setColorAt(0.50, QColor(249, 115, 22, 240)); // Vivid Orange
    gradR.setColorAt(1.0, QColor(234, 179, 8, 230));   // Golden Amber
    drawChannel(rightRect, tr("Right"), QColor(251, 146, 60), m_peaksR, gradR, m_levelR, m_peakR, true);

    // 4. Render Master Vertical Dual Meter (Extreme Right)
    p.setFont(QFont("sans-serif", 6, QFont::Bold));
    p.setPen(QColor(148, 163, 184));
    p.drawText(QRectF(masterRect.x(), masterRect.y() + 2, masterW, 9), Qt::AlignCenter, "0dB");
    p.drawText(QRectF(masterRect.x(), masterRect.bottom() - 11, masterW, 9), Qt::AlignCenter, "-∞");

    float vMeterTop = masterRect.y() + 13;
    float vMeterBottom = masterRect.bottom() - 13;
    float vMeterH = vMeterBottom - vMeterTop;
    const int numVsegs = 16;
    float vSegH = (vMeterH - (numVsegs - 1) * 1.5f) / static_cast<float>(numVsegs);
    float colW = 8.0f;
    float col1X = masterRect.x() + 5.0f;
    float col2X = masterRect.x() + 17.0f;

    int activeL = static_cast<int>(m_levelL * numVsegs);
    int activeR = static_cast<int>(m_levelR * numVsegs);

    for (int vs = 0; vs < numVsegs; ++vs) {
        float vy = vMeterBottom - (vs + 1) * vSegH - vs * 1.5f;
        QColor onColor;
        QColor offColor = QColor(28, 34, 44);

        if (vs < 10) {
            onColor = QColor(16, 185, 129); // Green
        } else if (vs < 13) {
            onColor = QColor(245, 158, 11); // Yellow
        } else {
            onColor = QColor(239, 68, 68);  // Red (0dB)
        }

        p.fillRect(QRectF(col1X, vy, colW, vSegH), vs < activeL ? onColor : offColor);
        p.fillRect(QRectF(col2X, vy, colW, vSegH), vs < activeR ? onColor : offColor);
    }
}
