/**
 * @file pdf_watermark_dialog.cpp
 * @brief Implementation of watermark stamp customization dialog.
 */

#include "pdf_watermark_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

PdfWatermarkDialog::PdfWatermarkDialog(QWidget *parent)
    : QDialog(parent)
    , m_color(QColor(QStringLiteral("#DC2626"))) // Default Red
{
    setWindowTitle(tr("🌊 Watermark & Document Stamp"));
    resize(420, 380);
    setupUi();
}

PdfWatermarkDialog::~PdfWatermarkDialog() = default;

void PdfWatermarkDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // Text & Presets
    QGroupBox *grpText = new QGroupBox(tr("Stamp Text"), this);
    QVBoxLayout *txtLayout = new QVBoxLayout(grpText);
    txtLayout->setSpacing(8);

    m_comboPresets = new QComboBox(grpText);
    m_comboPresets->addItem(tr("COPIA (COPY)"), QStringLiteral("COPIA"));
    m_comboPresets->addItem(tr("BOZZA (DRAFT)"), QStringLiteral("BOZZA"));
    m_comboPresets->addItem(tr("CONFIDENZIALE (CONFIDENTIAL)"), QStringLiteral("CONFIDENZIALE"));
    m_comboPresets->addItem(tr("APPROVATO (APPROVED)"), QStringLiteral("APPROVATO"));
    m_comboPresets->addItem(tr("ANNULLATO (VOID / CANCELLED)"), QStringLiteral("ANNULLATO"));
    m_comboPresets->addItem(tr("PAGATO (PAID)"), QStringLiteral("PAGATO"));
    m_comboPresets->addItem(tr("Custom Text..."), QString());
    connect(m_comboPresets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PdfWatermarkDialog::onPresetChanged);
    txtLayout->addWidget(m_comboPresets);

    m_txtCustom = new QLineEdit(grpText);
    m_txtCustom->setPlaceholderText(tr("Type custom stamp text..."));
    m_txtCustom->setText(QStringLiteral("COPIA"));
    m_txtCustom->setEnabled(false);
    connect(m_txtCustom, &QLineEdit::textChanged, this, &PdfWatermarkDialog::emitChanged);
    txtLayout->addWidget(m_txtCustom);
    mainLayout->addWidget(grpText);

    // Geometry & Appearance
    QGroupBox *grpStyle = new QGroupBox(tr("Appearance & Angle"), this);
    QVBoxLayout *styleLayout = new QVBoxLayout(grpStyle);
    styleLayout->setSpacing(10);

    // Graphic Style Row
    QHBoxLayout *styleRow = new QHBoxLayout();
    QLabel *lblStyle = new QLabel(tr("Style:"), grpStyle);
    styleRow->addWidget(lblStyle);

    m_comboStyle = new QComboBox(grpStyle);
    m_comboStyle->addItem(tr("Classic Rounded Border"), 0);
    m_comboStyle->addItem(tr("Official Double Border"), 1);
    m_comboStyle->addItem(tr("Dotted Border"), 2);
    m_comboStyle->addItem(tr("Plain Text (No Border)"), 3);
    connect(m_comboStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PdfWatermarkDialog::emitChanged);
    styleRow->addWidget(m_comboStyle, 1);
    styleLayout->addLayout(styleRow);

    // Angle Row (Free continuous degree adjustment)
    QHBoxLayout *angleRow = new QHBoxLayout();
    QLabel *lblAngle = new QLabel(tr("Angle:"), grpStyle);
    angleRow->addWidget(lblAngle);

    m_sliderAngle = new QSlider(Qt::Horizontal, grpStyle);
    m_sliderAngle->setRange(-180, 180);
    m_sliderAngle->setValue(45);
    angleRow->addWidget(m_sliderAngle, 1);

    m_spinAngle = new QSpinBox(grpStyle);
    m_spinAngle->setRange(-180, 180);
    m_spinAngle->setSuffix(QStringLiteral("°"));
    m_spinAngle->setValue(45);
    m_spinAngle->setFixedWidth(64);
    angleRow->addWidget(m_spinAngle);

    connect(m_sliderAngle, &QSlider::valueChanged, m_spinAngle, &QSpinBox::setValue);
    connect(m_spinAngle, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
        if (m_sliderAngle->value() != val) {
            m_sliderAngle->blockSignals(true);
            m_sliderAngle->setValue(val);
            m_sliderAngle->blockSignals(false);
        }
        emitChanged();
    });
    styleLayout->addLayout(angleRow);

    // Opacity Row
    QHBoxLayout *opacRow = new QHBoxLayout();
    QLabel *lblOpac = new QLabel(tr("Opacity:"), grpStyle);
    opacRow->addWidget(lblOpac);

    m_sliderOpacity = new QSlider(Qt::Horizontal, grpStyle);
    m_sliderOpacity->setRange(10, 100);
    m_sliderOpacity->setValue(35);
    opacRow->addWidget(m_sliderOpacity, 1);

    m_lblOpacityVal = new QLabel(QStringLiteral("35%"), grpStyle);
    m_lblOpacityVal->setFixedWidth(36);
    connect(m_sliderOpacity, &QSlider::valueChanged, this, [this](int val) {
        m_lblOpacityVal->setText(QStringLiteral("%1%").arg(val));
        emitChanged();
    });
    opacRow->addWidget(m_lblOpacityVal);
    styleLayout->addLayout(opacRow);

    // Color Row
    QHBoxLayout *colorRow = new QHBoxLayout();
    QLabel *lblColor = new QLabel(tr("Stamp Color:"), grpStyle);
    colorRow->addWidget(lblColor);

    m_btnColor = new QPushButton(grpStyle);
    m_btnColor->setFixedSize(30, 24);
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_color.name()));
    connect(m_btnColor, &QPushButton::clicked, this, &PdfWatermarkDialog::onColorClicked);
    colorRow->addWidget(m_btnColor);
    colorRow->addStretch(1);
    styleLayout->addLayout(colorRow);

    mainLayout->addWidget(grpStyle);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(tr("Done ✓"), this);
    m_btnOk->setDefault(true);
    m_btnOk->setStyleSheet(QStringLiteral("background-color: #0284C7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 6px;"));
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

QString PdfWatermarkDialog::watermarkText() const
{
    return m_txtCustom->text().trimmed();
}

qreal PdfWatermarkDialog::angle() const
{
    return static_cast<qreal>(m_spinAngle->value());
}

qreal PdfWatermarkDialog::opacity() const
{
    return static_cast<qreal>(m_sliderOpacity->value()) / 100.0;
}

int PdfWatermarkDialog::style() const
{
    return m_comboStyle->currentData().toInt();
}

void PdfWatermarkDialog::onPresetChanged(int index)
{
    QString preset = m_comboPresets->itemData(index).toString();
    if (preset.isEmpty()) {
        m_txtCustom->setEnabled(true);
        m_txtCustom->setFocus();
    } else {
        m_txtCustom->setText(preset);
        m_txtCustom->setEnabled(false);
    }
    emitChanged();
}

void PdfWatermarkDialog::onColorClicked()
{
    QColor chosen = QColorDialog::getColor(m_color, this, tr("Select Stamp Color"));
    if (chosen.isValid()) {
        m_color = chosen;
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_color.name()));
        emitChanged();
    }
}

void PdfWatermarkDialog::emitChanged()
{
    emit watermarkChanged(watermarkText(), angle(), opacity(), color(), style());
}

void PdfWatermarkDialog::setWatermarkText(const QString &text)
{
    int idx = m_comboPresets->findText(text);
    if (idx >= 0) {
        m_comboPresets->setCurrentIndex(idx);
        m_txtCustom->setText(text);
        m_txtCustom->setEnabled(false);
    } else {
        m_comboPresets->setCurrentIndex(m_comboPresets->count() - 1);
        m_txtCustom->setEnabled(true);
        m_txtCustom->setText(text);
    }
}

void PdfWatermarkDialog::setAngle(qreal angle)
{
    m_spinAngle->setValue(qRound(angle));
}

void PdfWatermarkDialog::setOpacity(qreal opacity)
{
    int val = qBound(10, static_cast<int>(opacity * 100.0), 100);
    m_sliderOpacity->setValue(val);
    m_lblOpacityVal->setText(QStringLiteral("%1%").arg(val));
}

void PdfWatermarkDialog::setColor(const QColor &color)
{
    m_color = color;
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_color.name()));
}

void PdfWatermarkDialog::setStyle(int style)
{
    int idx = m_comboStyle->findData(style);
    if (idx >= 0) {
        m_comboStyle->setCurrentIndex(idx);
    }
}

