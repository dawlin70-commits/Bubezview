/**
 * @file first_run_wizard.cpp
 * @brief Implementation of the FirstRunWizard dialog.
 */

#include "first_run_wizard.h"
#include "desktop_integrator.h"
#include "build_config.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include "cpu_info.h"
#include "compiler_wizard_dialog.h"
#include "build_options_dialog.h"
#include "build_console_dialog.h"
#include "package_manager_helper.h"
#include "settings_dialog.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QStyle>
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDialog>
#include <QKeyEvent>
#include <QTimer>

namespace {

/**
 * @class SpaceShortcutTestDialog
 * @brief Interactive modal dialog that captures key presses to verify spacebar responsiveness.
 */
class SpaceShortcutTestDialog : public QDialog {
public:
    explicit SpaceShortcutTestDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_spaceDetected(false)
    {
        setWindowTitle(FirstRunWizard::tr("Test Spacebar Shortcut"));
        resize(420, 180);
        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(14);

        auto *lblTitle = new QLabel(FirstRunWizard::tr("<h3>🧪 Testing Spacebar Key</h3>"), this);
        lblTitle->setAlignment(Qt::AlignCenter);
        layout->addWidget(lblTitle);

        m_lblStatus = new QLabel(
            FirstRunWizard::tr("Please press the <b>Spacebar</b> on your keyboard now to verify key capture."),
            this
        );
        m_lblStatus->setAlignment(Qt::AlignCenter);
        m_lblStatus->setWordWrap(true);
        layout->addWidget(m_lblStatus);

        auto *hBox = new QHBoxLayout();
        auto *btnCancel = new QPushButton(FirstRunWizard::tr("Cancel / Having Issues"), this);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        hBox->addStretch();
        hBox->addWidget(btnCancel);
        layout->addLayout(hBox);

        setFocusPolicy(Qt::StrongFocus);
    }

    bool spaceDetected() const { return m_spaceDetected; }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            m_spaceDetected = true;
            m_lblStatus->setText(FirstRunWizard::tr("<b style='color:#4ade80;'>✅ Spacebar detected successfully!</b>"));
            QTimer::singleShot(400, this, &QDialog::accept);
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    QLabel *m_lblStatus = nullptr;
    bool m_spaceDetected = false;
};

} // namespace

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("BubezView Setup Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    resize(700, 540);

    // Register pages
    setPage(Page_Welcome, createWelcomePage());
    setPage(Page_CpuOptimization, createCpuOptimizationPage());
    setPage(Page_Modules, createModulesPage());
    setPage(Page_Appearance, createAppearancePage());
    setPage(Page_Behavior, createBehaviorPage());
    setPage(Page_Summary, createSummaryPage());

    setStartId(Page_Welcome);

    // Custom button labels
    setButtonText(QWizard::NextButton, tr("Next >"));
    setButtonText(QWizard::BackButton, tr("< Back"));
    setButtonText(QWizard::FinishButton, tr("Finish & Launch BubezView"));
    setButtonText(QWizard::CancelButton, tr("Cancel"));

    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        if (id == Page_Summary) {
            updateSummaryText();
        }
    });
}

QWizardPage *FirstRunWizard::createWelcomePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Welcome to BubezView"));
    page->setSubTitle(tr("High-performance native file viewer and instant preview tool for Linux."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(16);

    auto *lblIcon = new QLabel(page);
    lblIcon->setAlignment(Qt::AlignCenter);
    lblIcon->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Document, QColor("#3b82f6")).pixmap(64, 64));
    layout->addWidget(lblIcon);

    auto *lblDesc = new QLabel(
        tr("This wizard will help you configure your preferred processor optimizations, "
           "document modules, visual appearance, and navigation behavior.<br><br>"
           "You can complete this setup in under a minute, and change any preference later "
           "from the Settings dialog or directly from application menus."),
        page
    );
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("font-size: 13px; line-height: 1.4;");
    layout->addWidget(lblDesc);

    // Detected Desktop Environment Card
    auto *groupEnv = new QGroupBox(tr("Detected Environment"), page);
    auto *envLayout = new QVBoxLayout(groupEnv);

    QString deName = DesktopIntegrator::desktopName();
    auto *lblDe = new QLabel(
        tr("<b>Desktop Environment:</b> %1<br>"
           "<b>Platform:</b> Native C++ / Qt6 engine (lightweight, hardware-accelerated)")
        .arg(deName),
        groupEnv
    );
    lblDe->setTextFormat(Qt::RichText);
    envLayout->addWidget(lblDe);

    layout->addWidget(groupEnv);
    layout->addStretch();

    return page;
}

QWizardPage *FirstRunWizard::createCpuOptimizationPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Processor & Hardware Optimization"));
    page->setSubTitle(tr("Select the performance profile tailored for your computer."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    // Host CPU Card
    auto *groupCpu = new QGroupBox(tr("Detected Host Processor"), page);
    auto *cpuLayout = new QVBoxLayout(groupCpu);
    QString cpuModel = CpuInfo::modelName();
    if (cpuModel.isEmpty()) {
        cpuModel = tr("Generic x86-64 Processor");
    }
    QString tier = CpuInfo::microarchitectureTier();
    auto *lblCpu = new QLabel(
        tr("<b>Model:</b> %1<br><b>Architecture tier:</b> x86-64-%2 (%3 logical cores)")
            .arg(cpuModel, tier, QString::number(CpuInfo::logicalCores())),
        groupCpu
    );
    lblCpu->setTextFormat(Qt::RichText);
    cpuLayout->addWidget(lblCpu);
    layout->addWidget(groupCpu);

    // Profile Selection
    auto *groupProfiles = new QGroupBox(tr("Performance Profile"), page);
    auto *profilesLayout = new QVBoxLayout(groupProfiles);
    profilesLayout->setSpacing(10);

    m_radioCpuDesktop = new QRadioButton(
        tr("🚀 High Performance (Everything Compiled from Source)\n"
           "   Maximum speed. All engines compiled with aggressive host optimizations (-O3, -march=native, LTO)."),
        groupProfiles
    );

    m_radioCpuBalanced = new QRadioButton(
        tr("📦 Balanced (System Packaged Dependencies)\n"
           "   Daily use. Uses precompiled system libraries from your distribution with zero compile wait times."),
        groupProfiles
    );

    profilesLayout->addWidget(m_radioCpuDesktop);
    profilesLayout->addWidget(m_radioCpuBalanced);

    connect(m_radioCpuDesktop, &QRadioButton::toggled, this, [this](bool) {
        updateModuleOptionTexts();
    });
    connect(m_radioCpuBalanced, &QRadioButton::toggled, this, [this](bool) {
        updateModuleOptionTexts();
    });

    // Intelligent auto-detection of optimal profile
    QString rec = CpuInfo::recommendedProfile();
    if (rec == QLatin1String("native")) {
        m_radioCpuDesktop->setChecked(true);
    } else {
        m_radioCpuBalanced->setChecked(true);
    }

    // Concise informative message regarding auto-detection & recompilation
    m_lblCpuDetectedNotice = new QLabel(
        tr("<i>Optimal profile detected automatically. Changing it will trigger a recompile (also editable later in Settings).</i>"),
        groupProfiles
    );
    m_lblCpuDetectedNotice->setStyleSheet("color: #64748b; font-size: 12px; margin-top: 4px;");
    m_lblCpuDetectedNotice->setWordWrap(true);
    profilesLayout->addWidget(m_lblCpuDetectedNotice);

    layout->addWidget(groupProfiles);
    layout->addStretch();

    return page;
}

QWizardPage *FirstRunWizard::createModulesPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Engines & Functional Modules"));
    page->setSubTitle(tr("Select which document readers and preview engines you want to enable."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    auto *groupModules = new QGroupBox(tr("Module Setup Preset"), page);
    auto *modulesLayout = new QVBoxLayout(groupModules);
    modulesLayout->setSpacing(10);

    m_radioModuleAll = new QRadioButton(groupModules);
    m_radioModuleEnginesOnly = new QRadioButton(groupModules);
    m_radioModuleDefault = new QRadioButton(
        tr("⭐ Keep Existing Setup (No Installation or Compilation)\n"
           "   Does not install or compile anything. Launches BubezView immediately in its current state."),
        groupModules
    );
    m_radioModuleCustom = new QRadioButton(
        tr("⚙️ Customize Individually\n"
           "   Choose each component and engine manually."),
        groupModules
    );

    // Initial label text setup based on currently selected profile
    updateModuleOptionTexts();

    // Default to Complete Suite so new users automatically get a full working setup
    m_radioModuleAll->setChecked(true);

    modulesLayout->addWidget(m_radioModuleAll);
    modulesLayout->addWidget(m_radioModuleEnginesOnly);
    modulesLayout->addWidget(m_radioModuleDefault);
    modulesLayout->addWidget(m_radioModuleCustom);

    // Customization button and note
    m_btnConfigureExtensions = new QPushButton(tr("⚙️ Configure Extensions & Engines..."), groupModules);
    m_btnConfigureExtensions->setCursor(Qt::PointingHandCursor);
    m_btnConfigureExtensions->setVisible(false);
    modulesLayout->addWidget(m_btnConfigureExtensions);

    m_lblModulesHint = new QLabel(
        tr("ℹ️ You can also configure or recompile individual extensions at any time from Settings → Extensions."),
        groupModules
    );
    m_lblModulesHint->setStyleSheet("color: #64748b; font-size: 12px; margin-top: 4px;");
    m_lblModulesHint->setWordWrap(true);
    m_lblModulesHint->setVisible(false);
    modulesLayout->addWidget(m_lblModulesHint);

    auto openExtensionsSettings = [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extensions);
        dlg.exec();
    };

    connect(m_radioModuleCustom, &QRadioButton::toggled, this, [this](bool checked) {
        if (m_btnConfigureExtensions) m_btnConfigureExtensions->setVisible(checked);
        if (m_lblModulesHint) m_lblModulesHint->setVisible(checked);
    });

    connect(m_btnConfigureExtensions, &QPushButton::clicked, this, openExtensionsSettings);

    layout->addWidget(groupModules);
    layout->addStretch();

    return page;
}

QWizardPage *FirstRunWizard::createAppearancePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Appearance & Theme"));
    page->setSubTitle(tr("Customize the visual style and UI components of BubezView."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    auto *groupTheme = new QGroupBox(tr("Application Theme"), page);
    auto *themeLayout = new QVBoxLayout(groupTheme);

    m_comboTheme = new QComboBox(groupTheme);
    m_comboTheme->addItem(tr("Native System Theme (Matches desktop light/dark mode)"), "native");
    m_comboTheme->addItem(tr("Dark Slate (Deep modern slate theme)"), "dark_slate");
    m_comboTheme->addItem(tr("Breeze Dark (KDE Breeze-inspired dark styling)"), "breeze_dark");

    themeLayout->addWidget(m_comboTheme);
    layout->addWidget(groupTheme);

    auto *groupComponents = new QGroupBox(tr("Viewer Components"), page);
    auto *compLayout = new QVBoxLayout(groupComponents);

    m_chkFilmstrip = new QCheckBox(
        tr("Show bottom thumbnail filmstrip when viewing folders, galleries, or multi-page documents"),
        groupComponents
    );
    m_chkFilmstrip->setChecked(false);
    compLayout->addWidget(m_chkFilmstrip);

    layout->addWidget(groupComponents);
    layout->addStretch();
    return page;
}

QWizardPage *FirstRunWizard::createBehaviorPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Navigation, Shortcuts & Behavior"));
    page->setSubTitle(tr("Configure shortcuts, dismissal keys, and navigation caching for daily use."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    // Primary Shortcut & Test
    auto *groupShortcut = new QGroupBox(tr("Primary Preview Shortcut"), page);
    auto *scLayout = new QVBoxLayout(groupShortcut);

    auto *lblShortcutInfo = new QLabel(
        tr("<b>Primary Shortcut:</b> Press <b>Space</b> on any file in your file manager for instant preview."),
        groupShortcut
    );
    lblShortcutInfo->setTextFormat(Qt::RichText);
    scLayout->addWidget(lblShortcutInfo);

    auto *hBoxTest = new QHBoxLayout();
    m_btnTestShortcut = new QPushButton(tr("🧪 Test Shortcut..."), groupShortcut);
    m_btnTestShortcut->setCursor(Qt::PointingHandCursor);
    hBoxTest->addWidget(m_btnTestShortcut);
    hBoxTest->addStretch();
    scLayout->addLayout(hBoxTest);

    m_lblTestResult = new QLabel(groupShortcut);
    m_lblTestResult->setTextFormat(Qt::RichText);
    m_lblTestResult->setWordWrap(true);
    scLayout->addWidget(m_lblTestResult);

    m_chkUseCtrlSpace = new QCheckBox(
        tr("Use Ctrl + Space fallback (connects to trigger_bubez.sh if Space is reserved by Dolphin)"),
        groupShortcut
    );
    m_chkUseCtrlSpace->setVisible(false);
    scLayout->addWidget(m_chkUseCtrlSpace);

    QString projectTrigger = QDir(QCoreApplication::applicationDirPath()).filePath("../trigger_bubez.sh");
    QString triggerPath = QDir::homePath() + "/.local/share/bubezview/trigger_bubez.sh";
    if (QFile::exists(projectTrigger)) {
        triggerPath = QFileInfo(projectTrigger).canonicalFilePath();
    } else if (QFile::exists("/home/davide/Progetti/BubezView/trigger_bubez.sh")) {
        triggerPath = "/home/davide/Progetti/BubezView/trigger_bubez.sh";
    }

    m_lblTriggerHelp = new QLabel(groupShortcut);
    m_lblTriggerHelp->setTextFormat(Qt::RichText);
    m_lblTriggerHelp->setWordWrap(true);
    m_lblTriggerHelp->setText(
        tr("<b>Important note for trigger fallback:</b> In your desktop system settings (KDE Plasma / GNOME Shortcuts), "
           "you will need to bind <b>Ctrl + Space</b> to the trigger script located at:<br>"
           "<code style='color:#38bdf8;'>%1</code>")
        .arg(triggerPath)
    );
    m_lblTriggerHelp->setStyleSheet("background-color: rgba(56, 189, 248, 0.08); border: 1px solid rgba(56, 189, 248, 0.25); border-radius: 6px; padding: 8px; margin-top: 6px; font-size: 12px;");
    m_lblTriggerHelp->setVisible(false);
    scLayout->addWidget(m_lblTriggerHelp);

    auto updateTriggerVisibility = [this](bool visible) {
        if (m_chkUseCtrlSpace) m_chkUseCtrlSpace->setVisible(visible);
        if (m_lblTriggerHelp) m_lblTriggerHelp->setVisible(visible && m_chkUseCtrlSpace->isChecked());
    };

    connect(m_chkUseCtrlSpace, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_lblTriggerHelp) m_lblTriggerHelp->setVisible(checked);
    });

    connect(m_btnTestShortcut, &QPushButton::clicked, this, [this, updateTriggerVisibility]() {
        SpaceShortcutTestDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && dlg.spaceDetected()) {
            m_lblTestResult->setText(tr("<b style='color:#4ade80;'>✅ Spacebar verified and functioning properly!</b>"));
            m_chkUseCtrlSpace->setChecked(false);
            updateTriggerVisibility(false);
        } else {
            m_lblTestResult->setText(tr("<b style='color:#facc15;'>⚠️ Space key not detected or conflict reported. Fallback available:</b>"));
            m_chkUseCtrlSpace->setChecked(true);
            updateTriggerVisibility(true);
        }
    });

    layout->addWidget(groupShortcut);

    // Quick Dismissal & Navigation
    auto *groupKeys = new QGroupBox(tr("Navigation & Window Memory"), page);
    auto *keysLayout = new QVBoxLayout(groupKeys);

    m_chkCloseOnEsc = new QCheckBox(
        tr("Instant dismissal: Press Escape (Esc) to close the viewer"),
        groupKeys
    );
    m_chkCloseOnEsc->setChecked(true);
    keysLayout->addWidget(m_chkCloseOnEsc);

    m_chkRememberGeometry = new QCheckBox(
        tr("Remember window size and position across preview sessions"),
        groupKeys
    );
    m_chkRememberGeometry->setChecked(true);
    keysLayout->addWidget(m_chkRememberGeometry);

    m_chkPreloadCache = new QCheckBox(
        tr("Preload adjacent files: Cache next and previous items in background for instant browsing"),
        groupKeys
    );
    m_chkPreloadCache->setChecked(true);
    keysLayout->addWidget(m_chkPreloadCache);

    m_chkStandbyMode = new QCheckBox(
        tr("Memory standby: Release preview memory cache when idle to save RAM"),
        groupKeys
    );
    m_chkStandbyMode->setChecked(false);
    keysLayout->addWidget(m_chkStandbyMode);

    layout->addWidget(groupKeys);

    // Shortcuts & Launchers
    auto *groupLaunchers = new QGroupBox(tr("Desktop Integration"), page);
    auto *launchersLayout = new QVBoxLayout(groupLaunchers);

    m_chkDesktopShortcut = new QCheckBox(
        tr("Place a quick launcher on the Desktop (~/Desktop or ~/Scrivania)"),
        groupLaunchers
    );
    m_chkDesktopShortcut->setChecked(false); // Disabled by default as requested
    launchersLayout->addWidget(m_chkDesktopShortcut);

    layout->addWidget(groupLaunchers);
    layout->addStretch();

    return page;
}

QWizardPage *FirstRunWizard::createSummaryPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Setup Complete!"));
    page->setSubTitle(tr("Review your configuration before launching BubezView."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    m_lblSummaryDetails = new QLabel(page);
    m_lblSummaryDetails->setTextFormat(Qt::RichText);
    m_lblSummaryDetails->setWordWrap(true);
    m_lblSummaryDetails->setStyleSheet("padding: 12px; font-size: 13px; line-height: 1.5;");

    layout->addWidget(m_lblSummaryDetails);
    layout->addStretch();

    return page;
}

void FirstRunWizard::updateSummaryText()
{
    if (!m_lblSummaryDetails) return;

    QString shortcutText = (m_chkUseCtrlSpace && m_chkUseCtrlSpace->isChecked()) ? "Ctrl+Space (trigger)" : "Space";
    QString cpuText = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked())
        ? tr("🚀 High Performance (Compiled from Source)")
        : tr("📦 Balanced (System Packages)");

    QString modulesText = tr("Default (Keep Current Setup)");
    if (m_radioModuleEnginesOnly && m_radioModuleEnginesOnly->isChecked()) {
        modulesText = tr("Core Plugins Only");
    } else if (m_radioModuleAll && m_radioModuleAll->isChecked()) {
        modulesText = tr("Complete Suite");
    } else if (m_radioModuleCustom && m_radioModuleCustom->isChecked()) {
        modulesText = tr("Custom Configuration");
    }

    QString themeText = m_comboTheme ? m_comboTheme->currentText() : tr("Native");

    QString summaryHtml = QString(
        "<h3>%1</h3>"
        "<ul>"
        "<li><b>%2:</b> %3</li>"
        "<li><b>%4:</b> %5</li>"
        "<li><b>%6:</b> %7</li>"
        "<li><b>%8:</b> %9</li>"
        "<li><b>%10:</b> %11</li>"
        "<li><b>%12:</b> %13</li>"
        "<li><b>%14:</b> %15</li>"
        "</ul>"
        "<p>%16</p>"
    ).arg(
        tr("Summary of Your Choices:"),
        tr("Primary Shortcut"), shortcutText,
        tr("Hardware Performance Profile"), cpuText,
        tr("Modules Preset"), modulesText,
        tr("Theme"), themeText,
        tr("Thumbnail Filmstrip"), (m_chkFilmstrip && m_chkFilmstrip->isChecked() ? tr("Enabled") : tr("Disabled")),
        tr("Quick Dismissal Key"), tr("Escape (Esc)"),
        tr("Preload Cache"), (m_chkPreloadCache && m_chkPreloadCache->isChecked() ? tr("Enabled") : tr("Disabled")),
        tr("Click 'Finish & Launch BubezView' to apply these settings and begin!")
    );

    m_lblSummaryDetails->setText(summaryHtml);
}

void FirstRunWizard::updateModuleOptionTexts()
{
    if (!m_radioModuleAll || !m_radioModuleEnginesOnly) return;

    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());

    if (isHighPerformance) {
        m_radioModuleAll->setText(
            tr("🚀 Complete Suite (BubezView + Core Plugins & Extensions)\n"
               "   Builds BubezView and all core plugins from source with aggressive host optimizations,\n"
               "   and verifies optional tools (QPDF, Tesseract OCR).")
        );
        m_radioModuleEnginesOnly->setText(
            tr("📦 Core Plugins Only (Fast Build)\n"
               "   Builds BubezView and core viewer plugins from source with high-performance flags.\n"
               "   Skips optional extensions (OCR, QPDF).")
        );
    } else {
        m_radioModuleAll->setText(
            tr("🚀 Complete Suite (System Packages)\n"
               "   Uses prebuilt binaries and installs system packages for all extensions (QPDF, Tesseract OCR)\n"
               "   with zero compilation time.")
        );
        m_radioModuleEnginesOnly->setText(
            tr("📦 Core Plugins Only (System Packages)\n"
               "   Uses prebuilt binaries for core viewers and skips optional extensions.")
        );
    }
}

void FirstRunWizard::accept()
{
    QSettings settings("Bubez", "BubezView");

    // 1. Mark first run as completed
    settings.setValue("first_run_completed", true);

    // 2. Hardware Performance Profile
    BuildConfig &cfg = BuildConfig::instance();
    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());
    if (isHighPerformance) {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::HighPerformance);
    } else {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::Balanced);
    }

    // 3. Execution of Compilation / Package Installation based on chosen scope
    bool scopeBase = (m_radioModuleEnginesOnly && m_radioModuleEnginesOnly->isChecked());
    bool scopeAll = (m_radioModuleAll && m_radioModuleAll->isChecked());

    if (isHighPerformance) {
        // High Performance Profile: compile BubezView and all core plugins with host optimizations
        if (scopeAll || scopeBase) {
            BuildConsoleDialog console(this);
            console.setWindowTitle(tr("Compiling BubezView — High Performance Profile"));
            console.startBuild();
            console.exec();
        }

        // For Complete Suite, verify optional system tools (QPDF, Tesseract OCR)
        if (scopeAll) {
            QStringList missingExts;
            const QStringList extIds = {QStringLiteral("qpdf"), QStringLiteral("tesseract")};
            for (const QString &cid : extIds) {
                if (!PackageManagerHelper::isComponentInstalled(cid)) {
                    QString pkg = PackageManagerHelper::packageNameForComponent(cid);
                    if (!pkg.isEmpty() && !missingExts.contains(pkg)) {
                        missingExts << pkg;
                    }
                }
            }
            if (!missingExts.isEmpty()) {
                PackageManagerHelper::runPrivilegedInstall(missingExts, this);
            }
        }
    } else {
        // Balanced Profile: check and install missing system packages without compiling
        QStringList componentsToCheck;
        if (scopeBase) {
            componentsToCheck << "engine-poppler" << "engine-libarchive";
        } else if (scopeAll) {
            componentsToCheck << "engine-poppler" << "engine-libarchive" << "qpdf" << "tesseract";
        }

        QStringList missingPackages;
        for (const QString &cid : componentsToCheck) {
            if (!PackageManagerHelper::isComponentInstalled(cid)) {
                QString pkg = PackageManagerHelper::packageNameForComponent(cid);
                if (!pkg.isEmpty() && !missingPackages.contains(pkg)) {
                    missingPackages << pkg;
                }
            }
        }
        if (!missingPackages.isEmpty()) {
            PackageManagerHelper::runPrivilegedInstall(missingPackages, this);
        }
    }

    if (scopeAll) {
        cfg.setEnableInternalOffice(true);
        settings.setValue("office_use_libreoffice", true);
    } else {
        cfg.setEnableInternalOffice(false);
        settings.setValue("office_use_libreoffice", true);
    }
    cfg.save();

    // 4. Primary Shortcut and System Integrations
    bool useCtrlSpace = (m_chkUseCtrlSpace && m_chkUseCtrlSpace->isChecked());
    QString chosenShortcut = useCtrlSpace ? "Ctrl+Space" : "Space";
    settings.setValue("shortcut", chosenShortcut);

    // Context menus and global shortcuts are automatically registered in the background
    DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
    if (de == DesktopIntegrator::DesktopEnv::KDE) {
        DesktopIntegrator::enableDolphin(chosenShortcut);
    } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
        DesktopIntegrator::enableGnomeNemo();
    }

    // 5. Appearance
    if (m_comboTheme) {
        QString themeCode = m_comboTheme->currentData().toString();
        ThemeManager::setPreset(ThemeManager::stringToPreset(themeCode));
    }
    if (m_chkFilmstrip) {
        settings.setValue("ui/filmstrip", m_chkFilmstrip->isChecked());
        settings.setValue("show_filmstrip", m_chkFilmstrip->isChecked());
    }

    // 6. Navigation, Behavior & Cache
    if (m_chkCloseOnEsc) {
        settings.setValue("quick_close_on_esc", m_chkCloseOnEsc->isChecked());
    }
    // Space is reserved for preview/navigation, not closing
    settings.setValue("quick_close_on_space", false);

    if (m_chkRememberGeometry) {
        settings.setValue("remember_geometry", m_chkRememberGeometry->isChecked());
    }
    if (m_chkPreloadCache) {
        settings.setValue("cache/preload_adjacent", m_chkPreloadCache->isChecked());
    }
    if (m_chkStandbyMode) {
        settings.setValue("cache/enable_standby", m_chkStandbyMode->isChecked());
    }

    // 7. Desktop Shortcut (optional, disabled by default)
    if (m_chkDesktopShortcut && m_chkDesktopShortcut->isChecked()) {
        QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopDir.isEmpty() || !QDir(desktopDir).exists()) {
            if (QDir(QDir::homePath() + "/Scrivania").exists()) {
                desktopDir = QDir::homePath() + "/Scrivania";
            } else {
                desktopDir = QDir::homePath() + "/Desktop";
            }
        }

        if (QDir(desktopDir).exists()) {
            QString desktopFilePath = desktopDir + "/BubezView.desktop";
            QFile file(desktopFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QString projectTrigger = QDir(QCoreApplication::applicationDirPath()).filePath("../trigger_bubez.sh");
                QString triggerScript = QDir::homePath() + "/.local/share/bubezview/trigger_bubez.sh";
                if (QFile::exists(projectTrigger)) {
                    triggerScript = QFileInfo(projectTrigger).canonicalFilePath();
                } else if (QFile::exists(QDir::homePath() + "/.local/share/bubezview/bubezview")) {
                    triggerScript = QDir::homePath() + "/.local/share/bubezview/bubezview";
                } else {
                    triggerScript = QCoreApplication::applicationFilePath();
                }

                QString iconPath = "bubezview";
                QString projectIcon = QDir(QCoreApplication::applicationDirPath()).filePath("../assets/bubezview_icon.png");
                if (QFile::exists(projectIcon)) {
                    iconPath = QFileInfo(projectIcon).canonicalFilePath();
                } else if (QFile::exists("/home/davide/Progetti/BubezView/assets/bubezview_icon.png")) {
                    iconPath = "/home/davide/Progetti/BubezView/assets/bubezview_icon.png";
                }
                QString content = QString(
                    "[Desktop Entry]\n"
                    "Version=1.0\n"
                    "Type=Application\n"
                    "Name=BubezView\n"
                    "GenericName=File Viewer\n"
                    "Comment=Universal fast file viewer\n"
                    "Exec=%1 %F\n"
                    "Icon=%2\n"
                    "Terminal=false\n"
                    "Categories=Utility;Viewer;\n"
                    "StartupNotify=true\n"
                    "MimeType=all/allfiles;inode/directory;\n"
                ).arg(triggerScript, iconPath);
                file.write(content.toUtf8());
                file.close();
                file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                    QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                    QFileDevice::ReadOther | QFileDevice::ExeOther);
            }
        }
    }

    settings.sync();
    QWizard::accept();
}
