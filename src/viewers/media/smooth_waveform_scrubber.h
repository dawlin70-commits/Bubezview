#pragma once

#include <QWidget>
#include <QVector>
#include <QString>
#include <QMouseEvent>
#include <QPaintEvent>

/**
 * @class SmoothWaveformScrubber
 * @brief Interactive smooth continuous spline waveform timeline scrubber for audio preview.
 */
class SmoothWaveformScrubber : public QWidget
{
    Q_OBJECT

public:
    explicit SmoothWaveformScrubber(QWidget *parent = nullptr);

    /**
     * @brief Asynchronously computes and loads smooth waveform amplitude peaks for the audio file.
     * @param filePath Absolute path to the audio file.
     */
    void loadAudioWaveform(const QString &filePath);

    /**
     * @brief Updates the current playback position and playhead progress.
     * @param currentMs Current playback position in milliseconds.
     * @param totalDurationMs Total track duration in milliseconds.
     */
    void setPlaybackPosition(qint64 currentMs, qint64 totalDurationMs);

    /**
     * @brief Resets the waveform and playhead to initial state.
     */
    void reset();

signals:
    /**
     * @brief Emitted when the user clicks or drags along the waveform to seek.
     * @param targetMs Target playback position in milliseconds.
     * @param exact True on mouse release for exact frame seek, false during drag.
     */
    void seekRequested(qint64 targetMs, bool exact);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 calculateSeekMs(int mouseX) const;
    static QVector<float> makeDefaultEnvelope(int count);

    QVector<float> m_peaks;
    QString m_currentFilePath;
    qint64 m_currentMs = 0;
    qint64 m_totalDurationMs = 0;
    double m_progress = 0.0;
    bool m_isDragging = false;
};
