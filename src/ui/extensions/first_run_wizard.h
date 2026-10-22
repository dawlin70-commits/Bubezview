/**
 * @file first_run_wizard.h
 * @brief First-run setup and configuration wizard for BubezView.
 */

#pragma once

#include <QWizard>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @class FirstRunWizard
 * @brief Guided onboarding wizard presented on first launch or on-demand to configure desktop integrations, shortcuts, office preview engines, themes, and navigation behavior.
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
        Page_Welcome = 0,
        Page_CpuOptimization = 1,
        Page_Modules = 2,
        Page_Appearance = 3,
        Page_Behavior = 4,
        Page_Summary = 5
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
     * @brief Saves all configured selections to persistent application settings (QSettings) and activates desktop integrations.
     */
    void accept() override;

private:
    /** @brief Creates the initial welcome and hardware summary page. */
    QWizardPage *createWelcomePage();
    /** @brief Creates the CPU microarchitecture and build profile page. */
    QWizardPage *createCpuOptimizationPage();
    /** @brief Creates the unified module presets and functional categories page. */
    QWizardPage *createModulesPage();
    /** @brief Creates the visual theme and filmstrip configuration page. */
    QWizardPage *createAppearancePage();
    /** @brief Creates the keyboard shortcut, dismissal, and cache behavior page. */
    QWizardPage *createBehaviorPage();
    /** @brief Creates the final review and confirmation summary page. */
    QWizardPage *createSummaryPage();

    // CPU Performance Profile
    QRadioButton *m_radioCpuDesktop = nullptr;
    QRadioButton *m_radioCpuBalanced = nullptr;
    QLabel *m_lblCpuDetectedNotice = nullptr;

    // Functional Modules & Engine Presets (Full, Minimal, Custom)
    QRadioButton *m_radioPresetFull = nullptr;
    QRadioButton *m_radioPresetMinimal = nullptr;
    QRadioButton *m_radioPresetCustom = nullptr;

    // 5 Category Extension Toggles
    QCheckBox *m_chkCatDocs = nullptr;
    QCheckBox *m_chkCatImages = nullptr;
    QCheckBox *m_chkCatVideo = nullptr;
    QCheckBox *m_chkCatAudio = nullptr;
    QCheckBox *m_chkCatArchives = nullptr;

    QLabel *m_lblDynamicSummary = nullptr;
    QPushButton *m_btnConfigureExtensions = nullptr;

    bool m_syncingModules = false;

    /**
     * @brief Applies a functional preset to the module category checkboxes.
     * @param presetIndex 0=Full Suite, 1=Minimal Core, 2=Custom.
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

    // Appearance & UI
    QComboBox *m_comboTheme = nullptr;
    QCheckBox *m_chkFilmstrip = nullptr;

    // Navigation, Behavior & Shortcuts
    QPushButton *m_btnTestShortcut = nullptr;
    QLabel *m_lblTestResult = nullptr;
    QCheckBox *m_chkUseCtrlSpace = nullptr;
    QLabel *m_lblTriggerHelp = nullptr;
    QCheckBox *m_chkCloseOnEsc = nullptr;
    QCheckBox *m_chkRememberGeometry = nullptr;
    QCheckBox *m_chkDesktopShortcut = nullptr;
    QCheckBox *m_chkPreloadCache = nullptr;
    QCheckBox *m_chkStandbyMode = nullptr;

    // Summary Page label
    QLabel *m_lblSummaryDetails = nullptr;

    /** @brief Populates the final confirmation summary page table. */
    void updateSummaryText();
    /** @brief Updates preset labels according to the selected CPU compilation profile. */
    void updateModuleOptionTexts();
};
