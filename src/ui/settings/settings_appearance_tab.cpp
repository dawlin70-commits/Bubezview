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
#include <QSettings>
#include <QMessageBox>

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
    m_comboTheme->addItem(tr("Dark Slate"), static_cast<int>(ThemePreset::DarkSlate));
    m_comboTheme->addItem(tr("Breeze Dark"), static_cast<int>(ThemePreset::BreezeDark));

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

    // 3. Transparent Images Background Group
    QGroupBox *grpTransp = new QGroupBox(tr("Transparent Images Background"), content);
    QHBoxLayout *layTransp = new QHBoxLayout(grpTransp);
    layTransp->setContentsMargins(12, 10, 12, 12);

    QLabel *lblTransp = new QLabel(tr("Background behind transparency:"), grpTransp);
    m_comboTransparency = new QComboBox(grpTransp);
    m_comboTransparency->setCursor(Qt::PointingHandCursor);
    m_comboTransparency->addItem(tr("Solid White (Recommended - Removes Transparency)"), "white");
    m_comboTransparency->addItem(tr("Checkerboard Pattern (Classic Grid)"), "checkerboard");
    m_comboTransparency->addItem(tr("Interface Theme Color"), "theme");
    m_comboTransparency->addItem(tr("Solid Black"), "black");

    QSettings settings("Bubez", "BubezView");
    QString savedTransp = settings.value("image/transparency_mode", "white").toString();
    int transpIdx = m_comboTransparency->findData(savedTransp);
    if (transpIdx >= 0) {
        m_comboTransparency->setCurrentIndex(transpIdx);
    }
    connect(m_comboTransparency, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsAppearanceTab::onTransparencyChanged);

    layTransp->addWidget(lblTransp);
    layTransp->addWidget(m_comboTransparency, 1);
    layout->addWidget(grpTransp);

    // 4. Language Selection Group
    QGroupBox *grpLang = new QGroupBox(tr("Language"), content);
    QHBoxLayout *layLang = new QHBoxLayout(grpLang);
    layLang->setContentsMargins(12, 10, 12, 12);

    QLabel *lblLang = new QLabel(tr("Interface language:"), grpLang);
    m_comboLanguage = new QComboBox(grpLang);
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

    layLang->addWidget(lblLang);
    layLang->addWidget(m_comboLanguage, 1);
    layout->addWidget(grpLang);

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

void SettingsAppearanceTab::onTransparencyChanged(int index)
{
    if (!m_comboTransparency || index < 0) return;
    QString mode = m_comboTransparency->itemData(index).toString();
    QSettings settings("Bubez", "BubezView");
    settings.setValue("image/transparency_mode", mode);
    settings.sync();
    emit themeChanged();
}

void SettingsAppearanceTab::onLanguageChanged(int index)
{
    if (!m_comboLanguage || index < 0) return;
    QString langCode = m_comboLanguage->itemData(index).toString();
    QSettings settings("Bubez", "BubezView");
    settings.setValue("language", langCode);
    settings.sync();

    QMessageBox::information(
        this,
        tr("Language Changed"),
        tr("Language preference saved.<br><br>Please restart BubezView for the changes to take full effect across all components.")
    );
}
