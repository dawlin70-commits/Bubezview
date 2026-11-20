/**
 * @file image_studio_widget.h
 * @brief Embedded Image Studio side panel implemented inside bubez_image.so.
 */

#pragma once

#include "core/interfaces/i_image_studio_plugin.h"
#include "core/services/image_processor.h"

#include <QSlider>
#include <QComboBox>
#include <QScrollArea>

/**
 * @class ImageStudioWidget
 * @brief Docked right-hand photo editing workspace hosted inside ImageViewer Edit Mode.
 */
class ImageStudioWidget : public IImageStudioWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the modular Image Studio side panel.
     * @param parent Optional parent widget.
     */
    explicit ImageStudioWidget(QWidget *parent = nullptr);
    ~ImageStudioWidget() override = default;

    /** @copydoc IImageStudioWidget::setSourceImage */
    void setSourceImage(const QImage &image) override;

    /** @copydoc IImageStudioWidget::updateTheme */
    void updateTheme() override;

    /** @copydoc IImageStudioWidget::setOnApplyImage */
    void setOnApplyImage(std::function<void(const QImage &)> cb) override;

    /** @copydoc IImageStudioWidget::setOnRequestCrop */
    void setOnRequestCrop(std::function<void()> cb) override;

    /** @copydoc IImageStudioWidget::setOnRequestRotate */
    void setOnRequestRotate(std::function<void(bool clockwise)> cb) override;

    /** @copydoc IImageStudioWidget::setOnRequestFlip */
    void setOnRequestFlip(std::function<void(bool horizontal)> cb) override;

    /** @copydoc IImageStudioWidget::setOnRequestRemoveTransparency */
    void setOnRequestRemoveTransparency(std::function<void()> cb) override;

private:
    void setupUi();
    void resetColorSliders();
    void resetEffectSliders();
    void resetAllSliders();
    void applyColorAdjustments();
    void applyAutoEnhance();
    void applySelectedPreset();
    void applyEffects();

    QImage m_sourceImage;

    std::function<void(const QImage &)> m_onApplyImage;
    std::function<void()> m_onRequestCrop;
    std::function<void(bool)> m_onRequestRotate;
    std::function<void(bool)> m_onRequestFlip;
    std::function<void()> m_onRequestRemoveTransparency;

    QScrollArea *m_scrollArea = nullptr;

    // Auto enhance
    QComboBox *m_comboEnhanceProfile = nullptr;

    // Light & color sliders
    QSlider *m_sliderBrightness = nullptr;
    QSlider *m_sliderContrast = nullptr;
    QSlider *m_sliderSaturation = nullptr;
    QSlider *m_sliderTemperature = nullptr;
    QSlider *m_sliderHue = nullptr;

    // Presets
    QComboBox *m_comboPresets = nullptr;
    QSlider *m_sliderPresetStrength = nullptr;

    // Effects
    QSlider *m_sliderSharpen = nullptr;
    QSlider *m_sliderBlur = nullptr;
    QSlider *m_sliderVignette = nullptr;
};
