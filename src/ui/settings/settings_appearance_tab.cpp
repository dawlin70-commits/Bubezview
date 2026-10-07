/**
 * @file settings_appearance_tab.cpp
 * @brief Implementation of the appearance settings tab.
 */

#include "settings_appearance_tab.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QFrame>

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
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setSpacing(12);
    layout->setContentsMargins(14, 14, 14, 14);

    // 1. Theme Selection Group
    QGroupBox *grpTheme = new QGroupBox(tr("Theme"), content);
    QHBoxLayout *layTheme = new QHBoxLayout(grpTheme);
    layTheme->setContentsMargins(12, 10, 12, 12);

    QLabel *lblTheme = new QLabel(tr("Interface theme:"), grpTheme);
    m_comboTheme = new QComboBox(grpTheme);
    m_comboTheme->setCursor(Qt::PointingHandCursor);
    m_comboTheme->addItem(tr("System Native"), static_cast<int>(ThemePreset::SystemNative));
    m_comboTheme->addItem("Dark Slate", static_cast<int>(ThemePreset::DarkSlate));
    m_comboTheme->addItem("Breeze Dark", static_cast<int>(ThemePreset::BreezeDark));

    ThemePreset currentTheme = ThemeManager::currentPreset();
    for (int i = 0; i < m_comboTheme->count(); ++i) {
        if (m_comboTheme->itemData(i).toInt() == static_cast<int>(currentTheme)) {
            m_comboTheme->setCurrentIndex(i);
            break;
        }
    }
    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onThemeChanged);

    layTheme->addWidget(lblTheme);
    layTheme->addWidget(m_comboTheme, 1);
    layout->addWidget(grpTheme);

    // 2. Transitions Group
    QGroupBox *grpTransition = new QGroupBox(tr("File Transitions"), content);
    QHBoxLayout *layTransition = new QHBoxLayout(grpTransition);
    layTransition->setContentsMargins(12, 10, 12, 12);

    QLabel *lblTransition = new QLabel(tr("Transition effect:"), grpTransition);
    m_comboTransition = new QComboBox(grpTransition);
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

    layTransition->addWidget(lblTransition);
    layTransition->addWidget(m_comboTransition, 1);
    layout->addWidget(grpTransition);

    layout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsAppearanceTab::onThemeChanged(int index)
{
    if (!m_comboTheme || index < 0) return;
    int val = m_comboTheme->itemData(index).toInt();
    ThemePreset preset = static_cast<ThemePreset>(val);
    ThemeManager::setPreset(preset);
    emit themeChanged();
}

void SettingsAppearanceTab::onTransitionChanged(int index)
{
    if (!m_comboTransition || index < 0) return;
    int val = m_comboTransition->itemData(index).toInt();
    TransitionEffect effect = static_cast<TransitionEffect>(val);
    ThemeManager::setTransition(effect);
}
