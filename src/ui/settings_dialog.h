/**
 * @file settings_dialog.h
 * @brief Application settings and desktop preferences dialog coordinator.
 */

#pragma once

#include <QDialog>

class QTabWidget;
class QFrame;
class QLabel;
class QPushButton;
class BuildConsoleDialog;

class SettingsAppearanceTab;
class SettingsIntegrationTab;
class SettingsPerformanceTab;
class SettingsExtensionsTab;
class SettingsExtraTab;

/**
 * @class SettingsDialog
 * @brief Coordinator dialog managing themes, desktop integration, compiler tuning,
 *        modular extensions, and general preferences.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @enum TabIndex
     * @brief Identifies the functional pages available in the settings notebook.
     */
    enum class TabIndex {
        Appearance = 0,         ///< Theme and transition configuration
        DesktopIntegration = 1, ///< Dolphin, GNOME/Nemo, and desktop shortcut integrations
        Performance = 2,        ///< Compiler level (-O2/-O3), prefetching, and standby
        Extensions = 3,         ///< Interactive functional module catalog
        Extra = 4               ///< Language and general preferences
    };

    /**
     * @brief Constructs the settings dialog.
     * @param parent Optional parent widget.
     * @param initialTab Active tab when the dialog is first opened.
     */
    explicit SettingsDialog(QWidget *parent = nullptr, TabIndex initialTab = TabIndex::Appearance);

    /**
     * @brief Destructor.
     */
    ~SettingsDialog() override;

    /**
     * @brief Switches the active view to the specified tab.
     * @param tab Target tab to activate.
     */
    void setCurrentTab(TabIndex tab);

signals:
    /**
     * @brief Emitted when the user changes theme preset or transition style.
     */
    void themeChanged();

public slots:
    /**
     * @brief Refreshes desktop integration statuses across tabs.
     */
    void refreshStatuses();

    /**
     * @brief Initiates in-place recompilation via the floating build console.
     */
    void startRecompilation();

    /**
     * @brief Reverts pending build and module changes back to active configuration.
     */
    void onRevertPendingClicked();

private slots:
    /**
     * @brief Evaluates whether pending build changes exist and updates banner visibility.
     */
    void updatePendingBanner();

private:
    void setupUi();
    bool hasPendingBuildChanges() const;

    QTabWidget *m_tabWidget = nullptr;

    SettingsAppearanceTab *m_appearanceTab = nullptr;
    SettingsIntegrationTab *m_integrationTab = nullptr;
    SettingsPerformanceTab *m_performanceTab = nullptr;
    SettingsExtensionsTab *m_extensionsTab = nullptr;
    SettingsExtraTab *m_extraTab = nullptr;

    // Initial build config snapshot for pending changes tracking
    bool m_initialPdfStatic = false;
    bool m_initialArchiveStatic = false;
    bool m_initialOfficeStatic = false;
    bool m_initialEnableOffice = false;

    // Pending Recompile Notification Banner & Floating Console
    QFrame *m_bannerPending = nullptr;
    QLabel *m_lblBannerMsg = nullptr;
    QPushButton *m_btnBannerRecompile = nullptr;
    QPushButton *m_btnBannerRevert = nullptr;
    BuildConsoleDialog *m_buildConsoleDialog = nullptr;
};
