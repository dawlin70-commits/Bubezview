#include "equalizer_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPainter>
#include <QLinearGradient>
#include <QInputDialog>
#include <QMessageBox>
#include <QStyle>
#include <QFrame>

// ============================================================================
// StereoVuMeterWidget
// ============================================================================

StereoVuMeterWidget::StereoVuMeterWidget(QWidget *parent)
    : QWidget(parent)
    , m_left(0.0f)
    , m_right(0.0f)
    , m_leftMax(0.0f)
    , m_rightMax(0.0f)
    , m_targetLeft(0.0f)
    , m_targetRight(0.0f)
    , m_decayTimer(new QTimer(this))
{
    setMinimumSize(48, 140);
    setMaximumWidth(64);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    connect(m_decayTimer, &QTimer::timeout, this, &StereoVuMeterWidget::onDecayTick);
}

void StereoVuMeterWidget::setLevels(float leftPeak, float rightPeak)
{
    m_targetLeft = qBound(0.0f, leftPeak, 1.2f);
    m_targetRight = qBound(0.0f, rightPeak, 1.2f);

    // Fast immediate attack on rising peak
    if (m_targetLeft > m_left) {
        m_left = m_left * 0.25f + m_targetLeft * 0.75f;
    }
    if (m_targetRight > m_right) {
        m_right = m_right * 0.25f + m_targetRight * 0.75f;
    }

    if (m_left > m_leftMax) m_leftMax = m_left;
    if (m_right > m_rightMax) m_rightMax = m_right;

    if (!m_decayTimer->isActive()) {
        m_decayTimer->start(30);
    }
    update();
}

void StereoVuMeterWidget::onDecayTick()
{
    // Natural studio meter falloff
    m_left = qMax(0.0f, m_left * 0.85f);
    m_right = qMax(0.0f, m_right * 0.85f);

    m_leftMax = qMax(0.0f, m_leftMax - 0.02f);
    m_rightMax = qMax(0.0f, m_rightMax - 0.02f);

    if (m_left < 0.005f && m_right < 0.005f && m_leftMax < 0.005f && m_rightMax < 0.005f) {
        m_left = 0.0f;
        m_right = 0.0f;
        m_leftMax = 0.0f;
        m_rightMax = 0.0f;
        m_decayTimer->stop();
    }
    update();
}

void StereoVuMeterWidget::reset()
{
    m_left = 0.0f;
    m_right = 0.0f;
    m_leftMax = 0.0f;
    m_rightMax = 0.0f;
    m_targetLeft = 0.0f;
    m_targetRight = 0.0f;
    if (m_decayTimer) {
        m_decayTimer->stop();
    }
    update();
}

void StereoVuMeterWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int totalW = width();
    const int totalH = height();

    // Background container
    p.fillRect(rect(), QColor(24, 24, 27));

    const int labelHeight = 16;
    const int meterTop = 8;
    const int meterHeight = totalH - labelHeight - meterTop - 4;
    const int barWidth = 9;
    const int spacing = 4;
    const int startX = (totalW - (barWidth * 2 + spacing)) / 2;

    auto drawMeterBar = [&](int x, float level, float peak) {
        // Meter background slot
        QRect barRect(x, meterTop, barWidth, meterHeight);
        p.fillRect(barRect, QColor(36, 36, 42));

        // Active fill
        float clamped = qBound(0.0f, level, 1.0f);
        int fillH = static_cast<int>(clamped * meterHeight);
        if (fillH > 0) {
            QRect fillRect(x, meterTop + meterHeight - fillH, barWidth, fillH);
            QLinearGradient grad(0, meterTop + meterHeight, 0, meterTop);
            grad.setColorAt(0.0, QColor(16, 185, 129));   // Green: -48dB to -12dB
            grad.setColorAt(0.65, QColor(245, 158, 11)); // Yellow: -12dB to -3dB
            grad.setColorAt(0.92, QColor(239, 68, 68));  // Red: 0dB clipping
            p.fillRect(fillRect, grad);
        }

        // Peak tick
        float clampedPeak = qBound(0.0f, peak, 1.0f);
        if (clampedPeak > 0.03f) {
            int peakY = meterTop + meterHeight - static_cast<int>(clampedPeak * meterHeight);
            p.setPen(clampedPeak >= 0.95f ? QColor(255, 60, 60) : QColor(255, 255, 255, 220));
            p.drawLine(x, peakY, x + barWidth - 1, peakY);
        }

        // Border
        p.setPen(QColor(60, 60, 65));
        p.drawRect(barRect);
    };

    // Draw Left & Right bars
    drawMeterBar(startX, m_left, m_leftMax);
    drawMeterBar(startX + barWidth + spacing, m_right, m_rightMax);

    // Channel labels below
    p.setPen(QColor(160, 160, 160));
    QFont f = font();
    f.setPointSize(8);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(startX, totalH - labelHeight - 2, barWidth, labelHeight), Qt::AlignCenter, "L");
    p.drawText(QRect(startX + barWidth + spacing, totalH - labelHeight - 2, barWidth, labelHeight), Qt::AlignCenter, "R");
}

// ============================================================================
// EqualizerWidget
// ============================================================================

EqualizerWidget::EqualizerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshPresetsCombo();

    // Connect preset manager changes to keep instances synchronized
    connect(&EqualizerManager::instance(), &EqualizerManager::presetsChanged,
            this, &EqualizerWidget::refreshPresetsCombo);

    // Select default "Flat" preset initially
    int flatIdx = m_comboPresets->findText(tr("Flat"));
    if (flatIdx >= 0) {
        m_comboPresets->setCurrentIndex(flatIdx);
    }
}

void EqualizerWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(10);

    // 1. Top Control Bar: Presets & Actions
    auto *topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    auto *lblPreset = new QLabel(tr("Preset:"), this);
    lblPreset->setStyleSheet("font-weight: bold;");
    topLayout->addWidget(lblPreset);

    m_comboPresets = new QComboBox(this);
    m_comboPresets->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_comboPresets->setMinimumWidth(160);
    connect(m_comboPresets, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EqualizerWidget::onPresetSelected);
    topLayout->addWidget(m_comboPresets);

    m_btnSavePreset = new QPushButton(tr("💾 Save..."), this);
    m_btnSavePreset->setToolTip(tr("Save current equalizer configuration as a custom preset"));
    connect(m_btnSavePreset, &QPushButton::clicked, this, &EqualizerWidget::onSavePresetClicked);
    topLayout->addWidget(m_btnSavePreset);

    m_btnDeletePreset = new QPushButton(tr("🗑️ Delete"), this);
    m_btnDeletePreset->setToolTip(tr("Delete selected custom preset"));
    m_btnDeletePreset->setEnabled(false);
    connect(m_btnDeletePreset, &QPushButton::clicked, this, &EqualizerWidget::onDeletePresetClicked);
    topLayout->addWidget(m_btnDeletePreset);

    m_btnResetFlat = new QPushButton(tr("🔄 Flat"), this);
    m_btnResetFlat->setToolTip(tr("Reset all bands and pre-amp to 0 dB"));
    connect(m_btnResetFlat, &QPushButton::clicked, this, &EqualizerWidget::onResetFlatClicked);
    topLayout->addWidget(m_btnResetFlat);

    topLayout->addSpacing(12);

    m_chkBypass = new QCheckBox(tr("Bypass EQ"), this);
    m_chkBypass->setToolTip(tr("Bypass equalizer processing without losing slider positions"));
    connect(m_chkBypass, &QCheckBox::toggled, this, &EqualizerWidget::onBypassToggled);
    topLayout->addWidget(m_chkBypass);

    mainLayout->addLayout(topLayout);

    // Separator line
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("color: #3f3f46;");
    mainLayout->addWidget(line);

    // 2. Middle Area: Pre-Amp | 10 Graphic Bands | VU Meter & DSP Tools
    auto *midLayout = new QHBoxLayout();
    midLayout->setSpacing(14);

    // --- Pre-Amp Section ---
    auto *preAmpGroup = new QGroupBox(tr("Pre-Amp"), this);
    auto *preAmpLayout = new QVBoxLayout(preAmpGroup);
    preAmpLayout->setContentsMargins(6, 8, 6, 8);
    preAmpLayout->setSpacing(4);

    m_lblPreAmpVal = new QLabel("0.0 dB", this);
    m_lblPreAmpVal->setAlignment(Qt::AlignCenter);
    m_lblPreAmpVal->setStyleSheet("font-size: 11px; font-weight: bold; color: #a1a1aa;");
    preAmpLayout->addWidget(m_lblPreAmpVal);

    m_sliderPreAmp = new QSlider(Qt::Vertical, this);
    m_sliderPreAmp->setRange(-120, 120); // -12.0 to +12.0 dB
    m_sliderPreAmp->setValue(0);
    m_sliderPreAmp->setTickPosition(QSlider::TicksBothSides);
    m_sliderPreAmp->setTickInterval(60);
    m_sliderPreAmp->setToolTip(tr("Pre-Amplification Gain (-12 dB to +12 dB)"));
    connect(m_sliderPreAmp, &QSlider::valueChanged, this, &EqualizerWidget::onSliderValueChanged);
    preAmpLayout->addWidget(m_sliderPreAmp, 1, Qt::AlignHCenter);

    auto *lblPreAmpBottom = new QLabel(tr("Gain"), this);
    lblPreAmpBottom->setAlignment(Qt::AlignCenter);
    lblPreAmpBottom->setStyleSheet("font-size: 11px; color: #71717a;");
    preAmpLayout->addWidget(lblPreAmpBottom);

    midLayout->addWidget(preAmpGroup);

    // --- 10 Frequency Bands ---
    auto *bandsGroup = new QGroupBox(tr("Graphic Equalizer (10 Bands)"), this);
    auto *bandsLayout = new QHBoxLayout(bandsGroup);
    bandsLayout->setContentsMargins(10, 8, 10, 8);
    bandsLayout->setSpacing(8);

    const auto &frequencies = EqualizerManager::bandFrequencies();
    m_bandSliders.reserve(frequencies.size());
    m_bandValLabels.reserve(frequencies.size());

    for (int i = 0; i < frequencies.size(); ++i) {
        auto *colLayout = new QVBoxLayout();
        colLayout->setSpacing(4);

        auto *valLabel = new QLabel("0.0 dB", this);
        valLabel->setAlignment(Qt::AlignCenter);
        valLabel->setStyleSheet("font-size: 10px; color: #a1a1aa;");
        m_bandValLabels.append(valLabel);
        colLayout->addWidget(valLabel);

        auto *slider = new QSlider(Qt::Vertical, this);
        slider->setRange(-120, 120); // -12.0 to +12.0 dB
        slider->setValue(0);
        slider->setTickPosition(QSlider::TicksBothSides);
        slider->setTickInterval(60);
        slider->setToolTip(QString("%1: %2").arg(tr("Band")).arg(EqualizerManager::formatFrequency(frequencies[i])));
        connect(slider, &QSlider::valueChanged, this, &EqualizerWidget::onSliderValueChanged);
        m_bandSliders.append(slider);
        colLayout->addWidget(slider, 1, Qt::AlignHCenter);

        auto *freqLabel = new QLabel(EqualizerManager::formatFrequency(frequencies[i]), this);
        freqLabel->setAlignment(Qt::AlignCenter);
        freqLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #71717a;");
        colLayout->addWidget(freqLabel);

        bandsLayout->addLayout(colLayout);
    }
    midLayout->addWidget(bandsGroup, 1);

    // --- Master & Spatial Tools Section ---
    auto *masterGroup = new QGroupBox(tr("Master & Output"), this);
    auto *masterLayout = new QHBoxLayout(masterGroup);
    masterLayout->setContentsMargins(8, 8, 8, 8);
    masterLayout->setSpacing(10);

    // Width & Limiter Column
    auto *toolsCol = new QVBoxLayout();
    toolsCol->setSpacing(8);

    m_chkLimiter = new QCheckBox(tr("Anti-Clip Limiter"), this);
    m_chkLimiter->setChecked(true);
    m_chkLimiter->setToolTip(tr("True-peak audio limiter preventing distortion from frequency boosts"));
    connect(m_chkLimiter, &QCheckBox::toggled, this, [this](bool) {
        onSliderValueChanged();
    });
    toolsCol->addWidget(m_chkLimiter);

    auto *lblWidth = new QLabel(tr("Stereo Width:"), this);
    lblWidth->setStyleSheet("font-size: 11px; color: #a1a1aa;");
    toolsCol->addWidget(lblWidth);

    m_lblWidthVal = new QLabel("100%", this);
    m_lblWidthVal->setAlignment(Qt::AlignCenter);
    m_lblWidthVal->setStyleSheet("font-size: 11px; font-weight: bold;");
    toolsCol->addWidget(m_lblWidthVal);

    m_sliderWidth = new QSlider(Qt::Horizontal, this);
    m_sliderWidth->setRange(0, 200); // 0% (Mono) to 200% (Ultra-Wide)
    m_sliderWidth->setValue(100);
    m_sliderWidth->setToolTip(tr("Stereo Imaging: 0% Mono, 100% Normal, 200% Wide"));
    connect(m_sliderWidth, &QSlider::valueChanged, this, [this](int val) {
        m_lblWidthVal->setText(QString("%1%").arg(val));
        onSliderValueChanged();
    });
    toolsCol->addWidget(m_sliderWidth);
    toolsCol->addStretch();

    masterLayout->addLayout(toolsCol);

    // VU Peak Meter
    m_vuMeter = new StereoVuMeterWidget(this);
    masterLayout->addWidget(m_vuMeter);

    midLayout->addWidget(masterGroup);
    mainLayout->addLayout(midLayout);

    applyTheme();
}

void EqualizerWidget::applyTheme()
{
    setStyleSheet(
        "QSlider::groove:vertical {"
        "    background: #27272a;"
        "    width: 6px;"
        "    border-radius: 3px;"
        "}"
        "QSlider::sub-page:vertical {"
        "    background: #27272a;"
        "    border-radius: 3px;"
        "}"
        "QSlider::add-page:vertical {"
        "    background: #3b82f6;"
        "    border-radius: 3px;"
        "}"
        "QSlider::handle:vertical {"
        "    background: #f4f4f5;"
        "    border: 1px solid #71717a;"
        "    height: 14px;"
        "    margin: 0 -4px;"
        "    border-radius: 7px;"
        "}"
        "QSlider::handle:vertical:hover {"
        "    background: #60a5fa;"
        "    border-color: #3b82f6;"
        "}"
    );
}

EqualizerPreset EqualizerWidget::currentPreset() const
{
    EqualizerPreset p;
    p.name = m_comboPresets->currentText();
    p.preAmpDb = m_sliderPreAmp->value() / 10.0;
    p.bandGainsDb.resize(m_bandSliders.size());
    for (int i = 0; i < m_bandSliders.size(); ++i) {
        p.bandGainsDb[i] = m_bandSliders[i]->value() / 10.0;
    }
    p.limiterEnabled = m_chkLimiter->isChecked();
    p.stereoWidth = m_sliderWidth->value() / 100.0;
    return p;
}

void EqualizerWidget::applyPreset(const EqualizerPreset &preset)
{
    m_updatingUi = true;

    m_sliderPreAmp->setValue(static_cast<int>(preset.preAmpDb * 10));
    m_lblPreAmpVal->setText(QString("%1%2 dB")
        .arg(preset.preAmpDb > 0.05 ? "+" : "")
        .arg(preset.preAmpDb, 0, 'f', 1));

    for (int i = 0; i < m_bandSliders.size(); ++i) {
        double gain = (i < preset.bandGainsDb.size()) ? preset.bandGainsDb[i] : 0.0;
        m_bandSliders[i]->setValue(static_cast<int>(gain * 10));
        m_bandValLabels[i]->setText(QString("%1%2 dB")
            .arg(gain > 0.05 ? "+" : "")
            .arg(gain, 0, 'f', 1));
    }

    m_chkLimiter->setChecked(preset.limiterEnabled);
    m_sliderWidth->setValue(static_cast<int>(preset.stereoWidth * 100));
    m_lblWidthVal->setText(QString("%1%").arg(static_cast<int>(preset.stereoWidth * 100)));

    m_updatingUi = false;
    emit configurationChanged(currentPreset(), isBypassed());
}

bool EqualizerWidget::isBypassed() const
{
    return m_chkBypass->isChecked();
}

void EqualizerWidget::updateVuMeter(float left, float right)
{
    if (m_vuMeter) {
        m_vuMeter->setLevels(left, right);
    }
}

void EqualizerWidget::onPresetSelected(int index)
{
    if (index < 0 || m_updatingUi) return;

    QString presetName = m_comboPresets->itemData(index).toString();
    EqualizerPreset p = EqualizerManager::instance().presetByName(presetName);

    m_btnDeletePreset->setEnabled(!p.isFactory);

    applyPreset(p);
}

void EqualizerWidget::onSavePresetClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save Custom Preset"),
                                         tr("Enter preset name:"),
                                         QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    EqualizerPreset p = currentPreset();
    p.name = name;
    p.isFactory = false;

    if (EqualizerManager::instance().saveCustomPreset(p)) {
        refreshPresetsCombo();
        int idx = m_comboPresets->findData(name);
        if (idx >= 0) {
            m_comboPresets->setCurrentIndex(idx);
        }
    } else {
        QMessageBox::warning(this, tr("Save Error"),
                             tr("Could not save preset. Name may already exist or be reserved by a factory preset."));
    }
}

void EqualizerWidget::onDeletePresetClicked()
{
    int idx = m_comboPresets->currentIndex();
    if (idx < 0) return;

    QString name = m_comboPresets->itemData(idx).toString();
    auto reply = QMessageBox::question(this, tr("Delete Preset"),
                                       tr("Are you sure you want to delete custom preset '%1'?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (EqualizerManager::instance().deleteCustomPreset(name)) {
            refreshPresetsCombo();
        }
    }
}

void EqualizerWidget::onResetFlatClicked()
{
    int flatIdx = m_comboPresets->findText(tr("Flat"));
    if (flatIdx >= 0) {
        m_comboPresets->setCurrentIndex(flatIdx);
    } else {
        EqualizerPreset flatPreset;
        applyPreset(flatPreset);
    }
}

void EqualizerWidget::onSliderValueChanged()
{
    if (m_updatingUi) return;

    // Update labels
    double preAmpVal = m_sliderPreAmp->value() / 10.0;
    m_lblPreAmpVal->setText(QString("%1%2 dB")
        .arg(preAmpVal > 0.05 ? "+" : "")
        .arg(preAmpVal, 0, 'f', 1));

    for (int i = 0; i < m_bandSliders.size(); ++i) {
        double gain = m_bandSliders[i]->value() / 10.0;
        m_bandValLabels[i]->setText(QString("%1%2 dB")
            .arg(gain > 0.05 ? "+" : "")
            .arg(gain, 0, 'f', 1));
    }

    emit configurationChanged(currentPreset(), isBypassed());
}

void EqualizerWidget::onBypassToggled(bool checked)
{
    // Visually toggle enabled state of sliders
    m_sliderPreAmp->setEnabled(!checked);
    for (auto *slider : m_bandSliders) {
        slider->setEnabled(!checked);
    }
    m_chkLimiter->setEnabled(!checked);
    m_sliderWidth->setEnabled(!checked);

    emit configurationChanged(currentPreset(), checked);
}

void EqualizerWidget::refreshPresetsCombo()
{
    QString currentSelected = m_comboPresets->currentData().toString();

    m_updatingUi = true;
    m_comboPresets->clear();

    const auto presets = EqualizerManager::instance().allPresets();
    for (const auto &p : presets) {
        QString displayText = p.name;
        if (!p.isFactory) {
            displayText = QString("⭐ %1 [Custom]").arg(p.name);
        }
        m_comboPresets->addItem(displayText, p.name);
    }

    // Restore previous selection if possible
    int foundIdx = m_comboPresets->findData(currentSelected);
    if (foundIdx >= 0) {
        m_comboPresets->setCurrentIndex(foundIdx);
    } else if (m_comboPresets->count() > 0) {
        m_comboPresets->setCurrentIndex(0);
    }

    // Enable/disable delete button based on current preset
    QString activeName = m_comboPresets->currentData().toString();
    EqualizerPreset activePreset = EqualizerManager::instance().presetByName(activeName);
    m_btnDeletePreset->setEnabled(!activePreset.isFactory);

    m_updatingUi = false;
}
