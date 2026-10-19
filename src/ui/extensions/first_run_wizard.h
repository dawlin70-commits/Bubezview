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
    QWizardPage *createWelcomePage();
    QWizardPage *createCpuOptimizationPage();
    QWizardPage *createModulesPage();
    QWizardPage *createAppearancePage();
    QWizardPage *createBehaviorPage();
    QWizardPage *createSummaryPage();

    // CPU Performance Profile
    QRadioButton *m_radioCpuDesktop = nullptr;
    QRadioButton *m_radioCpuBalanced = nullptr;
    QLabel *m_lblCpuDetectedNotice = nullptr;

    // Functional Modules & Engine Presets (Proposal 3)
    QRadioButton *m_radioPresetFull = nullptr;
    QRadioButton *m_radioPresetDocs = nullptr;
    QRadioButton *m_radioPresetMedia = nullptr;
    QRadioButton *m_radioPresetMinimal = nullptr;
    QRadioButton *m_radioPresetCustom = nullptr;
    QLabel *m_lblPresetDesc = nullptr;

    // Granular Module Toggles (Hierarchical Document, Media & Archive structure)
    QCheckBox *m_chkModulePdf = nullptr;
    QCheckBox *m_chkSubOcr = nullptr;
    QCheckBox *m_chkSubQpdf = nullptr;

    QCheckBox *m_chkModuleOffice = nullptr;

    QCheckBox *m_chkModuleMedia = nullptr;
    QCheckBox *m_chkSubEqualizer = nullptr;

    QCheckBox *m_chkModuleArchive = nullptr;
    QCheckBox *m_chkSubSevenZip = nullptr;

    // Status Badges
    QLabel *m_badgePdf = nullptr;
    QLabel *m_badgeOcr = nullptr;
    QLabel *m_badgeQpdf = nullptr;
    QLabel *m_badgeOffice = nullptr;
    QLabel *m_badgeMedia = nullptr;
    QLabel *m_badgeEqualizer = nullptr;
    QLabel *m_badgeArchive = nullptr;
    QLabel *m_badgeSevenZip = nullptr;

    QPushButton *m_btnConfigureExtensions = nullptr;
    QLabel *m_lblModulesHint = nullptr;

    bool m_syncingModules = false;

    /**
     * @brief Applies a functional preset to the module checkboxes.
     * @param presetIndex 0=Full, 1=Docs/Office, 2=Media, 3=Minimal.
     */
    void applyPreset(int presetIndex);

    /**
     * @brief Evaluates the current state of module checkboxes to dynamically select the corresponding preset.
     */
    void evaluatePresetFromCheckboxes();

    /**
     * @brief Refreshes the runtime status indicators next to each module.
     */
    void updateModuleStatusBadges();

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

    void updateSummaryText();
    void updateModuleOptionTexts();
};
