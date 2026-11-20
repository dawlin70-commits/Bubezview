/**
 * @file pdf_studio_panel.cpp
 * @brief Implementation of unified PDF editing studio side panel inheriting BaseStudioPanel.
 */

#include "pdf_studio_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QCheckBox>
#include <QToolButton>
#include <QPushButton>
#include <QColorDialog>
#include <QFontDatabase>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGroupBox>

// =============================================================================
// PdfStudioPanel Implementation
// =============================================================================

PdfStudioPanel::PdfStudioPanel(QWidget *parent)
    : BaseStudioPanel(parent)
{
    setHeaderTitle(tr("PDF Studio"));
    setHeaderIcon(VectorIcons::UiIcon::Palette, QColor(QStringLiteral("#38bdf8")));
    setCompareButtonVisible(false);
    setResetButtonVisible(false);
    setApplyButtonText(tr("Done"));

    setupPages();

    connect(this, &BaseStudioPanel::applyRequested, this, &BaseStudioPanel::closeRequested);
    connect(this, &BaseStudioPanel::cancelRequested, this, &BaseStudioPanel::closeRequested);
}

void PdfStudioPanel::setupPages()
{
    addTabPage(tr("Text"), createTextPage());
    addTabPage(tr("Shapes"), createShapesPage());
    addTabPage(tr("Stamps"), createWatermarkPage());
    addTabPage(tr("Sign"), createSignaturePage());
    addTabPage(tr("Redact"), createRedactionPage());
}

QWidget *PdfStudioPanel::createTextPage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    // 1. Font Family & Size
    QLabel *lblFont = new QLabel(tr("Typography"), container);
    lblFont->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblFont);

    QHBoxLayout *fontRow = new QHBoxLayout();
    fontRow->setSpacing(6);

    m_comboFontFamily = new QComboBox(container);
    m_comboFontFamily->addItem(QStringLiteral("Sans Serif"));
    m_comboFontFamily->addItem(QStringLiteral("Serif"));
    m_comboFontFamily->addItem(QStringLiteral("Monospace"));
    m_comboFontFamily->addItem(QStringLiteral("Arial"));
    m_comboFontFamily->addItem(QStringLiteral("Times New Roman"));
    m_comboFontFamily->addItem(QStringLiteral("Courier New"));
    m_comboFontFamily->addItem(QStringLiteral("Helvetica"));
    fontRow->addWidget(m_comboFontFamily, 1);

    m_spinFontSize = new QSpinBox(container);
    m_spinFontSize->setRange(6, 96);
    m_spinFontSize->setValue(12);
    m_spinFontSize->setSuffix(QStringLiteral(" pt"));
    m_spinFontSize->setFixedWidth(75);
    fontRow->addWidget(m_spinFontSize);

    layout->addLayout(fontRow);

    // 2. Style & Alignment
    QHBoxLayout *styleRow = new QHBoxLayout();
    styleRow->setSpacing(4);

    m_btnBold = new QToolButton(container);
    m_btnBold->setText(QStringLiteral("B"));
    m_btnBold->setCheckable(true);
    m_btnBold->setFixedSize(28, 26);
    m_btnBold->setStyleSheet(QStringLiteral("font-weight: 900;"));
    m_btnBold->setToolTip(tr("Bold (Ctrl+B)"));
    styleRow->addWidget(m_btnBold);

    m_btnItalic = new QToolButton(container);
    m_btnItalic->setText(QStringLiteral("I"));
    m_btnItalic->setCheckable(true);
    m_btnItalic->setFixedSize(28, 26);
    m_btnItalic->setStyleSheet(QStringLiteral("font-style: italic; font-weight: 700;"));
    m_btnItalic->setToolTip(tr("Italic (Ctrl+I)"));
    styleRow->addWidget(m_btnItalic);

    m_btnUnderline = new QToolButton(container);
    m_btnUnderline->setText(QStringLiteral("U"));
    m_btnUnderline->setCheckable(true);
    m_btnUnderline->setFixedSize(28, 26);
    m_btnUnderline->setStyleSheet(QStringLiteral("text-decoration: underline; font-weight: 700;"));
    m_btnUnderline->setToolTip(tr("Underline (Ctrl+U)"));
    styleRow->addWidget(m_btnUnderline);

    styleRow->addSpacing(10);

    m_btnAlignLeft = new QToolButton(container);
    m_btnAlignLeft->setText(QStringLiteral("⬅"));
    m_btnAlignLeft->setCheckable(true);
    m_btnAlignLeft->setChecked(true);
    m_btnAlignLeft->setFixedSize(28, 26);
    m_btnAlignLeft->setToolTip(tr("Align Left"));
    styleRow->addWidget(m_btnAlignLeft);

    m_btnAlignCenter = new QToolButton(container);
    m_btnAlignCenter->setText(QStringLiteral("⬌"));
    m_btnAlignCenter->setCheckable(true);
    m_btnAlignCenter->setFixedSize(28, 26);
    m_btnAlignCenter->setToolTip(tr("Align Center"));
    styleRow->addWidget(m_btnAlignCenter);

    m_btnAlignRight = new QToolButton(container);
    m_btnAlignRight->setText(QStringLiteral("➡"));
    m_btnAlignRight->setCheckable(true);
    m_btnAlignRight->setFixedSize(28, 26);
    m_btnAlignRight->setToolTip(tr("Align Right"));
    styleRow->addWidget(m_btnAlignRight);

    styleRow->addStretch();
    layout->addLayout(styleRow);

    // 3. Text Color Swatches
    QLabel *lblColor = new QLabel(tr("Text Color"), container);
    lblColor->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblColor);

    QHBoxLayout *colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);

    const QStringList textColors = {
        QStringLiteral("#000000"), QStringLiteral("#334155"), QStringLiteral("#2563EB"),
        QStringLiteral("#DC2626"), QStringLiteral("#16A34A"), QStringLiteral("#D97706"),
        QStringLiteral("#9333EA")
    };

    for (const QString &cHex : textColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("colorHex", cHex);
        connect(btn, &QToolButton::clicked, this, [this, cHex]() {
            m_textColor = QColor(cHex);
            updateTextColorButtons();
            emit textColorChanged(m_textColor);
        });
        colorRow->addWidget(btn);
        m_textColorButtons.append(btn);
    }

    m_btnTextCustomColor = new QToolButton(container);
    m_btnTextCustomColor->setText(QStringLiteral("🎨"));
    m_btnTextCustomColor->setFixedSize(24, 24);
    m_btnTextCustomColor->setCursor(Qt::PointingHandCursor);
    m_btnTextCustomColor->setToolTip(tr("Custom color..."));
    connect(m_btnTextCustomColor, &QToolButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_textColor, this, tr("Select Text Color"));
        if (chosen.isValid()) {
            m_textColor = chosen;
            updateTextColorButtons();
            emit textColorChanged(m_textColor);
        }
    });
    colorRow->addWidget(m_btnTextCustomColor);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    updateTextColorButtons();

    // 4. Background Whiteout
    m_chkWhiteout = new QCheckBox(tr("Cover / Erase underlying text (Whiteout)"), container);
    m_chkWhiteout->setToolTip(tr("Render an opaque white background under text to neatly cover existing content"));
    layout->addWidget(m_chkWhiteout);

    connect(m_chkWhiteout, &QCheckBox::toggled, this, &PdfStudioPanel::textWhiteoutChanged);

    // 5. Action Button & Instructions
    QPushButton *btnPlaceText = new QPushButton(tr("✍️ Place Text Box"), container);
    btnPlaceText->setCursor(Qt::PointingHandCursor);
    btnPlaceText->setFixedHeight(32);
    connect(btnPlaceText, &QPushButton::clicked, this, &PdfStudioPanel::placeTextRequested);
    layout->addWidget(btnPlaceText);

    QLabel *lblTip = new QLabel(tr("💡 <b>Tip</b>: Click anywhere on the PDF page to start typing directly. Drag the side handles to resize the text column width."), container);
    lblTip->setWordWrap(true);
    lblTip->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    layout->addWidget(lblTip);

    layout->addStretch();

    // Wire property changes
    auto emitFont = [this]() { emit textFontChanged(currentFont()); };
    connect(m_comboFontFamily, &QComboBox::currentTextChanged, this, emitFont);
    connect(m_spinFontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, emitFont);
    connect(m_btnBold, &QToolButton::toggled, this, emitFont);
    connect(m_btnItalic, &QToolButton::toggled, this, emitFont);
    connect(m_btnUnderline, &QToolButton::toggled, this, emitFont);

    connect(m_btnAlignLeft, &QToolButton::clicked, this, [this]() {
        m_btnAlignLeft->setChecked(true);
        m_btnAlignCenter->setChecked(false);
        m_btnAlignRight->setChecked(false);
        m_textAlignment = Qt::AlignLeft;
        emit textAlignmentChanged(m_textAlignment);
    });
    connect(m_btnAlignCenter, &QToolButton::clicked, this, [this]() {
        m_btnAlignLeft->setChecked(false);
        m_btnAlignCenter->setChecked(true);
        m_btnAlignRight->setChecked(false);
        m_textAlignment = Qt::AlignHCenter;
        emit textAlignmentChanged(m_textAlignment);
    });
    connect(m_btnAlignRight, &QToolButton::clicked, this, [this]() {
        m_btnAlignLeft->setChecked(false);
        m_btnAlignCenter->setChecked(false);
        m_btnAlignRight->setChecked(true);
        m_textAlignment = Qt::AlignRight;
        emit textAlignmentChanged(m_textAlignment);
    });

    return createStandardScrollArea(container, this);
}

void PdfStudioPanel::updateTextColorButtons()
{
    QString curHex = m_textColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_textColorButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        bool active = (hex == curHex);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border: %2 solid %3; border-radius: 12px; }"
        ).arg(hex, active ? QStringLiteral("2.5px") : QStringLiteral("1px"),
              active ? QStringLiteral("#38bdf8") : QStringLiteral("#64748b")));
    }
}

QFont PdfStudioPanel::currentFont() const
{
    QFont f(m_comboFontFamily ? m_comboFontFamily->currentText() : QStringLiteral("Sans Serif"));
    f.setPointSize(m_spinFontSize ? m_spinFontSize->value() : 12);
    f.setBold(m_btnBold && m_btnBold->isChecked());
    f.setItalic(m_btnItalic && m_btnItalic->isChecked());
    f.setUnderline(m_btnUnderline && m_btnUnderline->isChecked());
    return f;
}

bool PdfStudioPanel::isWhiteout() const
{
    return m_chkWhiteout && m_chkWhiteout->isChecked();
}

QWidget *PdfStudioPanel::createShapesPage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    // 1. Shape Type
    QLabel *lblType = new QLabel(tr("Shape Geometry"), container);
    lblType->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblType);

    m_comboShapeType = new QComboBox(container);
    m_comboShapeType->addItem(tr("Rectangle"), 0);
    m_comboShapeType->addItem(tr("Circle / Ellipse"), 1);
    m_comboShapeType->addItem(tr("Arrow"), 2);
    m_comboShapeType->addItem(tr("Straight Line"), 3);
    m_comboShapeType->addItem(tr("Rounded Rectangle"), 4);
    layout->addWidget(m_comboShapeType);

    // 2. Stroke Width
    QLabel *lblStroke = new QLabel(tr("Stroke Width"), container);
    lblStroke->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblStroke);

    QHBoxLayout *strokeRow = new QHBoxLayout();
    m_sliderShapeWidth = new QSlider(Qt::Horizontal, container);
    m_sliderShapeWidth->setRange(1, 24);
    m_sliderShapeWidth->setValue(2);
    strokeRow->addWidget(m_sliderShapeWidth, 1);

    m_lblShapeWidthVal = new QLabel(QStringLiteral("2 px"), container);
    m_lblShapeWidthVal->setFixedWidth(40);
    strokeRow->addWidget(m_lblShapeWidthVal);
    layout->addLayout(strokeRow);

    connect(m_sliderShapeWidth, &QSlider::valueChanged, this, [this](int val) {
        m_shapeStrokeWidth = val;
        if (m_lblShapeWidthVal) m_lblShapeWidthVal->setText(QStringLiteral("%1 px").arg(val));
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });

    // 3. Stroke Color
    QLabel *lblStrokeColor = new QLabel(tr("Border Color"), container);
    lblStrokeColor->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblStrokeColor);

    QHBoxLayout *scRow = new QHBoxLayout();
    scRow->setSpacing(6);

    const QStringList strokeColors = {
        QStringLiteral("#EF4444"), QStringLiteral("#2563EB"), QStringLiteral("#16A34A"),
        QStringLiteral("#D97706"), QStringLiteral("#000000"), QStringLiteral("#8B5CF6")
    };

    for (const QString &cHex : strokeColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("colorHex", cHex);
        connect(btn, &QToolButton::clicked, this, [this, cHex]() {
            m_shapeStrokeColor = QColor(cHex);
            updateShapeColorButtons();
            emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
        });
        scRow->addWidget(btn);
        m_shapeStrokeButtons.append(btn);
    }

    m_btnShapeCustomStroke = new QToolButton(container);
    m_btnShapeCustomStroke->setText(QStringLiteral("🎨"));
    m_btnShapeCustomStroke->setFixedSize(24, 24);
    m_btnShapeCustomStroke->setCursor(Qt::PointingHandCursor);
    connect(m_btnShapeCustomStroke, &QToolButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_shapeStrokeColor, this, tr("Select Border Color"));
        if (chosen.isValid()) {
            m_shapeStrokeColor = chosen;
            updateShapeColorButtons();
            emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
        }
    });
    scRow->addWidget(m_btnShapeCustomStroke);
    scRow->addStretch();
    layout->addLayout(scRow);

    // 4. Fill Color
    QLabel *lblFill = new QLabel(tr("Fill Interior"), container);
    lblFill->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblFill);

    m_chkShapeNoFill = new QCheckBox(tr("Transparent (No Fill)"), container);
    m_chkShapeNoFill->setChecked(true);
    layout->addWidget(m_chkShapeNoFill);

    QHBoxLayout *fcRow = new QHBoxLayout();
    fcRow->setSpacing(6);

    const QStringList fillColors = {
        QStringLiteral("#FCA5A5"), QStringLiteral("#93C5FD"), QStringLiteral("#86EFAC"),
        QStringLiteral("#FDE047"), QStringLiteral("#E2E8F0"), QStringLiteral("#FFFFFF")
    };

    for (const QString &cHex : fillColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("colorHex", cHex);
        connect(btn, &QToolButton::clicked, this, [this, cHex]() {
            if (m_chkShapeNoFill) m_chkShapeNoFill->setChecked(false);
            m_shapeFillColor = QColor(cHex);
            updateShapeColorButtons();
            emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
        });
        fcRow->addWidget(btn);
        m_shapeFillButtons.append(btn);
    }

    m_btnShapeCustomFill = new QToolButton(container);
    m_btnShapeCustomFill->setText(QStringLiteral("🎨"));
    m_btnShapeCustomFill->setFixedSize(24, 24);
    m_btnShapeCustomFill->setCursor(Qt::PointingHandCursor);
    connect(m_btnShapeCustomFill, &QToolButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_shapeFillColor.isValid() ? m_shapeFillColor : Qt::white, this, tr("Select Fill Color"));
        if (chosen.isValid()) {
            if (m_chkShapeNoFill) m_chkShapeNoFill->setChecked(false);
            m_shapeFillColor = chosen;
            updateShapeColorButtons();
            emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
        }
    });
    fcRow->addWidget(m_btnShapeCustomFill);
    fcRow->addStretch();
    layout->addLayout(fcRow);

    connect(m_chkShapeNoFill, &QCheckBox::toggled, this, [this](bool noFill) {
        m_shapeFillColor = noFill ? Qt::transparent : QColor(QStringLiteral("#FCA5A5"));
        updateShapeColorButtons();
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });

    updateShapeColorButtons();

    // 5. Action Button & Instructions
    QPushButton *btnDrawShape = new QPushButton(tr("📐 Draw Shape on Page"), container);
    btnDrawShape->setCursor(Qt::PointingHandCursor);
    btnDrawShape->setFixedHeight(32);
    connect(btnDrawShape, &QPushButton::clicked, this, &PdfStudioPanel::drawShapeRequested);
    layout->addWidget(btnDrawShape);

    QLabel *lblTip = new QLabel(tr("💡 <b>Tip</b>: Drag anywhere on the page to create the shape. It remains movable and resizable with handles until you click <b>Apply</b> or press Enter."), container);
    lblTip->setWordWrap(true);
    lblTip->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    layout->addWidget(lblTip);

    layout->addStretch();

    connect(m_comboShapeType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_shapeType = m_comboShapeType->itemData(idx).toInt();
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });

    return createStandardScrollArea(container, this);
}

void PdfStudioPanel::updateShapeColorButtons()
{
    QString curStroke = m_shapeStrokeColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_shapeStrokeButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        bool active = (hex == curStroke);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border: %2 solid %3; border-radius: 12px; }"
        ).arg(hex, active ? QStringLiteral("2.5px") : QStringLiteral("1px"),
              active ? QStringLiteral("#38bdf8") : QStringLiteral("#64748b")));
    }

    bool transparent = (!m_shapeFillColor.isValid() || m_shapeFillColor.alpha() == 0);
    QString curFill = m_shapeFillColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_shapeFillButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        bool active = (!transparent && hex == curFill);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border: %2 solid %3; border-radius: 12px; }"
        ).arg(hex, active ? QStringLiteral("2.5px") : QStringLiteral("1px"),
              active ? QStringLiteral("#38bdf8") : QStringLiteral("#64748b")));
    }
}

QWidget *PdfStudioPanel::createWatermarkPage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    // 1. Presets
    QLabel *lblPresets = new QLabel(tr("Preset Stamps"), container);
    lblPresets->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblPresets);

    QGridLayout *presetsGrid = new QGridLayout();
    presetsGrid->setSpacing(6);

    struct PresetStamp {
        const char *text;
        const char *color;
    };
    const PresetStamp presets[] = {
        {"APPROVED", "#16A34A"}, {"DRAFT", "#64748B"}, {"CONFIDENTIAL", "#DC2626"},
        {"PAID", "#2563EB"},     {"COPY", "#D97706"},  {"REJECTED", "#B91C1C"}
    };

    for (int i = 0; i < 6; ++i) {
        QPushButton *pBtn = new QPushButton(QLatin1String(presets[i].text), container);
        pBtn->setFixedHeight(28);
        pBtn->setCursor(Qt::PointingHandCursor);
        QString c = QLatin1String(presets[i].color);
        pBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; border: 1.5px solid %1; color: %1; font-weight: 700; border-radius: 6px; font-size: 11px; }"
            "QPushButton:hover { background-color: %1; color: #ffffff; }"
        ).arg(c));
        connect(pBtn, &QPushButton::clicked, this, [this, i, presets]() {
            if (m_editWatermarkText) m_editWatermarkText->setText(QLatin1String(presets[i].text));
            m_watermarkColor = QColor(QLatin1String(presets[i].color));
            updateWatermarkColorButtons();
            emit watermarkParamsChanged(watermarkText(), watermarkAngle(), watermarkOpacity(), m_watermarkColor, watermarkStyle());
        });
        presetsGrid->addWidget(pBtn, i / 2, i % 2);
    }
    layout->addLayout(presetsGrid);

    // 2. Custom Text
    QLabel *lblText = new QLabel(tr("Stamp Text"), container);
    lblText->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblText);

    m_editWatermarkText = new QLineEdit(QStringLiteral("CONFIDENTIAL"), container);
    layout->addWidget(m_editWatermarkText);

    // 3. Graphic Style
    QLabel *lblStyle = new QLabel(tr("Stamp Graphic Style"), container);
    lblStyle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblStyle);

    m_comboWatermarkStyle = new QComboBox(container);
    m_comboWatermarkStyle->addItem(tr("Classic Rounded Box"), 0);
    m_comboWatermarkStyle->addItem(tr("Official Double Border"), 1);
    m_comboWatermarkStyle->addItem(tr("Modern Dotted Border"), 2);
    m_comboWatermarkStyle->addItem(tr("Plain Text Only"), 3);
    layout->addWidget(m_comboWatermarkStyle);

    // 4. Rotation Angle
    QLabel *lblAngle = new QLabel(tr("Rotation Angle"), container);
    lblAngle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblAngle);

    QHBoxLayout *angleRow = new QHBoxLayout();
    m_sliderWatermarkAngle = new QSlider(Qt::Horizontal, container);
    m_sliderWatermarkAngle->setRange(-90, 90);
    m_sliderWatermarkAngle->setValue(45);
    angleRow->addWidget(m_sliderWatermarkAngle, 1);

    m_lblWatermarkAngleVal = new QLabel(QStringLiteral("45°"), container);
    m_lblWatermarkAngleVal->setFixedWidth(35);
    angleRow->addWidget(m_lblWatermarkAngleVal);
    layout->addLayout(angleRow);

    QHBoxLayout *snapRow = new QHBoxLayout();
    snapRow->setSpacing(6);
    QPushButton *btnSnapM45 = new QPushButton(QStringLiteral("-45°"), container);
    QPushButton *btnSnap0 = new QPushButton(QStringLiteral("0° (Horizontal)"), container);
    QPushButton *btnSnapP45 = new QPushButton(QStringLiteral("+45°"), container);
    connect(btnSnapM45, &QPushButton::clicked, this, [this]() { if (m_sliderWatermarkAngle) m_sliderWatermarkAngle->setValue(-45); });
    connect(btnSnap0, &QPushButton::clicked, this, [this]() { if (m_sliderWatermarkAngle) m_sliderWatermarkAngle->setValue(0); });
    connect(btnSnapP45, &QPushButton::clicked, this, [this]() { if (m_sliderWatermarkAngle) m_sliderWatermarkAngle->setValue(45); });
    snapRow->addWidget(btnSnapM45);
    snapRow->addWidget(btnSnap0);
    snapRow->addWidget(btnSnapP45);
    layout->addLayout(snapRow);

    // 5. Opacity
    QLabel *lblOpacity = new QLabel(tr("Opacity"), container);
    lblOpacity->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblOpacity);

    QHBoxLayout *opRow = new QHBoxLayout();
    m_sliderWatermarkOpacity = new QSlider(Qt::Horizontal, container);
    m_sliderWatermarkOpacity->setRange(10, 100);
    m_sliderWatermarkOpacity->setValue(40);
    opRow->addWidget(m_sliderWatermarkOpacity, 1);

    m_lblWatermarkOpacityVal = new QLabel(QStringLiteral("40%"), container);
    m_lblWatermarkOpacityVal->setFixedWidth(35);
    opRow->addWidget(m_lblWatermarkOpacityVal);
    layout->addLayout(opRow);

    // 6. Color
    QLabel *lblColor = new QLabel(tr("Stamp Color"), container);
    lblColor->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblColor);

    QHBoxLayout *cRow = new QHBoxLayout();
    cRow->setSpacing(6);

    const QStringList stampColors = {
        QStringLiteral("#DC2626"), QStringLiteral("#2563EB"), QStringLiteral("#16A34A"),
        QStringLiteral("#D97706"), QStringLiteral("#64748B"), QStringLiteral("#000000")
    };

    for (const QString &cHex : stampColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("colorHex", cHex);
        connect(btn, &QToolButton::clicked, this, [this, cHex]() {
            m_watermarkColor = QColor(cHex);
            updateWatermarkColorButtons();
            emit watermarkParamsChanged(watermarkText(), watermarkAngle(), watermarkOpacity(), m_watermarkColor, watermarkStyle());
        });
        cRow->addWidget(btn);
        m_watermarkColorButtons.append(btn);
    }

    m_btnWatermarkCustomColor = new QToolButton(container);
    m_btnWatermarkCustomColor->setText(QStringLiteral("🎨"));
    m_btnWatermarkCustomColor->setFixedSize(24, 24);
    m_btnWatermarkCustomColor->setCursor(Qt::PointingHandCursor);
    connect(m_btnWatermarkCustomColor, &QToolButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_watermarkColor, this, tr("Select Stamp Color"));
        if (chosen.isValid()) {
            m_watermarkColor = chosen;
            updateWatermarkColorButtons();
            emit watermarkParamsChanged(watermarkText(), watermarkAngle(), watermarkOpacity(), m_watermarkColor, watermarkStyle());
        }
    });
    cRow->addWidget(m_btnWatermarkCustomColor);
    cRow->addStretch();
    layout->addLayout(cRow);

    updateWatermarkColorButtons();

    // 7. Action Button
    QPushButton *btnPlace = new QPushButton(tr("💧 Place Stamp on Page"), container);
    btnPlace->setCursor(Qt::PointingHandCursor);
    btnPlace->setFixedHeight(32);
    connect(btnPlace, &QPushButton::clicked, this, &PdfStudioPanel::placeWatermarkRequested);
    layout->addWidget(btnPlace);

    layout->addStretch();

    auto emitWatermark = [this]() {
        emit watermarkParamsChanged(watermarkText(), watermarkAngle(), watermarkOpacity(), m_watermarkColor, watermarkStyle());
    };
    connect(m_editWatermarkText, &QLineEdit::textChanged, this, emitWatermark);
    connect(m_comboWatermarkStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, emitWatermark);
    connect(m_sliderWatermarkAngle, &QSlider::valueChanged, this, [this, emitWatermark](int val) {
        if (m_lblWatermarkAngleVal) m_lblWatermarkAngleVal->setText(QStringLiteral("%1°").arg(val));
        emitWatermark();
    });
    connect(m_sliderWatermarkOpacity, &QSlider::valueChanged, this, [this, emitWatermark](int val) {
        if (m_lblWatermarkOpacityVal) m_lblWatermarkOpacityVal->setText(QStringLiteral("%1%").arg(val));
        emitWatermark();
    });

    return createStandardScrollArea(container, this);
}

void PdfStudioPanel::updateWatermarkColorButtons()
{
    QString curHex = m_watermarkColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_watermarkColorButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        bool active = (hex == curHex);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border: %2 solid %3; border-radius: 12px; }"
        ).arg(hex, active ? QStringLiteral("2.5px") : QStringLiteral("1px"),
              active ? QStringLiteral("#38bdf8") : QStringLiteral("#64748b")));
    }
}

QString PdfStudioPanel::watermarkText() const
{
    return m_editWatermarkText ? m_editWatermarkText->text().trimmed() : QStringLiteral("CONFIDENTIAL");
}

qreal PdfStudioPanel::watermarkAngle() const
{
    return m_sliderWatermarkAngle ? static_cast<qreal>(m_sliderWatermarkAngle->value()) : 45.0;
}

qreal PdfStudioPanel::watermarkOpacity() const
{
    return m_sliderWatermarkOpacity ? static_cast<qreal>(m_sliderWatermarkOpacity->value()) / 100.0 : 0.40;
}

int PdfStudioPanel::watermarkStyle() const
{
    return m_comboWatermarkStyle ? m_comboWatermarkStyle->currentData().toInt() : 0;
}

QWidget *PdfStudioPanel::createSignaturePage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    // 1. Drawing pad header
    QLabel *lblPad = new QLabel(tr("Handwritten Signature Pad"), container);
    lblPad->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblPad);

    m_sigCanvas = new SignatureCanvasWidget(container);
    layout->addWidget(m_sigCanvas);

    // 2. Pad controls: Pen width, Colors & Clear
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(6);

    auto *btnClear = new QPushButton(tr("🗑️ Clear"), container);
    btnClear->setFixedHeight(26);
    btnClear->setCursor(Qt::PointingHandCursor);
    connect(btnClear, &QPushButton::clicked, m_sigCanvas, &SignatureCanvasWidget::clearCanvas);
    ctrlRow->addWidget(btnClear);

    auto *btnLargePad = new QPushButton(tr("⛶ Large Canvas..."), container);
    btnLargePad->setFixedHeight(26);
    btnLargePad->setCursor(Qt::PointingHandCursor);
    btnLargePad->setToolTip(tr("Open comfortable full-size signature dialog"));
    connect(btnLargePad, &QPushButton::clicked, this, &PdfStudioPanel::openLargeSignaturePadRequested);
    ctrlRow->addWidget(btnLargePad);

    ctrlRow->addSpacing(8);

    const QStringList sigColors = { QStringLiteral("#000000"), QStringLiteral("#1E40AF"), QStringLiteral("#991B1B") };
    for (const QString &cHex : sigColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(22, 22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("background-color: %1; border-radius: 11px; border: 1px solid #94a3b8;").arg(cHex));
        connect(btn, &QToolButton::clicked, this, [this, cHex]() {
            if (m_sigCanvas) m_sigCanvas->setStrokeColor(QColor(cHex));
        });
        ctrlRow->addWidget(btn);
    }

    ctrlRow->addStretch();
    layout->addLayout(ctrlRow);

    // 3. Insert Signature Action
    QPushButton *btnPlaceSig = new QPushButton(tr("✍️ Place Signature on Page"), container);
    btnPlaceSig->setCursor(Qt::PointingHandCursor);
    btnPlaceSig->setFixedHeight(32);
    connect(btnPlaceSig, &QPushButton::clicked, this, [this]() {
        if (m_sigCanvas) {
            QImage img = m_sigCanvas->renderSignatureImage();
            if (!img.isNull()) {
                emit placeSignatureRequested(img);
            }
        }
    });
    layout->addWidget(btnPlaceSig);

    layout->addSpacing(10);

    // 4. Image / Logo insertion
    QFrame *line = new QFrame(container);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    QLabel *lblImg = new QLabel(tr("Graphic or Logo"), container);
    lblImg->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblImg);

    QPushButton *btnInsertImg = new QPushButton(tr("🖼️ Insert Image / Logo..."), container);
    btnInsertImg->setCursor(Qt::PointingHandCursor);
    btnInsertImg->setFixedHeight(32);
    connect(btnInsertImg, &QPushButton::clicked, this, &PdfStudioPanel::insertImageRequested);
    layout->addWidget(btnInsertImg);

    layout->addSpacing(10);

    // 5. Digital Cryptographic Signature (X.509 / P12 / Smart Card)
    QFrame *line2 = new QFrame(container);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line2);

    QLabel *lblCrypto = new QLabel(tr("Digital Signature (Cryptographic)"), container);
    lblCrypto->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblCrypto);

    QPushButton *btnSignCrypto = new QPushButton(tr("🔏 Sign Document with Certificate (P12)..."), container);
    btnSignCrypto->setCursor(Qt::PointingHandCursor);
    btnSignCrypto->setFixedHeight(32);
    connect(btnSignCrypto, &QPushButton::clicked, this, &PdfStudioPanel::cryptoSignatureRequested);
    layout->addWidget(btnSignCrypto);

    QPushButton *btnVerifyCrypto = new QPushButton(tr("🔍 Verify Document Signatures..."), container);
    btnVerifyCrypto->setCursor(Qt::PointingHandCursor);
    btnVerifyCrypto->setFixedHeight(32);
    connect(btnVerifyCrypto, &QPushButton::clicked, this, &PdfStudioPanel::verifyCryptoSignaturesRequested);
    layout->addWidget(btnVerifyCrypto);

    layout->addStretch();
    return createStandardScrollArea(container, this);
}

QWidget *PdfStudioPanel::createRedactionPage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    QLabel *lblTitle = new QLabel(tr("Privacy Redaction"), container);
    lblTitle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblTitle);

    QLabel *lblNotice = new QLabel(tr("⚠️ Redaction permanently obscures sensitive information. Underlying text and graphics are burned and unrecoverable once applied."), container);
    lblNotice->setWordWrap(true);
    lblNotice->setStyleSheet(QStringLiteral("color: #ef4444; font-size: 11px; background-color: rgba(239, 68, 68, 0.1); border: 1px solid rgba(239, 68, 68, 0.3); border-radius: 6px; padding: 8px;"));
    layout->addWidget(lblNotice);

    QLabel *lblColor = new QLabel(tr("Redaction Color"), container);
    lblColor->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblColor);

    QHBoxLayout *rRow = new QHBoxLayout();
    rRow->setSpacing(8);

    const struct RedactColorOpt { const char *hex; const char *name; } redactColors[] = {
        {"#000000", "Blackout"},
        {"#FFFFFF", "Whiteout"},
        {"#808080", "Gray"}
    };

    for (const auto &opt : redactColors) {
        auto *btn = new QToolButton(container);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(QLatin1String(opt.name));
        btn->setProperty("colorHex", QLatin1String(opt.hex));
        connect(btn, &QToolButton::clicked, this, [this, opt]() {
            m_redactionColor = QColor(QLatin1String(opt.hex));
            updateRedactionColorButtons();
            emit redactionColorChanged(m_redactionColor);
        });
        rRow->addWidget(btn);
        m_redactColorButtons.append(btn);
    }

    m_btnRedactCustomColor = new QToolButton(container);
    m_btnRedactCustomColor->setText(QStringLiteral("🎨"));
    m_btnRedactCustomColor->setFixedSize(28, 28);
    m_btnRedactCustomColor->setCursor(Qt::PointingHandCursor);
    m_btnRedactCustomColor->setToolTip(tr("Custom Redaction Color..."));
    connect(m_btnRedactCustomColor, &QToolButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_redactionColor, this, tr("Select Redaction Color"));
        if (chosen.isValid()) {
            m_redactionColor = chosen;
            updateRedactionColorButtons();
            emit redactionColorChanged(m_redactionColor);
        }
    });
    rRow->addWidget(m_btnRedactCustomColor);
    rRow->addStretch();
    layout->addLayout(rRow);

    updateRedactionColorButtons();

    QPushButton *btnStartRedact = new QPushButton(tr("⬛ Redact Area on Page"), container);
    btnStartRedact->setCursor(Qt::PointingHandCursor);
    btnStartRedact->setFixedHeight(32);
    connect(btnStartRedact, &QPushButton::clicked, this, &PdfStudioPanel::startRedactionRequested);
    layout->addWidget(btnStartRedact);

    layout->addStretch();
    return createStandardScrollArea(container, this);
}

void PdfStudioPanel::updateRedactionColorButtons()
{
    QString curHex = m_redactionColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_redactColorButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        bool active = (hex == curHex);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border: %2 solid %3; border-radius: 14px; }"
        ).arg(hex, active ? QStringLiteral("2.5px") : QStringLiteral("1px"),
              active ? QStringLiteral("#38bdf8") : QStringLiteral("#64748b")));
    }
}

QWidget *PdfStudioPanel::createPagesPage()
{
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    // 1. Current Page Status
    m_lblPageStatus = new QLabel(tr("Page 1 of 1"), container);
    m_lblPageStatus->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13px;"));
    layout->addWidget(m_lblPageStatus);

    QHBoxLayout *navRow = new QHBoxLayout();
    QLabel *lblJump = new QLabel(tr("Go to page:"), container);
    navRow->addWidget(lblJump);

    m_spinCurrentPage = new QSpinBox(container);
    m_spinCurrentPage->setRange(1, 1);
    m_spinCurrentPage->setValue(1);
    navRow->addWidget(m_spinCurrentPage);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_spinCurrentPage, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit pageSelected(val - 1);
    });

    // 2. Rotate Actions
    QLabel *lblRotate = new QLabel(tr("Rotate Page"), container);
    lblRotate->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblRotate);

    QHBoxLayout *rotRow = new QHBoxLayout();
    QPushButton *btnRotCW = new QPushButton(tr("🔄 Rotate 90° CW"), container);
    btnRotCW->setCursor(Qt::PointingHandCursor);
    connect(btnRotCW, &QPushButton::clicked, this, [this]() { emit rotatePageRequested(90); });
    rotRow->addWidget(btnRotCW);

    QPushButton *btnRotCCW = new QPushButton(tr("↺ Rotate 90° CCW"), container);
    btnRotCCW->setCursor(Qt::PointingHandCursor);
    connect(btnRotCCW, &QPushButton::clicked, this, [this]() { emit rotatePageRequested(270); });
    rotRow->addWidget(btnRotCCW);
    layout->addLayout(rotRow);

    // 3. Page Modifications
    QLabel *lblOps = new QLabel(tr("Page Structure"), container);
    lblOps->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px;"));
    layout->addWidget(lblOps);

    QPushButton *btnOrganizer = new QPushButton(tr("📑 Page Organizer..."), container);
    btnOrganizer->setCursor(Qt::PointingHandCursor);
    btnOrganizer->setToolTip(tr("Reorder, rotate, delete, or extract multiple pages interactively"));
    connect(btnOrganizer, &QPushButton::clicked, this, &PdfStudioPanel::openPageOrganizerRequested);
    layout->addWidget(btnOrganizer);

    QPushButton *btnInsertBlank = new QPushButton(tr("➕ Insert Blank Page After"), container);
    btnInsertBlank->setCursor(Qt::PointingHandCursor);
    connect(btnInsertBlank, &QPushButton::clicked, this, [this]() {
        emit insertBlankPageRequested(m_currentPage);
    });
    layout->addWidget(btnInsertBlank);

    QPushButton *btnDeletePage = new QPushButton(tr("🗑️ Delete Current Page"), container);
    btnDeletePage->setCursor(Qt::PointingHandCursor);
    btnDeletePage->setStyleSheet(QStringLiteral("color: #ef4444;"));
    connect(btnDeletePage, &QPushButton::clicked, this, [this]() {
        emit deletePageRequested(m_currentPage);
    });
    layout->addWidget(btnDeletePage);

    // 4. Export Page
    QLabel *lblExport = new QLabel(tr("Export Current Page"), container);
    lblExport->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11px; margin-top: 6px;"));
    layout->addWidget(lblExport);

    QHBoxLayout *exportRow = new QHBoxLayout();
    QPushButton *btnSavePagePdf = new QPushButton(tr("📄 As PDF"), container);
    btnSavePagePdf->setCursor(Qt::PointingHandCursor);
    btnSavePagePdf->setToolTip(tr("Save current page as an independent single-page PDF document"));
    connect(btnSavePagePdf, &QPushButton::clicked, this, &PdfStudioPanel::saveCurrentPageAsPdfRequested);
    exportRow->addWidget(btnSavePagePdf);

    QPushButton *btnSavePageImg = new QPushButton(tr("🖼️ As Image"), container);
    btnSavePageImg->setCursor(Qt::PointingHandCursor);
    btnSavePageImg->setToolTip(tr("Save current page as a high-resolution raster image (PNG/JPEG/WebP)"));
    connect(btnSavePageImg, &QPushButton::clicked, this, &PdfStudioPanel::saveCurrentPageAsImageRequested);
    exportRow->addWidget(btnSavePageImg);
    layout->addLayout(exportRow);

    layout->addStretch();
    return createStandardScrollArea(container, this);
}

void PdfStudioPanel::updatePageInfo(int currentPage, int totalPages)
{
    m_currentPage = currentPage;
    m_totalPages = totalPages;

    if (m_lblPageStatus) {
        m_lblPageStatus->setText(tr("Page %1 of %2").arg(currentPage + 1).arg(totalPages));
    }
    if (m_spinCurrentPage) {
        QSignalBlocker blocker(m_spinCurrentPage);
        m_spinCurrentPage->setRange(1, std::max(1, totalPages));
        m_spinCurrentPage->setValue(currentPage + 1);
    }
}

void PdfStudioPanel::customizeTheme(QString &qss, bool isDark)
{
    Q_UNUSED(qss);
    Q_UNUSED(isDark);
    updateTextColorButtons();
    updateShapeColorButtons();
    updateWatermarkColorButtons();
    updateRedactionColorButtons();
}
