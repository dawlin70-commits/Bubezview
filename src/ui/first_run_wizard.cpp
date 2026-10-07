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

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("BubezView Setup Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    resize(680, 520);

    // Register pages
    setPage(Page_Welcome, createWelcomePage());
    setPage(Page_DesktopIntegration, createDesktopIntegrationPage());
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
        tr("This wizard will help you configure your preferred keyboard shortcuts, "
           "file manager integrations, Office rendering engine, and visual appearance.<br><br>"
           "You can complete this setup in under a minute, and change any preference later "
           "from the Settings dialog."),
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

QWizardPage *FirstRunWizard::createDesktopIntegrationPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Desktop & File Manager Integration"));
    page->setSubTitle(tr("Configure how BubezView integrates with your file manager and keyboard shortcuts."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    // Shortcut selection group
    auto *groupShortcut = new QGroupBox(tr("Primary Preview Shortcut"), page);
    auto *shortcutLayout = new QVBoxLayout(groupShortcut);

    m_radioSpaceShortcut = new QRadioButton(
        tr("Space (macOS Quick Look style - Recommended for single-key preview)"),
        groupShortcut
    );
    m_radioCtrlSpaceShortcut = new QRadioButton(
        tr("Ctrl + Space (Preserves the Space key for file selection in Dolphin)"),
        groupShortcut
    );

    // Default to Space for intuitive quick previews
    m_radioSpaceShortcut->setChecked(true);

    shortcutLayout->addWidget(m_radioSpaceShortcut);
    shortcutLayout->addWidget(m_radioCtrlSpaceShortcut);
    layout->addWidget(groupShortcut);

    // Additional integration options
    auto *groupIntegrations = new QGroupBox(tr("File Manager Actions"), page);
    auto *integrationsLayout = new QVBoxLayout(groupIntegrations);

    m_chkFileManagerMenu = new QCheckBox(
        tr("Add 'Quick Preview with BubezView' to file manager context menus (Dolphin, Nemo, Nautilus)"),
        groupIntegrations
    );
    m_chkFileManagerMenu->setChecked(true);

    m_chkGlobalShortcut = new QCheckBox(
        tr("Register system-wide global shortcut to preview selected desktop items"),
        groupIntegrations
    );
    m_chkGlobalShortcut->setChecked(true);

    integrationsLayout->addWidget(m_chkFileManagerMenu);
    integrationsLayout->addWidget(m_chkGlobalShortcut);
    layout->addWidget(groupIntegrations);

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
        tr("🖥️ PC Desktop / Workstation (High Performance)\n"
           "   Maximizes compute speed. Ideal for complex PDFs, massive RAW photos, and heavy media."),
        groupProfiles
    );

    m_radioCpuLaptop = new QRadioButton(
        tr("💻 PC Laptop / Efficiency (Low Power Consumption)\n"
           "   Balanced profile with low battery impact and minimal temperatures. Ready immediately."),
        groupProfiles
    );

    m_radioCpuBalanced = new QRadioButton(
        tr("⚖️ Balanced (Recommended for Modern Everyday Computing)\n"
           "   Combines smooth response times with moderate power usage."),
        groupProfiles
    );

    // Default to Laptop / Efficiency
    m_radioCpuLaptop->setChecked(true);

    profilesLayout->addWidget(m_radioCpuDesktop);
    profilesLayout->addWidget(m_radioCpuLaptop);
    profilesLayout->addWidget(m_radioCpuBalanced);

    auto *hBoxCustom = new QHBoxLayout();
    hBoxCustom->addStretch();
    m_btnCpuCustomize = new QPushButton(tr("⚙️ Customize / Advanced..."), groupProfiles);
    m_btnCpuCustomize->setCursor(Qt::PointingHandCursor);
    connect(m_btnCpuCustomize, &QPushButton::clicked, this, [this]() {
        CompilerWizardDialog dlg(this);
        dlg.exec();
    });
    hBoxCustom->addWidget(m_btnCpuCustomize);
    profilesLayout->addLayout(hBoxCustom);

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

    m_radioModuleMinimal = new QRadioButton(
        tr("🔹 Minimal (Ultra-Lightweight Core)\n"
           "   High-speed image viewer and plain text reader only. Zero extra overhead."),
        groupModules
    );

    m_radioModuleRecommended = new QRadioButton(
        tr("⭐ Recommended (Standard Universal Viewer)\n"
           "   Core viewers + PDF documents (Poppler), compressed archives, and media playback."),
        groupModules
    );

    m_radioModuleEnginesOnly = new QRadioButton(
        tr("📦 Reader Engines Only\n"
           "   Enables dedicated parsing engines for PDF, Archives, and Audio/Video."),
        groupModules
    );

    m_radioModuleAll = new QRadioButton(
        tr("🚀 Complete (All Engines & Extensions)\n"
           "   Full suite including Office documents (LibreOffice / native parser), QPDF tools, and QR codes."),
        groupModules
    );

    m_radioModuleCustom = new QRadioButton(
        tr("⚙️ Customize\n"
           "   Full granular control over each individual engine, library, and compilation option."),
        groupModules
    );

    m_radioModuleRecommended->setChecked(true);

    modulesLayout->addWidget(m_radioModuleMinimal);
    modulesLayout->addWidget(m_radioModuleRecommended);
    modulesLayout->addWidget(m_radioModuleEnginesOnly);
    modulesLayout->addWidget(m_radioModuleAll);
    modulesLayout->addWidget(m_radioModuleCustom);

    auto *hBoxCustomMod = new QHBoxLayout();
    hBoxCustomMod->addStretch();
    m_btnModuleCustomize = new QPushButton(tr("⚙️ Open Advanced Module Manager..."), groupModules);
    m_btnModuleCustomize->setCursor(Qt::PointingHandCursor);
    connect(m_btnModuleCustomize, &QPushButton::clicked, this, [this]() {
        BuildOptionsDialog dlg(this);
        dlg.exec();
    });
    connect(m_radioModuleCustom, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            BuildOptionsDialog dlg(this);
            dlg.exec();
        }
    });
    hBoxCustomMod->addWidget(m_btnModuleCustomize);
    modulesLayout->addLayout(hBoxCustomMod);

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
    m_comboTheme->addItem(tr("Dark Modern (Deep dark theme for contrast)"), "dark");
    m_comboTheme->addItem(tr("Light Clean (Bright minimalist theme)"), "light");

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
    page->setTitle(tr("Window & Navigation Behavior"));
    page->setSubTitle(tr("Configure shortcuts and window memory for seamless daily use."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);

    auto *groupKeys = new QGroupBox(tr("Quick Navigation"), page);
    auto *keysLayout = new QVBoxLayout(groupKeys);

    m_chkCloseOnSpaceOrEsc = new QCheckBox(
        tr("Instant dismissal: Press Space or Escape to instantly close the viewer"),
        groupKeys
    );
    m_chkCloseOnSpaceOrEsc->setChecked(true);
    keysLayout->addWidget(m_chkCloseOnSpaceOrEsc);

    m_chkRememberGeometry = new QCheckBox(
        tr("Remember window size and position across preview sessions"),
        groupKeys
    );
    m_chkRememberGeometry->setChecked(true);
    keysLayout->addWidget(m_chkRememberGeometry);

    layout->addWidget(groupKeys);

    auto *groupShortcuts = new QGroupBox(tr("Shortcuts & Launchers"), page);
    auto *scLayout = new QVBoxLayout(groupShortcuts);

    m_chkDesktopShortcut = new QCheckBox(
        tr("Place a quick launcher on the Desktop (~/Desktop or ~/Scrivania)"),
        groupShortcuts
    );
    m_chkDesktopShortcut->setChecked(true);
    scLayout->addWidget(m_chkDesktopShortcut);

    layout->addWidget(groupShortcuts);
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

    QString shortcutText = (m_radioSpaceShortcut && m_radioSpaceShortcut->isChecked()) ? "Space" : "Ctrl+Space";
    QString cpuText = tr("Laptop / Efficiency (Low Power)");
    if (m_radioCpuDesktop && m_radioCpuDesktop->isChecked()) {
        cpuText = tr("Desktop / Workstation (High Performance)");
    } else if (m_radioCpuBalanced && m_radioCpuBalanced->isChecked()) {
        cpuText = tr("Balanced (Modern Everyday Computing)");
    }

    QString modulesText = tr("Recommended (PDF, Archives, Media)");
    if (m_radioModuleMinimal && m_radioModuleMinimal->isChecked()) {
        modulesText = tr("Minimal Core (Images & Text)");
    } else if (m_radioModuleEnginesOnly && m_radioModuleEnginesOnly->isChecked()) {
        modulesText = tr("Engines Only (PDF, Archives, Media)");
    } else if (m_radioModuleAll && m_radioModuleAll->isChecked()) {
        modulesText = tr("Complete Suite (All Modules & Office)");
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
        "</ul>"
        "<p>%14</p>"
    ).arg(
        tr("Summary of Your Choices:"),
        tr("Primary Shortcut"), shortcutText,
        tr("Hardware Performance Profile"), cpuText,
        tr("Functional Modules Preset"), modulesText,
        tr("Theme"), themeText,
        tr("Thumbnail Filmstrip"), (m_chkFilmstrip && m_chkFilmstrip->isChecked() ? tr("Enabled") : tr("Disabled")),
        tr("Quick Dismissal (Space/Esc)"), (m_chkCloseOnSpaceOrEsc && m_chkCloseOnSpaceOrEsc->isChecked() ? tr("Enabled") : tr("Disabled")),
        tr("Click 'Finish & Launch BubezView' to apply these settings and begin!")
    );

    m_lblSummaryDetails->setText(summaryHtml);
}

void FirstRunWizard::accept()
{
    QSettings settings("Bubez", "BubezView");

    // 1. Mark first run as completed
    settings.setValue("first_run_completed", true);

    // 2. Shortcut and Desktop Integrations
    QString chosenShortcut = (m_radioSpaceShortcut && m_radioSpaceShortcut->isChecked()) ? "Space" : "Ctrl+Space";
    settings.setValue("shortcut", chosenShortcut);

    if (m_chkFileManagerMenu && m_chkFileManagerMenu->isChecked()) {
        DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            DesktopIntegrator::enableDolphin(chosenShortcut);
        } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
            DesktopIntegrator::enableGnomeNemo();
        }
    }

    // 3. CPU Performance Profile
    BuildConfig &cfg = BuildConfig::instance();
    if (m_radioCpuDesktop && m_radioCpuDesktop->isChecked()) {
        cfg.setCpuProfile(QStringLiteral("native"));
        cfg.setOptimizationLevel(QStringLiteral("O3"));
    } else if (m_radioCpuLaptop && m_radioCpuLaptop->isChecked()) {
        cfg.setCpuProfile(QStringLiteral("standard"));
        cfg.setOptimizationLevel(QStringLiteral("O2"));
    } else if (m_radioCpuBalanced && m_radioCpuBalanced->isChecked()) {
        cfg.setCpuProfile(QStringLiteral("balanced"));
        cfg.setOptimizationLevel(QStringLiteral("O2"));
    }

    // 4. Functional Modules Preset
    if (m_radioModuleMinimal && m_radioModuleMinimal->isChecked()) {
        cfg.setStaticPdf(false);
        cfg.setStaticArchive(false);
        cfg.setEnableInternalOffice(false);
        settings.setValue("office_use_libreoffice", true);
    } else if (m_radioModuleRecommended && m_radioModuleRecommended->isChecked()) {
        cfg.setStaticPdf(false);
        cfg.setStaticArchive(false);
        cfg.setEnableInternalOffice(false);
        settings.setValue("office_use_libreoffice", true);
    } else if (m_radioModuleEnginesOnly && m_radioModuleEnginesOnly->isChecked()) {
        cfg.setStaticPdf(false);
        cfg.setStaticArchive(false);
        cfg.setEnableInternalOffice(false);
        settings.setValue("office_use_libreoffice", true);
    } else if (m_radioModuleAll && m_radioModuleAll->isChecked()) {
        cfg.setStaticPdf(false);
        cfg.setStaticArchive(false);
        cfg.setEnableInternalOffice(true);
        settings.setValue("office_use_libreoffice", false);
    }
    cfg.save();

    // 4. Appearance
    if (m_comboTheme) {
        QString themeCode = m_comboTheme->currentData().toString();
        settings.setValue("theme", themeCode);
    }
    if (m_chkFilmstrip) {
        settings.setValue("ui/filmstrip", m_chkFilmstrip->isChecked());
        settings.setValue("show_filmstrip", m_chkFilmstrip->isChecked());
    }

    // 5. Behavior
    if (m_chkCloseOnSpaceOrEsc) {
        settings.setValue("quick_close_on_space", m_chkCloseOnSpaceOrEsc->isChecked());
    }
    if (m_chkRememberGeometry) {
        settings.setValue("remember_geometry", m_chkRememberGeometry->isChecked());
    }

    // 6. Desktop Shortcut
    if (m_chkDesktopShortcut && m_chkDesktopShortcut->isChecked()) {
        QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopDir.isEmpty() || !QDir(desktopDir).exists()) {
            // Check Scrivania / Desktop fallbacks
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
                QString triggerScript = QDir::homePath() + "/.local/share/bubezview/bubezview";
                if (QFile::exists("/home/davide/Progetti/BubezView/trigger_bubez.sh")) {
                    triggerScript = "/home/davide/Progetti/BubezView/trigger_bubez.sh";
                } else if (QFile::exists(QCoreApplication::applicationFilePath())) {
                    triggerScript = QCoreApplication::applicationFilePath();
                }

                QString iconPath = "bubezview";
                if (QFile::exists("/home/davide/Progetti/BubezView/assets/bubezview_icon.png")) {
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
