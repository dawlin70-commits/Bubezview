#pragma once

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QMouseEvent>
#include <QPair>

/**
 * @class StereoOscilloscopeWidget
 * @brief High-end dual-channel audio visualizer featuring separate Left and Right
 * continuous waveform envelopes with rich horizontal gradients, high-density
 * LED VU meters with serigraphed dB scales, and a master vertical VU meter.
 */
class StereoOscilloscopeWidget : public QWidget {
    Q_OBJECT

public:
    explicit StereoOscilloscopeWidget(QWidget *parent = nullptr);
    ~StereoOscilloscopeWidget() override;

    /**
     * @brief Computes and loads real stereo audio peak waveforms (L and R) from file.
     * @param filePath Absolute path to the media file.
     */
    void loadAudioWaveform(const QString &filePath);

    /**
     * @brief Sets playback playing status.
     * @param playing True if media is actively playing.
     */
    void setPlaying(bool playing);

    /**
     * @brief Updates playback progress and playhead position.
     * @param currentMs Current playback timestamp in milliseconds.
     * @param totalDurationMs Total file duration in milliseconds.
     */
    void setPlaybackPosition(qint64 currentMs, qint64 totalDurationMs);

    /**
     * @brief Sets playback volume factor.
     * @param volume Normalized volume value (0.0 to 1.0).
     */
    void setVolume(double volume);

    /**
     * @brief Resets all meter and waveform data.
     */
    void reset();

signals:
    /**
     * @brief Emitted when user clicks or drags on either waveform box to seek.
     * @param targetMs Target seek position in milliseconds.
     * @param exact True if this is the final seek (on release/click), false while dragging.
     */
    void seekRequested(qint64 targetMs, bool exact = false);

    /**
     * @brief Emitted when real-time peak audio levels for Left and Right channels are calculated.
     * @param left Normalized Left channel peak level (0.0 to 1.0).
     * @param right Normalized Right channel peak level (0.0 to 1.0).
     */
    void levelsUpdated(float left, float right);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return QSize(800, 160); }
    QSize minimumSizeHint() const override { return QSize(400, 130); }

private slots:
    void onAnimationTick();

private:
    qint64 calculateSeekMs(const QPoint &pos) const;

    QTimer *m_timer;
    QVector<float> m_peaksL;
    QVector<float> m_peaksR;
    bool m_isPlaying;
    double m_volume;
    qint64 m_currentMs;
    qint64 m_totalDurationMs;
    double m_progress;

    float m_levelL;
    float m_levelR;
    float m_peakL;
    float m_peakR;
    double m_phase;
    QString m_currentFilePath;
    bool m_isMouseSeeking;
};

