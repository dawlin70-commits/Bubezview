/**
 * @file photo_studio_panel.cpp
 * @brief Implementation of the Photo Studio adjustments panel.
 */

#include "photo_studio_panel.h"
#include <QGridLayout>
#include <QGroupBox>

PhotoStudioPanel::PhotoStudioPanel(QWidget *parent)
    : BaseStudioPanel(parent)
{
    setObjectName("PhotoStudioPanel");
    setHeaderTitle(tr("Photo Studio"));
    setHeaderIcon(VectorIcons::UiIcon::Palette, QColor("#38bdf8"));

    // Connect header reset button to resetAll
    connect(this, &BaseStudioPanel::resetRequested, this, &PhotoStudioPanel::resetAll);

    setupPhotoPages();
    setCurrentTab(0);
    updateTheme();
}

void PhotoStudioPanel::setupPhotoPages()
{
    addTabPage(tr("Auto"), createAutoPage());
    addTabPage(tr("Adjust"), createAdjustPage());
    addTabPage(tr("Filters"), createFiltersPage());
    addTabPage(tr("Effects"), createEffectsPage());
}

QWidget *PhotoStudioPanel::createAutoPage()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 8, 4);
    layout->setSpacing(12);

    // Toggle switch
    m_chkAutoEnable = new QCheckBox(tr("Enable Smart Auto-Enhance"), container);
    m_chkAutoEnable->setObjectName("chkAutoEnable");
    connect(m_chkAutoEnable, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    layout->addWidget(m_chkAutoEnable);

    // Sub-options container (disabled by default until auto-enhance is enabled)
    m_autoOptionsContainer = new QWidget(container);
    auto *optionsLayout = new QVBoxLayout(m_autoOptionsContainer);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(10);

    // Profile selector
    auto *profLayout = new QVBoxLayout();
    profLayout->setSpacing(4);
    auto *lblProf = new QLabel(tr("Enhancement Profile:"), m_autoOptionsContainer);
    lblProf->setObjectName("sliderTitle");
    profLayout->addWidget(lblProf);

    m_comboAutoProfile = new QComboBox(m_autoOptionsContainer);
    m_comboAutoProfile->addItem(tr("Natural (Balanced)"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::Natural));
    m_comboAutoProfile->addItem(tr("Vivid Landscape"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::Vivid));
    m_comboAutoProfile->addItem(tr("Backlit & Shadows"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::BacklightShadows));
    m_comboAutoProfile->addItem(tr("Dehaze & Clear"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::DehazeClear));
    m_comboAutoProfile->setFixedHeight(28);
    connect(m_comboAutoProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PhotoStudioPanel::adjustmentsChanged);
    profLayout->addWidget(m_comboAutoProfile);
    optionsLayout->addLayout(profLayout);

    // Intensity slider
    optionsLayout->addWidget(createSliderRow(tr("Enhance Strength"), m_sldAutoIntensity, m_lblAutoIntensityVal, 0, 150, 100));

    // Advanced options
    auto *grpAdv = new QGroupBox(tr("Fine Tuning Options"), m_autoOptionsContainer);
    auto *advLay = new QVBoxLayout(grpAdv);
    advLay->setSpacing(8);

    m_chkRecoverShadows = new QCheckBox(tr("Recover Deep Shadows"), grpAdv);
    m_chkRecoverShadows->setChecked(true);
    connect(m_chkRecoverShadows, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkRecoverShadows);

    m_chkProtectHighlights = new QCheckBox(tr("Protect Highlights"), grpAdv);
    m_chkProtectHighlights->setChecked(true);
    connect(m_chkProtectHighlights, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkProtectHighlights);

    m_chkNeutralizeWb = new QCheckBox(tr("Auto White Balance"), grpAdv);
    m_chkNeutralizeWb->setChecked(true);
    connect(m_chkNeutralizeWb, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkNeutralizeWb);

    m_chkProtectSkin = new QCheckBox(tr("Protect Skin Tones"), grpAdv);
    m_chkProtectSkin->setChecked(true);
    connect(m_chkProtectSkin, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkProtectSkin);

    m_chkDenoiseShadows = new QCheckBox(tr("Denoise Lifted Shadows"), grpAdv);
    m_chkDenoiseShadows->setChecked(true);
    connect(m_chkDenoiseShadows, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkDenoiseShadows);

    m_chkCenterWeighted = new QCheckBox(tr("Center-Weighted Metering"), grpAdv);
    m_chkCenterWeighted->setChecked(true);
    connect(m_chkCenterWeighted, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    advLay->addWidget(m_chkCenterWeighted);

    optionsLayout->addWidget(grpAdv);

    // Eyedropper calibration
    auto *eyeBox = new QGroupBox(tr("Neutral White Balance"), m_autoOptionsContainer);
    auto *eyeLay = new QVBoxLayout(eyeBox);
    eyeLay->setSpacing(6);

    m_btnEyedropper = new QToolButton(eyeBox);
    m_btnEyedropper->setText(tr(" Pipette Neutral Point"));
    m_btnEyedropper->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Pen, QColor("#38bdf8"), 16));
    m_btnEyedropper->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnEyedropper->setCheckable(true);
    m_btnEyedropper->setCursor(Qt::PointingHandCursor);
    m_btnEyedropper->setFixedHeight(28);
    connect(m_btnEyedropper, &QToolButton::toggled, this, [this](bool active) {
        emit eyedropperToggled(active);
        m_lblEyedropperStatus->setText(active ? tr("Click on a gray/white pixel...") : tr("Calibration inactive"));
    });
    eyeLay->addWidget(m_btnEyedropper);

    m_lblEyedropperStatus = new QLabel(tr("Click pipette, then click a neutral pixel on canvas."), eyeBox);
    m_lblEyedropperStatus->setObjectName("hintLabel");
    m_lblEyedropperStatus->setWordWrap(true);
    eyeLay->addWidget(m_lblEyedropperStatus);

    optionsLayout->addWidget(eyeBox);

    // Connect toggle to enable/disable sub-options
    connect(m_chkAutoEnable, &QCheckBox::toggled, m_autoOptionsContainer, &QWidget::setEnabled);
    m_autoOptionsContainer->setEnabled(false);

    layout->addWidget(m_autoOptionsContainer);
    layout->addStretch(1);

    return createStandardScrollArea(container, this);
}

QWidget *PhotoStudioPanel::createAdjustPage()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 8, 4);
    layout->setSpacing(12);

    // 1. Live RGB & Luminance Histogram
    m_histogram = new HistogramWidget(container);
    m_histogram->setFixedHeight(90);
    layout->addWidget(m_histogram);

    // 2. Light & Tone
    auto *lblLight = new QLabel(tr("LIGHT & TONE"), container);
    lblLight->setObjectName("sectionHeader");
    layout->addWidget(lblLight);

    layout->addWidget(createSliderRow(tr("Exposure"), m_sldExposure, m_lblExposureVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Brightness"), m_sldBrightness, m_lblBrightnessVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Contrast"), m_sldContrast, m_lblContrastVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Highlights"), m_sldHighlights, m_lblHighlightsVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Shadows"), m_sldShadows, m_lblShadowsVal, -100, 100, 0));

    // 3. Color & Balance
    auto *lblColor = new QLabel(tr("COLOR & BALANCE"), container);
    lblColor->setObjectName("sectionHeader");
    layout->addWidget(lblColor);

    layout->addWidget(createSliderRow(tr("Saturation"), m_sldSaturation, m_lblSaturationVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Temperature"), m_sldTemperature, m_lblTemperatureVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Tint"), m_sldTint, m_lblTintVal, -100, 100, 0));

    // 4. RGB Channels
    auto *lblRgb = new QLabel(tr("RGB CHANNELS"), container);
    lblRgb->setObjectName("sectionHeader");
    layout->addWidget(lblRgb);

    layout->addWidget(createSliderRow(tr("Red"), m_sldRed, m_lblRedVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Green"), m_sldGreen, m_lblGreenVal, -100, 100, 0));
    layout->addWidget(createSliderRow(tr("Blue"), m_sldBlue, m_lblBlueVal, -100, 100, 0));

    layout->addStretch(1);

    return createStandardScrollArea(container, this);
}

QWidget *PhotoStudioPanel::createFiltersPage()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 8, 4);
    layout->setSpacing(12);

    // Preset Intensity
    layout->addWidget(createSliderRow(tr("Filter Strength"), m_sldPresetIntensity, m_lblPresetIntensityVal, 0, 100, 100));

    // Grid of Preset Buttons
    auto *lblPresets = new QLabel(tr("ENHANCEMENT PRESETS"), container);
    lblPresets->setObjectName("sectionHeader");
    layout->addWidget(lblPresets);

    auto *gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);

    struct PresetInfo {
        ImageProcessor::FilterPreset preset;
        QString name;
    };

    QList<PresetInfo> presets = {
        { ImageProcessor::FilterPreset::Original, tr("Original") },
        { ImageProcessor::FilterPreset::TealAndOrange, tr("Vivid Landscape") },
        { ImageProcessor::FilterPreset::GoldenHour, tr("Golden Hour") },
        { ImageProcessor::FilterPreset::NordicCool, tr("Nordic Cool") },
        { ImageProcessor::FilterPreset::FilmNoir, tr("Fine Art B&&W") },
        { ImageProcessor::FilterPreset::DocumentEnhance, tr("Document Scan") }
    };

    int row = 0;
    int col = 0;
    for (const auto &p : presets) {
        auto *btn = new QPushButton(p.name, container);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(32);
        connect(btn, &QPushButton::clicked, this, [this, pr = p.preset]() {
            selectPreset(pr);
        });
        m_presetButtons[p.preset] = btn;
        gridLayout->addWidget(btn, row, col);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    layout->addLayout(gridLayout);
    layout->addStretch(1);

    selectPreset(ImageProcessor::FilterPreset::Original);

    return createStandardScrollArea(container, this);
}

QWidget *PhotoStudioPanel::createEffectsPage()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 8, 4);
    layout->setSpacing(12);

    // Detail & Sharpen
    auto *lblDetail = new QLabel(tr("DETAIL & CLARITY"), container);
    lblDetail->setObjectName("sectionHeader");
    layout->addWidget(lblDetail);

    layout->addWidget(createSliderRow(tr("Sharpen"), m_sldSharpness, m_lblSharpnessVal, 0, 100, 0));

    // Vignette
    auto *lblVignette = new QLabel(tr("VIGNETTE"), container);
    lblVignette->setObjectName("sectionHeader");
    layout->addWidget(lblVignette);

    layout->addWidget(createSliderRow(tr("Vignette Intensity"), m_sldVignette, m_lblVignetteVal, 0, 100, 0));
    layout->addWidget(createSliderRow(tr("Vignette Radius"), m_sldVignetteRadius, m_lblVignetteRadiusVal, 20, 100, 80));

    // Color effects
    m_chkInvert = new QCheckBox(tr("Invert Colors (Negative)"), container);
    connect(m_chkInvert, &QCheckBox::toggled, this, &PhotoStudioPanel::adjustmentsChanged);
    layout->addWidget(m_chkInvert);

    // Privacy & Redaction Tools
    auto *grpPriv = new QGroupBox(tr("Privacy Redaction Tools"), container);
    auto *privLay = new QVBoxLayout(grpPriv);
    privLay->setSpacing(8);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    m_btnPrivacyPixelate = new QToolButton(grpPriv);
    m_btnPrivacyPixelate->setText(tr("Pixelate Area"));
    m_btnPrivacyPixelate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, QColor("#38bdf8"), 16));
    m_btnPrivacyPixelate->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnPrivacyPixelate->setCursor(Qt::PointingHandCursor);
    m_btnPrivacyPixelate->setFixedHeight(30);
    connect(m_btnPrivacyPixelate, &QToolButton::clicked, this, [this]() {
        emit privacyToolRequested("pixelate");
        m_lblPrivacyStatus->setText(tr("Drag selection on image to pixelate."));
    });
    btnRow->addWidget(m_btnPrivacyPixelate, 1);

    m_btnPrivacyBlur = new QToolButton(grpPriv);
    m_btnPrivacyBlur->setText(tr("Blur Area"));
    m_btnPrivacyBlur->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Highlighter, QColor("#38bdf8"), 16));
    m_btnPrivacyBlur->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnPrivacyBlur->setCursor(Qt::PointingHandCursor);
    m_btnPrivacyBlur->setFixedHeight(30);
    connect(m_btnPrivacyBlur, &QToolButton::clicked, this, [this]() {
        emit privacyToolRequested("blur");
        m_lblPrivacyStatus->setText(tr("Drag selection on image to blur."));
    });
    btnRow->addWidget(m_btnPrivacyBlur, 1);
    privLay->addLayout(btnRow);

    m_lblPrivacyStatus = new QLabel(tr("Select an action above, then drag a rectangle on the canvas."), grpPriv);
    m_lblPrivacyStatus->setObjectName("hintLabel");
    m_lblPrivacyStatus->setWordWrap(true);
    privLay->addWidget(m_lblPrivacyStatus);

    layout->addWidget(grpPriv);
    layout->addStretch(1);

    return createStandardScrollArea(container, this);
}

QWidget *PhotoStudioPanel::createSliderRow(const QString &title, PhotoSlider *&slider, QLabel *&valLabel, int min, int max, int def)
{
    auto *row = new QWidget(this);
    auto *vbox = new QVBoxLayout(row);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(2);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 8, 0);

    auto *lbl = new QLabel(title, row);
    lbl->setObjectName("sliderTitle");
    header->addWidget(lbl);

    header->addStretch(1);

    valLabel = new QLabel(QString::number(def), row);
    valLabel->setObjectName("sliderVal");
    valLabel->setMinimumWidth(28);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(valLabel);
    vbox->addLayout(header);

    slider = new PhotoSlider(Qt::Horizontal, min, max, def, row);
    connect(slider, &QSlider::valueChanged, this, [this, valLabel](int v) {
        valLabel->setText(QString::number(v));
        emit adjustmentsChanged();
    });
    vbox->addWidget(slider);

    return row;
}

void PhotoStudioPanel::selectPreset(ImageProcessor::FilterPreset preset)
{
    m_activePreset = preset;
    for (auto it = m_presetButtons.begin(); it != m_presetButtons.end(); ++it) {
        it.value()->setChecked(it.key() == preset);
    }
    emit adjustmentsChanged();
}

void PhotoStudioPanel::updateHistogram(const QImage &image)
{
    if (m_histogram) {
        m_histogram->updateHistogram(image);
    }
}

void PhotoStudioPanel::resetAll()
{
    const bool wasBlocked = blockSignals(true);

    if (m_chkAutoEnable) m_chkAutoEnable->setChecked(false);
    if (m_autoOptionsContainer) m_autoOptionsContainer->setEnabled(false);
    if (m_comboAutoProfile) m_comboAutoProfile->setCurrentIndex(0);
    if (m_sldAutoIntensity) { m_sldAutoIntensity->setValue(100); if (m_lblAutoIntensityVal) m_lblAutoIntensityVal->setText("100"); }
    if (m_chkRecoverShadows) m_chkRecoverShadows->setChecked(true);
    if (m_chkProtectHighlights) m_chkProtectHighlights->setChecked(true);
    if (m_chkNeutralizeWb) m_chkNeutralizeWb->setChecked(true);
    if (m_chkProtectSkin) m_chkProtectSkin->setChecked(true);
    if (m_chkDenoiseShadows) m_chkDenoiseShadows->setChecked(true);
    if (m_chkCenterWeighted) m_chkCenterWeighted->setChecked(true);
    m_neutralSamplePoint = QPoint(-1, -1);

    if (m_sldExposure) { m_sldExposure->setValue(0); m_lblExposureVal->setText("0"); }
    if (m_sldBrightness) { m_sldBrightness->setValue(0); m_lblBrightnessVal->setText("0"); }
    if (m_sldContrast) { m_sldContrast->setValue(0); m_lblContrastVal->setText("0"); }
    if (m_sldHighlights) { m_sldHighlights->setValue(0); m_lblHighlightsVal->setText("0"); }
    if (m_sldShadows) { m_sldShadows->setValue(0); m_lblShadowsVal->setText("0"); }
    if (m_sldSaturation) { m_sldSaturation->setValue(0); m_lblSaturationVal->setText("0"); }
    if (m_sldTemperature) { m_sldTemperature->setValue(0); m_lblTemperatureVal->setText("0"); }
    if (m_sldTint) { m_sldTint->setValue(0); m_lblTintVal->setText("0"); }
    if (m_sldRed) { m_sldRed->setValue(0); m_lblRedVal->setText("0"); }
    if (m_sldGreen) { m_sldGreen->setValue(0); m_lblGreenVal->setText("0"); }
    if (m_sldBlue) { m_sldBlue->setValue(0); m_lblBlueVal->setText("0"); }

    if (m_sldPresetIntensity) { m_sldPresetIntensity->setValue(100); if (m_lblPresetIntensityVal) m_lblPresetIntensityVal->setText("100"); }
    m_activePreset = ImageProcessor::FilterPreset::Original;
    for (auto it = m_presetButtons.begin(); it != m_presetButtons.end(); ++it) {
        it.value()->setChecked(it.key() == ImageProcessor::FilterPreset::Original);
    }

    if (m_sldSharpness) { m_sldSharpness->setValue(0); m_lblSharpnessVal->setText("0"); }
    if (m_sldVignette) { m_sldVignette->setValue(0); m_lblVignetteVal->setText("0"); }
    if (m_sldVignetteRadius) { m_sldVignetteRadius->setValue(80); m_lblVignetteRadiusVal->setText("80"); }
    if (m_chkInvert) m_chkInvert->setChecked(false);

    blockSignals(wasBlocked);
    emit adjustmentsChanged();
}

bool PhotoStudioPanel::isAutoEnhanceEnabled() const
{
    return m_chkAutoEnable && m_chkAutoEnable->isChecked();
}

ImageProcessor::AutoEnhanceOptions PhotoStudioPanel::autoEnhanceOptions() const
{
    ImageProcessor::AutoEnhanceOptions opts;
    opts.intensity = m_sldAutoIntensity ? m_sldAutoIntensity->value() : 100;
    opts.protectHighlights = m_chkProtectHighlights ? m_chkProtectHighlights->isChecked() : true;
    opts.recoverShadows = m_chkRecoverShadows ? m_chkRecoverShadows->isChecked() : true;
    opts.correctWhiteBalance = m_chkNeutralizeWb ? m_chkNeutralizeWb->isChecked() : true;
    opts.protectSkinTones = m_chkProtectSkin ? m_chkProtectSkin->isChecked() : true;
    opts.autoDenoiseShadows = m_chkDenoiseShadows ? m_chkDenoiseShadows->isChecked() : true;
    opts.centerWeighted = m_chkCenterWeighted ? m_chkCenterWeighted->isChecked() : true;
    opts.neutralSamplePoint = m_neutralSamplePoint;

    if (m_comboAutoProfile) {
        opts.profile = static_cast<ImageProcessor::AutoEnhanceOptions::Profile>(
            m_comboAutoProfile->currentData().toInt());
    }

    return opts;
}

void PhotoStudioPanel::setNeutralSamplePoint(const QPoint &pt)
{
    m_neutralSamplePoint = pt;
    if (m_btnEyedropper) m_btnEyedropper->setChecked(false);
    if (m_lblEyedropperStatus) {
        m_lblEyedropperStatus->setText(pt.x() >= 0 ? tr("Neutral point calibrated: (%1, %2)").arg(pt.x()).arg(pt.y()) : tr("Pipette reset."));
    }
    emit adjustmentsChanged();
}

int PhotoStudioPanel::exposure() const { return m_sldExposure ? m_sldExposure->value() : 0; }
int PhotoStudioPanel::brightness() const { return m_sldBrightness ? m_sldBrightness->value() : 0; }
int PhotoStudioPanel::contrast() const { return m_sldContrast ? m_sldContrast->value() : 0; }
int PhotoStudioPanel::highlights() const { return m_sldHighlights ? m_sldHighlights->value() : 0; }
int PhotoStudioPanel::shadows() const { return m_sldShadows ? m_sldShadows->value() : 0; }
int PhotoStudioPanel::saturation() const { return m_sldSaturation ? m_sldSaturation->value() : 0; }
int PhotoStudioPanel::temperature() const { return m_sldTemperature ? m_sldTemperature->value() : 0; }
int PhotoStudioPanel::tint() const { return m_sldTint ? m_sldTint->value() : 0; }
int PhotoStudioPanel::red() const { return m_sldRed ? m_sldRed->value() : 0; }
int PhotoStudioPanel::green() const { return m_sldGreen ? m_sldGreen->value() : 0; }
int PhotoStudioPanel::blue() const { return m_sldBlue ? m_sldBlue->value() : 0; }

int PhotoStudioPanel::presetIntensity() const { return m_sldPresetIntensity ? m_sldPresetIntensity->value() : 100; }

int PhotoStudioPanel::sharpness() const { return m_sldSharpness ? m_sldSharpness->value() : 0; }
int PhotoStudioPanel::vignette() const { return m_sldVignette ? m_sldVignette->value() : 0; }
int PhotoStudioPanel::vignetteRadius() const { return m_sldVignetteRadius ? m_sldVignetteRadius->value() : 80; }
bool PhotoStudioPanel::isInvertColors() const { return m_chkInvert && m_chkInvert->isChecked(); }

void PhotoStudioPanel::customizeTheme(QString &, bool isDark)
{
    const QColor accentIconColor = isDark ? QColor("#38bdf8") : QColor("#0284c7");

    if (m_btnEyedropper) {
        m_btnEyedropper->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Pen, accentIconColor, 16));
    }
    if (m_btnPrivacyPixelate) {
        m_btnPrivacyPixelate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, accentIconColor, 16));
    }
    if (m_btnPrivacyBlur) {
        m_btnPrivacyBlur->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Highlighter, accentIconColor, 16));
    }
}
