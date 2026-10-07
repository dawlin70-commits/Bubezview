/**
 * @file pdf_tools_dialog.cpp
 * @brief Implementation of the unified multi-tab PDF markup tools dialog with continuous mode and graphic libraries.
 */

#include "pdf_tools_dialog.h"
#include "pdf_signature_dialog.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QListWidget>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QApplication>
#include <QIcon>
#include <QFrame>

PdfToolsDialog::PdfToolsDialog(int defaultTab, QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_selectedAction(PdfToolAction::None)
    , m_highlighterColor(QColor(255, 235, 59))
    , m_highlighterOpacity(0.40)
    , m_sliderHlOpacity(nullptr)
    , m_lblHlOpacityVal(nullptr)
    , m_hlPreviewBox(nullptr)
    , m_btnHlCustomColor(nullptr)
    , m_radioRect(nullptr)
    , m_radioEllipse(nullptr)
    , m_radioLine(nullptr)
    , m_radioArrow(nullptr)
    , m_spinShapeWidth(nullptr)
    , m_btnShapeStrokeColor(nullptr)
    , m_shapeStrokeColor(QColor(239, 68, 68))
    , m_chkShapeFill(nullptr)
    , m_btnShapeFillColor(nullptr)
    , m_shapeFillColor(QColor(239, 68, 68, 40))
    , m_radioRedactBlack(nullptr)
    , m_radioRedactWhite(nullptr)
    , m_comboWkPresets(nullptr)
    , m_txtWkCustom(nullptr)
    , m_comboWkAngle(nullptr)
    , m_sliderWkOpacity(nullptr)
    , m_lblWkOpacityVal(nullptr)
    , m_btnWkColor(nullptr)
    , m_watermarkColor(QColor(220, 38, 38))
    , m_imageSubTabs(nullptr)
    , m_btnBrowseImage(nullptr)
    , m_lblImagePath(nullptr)
    , m_lblImagePreview(nullptr)
    , m_chkSaveImageToLibrary(nullptr)
    , m_listSavedImages(nullptr)
    , m_btnDeleteSavedImage(nullptr)
    , m_btnApplyImage(nullptr)
    , m_sigSubTabs(nullptr)
    , m_sigCanvas(nullptr)
    , m_btnSigColorBlack(nullptr)
    , m_btnSigColorBlue(nullptr)
    , m_sliderSigWidth(nullptr)
    , m_btnClearSigCanvas(nullptr)
    , m_lblSigScanPreview(nullptr)
    , m_btnBrowseSigScan(nullptr)
    , m_chkAutoRemoveWhite(nullptr)
    , m_sliderSigThreshold(nullptr)
    , m_listSavedSigs(nullptr)
    , m_btnDeleteSavedSig(nullptr)
    , m_chkSaveSigToLibrary(nullptr)
{
    setWindowTitle(tr("PDF Markup & Annotation Tools"));
    setMinimumSize(580, 520);
    resize(600, 560);

    setupUi();
    loadSavedSignatures();
    loadSavedImages();
    setActiveTab(defaultTab);
}

PdfToolsDialog::~PdfToolsDialog() = default;

QString PdfToolsDialog::signaturesDirPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/bubezview");
    }
    QString dirPath = base + QStringLiteral("/signatures");
    QDir().mkpath(dirPath);
    return dirPath;
}

QString PdfToolsDialog::savedImagesDirPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/bubezview");
    }
    QString dirPath = base + QStringLiteral("/saved_images");
    QDir().mkpath(dirPath);
    return dirPath;
}

void PdfToolsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);

    m_tabWidget->addTab(createHighlighterTab(), tr("🖍️ Highlighter"));
    m_tabWidget->addTab(createShapesTab(), tr("📐 Shapes & Arrows"));
    m_tabWidget->addTab(createRedactTab(), tr("⬛ Redaction"));
    m_tabWidget->addTab(createWatermarkTab(), tr("🔖 Watermark"));
    m_tabWidget->addTab(createImageTab(), tr("🖼️ Images & Emblems"));
    m_tabWidget->addTab(createSignatureTab(), tr("✍️ Signature"));

    mainLayout->addWidget(m_tabWidget, 1);

    // Bottom close / cancel button
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    auto *btnClose = new QPushButton(tr("Close"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnClose);

    mainLayout->addLayout(bottomLayout);
}

void PdfToolsDialog::setActiveTab(int tabIndex)
{
    if (m_tabWidget && tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(tabIndex);
    }
}

// =============================================================================
// TAB 1: HIGHLIGHTER
// =============================================================================

QWidget *PdfToolsDialog::createHighlighterTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(14);

    auto *lblDesc = new QLabel(tr("Drag a rectangular box over any text or graphical area to apply a vibrant, semi-transparent highlight. Highlights can be undone at any time with Ctrl+Z."), tab);
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("color: #888888; font-size: 12px;");
    layout->addWidget(lblDesc);

    auto *grpColors = new QGroupBox(tr("Highlight Color"), tab);
    auto *colorLayout = new QVBoxLayout(grpColors);

    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(8);

    struct Preset {
        const char *name;
        const char *hex;
    };
    const Preset presets[] = {
        {"Yellow", "#FFFF00"},
        {"Lime", "#4ADE80"},
        {"Cyan", "#38BDF8"},
        {"Pink", "#F472B6"},
        {"Orange", "#FB923C"},
        {"Purple", "#C084FC"}
    };

    for (const auto &p : presets) {
        auto *btn = new QPushButton(QString::fromUtf8(p.name), grpColors);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("background-color: %1; color: #000000; font-weight: bold; border: 1px solid #777777; border-radius: 4px; padding: 6px 10px;").arg(p.hex));
        btn->setProperty("colorHex", QString(p.hex));
        connect(btn, &QPushButton::clicked, this, &PdfToolsDialog::onHighlighterColorPresetClicked);
        presetRow->addWidget(btn);
    }

    m_btnHlCustomColor = new QPushButton(tr("Custom Color..."), grpColors);
    m_btnHlCustomColor->setCursor(Qt::PointingHandCursor);
    connect(m_btnHlCustomColor, &QPushButton::clicked, this, &PdfToolsDialog::onHighlighterCustomColorClicked);
    presetRow->addWidget(m_btnHlCustomColor);
    presetRow->addStretch();

    colorLayout->addLayout(presetRow);
    layout->addWidget(grpColors);

    auto *grpOpacity = new QGroupBox(tr("Transparency / Opacity"), tab);
    auto *opacityLayout = new QHBoxLayout(grpOpacity);

    m_sliderHlOpacity = new QSlider(Qt::Horizontal, grpOpacity);
    m_sliderHlOpacity->setRange(10, 90);
    m_sliderHlOpacity->setValue(40);
    m_lblHlOpacityVal = new QLabel("40%", grpOpacity);
    m_lblHlOpacityVal->setFixedWidth(40);

    connect(m_sliderHlOpacity, &QSlider::valueChanged, this, &PdfToolsDialog::onHighlighterOpacityChanged);

    opacityLayout->addWidget(m_sliderHlOpacity, 1);
    opacityLayout->addWidget(m_lblHlOpacityVal);
    layout->addWidget(grpOpacity);

    auto *grpPreview = new QGroupBox(tr("Live Preview"), tab);
    auto *previewLayout = new QVBoxLayout(grpPreview);
    m_hlPreviewBox = new QFrame(grpPreview);
    m_hlPreviewBox->setFrameShape(QFrame::StyledPanel);
    m_hlPreviewBox->setMinimumHeight(44);
    auto *boxLayout = new QHBoxLayout(m_hlPreviewBox);
    auto *lblSample = new QLabel(tr("Sample highlighted text demonstrating transparency over content."), m_hlPreviewBox);
    lblSample->setAlignment(Qt::AlignCenter);
    lblSample->setStyleSheet("font-size: 13px; font-weight: bold; color: #111111;");
    boxLayout->addWidget(lblSample);
    previewLayout->addWidget(m_hlPreviewBox);
    layout->addWidget(grpPreview);

    updateHighlighterPreview();
    layout->addStretch();

    auto *btnStart = new QPushButton(tr("🖍️ Start Highlighting on Document"), tab);
    btnStart->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #0284c7; color: white; border-radius: 4px;");
    btnStart->setCursor(Qt::PointingHandCursor);
    connect(btnStart, &QPushButton::clicked, this, &PdfToolsDialog::onApplyHighlighter);
    layout->addWidget(btnStart);

    return tab;
}

void PdfToolsDialog::onHighlighterColorPresetClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (btn) {
        QString hex = btn->property("colorHex").toString();
        if (!hex.isEmpty()) {
            m_highlighterColor = QColor(hex);
            updateHighlighterPreview();
        }
    }
}

void PdfToolsDialog::onHighlighterCustomColorClicked()
{
    QColor c = QColorDialog::getColor(m_highlighterColor, this, tr("Select Highlight Color"));
    if (c.isValid()) {
        m_highlighterColor = c;
        updateHighlighterPreview();
    }
}

void PdfToolsDialog::onHighlighterOpacityChanged(int val)
{
    m_highlighterOpacity = static_cast<double>(val) / 100.0;
    if (m_lblHlOpacityVal) {
        m_lblHlOpacityVal->setText(QString("%1%").arg(val));
    }
    updateHighlighterPreview();
}

void PdfToolsDialog::updateHighlighterPreview()
{
    if (!m_hlPreviewBox) return;
    QColor c = m_highlighterColor;
    c.setAlphaF(m_highlighterOpacity);
    QString rgba = QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(m_highlighterOpacity);
    m_hlPreviewBox->setStyleSheet(QString("background-color: %1; border: 1px dashed #999999; border-radius: 4px;").arg(rgba));
}

void PdfToolsDialog::onApplyHighlighter()
{
    m_selectedAction = PdfToolAction::Highlighter;
    accept();
}

// =============================================================================
// TAB 2: SHAPES & ARROWS
// =============================================================================

QWidget *PdfToolsDialog::createShapesTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(14);

    auto *grpType = new QGroupBox(tr("Shape Type"), tab);
    auto *typeLayout = new QHBoxLayout(grpType);
    m_radioRect = new QRadioButton(tr("Rectangle"), grpType);
    m_radioEllipse = new QRadioButton(tr("Ellipse"), grpType);
    m_radioLine = new QRadioButton(tr("Line"), grpType);
    m_radioArrow = new QRadioButton(tr("Arrow"), grpType);
    m_radioRect->setChecked(true);

    typeLayout->addWidget(m_radioRect);
    typeLayout->addWidget(m_radioEllipse);
    typeLayout->addWidget(m_radioLine);
    typeLayout->addWidget(m_radioArrow);
    layout->addWidget(grpType);

    auto *grpStyle = new QGroupBox(tr("Outline & Stroke"), tab);
    auto *styleLayout = new QGridLayout(grpStyle);

    styleLayout->addWidget(new QLabel(tr("Stroke Width:"), grpStyle), 0, 0);
    m_spinShapeWidth = new QSpinBox(grpStyle);
    m_spinShapeWidth->setRange(1, 20);
    m_spinShapeWidth->setValue(3);
    m_spinShapeWidth->setSuffix(" px");
    styleLayout->addWidget(m_spinShapeWidth, 0, 1);

    styleLayout->addWidget(new QLabel(tr("Stroke Color:"), grpStyle), 1, 0);
    m_btnShapeStrokeColor = new QPushButton(grpStyle);
    m_btnShapeStrokeColor->setCursor(Qt::PointingHandCursor);
    connect(m_btnShapeStrokeColor, &QPushButton::clicked, this, &PdfToolsDialog::onShapeStrokeColorClicked);
    styleLayout->addWidget(m_btnShapeStrokeColor, 1, 1);

    layout->addWidget(grpStyle);

    auto *grpFill = new QGroupBox(tr("Fill"), tab);
    auto *fillLayout = new QHBoxLayout(grpFill);
    m_chkShapeFill = new QCheckBox(tr("Enable Interior Fill"), grpFill);
    m_btnShapeFillColor = new QPushButton(grpFill);
    m_btnShapeFillColor->setCursor(Qt::PointingHandCursor);
    connect(m_btnShapeFillColor, &QPushButton::clicked, this, &PdfToolsDialog::onShapeFillColorClicked);

    fillLayout->addWidget(m_chkShapeFill);
    fillLayout->addWidget(m_btnShapeFillColor);
    fillLayout->addStretch();
    layout->addWidget(grpFill);

    updateShapeColorsUi();
    layout->addStretch();

    auto *btnStart = new QPushButton(tr("📐 Start Drawing Shape on Document"), tab);
    btnStart->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #0284c7; color: white; border-radius: 4px;");
    btnStart->setCursor(Qt::PointingHandCursor);
    connect(btnStart, &QPushButton::clicked, this, &PdfToolsDialog::onApplyShape);
    layout->addWidget(btnStart);

    return tab;
}

void PdfToolsDialog::updateShapeColorsUi()
{
    if (m_btnShapeStrokeColor) {
        m_btnShapeStrokeColor->setStyleSheet(QString("background-color: %1; border: 1px solid #777; border-radius: 3px; min-width: 80px; min-height: 22px;").arg(m_shapeStrokeColor.name()));
    }
    if (m_btnShapeFillColor) {
        m_btnShapeFillColor->setStyleSheet(QString("background-color: %1; border: 1px solid #777; border-radius: 3px; min-width: 80px; min-height: 22px;").arg(m_shapeFillColor.name()));
    }
}

void PdfToolsDialog::onShapeStrokeColorClicked()
{
    QColor c = QColorDialog::getColor(m_shapeStrokeColor, this, tr("Select Stroke Color"));
    if (c.isValid()) {
        m_shapeStrokeColor = c;
        updateShapeColorsUi();
    }
}

void PdfToolsDialog::onShapeFillColorClicked()
{
    QColor c = QColorDialog::getColor(m_shapeFillColor, this, tr("Select Fill Color"));
    if (c.isValid()) {
        m_shapeFillColor = c;
        updateShapeColorsUi();
    }
}

PdfShapeType PdfToolsDialog::shapeType() const
{
    if (m_radioEllipse && m_radioEllipse->isChecked()) return PdfShapeType::Ellipse;
    if (m_radioLine && m_radioLine->isChecked()) return PdfShapeType::Line;
    if (m_radioArrow && m_radioArrow->isChecked()) return PdfShapeType::Arrow;
    return PdfShapeType::Rectangle;
}

int PdfToolsDialog::shapeStrokeWidth() const
{
    return m_spinShapeWidth ? m_spinShapeWidth->value() : 3;
}

QColor PdfToolsDialog::shapeFillColor() const
{
    if (m_chkShapeFill && m_chkShapeFill->isChecked()) {
        return m_shapeFillColor;
    }
    return QColor(0, 0, 0, 0);
}

void PdfToolsDialog::onApplyShape()
{
    m_selectedAction = PdfToolAction::Shape;
    accept();
}

// =============================================================================
// TAB 3: REDACTION
// =============================================================================

QWidget *PdfToolsDialog::createRedactTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(14);

    auto *lblDesc = new QLabel(tr("Redaction permanently covers sensitive or confidential sections of the page. Once applied, the information is completely obscured."), tab);
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("color: #888888; font-size: 12px;");
    layout->addWidget(lblDesc);

    auto *grpMode = new QGroupBox(tr("Redaction Overlay Style"), tab);
    auto *modeLayout = new QVBoxLayout(grpMode);

    m_radioRedactBlack = new QRadioButton(tr("Solid Blackout (Standard confidential blackout box)"), grpMode);
    m_radioRedactWhite = new QRadioButton(tr("Clean Whiteout (White background eraser)"), grpMode);
    m_radioRedactBlack->setChecked(true);

    modeLayout->addWidget(m_radioRedactBlack);
    modeLayout->addWidget(m_radioRedactWhite);
    layout->addWidget(grpMode);

    auto *lblTip = new QLabel(tr("Tip: Drag the rectangle over the target area. You can resize and adjust it with the bounding handles, then click 'Apply' to commit."), tab);
    lblTip->setWordWrap(true);
    lblTip->setStyleSheet("font-style: italic; color: #666666; font-size: 12px;");
    layout->addWidget(lblTip);

    layout->addStretch();

    auto *btnStart = new QPushButton(tr("⬛ Start Redacting Area"), tab);
    btnStart->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #dc2626; color: white; border-radius: 4px;");
    btnStart->setCursor(Qt::PointingHandCursor);
    connect(btnStart, &QPushButton::clicked, this, &PdfToolsDialog::onApplyRedaction);
    layout->addWidget(btnStart);

    return tab;
}

bool PdfToolsDialog::isRedactionWhiteout() const
{
    return m_radioRedactWhite && m_radioRedactWhite->isChecked();
}

void PdfToolsDialog::onApplyRedaction()
{
    m_selectedAction = PdfToolAction::Redact;
    accept();
}

// =============================================================================
// TAB 4: WATERMARK & STAMPS
// =============================================================================

QWidget *PdfToolsDialog::createWatermarkTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(14);

    auto *grpText = new QGroupBox(tr("Stamp Text"), tab);
    auto *textLayout = new QGridLayout(grpText);

    textLayout->addWidget(new QLabel(tr("Preset:"), grpText), 0, 0);
    m_comboWkPresets = new QComboBox(grpText);
    m_comboWkPresets->addItem(tr("CONFIDENZIALE"));
    m_comboWkPresets->addItem(tr("COPIA"));
    m_comboWkPresets->addItem(tr("BOZZA"));
    m_comboWkPresets->addItem(tr("APPROVATO"));
    m_comboWkPresets->addItem(tr("PAGATO"));
    m_comboWkPresets->addItem(tr("URGENTE"));
    m_comboWkPresets->addItem(tr("CUSTOM..."));
    textLayout->addWidget(m_comboWkPresets, 0, 1);

    textLayout->addWidget(new QLabel(tr("Text:"), grpText), 1, 0);
    m_txtWkCustom = new QLineEdit("CONFIDENZIALE", grpText);
    textLayout->addWidget(m_txtWkCustom, 1, 1);

    connect(m_comboWkPresets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PdfToolsDialog::onWatermarkPresetChanged);
    layout->addWidget(grpText);

    auto *grpProps = new QGroupBox(tr("Appearance"), tab);
    auto *propsLayout = new QGridLayout(grpProps);

    propsLayout->addWidget(new QLabel(tr("Angle:"), grpProps), 0, 0);
    m_comboWkAngle = new QComboBox(grpProps);
    m_comboWkAngle->addItem(tr("45° (Diagonal Up)"), 45.0);
    m_comboWkAngle->addItem(tr("0° (Horizontal)"), 0.0);
    m_comboWkAngle->addItem(tr("-45° (Diagonal Down)"), -45.0);
    m_comboWkAngle->addItem(tr("90° (Vertical)"), 90.0);
    propsLayout->addWidget(m_comboWkAngle, 0, 1);

    propsLayout->addWidget(new QLabel(tr("Opacity:"), grpProps), 1, 0);
    m_sliderWkOpacity = new QSlider(Qt::Horizontal, grpProps);
    m_sliderWkOpacity->setRange(5, 100);
    m_sliderWkOpacity->setValue(25);
    m_lblWkOpacityVal = new QLabel("25%", grpProps);
    m_lblWkOpacityVal->setFixedWidth(35);
    connect(m_sliderWkOpacity, &QSlider::valueChanged, this, [this](int v) {
        if (m_lblWkOpacityVal) m_lblWkOpacityVal->setText(QString("%1%").arg(v));
    });

    auto *opRow = new QHBoxLayout();
    opRow->addWidget(m_sliderWkOpacity, 1);
    opRow->addWidget(m_lblWkOpacityVal);
    propsLayout->addLayout(opRow, 1, 1);

    propsLayout->addWidget(new QLabel(tr("Color:"), grpProps), 2, 0);
    m_btnWkColor = new QPushButton(grpProps);
    m_btnWkColor->setStyleSheet(QString("background-color: %1; border: 1px solid #777; border-radius: 3px; min-width: 80px; min-height: 22px;").arg(m_watermarkColor.name()));
    m_btnWkColor->setCursor(Qt::PointingHandCursor);
    connect(m_btnWkColor, &QPushButton::clicked, this, &PdfToolsDialog::onWatermarkColorClicked);
    propsLayout->addWidget(m_btnWkColor, 2, 1);

    layout->addWidget(grpProps);
    layout->addStretch();

    auto *btnStart = new QPushButton(tr("🔖 Apply Watermark"), tab);
    btnStart->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #0284c7; color: white; border-radius: 4px;");
    btnStart->setCursor(Qt::PointingHandCursor);
    connect(btnStart, &QPushButton::clicked, this, &PdfToolsDialog::onApplyWatermark);
    layout->addWidget(btnStart);

    return tab;
}

void PdfToolsDialog::onWatermarkPresetChanged(int index)
{
    if (m_comboWkPresets && m_txtWkCustom) {
        if (index < m_comboWkPresets->count() - 1) {
            m_txtWkCustom->setText(m_comboWkPresets->currentText());
        } else {
            m_txtWkCustom->setFocus();
        }
    }
}

void PdfToolsDialog::onWatermarkColorClicked()
{
    QColor c = QColorDialog::getColor(m_watermarkColor, this, tr("Select Watermark Color"));
    if (c.isValid()) {
        m_watermarkColor = c;
        if (m_btnWkColor) {
            m_btnWkColor->setStyleSheet(QString("background-color: %1; border: 1px solid #777; border-radius: 3px; min-width: 80px; min-height: 22px;").arg(m_watermarkColor.name()));
        }
    }
}

QString PdfToolsDialog::watermarkText() const
{
    return m_txtWkCustom ? m_txtWkCustom->text() : "CONFIDENZIALE";
}

qreal PdfToolsDialog::watermarkAngle() const
{
    return m_comboWkAngle ? m_comboWkAngle->currentData().toReal() : 45.0;
}

qreal PdfToolsDialog::watermarkOpacity() const
{
    return m_sliderWkOpacity ? (static_cast<qreal>(m_sliderWkOpacity->value()) / 100.0) : 0.25;
}

void PdfToolsDialog::onApplyWatermark()
{
    m_selectedAction = PdfToolAction::Watermark;
    accept();
}

// =============================================================================
// TAB 5: INSERT IMAGE & EMBLEMS GALLERY
// =============================================================================

QWidget *PdfToolsDialog::createImageTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(12);

    m_imageSubTabs = new QTabWidget(tab);

    // Subtab 1: From File
    auto *fileTab = new QWidget(m_imageSubTabs);
    auto *fileLayout = new QVBoxLayout(fileTab);
    fileLayout->setContentsMargins(8, 8, 8, 8);
    fileLayout->setSpacing(10);

    auto *browseRow = new QHBoxLayout();
    m_btnBrowseImage = new QPushButton(tr("📁 Browse Image File..."), fileTab);
    m_btnBrowseImage->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowseImage, &QPushButton::clicked, this, &PdfToolsDialog::onBrowseImageClicked);
    browseRow->addWidget(m_btnBrowseImage);

    m_lblImagePath = new QLabel(tr("No image selected"), fileTab);
    m_lblImagePath->setStyleSheet("color: #777777; font-style: italic;");
    browseRow->addWidget(m_lblImagePath, 1);
    fileLayout->addLayout(browseRow);

    auto *grpPreview = new QGroupBox(tr("Image Preview"), fileTab);
    auto *prevLayout = new QVBoxLayout(grpPreview);
    m_lblImagePreview = new QLabel(grpPreview);
    m_lblImagePreview->setAlignment(Qt::AlignCenter);
    m_lblImagePreview->setMinimumHeight(140);
    m_lblImagePreview->setStyleSheet("border: 1px dashed #777; border-radius: 4px; background: rgba(0,0,0,0.05);");
    prevLayout->addWidget(m_lblImagePreview);
    fileLayout->addWidget(grpPreview, 1);

    m_chkSaveImageToLibrary = new QCheckBox(tr("⭐ Save this emblem / logo to gallery for 1-click reuse"), fileTab);
    m_chkSaveImageToLibrary->setChecked(true);
    fileLayout->addWidget(m_chkSaveImageToLibrary);

    m_imageSubTabs->addTab(fileTab, tr("📂 From File / Scan"));

    // Subtab 2: Saved Emblems & Logos Gallery
    auto *savedTab = new QWidget(m_imageSubTabs);
    auto *savedLayout = new QVBoxLayout(savedTab);
    savedLayout->setContentsMargins(8, 8, 8, 8);
    savedLayout->setSpacing(10);

    m_listSavedImages = new QListWidget(savedTab);
    m_listSavedImages->setViewMode(QListView::IconMode);
    m_listSavedImages->setIconSize(QSize(96, 96));
    m_listSavedImages->setGridSize(QSize(120, 120));
    m_listSavedImages->setSpacing(8);
    m_listSavedImages->setStyleSheet(
        "QListWidget { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 8px; }"
        "QListWidget::item { background-color: #1E293B; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QListWidget::item:selected { border: 2px solid #38BDF8; background-color: #2D3D58; }"
    );
    connect(m_listSavedImages, &QListWidget::itemSelectionChanged, this, &PdfToolsDialog::onSavedImageSelectionChanged);
    savedLayout->addWidget(m_listSavedImages, 1);

    auto *savedActRow = new QHBoxLayout();
    m_btnDeleteSavedImage = new QPushButton(tr("🗑️ Delete Selected"), savedTab);
    m_btnDeleteSavedImage->setEnabled(false);
    connect(m_btnDeleteSavedImage, &QPushButton::clicked, this, &PdfToolsDialog::onDeleteSavedImageClicked);
    savedActRow->addWidget(m_btnDeleteSavedImage);
    savedActRow->addStretch();
    savedLayout->addLayout(savedActRow);

    m_imageSubTabs->addTab(savedTab, tr("⭐ Saved Emblems & Logos"));

    layout->addWidget(m_imageSubTabs, 1);

    m_btnApplyImage = new QPushButton(tr("🖼️ Insert Image on Page"), tab);
    m_btnApplyImage->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #0284c7; color: white; border-radius: 4px;");
    m_btnApplyImage->setCursor(Qt::PointingHandCursor);
    m_btnApplyImage->setEnabled(false);
    connect(m_btnApplyImage, &QPushButton::clicked, this, &PdfToolsDialog::onApplyImage);
    layout->addWidget(m_btnApplyImage);

    return tab;
}

void PdfToolsDialog::onBrowseImageClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Select Image or Emblem to Insert"), QString(),
                                                    tr("Image Files (*.png *.jpg *.jpeg *.bmp *.svg *.webp);;All Files (*)"));
    if (!filePath.isEmpty()) {
        QImage img(filePath);
        if (!img.isNull()) {
            m_imageFromFile = img;
            if (m_lblImagePath) m_lblImagePath->setText(QFileInfo(filePath).fileName());
            if (m_lblImagePreview) {
                QPixmap pm = QPixmap::fromImage(img).scaled(240, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                m_lblImagePreview->setPixmap(pm);
            }
            if (m_btnApplyImage) m_btnApplyImage->setEnabled(true);
        }
    }
}

void PdfToolsDialog::loadSavedImages()
{
    if (!m_listSavedImages) return;
    m_listSavedImages->clear();
    QDir dir(savedImagesDirPath());
    QStringList filters;
    filters << QStringLiteral("*.png") << QStringLiteral("*.jpg") << QStringLiteral("*.webp");
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    for (const auto &fi : files) {
        QPixmap pix(fi.absoluteFilePath());
        if (!pix.isNull()) {
            QIcon icon(pix);
            auto item = new QListWidgetItem(icon, fi.baseName(), m_listSavedImages);
            item->setData(Qt::UserRole, fi.absoluteFilePath());
        }
    }
}

void PdfToolsDialog::onSavedImageSelectionChanged()
{
    if (m_listSavedImages && m_btnDeleteSavedImage) {
        bool hasSel = !m_listSavedImages->selectedItems().isEmpty();
        m_btnDeleteSavedImage->setEnabled(hasSel);
        if (m_btnApplyImage && hasSel) {
            m_btnApplyImage->setEnabled(true);
        }
    }
}

void PdfToolsDialog::onDeleteSavedImageClicked()
{
    if (!m_listSavedImages) return;
    auto items = m_listSavedImages->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(Qt::UserRole).toString();
    if (QFile::remove(path)) {
        delete items.first();
    }
}

QImage PdfToolsDialog::imageToInsert() const
{
    if (m_imageSubTabs && m_imageSubTabs->currentIndex() == 1) {
        // From saved gallery
        if (m_listSavedImages && !m_listSavedImages->selectedItems().isEmpty()) {
            QString path = m_listSavedImages->selectedItems().first()->data(Qt::UserRole).toString();
            QImage img(path);
            if (!img.isNull()) return img;
        }
    }
    return m_imageFromFile;
}

void PdfToolsDialog::onApplyImage()
{
    QImage img = imageToInsert();
    if (img.isNull()) {
        QMessageBox::warning(this, tr("No Image Selected"), tr("Please select an image file or choose a saved emblem from the gallery."));
        return;
    }

    // Save to gallery if checked on the file tab
    if (m_imageSubTabs && m_imageSubTabs->currentIndex() == 0 && m_chkSaveImageToLibrary && m_chkSaveImageToLibrary->isChecked()) {
        QString filename = QStringLiteral("emblem_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
        QString savePath = savedImagesDirPath() + QStringLiteral("/") + filename;
        img.save(savePath, "PNG");
    }

    m_selectedAction = PdfToolAction::InsertImage;
    accept();
}

// =============================================================================
// TAB 6: SIGNATURE STUDIO (RESTORED COMPLETE LIBRARY & DIGITAL ROADMAP)
// =============================================================================

QWidget *PdfToolsDialog::createSignatureTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(10);

    m_sigSubTabs = new QTabWidget(tab);

    // 1. Draw Freehand Tab
    auto *drawTab = new QWidget(m_sigSubTabs);
    auto *drawLayout = new QVBoxLayout(drawTab);
    drawLayout->setContentsMargins(8, 8, 8, 8);
    drawLayout->setSpacing(8);

    m_sigCanvas = new SignatureCanvasWidget(drawTab);
    m_sigCanvas->setMinimumHeight(140);
    drawLayout->addWidget(m_sigCanvas, 1);

    auto *drawCtrlRow = new QHBoxLayout();
    drawCtrlRow->addWidget(new QLabel(tr("Ink:"), drawTab));

    m_btnSigColorBlack = new QPushButton(drawTab);
    m_btnSigColorBlack->setFixedSize(22, 22);
    m_btnSigColorBlack->setStyleSheet("background-color: #000000; border-radius: 11px; border: 2px solid #64748B;");
    connect(m_btnSigColorBlack, &QPushButton::clicked, this, [this]() { onColorChanged(Qt::black); });
    drawCtrlRow->addWidget(m_btnSigColorBlack);

    m_btnSigColorBlue = new QPushButton(drawTab);
    m_btnSigColorBlue->setFixedSize(22, 22);
    m_btnSigColorBlue->setStyleSheet("background-color: #1D4ED8; border-radius: 11px; border: 2px solid #64748B;");
    connect(m_btnSigColorBlue, &QPushButton::clicked, this, [this]() { onColorChanged(QColor("#1D4ED8")); });
    drawCtrlRow->addWidget(m_btnSigColorBlue);

    drawCtrlRow->addWidget(new QLabel(tr("Stroke:"), drawTab));
    m_sliderSigWidth = new QSlider(Qt::Horizontal, drawTab);
    m_sliderSigWidth->setRange(1, 8);
    m_sliderSigWidth->setValue(3);
    m_sliderSigWidth->setFixedWidth(90);
    connect(m_sliderSigWidth, &QSlider::valueChanged, m_sigCanvas, &SignatureCanvasWidget::setStrokeWidth);
    drawCtrlRow->addWidget(m_sliderSigWidth);

    drawCtrlRow->addStretch();
    m_btnClearSigCanvas = new QPushButton(tr("🗑️ Clear"), drawTab);
    connect(m_btnClearSigCanvas, &QPushButton::clicked, this, [this]() {
        if (m_sigCanvas) m_sigCanvas->clearCanvas();
    });
    drawCtrlRow->addWidget(m_btnClearSigCanvas);
    drawLayout->addLayout(drawCtrlRow);

    m_sigSubTabs->addTab(drawTab, tr("✍️ Draw Freehand"));

    // 2. Scan / Import Photo Tab
    auto *scanTab = new QWidget(m_sigSubTabs);
    auto *scanLayout = new QVBoxLayout(scanTab);
    scanLayout->setContentsMargins(8, 8, 8, 8);
    scanLayout->setSpacing(8);

    m_lblSigScanPreview = new QLabel(scanTab);
    m_lblSigScanPreview->setMinimumHeight(140);
    m_lblSigScanPreview->setAlignment(Qt::AlignCenter);
    m_lblSigScanPreview->setStyleSheet("background-color: #0F172A; border: 2px dashed #334155; border-radius: 8px; color: #94A3B8;");
    m_lblSigScanPreview->setText(tr("No image loaded.<br>Click 'Browse Photo / Scan' below."));
    scanLayout->addWidget(m_lblSigScanPreview, 1);

    auto *scanBtnRow = new QHBoxLayout();
    m_btnBrowseSigScan = new QPushButton(tr("📂 Browse Photo / Scan..."), scanTab);
    m_btnBrowseSigScan->setStyleSheet("font-weight: 600; padding: 4px 10px;");
    connect(m_btnBrowseSigScan, &QPushButton::clicked, this, &PdfToolsDialog::onBrowseSignatureImageClicked);
    scanBtnRow->addWidget(m_btnBrowseSigScan);
    scanBtnRow->addStretch();
    scanLayout->addLayout(scanBtnRow);

    m_chkAutoRemoveWhite = new QCheckBox(tr("✨ Automatic paper background removal (White -> Transparent)"), scanTab);
    m_chkAutoRemoveWhite->setChecked(true);
    connect(m_chkAutoRemoveWhite, &QCheckBox::toggled, this, &PdfToolsDialog::processImportedSignatureImage);
    scanLayout->addWidget(m_chkAutoRemoveWhite);

    auto *threshRow = new QHBoxLayout();
    threshRow->addWidget(new QLabel(tr("Background Sensitivity:"), scanTab));
    m_sliderSigThreshold = new QSlider(Qt::Horizontal, scanTab);
    m_sliderSigThreshold->setRange(160, 255);
    m_sliderSigThreshold->setValue(225);
    connect(m_sliderSigThreshold, &QSlider::valueChanged, this, &PdfToolsDialog::onThresholdSliderChanged);
    threshRow->addWidget(m_sliderSigThreshold, 1);
    scanLayout->addLayout(threshRow);

    m_sigSubTabs->addTab(scanTab, tr("📷 Photo / Scan Import"));

    // 3. Saved Signatures Library Tab
    auto *savedSigTab = new QWidget(m_sigSubTabs);
    auto *savedSigLayout = new QVBoxLayout(savedSigTab);
    savedSigLayout->setContentsMargins(8, 8, 8, 8);
    savedSigLayout->setSpacing(8);

    m_listSavedSigs = new QListWidget(savedSigTab);
    m_listSavedSigs->setViewMode(QListView::IconMode);
    m_listSavedSigs->setIconSize(QSize(130, 65));
    m_listSavedSigs->setGridSize(QSize(150, 90));
    m_listSavedSigs->setSpacing(8);
    m_listSavedSigs->setStyleSheet(
        "QListWidget { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 8px; }"
        "QListWidget::item { background-color: #1E293B; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QListWidget::item:selected { border: 2px solid #38BDF8; background-color: #2D3D58; }"
    );
    connect(m_listSavedSigs, &QListWidget::itemSelectionChanged, this, &PdfToolsDialog::onSavedSigSelectionChanged);
    savedSigLayout->addWidget(m_listSavedSigs, 1);

    auto *sigActRow = new QHBoxLayout();
    m_btnDeleteSavedSig = new QPushButton(tr("🗑️ Delete Selected"), savedSigTab);
    m_btnDeleteSavedSig->setEnabled(false);
    connect(m_btnDeleteSavedSig, &QPushButton::clicked, this, &PdfToolsDialog::onDeleteSavedSigClicked);
    sigActRow->addWidget(m_btnDeleteSavedSig);
    sigActRow->addStretch();
    savedSigLayout->addLayout(sigActRow);

    m_sigSubTabs->addTab(savedSigTab, tr("📁 Saved Signatures"));

    // 4. Digital / Electronic Certificate (Roadmap)
    auto *digitalTab = new QWidget(m_sigSubTabs);
    auto *digLayout = new QVBoxLayout(digitalTab);
    digLayout->setContentsMargins(12, 12, 12, 12);
    digLayout->setSpacing(12);

    auto *lblDigInfo = new QLabel(tr("<b>Electronic & Digital Signature Hub (PAdES / X.509)</b><br><br>"
                                     "Support for legal digital signatures using:<br>"
                                     "• Smart Card / Hardware Token (CIE, CNS, USB crypto tokens)<br>"
                                     "• PKCS#12 Certificate files (.p12, .pfx)<br>"
                                     "• Timestamp Authority (TSA) synchronization<br><br>"
                                     "All signature operations will be centralized in this panel."), digitalTab);
    lblDigInfo->setWordWrap(true);
    lblDigInfo->setStyleSheet("background: rgba(0, 120, 215, 0.08); border: 1px solid rgba(0, 120, 215, 0.3); border-radius: 6px; padding: 12px;");
    digLayout->addWidget(lblDigInfo);

    auto *btnCertSoon = new QPushButton(tr("Digital Certificate Manager (Under Development)"), digitalTab);
    btnCertSoon->setEnabled(false);
    digLayout->addWidget(btnCertSoon);
    digLayout->addStretch();

    m_sigSubTabs->addTab(digitalTab, tr("🔒 Digital / Electronic Certificate"));

    layout->addWidget(m_sigSubTabs, 1);

    m_chkSaveSigToLibrary = new QCheckBox(tr("💾 Save signature to library for 1-click future reuse"), tab);
    m_chkSaveSigToLibrary->setChecked(true);
    layout->addWidget(m_chkSaveSigToLibrary);

    auto *btnApply = new QPushButton(tr("✍️ Place Signature on Document"), tab);
    btnApply->setStyleSheet("font-weight: bold; font-size: 13px; padding: 8px 16px; background-color: #0284c7; color: white; border-radius: 4px;");
    btnApply->setCursor(Qt::PointingHandCursor);
    connect(btnApply, &QPushButton::clicked, this, &PdfToolsDialog::onApplySignature);
    layout->addWidget(btnApply);

    return tab;
}

void PdfToolsDialog::onColorChanged(const QColor &color)
{
    if (m_sigCanvas) {
        m_sigCanvas->setStrokeColor(color);
    }
}

void PdfToolsDialog::onBrowseSignatureImageClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Signature Image / Scan"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff);;All Files (*)")
    );
    if (!path.isEmpty()) {
        m_rawSigImportImage.load(path);
        processImportedSignatureImage();
    }
}

void PdfToolsDialog::onThresholdSliderChanged(int /*value*/)
{
    processImportedSignatureImage();
}

void PdfToolsDialog::processImportedSignatureImage()
{
    if (m_rawSigImportImage.isNull() || !m_lblSigScanPreview || !m_sliderSigThreshold || !m_chkAutoRemoveWhite) {
        return;
    }

    QImage src = m_rawSigImportImage.convertToFormat(QImage::Format_ARGB32);
    if (!m_chkAutoRemoveWhite->isChecked()) {
        m_processedSigImportImage = src;
    } else {
        int threshold = m_sliderSigThreshold->value();
        m_processedSigImportImage = QImage(src.size(), QImage::Format_ARGB32);

        for (int y = 0; y < src.height(); ++y) {
            const QRgb *srcRow = reinterpret_cast<const QRgb*>(src.constScanLine(y));
            QRgb *dstRow = reinterpret_cast<QRgb*>(m_processedSigImportImage.scanLine(y));
            for (int x = 0; x < src.width(); ++x) {
                QRgb pixel = srcRow[x];
                int r = qRed(pixel);
                int g = qGreen(pixel);
                int b = qBlue(pixel);
                int luminance = (r * 299 + g * 587 + b * 114) / 1000;

                if (luminance >= threshold) {
                    dstRow[x] = qRgba(r, g, b, 0); // Transparent
                } else if (luminance >= threshold - 30) {
                    double alphaFactor = 1.0 - (static_cast<double>(luminance - (threshold - 30)) / 30.0);
                    int alpha = static_cast<int>(qAlpha(pixel) * alphaFactor);
                    dstRow[x] = qRgba(r, g, b, alpha);
                } else {
                    dstRow[x] = pixel;
                }
            }
        }
    }

    QSize targetSize = m_lblSigScanPreview->size() - QSize(20, 20);
    if (targetSize.width() < 10 || targetSize.height() < 10) {
        targetSize = QSize(260, 130);
    }
    QPixmap previewPix = QPixmap::fromImage(m_processedSigImportImage)
        .scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_lblSigScanPreview->setPixmap(previewPix);
}

void PdfToolsDialog::loadSavedSignatures()
{
    if (!m_listSavedSigs) return;
    m_listSavedSigs->clear();
    QDir dir(signaturesDirPath());
    QStringList filters;
    filters << QStringLiteral("*.png");
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    for (const auto &fi : files) {
        QPixmap pix(fi.absoluteFilePath());
        if (!pix.isNull()) {
            QIcon icon(pix);
            auto item = new QListWidgetItem(icon, fi.baseName(), m_listSavedSigs);
            item->setData(Qt::UserRole, fi.absoluteFilePath());
        }
    }
}

void PdfToolsDialog::onSavedSigSelectionChanged()
{
    if (m_listSavedSigs && m_btnDeleteSavedSig) {
        m_btnDeleteSavedSig->setEnabled(!m_listSavedSigs->selectedItems().isEmpty());
    }
}

void PdfToolsDialog::onDeleteSavedSigClicked()
{
    if (!m_listSavedSigs) return;
    auto items = m_listSavedSigs->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(Qt::UserRole).toString();
    if (QFile::remove(path)) {
        delete items.first();
    }
}

QImage PdfToolsDialog::signatureImage() const
{
    return m_finalSignatureImage;
}

void PdfToolsDialog::onApplySignature()
{
    int currentTab = m_sigSubTabs ? m_sigSubTabs->currentIndex() : 0;
    QImage result;

    if (currentTab == 0) {
        // Draw tab
        if (!m_sigCanvas || m_sigCanvas->isEmpty()) {
            QMessageBox::warning(this, tr("No Signature"), tr("Please draw your signature on the canvas first."));
            return;
        }
        result = m_sigCanvas->renderSignatureImage();
    } else if (currentTab == 1) {
        // Scan/Import tab
        if (m_processedSigImportImage.isNull()) {
            QMessageBox::warning(this, tr("No Image"), tr("Please select a signature image file first."));
            return;
        }
        result = m_processedSigImportImage;
    } else if (currentTab == 2) {
        // Saved signatures tab
        if (!m_listSavedSigs || m_listSavedSigs->selectedItems().isEmpty()) {
            QMessageBox::warning(this, tr("Select Signature"), tr("Please select a saved signature from the list."));
            return;
        }
        QString path = m_listSavedSigs->selectedItems().first()->data(Qt::UserRole).toString();
        result.load(path);
    }

    if (result.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to generate signature image."));
        return;
    }

    m_finalSignatureImage = result;

    // Save to user's signature library if requested
    if (currentTab != 2 && m_chkSaveSigToLibrary && m_chkSaveSigToLibrary->isChecked()) {
        QString filename = QStringLiteral("sig_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
        QString savePath = signaturesDirPath() + QStringLiteral("/") + filename;
        m_finalSignatureImage.save(savePath, "PNG");
    }

    m_selectedAction = PdfToolAction::Signature;
    accept();
}
