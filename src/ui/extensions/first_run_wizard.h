/**
 * @file first_run_wizard.h
 * @brief Guided onboarding and setup wizard for BubezView.
 */

#pragma once

#include "dialogs/frameless_dialog.h"

#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
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
 * - **Page 2 (`Page_Modules`)**: Simple module suite choices (Lightweight On-Demand recommended
 *   vs simple compartment buttons for PDF, Audio, OCR, Archives).
 * - **Page 3 (`Page_Performance`)**: 4-tier compilation slider and fast restart standby selection
 *   (default 30 minutes). All other performance defaults applied silently.
 */
class FirstRunWizard : public FramelessDialog
{
    Q_OBJECT

public:
    enum PageId {
        Page_Welcome = 0,     ///< Welcome & Appearance (Language & 3 Themes with live preview)
        Page_Spacebar = 1,    ///< Desktop Integration & Spacebar Key Test
        Page_Modules = 2,     ///< Functional Modules (Simple direct buttons)
        Page_Performance = 3  ///< 4-Tier Compilation Slider & Fast Restart Standby
    };

    explicit FirstRunWizard(QWidget *parent = nullptr);
    ~FirstRunWizard() override = default;

    void accept() override;
    void next();
    void back();
    void goToPage(int id);
    int currentId() const;
    QList<int> pageIds() const { return {Page_Welcome, Page_Spacebar, Page_Modules, Page_Performance}; }
    void updateThemeStyles() override;

private:
    QWidget *createWelcomePage();
    QWidget *createSpacebarPage();
    QWidget *createModulesPage();
    QWidget *createPerformancePage();

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

    // Page 2: Modules
    QRadioButton *m_radioOnDemand = nullptr;
    QRadioButton *m_radioPrepareNow = nullptr;
    QWidget *m_panelModulesNow = nullptr;
    QCheckBox *m_chkModDocs = nullptr;
    QCheckBox *m_chkModMedia = nullptr;
    QCheckBox *m_chkModArchives = nullptr;
    QCheckBox *m_chkModImages = nullptr;

    // Page 3: Performance & Compilation
    int m_selectedTier = 1; // 0 = System, 1 = CPU Native (Recommended), 2 = Full
    QPushButton *m_btnTierSystem = nullptr;
    QPushButton *m_btnTierCpu = nullptr;
    QPushButton *m_btnTierFull = nullptr;
    QButtonGroup *m_tierGroup = nullptr;
    QLabel *m_lblTierDesc = nullptr;
    QComboBox *m_comboStandby = nullptr;

    void selectTheme(const QString &themeCode);
    void styleThemeButton(QPushButton *btn, bool active);
    void selectCompilationTier(int tier);
    void styleTierButton(QPushButton *btn, bool active);
    void runSpacebarAutoTest();
};
