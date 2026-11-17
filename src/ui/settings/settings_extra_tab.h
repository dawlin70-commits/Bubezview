/**
 * @file settings_extra_tab.h
 * @brief Diagnostics, multi-level debug modes, and system maintenance settings page.
 */

#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QGroupBox;

/**
 * @class SettingsExtraTab
 * @brief Settings notebook tab managing system diagnostics, multi-level debug modes, and setup maintenance.
 */
class SettingsExtraTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the diagnostics and maintenance settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsExtraTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsExtraTab() override = default;

    /**
     * @brief Updates Liquid Glass card styling and controls to match the active theme.
     */
    void updateThemeStyles();

signals:
    /**
     * @brief Emitted when theme or setup configuration is updated.
     */
    void themeChanged();

    /**
     * @brief Emitted when the first-run wizard finishes.
     */
    void setupWizardCompleted();

    /**
     * @brief Emitted when a debug mode transition requires recompiling a specific target.
     * @param target Specific target to compile (defaults to "bubezview").
     */
    void recompileRequested(const QString &target = QStringLiteral("bubezview"));

private slots:
    /**
     * @brief Handles selection changes in the debug level combo box.
     * @param index Chosen combo box index.
     */
    void onDebugLevelChanged(int index);

    /**
     * @brief Gathers system diagnostics, soft dependencies, plugins, and session debug logs, copying to clipboard.
     */
    void copyFullReportToClipboard();

    /**
     * @brief Launches the interactive initial setup and onboarding wizard.
     */
    void launchSetupWizard();

    /**
     * @brief Repairs and reinstalls desktop launcher, file manager context menus, and quick-look shortcuts.
     */
    void reinstallIntegrations();

    /**
     * @brief Clears cached thumbnails, shader caches, and temporary data.
     */
    void clearThumbnailCache();

    /**
     * @brief Displays an uninstallation dialog and cleanly uninstalls BubezView components.
     */
    void uninstallApplication();

private:
    /**
     * @brief Initializes widgets, layouts, and data bindings.
     */
    void setupUi();

    /**
     * @brief Updates status label text according to the currently active debug level.
     */
    void updateDebugStatusLabel();

    QGroupBox *m_grpMaintenance = nullptr;
    QGroupBox *m_grpDebug = nullptr;
    QComboBox *m_comboDebugLevel = nullptr;
    QLabel *m_lblDebugStatus = nullptr;
    QLabel *m_lblMaintSub = nullptr;
    QLabel *m_lblDebugSub = nullptr;
    QLabel *m_lblTermHint = nullptr;
    QPushButton *m_btnRunWizard = nullptr;
    QPushButton *m_btnReinstall = nullptr;
    QPushButton *m_btnClearCache = nullptr;
    QPushButton *m_btnUninstall = nullptr;
    QPushButton *m_btnCopyReport = nullptr;
    int m_previousDebugLevel = 0;
    bool m_isUpdatingUi = false;
};
