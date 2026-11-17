/**
 * @file settings_integration_tab.h
 * @brief Desktop environment and file manager integration configuration page.
 */

#pragma once

#include <QWidget>

class QLabel;
class QGroupBox;
class QPushButton;
class CapsuleToggleSwitch;

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
     * @brief Queries DesktopIntegrator and updates toggle switch states.
     */
    void refreshStatuses();

    /**
     * @brief Updates Liquid Glass card styling and controls to match the active theme.
     */
    void updateThemeStyles();

private slots:
    void onSpacePreviewToggled(bool checked);
    void onFallbackShortcutToggled(bool checked);
    void onContextMenuToggled(bool checked);
    void onAppMenuToggled(bool checked);
    void onUseLibreOfficeToggled(bool checked);
    void onTestSpaceClicked();
    void onUnsupportedGuideClicked();

private:
    void setupUi();
    QWidget *createSettingRow(
        const QString &title,
        QWidget *control,
        QWidget *parent,
        QWidget *extraWidget = nullptr,
        const QString &subtitle = QString()
    );

    QLabel *m_lblHeader = nullptr;

    // Card 1: File Manager & Spacebar Quick Look
    QGroupBox *m_grpQuickLook = nullptr;
    CapsuleToggleSwitch *m_switchSpace = nullptr;
    QPushButton *m_btnTestSpace = nullptr;
    CapsuleToggleSwitch *m_switchFallback = nullptr;
    QPushButton *m_btnGuide = nullptr;

    // Card 2: System Integration (Context Menu & Launcher)
    QGroupBox *m_grpSystem = nullptr;
    CapsuleToggleSwitch *m_switchContextMenu = nullptr;
    CapsuleToggleSwitch *m_switchAppMenu = nullptr;

    // Card 3: Office Documents
    QGroupBox *m_grpOffice = nullptr;
    CapsuleToggleSwitch *m_switchLibreOffice = nullptr;
};
