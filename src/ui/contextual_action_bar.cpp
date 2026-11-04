/**
 * @file contextual_action_bar.cpp
 * @brief Implementation of ContextualActionBar.
 */

#include "contextual_action_bar.h"
#include "vector_icons.h"
#include <QPainter>
#include <QColorDialog>
#include <QMenu>
#include <QWidgetAction>
#include <QFontDatabase>
#include <QFontInfo>
#include <QLineEdit>

ContextualActionBar::ContextualActionBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ContextualActionBar"));
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFixedHeight(0);
    hide();
    setupUi();
    applyThemePreset(ThemePreset::DarkSlate);
}

QSize ContextualActionBar::minimumSizeHint() const
{
    return QSize(320, height());
}

void ContextualActionBar::setupUi()
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 2, 12, 2);
    rootLayout->setSpacing(8);

    m_pagesStack = new QStackedWidget(this);

    setupTextPage();
    setupHighlightPage();
    setupShapesPage();
    setupRedactPage();
    setupStampPage();

    m_pagesStack->addWidget(m_textPage);
    m_pagesStack->addWidget(m_highlightPage);
    m_pagesStack->addWidget(m_shapesPage);
    m_pagesStack->addWidget(m_redactPage);
    m_pagesStack->addWidget(m_stampPage);

    rootLayout->addWidget(m_pagesStack);
}

// -----------------------------------------------------------------------------
// 1. TEXT PAGE
// -----------------------------------------------------------------------------
void ContextualActionBar::setupTextPage()
{
    m_textPage = new QWidget(m_pagesStack);
    auto *lay = new QHBoxLayout(m_textPage);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *lblFont = new QLabel(tr("Font:"), m_textPage);
    lay->addWidget(lblFont, 0, Qt::AlignVCenter);

    m_cmbFontFamily = new QComboBox(m_textPage);
    m_cmbFontFamily->setFixedHeight(26);
    m_cmbFontFamily->setMinimumWidth(130);
    m_cmbFontFamily->setMaximumWidth(170);

    const QStringList popularFamilies = {
        QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans"),
        QStringLiteral("Liberation Sans"), QStringLiteral("Ubuntu"),
        QStringLiteral("Arial"), QStringLiteral("Times New Roman"),
        QStringLiteral("DejaVu Serif"), QStringLiteral("Courier New"),
        QStringLiteral("Monospace")
    };
    for (const auto &fam : popularFamilies) {
        m_cmbFontFamily->addItem(fam);
    }
    m_cmbFontFamily->setToolTip(tr("Font Family"));
    connect(m_cmbFontFamily, &QComboBox::currentIndexChanged, this, [this]() {
        emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
    });
    lay->addWidget(m_cmbFontFamily, 0, Qt::AlignVCenter);

    m_cmbFontSize = new QComboBox(m_textPage);
    m_cmbFontSize->setFixedHeight(26);
    m_cmbFontSize->setFixedWidth(64);
    m_cmbFontSize->setEditable(true);
    const QList<int> sizes = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64};
    for (int s : sizes) {
        m_cmbFontSize->addItem(QString::number(s));
    }
    m_cmbFontSize->setCurrentText(QStringLiteral("12"));
    m_cmbFontSize->setToolTip(tr("Font Size (pt)"));
    connect(m_cmbFontSize, &QComboBox::currentTextChanged, this, [this]() {
        emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
    });
    lay->addWidget(m_cmbFontSize, 0, Qt::AlignVCenter);

    m_btnBold = new QToolButton(m_textPage);
    m_btnBold->setFixedSize(26, 26);
    m_btnBold->setText(QStringLiteral("B"));
    m_btnBold->setCheckable(true);
    m_btnBold->setCursor(Qt::PointingHandCursor);
    m_btnBold->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    m_btnBold->setToolTip(tr("Bold (Ctrl+B)"));
    connect(m_btnBold, &QToolButton::toggled, this, [this]() {
        emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
    });
    lay->addWidget(m_btnBold, 0, Qt::AlignVCenter);

    m_btnItalic = new QToolButton(m_textPage);
    m_btnItalic->setFixedSize(26, 26);
    m_btnItalic->setText(QStringLiteral("I"));
    m_btnItalic->setCheckable(true);
    m_btnItalic->setCursor(Qt::PointingHandCursor);
    m_btnItalic->setStyleSheet(QStringLiteral("font-style: italic; font-weight: bold; font-size: 12px;"));
    m_btnItalic->setToolTip(tr("Italic (Ctrl+I)"));
    connect(m_btnItalic, &QToolButton::toggled, this, [this]() {
        emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
    });
    lay->addWidget(m_btnItalic, 0, Qt::AlignVCenter);

    auto *lblColor = new QLabel(tr("Color:"), m_textPage);
    lay->addWidget(lblColor, 0, Qt::AlignVCenter);

    m_btnTextColor = new QToolButton(m_textPage);
    m_btnTextColor->setFixedSize(28, 26);
    m_btnTextColor->setCursor(Qt::PointingHandCursor);
    m_btnTextColor->setToolTip(tr("Text Color..."));
    updateColorSwatchButton(m_btnTextColor, m_textColor);
    connect(m_btnTextColor, &QToolButton::clicked, this, [this]() {
        openColorDialog(0);
    });
    lay->addWidget(m_btnTextColor, 0, Qt::AlignVCenter);

    lay->addSpacing(4);

    auto *lblBg = new QLabel(tr("Background:"), m_textPage);
    lay->addWidget(lblBg, 0, Qt::AlignVCenter);

    m_cmbTextBgMode = new QComboBox(m_textPage);
    m_cmbTextBgMode->setFixedHeight(26);
    m_cmbTextBgMode->addItem(tr("🪄 Auto (Sampled)"), static_cast<int>(PdfTextBgMode::Auto));
    m_cmbTextBgMode->addItem(tr("⚪ None (Transparent)"), static_cast<int>(PdfTextBgMode::Transparent));
    m_cmbTextBgMode->addItem(tr("🌫️ Feathered (Soft)"), static_cast<int>(PdfTextBgMode::Feathered));
    m_cmbTextBgMode->addItem(tr("🎨 Custom Color..."), static_cast<int>(PdfTextBgMode::Custom));
    m_cmbTextBgMode->setToolTip(tr("Background coverage behind text: Auto samples the exact page color, None keeps background transparent, Feathered softens rectangular borders, Custom lets you pick any tint."));
    lay->addWidget(m_cmbTextBgMode, 0, Qt::AlignVCenter);

    m_btnTextBgColor = new QToolButton(m_textPage);
    m_btnTextBgColor->setFixedSize(28, 26);
    m_btnTextBgColor->setCursor(Qt::PointingHandCursor);
    m_btnTextBgColor->setToolTip(tr("Background Patch Color..."));
    updateColorSwatchButton(m_btnTextBgColor, m_textBgColor);
    connect(m_btnTextBgColor, &QToolButton::clicked, this, [this]() {
        openColorDialog(5);
    });
    lay->addWidget(m_btnTextBgColor, 0, Qt::AlignVCenter);

    connect(m_cmbTextBgMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto mode = static_cast<PdfTextBgMode>(m_cmbTextBgMode->itemData(index).toInt());
        m_textBgMode = mode;
        if (m_chkWhiteout) {
            QSignalBlocker b(m_chkWhiteout);
            m_chkWhiteout->setChecked(mode != PdfTextBgMode::Transparent);
        }
        if (mode == PdfTextBgMode::Custom) {
            openColorDialog(5);
        } else {
            emit textBgChanged(m_textBgMode, m_textBgColor);
            emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
        }
    });

    m_chkWhiteout = new QCheckBox(tr("Whiteout background"), m_textPage);
    m_chkWhiteout->setVisible(false);

    m_txtEditContent = new QLineEdit(m_textPage);
    m_txtEditContent->setVisible(false);

    m_btnApplyText = new QPushButton(tr("✓ Apply"), m_textPage);
    m_btnApplyText->setVisible(false);
    connect(m_btnApplyText, &QPushButton::clicked, this, [this]() {
        emit textReplaceRequested(m_txtEditContent->text(), textFont(), m_textColor, isWhiteout());
    });
    connect(m_txtEditContent, &QLineEdit::returnPressed, m_btnApplyText, &QPushButton::click);

    lay->addStretch(1);

    m_btnCloseText = new QToolButton(m_textPage);
    m_btnCloseText->setFixedHeight(26);
    m_btnCloseText->setText(tr("✓ Done"));
    m_btnCloseText->setCursor(Qt::PointingHandCursor);
    m_btnCloseText->setToolTip(tr("Done / Finish In-Place Text Editing (Ctrl+Enter / Esc)"));
    connect(m_btnCloseText, &QToolButton::clicked, this, [this]() {
        emit textDoneRequested();
        emit closeRequested();
    });
    lay->addWidget(m_btnCloseText, 0, Qt::AlignVCenter);
}

// -----------------------------------------------------------------------------
// 2. HIGHLIGHTER PAGE
// -----------------------------------------------------------------------------
void ContextualActionBar::setupHighlightPage()
{
    m_highlightPage = new QWidget(m_pagesStack);
    auto *lay = new QHBoxLayout(m_highlightPage);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *lblColor = new QLabel(tr("Color:"), m_highlightPage);
    lay->addWidget(lblColor, 0, Qt::AlignVCenter);

    // 5 Popular Pastel Highlighter Chips
    const struct HlColor { const char *hex; const char *tip; } hlColors[] = {
        {"#fef08a", "Yellow Highlighter"},
        {"#bbf7d0", "Green Highlighter"},
        {"#bae6fd", "Blue Highlighter"},
        {"#fbcfe8", "Pink Highlighter"},
        {"#fed7aa", "Orange Highlighter"}
    };

    for (const auto &entry : hlColors) {
        auto *btn = new QToolButton(m_highlightPage);
        btn->setFixedSize(22, 22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr(entry.tip));
        QString hex = QString::fromLatin1(entry.hex);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border-radius: 11px; border: 1px solid rgba(0,0,0,0.25); }"
            "QToolButton:hover { border: 2px solid #38bdf8; }"
        ).arg(hex));
        connect(btn, &QToolButton::clicked, this, [this, hex]() {
            setHighlighterColor(QColor(hex));
            emit highlightPropertiesChanged(m_highlighterColor, highlighterOpacity(), isSpotlight());
            emit drawingPropertiesChanged(0, 0, m_highlighterColor, Qt::transparent);
        });
        m_hlPaletteButtons.append(btn);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    }

    // Custom Color picker
    m_btnHlColor = new QToolButton(m_highlightPage);
    m_btnHlColor->setFixedSize(26, 26);
    m_btnHlColor->setCursor(Qt::PointingHandCursor);
    m_btnHlColor->setToolTip(tr("Custom Highlighter Color..."));
    updateColorSwatchButton(m_btnHlColor, m_highlighterColor);
    connect(m_btnHlColor, &QToolButton::clicked, this, [this]() {
        openColorDialog(1);
    });
    lay->addWidget(m_btnHlColor, 0, Qt::AlignVCenter);

    lay->addSpacing(6);

    // Opacity Slider (20% to 80%)
    auto *lblOpacity = new QLabel(tr("Opacity:"), m_highlightPage);
    lay->addWidget(lblOpacity, 0, Qt::AlignVCenter);

    m_sldHlOpacity = new QSlider(Qt::Horizontal, m_highlightPage);
    m_sldHlOpacity->setRange(20, 80);
    m_sldHlOpacity->setValue(40);
    m_sldHlOpacity->setFixedWidth(90);
    m_sldHlOpacity->setToolTip(tr("Highlighter Translucency"));
    lay->addWidget(m_sldHlOpacity, 0, Qt::AlignVCenter);

    m_lblHlOpacityVal = new QLabel(QStringLiteral("40%"), m_highlightPage);
    m_lblHlOpacityVal->setFixedWidth(32);
    lay->addWidget(m_lblHlOpacityVal, 0, Qt::AlignVCenter);

    connect(m_sldHlOpacity, &QSlider::valueChanged, this, [this](int val) {
        if (m_lblHlOpacityVal) {
            m_lblHlOpacityVal->setText(QStringLiteral("%1%").arg(val));
        }
        emit highlightPropertiesChanged(m_highlighterColor, highlighterOpacity(), false);
    });

    lay->addStretch(1);

    m_btnCloseHighlight = new QToolButton(m_highlightPage);
    m_btnCloseHighlight->setFixedHeight(26);
    m_btnCloseHighlight->setText(tr("✓ Done"));
    m_btnCloseHighlight->setCursor(Qt::PointingHandCursor);
    m_btnCloseHighlight->setToolTip(tr("Done / Finish Highlighting (Esc)"));
    connect(m_btnCloseHighlight, &QToolButton::clicked, this, &ContextualActionBar::closeRequested);
    lay->addWidget(m_btnCloseHighlight, 0, Qt::AlignVCenter);
}

// -----------------------------------------------------------------------------
// 3. SHAPES PAGE
// -----------------------------------------------------------------------------
void ContextualActionBar::setupShapesPage()
{
    m_shapesPage = new QWidget(m_pagesStack);
    auto *lay = new QHBoxLayout(m_shapesPage);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto *lblShape = new QLabel(tr("Shape:"), m_shapesPage);
    lay->addWidget(lblShape, 0, Qt::AlignVCenter);

    m_shapeTypeGroup = new QButtonGroup(this);
    m_shapeTypeGroup->setExclusive(true);

    auto makeShapeBtn = [this, lay](QToolButton *&btn, int id, const QString &text, const QString &tip) {
        btn = new QToolButton(m_shapesPage);
        btn->setFixedSize(32, 26);
        btn->setText(text);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        QFont f = btn->font();
        f.setPointSize(13);
        f.setBold(true);
        btn->setFont(f);
        m_shapeTypeGroup->addButton(btn, id);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    makeShapeBtn(m_btnShapeRect, 0, QStringLiteral("▭"), tr("Rectangle"));
    makeShapeBtn(m_btnShapeCircle, 1, QStringLiteral("◯"), tr("Ellipse / Circle"));
    makeShapeBtn(m_btnShapeArrow, 2, QStringLiteral("➔"), tr("Directional Arrow"));
    makeShapeBtn(m_btnShapeLine, 3, QStringLiteral("―"), tr("Straight Line"));
    m_btnShapeRect->setChecked(true);

    connect(m_shapeTypeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit shapePropertiesChanged(id, strokeWidth(), m_strokeColor, m_fillColor);
        emit drawingPropertiesChanged(id, strokeWidth(), m_strokeColor, m_fillColor);
    });

    lay->addSpacing(4);

    auto *lblWidth = new QLabel(tr("Width:"), m_shapesPage);
    lay->addWidget(lblWidth, 0, Qt::AlignVCenter);

    m_cmbShapeStrokeWidth = new QComboBox(m_shapesPage);
    m_cmbShapeStrokeWidth->setFixedHeight(26);
    m_cmbShapeStrokeWidth->setFixedWidth(64);
    m_cmbShapeStrokeWidth->addItem(tr("1 px"), 1);
    m_cmbShapeStrokeWidth->addItem(tr("2 px"), 2);
    m_cmbShapeStrokeWidth->addItem(tr("3 px"), 3);
    m_cmbShapeStrokeWidth->addItem(tr("5 px"), 5);
    m_cmbShapeStrokeWidth->addItem(tr("8 px"), 8);
    m_cmbShapeStrokeWidth->setCurrentIndex(1); // 2px
    m_cmbShapeStrokeWidth->setToolTip(tr("Stroke Width"));
    connect(m_cmbShapeStrokeWidth, &QComboBox::currentIndexChanged, this, [this]() {
        emit shapePropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
        emit drawingPropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
    });
    lay->addWidget(m_cmbShapeStrokeWidth, 0, Qt::AlignVCenter);

    lay->addSpacing(4);

    auto *lblStroke = new QLabel(tr("Stroke:"), m_shapesPage);
    lay->addWidget(lblStroke, 0, Qt::AlignVCenter);

    const struct PaletteColor { const char *hex; const char *tip; } palette[] = {
        {"#ef4444", "Red"},
        {"#3b82f6", "Blue"},
        {"#22c55e", "Green"},
        {"#f97316", "Orange"},
        {"#fbbf24", "Yellow"},
        {"#111827", "Black"}
    };

    for (const auto &entry : palette) {
        auto *btn = new QToolButton(m_shapesPage);
        btn->setFixedSize(22, 22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr(entry.tip));
        QString hex = QString::fromLatin1(entry.hex);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border-radius: 11px; border: 1px solid rgba(0,0,0,0.25); }"
            "QToolButton:hover { border: 2px solid #38bdf8; }"
        ).arg(hex));
        connect(btn, &QToolButton::clicked, this, [this, hex]() {
            setStrokeColor(QColor(hex));
            emit shapePropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
            emit drawingPropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
        });
        m_shapePaletteButtons.append(btn);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    }

    m_btnShapeStrokeColor = new QToolButton(m_shapesPage);
    m_btnShapeStrokeColor->setFixedSize(26, 26);
    m_btnShapeStrokeColor->setCursor(Qt::PointingHandCursor);
    m_btnShapeStrokeColor->setToolTip(tr("Custom Stroke Color..."));
    updateColorSwatchButton(m_btnShapeStrokeColor, m_strokeColor);
    connect(m_btnShapeStrokeColor, &QToolButton::clicked, this, [this]() {
        openColorDialog(2);
    });
    lay->addWidget(m_btnShapeStrokeColor, 0, Qt::AlignVCenter);

    lay->addSpacing(4);

    // Fill Options
    auto *lblFill = new QLabel(tr("Fill:"), m_shapesPage);
    lay->addWidget(lblFill, 0, Qt::AlignVCenter);

    m_btnShapeFillTransp = new QToolButton(m_shapesPage);
    m_btnShapeFillTransp->setFixedHeight(26);
    m_btnShapeFillTransp->setText(tr("None"));
    m_btnShapeFillTransp->setCheckable(true);
    m_btnShapeFillTransp->setChecked(true);
    m_btnShapeFillTransp->setCursor(Qt::PointingHandCursor);
    m_btnShapeFillTransp->setToolTip(tr("Transparent Fill (No interior color)"));
    connect(m_btnShapeFillTransp, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_fillColor = Qt::transparent;
            updateColorSwatchButton(m_btnShapeFillColor, Qt::transparent);
            emit shapePropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
            emit drawingPropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
        }
    });
    lay->addWidget(m_btnShapeFillTransp, 0, Qt::AlignVCenter);

    m_btnShapeFillColor = new QToolButton(m_shapesPage);
    m_btnShapeFillColor->setFixedSize(26, 26);
    m_btnShapeFillColor->setCursor(Qt::PointingHandCursor);
    m_btnShapeFillColor->setToolTip(tr("Pick Fill Color..."));
    updateColorSwatchButton(m_btnShapeFillColor, m_fillColor);
    connect(m_btnShapeFillColor, &QToolButton::clicked, this, [this]() {
        openColorDialog(3);
    });
    lay->addWidget(m_btnShapeFillColor, 0, Qt::AlignVCenter);

    lay->addStretch(1);

    m_btnCloseShapes = new QToolButton(m_shapesPage);
    m_btnCloseShapes->setFixedHeight(26);
    m_btnCloseShapes->setText(tr("✓ Done"));
    m_btnCloseShapes->setCursor(Qt::PointingHandCursor);
    m_btnCloseShapes->setToolTip(tr("Done / Finish Shapes (Esc)"));
    connect(m_btnCloseShapes, &QToolButton::clicked, this, &ContextualActionBar::closeRequested);
    lay->addWidget(m_btnCloseShapes, 0, Qt::AlignVCenter);
}

// -----------------------------------------------------------------------------
// 4. REDACTION PAGE
// -----------------------------------------------------------------------------
void ContextualActionBar::setupRedactPage()
{
    m_redactPage = new QWidget(m_pagesStack);
    auto *lay = new QHBoxLayout(m_redactPage);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *lblStyle = new QLabel(tr("Style:"), m_redactPage);
    lay->addWidget(lblStyle, 0, Qt::AlignVCenter);

    m_redactStyleGroup = new QButtonGroup(this);
    m_redactStyleGroup->setExclusive(true);

    m_btnRedactBlack = new QToolButton(m_redactPage);
    m_btnRedactBlack->setFixedHeight(26);
    m_btnRedactBlack->setText(tr("⬛ Blackout"));
    m_btnRedactBlack->setCheckable(true);
    m_btnRedactBlack->setChecked(true);
    m_btnRedactBlack->setCursor(Qt::PointingHandCursor);
    m_btnRedactBlack->setToolTip(tr("Black opaque redaction box"));
    m_redactStyleGroup->addButton(m_btnRedactBlack, 0);
    lay->addWidget(m_btnRedactBlack, 0, Qt::AlignVCenter);

    m_btnRedactWhite = new QToolButton(m_redactPage);
    m_btnRedactWhite->setFixedHeight(26);
    m_btnRedactWhite->setText(tr("⬜ Whiteout"));
    m_btnRedactWhite->setCheckable(true);
    m_btnRedactWhite->setCursor(Qt::PointingHandCursor);
    m_btnRedactWhite->setToolTip(tr("White opaque redaction box"));
    m_redactStyleGroup->addButton(m_btnRedactWhite, 1);
    lay->addWidget(m_btnRedactWhite, 0, Qt::AlignVCenter);

    connect(m_redactStyleGroup, &QButtonGroup::idClicked, this, [this](int) {
        emit redactPropertiesChanged(redactStyle(), isRedactPermanent());
    });

    lay->addSpacing(8);

    auto *lblMode = new QLabel(tr("Security:"), m_redactPage);
    lay->addWidget(lblMode, 0, Qt::AlignVCenter);

    m_redactSecGroup = new QButtonGroup(this);
    m_redactSecGroup->setExclusive(true);

    m_btnRedactVisual = new QToolButton(m_redactPage);
    m_btnRedactVisual->setFixedHeight(26);
    m_btnRedactVisual->setText(tr("👁️ Visual Overlay"));
    m_btnRedactVisual->setCheckable(true);
    m_btnRedactVisual->setChecked(true);
    m_btnRedactVisual->setCursor(Qt::PointingHandCursor);
    m_btnRedactVisual->setToolTip(tr("Visual overlay (fast, reversible)"));
    m_redactSecGroup->addButton(m_btnRedactVisual, 0);
    lay->addWidget(m_btnRedactVisual, 0, Qt::AlignVCenter);

    m_btnRedactSanitize = new QToolButton(m_redactPage);
    m_btnRedactSanitize->setFixedHeight(26);
    m_btnRedactSanitize->setText(tr("🔒 Real Sanitize"));
    m_btnRedactSanitize->setCheckable(true);
    m_btnRedactSanitize->setCursor(Qt::PointingHandCursor);
    m_btnRedactSanitize->setToolTip(tr("Permanent redaction: permanently wipes underlying text & vector data"));
    m_redactSecGroup->addButton(m_btnRedactSanitize, 1);
    lay->addWidget(m_btnRedactSanitize, 0, Qt::AlignVCenter);

    connect(m_redactSecGroup, &QButtonGroup::idClicked, this, [this](int) {
        emit redactPropertiesChanged(redactStyle(), isRedactPermanent());
    });

    lay->addStretch(1);

    m_btnCloseRedact = new QToolButton(m_redactPage);
    m_btnCloseRedact->setFixedHeight(26);
    m_btnCloseRedact->setText(tr("✓ Done"));
    m_btnCloseRedact->setCursor(Qt::PointingHandCursor);
    m_btnCloseRedact->setToolTip(tr("Done / Finish Redaction (Esc)"));
    connect(m_btnCloseRedact, &QToolButton::clicked, this, &ContextualActionBar::closeRequested);
    lay->addWidget(m_btnCloseRedact, 0, Qt::AlignVCenter);
}

// -----------------------------------------------------------------------------
// 5. RUBBER STAMP PAGE
// -----------------------------------------------------------------------------
void ContextualActionBar::setupStampPage()
{
    m_stampPage = new QWidget(m_pagesStack);
    auto *lay = new QHBoxLayout(m_stampPage);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto *lblStamp = new QLabel(tr("Stamp:"), m_stampPage);
    lay->addWidget(lblStamp, 0, Qt::AlignVCenter);

    m_cmbStampPreset = new QComboBox(m_stampPage);
    m_cmbStampPreset->setFixedHeight(26);
    m_cmbStampPreset->setMinimumWidth(115);
    m_cmbStampPreset->setEditable(true);
    m_cmbStampPreset->setInsertPolicy(QComboBox::NoInsert);
    m_cmbStampPreset->setCompleter(nullptr);

    const QStringList presets = {
        QStringLiteral("APPROVED"),
        QStringLiteral("REJECTED"),
        QStringLiteral("CONFIDENTIAL"),
        QStringLiteral("DRAFT"),
        QStringLiteral("PAID"),
        QStringLiteral("COPY")
    };
    for (const auto &p : presets) {
        m_cmbStampPreset->addItem(p);
    }
    m_cmbStampPreset->setToolTip(tr("Select rubber stamp preset or type custom text"));
    connect(m_cmbStampPreset, &QComboBox::activated, this, [this](int) {
        emitStampProperties();
        emit stampPropertiesCommitted();
    });
    if (m_cmbStampPreset->lineEdit()) {
        connect(m_cmbStampPreset->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
            emitStampProperties();
        });
    }
    lay->addWidget(m_cmbStampPreset, 0, Qt::AlignVCenter);

    // Border Style Combo (right next to text preset combo)
    m_cmbStampStyle = new QComboBox(m_stampPage);
    m_cmbStampStyle->setFixedHeight(26);
    m_cmbStampStyle->addItem(tr("Rounded Box"), 0);
    m_cmbStampStyle->addItem(tr("Double Border"), 1);
    m_cmbStampStyle->addItem(tr("Dotted Border"), 2);
    m_cmbStampStyle->addItem(tr("Plain Text"), 3);
    m_cmbStampStyle->addItem(tr("Solid Box"), 4);
    m_cmbStampStyle->setToolTip(tr("Border style (Rounded, Double, Dotted, Plain, Solid)"));
    connect(m_cmbStampStyle, &QComboBox::activated, this, [this](int) {
        emitStampProperties();
        emit stampPropertiesCommitted();
    });
    lay->addWidget(m_cmbStampStyle, 0, Qt::AlignVCenter);

    lay->addSpacing(4);

    // Color controls: 3 round circles without text + Palette button
    auto *lblColor = new QLabel(tr("Color:"), m_stampPage);
    lay->addWidget(lblColor, 0, Qt::AlignVCenter);

    m_stampColorGroup = new QButtonGroup(this);
    m_stampColorGroup->setExclusive(true);

    auto makeStampColorBtn = [this, lay](QToolButton *&btn, int id, const QString &hex, const QString &tip) {
        btn = new QToolButton(m_stampPage);
        btn->setFixedSize(22, 22);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString(
            "QToolButton { background-color: %1; border-radius: 11px; border: 1px solid rgba(0,0,0,0.3); }"
            "QToolButton:hover { border: 2px solid #38bdf8; }"
            "QToolButton:checked { border: 2px solid #ffffff; outline: 1px solid %1; }"
        ).arg(hex));
        m_stampColorGroup->addButton(btn, id);
        connect(btn, &QToolButton::clicked, this, [this, hex]() {
            m_stampColor = QColor(hex);
            emitStampProperties();
            emit stampPropertiesCommitted();
        });
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    makeStampColorBtn(m_btnStampRed, 0, QStringLiteral("#dc2626"), tr("Red Stamp"));
    makeStampColorBtn(m_btnStampGreen, 1, QStringLiteral("#16a34a"), tr("Green Stamp"));
    makeStampColorBtn(m_btnStampBlue, 2, QStringLiteral("#2563eb"), tr("Blue Stamp"));
    m_btnStampRed->setChecked(true);

    // Palette icon button for custom color
    m_btnStampPalette = new QToolButton(m_stampPage);
    m_btnStampPalette->setFixedSize(26, 26);
    m_btnStampPalette->setCursor(Qt::PointingHandCursor);
    m_btnStampPalette->setToolTip(tr("Custom Stamp Color (Palette)..."));
    m_btnStampPalette->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, QColor(QStringLiteral("#cbd5e1")), 16));
    m_btnStampPalette->setIconSize(QSize(16, 16));
    connect(m_btnStampPalette, &QToolButton::clicked, this, [this]() {
        openColorDialog(4);
    });
    lay->addWidget(m_btnStampPalette, 0, Qt::AlignVCenter);

    lay->addSpacing(4);

    // Angle slider + label + 0° reset button
    auto *lblAngle = new QLabel(tr("Angle:"), m_stampPage);
    lay->addWidget(lblAngle, 0, Qt::AlignVCenter);

    m_sliderStampAngle = new QSlider(Qt::Horizontal, m_stampPage);
    m_sliderStampAngle->setRange(-45, 45);
    m_sliderStampAngle->setValue(0);
    m_sliderStampAngle->setFixedWidth(65);
    m_sliderStampAngle->setToolTip(tr("Stamp rotation angle (-45° to +45°)"));
    lay->addWidget(m_sliderStampAngle, 0, Qt::AlignVCenter);

    m_lblStampAngle = new QLabel(QStringLiteral("0°"), m_stampPage);
    m_lblStampAngle->setFixedWidth(26);
    m_lblStampAngle->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_lblStampAngle, 0, Qt::AlignVCenter);

    m_btnResetAngle = new QToolButton(m_stampPage);
    m_btnResetAngle->setFixedHeight(24);
    m_btnResetAngle->setFixedWidth(26);
    m_btnResetAngle->setText(QStringLiteral("0°"));
    m_btnResetAngle->setCursor(Qt::PointingHandCursor);
    m_btnResetAngle->setToolTip(tr("Reset angle to 0°"));
    connect(m_btnResetAngle, &QToolButton::clicked, this, [this]() {
        setStampAngle(0);
        emitStampProperties();
        emit stampPropertiesCommitted();
    });
    lay->addWidget(m_btnResetAngle, 0, Qt::AlignVCenter);

    connect(m_sliderStampAngle, &QSlider::valueChanged, this, [this](int val) {
        if (m_lblStampAngle) {
            m_lblStampAngle->setText(QStringLiteral("%1%2°").arg(val > 0 ? "+" : "").arg(val));
        }
        emitStampProperties();
    });
    connect(m_sliderStampAngle, &QSlider::sliderReleased, this, &ContextualActionBar::stampPropertiesCommitted);

    lay->addSpacing(4);

    // Size slider + label + reset button
    auto *lblSize = new QLabel(tr("Size:"), m_stampPage);
    lay->addWidget(lblSize, 0, Qt::AlignVCenter);

    m_sliderStampSize = new QSlider(Qt::Horizontal, m_stampPage);
    m_sliderStampSize->setRange(14, 72);
    m_sliderStampSize->setValue(28);
    m_sliderStampSize->setFixedWidth(65);
    m_sliderStampSize->setToolTip(tr("Stamp size / font size (14 pt to 72 pt)"));
    lay->addWidget(m_sliderStampSize, 0, Qt::AlignVCenter);

    m_lblStampSize = new QLabel(QStringLiteral("28 pt"), m_stampPage);
    m_lblStampSize->setFixedWidth(34);
    m_lblStampSize->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_lblStampSize, 0, Qt::AlignVCenter);

    m_btnResetSize = new QToolButton(m_stampPage);
    m_btnResetSize->setFixedHeight(24);
    m_btnResetSize->setFixedWidth(24);
    m_btnResetSize->setText(QStringLiteral("↺"));
    m_btnResetSize->setCursor(Qt::PointingHandCursor);
    m_btnResetSize->setToolTip(tr("Reset size to default (28 pt)"));
    connect(m_btnResetSize, &QToolButton::clicked, this, [this]() {
        setStampSize(28);
        emitStampProperties();
        emit stampPropertiesCommitted();
    });
    lay->addWidget(m_btnResetSize, 0, Qt::AlignVCenter);

    connect(m_sliderStampSize, &QSlider::valueChanged, this, [this](int val) {
        if (m_lblStampSize) {
            m_lblStampSize->setText(QStringLiteral("%1 pt").arg(val));
        }
        emitStampProperties();
    });
    connect(m_sliderStampSize, &QSlider::sliderReleased, this, &ContextualActionBar::stampPropertiesCommitted);

    lay->addStretch(1);

    m_btnCloseStamp = new QToolButton(m_stampPage);
    m_btnCloseStamp->setFixedHeight(26);
    m_btnCloseStamp->setText(tr("✓ Done"));
    m_btnCloseStamp->setCursor(Qt::PointingHandCursor);
    m_btnCloseStamp->setToolTip(tr("Done / Finish Stamp (Esc)"));
    connect(m_btnCloseStamp, &QToolButton::clicked, this, [this]() {
        emit stampPropertiesCommitted();
        emit closeRequested();
    });
    lay->addWidget(m_btnCloseStamp, 0, Qt::AlignVCenter);
}



// -----------------------------------------------------------------------------
// MODE MANAGEMENT & THEME STYLING
// -----------------------------------------------------------------------------
void ContextualActionBar::setMode(ContextBarMode mode)
{
    m_mode = mode;
    if (mode == ContextBarMode::Hidden) {
        setFixedHeight(0);
        hide();
    } else {
        if (mode == ContextBarMode::Text) {
            m_pagesStack->setCurrentWidget(m_textPage);
        } else if (mode == ContextBarMode::Highlight) {
            m_pagesStack->setCurrentWidget(m_highlightPage);
        } else if (mode == ContextBarMode::Shapes) {
            m_pagesStack->setCurrentWidget(m_shapesPage);
        } else if (mode == ContextBarMode::Redact) {
            m_pagesStack->setCurrentWidget(m_redactPage);
        } else if (mode == ContextBarMode::Stamp) {
            m_pagesStack->setCurrentWidget(m_stampPage);
        }
        setFixedHeight(36);
        show();
    }
    update();
}

void ContextualActionBar::applyThemePreset(ThemePreset preset)
{
    m_currentPreset = preset;
    updateThemeStyles();
}

void ContextualActionBar::updateThemeStyles()
{
    bool isDark = ThemeManager::isDark(m_currentPreset);

    QString barBg = isDark ? QStringLiteral("#181b20") : QStringLiteral("#f1f2f5");
    QString borderTop = isDark ? QStringLiteral("#2e3440") : QStringLiteral("#d2d4dc");
    QString textColor = isDark ? QStringLiteral("#e2e8f0") : QStringLiteral("#1f2937");
    QString btnBg = isDark ? QStringLiteral("#262b33") : QStringLiteral("#e4e7ed");
    QString btnHover = isDark ? QStringLiteral("#333a45") : QStringLiteral("#d8dbe2");
    QString btnChecked = isDark ? QStringLiteral("#0284c7") : QStringLiteral("#007aff");

    setStyleSheet(QString(
        "#ContextualActionBar {"
        "   background-color: %1;"
        "   border-top: 1px solid %2;"
        "}"
        "QLabel {"
        "   color: %3;"
        "   font-weight: 500;"
        "   font-size: 11px;"
        "}"
        "QComboBox {"
        "   background-color: %4;"
        "   color: %3;"
        "   border: 1px solid %2;"
        "   border-radius: 4px;"
        "   padding: 1px 6px;"
        "   font-size: 11px;"
        "}"
        "QComboBox:hover {"
        "   border-color: #38bdf8;"
        "}"
        "QToolButton {"
        "   background-color: %4;"
        "   color: %3;"
        "   border: 1px solid %2;"
        "   border-radius: 4px;"
        "   padding: 2px 6px;"
        "   font-size: 11px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %5;"
        "   border-color: #38bdf8;"
        "}"
        "QToolButton:checked {"
        "   background-color: %6;"
        "   color: #ffffff;"
        "   border-color: %6;"
        "}"
        "QCheckBox {"
        "   color: %3;"
        "   font-size: 11px;"
        "   spacing: 4px;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: %2;"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: %6;"
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #ffffff;"
        "   border: 1px solid %6;"
        "   width: 14px;"
        "   margin-top: -5px;"
        "   margin-bottom: -5px;"
        "   border-radius: 7px;"
        "}"
    ).arg(barBg, borderTop, textColor, btnBg, btnHover, btnChecked));

    updateColorSwatchButton(m_btnTextColor, m_textColor);
    updateColorSwatchButton(m_btnHlColor, m_highlighterColor);
    updateColorSwatchButton(m_btnShapeStrokeColor, m_strokeColor);
    updateColorSwatchButton(m_btnShapeFillColor, m_fillColor);
    if (m_btnStampPalette) {
        QColor icCol = isDark ? QColor(QStringLiteral("#cbd5e1")) : QColor(QStringLiteral("#475569"));
        m_btnStampPalette->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, icCol, 16));
    }
}

void ContextualActionBar::updateColorSwatchButton(QToolButton *btn, const QColor &color)
{
    if (!btn) return;
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    {
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        if (color == Qt::transparent) {
            // Draw red diagonal line for transparent
            p.setPen(QPen(QColor(239, 68, 68), 2));
            p.drawLine(2, 14, 14, 2);
            p.setPen(QPen(QColor(128, 128, 128, 140), 1));
            p.drawEllipse(1, 1, 14, 14);
        } else {
            p.setBrush(color);
            p.setPen(QPen(QColor(128, 128, 128, 140), 1));
            p.drawEllipse(1, 1, 14, 14);
        }
    }
    btn->setIcon(QIcon(pix));
    btn->setIconSize(QSize(16, 16));
}

void ContextualActionBar::openColorDialog(int colorTarget)
{
    QColor initial = Qt::black;
    QString title = tr("Select Color");
    if (colorTarget == 0) { initial = m_textColor; title = tr("Select Text Color"); }
    else if (colorTarget == 1) { initial = m_highlighterColor; title = tr("Select Highlighter Color"); }
    else if (colorTarget == 2) { initial = m_strokeColor; title = tr("Select Stroke Color"); }
    else if (colorTarget == 3) { initial = m_fillColor.isValid() ? m_fillColor : Qt::white; title = tr("Select Fill Color"); }
    else if (colorTarget == 4) { initial = m_stampColor; title = tr("Select Stamp Color"); }
    else if (colorTarget == 5) { initial = m_textBgColor; title = tr("Select Background Color"); }

    QColor picked = QColorDialog::getColor(initial, this, title);
    if (picked.isValid()) {
        if (colorTarget == 0) {
            setTextColor(picked);
            emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
        } else if (colorTarget == 1) {
            setHighlighterColor(picked);
            emit highlightPropertiesChanged(m_highlighterColor, highlighterOpacity(), isSpotlight());
            emit drawingPropertiesChanged(0, 0, m_highlighterColor, Qt::transparent);
        } else if (colorTarget == 2) {
            setStrokeColor(picked);
            emit shapePropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
            emit drawingPropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
        } else if (colorTarget == 3) {
            setFillColor(picked);
            if (m_btnShapeFillTransp) m_btnShapeFillTransp->setChecked(false);
            emit shapePropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
            emit drawingPropertiesChanged(activeShapeType(), strokeWidth(), m_strokeColor, m_fillColor);
        } else if (colorTarget == 4) {
            setStampColor(picked);
            emitStampProperties();
            emit stampPropertiesCommitted();
        } else if (colorTarget == 5) {
            setTextBgColor(picked);
            setTextBgMode(PdfTextBgMode::Custom);
            emit textBgChanged(PdfTextBgMode::Custom, m_textBgColor);
            emit textPropertiesChanged(textFont(), m_textColor, isWhiteout());
        }
    }
}

// -----------------------------------------------------------------------------
// GETTERS & SETTERS
// -----------------------------------------------------------------------------
QFont ContextualActionBar::textFont() const
{
    QString fam = m_cmbFontFamily ? m_cmbFontFamily->currentText() : QStringLiteral("Noto Sans");
    int size = m_cmbFontSize ? m_cmbFontSize->currentText().toInt() : 12;
    if (size <= 0) size = 12;

    QFont f(fam, size);
    if (m_btnBold && m_btnBold->isChecked()) f.setBold(true);
    if (m_btnItalic && m_btnItalic->isChecked()) f.setItalic(true);
    return f;
}

void ContextualActionBar::setTextFont(const QFont &font)
{
    if (m_cmbFontFamily) {
        QString fam = font.family();
        QString resolved = QFontInfo(font).family();
        if (!resolved.isEmpty()) {
            fam = resolved;
        }
        int idx = m_cmbFontFamily->findText(fam, Qt::MatchFixedString);
        if (idx < 0) {
            idx = m_cmbFontFamily->findText(font.family(), Qt::MatchFixedString);
        }
        if (idx < 0) {
            m_cmbFontFamily->addItem(fam);
            idx = m_cmbFontFamily->findText(fam, Qt::MatchFixedString);
        }
        if (idx >= 0) {
            m_cmbFontFamily->setCurrentIndex(idx);
        }
    }
    if (m_cmbFontSize) {
        m_cmbFontSize->setCurrentText(QString::number(font.pointSize() > 0 ? font.pointSize() : 12));
    }
    if (m_btnBold) m_btnBold->setChecked(font.bold());
    if (m_btnItalic) m_btnItalic->setChecked(font.italic());
}

void ContextualActionBar::setTextColor(const QColor &color)
{
    m_textColor = color;
    updateColorSwatchButton(m_btnTextColor, m_textColor);
}

bool ContextualActionBar::isWhiteout() const
{
    return m_textBgMode != PdfTextBgMode::Transparent;
}

void ContextualActionBar::setWhiteout(bool whiteout)
{
    setTextBgMode(whiteout ? PdfTextBgMode::Auto : PdfTextBgMode::Transparent);
}

void ContextualActionBar::setTextBgMode(PdfTextBgMode mode)
{
    m_textBgMode = mode;
    if (m_chkWhiteout) {
        QSignalBlocker b(m_chkWhiteout);
        m_chkWhiteout->setChecked(mode != PdfTextBgMode::Transparent);
    }
    if (m_cmbTextBgMode) {
        int idx = m_cmbTextBgMode->findData(static_cast<int>(mode));
        if (idx >= 0 && m_cmbTextBgMode->currentIndex() != idx) {
            QSignalBlocker b(m_cmbTextBgMode);
            m_cmbTextBgMode->setCurrentIndex(idx);
        }
    }
}

void ContextualActionBar::setTextBgColor(const QColor &color)
{
    m_textBgColor = color;
    updateColorSwatchButton(m_btnTextBgColor, m_textBgColor);
}

QString ContextualActionBar::textContent() const
{
    return m_txtEditContent ? m_txtEditContent->text() : QString();
}

void ContextualActionBar::setTextContent(const QString &text)
{
    if (m_txtEditContent) {
        QSignalBlocker blocker(m_txtEditContent);
        m_txtEditContent->setText(text);
    }
}

// Highlighter
void ContextualActionBar::setHighlighterColor(const QColor &color)
{
    m_highlighterColor = color;
    updateColorSwatchButton(m_btnHlColor, m_highlighterColor);
}

double ContextualActionBar::highlighterOpacity() const
{
    return m_sldHlOpacity ? (static_cast<double>(m_sldHlOpacity->value()) / 100.0) : 0.40;
}

void ContextualActionBar::setHighlighterOpacity(double opacity)
{
    int val = qBound(20, qRound(opacity * 100.0), 80);
    if (m_sldHlOpacity) m_sldHlOpacity->setValue(val);
}

bool ContextualActionBar::isSpotlight() const
{
    return false;
}

void ContextualActionBar::setSpotlight(bool)
{
}

// Shapes
int ContextualActionBar::activeShapeType() const
{
    return m_shapeTypeGroup ? m_shapeTypeGroup->checkedId() : 0;
}

void ContextualActionBar::setActiveShapeType(int shapeType)
{
    if (m_shapeTypeGroup) {
        auto *btn = m_shapeTypeGroup->button(shapeType);
        if (btn) btn->setChecked(true);
    }
}

int ContextualActionBar::strokeWidth() const
{
    return m_cmbShapeStrokeWidth ? m_cmbShapeStrokeWidth->currentData().toInt() : 2;
}

void ContextualActionBar::setStrokeWidth(int width)
{
    if (!m_cmbShapeStrokeWidth) return;
    int idx = m_cmbShapeStrokeWidth->findData(width);
    if (idx >= 0) m_cmbShapeStrokeWidth->setCurrentIndex(idx);
}

void ContextualActionBar::setStrokeColor(const QColor &color)
{
    m_strokeColor = color;
    updateColorSwatchButton(m_btnShapeStrokeColor, m_strokeColor);
}

void ContextualActionBar::setFillColor(const QColor &color)
{
    m_fillColor = color;
    updateColorSwatchButton(m_btnShapeFillColor, m_fillColor);
}

// Redaction
int ContextualActionBar::redactStyle() const
{
    return m_redactStyleGroup ? m_redactStyleGroup->checkedId() : 0;
}

void ContextualActionBar::setRedactStyle(int style)
{
    if (m_redactStyleGroup) {
        auto *btn = m_redactStyleGroup->button(style);
        if (btn) btn->setChecked(true);
    }
}

bool ContextualActionBar::isRedactPermanent() const
{
    return m_redactSecGroup ? (m_redactSecGroup->checkedId() == 1) : false;
}

void ContextualActionBar::setRedactPermanent(bool permanent)
{
    if (m_redactSecGroup) {
        auto *btn = m_redactSecGroup->button(permanent ? 1 : 0);
        if (btn) btn->setChecked(true);
    }
}

// Rubber Stamp
void ContextualActionBar::emitStampProperties()
{
    emit stampPropertiesChanged(stampText(), m_stampColor, stampAngle(), stampStyle(), stampSize());
}

QString ContextualActionBar::stampText() const
{
    return m_cmbStampPreset ? m_cmbStampPreset->currentText() : QStringLiteral("APPROVED");
}

void ContextualActionBar::setStampText(const QString &text)
{
    if (m_cmbStampPreset) {
        m_cmbStampPreset->setCurrentText(text);
    }
}

void ContextualActionBar::setStampColor(const QColor &color)
{
    m_stampColor = color;
    if (m_stampColorGroup) {
        if (color == QColor(QStringLiteral("#dc2626")) && m_btnStampRed) {
            m_btnStampRed->setChecked(true);
        } else if (color == QColor(QStringLiteral("#16a34a")) && m_btnStampGreen) {
            m_btnStampGreen->setChecked(true);
        } else if (color == QColor(QStringLiteral("#2563eb")) && m_btnStampBlue) {
            m_btnStampBlue->setChecked(true);
        } else {
            auto *checked = m_stampColorGroup->checkedButton();
            if (checked) {
                m_stampColorGroup->setExclusive(false);
                checked->setChecked(false);
                m_stampColorGroup->setExclusive(true);
            }
        }
    }
}

qreal ContextualActionBar::stampAngle() const
{
    return m_sliderStampAngle ? static_cast<qreal>(m_sliderStampAngle->value()) : 0.0;
}

void ContextualActionBar::setStampAngle(qreal angle)
{
    int val = qBound(-45, qRound(angle), 45);
    if (m_sliderStampAngle) {
        QSignalBlocker b(m_sliderStampAngle);
        m_sliderStampAngle->setValue(val);
    }
    if (m_lblStampAngle) {
        m_lblStampAngle->setText(QStringLiteral("%1%2°").arg(val > 0 ? "+" : "").arg(val));
    }
}

int ContextualActionBar::stampStyle() const
{
    return m_cmbStampStyle ? m_cmbStampStyle->currentData().toInt() : 0;
}

void ContextualActionBar::setStampStyle(int style)
{
    if (m_cmbStampStyle) {
        QSignalBlocker b(m_cmbStampStyle);
        int idx = m_cmbStampStyle->findData(style);
        if (idx >= 0) m_cmbStampStyle->setCurrentIndex(idx);
    }
}

int ContextualActionBar::stampSize() const
{
    return m_sliderStampSize ? m_sliderStampSize->value() : 28;
}

void ContextualActionBar::setStampSize(int size)
{
    int val = qBound(14, size, 72);
    if (m_sliderStampSize) {
        QSignalBlocker b(m_sliderStampSize);
        m_sliderStampSize->setValue(val);
    }
    if (m_lblStampSize) {
        m_lblStampSize->setText(QStringLiteral("%1 pt").arg(val));
    }
}

void ContextualActionBar::setStampProperties(const QString &text, const QColor &color, qreal angle, int style, int size)
{
    if (m_cmbStampPreset) {
        QSignalBlocker bPreset(m_cmbStampPreset);
        if (m_cmbStampPreset->lineEdit()) {
            m_cmbStampPreset->lineEdit()->blockSignals(true);
        }
        setStampText(text);
        if (m_cmbStampPreset->lineEdit()) {
            m_cmbStampPreset->lineEdit()->blockSignals(false);
        }
    }

    setStampStyle(style);
    setStampColor(color);
    setStampAngle(angle);
    setStampSize(size);
}

// Legacy Drawing compatibility
int ContextualActionBar::activeDrawingTool() const
{
    return activeShapeType();
}

void ContextualActionBar::setActiveDrawingTool(int toolId)
{
    setActiveShapeType(toolId);
}

void ContextualActionBar::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
}
