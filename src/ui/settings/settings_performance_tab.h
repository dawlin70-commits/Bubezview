/**
 * @file settings_performance_tab.h
 * @brief Performance, compiler tuning, cache prefetching, and daemon standby configuration page.
 */

#pragma once

#include <QWidget>

class QPushButton;
class QComboBox;
class QLabel;
class QGroupBox;
class CapsuleToggleSwitch;

/**
 * @class SettingsPerformanceTab
 * @brief Settings notebook tab managing hardware profiles (Desktop, Balanced, Custom),
 *        the Compiler Wizard, adjacent file pre-caching, GPU acceleration, and background standby duration.
 */
class SettingsPerformanceTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @enum Profile
     * @brief Compilation scope levels governing system packages vs source builds.
     */
    enum class Profile {
        SystemPackages = 0, ///< Level 1: System Packages (No Compilation / Zero build)
        MissingOnly = 1,    ///< Level 2: Missing Components Only
        AllStable = 2,      ///< Level 3: All Stable Components
        AllSource = 3,      ///< Level 4: All From Source (Including Unrecommended)
        Custom = 4          ///< Fine-grained custom compiler flags via Compiler Wizard
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
     * @brief Updates Liquid Glass card styling and controls to match the active theme.
     */
    void updateThemeStyles();

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
    void selectProfile(Profile profile);
    void openCompilerWizard();
    void onProfileComboChanged(int index);
    void onStandbyComboChanged(int index);

private:
    void setupUi();
    QWidget *createSettingRow(const QString &title, QWidget *control, QWidget *parent, const QString &subtitle = QString());
    Profile detectActiveProfile() const;
    void syncProfileUi();

    Profile m_activeProfile = Profile::MissingOnly;
    Profile m_initialProfile = Profile::MissingOnly;

    QString m_optLevel;
    QString m_initialOptLevel;
    QString m_initialCpuProfile;
    QString m_initialCustomFlags;

    // Card 1: Component Optimization
    QGroupBox *m_grpCompiler = nullptr;
    QComboBox *m_comboProfile = nullptr;
    QPushButton *m_btnWizard = nullptr;
    QLabel *m_lblOptHint = nullptr;

    // Card 2: Cache & GPU Acceleration
    QGroupBox *m_grpAcceleration = nullptr;
    CapsuleToggleSwitch *m_switchPreload = nullptr;
    CapsuleToggleSwitch *m_switchGpu = nullptr;

    // Card 3: Background Standby
    QGroupBox *m_grpStandby = nullptr;
    QComboBox *m_comboStandby = nullptr;
    QPushButton *m_btnResetNotices = nullptr;
};
