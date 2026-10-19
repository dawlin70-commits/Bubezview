#pragma once

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QProgressBar>
#include <QVector>
#include <QPaintEvent>
#include <QWidget>
#include <QTimer>
#include <QTabWidget>
#include <mpv/client.h>
#include "core/audio/audio_processor.h"
#include "equalizer_widget.h"

/**
 * @class WaveformWidget
 * @brief Interactive waveform canvas displaying amplitude peaks and [A - B] cut markers.
 */
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void setPeaks(const QVector<float> &peaks);
    void setSelection(qint64 startMs, qint64 endMs, qint64 totalMs);
    void setCurrentPlayPosition(qint64 currentMs, qint64 totalMs);

signals:
    void rangeSelected(qint64 startMs, qint64 endMs);
    void seekRequested(qint64 posMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 pixelToMs(int x) const;
    int msToPixel(qint64 ms) const;

    QVector<float> m_peaks;
    qint64 m_startMs;
    qint64 m_endMs;
    qint64 m_totalMs;
    qint64 m_currentMs;
    bool m_draggingStart;
    bool m_draggingEnd;
    bool m_draggingRange;
    int m_dragOriginX;
};

/**
 * @class SpectrumWidget
 * @brief Interactive frequency response curve and audio spectrum visualizer (20Hz - 20kHz).
 */
class SpectrumWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    /**
     * @brief Updates filter parameters to dynamically draw the frequency response curve.
     * @param lowCutHz Cutoff frequency for highpass filter in Hz.
     * @param denoiseDb Spectral noise reduction attenuation in dB.
     * @param dehumHz Mains hum notch filter frequency (0, 50, 60).
     * @param voiceEnhance True if 3kHz voice presence boost is active.
     * @param deesser True if high-frequency sibilance dip (5-8kHz) is active.
     */
    void setFilterParameters(int lowCutHz, int denoiseDb, int dehumHz,
                             bool voiceEnhance, bool deesser = false,
                             int voiceFreqHz = 3000, double voiceGainDb = 3.5,
                             int deessFreqHz = 6500, double deessDepthDb = -6.0,
                             int highCutHz = 0);

    /**
     * @brief Sets live FFT spectrum band magnitudes computed from actual audio playback.
     * @param bands Normalized energy magnitudes (0.0 to 1.0) for each frequency band.
     */
    void setSpectrumFftData(const QVector<float> &bands);

    /**
     * @brief Toggles live animated audio activity bars.
     * @param playing True if audio preview is playing.
     */
    void setPlaying(bool playing);

signals:
    void lowCutChanged(int hz);
    void voiceParamChanged(int freqHz, double gainDb);
    void deessParamChanged(int freqHz, double depthDb);
    void highCutChanged(int hz);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    double freqToX(double freq) const;
    double xToFreq(double x) const;

    enum class DragNode { None, LowCut, Voice, DeEsser, HighCut };

    int m_lowCutHz;
    int m_denoiseDb;
    int m_dehumHz;
    bool m_voiceEnhance;
    bool m_deesser;
    int m_voiceFreqHz;
    double m_voiceGainDb;
    int m_deessFreqHz;
    double m_deessDepthDb;
    int m_highCutHz;
    bool m_isPlaying;
    DragNode m_activeDragNode;
    QVector<float> m_realBands;
};

/**
 * @class AudioTrimDialog
 * @brief Unified audio trimming, sound filtering, noise reduction, and restoration suite.
 */
class AudioTrimDialog : public QDialog {
    Q_OBJECT

public:
    explicit AudioTrimDialog(const QString &audioFilePath, QWidget *parent = nullptr);
    ~AudioTrimDialog() override;

    QString savedFilePath() const { return m_savedPath; }

private slots:
    void onPlayPreviewClicked();
    void onSetStartCurrent();
    void onSetEndCurrent();
    void onRangeChangedFromWaveform(qint64 startMs, qint64 endMs);
    void onWaveformSeek(qint64 posMs);
    void onSaveClicked();
    void onFormatChanged(int index);
    void updatePreviewTimer();

    /**
     * @brief Triggered whenever any audio filter parameter changes.
     * Updates lossless state and dynamically refreshes real-time preview playback.
     */
    void onFilterChanged();

    /**
     * @brief Triggered when user selects a preset from the presets combo.
     * @param index Preset index.
     */
    void onPresetSelected(int index);

    /**
     * @brief Triggered when manual sliders or controls are adjusted by user.
     */
    void onManualControlChanged();

    /**
     * @brief Play/pause toggle specifically inside Tab 2 (Filters & Noise Reduction).
     */
    void onPlayFiltersClicked();

    /**
     * @brief Seek handler for Tab 2 mini-player slider.
     */
    void onFiltersSeek(int val);

    /**
     * @brief Instant A/B bypass toggle to compare raw vs filtered audio.
     * @param checked True if bypass is active (filters temporarily disabled).
     */
    void onBypassToggled(bool checked);

    void onAutoDetectSilenceClicked();
    void onSplitCueClicked();
    void onOpenTagEditorClicked();
    void onResetPitchClicked();
    void onResetSpeedClicked();

private:
    void setupUi();
    void applyTheme();
    void updateTimeLabels();
    QString formatTime(qint64 ms) const;
    void startPreview();
    void stopPreview();

    QWidget *createTrimTab();
    QWidget *createFiltersTab();
    QWidget *createDspTab();
    QWidget *createEqualizerTab();

    /**
     * @brief Applies active audio filters (fades, normalization, noise filters, DSP, channels) to the live preview instance.
     */
    void applyPreviewFilters();

    QString m_filePath;
    QString m_savedPath;
    AudioMetadata m_meta;

    // Header Info
    QLabel *m_lblInfo;

    // Tab Widget (Placed at the top)
    QTabWidget *m_tabWidget;

    // Tab 1: Trimming & Fades
    WaveformWidget *m_waveform;
    QLabel *m_lblStart;
    QLabel *m_lblEnd;
    QLabel *m_lblDuration;
    QPushButton *m_btnPlayPreview;
    QPushButton *m_btnSetStart;
    QPushButton *m_btnSetEnd;
    QPushButton *m_btnAutoDetectSilence;
    QPushButton *m_btnSplitCue;
    QDoubleSpinBox *m_spinFadeIn;
    QDoubleSpinBox *m_spinFadeOut;
    QCheckBox *m_chkLossless;

    // Tab 2: Filters & Enhancement
    QComboBox *m_comboPresets;
    QPushButton *m_btnPlayFilters;
    QPushButton *m_btnBypass;
    QSlider *m_sliderSeekFilters;
    QLabel *m_lblTimeFilters;
    SpectrumWidget *m_spectrumWidget;
    QSlider *m_sliderLowCut;
    QLabel *m_lblLowCutVal;
    QSlider *m_sliderHighCut;
    QLabel *m_lblHighCutVal;
    QSlider *m_sliderDenoise;
    QLabel *m_lblDenoiseVal;
    QComboBox *m_comboDehum;
    QCheckBox *m_chkDeclick;
    QCheckBox *m_chkDeesser;
    QSlider *m_sliderDeessFreq;
    QSlider *m_sliderDeessDepth;
    QLabel *m_lblDeessVal;
    QCheckBox *m_chkVoiceEnhance;
    QSlider *m_sliderVoiceFreq;
    QSlider *m_sliderVoiceGain;
    QLabel *m_lblVoiceVal;
    QCheckBox *m_chkNormalize;
    QCheckBox *m_chkNoiseGate;

    // Tab 3: Canali & Tonalità (Spatial, Pitch, Speed, Reverse)
    QComboBox *m_comboChannels;
    QSlider *m_sliderPitch;
    QLabel *m_lblPitchVal;
    QPushButton *m_btnResetPitch;
    QSlider *m_sliderSpeed;
    QLabel *m_lblSpeedVal;
    QPushButton *m_btnResetSpeed;
    QCheckBox *m_chkReverse;
    QCheckBox *m_chkRemoveSilence;

    // Tab 4: Equalizzatore Traccia
    EqualizerWidget *m_equalizerWidget;

    // Footer (Shared)
    QComboBox *m_comboFormat;
    QComboBox *m_comboBitrate;
    QComboBox *m_comboSampleRate;
    QPushButton *m_btnOpenTagEditor;
    QLabel *m_lblStatusBadge;
    QProgressBar *m_progressBar;
    QPushButton *m_btnSave;
    QPushButton *m_btnCancel;

    // libmpv preview instance
    mpv_handle *m_previewMpv;
    QTimer *m_playbackTimer;
    qint64 m_startMs;
    qint64 m_endMs;
    bool m_isPreviewPlaying;
    bool m_updatingPresetUI;
    bool m_isFiltersPlaying;
    bool m_isBypassActive;
    QVector<float> m_pcmData;
    int m_pcmSampleRate;
};
