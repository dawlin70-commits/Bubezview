/**
 * @file settings_appearance_tab.h
 * @brief Appearance configuration page for themes and transition animations.
 */

#pragma once

#include <QWidget>

class QComboBox;

/**
 * @class SettingsAppearanceTab
 * @brief Settings notebook tab managing interface color themes and file transition effects.
 */
class SettingsAppearanceTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the appearance settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsAppearanceTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsAppearanceTab() override = default;

signals:
    /**
     * @brief Emitted when the user selects a new theme preset.
     */
    void themeChanged();

private slots:
    /**
     * @brief Handles theme combo box selection changes.
     * @param index Chosen combo index.
     */
    void onThemeChanged(int index);

    /**
     * @brief Handles transition combo box selection changes.
     * @param index Chosen combo index.
     */
    void onTransitionChanged(int index);

private:
    void setupUi();

    QComboBox *m_comboTheme = nullptr;
    QComboBox *m_comboTransition = nullptr;
};
