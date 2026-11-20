/**
 * @file first_run_wizard.h
 * @brief Guided onboarding and setup wizard for BubezView.
 */

#pragma once

#include "dialogs/frameless_dialog.h"
#include "theme_manager.h"

#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QStackedWidget>

class TopBarPreviewWidget;

/**
 * @class FirstRunWizard
 * @brief Guided onboarding wizard presented on first launch or on-demand.
 *
 * 4 Streamlined Pages:
 * - **Page 0 (`Page_Welcome`)**: Welcome & Appearance (Pre-selected system language,
 *   interactive BubezView top bar preview with live theme switching: Zinc Gray, Dark Slate, Modern Light).
 * - **Page 1 (`Page_Spacebar`)**: Desktop Integration & Spacebar test (Automatic test execution
 *   on entry; explanation guide if unsupported; desktop integration checked by default;
 *   LibreOffice detected automatically on demand).
 * - **Page 2 (`Page_Performance`)**: 3 execution strategy profiles (System Packages,
 *   Missing Only, Maximum Performance) and fast restart standby selection (default 30 minutes).
 *   All modules are ready on-demand and other performance defaults applied silently.
 * - **Page 3 (`Page_Summary`)**: Configuration summary and dynamic action preview showing
 *   selected options and any local components queued for compilation before launch.
 */
class FirstRunWizard : public FramelessDialog
{
    Q_OBJECT

public:
    enum PageId {
        Page_Welcome = 0,     ///< Welcome & Appearance (Language & 3 Themes with live preview)
        Page_Spacebar = 1,    ///< Desktop Integration & Spacebar Key Test
        Page_Performance = 2, ///< Execution Strategy (3 Profiles) & Fast Restart Standby
        Page_Summary = 3      ///< Final Configuration Summary & Launch Confirmation
    };

    explicit FirstRunWizard(QWidget *parent = nullptr);
    ~FirstRunWizard() override = default;

    void accept() override;

    /**
     * @brief Restores initial theme and desktop integration state when the wizard is cancelled.
     */
    void reject() override;

    void next();
    void back();
    void goToPage(int id);
    int currentId() const;
    QList<int> pageIds() const { return {Page_Welcome, Page_Spacebar, Page_Performance, Page_Summary}; }
    void updateThemeStyles() override;

    /**
     * @brief Computes the list of user-space components requiring local compilation
     *        based on the currently selected compilation strategy tier.
     * @return List of component identifiers to build; empty if no compilation is needed.
     */
    QStringList pendingBuildComponents() const;

private:
    QWidget *createWelcomePage();
    QWidget *createSpacebarPage();
    QWidget *createPerformancePage();
    QWidget *createSummaryPage();
    void refreshSummaryPage();
    void updateNextButtonStyle();

    // Stacked navigation
    QStackedWidget *m_pageStack = nullptr;
    QPushButton *m_btnBack = nullptr;
    QPushButton *m_btnNext = nullptr;
    QPushButton *m_btnCancel = nullptr;

    // Page 0: Welcome & Appearance
    QComboBox *m_comboLanguage = nullptr;
    TopBarPreviewWidget *m_barPreview = nullptr;
    QPushButton *m_btnThemeZinc = nullptr;
    QPushButton *m_btnThemeDarkSlate = nullptr;
    QPushButton *m_btnThemeModernLight = nullptr;
    QButtonGroup *m_themeGroup = nullptr;

    // Page 1: Spacebar & Desktop Integration
    QLabel *m_lblTestStatus = nullptr;
    QWidget *m_cardFallback = nullptr;
    QPushButton *m_btnCopyCommand = nullptr;
    QPushButton *m_btnGuide = nullptr;
    QCheckBox *m_chkDesktopIntegration = nullptr;

    // Page 2: Performance & Compilation Strategy
    int m_selectedTier = 1;    ///< 0 = System, 1 = Missing Only, 2 = Max Performance
    int m_recommendedTier = 1; ///< Hardware-recommended tier computed from CpuInfo::recommendedProfile()
    QPushButton *m_btnTierSystem = nullptr;
    QPushButton *m_btnTierMissing = nullptr;
    QPushButton *m_btnTierMax = nullptr;
    QButtonGroup *m_tierGroup = nullptr;
    QLabel *m_lblTierDesc = nullptr;
    QComboBox *m_comboStandby = nullptr;

    // Page 3: Summary & Launch Confirmation
    QLabel *m_lblSumAppearance = nullptr;
    QLabel *m_lblSumDesktop = nullptr;
    QLabel *m_lblSumHardware = nullptr;
    QLabel *m_lblSumStrategy = nullptr;
    QLabel *m_lblSumStandby = nullptr;
    QWidget *m_cardSumAction = nullptr;
    QLabel *m_lblSumActionTitle = nullptr;
    QLabel *m_lblSumActionDesc = nullptr;

    // Initial state snapshot for clean rollback on reject()
    ThemePreset m_initialPreset = ThemePreset::ZincGray;
    bool m_initialDesktopIntegration = false;
    bool m_spacebarTestModifiedSystem = false;

    void selectTheme(const QString &themeCode);
    void styleThemeButton(QPushButton *btn, bool active);
    void selectCompilationTier(int tier);
    void styleTierButton(QPushButton *btn, bool active);
    void runSpacebarAutoTest();
};
