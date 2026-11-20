/**
 * @file image_studio_widget.cpp
 * @brief Implementation of the modular Image Studio side panel inside bubez_image.so.
 */

#include "image_studio_widget.h"
#include "core/services/theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QGuiApplication>

namespace {

/**
 * @brief Scoped wait-cursor guard for synchronous CPU-bound image processing steps.
 */
struct ScopedWaitCursor {
    ScopedWaitCursor() { QGuiApplication::setOverrideCursor(Qt::WaitCursor); }
    ~ScopedWaitCursor() { QGuiApplication::restoreOverrideCursor(); }
};

} // namespace

ImageStudioWidget::ImageStudioWidget(QWidget *parent)
    : IImageStudioWidget(parent)
{
    setObjectName(QStringLiteral("ImageStudioWidget"));
    // Slightly wider sidebar to avoid clipping localized 2-column button labels
    setFixedWidth(292);
    setupUi();
    updateTheme();
}

void ImageStudioWidget::setSourceImage(const QImage &image)
{
    m_sourceImage = image;
    resetAllSliders();
}

void ImageStudioWidget::setOnApplyImage(std::function<void(const QImage &)> cb)
{
    m_onApplyImage = std::move(cb);
}

void ImageStudioWidget::setOnRequestCrop(std::function<void()> cb)
{
    m_onRequestCrop = std::move(cb);
}

void ImageStudioWidget::setOnRequestRotate(std::function<void(bool clockwise)> cb)
{
    m_onRequestRotate = std::move(cb);
}

void ImageStudioWidget::setOnRequestFlip(std::function<void(bool horizontal)> cb)
{
    m_onRequestFlip = std::move(cb);
}

void ImageStudioWidget::setOnRequestRemoveTransparency(std::function<void()> cb)
{
    m_onRequestRemoveTransparency = std::move(cb);
}

void ImageStudioWidget::setupUi()
{
    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("ImageStudioScroll"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(m_scrollArea);
    content->setObjectName(QStringLiteral("ImageStudioCanvas"));
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(12, 12, 12, 16);
    lay->setSpacing(12);

    auto makeSectionCard = [&](const QString &title) -> std::pair<QFrame *, QVBoxLayout *> {
        auto *card = new QFrame(content);
        card->setObjectName(QStringLiteral("ImageStudioCard"));
        auto *cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 12);
        cardLay->setSpacing(8);
        auto *hdr = new QLabel(title, card);
        hdr->setObjectName(QStringLiteral("ImageStudioSectionTitle"));
        cardLay->addWidget(hdr);
        lay->addWidget(card);
        return {card, cardLay};
    };

    auto makeSliderRow = [](QWidget *parent, const QString &label, int minV, int maxV, int defV,
                            QSlider **outSlider, const QString &suffix = QString()) -> QWidget * {
        auto *rowW = new QWidget(parent);
        auto *vLay = new QVBoxLayout(rowW);
        vLay->setContentsMargins(0, 0, 0, 0);
        vLay->setSpacing(2);

        auto *top = new QHBoxLayout();
        top->setContentsMargins(0, 0, 0, 0);
        auto *lbl = new QLabel(label, rowW);
        auto *val = new QLabel(QString::number(defV) + suffix, rowW);
        val->setObjectName(QStringLiteral("ImageStudioValLabel"));
        top->addWidget(lbl);
        top->addStretch(1);
        top->addWidget(val);
        vLay->addLayout(top);

        auto *slider = new QSlider(Qt::Horizontal, rowW);
        slider->setRange(minV, maxV);
        slider->setValue(defV);
        QObject::connect(slider, &QSlider::valueChanged, val, [val, suffix](int v) {
            val->setText(QString::number(v) + suffix);
        });
        vLay->addWidget(slider);

        *outSlider = slider;
        return rowW;
    };

    // 1. Quick Transform & Geometry
    {
        auto [card, cardLay] = makeSectionCard(tr("Transform"));
        auto *grid = new QGridLayout();
        grid->setSpacing(6);

        auto makeBtn = [&](const QString &text, std::function<void()> cb) {
            auto *b = new QPushButton(text, card);
            b->setObjectName(QStringLiteral("ImageStudioSecBtn"));
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumHeight(28);
            connect(b, &QPushButton::clicked, this, [cb = std::move(cb)]() {
                if (cb) cb();
            });
            return b;
        };

        grid->addWidget(makeBtn(tr("Rotate Left"), [this]() { if (m_onRequestRotate) m_onRequestRotate(false); }), 0, 0);
        grid->addWidget(makeBtn(tr("Rotate Right"), [this]() { if (m_onRequestRotate) m_onRequestRotate(true); }), 0, 1);
        grid->addWidget(makeBtn(tr("Flip H"), [this]() { if (m_onRequestFlip) m_onRequestFlip(true); }), 1, 0);
        grid->addWidget(makeBtn(tr("Flip V"), [this]() { if (m_onRequestFlip) m_onRequestFlip(false); }), 1, 1);
        grid->addWidget(makeBtn(tr("Crop"), [this]() { if (m_onRequestCrop) m_onRequestCrop(); }), 2, 0);
        grid->addWidget(makeBtn(tr("Flatten Alpha"), [this]() { if (m_onRequestRemoveTransparency) m_onRequestRemoveTransparency(); }), 2, 1);

        cardLay->addLayout(grid);
    }

    // 2. Auto Enhance
    {
        auto [card, cardLay] = makeSectionCard(tr("Auto Enhance"));
        m_comboEnhanceProfile = new QComboBox(card);
        m_comboEnhanceProfile->setView(new QListView(m_comboEnhanceProfile));
        m_comboEnhanceProfile->addItem(tr("Natural"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::Natural));
        m_comboEnhanceProfile->addItem(tr("Vivid"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::Vivid));
        m_comboEnhanceProfile->addItem(tr("Backlight Shadows"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::BacklightShadows));
        m_comboEnhanceProfile->addItem(tr("Dehaze & Clear"), static_cast<int>(ImageProcessor::AutoEnhanceOptions::Profile::DehazeClear));
        m_comboEnhanceProfile->setMinimumHeight(30);
        cardLay->addWidget(m_comboEnhanceProfile);

        auto *btnEnhance = new QPushButton(tr("Apply Auto Enhance"), card);
        btnEnhance->setObjectName(QStringLiteral("ImageStudioPrimaryBtn"));
        btnEnhance->setCursor(Qt::PointingHandCursor);
        btnEnhance->setMinimumHeight(30);
        connect(btnEnhance, &QPushButton::clicked, this, &ImageStudioWidget::applyAutoEnhance);
        cardLay->addWidget(btnEnhance);
    }

    // 3. Light & Color Adjustments
    {
        auto [card, cardLay] = makeSectionCard(tr("Light & Color"));
        cardLay->addWidget(makeSliderRow(card, tr("Brightness"), -100, 100, 0, &m_sliderBrightness));
        cardLay->addWidget(makeSliderRow(card, tr("Contrast"), -100, 100, 0, &m_sliderContrast));
        cardLay->addWidget(makeSliderRow(card, tr("Saturation"), -100, 100, 0, &m_sliderSaturation));
        cardLay->addWidget(makeSliderRow(card, tr("Temperature"), -100, 100, 0, &m_sliderTemperature));
        cardLay->addWidget(makeSliderRow(card, tr("Hue"), -180, 180, 0, &m_sliderHue, QStringLiteral("°")));

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(6);
        auto *btnReset = new QPushButton(tr("Reset"), card);
        btnReset->setObjectName(QStringLiteral("ImageStudioSecBtn"));
        btnReset->setCursor(Qt::PointingHandCursor);
        btnReset->setMinimumHeight(28);
        connect(btnReset, &QPushButton::clicked, this, &ImageStudioWidget::resetColorSliders);

        auto *btnApply = new QPushButton(tr("Apply"), card);
        btnApply->setObjectName(QStringLiteral("ImageStudioPrimaryBtn"));
        btnApply->setCursor(Qt::PointingHandCursor);
        btnApply->setMinimumHeight(28);
        connect(btnApply, &QPushButton::clicked, this, &ImageStudioWidget::applyColorAdjustments);

        btnRow->addWidget(btnReset);
        btnRow->addWidget(btnApply, 1);
        cardLay->addLayout(btnRow);
    }

    // 4. Cinematic Presets
    {
        auto [card, cardLay] = makeSectionCard(tr("Color Presets"));
        m_comboPresets = new QComboBox(card);
        m_comboPresets->setView(new QListView(m_comboPresets));
        const QList<ImageProcessor::FilterPreset> presets = {
            ImageProcessor::FilterPreset::TealAndOrange,
            ImageProcessor::FilterPreset::VintageSepia,
            ImageProcessor::FilterPreset::RetroFilm,
            ImageProcessor::FilterPreset::FilmNoir,
            ImageProcessor::FilterPreset::RedFilterBW,
            ImageProcessor::FilterPreset::OrangeFilterBW,
            ImageProcessor::FilterPreset::GoldenHour,
            ImageProcessor::FilterPreset::NordicCool,
            ImageProcessor::FilterPreset::CrossProcess,
            ImageProcessor::FilterPreset::Cyberpunk,
            ImageProcessor::FilterPreset::DocumentEnhance
        };
        for (auto p : presets) {
            m_comboPresets->addItem(ImageProcessor::presetDisplayName(p), static_cast<int>(p));
        }
        m_comboPresets->setMinimumHeight(30);
        cardLay->addWidget(m_comboPresets);

        cardLay->addWidget(makeSliderRow(card, tr("Intensity"), 10, 100, 100,
                                         &m_sliderPresetStrength, QStringLiteral("%")));

        auto *btnPreset = new QPushButton(tr("Apply Preset"), card);
        btnPreset->setObjectName(QStringLiteral("ImageStudioPrimaryBtn"));
        btnPreset->setCursor(Qt::PointingHandCursor);
        btnPreset->setMinimumHeight(30);
        connect(btnPreset, &QPushButton::clicked, this, &ImageStudioWidget::applySelectedPreset);
        cardLay->addWidget(btnPreset);
    }

    // 5. Details & Effects
    {
        auto [card, cardLay] = makeSectionCard(tr("Details & Effects"));
        cardLay->addWidget(makeSliderRow(card, tr("Sharpen"), 0, 100, 0, &m_sliderSharpen));
        cardLay->addWidget(makeSliderRow(card, tr("Blur"), 0, 20, 0, &m_sliderBlur));
        cardLay->addWidget(makeSliderRow(card, tr("Vignette"), 0, 100, 0, &m_sliderVignette));

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(6);
        auto *btnInvert = new QPushButton(tr("Invert"), card);
        btnInvert->setObjectName(QStringLiteral("ImageStudioSecBtn"));
        btnInvert->setCursor(Qt::PointingHandCursor);
        btnInvert->setMinimumHeight(28);
        connect(btnInvert, &QPushButton::clicked, this, [this]() {
            if (m_sourceImage.isNull() || !m_onApplyImage) return;
            ScopedWaitCursor waitCursor;
            QImage out = ImageProcessor::applyInvert(m_sourceImage);
            if (!out.isNull()) {
                m_sourceImage = out;
                m_onApplyImage(out);
            }
        });

        auto *btnFx = new QPushButton(tr("Apply Effects"), card);
        btnFx->setObjectName(QStringLiteral("ImageStudioPrimaryBtn"));
        btnFx->setCursor(Qt::PointingHandCursor);
        btnFx->setMinimumHeight(28);
        connect(btnFx, &QPushButton::clicked, this, &ImageStudioWidget::applyEffects);

        btnRow->addWidget(btnInvert);
        btnRow->addWidget(btnFx, 1);
        cardLay->addLayout(btnRow);
    }

    lay->addStretch(1);
    m_scrollArea->setWidget(content);
    outerLay->addWidget(m_scrollArea);
}

void ImageStudioWidget::resetColorSliders()
{
    // Reset only the Light & Color card sliders without disturbing pending effects
    if (m_sliderBrightness) m_sliderBrightness->setValue(0);
    if (m_sliderContrast) m_sliderContrast->setValue(0);
    if (m_sliderSaturation) m_sliderSaturation->setValue(0);
    if (m_sliderTemperature) m_sliderTemperature->setValue(0);
    if (m_sliderHue) m_sliderHue->setValue(0);
}

void ImageStudioWidget::resetEffectSliders()
{
    // Reset only the Details & Effects card sliders without disturbing pending color tweaks
    if (m_sliderSharpen) m_sliderSharpen->setValue(0);
    if (m_sliderBlur) m_sliderBlur->setValue(0);
    if (m_sliderVignette) m_sliderVignette->setValue(0);
}

void ImageStudioWidget::resetAllSliders()
{
    resetColorSliders();
    resetEffectSliders();
    if (m_sliderPresetStrength) m_sliderPresetStrength->setValue(100);
}

void ImageStudioWidget::applyColorAdjustments()
{
    if (m_sourceImage.isNull() || !m_onApplyImage) return;
    const int br = m_sliderBrightness ? m_sliderBrightness->value() : 0;
    const int ct = m_sliderContrast ? m_sliderContrast->value() : 0;
    const int sat = m_sliderSaturation ? m_sliderSaturation->value() : 0;
    const int temp = m_sliderTemperature ? m_sliderTemperature->value() : 0;
    const int hue = m_sliderHue ? m_sliderHue->value() : 0;
    if (br == 0 && ct == 0 && sat == 0 && temp == 0 && hue == 0) return;

    ScopedWaitCursor waitCursor;
    QImage out = ImageProcessor::adjust(m_sourceImage, br, ct, sat, hue, temp);
    if (!out.isNull()) {
        m_sourceImage = out;
        resetColorSliders();
        m_onApplyImage(out);
    }
}

void ImageStudioWidget::applyAutoEnhance()
{
    if (m_sourceImage.isNull() || !m_onApplyImage) return;
    ImageProcessor::AutoEnhanceOptions opts;
    if (m_comboEnhanceProfile) {
        opts.profile = static_cast<ImageProcessor::AutoEnhanceOptions::Profile>(
            m_comboEnhanceProfile->currentData().toInt());
    }
    ScopedWaitCursor waitCursor;
    QImage out = ImageProcessor::autoEnhance(m_sourceImage, opts);
    if (!out.isNull()) {
        m_sourceImage = out;
        m_onApplyImage(out);
    }
}

void ImageStudioWidget::applySelectedPreset()
{
    if (m_sourceImage.isNull() || !m_onApplyImage || !m_comboPresets) return;
    const auto preset = static_cast<ImageProcessor::FilterPreset>(m_comboPresets->currentData().toInt());
    ScopedWaitCursor waitCursor;
    QImage styled = ImageProcessor::applyPreset(m_sourceImage, preset);
    if (styled.isNull()) return;

    // Blend with the base image only when the user requested partial strength (< 100%)
    const int strengthPct = m_sliderPresetStrength ? m_sliderPresetStrength->value() : 100;
    QImage out = (strengthPct < 100)
                     ? ImageProcessor::blend(m_sourceImage, styled, strengthPct / 100.0)
                     : styled;
    if (!out.isNull()) {
        m_sourceImage = out;
        m_onApplyImage(out);
    }
}

void ImageStudioWidget::applyEffects()
{
    if (m_sourceImage.isNull() || !m_onApplyImage) return;
    const int sharp = m_sliderSharpen ? m_sliderSharpen->value() : 0;
    const int blur = m_sliderBlur ? m_sliderBlur->value() : 0;
    const int vig = m_sliderVignette ? m_sliderVignette->value() : 0;
    if (sharp == 0 && blur == 0 && vig == 0) return;

    // Apply effects sequentially (sharpen -> blur -> vignette) on a single working copy
    ScopedWaitCursor waitCursor;
    QImage out = m_sourceImage;
    if (sharp > 0) out = ImageProcessor::applySharpen(out, sharp);
    if (blur > 0) out = ImageProcessor::applyBlur(out, blur);
    if (vig > 0) out = ImageProcessor::applyVignette(out, vig, 45);
    if (!out.isNull()) {
        m_sourceImage = out;
        resetEffectSliders();
        m_onApplyImage(out);
    }
}

void ImageStudioWidget::updateTheme()
{
    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = !ThemeManager::isDark(preset);
    const bool isZinc = (preset == ThemePreset::ZincGray);

    const QString bg = isLight ? QStringLiteral("#eef1f5")
                               : (isZinc ? QStringLiteral("#494a4f") : QStringLiteral("#181a1f"));
    const QString cardBg = isLight ? QStringLiteral("#ffffff")
                                   : (isZinc ? QStringLiteral("#3c3d42") : QStringLiteral("#1e232e"));
    const QString border = isLight ? QStringLiteral("rgba(15, 23, 42, 0.12)")
                                   : (isZinc ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(255, 255, 255, 0.09)"));
    const QString text = isLight ? QStringLiteral("#0f172a") : QStringLiteral("#f1f5f9");
    const QString subText = isLight ? QStringLiteral("#64748b") : QStringLiteral("#94a3b8");
    const QString inputBg = isLight ? QStringLiteral("#f1f5f9")
                                    : (isZinc ? QStringLiteral("#333438") : QStringLiteral("#141820"));
    const QString secBtnBg = isLight ? QStringLiteral("#e2e8f0")
                                     : (isZinc ? QStringLiteral("#56575e") : QStringLiteral("#283040"));
    const QString secBtnHover = isLight ? QStringLiteral("#cbd5e1")
                                        : (isZinc ? QStringLiteral("#63646c") : QStringLiteral("#334155"));

    const QString css = QStringLiteral(
        "QWidget#ImageStudioWidget, QScrollArea#ImageStudioScroll, QWidget#ImageStudioCanvas {"
        "   background-color: %1; color: %2; border: none;"
        "}"
        "QWidget#ImageStudioWidget { border-left: 1px solid %4; }"
        "QFrame#ImageStudioCard {"
        "   background-color: %3; border: 1px solid %4; border-radius: 12px;"
        "}"
        "QLabel { color: %2; background: transparent; border: none; font-size: 11.5px; }"
        "QLabel#ImageStudioSectionTitle { font-weight: 700; font-size: 12px; color: #38bdf8; }"
        "QLabel#ImageStudioValLabel { color: %5; font-size: 11px; }"
        "QComboBox {"
        "   background-color: %6; color: %2; border: 1px solid %4; border-radius: 8px; padding: 3px 10px; font-size: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox QAbstractItemView {"
        "   background-color: %3; color: %2; border: 1px solid %4; selection-background-color: #0284c7; selection-color: #ffffff; outline: none;"
        "}"
        "QPushButton#ImageStudioPrimaryBtn {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff; border: none; border-radius: 8px; padding: 0px 10px; font-weight: 600; font-size: 11.5px;"
        "}"
        "QPushButton#ImageStudioPrimaryBtn:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6); }"
        "QPushButton#ImageStudioSecBtn {"
        "   background-color: %7; color: %2; border: 1px solid %4; border-radius: 8px; padding: 0px 8px; font-weight: 600; font-size: 11px;"
        "}"
        "QPushButton#ImageStudioSecBtn:hover { background-color: %8; }"
    ).arg(bg, text, cardBg, border, subText, inputBg, secBtnBg, secBtnHover);

    setStyleSheet(css + ThemeManager::scrollBarStyleSheet(preset));
}
