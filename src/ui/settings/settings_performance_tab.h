/**
 * @file settings_performance_tab.h
 * @brief Performance, compiler tuning, cache prefetching, and daemon standby configuration page.
 */

#pragma once

#include <QWidget>

class QPushButton;
class QCheckBox;
class QLabel;

/**
 * @class SettingsPerformanceTab
 * @brief Settings notebook tab managing hardware profiles (Desktop, Balanced, Laptop, Custom),
 *        the Compiler Wizard, adjacent file pre-caching, and background standby duration.
 */
class SettingsPerformanceTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @enum Profile
     * @brief System hardware optimization profiles.
     */
    enum class Profile {
        Desktop = 0,    ///< Potente / Desktop: Everything compiled from source with extreme flags
        Balanced = 1,   ///< Bilanciato: Everything from system precompiled packages
        Custom = 2      ///< Fine-grained custom compiler flags via Compiler Wizard
    };

    /**
     * @brief Constructs the performance settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsPerformanceTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsPerformanceTab() override = default;

    /**
     * @brief Checks if compiler options have changed from active build configuration.
     * @return True if pending recompile changes exist.
     */
    bool hasPendingChanges() const;

    /**
     * @brief Reverts compiler options back to active build configuration.
     */
    void revertPendingChanges();

    /**
     * @brief Synchronizes initial state after a successful compilation.
     */
    void onBuildSucceeded();

    /**
     * @brief Returns chosen optimization level ("O2" or "O3").
     */
    QString optimizationLevel() const { return m_optLevel; }

    /**
     * @brief Returns current active profile.
     */
    Profile activeProfile() const { return m_activeProfile; }

signals:
    /**
     * @brief Emitted when compiler options change, requiring recompilation.
     */
    void pendingStateChanged();

    /**
     * @brief Emitted when the Compiler Wizard triggers an immediate rebuild.
     * @param target Specific CMake target to build (empty for full build).
     */
    void recompileRequested(const QString &target = QString());

    /**
     * @brief Emitted when the user requests manual plugin management in Extensions tab.
     */
    void openExtensionsTabRequested();

private slots:
    /**
     * @brief Selects one of the hardware profiles.
     * @param profile Target profile.
     */
    void selectProfile(Profile profile);

    /**
     * @brief Opens the interactive Compiler Wizard dialog.
     */
    void openCompilerWizard();

private:
    void setupUi();
    void updateProfileButtons(Profile active);
    Profile detectActiveProfile() const;

    Profile m_activeProfile = Profile::Balanced;
    Profile m_initialProfile = Profile::Balanced;

    QString m_optLevel;
    QString m_initialOptLevel;
    QString m_initialCpuProfile;
    QString m_initialCustomFlags;

    QPushButton *m_btnProfileDesktop = nullptr;
    QPushButton *m_btnProfileBalanced = nullptr;
    QPushButton *m_btnProfileCustom = nullptr;

    QLabel *m_lblProfileDesc = nullptr;

    QCheckBox *m_chkEnablePreload = nullptr;

    QPushButton *m_btnStandbyMinus = nullptr;
    QPushButton *m_btnStandbyPlus = nullptr;
    QLabel *m_lblStandbyValue = nullptr;
    int m_standbyMinutes = 0;

    QCheckBox *m_chkEnableGpu = nullptr;
    QLabel *m_lblGpuRenderer = nullptr;
    QLabel *m_lblGpuDriver = nullptr;
    QLabel *m_lblGpuVramNotice = nullptr;
};
