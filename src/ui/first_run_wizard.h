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
        Page_DesktopIntegration = 1,
        Page_CpuOptimization = 2,
        Page_Modules = 3,
        Page_Appearance = 4,
        Page_Behavior = 5,
        Page_Summary = 6
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
    QWizardPage *createDesktopIntegrationPage();
    QWizardPage *createCpuOptimizationPage();
    QWizardPage *createModulesPage();
    QWizardPage *createAppearancePage();
    QWizardPage *createBehaviorPage();
    QWizardPage *createSummaryPage();

    // Integration & Shortcut selections
    QRadioButton *m_radioSpaceShortcut = nullptr;
    QRadioButton *m_radioCtrlSpaceShortcut = nullptr;
    QCheckBox *m_chkFileManagerMenu = nullptr;
    QCheckBox *m_chkGlobalShortcut = nullptr;

    // CPU Performance Profile
    QRadioButton *m_radioCpuDesktop = nullptr;
    QRadioButton *m_radioCpuLaptop = nullptr;
    QRadioButton *m_radioCpuBalanced = nullptr;
    QPushButton *m_btnCpuCustomize = nullptr;

    // Functional Modules
    QRadioButton *m_radioModuleMinimal = nullptr;
    QRadioButton *m_radioModuleRecommended = nullptr;
    QRadioButton *m_radioModuleEnginesOnly = nullptr;
    QRadioButton *m_radioModuleAll = nullptr;
    QRadioButton *m_radioModuleCustom = nullptr;
    QPushButton *m_btnModuleCustomize = nullptr;

    // Appearance & UI
    QComboBox *m_comboTheme = nullptr;
    QCheckBox *m_chkFilmstrip = nullptr;

    // Navigation & Behavior
    QCheckBox *m_chkCloseOnSpaceOrEsc = nullptr;
    QCheckBox *m_chkRememberGeometry = nullptr;
    QCheckBox *m_chkDesktopShortcut = nullptr;

    // Summary Page label
    QLabel *m_lblSummaryDetails = nullptr;

    void updateSummaryText();
};
