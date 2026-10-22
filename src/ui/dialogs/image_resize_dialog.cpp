#include "image_resize_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <cmath>

ImageResizeDialog::ImageResizeDialog(int originalWidth, int originalHeight, QWidget *parent)
    : QDialog(parent)
    , m_origWidth(std::max(1, originalWidth))
    , m_origHeight(std::max(1, originalHeight))
{
    setWindowTitle(tr("Resize Image"));
    resize(460, 380);
    setMinimumWidth(400);

    setupUi();
}

void ImageResizeDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    // 1. Header Banner
    auto *headerWidget = new QWidget(this);
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    auto *titleLabel = new QLabel(tr("<b>Resize Image</b>"), headerWidget);
    QFont f = titleLabel->font();
    f.setPointSize(f.pointSize() + 2);
    titleLabel->setFont(f);

    auto *subtitleLabel = new QLabel(
        tr("Original size: %1 × %2 pixels (%3 MP)")
            .arg(m_origWidth)
            .arg(m_origHeight)
            .arg((static_cast<double>(m_origWidth) * m_origHeight) / 1000000.0, 0, 'f', 2),
        headerWidget);
    subtitleLabel->setStyleSheet("color: #888888;");

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);
    rootLayout->addWidget(headerWidget);

    // 2. Dimensions Group
    auto *grpDimensions = new QGroupBox(tr("Dimensions"), this);
    auto *gridDimensions = new QGridLayout(grpDimensions);
    gridDimensions->setContentsMargins(14, 14, 14, 14);
    gridDimensions->setSpacing(10);

    // Column headers
    gridDimensions->addWidget(new QLabel(tr("<b>Pixels</b>"), grpDimensions), 0, 1, Qt::AlignCenter);
    gridDimensions->addWidget(new QLabel(tr("<b>Percentage</b>"), grpDimensions), 0, 2, Qt::AlignCenter);

    // Width
    gridDimensions->addWidget(new QLabel(tr("Width:"), grpDimensions), 1, 0);
    m_spinPixelWidth = new QSpinBox(grpDimensions);
    m_spinPixelWidth->setRange(1, 65535);
    m_spinPixelWidth->setValue(m_origWidth);
    m_spinPixelWidth->setSuffix(" px");
    gridDimensions->addWidget(m_spinPixelWidth, 1, 1);

    m_spinPercentWidth = new QDoubleSpinBox(grpDimensions);
    m_spinPercentWidth->setRange(1.0, 2000.0);
    m_spinPercentWidth->setValue(100.0);
    m_spinPercentWidth->setDecimals(1);
    m_spinPercentWidth->setSuffix(" %");
    gridDimensions->addWidget(m_spinPercentWidth, 1, 2);

    // Height
    gridDimensions->addWidget(new QLabel(tr("Height:"), grpDimensions), 2, 0);
    m_spinPixelHeight = new QSpinBox(grpDimensions);
    m_spinPixelHeight->setRange(1, 65535);
    m_spinPixelHeight->setValue(m_origHeight);
    m_spinPixelHeight->setSuffix(" px");
    gridDimensions->addWidget(m_spinPixelHeight, 2, 1);

    m_spinPercentHeight = new QDoubleSpinBox(grpDimensions);
    m_spinPercentHeight->setRange(1.0, 2000.0);
    m_spinPercentHeight->setValue(100.0);
    m_spinPercentHeight->setDecimals(1);
    m_spinPercentHeight->setSuffix(" %");
    gridDimensions->addWidget(m_spinPercentHeight, 2, 2);

    // Keep aspect ratio checkbox
    m_chkKeepRatio = new QCheckBox(tr("🔒 Keep aspect ratio (proportional)"), grpDimensions);
    m_chkKeepRatio->setChecked(true);
    gridDimensions->addWidget(m_chkKeepRatio, 3, 0, 1, 3);

    rootLayout->addWidget(grpDimensions);

    // 3. Presets & Quality Options Group
    auto *grpOptions = new QGroupBox(tr("Presets && Quality"), this);
    auto *formOptions = new QFormLayout(grpOptions);
    formOptions->setContentsMargins(14, 14, 14, 14);
    formOptions->setSpacing(10);

    m_comboPresets = new QComboBox(grpOptions);
    m_comboPresets->addItem(tr("Custom"));
    m_comboPresets->addItem(tr("25% (Quarter size)"));
    m_comboPresets->addItem(tr("50% (Half size)"));
    m_comboPresets->addItem(tr("75% (Three quarters)"));
    m_comboPresets->addItem(tr("100% (Original size)"));
    m_comboPresets->addItem(tr("150%"));
    m_comboPresets->addItem(tr("200% (Double size)"));
    m_comboPresets->addItem(tr("Fit Full HD (1920 × 1080)"));
    m_comboPresets->addItem(tr("Fit HD (1280 × 720)"));
    m_comboPresets->addItem(tr("Fit 4K UHD (3840 × 2160)"));
    m_comboPresets->addItem(tr("Square (1080 × 1080)"));
    m_comboPresets->setCurrentIndex(4); // 100% default
    formOptions->addRow(tr("Quick Preset:"), m_comboPresets);

    m_comboQuality = new QComboBox(grpOptions);
    m_comboQuality->addItem(tr("Smooth / Bicubic (Best for photos and general images)"), static_cast<int>(Qt::SmoothTransformation));
    m_comboQuality->addItem(tr("Nearest Neighbor (Sharp pixel edges / Pixel Art)"), static_cast<int>(Qt::FastTransformation));
    formOptions->addRow(tr("Resampling:"), m_comboQuality);

    rootLayout->addWidget(grpOptions);

    // 4. Result Summary Banner
    m_lblSummary = new QLabel(this);
    m_lblSummary->setStyleSheet("background: rgba(128, 128, 128, 0.1); border-radius: 4px; padding: 6px 10px; font-size: 11px;");
    rootLayout->addWidget(m_lblSummary);

    // 5. Button Box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Resize"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttonBox);

    // Signal connections
    connect(m_spinPixelWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageResizeDialog::onPixelWidthChanged);
    connect(m_spinPixelHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageResizeDialog::onPixelHeightChanged);
    connect(m_spinPercentWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImageResizeDialog::onPercentWidthChanged);
    connect(m_spinPercentHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImageResizeDialog::onPercentHeightChanged);
    connect(m_chkKeepRatio, &QCheckBox::toggled, this, &ImageResizeDialog::onKeepRatioToggled);
    connect(m_comboPresets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImageResizeDialog::onPresetSelected);

    updateSummary();
}

int ImageResizeDialog::targetWidth() const
{
    return m_spinPixelWidth ? m_spinPixelWidth->value() : m_origWidth;
}

int ImageResizeDialog::targetHeight() const
{
    return m_spinPixelHeight ? m_spinPixelHeight->value() : m_origHeight;
}

Qt::TransformationMode ImageResizeDialog::transformationMode() const
{
    if (!m_comboQuality) return Qt::SmoothTransformation;
    return static_cast<Qt::TransformationMode>(m_comboQuality->currentData().toInt());
}

void ImageResizeDialog::onPixelWidthChanged(int w)
{
    if (m_updating) return;
    m_updating = true;

    if (m_comboPresets) m_comboPresets->setCurrentIndex(0);

    double p = (static_cast<double>(w) * 100.0) / m_origWidth;
    m_spinPercentWidth->setValue(p);

    if (m_chkKeepRatio->isChecked()) {
        double ratio = static_cast<double>(m_origHeight) / m_origWidth;
        int newH = std::max(1, static_cast<int>(std::round(w * ratio)));
        m_spinPixelHeight->setValue(newH);
        m_spinPercentHeight->setValue((static_cast<double>(newH) * 100.0) / m_origHeight);
    }

    m_updating = false;
    updateSummary();
}

void ImageResizeDialog::onPixelHeightChanged(int h)
{
    if (m_updating) return;
    m_updating = true;

    if (m_comboPresets) m_comboPresets->setCurrentIndex(0);

    double p = (static_cast<double>(h) * 100.0) / m_origHeight;
    m_spinPercentHeight->setValue(p);

    if (m_chkKeepRatio->isChecked()) {
        double ratio = static_cast<double>(m_origWidth) / m_origHeight;
        int newW = std::max(1, static_cast<int>(std::round(h * ratio)));
        m_spinPixelWidth->setValue(newW);
        m_spinPercentWidth->setValue((static_cast<double>(newW) * 100.0) / m_origWidth);
    }

    m_updating = false;
    updateSummary();
}

void ImageResizeDialog::onPercentWidthChanged(double p)
{
    if (m_updating) return;
    m_updating = true;

    if (m_comboPresets) m_comboPresets->setCurrentIndex(0);

    int newW = std::max(1, static_cast<int>(std::round((p * m_origWidth) / 100.0)));
    m_spinPixelWidth->setValue(newW);

    if (m_chkKeepRatio->isChecked()) {
        m_spinPercentHeight->setValue(p);
        int newH = std::max(1, static_cast<int>(std::round((p * m_origHeight) / 100.0)));
        m_spinPixelHeight->setValue(newH);
    }

    m_updating = false;
    updateSummary();
}

void ImageResizeDialog::onPercentHeightChanged(double p)
{
    if (m_updating) return;
    m_updating = true;

    if (m_comboPresets) m_comboPresets->setCurrentIndex(0);

    int newH = std::max(1, static_cast<int>(std::round((p * m_origHeight) / 100.0)));
    m_spinPixelHeight->setValue(newH);

    if (m_chkKeepRatio->isChecked()) {
        m_spinPercentWidth->setValue(p);
        int newW = std::max(1, static_cast<int>(std::round((p * m_origWidth) / 100.0)));
        m_spinPixelWidth->setValue(newW);
    }

    m_updating = false;
    updateSummary();
}

void ImageResizeDialog::onKeepRatioToggled(bool keep)
{
    if (keep && !m_updating) {
        onPixelWidthChanged(m_spinPixelWidth->value());
    }
}

void ImageResizeDialog::onPresetSelected(int index)
{
    if (index == 0 || m_updating) return; // Custom

    m_updating = true;
    double scale = 1.0;
    bool fitBox = false;
    bool exactBox = false;
    int boxW = 0, boxH = 0;

    switch (index) {
    case 1: scale = 0.25; break; // 25%
    case 2: scale = 0.50; break; // 50%
    case 3: scale = 0.75; break; // 75%
    case 4: scale = 1.00; break; // 100%
    case 5: scale = 1.50; break; // 150%
    case 6: scale = 2.00; break; // 200%
    case 7: fitBox = true; boxW = 1920; boxH = 1080; break; // Full HD
    case 8: fitBox = true; boxW = 1280; boxH = 720;  break; // HD
    case 9: fitBox = true; boxW = 3840; boxH = 2160; break; // 4K UHD
    case 10: exactBox = true; boxW = 1080; boxH = 1080; break; // Square
    default: break;
    }

    int targetW = m_origWidth;
    int targetH = m_origHeight;

    if (exactBox) {
        m_chkKeepRatio->setChecked(false);
        targetW = boxW;
        targetH = boxH;
    } else if (fitBox) {
        if (m_chkKeepRatio->isChecked()) {
            double rw = static_cast<double>(boxW) / m_origWidth;
            double rh = static_cast<double>(boxH) / m_origHeight;
            double r = std::min(rw, rh);
            targetW = std::max(1, static_cast<int>(std::round(m_origWidth * r)));
            targetH = std::max(1, static_cast<int>(std::round(m_origHeight * r)));
        } else {
            targetW = boxW;
            targetH = boxH;
        }
    } else {
        targetW = std::max(1, static_cast<int>(std::round(m_origWidth * scale)));
        targetH = std::max(1, static_cast<int>(std::round(m_origHeight * scale)));
    }

    m_spinPixelWidth->setValue(targetW);
    m_spinPixelHeight->setValue(targetH);
    m_spinPercentWidth->setValue((static_cast<double>(targetW) * 100.0) / m_origWidth);
    m_spinPercentHeight->setValue((static_cast<double>(targetH) * 100.0) / m_origHeight);

    m_updating = false;
    updateSummary();
}

void ImageResizeDialog::updateSummary()
{
    int w = targetWidth();
    int h = targetHeight();
    double oldPixels = static_cast<double>(m_origWidth) * m_origHeight;
    double newPixels = static_cast<double>(w) * h;
    double diffPercent = ((newPixels - oldPixels) / oldPixels) * 100.0;

    QString sign = (diffPercent >= 0.0) ? "+" : "";
    QString mpStr = QString::number(newPixels / 1000000.0, 'f', 2);

    m_lblSummary->setText(
        tr("<b>New size:</b> %1 × %2 px (%3 MP) • <b>Change:</b> %4%5% total pixels")
            .arg(w)
            .arg(h)
            .arg(mpStr, sign, QString::number(diffPercent, 'f', 1)));
}
