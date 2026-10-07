#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QVector>
#include <QTimer>
#include <QPaintEvent>
#include "core/equalizer_manager.h"

/**
 * @class StereoVuMeterWidget
 * @brief Dynamic stereo LED peak meter displaying Left and Right channel levels (-48 dB to 0 dB+).
 */
class StereoVuMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit StereoVuMeterWidget(QWidget *parent = nullptr);
    ~StereoVuMeterWidget() override = default;

    void setLevels(float leftPeak, float rightPeak);
    void reset();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onDecayTick();

private:
    float m_left;
    float m_right;
    float m_leftMax;
    float m_rightMax;
    float m_targetLeft;
    float m_targetRight;
    QTimer *m_decayTimer;
};

/**
 * @class EqualizerWidget
 * @brief Full-featured 10-band graphic equalizer with pre-amp, true-peak limiter,
 * stereo widener, VU meter, and shared custom preset manager.
 */
class EqualizerWidget : public QWidget {
    Q_OBJECT

public:
    explicit EqualizerWidget(QWidget *parent = nullptr);
    ~EqualizerWidget() override = default;

    /**
     * @brief Gets the current equalizer configuration.
     * @return Current EqualizerPreset data.
     */
    EqualizerPreset currentPreset() const;

    /**
     * @brief Sets the equalizer configuration from a preset.
     * @param preset Equalizer parameters.
     */
    void applyPreset(const EqualizerPreset &preset);

    /**
     * @brief Checks if equalizer is currently bypassed.
     * @return True if bypassed.
     */
    bool isBypassed() const;

    /**
     * @brief Updates VU meter peak levels during live audio playback.
     * @param left Left channel amplitude (0.0 to 1.0+).
     * @param right Right channel amplitude (0.0 to 1.0+).
     */
    void updateVuMeter(float left, float right);

signals:
    /**
     * @brief Emitted whenever any slider, limiter, widener, or bypass state changes.
     * @param preset Current configuration.
     * @param bypassed True if EQ is currently bypassed.
     */
    void configurationChanged(const EqualizerPreset &preset, bool bypassed);

private slots:
    void onPresetSelected(int index);
    void onSavePresetClicked();
    void onDeletePresetClicked();
    void onResetFlatClicked();
    void onSliderValueChanged();
    void onBypassToggled(bool checked);
    void refreshPresetsCombo();

private:
    void setupUi();
    void applyTheme();

    QComboBox *m_comboPresets;
    QPushButton *m_btnSavePreset;
    QPushButton *m_btnDeletePreset;
    QPushButton *m_btnResetFlat;
    QCheckBox *m_chkBypass;

    // Pre-Amp
    QSlider *m_sliderPreAmp;
    QLabel *m_lblPreAmpVal;

    // 10 EQ Bands
    QVector<QSlider*> m_bandSliders;
    QVector<QLabel*> m_bandValLabels;

    // Right tools
    StereoVuMeterWidget *m_vuMeter;
    QCheckBox *m_chkLimiter;
    QSlider *m_sliderWidth;
    QLabel *m_lblWidthVal;

    bool m_updatingUi = false;
};
