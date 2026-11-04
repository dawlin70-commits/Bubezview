/**
 * @file first_run_wizard.h
 * @brief Guided onboarding and setup wizard for BubezView.
 */

#pragma once

#include <QWizard>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>

/**
 * @class FirstRunWizard
 * @brief Guided onboarding wizard presented on first launch or on-demand.
 *
 * - **Page 0 (`Page_Welcome`)**: Hero welcome banner with official BubezView emblem,
 *   detected hardware/desktop pills, and live-preview visual theme cards.
 * - **Page 1 (`Page_Spacebar`)**: Dedicated Spacebar preview test screen with single test
 *   button and automatic trigger script copy fallback.
 * - **Page 2 (`Page_Modules`)**: Additional options: on-demand modules, CPU profile,
 *   and window behavior memory.
 */
class FirstRunWizard : public QWizard
{
    Q_OBJECT

public:
    /**
     * @enum PageId
     * @brief Numeric identifier for each sequential step in the setup wizard.
     */
    enum PageId {
        Page_Welcome = 0,  ///< Welcome Hero and Live Theme selection
        Page_Spacebar = 1, ///< Dedicated Spacebar test and trigger script fallback
        Page_Modules = 2   ///< Additional Options: On-demand modules, CPU profile, behavior
    };

    /**
     * @brief Constructs the first-run wizard dialog.
     * @param parent Optional parent widget.
     */
    explicit FirstRunWizard(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~FirstRunWizard() override = default;

    /**
     * @brief Saves all configured selections to persistent application settings (QSettings)
     *        and activates desktop file manager integrations.
     */
    void accept() override;

private:
    /** @brief Creates Page 0: Hero welcome and Live Theme picker. */
    QWizardPage *createWelcomePage();
    /** @brief Creates Page 1: Dedicated Spacebar test and trigger script fallback. */
    QWizardPage *createSpacebarPage();
    /** @brief Creates Page 2: Additional options (modules, CPU profile, behavior). */
    QWizardPage *createModulesPage();

    // Live Theme Selection & Filmstrip (Page 0)
    QComboBox *m_comboTheme = nullptr;
    QButtonGroup *m_themeCardGroup = nullptr;
    QPushButton *m_btnThemeDarkSlate = nullptr;
    QPushButton *m_btnThemeBreezeDark = nullptr;
    QPushButton *m_btnThemeModernLight = nullptr;
    QPushButton *m_btnThemeBreezeLight = nullptr;
    QCheckBox *m_chkFilmstrip = nullptr;

    // Instant Preview Spacebar Test & Trigger (Page 1)
    QPushButton *m_btnTestShortcut = nullptr;
    QLabel *m_lblTestResult = nullptr;
    QWidget *m_triggerBox = nullptr;
    QLabel *m_lblTriggerHelp = nullptr;
    QPushButton *m_btnCopyTrigger = nullptr;
    bool m_useTriggerFallback = false;

    // Functional Modules & Engine Presets (Page 2)
    QRadioButton *m_radioPresetMinimal = nullptr;
    QRadioButton *m_radioPresetFull = nullptr;
    QRadioButton *m_radioPresetCustom = nullptr;

    // 5 Category Extension Toggles (Page 2)
    QCheckBox *m_chkCatDocs = nullptr;
    QCheckBox *m_chkCatImages = nullptr;
    QCheckBox *m_chkCatVideo = nullptr;
    QCheckBox *m_chkCatAudio = nullptr;
    QCheckBox *m_chkCatArchives = nullptr;

    QLabel *m_lblDynamicSummary = nullptr;
    QPushButton *m_btnConfigureExtensions = nullptr;

    // CPU Performance Profile (Page 2)
    QRadioButton *m_radioCpuDesktop = nullptr;
    QRadioButton *m_radioCpuBalanced = nullptr;
    QLabel *m_lblCpuDetectedNotice = nullptr;

    // Navigation & Cache Behavior (Page 2)
    QCheckBox *m_chkCloseOnEsc = nullptr;
    QCheckBox *m_chkRememberGeometry = nullptr;
    QCheckBox *m_chkPreloadCache = nullptr;
    QCheckBox *m_chkStandbyMode = nullptr;

    bool m_syncingModules = false;

    /**
     * @brief Applies a functional preset to the module category checkboxes.
     * @param presetIndex 0=Full Suite, 1=Minimal & On-Demand, 2=Custom.
     */
    void applyPreset(int presetIndex);

    /**
     * @brief Evaluates the current state of category checkboxes to select the corresponding preset radio.
     */
    void evaluatePresetFromCheckboxes();

    /**
     * @brief Updates the real-time dynamic summary card in the modules selection page.
     */
    void updateDynamicSummary();

    /**
     * @brief Updates preset labels according to the selected CPU compilation profile.
     */
    void updateModuleOptionTexts();

    /**
     * @brief Synchronizes visual styling of the interactive theme cards and applies live preview.
     * @param themeCode Theme identifier ("dark_slate", "breeze_dark", "modern_light", "breeze_light").
     */
    void selectLiveTheme(const QString &themeCode);

    /**
     * @brief Automatically inspects the desktop environment and runs the spacebar test immediately upon entering Page_Spacebar.
     */
    void runAutoSpacebarCheck();
};
