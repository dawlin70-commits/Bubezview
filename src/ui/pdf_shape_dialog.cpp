/**
 * @file pdf_shape_dialog.cpp
 * @brief Implementation of shape and arrow configuration dialog for PDF markup.
 */

#include "pdf_shape_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QColorDialog>

PdfShapeDialog::PdfShapeDialog(QWidget *parent)
    : QDialog(parent)
    , m_strokeColor(QColor(QStringLiteral("#DC2626"))) // Default Red
    , m_fillColor(QColor(254, 240, 138, 120))          // Default semi-transparent yellow
{
    setWindowTitle(tr("📐 Shapes, Arrows & Drawing"));
    resize(380, 360);
    setupUi();
}

PdfShapeDialog::~PdfShapeDialog() = default;

void PdfShapeDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 1. Shape Type Group
    QGroupBox *grpShape = new QGroupBox(tr("Select Geometry"), this);
    QVBoxLayout *shapeLayout = new QVBoxLayout(grpShape);
    shapeLayout->setSpacing(8);

    m_radioRect = new QRadioButton(tr("▭ Rectangle / Box"), grpShape);
    m_radioRect->setChecked(true);
    m_radioEllipse = new QRadioButton(tr("◯ Circle / Ellipse"), grpShape);
    m_radioArrow = new QRadioButton(tr("➔ Directional Arrow"), grpShape);
    m_radioLine = new QRadioButton(tr("― Straight Line"), grpShape);
    m_radioFreehand = new QRadioButton(tr("✏️ Freehand Pen"), grpShape);

    shapeLayout->addWidget(m_radioRect);
    shapeLayout->addWidget(m_radioEllipse);
    shapeLayout->addWidget(m_radioArrow);
    shapeLayout->addWidget(m_radioLine);
    shapeLayout->addWidget(m_radioFreehand);
    mainLayout->addWidget(grpShape);

    // 2. Stroke & Color Options
    QGroupBox *grpStyle = new QGroupBox(tr("Stroke & Fill"), this);
    QVBoxLayout *styleLayout = new QVBoxLayout(grpStyle);
    styleLayout->setSpacing(10);

    QHBoxLayout *strokeRow = new QHBoxLayout();
    QLabel *lblStroke = new QLabel(tr("Stroke Color:"), grpStyle);
    strokeRow->addWidget(lblStroke);

    m_btnStrokeColor = new QPushButton(grpStyle);
    m_btnStrokeColor->setFixedSize(30, 24);
    m_btnStrokeColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_strokeColor.name()));
    connect(m_btnStrokeColor, &QPushButton::clicked, this, &PdfShapeDialog::onStrokeColorClicked);
    strokeRow->addWidget(m_btnStrokeColor);

    QLabel *lblWidth = new QLabel(tr("Thickness:"), grpStyle);
    lblWidth->setStyleSheet(QStringLiteral("margin-left: 12px;"));
    strokeRow->addWidget(lblWidth);

    m_spinWidth = new QSpinBox(grpStyle);
    m_spinWidth->setRange(1, 20);
    m_spinWidth->setValue(3);
    m_spinWidth->setSuffix(QStringLiteral(" px"));
    strokeRow->addWidget(m_spinWidth);
    strokeRow->addStretch(1);
    styleLayout->addLayout(strokeRow);

    // Fill Color Row
    QHBoxLayout *fillRow = new QHBoxLayout();
    m_chkEnableFill = new QCheckBox(tr("Fill Interior:"), grpStyle);
    m_chkEnableFill->setChecked(false);
    fillRow->addWidget(m_chkEnableFill);

    m_btnFillColor = new QPushButton(grpStyle);
    m_btnFillColor->setFixedSize(30, 24);
    m_btnFillColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_fillColor.name()));
    connect(m_btnFillColor, &QPushButton::clicked, this, &PdfShapeDialog::onFillColorClicked);
    fillRow->addWidget(m_btnFillColor);
    fillRow->addStretch(1);
    styleLayout->addLayout(fillRow);

    mainLayout->addWidget(grpStyle);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(tr("Start Drawing ➔"), this);
    m_btnOk->setDefault(true);
    m_btnOk->setStyleSheet(QStringLiteral("background-color: #0284C7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 6px;"));
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

PdfShapeType PdfShapeDialog::shapeType() const
{
    if (m_radioEllipse->isChecked()) return PdfShapeType::Ellipse;
    if (m_radioArrow->isChecked()) return PdfShapeType::Arrow;
    if (m_radioLine->isChecked()) return PdfShapeType::Line;
    if (m_radioFreehand->isChecked()) return PdfShapeType::Freehand;
    return PdfShapeType::Rectangle;
}

int PdfShapeDialog::strokeWidth() const
{
    return m_spinWidth->value();
}

QColor PdfShapeDialog::fillColor() const
{
    if (!m_chkEnableFill->isChecked()) {
        return Qt::transparent;
    }
    return m_fillColor;
}

void PdfShapeDialog::onStrokeColorClicked()
{
    QColor chosen = QColorDialog::getColor(m_strokeColor, this, tr("Select Stroke Color"));
    if (chosen.isValid()) {
        m_strokeColor = chosen;
        m_btnStrokeColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_strokeColor.name()));
    }
}

void PdfShapeDialog::onFillColorClicked()
{
    QColor chosen = QColorDialog::getColor(m_fillColor, this, tr("Select Fill Color"), QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        m_fillColor = chosen;
        m_btnFillColor->setStyleSheet(QString("background-color: %1; border: 1px solid #475569; border-radius: 4px;").arg(m_fillColor.name()));
        m_chkEnableFill->setChecked(true);
    }
}
