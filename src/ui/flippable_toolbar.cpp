/**
 * @file flippable_toolbar.cpp
 * @brief Implementation of FlippableToolbar with 400ms 3D vertical card flip animation.
 */

#include "flippable_toolbar.h"
#include <QPainter>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>
#include <QColorDialog>
#include <QEasingCurve>
#include <QTransform>

FlippableToolbar::FlippableToolbar(QWidget *parent)
    : QWidget(parent)
    , m_hlColor(QColor(255, 235, 59))
    , m_hlOpacity(0.40)
    , m_shapeStrokeColor(QColor(239, 68, 68))
    , m_shapeFillColor(QColor(239, 68, 68, 40))
{
    // Container layout holds whichever bar is active when not animating
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 1. Standard Main QToolBar
    m_mainToolBar = new QToolBar(tr("Main Toolbar"), this);
    m_mainToolBar->setObjectName(QStringLiteral("MainToolbar"));
    m_mainToolBar->setMovable(false);
    layout->addWidget(m_mainToolBar);

    // 2. Annotation Container
    m_annotationContainer = new QWidget(this);
    auto *annLayout = new QVBoxLayout(m_annotationContainer);
    annLayout->setContentsMargins(0, 0, 0, 0);
    annLayout->setSpacing(0);

    setupHighlighterBar();
    setupShapesBar();

    annLayout->addWidget(m_highlighterBar);
    annLayout->addWidget(m_shapesBar);
    m_annotationContainer->hide();
    layout->addWidget(m_annotationContainer);

    // 3. Flip Animation: Exactly 400ms with InOutCubic easing curve
    m_flipAnimation = new QVariantAnimation(this);
    m_flipAnimation->setDuration(400);
    m_flipAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    m_flipAnimation->setStartValue(0.0);
    m_flipAnimation->setEndValue(180.0);

    connect(m_flipAnimation, &QVariantAnimation::valueChanged, this, &FlippableToolbar::onAnimationValueChanged);
    connect(m_flipAnimation, &QVariantAnimation::finished, this, &FlippableToolbar::onAnimationFinished);
}

void FlippableToolbar::setupHighlighterBar()
{
    m_highlighterBar = new QWidget(m_annotationContainer);
    m_highlighterBar->setObjectName(QStringLiteral("HighlighterToolstrip"));
    m_highlighterBar->setStyleSheet(
        "QWidget#HighlighterToolstrip {"
        "  background: palette(window);"
        "  border-bottom: 2px solid #0284c7;"
        "}"
    );

    auto *hLayout = new QHBoxLayout(m_highlighterBar);
    hLayout->setContentsMargins(12, 4, 12, 4);
    hLayout->setSpacing(10);

    auto *lblTitle = new QLabel(tr("🖍️ HIGHLIGHTER"), m_highlighterBar);
    lblTitle->setStyleSheet("font-weight: 800; font-size: 12px; color: #0284c7; letter-spacing: 0.5px;");
    hLayout->addWidget(lblTitle);

    auto *sep1 = new QFrame(m_highlighterBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: palette(mid);");
    hLayout->addWidget(sep1);

    // Color Swatches
    struct Swatch {
        const char *name;
        const char *hex;
    };
    const Swatch swatches[] = {
        {"Yellow", "#FFFF00"},
        {"Lime", "#4ADE80"},
        {"Cyan", "#38BDF8"},
        {"Pink", "#F472B6"},
        {"Orange", "#FB923C"},
        {"Purple", "#C084FC"}
    };

    for (const auto &sw : swatches) {
        auto *btn = new QPushButton(m_highlighterBar);
        btn->setFixedSize(22, 22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr(sw.name));
        btn->setProperty("colorHex", QString::fromLatin1(sw.hex));
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border-radius: 11px; border: 2px solid palette(mid); }"
            "QPushButton:hover { border: 2px solid #0284c7; }"
        ).arg(sw.hex));

        connect(btn, &QPushButton::clicked, this, [this, hex = QString::fromLatin1(sw.hex)]() {
            m_hlColor = QColor(hex);
            updateHighlighterColorButtons();
            emit highlighterParamsChanged(m_hlColor, m_hlOpacity);
        });

        m_hlColorButtons.append(btn);
        hLayout->addWidget(btn);
    }

    auto *btnCustom = new QPushButton(tr("🎨"), m_highlighterBar);
    btnCustom->setFixedSize(26, 26);
    btnCustom->setCursor(Qt::PointingHandCursor);
    btnCustom->setToolTip(tr("Custom color..."));
    btnCustom->setStyleSheet("QPushButton { font-size: 13px; border-radius: 13px; border: 1px solid palette(mid); background: palette(button); color: palette(button-text); } QPushButton:hover { background: palette(light); }");
    connect(btnCustom, &QPushButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_hlColor, this, tr("Select Highlight Color"));
        if (chosen.isValid()) {
            m_hlColor = chosen;
            updateHighlighterColorButtons();
            emit highlighterParamsChanged(m_hlColor, m_hlOpacity);
        }
    });
    hLayout->addWidget(btnCustom);

    auto *sep2 = new QFrame(m_highlighterBar);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: palette(mid);");
    hLayout->addWidget(sep2);

    // Opacity Slider
    auto *lblOp = new QLabel(tr("Opacity:"), m_highlighterBar);
    lblOp->setStyleSheet("font-size: 11px; font-weight: 600; color: palette(window-text);");
    hLayout->addWidget(lblOp);

    m_hlOpacitySlider = new QSlider(Qt::Horizontal, m_highlighterBar);
    m_hlOpacitySlider->setRange(10, 90);
    m_hlOpacitySlider->setValue(40);
    m_hlOpacitySlider->setFixedWidth(90);
    hLayout->addWidget(m_hlOpacitySlider);

    m_hlOpacityLabel = new QLabel(QStringLiteral("40%"), m_highlighterBar);
    m_hlOpacityLabel->setFixedWidth(35);
    m_hlOpacityLabel->setStyleSheet("font-weight: bold; font-size: 11px; color: palette(window-text);");
    hLayout->addWidget(m_hlOpacityLabel);

    connect(m_hlOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_hlOpacity = val / 100.0;
        m_hlOpacityLabel->setText(QStringLiteral("%1%").arg(val));
        emit highlighterParamsChanged(m_hlColor, m_hlOpacity);
    });

    hLayout->addStretch();

    // Exit Button
    auto *btnExit = new QPushButton(tr("✕ Exit (Esc)"), m_highlighterBar);
    btnExit->setCursor(Qt::PointingHandCursor);
    btnExit->setStyleSheet(
        "QPushButton {"
        "  background-color: #EF4444; color: #FFFFFF; font-weight: 700; font-size: 11px;"
        "  border-radius: 6px; padding: 4px 14px; border: none;"
        "}"
        "QPushButton:hover { background-color: #DC2626; }"
    );
    connect(btnExit, &QPushButton::clicked, this, &FlippableToolbar::flipToMain);
    hLayout->addWidget(btnExit);

    updateHighlighterColorButtons();
}

void FlippableToolbar::setupShapesBar()
{
    m_shapesBar = new QWidget(m_annotationContainer);
    m_shapesBar->setObjectName(QStringLiteral("ShapesToolstrip"));
    m_shapesBar->setStyleSheet(
        "QWidget#ShapesToolstrip {"
        "  background: palette(window);"
        "  border-bottom: 2px solid #d97706;"
        "}"
    );

    auto *hLayout = new QHBoxLayout(m_shapesBar);
    hLayout->setContentsMargins(12, 4, 12, 4);
    hLayout->setSpacing(10);

    auto *lblTitle = new QLabel(tr("📐 SHAPES & ARROWS"), m_shapesBar);
    lblTitle->setStyleSheet("font-weight: 800; font-size: 12px; color: #d97706; letter-spacing: 0.5px;");
    hLayout->addWidget(lblTitle);

    auto *sep1 = new QFrame(m_shapesBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: palette(mid);");
    hLayout->addWidget(sep1);

    // Shape Type Radio/Toggle Buttons
    m_shapeTypeGroup = new QButtonGroup(this);
    m_shapeTypeGroup->setExclusive(true);

    struct ShapeOption {
        int id;
        const char *icon;
        const char *tooltip;
    };
    const ShapeOption shapes[] = {
        {0, "▭", "Rectangle"},
        {1, "◯", "Ellipse"},
        {2, "➔", "Arrow"},
        {3, "―", "Line"}
    };

    for (const auto &sh : shapes) {
        auto *btn = new QPushButton(QString::fromUtf8(sh.icon), m_shapesBar);
        btn->setCheckable(true);
        btn->setFixedSize(28, 26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr(sh.tooltip));
        btn->setStyleSheet(
            "QPushButton { font-size: 14px; background: palette(button); color: palette(button-text); border-radius: 4px; border: 1px solid palette(mid); }"
            "QPushButton:checked { background: #d97706; color: #ffffff; font-weight: bold; border: 1px solid #b45309; }"
            "QPushButton:hover:!checked { background: palette(light); }"
        );
        m_shapeTypeGroup->addButton(btn, sh.id);
        hLayout->addWidget(btn);
    }
    if (auto *defaultBtn = m_shapeTypeGroup->button(0)) {
        defaultBtn->setChecked(true);
    }

    connect(m_shapeTypeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_shapeType = id;
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });

    auto *sep2 = new QFrame(m_shapesBar);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: palette(mid);");
    hLayout->addWidget(sep2);

    // Stroke Color Swatches
    const char *strokeColors[] = {"#EF4444", "#38BDF8", "#10B981", "#F59E0B", "#8B5CF6", "#000000"};
    for (const char *hex : strokeColors) {
        auto *btn = new QPushButton(m_shapesBar);
        btn->setFixedSize(20, 20);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("colorHex", QString::fromLatin1(hex));
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border-radius: 10px; border: 2px solid palette(mid); }"
            "QPushButton:hover { border: 2px solid #d97706; }"
        ).arg(hex));
        connect(btn, &QPushButton::clicked, this, [this, h = QString::fromLatin1(hex)]() {
            m_shapeStrokeColor = QColor(h);
            if (m_shapeFillColor.isValid() && m_shapeFillColor.alpha() > 0) {
                m_shapeFillColor = m_shapeStrokeColor;
                m_shapeFillColor.setAlpha(40);
            }
            updateShapeButtons();
            emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
        });
        m_shapeStrokeColorButtons.append(btn);
        hLayout->addWidget(btn);
    }

    // Fill Toggle
    m_btnShapeFillToggle = new QPushButton(tr("Fill"), m_shapesBar);
    m_btnShapeFillToggle->setCheckable(true);
    m_btnShapeFillToggle->setChecked(true);
    m_btnShapeFillToggle->setCursor(Qt::PointingHandCursor);
    m_btnShapeFillToggle->setStyleSheet(
        "QPushButton { font-size: 11px; font-weight: 600; padding: 4px 8px; background: palette(button); color: palette(button-text); border-radius: 4px; border: 1px solid palette(mid); }"
        "QPushButton:checked { background: #d97706; color: #ffffff; border: 1px solid #b45309; font-weight: bold; }"
    );
    connect(m_btnShapeFillToggle, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_shapeFillColor = m_shapeStrokeColor;
            m_shapeFillColor.setAlpha(40);
        } else {
            m_shapeFillColor = QColor(Qt::transparent);
        }
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });
    hLayout->addWidget(m_btnShapeFillToggle);

    // Stroke Width Control (px)
    auto *sep3 = new QFrame(m_shapesBar);
    sep3->setFrameShape(QFrame::VLine);
    sep3->setStyleSheet("color: palette(mid);");
    hLayout->addWidget(sep3);

    auto *lblWidth = new QLabel(tr("Width:"), m_shapesBar);
    lblWidth->setStyleSheet("font-size: 11px; font-weight: 600; color: palette(window-text);");
    hLayout->addWidget(lblWidth);

    m_spinShapeWidth = new QSpinBox(m_shapesBar);
    m_spinShapeWidth->setRange(1, 30);
    m_spinShapeWidth->setValue(m_shapeStrokeWidth);
    m_spinShapeWidth->setSuffix(QStringLiteral(" px"));
    m_spinShapeWidth->setFixedWidth(68);
    m_spinShapeWidth->setCursor(Qt::PointingHandCursor);
    m_spinShapeWidth->setToolTip(tr("Stroke thickness in pixels"));
    connect(m_spinShapeWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_shapeStrokeWidth = val;
        emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    });
    hLayout->addWidget(m_spinShapeWidth);

    hLayout->addStretch();

    // Exit Button
    auto *btnExit = new QPushButton(tr("✕ Exit (Esc)"), m_shapesBar);
    btnExit->setCursor(Qt::PointingHandCursor);
    btnExit->setStyleSheet(
        "QPushButton {"
        "  background-color: #EF4444; color: #FFFFFF; font-weight: 700; font-size: 11px;"
        "  border-radius: 6px; padding: 4px 14px; border: none;"
        "}"
        "QPushButton:hover { background-color: #DC2626; }"
    );
    connect(btnExit, &QPushButton::clicked, this, &FlippableToolbar::flipToMain);
    hLayout->addWidget(btnExit);

    updateShapeButtons();
}

void FlippableToolbar::updateHighlighterColorButtons()
{
    QString currentHex = m_hlColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_hlColorButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        if (hex == currentHex) {
            btn->setStyleSheet(QString(
                "QPushButton { background-color: %1; border-radius: 11px; border: 3px solid #FFFFFF; }"
            ).arg(hex));
        } else {
            btn->setStyleSheet(QString(
                "QPushButton { background-color: %1; border-radius: 11px; border: 2px solid #475569; }"
                "QPushButton:hover { border: 2px solid #FFFFFF; }"
            ).arg(hex));
        }
    }
}

void FlippableToolbar::updateShapeButtons()
{
    QString currentHex = m_shapeStrokeColor.name(QColor::HexRgb).toUpper();
    for (auto *btn : m_shapeStrokeColorButtons) {
        QString hex = btn->property("colorHex").toString().toUpper();
        if (hex == currentHex) {
            btn->setStyleSheet(QString(
                "QPushButton { background-color: %1; border-radius: 10px; border: 3px solid #FFFFFF; }"
            ).arg(hex));
        } else {
            btn->setStyleSheet(QString(
                "QPushButton { background-color: %1; border-radius: 10px; border: 2px solid #475569; }"
                "QPushButton:hover { border: 2px solid #FFFFFF; }"
            ).arg(hex));
        }
    }
}

void FlippableToolbar::flipToHighlighter(const QColor &color, double opacity)
{
    m_hlColor = color;
    m_hlOpacity = opacity;
    if (m_hlOpacitySlider) {
        QSignalBlocker blocker(m_hlOpacitySlider);
        m_hlOpacitySlider->setValue(qRound(opacity * 100));
        if (m_hlOpacityLabel) m_hlOpacityLabel->setText(QStringLiteral("%1%").arg(qRound(opacity * 100)));
    }
    updateHighlighterColorButtons();
    startFlipAnimation(FlippableToolbarMode::Highlighter);
    emit highlighterParamsChanged(m_hlColor, m_hlOpacity);
}

void FlippableToolbar::flipToShapes(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeType = shapeType;
    m_shapeStrokeWidth = strokeWidth;
    m_shapeStrokeColor = strokeColor;
    m_shapeFillColor = fillColor;
    if (m_shapeTypeGroup && m_shapeTypeGroup->button(shapeType)) {
        m_shapeTypeGroup->button(shapeType)->setChecked(true);
    }
    if (m_spinShapeWidth) {
        QSignalBlocker blocker(m_spinShapeWidth);
        m_spinShapeWidth->setValue(strokeWidth);
    }
    if (m_btnShapeFillToggle) {
        m_btnShapeFillToggle->setChecked(fillColor.isValid() && fillColor.alpha() > 0);
    }
    updateShapeButtons();
    startFlipAnimation(FlippableToolbarMode::Shapes);
    emit shapeParamsChanged(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
}

void FlippableToolbar::flipToMain()
{
    if (m_currentMode == FlippableToolbarMode::Main && !m_isFlipping) return;
    startFlipAnimation(FlippableToolbarMode::Main);
    emit annotationModeExited();
}

void FlippableToolbar::startFlipAnimation(FlippableToolbarMode targetMode)
{
    if (m_currentMode == targetMode && !m_isFlipping) return;

    m_targetMode = targetMode;

    // Grab pixmap of current visible view (front)
    m_frontPixmap = grab();

    // Prepare back view in hidden mode to grab its snapshot
    if (targetMode == FlippableToolbarMode::Main) {
        m_mainToolBar->show();
        m_annotationContainer->hide();
    } else {
        m_mainToolBar->hide();
        m_annotationContainer->show();
        m_highlighterBar->setVisible(targetMode == FlippableToolbarMode::Highlighter);
        m_shapesBar->setVisible(targetMode == FlippableToolbarMode::Shapes);
    }

    // Force layout update and grab back snapshot
    layout()->activate();
    m_backPixmap = grab();

    // Restore views to front state while animating
    if (m_currentMode == FlippableToolbarMode::Main) {
        m_mainToolBar->show();
        m_annotationContainer->hide();
    } else {
        m_mainToolBar->hide();
        m_annotationContainer->show();
        m_highlighterBar->setVisible(m_currentMode == FlippableToolbarMode::Highlighter);
        m_shapesBar->setVisible(m_currentMode == FlippableToolbarMode::Shapes);
    }

    // Hide child widgets during the 3D paint transition so only our custom paintEvent renders the rotation
    m_mainToolBar->setVisible(false);
    m_annotationContainer->setVisible(false);

    m_isFlipping = true;
    m_flipAngle = 0.0;
    m_flipAnimation->stop();
    m_flipAnimation->setStartValue(0.0);
    m_flipAnimation->setEndValue(180.0);
    m_flipAnimation->start();
    update();
}

void FlippableToolbar::onAnimationValueChanged(const QVariant &value)
{
    m_flipAngle = value.toReal();
    update();
}

void FlippableToolbar::onAnimationFinished()
{
    m_isFlipping = false;
    m_currentMode = m_targetMode;

    // Show the target widget permanently
    if (m_currentMode == FlippableToolbarMode::Main) {
        m_mainToolBar->show();
        m_annotationContainer->hide();
    } else {
        m_mainToolBar->hide();
        m_annotationContainer->show();
        m_highlighterBar->setVisible(m_currentMode == FlippableToolbarMode::Highlighter);
        m_shapesBar->setVisible(m_currentMode == FlippableToolbarMode::Shapes);
    }
    update();
}

void FlippableToolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void FlippableToolbar::paintEvent(QPaintEvent *event)
{
    if (!m_isFlipping) {
        QWidget::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) return;

    painter.fillRect(rect(), palette().window());

    // 3D vertical card flip around the center horizontal axis
    // 0° -> 90°: displays front pixmap rotating away from view
    // 90° -> 180°: displays back pixmap rotating into view
    qreal angle = m_flipAngle;
    bool isFront = (angle < 90.0);

    const QPixmap &pm = isFront ? m_frontPixmap : m_backPixmap;
    if (pm.isNull()) return;

    painter.save();
    painter.translate(w / 2.0, h / 2.0);

    // Apply perspective projection (m23 simulates distance along Z)
    QTransform transform;
    transform.setMatrix(1, 0, 0,
                        0, 1, 0.0015,
                        0, 0, 1);

    qreal rotAngle = isFront ? -angle : (180.0 - angle);
    transform.rotate(rotAngle, Qt::XAxis);

    painter.setTransform(transform, true);
    painter.drawPixmap(-w / 2.0, -h / 2.0, w, h, pm);

    // Subtle dark shading at steep angles to enhance 3D depth
    qreal darkness = std::sin(angle * M_PI / 180.0) * 0.45;
    if (darkness > 0.01) {
        painter.fillRect(QRectF(-w / 2.0, -h / 2.0, w, h), QColor(0, 0, 0, static_cast<int>(darkness * 255)));
    }

    painter.restore();
}
