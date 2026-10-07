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
 * @brief Settings notebook tab managing compiler optimization level (-O2/-O3),
 *        the Compiler Wizard, adjacent file pre-caching, and background standby duration.
 */
class SettingsPerformanceTab : public QWidget {
    Q_OBJECT

public:
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

signals:
    /**
     * @brief Emitted when compiler options change, requiring recompilation.
     */
    void pendingStateChanged();

    /**
     * @brief Emitted when the Compiler Wizard triggers an immediate rebuild.
     */
    void recompileRequested();

private slots:
    /**
     * @brief Opens the interactive Compiler Wizard dialog if CPU meets x86-64-v3 requirements.
     */
    void openCompilerWizard();

private:
    void setupUi();
    void applyOptSegmentedStyle(bool isO3);

    QString m_optLevel;
    QString m_initialOptLevel;
    QString m_initialCustomFlags;

    QPushButton *m_btnOptO2 = nullptr;
    QPushButton *m_btnOptO3 = nullptr;

    QCheckBox *m_chkEnablePreload = nullptr;

    QPushButton *m_btnStandbyMinus = nullptr;
    QPushButton *m_btnStandbyPlus = nullptr;
    QLabel *m_lblStandbyValue = nullptr;
    int m_standbyMinutes = 0;
};
