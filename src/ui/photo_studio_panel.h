#pragma once

#include "base_studio_panel.h"
#include <QComboBox>
#include <QCheckBox>
#include "dialogs/photo_studio_dialog.h"
#include "core/services/image_processor.h"

/**
 * @brief Professional, collapsible, resizable right-hand side panel providing live photo studio adjustments.
 *
 * Inherits the standardized BaseStudioPanel shell and organizes photo adjustments into four tabs:
 * (Auto-Enhance, Adjustments, Filters, Effects & Privacy) with real-time histogram and live preview proxy.
 */
class PhotoStudioPanel : public BaseStudioPanel {
    Q_OBJECT

public:
    explicit PhotoStudioPanel(QWidget *parent = nullptr);
    ~PhotoStudioPanel() override = default;

    /**
     * @brief Resets all adjustment sliders and active preset back to default untouched values.
     */
    void resetAll();

    /**
     * @brief Updates the live RGB & Luminance histogram with the processed image.
     * @param image Currently displayed image.
     */
    void updateHistogram(const QImage &image);

    // --- Auto-Enhance Options ---
    bool isAutoEnhanceEnabled() const;
    ImageProcessor::AutoEnhanceOptions autoEnhanceOptions() const;
    void setNeutralSamplePoint(const QPoint &pt);

    // --- Light & Color Adjustments ---
    int exposure() const;
    int brightness() const;
    int contrast() const;
    int highlights() const;
    int shadows() const;
    int saturation() const;
    int temperature() const;
    int tint() const;
    int red() const;
    int green() const;
    int blue() const;

    // --- Filter Presets ---
    ImageProcessor::FilterPreset activePreset() const { return m_activePreset; }
    int presetIntensity() const;

    // --- Detail & Effects ---
    int sharpness() const;
    int blur() const { return 0; }
    int vignette() const;
    int vignetteRadius() const;
    bool isInvertColors() const;

signals:
    /**
     * @brief Emitted in real-time as user adjusts any parameter.
     */
    void adjustmentsChanged();

    /**
     * @brief Emitted when eyedropper neutral point sampling is activated or deactivated.
     * @param active True if eyedropper mode is active.
     */
    void eyedropperToggled(bool active);

    /**
     * @brief Emitted when user requests privacy region selection on canvas.
     * @param mode "pixelate" or "blur".
     */
    void privacyToolRequested(const QString &mode);

protected:
    /**
     * @brief Customizes theme icons and controls specific to Photo Studio.
     * @param qss Stylesheet string to append rules to.
     * @param isDark True if the active theme preset is dark.
     */
    void customizeTheme(QString &qss, bool isDark) override;

private:
    void setupPhotoPages();
    QWidget *createAutoPage();
    QWidget *createAdjustPage();
    QWidget *createFiltersPage();
    QWidget *createEffectsPage();

    QWidget *createSliderRow(const QString &title, PhotoSlider *&slider, QLabel *&valLabel, int min, int max, int def);
    void selectPreset(ImageProcessor::FilterPreset preset);

    // --- Page 0: Auto-Enhance ---
    QCheckBox *m_chkAutoEnable = nullptr;
    QWidget *m_autoOptionsContainer = nullptr;
    QComboBox *m_comboAutoProfile = nullptr;
    PhotoSlider *m_sldAutoIntensity = nullptr;
    QLabel *m_lblAutoIntensityVal = nullptr;
    QCheckBox *m_chkProtectHighlights = nullptr;
    QCheckBox *m_chkRecoverShadows = nullptr;
    QCheckBox *m_chkNeutralizeWb = nullptr;
    QCheckBox *m_chkProtectSkin = nullptr;
    QCheckBox *m_chkDenoiseShadows = nullptr;
    QCheckBox *m_chkCenterWeighted = nullptr;
    QToolButton *m_btnEyedropper = nullptr;
    QLabel *m_lblEyedropperStatus = nullptr;
    QPoint m_neutralSamplePoint = QPoint(-1, -1);

    // --- Page 1: Adjust (Light, Color, Channels) ---
    HistogramWidget *m_histogram = nullptr;
    PhotoSlider *m_sldExposure = nullptr;
    QLabel *m_lblExposureVal = nullptr;
    PhotoSlider *m_sldBrightness = nullptr;
    QLabel *m_lblBrightnessVal = nullptr;
    PhotoSlider *m_sldContrast = nullptr;
    QLabel *m_lblContrastVal = nullptr;
    PhotoSlider *m_sldHighlights = nullptr;
    QLabel *m_lblHighlightsVal = nullptr;
    PhotoSlider *m_sldShadows = nullptr;
    QLabel *m_lblShadowsVal = nullptr;
    PhotoSlider *m_sldSaturation = nullptr;
    QLabel *m_lblSaturationVal = nullptr;
    PhotoSlider *m_sldTemperature = nullptr;
    QLabel *m_lblTemperatureVal = nullptr;
    PhotoSlider *m_sldTint = nullptr;
    QLabel *m_lblTintVal = nullptr;
    PhotoSlider *m_sldRed = nullptr;
    QLabel *m_lblRedVal = nullptr;
    PhotoSlider *m_sldGreen = nullptr;
    QLabel *m_lblGreenVal = nullptr;
    PhotoSlider *m_sldBlue = nullptr;
    QLabel *m_lblBlueVal = nullptr;

    // --- Page 2: Filters ---
    PhotoSlider *m_sldPresetIntensity = nullptr;
    QLabel *m_lblPresetIntensityVal = nullptr;
    ImageProcessor::FilterPreset m_activePreset = ImageProcessor::FilterPreset::Original;
    QMap<ImageProcessor::FilterPreset, QPushButton*> m_presetButtons;

    // --- Page 3: Effects & Privacy ---
    PhotoSlider *m_sldSharpness = nullptr;
    QLabel *m_lblSharpnessVal = nullptr;
    PhotoSlider *m_sldVignette = nullptr;
    QLabel *m_lblVignetteVal = nullptr;
    PhotoSlider *m_sldVignetteRadius = nullptr;
    QLabel *m_lblVignetteRadiusVal = nullptr;
    QCheckBox *m_chkInvert = nullptr;
    QToolButton *m_btnPrivacyPixelate = nullptr;
    QToolButton *m_btnPrivacyBlur = nullptr;
    QLabel *m_lblPrivacyStatus = nullptr;
};
