/**
 * @file settings_integration_tab.h
 * @brief Desktop environment and file manager integration configuration page.
 */

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;

/**
 * @class SettingsIntegrationTab
 * @brief Settings notebook tab managing desktop environment shortcuts, context menus,
 *        and file manager integration for KDE Dolphin, GNOME Nautilus, and Nemo.
 */
class SettingsIntegrationTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the desktop integration settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsIntegrationTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsIntegrationTab() override = default;

    /**
     * @brief Queries DesktopIntegrator and updates label status text and button toggles.
     */
    void refreshStatuses();

private slots:
    /**
     * @brief Toggles KDE Dolphin file manager integration shortcuts and scripts.
     */
    void toggleDolphin();

    /**
     * @brief Toggles GNOME / Nemo file manager DBus integration and preview keys.
     */
    void toggleGnomeNemo();

    /**
     * @brief Toggles file manager right-click context menu actions.
     */
    void toggleContextMenu();

    /**
     * @brief Toggles system application menu desktop launcher entry.
     */
    void toggleAppMenu();
    void onUseLibreOfficeToggled(bool checked);

private:
    void setupUi();

    QCheckBox *m_chkUseLibreOffice = nullptr;

    // Dolphin
    QLabel *m_lblDolphinStatus = nullptr;
    QComboBox *m_comboDolphinShortcut = nullptr;
    QPushButton *m_btnDolphinToggle = nullptr;

    // GNOME / Nemo
    QLabel *m_lblGnomeStatus = nullptr;
    QPushButton *m_btnGnomeToggle = nullptr;

    // Context Menu
    QLabel *m_lblContextStatus = nullptr;
    QPushButton *m_btnContextToggle = nullptr;

    // App Menu
    QLabel *m_lblAppMenuStatus = nullptr;
    QPushButton *m_btnAppMenuToggle = nullptr;
};
