/**
 * @file settings_appearance_tab.cpp
 * @brief Implementation of the appearance settings tab.
 */

#include "settings_appearance_tab.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QListView>
#include <QScrollArea>
#include <QFrame>
#include <QSettings>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QEasingCurve>

// =============================================================================
// CapsuleToggleSwitch Implementation
// =============================================================================

CapsuleToggleSwitch::CapsuleToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_thumbOffset(0.0)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(48, 26);

    m_anim = new QPropertyAnimation(this, "thumbOffset", this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);

    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        if (!isVisible()) {
            m_thumbOffset = checked ? 1.0 : 0.0;
            update();
            return;
        }
        m_anim->stop();
        m_anim->setStartValue(m_thumbOffset);
        m_anim->setEndValue(checked ? 1.0 : 0.0);
        m_anim->start();
    });
}

void CapsuleToggleSwitch::setThumbOffset(qreal offset)
{
    m_thumbOffset = qBound(0.0, offset, 1.0);
    update();
}

void CapsuleToggleSwitch::setCheckedAnimated(bool checked)
{
    if (m_anim) m_anim->stop();
    const bool wasBlocked = blockSignals(true);
    setChecked(checked);
    blockSignals(wasBlocked);
    m_thumbOffset = checked ? 1.0 : 0.0;
    update();
}

void CapsuleToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled()) {
        p.setOpacity(0.45);
    }

    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool dark = ThemeManager::isDarkTheme();
    const QRectF trackRect(1.0, 1.0, width() - 2.0, height() - 2.0);
    const qreal radius = trackRect.height() / 2.0;

    // Outer track
    if (m_thumbOffset > 0.01) {
        QLinearGradient bgGrad(trackRect.topLeft(), trackRect.bottomRight());
        if (preset == ThemePreset::ZincGray) {
            bgGrad.setColorAt(0.0, QColor("#6b6c74"));
            bgGrad.setColorAt(1.0, QColor("#525359"));
            p.setBrush(bgGrad);
            p.setPen(QPen(QColor(255, 255, 255, 120), 1.0));
        } else {
            bgGrad.setColorAt(0.0, QColor("#38bdf8"));
            bgGrad.setColorAt(1.0, QColor("#2563eb"));
            p.setBrush(bgGrad);
            p.setPen(QPen(QColor(125, 211, 252, 170), 1.0));
        }
    } else {
        if (preset == ThemePreset::ZincGray) {
            p.setBrush(QColor(45, 46, 50, 210));
            p.setPen(QPen(QColor(255, 255, 255, 40), 1.0));
        } else {
            p.setBrush(dark ? QColor(36, 48, 68, 200) : QColor(203, 213, 225, 200));
            p.setPen(QPen(dark ? QColor(255, 255, 255, 30) : QColor(15, 23, 42, 35), 1.0));
        }
    }
    p.drawRoundedRect(trackRect, radius, radius);

    // Sliding thumb
    const qreal thumbDiameter = trackRect.height() - 4.0;
    const qreal minX = trackRect.left() + 2.0;
    const qreal maxX = trackRect.right() - 2.0 - thumbDiameter;
    const qreal thumbX = minX + (maxX - minX) * m_thumbOffset;
    const qreal thumbY = trackRect.top() + 2.0;
    const QRectF thumbRect(thumbX, thumbY, thumbDiameter, thumbDiameter);

    // Soft drop shadow under thumb
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 45));
    p.drawEllipse(thumbRect.translated(0.0, 1.0));

    // Crisp white thumb
    p.setBrush(Qt::white);
    p.drawEllipse(thumbRect);
}

// =============================================================================
// SettingsAppearanceTab Implementation
// =============================================================================

SettingsAppearanceTab::SettingsAppearanceTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsAppearanceTab::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget(scrollArea);
    content->setObjectName("AppearanceContent");
    QGridLayout *grid = new QGridLayout(content);
    grid->setContentsMargins(14, 8, 14, 8);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    QSettings settings("Bubez", "BubezView");

    // 1. Theme Selection Group (Row 0, Col 0)
    m_grpTheme = new QGroupBox(tr("Theme"), content);
    QHBoxLayout *layTheme = new QHBoxLayout(m_grpTheme);
    layTheme->setContentsMargins(14, 12, 14, 8);
    layTheme->setSpacing(10);

    m_comboTheme = new QComboBox(m_grpTheme);
    m_comboTheme->setView(new QListView(m_comboTheme));
    m_comboTheme->setCursor(Qt::PointingHandCursor);
    m_comboTheme->addItem(tr("System Default (Automatic)"), static_cast<int>(ThemePreset::System));
    m_comboTheme->addItem(tr("Dark Slate (Midnight Blue)"), static_cast<int>(ThemePreset::DarkSlate));
    m_comboTheme->addItem(tr("Zinc Gray (Neutral #36373b)"), static_cast<int>(ThemePreset::ZincGray));
    m_comboTheme->addItem(tr("Modern Light"), static_cast<int>(ThemePreset::ModernLight));

    ThemePreset currentTheme = ThemeManager::currentPreset();
    int themeIdx = -1;
    for (int i = 0; i < m_comboTheme->count(); ++i) {
        if (m_comboTheme->itemData(i).toInt() == static_cast<int>(currentTheme)) {
            themeIdx = i;
            break;
        }
    }
    // Fallback mapping for legacy Breeze presets
    if (themeIdx < 0) {
        if (currentTheme == ThemePreset::BreezeDark) {
            themeIdx = m_comboTheme->findData(static_cast<int>(ThemePreset::DarkSlate));
        } else if (currentTheme == ThemePreset::BreezeLight) {
            themeIdx = m_comboTheme->findData(static_cast<int>(ThemePreset::ModernLight));
        }
    }
    if (themeIdx >= 0) {
        m_comboTheme->setCurrentIndex(themeIdx);
    }
    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onThemeChanged);

    layTheme->addWidget(m_comboTheme, 1);
    grid->addWidget(m_grpTheme, 0, 0);

    // 2. File Transitions Group (Row 0, Col 1)
    m_grpTransition = new QGroupBox(tr("File Transitions"), content);
    QHBoxLayout *layTransition = new QHBoxLayout(m_grpTransition);
    layTransition->setContentsMargins(14, 12, 14, 8);
    layTransition->setSpacing(10);

    m_comboTransition = new QComboBox(m_grpTransition);
    m_comboTransition->setView(new QListView(m_comboTransition));
    m_comboTransition->setCursor(Qt::PointingHandCursor);
    m_comboTransition->addItem(tr("Zoom & Fade"), static_cast<int>(TransitionEffect::ZoomFade));
    m_comboTransition->addItem(tr("Parallax Slide"), static_cast<int>(TransitionEffect::ParallaxSlide));
    m_comboTransition->addItem(tr("Cross Fade"), static_cast<int>(TransitionEffect::CrossFade));
    m_comboTransition->addItem(tr("None (instant)"), static_cast<int>(TransitionEffect::None));

    TransitionEffect currentTrans = ThemeManager::currentTransition();
    for (int i = 0; i < m_comboTransition->count(); ++i) {
        if (m_comboTransition->itemData(i).toInt() == static_cast<int>(currentTrans)) {
            m_comboTransition->setCurrentIndex(i);
            break;
        }
    }
    connect(m_comboTransition, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onTransitionChanged);

    layTransition->addWidget(m_comboTransition, 1);
    grid->addWidget(m_grpTransition, 0, 1);

    // 3. Language Selection Group (Row 1, Col 0)
    m_grpLang = new QGroupBox(tr("Language"), content);
    QHBoxLayout *layLang = new QHBoxLayout(m_grpLang);
    layLang->setContentsMargins(14, 12, 14, 8);
    layLang->setSpacing(10);

    m_comboLanguage = new QComboBox(m_grpLang);
    m_comboLanguage->setView(new QListView(m_comboLanguage));
    m_comboLanguage->setCursor(Qt::PointingHandCursor);
    m_comboLanguage->addItem(tr("System Default (Automatic)"), "system");
    m_comboLanguage->addItem("Italiano", "it");
    m_comboLanguage->addItem("English", "en");
    m_comboLanguage->addItem("Español", "es");
    m_comboLanguage->addItem("Français", "fr");
    m_comboLanguage->addItem("Deutsch", "de");

    QString savedLang = settings.value("language", "system").toString();
    int langIdx = m_comboLanguage->findData(savedLang);
    if (langIdx >= 0) {
        m_comboLanguage->setCurrentIndex(langIdx);
    }
    connect(m_comboLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onLanguageChanged);

    layLang->addWidget(m_comboLanguage, 1);
    grid->addWidget(m_grpLang, 1, 0);

    // 4. Window Geometry Group (Row 1, Col 1)
    m_grpWindow = new QGroupBox(tr("Window Geometry"), content);
    QHBoxLayout *layWindow = new QHBoxLayout(m_grpWindow);
    layWindow->setContentsMargins(14, 12, 14, 8);
    layWindow->setSpacing(10);

    m_lblRememberGeometry = new QLabel(tr("Remember window size and position"), m_grpWindow);
    m_lblRememberGeometry->setWordWrap(true);
    m_lblRememberGeometry->setCursor(Qt::PointingHandCursor);
    m_lblRememberGeometry->installEventFilter(this);

    m_switchRememberGeometry = new CapsuleToggleSwitch(m_grpWindow);
    m_switchRememberGeometry->setCheckedAnimated(settings.value("remember_geometry", false).toBool());
    connect(m_switchRememberGeometry, &QAbstractButton::toggled, this, [](bool checked) {
        QSettings s("Bubez", "BubezView");
        s.setValue("remember_geometry", checked);
    });

    layWindow->addWidget(m_lblRememberGeometry, 1);
    layWindow->addWidget(m_switchRememberGeometry, 0, Qt::AlignVCenter);
    grid->addWidget(m_grpWindow, 1, 1);

    // 5. Interface Scale Group (Row 2, Col 0)
    m_grpScale = new QGroupBox(tr("Interface Scale"), content);
    QHBoxLayout *layScale = new QHBoxLayout(m_grpScale);
    layScale->setContentsMargins(14, 12, 14, 8);
    layScale->setSpacing(10);

    m_comboScale = new QComboBox(m_grpScale);
    m_comboScale->setView(new QListView(m_comboScale));
    m_comboScale->setCursor(Qt::PointingHandCursor);
    m_comboScale->addItem(tr("75%"), 75);
    m_comboScale->addItem(tr("85%"), 85);
    m_comboScale->addItem(tr("100% (Default)"), 100);
    m_comboScale->addItem(tr("115%"), 115);
    m_comboScale->addItem(tr("125%"), 125);
    m_comboScale->addItem(tr("150%"), 150);

    int curScale = ThemeManager::uiScalePercent();
    int scaleIdx = m_comboScale->findData(curScale);
    if (scaleIdx >= 0) {
        m_comboScale->setCurrentIndex(scaleIdx);
    }
    connect(m_comboScale, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onScaleChanged);

    layScale->addWidget(m_comboScale, 1);
    grid->addWidget(m_grpScale, 2, 0);

    // 6. Icon Style Group (Row 2, Col 1)
    m_grpIconStyle = new QGroupBox(tr("Icon Style"), content);
    QHBoxLayout *layIconStyle = new QHBoxLayout(m_grpIconStyle);
    layIconStyle->setContentsMargins(14, 12, 14, 8);
    layIconStyle->setSpacing(10);

    m_comboIconStyle = new QComboBox(m_grpIconStyle);
    m_comboIconStyle->setView(new QListView(m_comboIconStyle));
    m_comboIconStyle->setCursor(Qt::PointingHandCursor);
    m_comboIconStyle->addItem(tr("Linear (Default)"), static_cast<int>(IconStyle::Linear));
    m_comboIconStyle->addItem(tr("Color Accents"), static_cast<int>(IconStyle::ColorAccents));
    m_comboIconStyle->addItem(tr("Amber Yellow"), static_cast<int>(IconStyle::Yellow));
    m_comboIconStyle->addItem(tr("Coral Red"), static_cast<int>(IconStyle::Red));
    m_comboIconStyle->addItem(tr("Emerald Green"), static_cast<int>(IconStyle::Green));
    m_comboIconStyle->addItem(tr("Sky Blue"), static_cast<int>(IconStyle::Blue));

    IconStyle curStyle = ThemeManager::iconStyle();
    int styleIdx = m_comboIconStyle->findData(static_cast<int>(curStyle));
    if (styleIdx >= 0) {
        m_comboIconStyle->setCurrentIndex(styleIdx);
    }
    connect(m_comboIconStyle, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onIconStyleChanged);

    layIconStyle->addWidget(m_comboIconStyle, 1);
    grid->addWidget(m_grpIconStyle, 2, 1);

    grid->setRowStretch(3, 0);

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    updateThemeStyles();
}

bool SettingsAppearanceTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == m_lblRememberGeometry && m_switchRememberGeometry) {
            m_switchRememberGeometry->toggle();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsAppearanceTab::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool dark = ThemeManager::isDarkTheme();

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString textCol;
    QString comboBg;
    QString comboBorder;
    QString comboPopupBg;
    QString comboSelectionBg;
    QString comboSelectionText;
    QString hoverBorder;

    if (active == ThemePreset::ZincGray) {
        // 1. Zinc Gray Theme: Window is #36373b, pills & dropdowns are #585960
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");

        // Pill badge and combobox dropdown share the EXACT same color (#585960)
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        textCol = QStringLiteral("#ffffff");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        comboBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        comboPopupBg = QStringLiteral("#2a2b2e");
        comboSelectionBg = QStringLiteral("#585960");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("rgba(255, 255, 255, 0.40)");
    } else if (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight) {
        // 2. Modern Light Theme: Frosted satin pearl cards, refined titanium pills & dropdowns
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.09)");

        // Pill badge and combobox dropdown share matching frosted titanium finish
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#334155");
        textCol = QStringLiteral("#1e293b");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0)");
        comboBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        comboPopupBg = QStringLiteral("#f8fafc");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#0284c7");
    } else {
        // 3. Dark Slate (Proposal 1 Liquid Slate): Window is deep slate (#1e2538 -> #0e121c),
        // cards are frosted navy glass, pills & dropdowns share matching deep cyan-slate (#253347 -> #1a2434)
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");

        // Pill badge and combobox dropdown share the EXACT same color
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        textCol = QStringLiteral("#f8fafc");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        comboBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        comboPopupBg = QStringLiteral("#151c28");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#38bdf8");
    }

    const QString style = QString(
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 14px;"
        "  background: %3;"
        "  margin-top: 12px;"
        "  padding: 12px 14px 10px 14px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 14px;"
        "  top: 1px;"
        "  padding: 2px 12px;"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  background: transparent;"
        "}"
        "QComboBox {"
        "  background: %7;"
        "  color: %1;"
        "  border: 1px solid %6;"
        "  border-radius: 10px;"
        "  padding: 6px 30px 6px 12px;"
        "  font-size: 12px;"
        "  min-height: 28px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %8;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 24px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(%9);"
        "  width: 12px;"
        "  height: 12px;"
        "  margin-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %10;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 10px;"
        "  selection-background-color: %11;"
        "  selection-color: %12;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  min-height: 26px;"
        "  padding: 5px 12px;"
        "  border-radius: 6px;"
        "  color: %1;"
        "  background: transparent;"
        "}"
        "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
        "  background-color: %11;"
        "  color: %12;"
        "}"
    ).arg(textCol, cardBorder, cardBg, pillBg, pillColor, pillBorder, comboBg, hoverBorder,
          dark ? QStringLiteral(":/icons/chevron_down_light.svg") : QStringLiteral(":/icons/chevron_down_dark.svg"),
          comboPopupBg, comboSelectionBg, comboSelectionText);

    setStyleSheet(style);

    const QString popupViewStyle = QString(
        "QListView {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QListView::item {"
        "  min-height: 26px;"
        "  padding: 5px 12px;"
        "  border-radius: 6px;"
        "  color: %2;"
        "  background: transparent;"
        "}"
        "QListView::item:hover, QListView::item:selected {"
        "  background-color: %4;"
        "  color: %5;"
        "}"
    ).arg(comboPopupBg, textCol, cardBorder, comboSelectionBg, comboSelectionText);

    if (m_comboTheme && m_comboTheme->view()) m_comboTheme->view()->setStyleSheet(popupViewStyle);
    if (m_comboTransition && m_comboTransition->view()) m_comboTransition->view()->setStyleSheet(popupViewStyle);
    if (m_comboLanguage && m_comboLanguage->view()) m_comboLanguage->view()->setStyleSheet(popupViewStyle);
    if (m_comboScale && m_comboScale->view()) m_comboScale->view()->setStyleSheet(popupViewStyle);
    if (m_comboIconStyle && m_comboIconStyle->view()) m_comboIconStyle->view()->setStyleSheet(popupViewStyle);

    if (m_switchRememberGeometry) {
        m_switchRememberGeometry->update();
    }
}

void SettingsAppearanceTab::onThemeChanged(int index)
{
    if (!m_comboTheme || index < 0) return;
    int val = m_comboTheme->itemData(index).toInt();
    ThemePreset preset = static_cast<ThemePreset>(val);
    ThemeManager::setPreset(preset);
    updateThemeStyles();
    emit themeChanged();
}

void SettingsAppearanceTab::onTransitionChanged(int index)
{
    if (!m_comboTransition || index < 0) return;
    int val = m_comboTransition->itemData(index).toInt();
    TransitionEffect effect = static_cast<TransitionEffect>(val);
    ThemeManager::setTransition(effect);
}

void SettingsAppearanceTab::onLanguageChanged(int index)
{
    if (!m_comboLanguage || index < 0) return;
    QString langCode = m_comboLanguage->itemData(index).toString();
    QSettings settings("Bubez", "BubezView");
    settings.setValue("language", langCode);
    settings.sync();
}

void SettingsAppearanceTab::onScaleChanged(int index)
{
    if (!m_comboScale || index < 0) return;
    int scale = m_comboScale->itemData(index).toInt();
    ThemeManager::setUiScalePercent(scale);
    emit themeChanged();
}

void SettingsAppearanceTab::onIconStyleChanged(int index)
{
    if (!m_comboIconStyle || index < 0) return;
    int styleVal = m_comboIconStyle->itemData(index).toInt();
    ThemeManager::setIconStyle(static_cast<IconStyle>(styleVal));
    emit themeChanged();
}
