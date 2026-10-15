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
#include <QScrollArea>
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
    resize(740, 600);
    setMinimumSize(680, 520);

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
    page->setSubTitle(tr("Select which document readers, media players, and preview engines you want to enable."));

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *container = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(container);
    layout->setSpacing(12);
    layout->setContentsMargins(8, 8, 8, 8);

    // 1. Module Setup Presets (High-level scenarios)
    auto *groupPresets = new QGroupBox(tr("Module Setup Presets"), container);
    auto *presetsLayout = new QVBoxLayout(groupPresets);
    presetsLayout->setSpacing(6);

    m_radioPresetFull = new QRadioButton(groupPresets);
    m_radioPresetDocs = new QRadioButton(groupPresets);
    m_radioPresetMedia = new QRadioButton(groupPresets);
    m_radioPresetMinimal = new QRadioButton(groupPresets);
    m_radioPresetCustom = new QRadioButton(
        tr("⚙️ Custom Configuration\n"
           "   Choose each component and engine manually using the checkboxes below."),
        groupPresets
    );

    presetsLayout->addWidget(m_radioPresetFull);
    presetsLayout->addWidget(m_radioPresetDocs);
    presetsLayout->addWidget(m_radioPresetMedia);
    presetsLayout->addWidget(m_radioPresetMinimal);
    presetsLayout->addWidget(m_radioPresetCustom);

    layout->addWidget(groupPresets);

    // 2. Granular Components & Capabilities (Hierarchical Categorized Toggles)
    auto *groupComponents = new QGroupBox(tr("Active Components & Capabilities"), container);
    auto *compLayout = new QVBoxLayout(groupComponents);
    compLayout->setSpacing(8);

    auto addCategoryHeader = [compLayout, groupComponents](const QString &headerTitle) {
        auto *lblCat = new QLabel(headerTitle, groupComponents);
        lblCat->setStyleSheet(
            "font-size: 12px; font-weight: bold; color: #38bdf8; "
            "margin-top: 6px; margin-bottom: 2px; text-transform: uppercase; letter-spacing: 0.5px;"
        );
        compLayout->addWidget(lblCat);
    };

    auto addModuleRow = [compLayout, groupComponents](
        QCheckBox *&chk, QLabel *&badge,
        const QString &title, const QString &desc
    ) {
        auto *rowWidget = new QWidget(groupComponents);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 3, 0, 3);
        rowLayout->setSpacing(8);

        auto *colText = new QVBoxLayout();
        colText->setSpacing(1);

        chk = new QCheckBox(title, rowWidget);
        chk->setStyleSheet("font-weight: 600; font-size: 13px;");
        colText->addWidget(chk);

        auto *lblDesc = new QLabel(desc, rowWidget);
        lblDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 20px;");
        lblDesc->setWordWrap(true);
        colText->addWidget(lblDesc);

        badge = new QLabel(rowWidget);
        badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        badge->setTextFormat(Qt::RichText);

        rowLayout->addLayout(colText, 1);
        rowLayout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);

        compLayout->addWidget(rowWidget);
    };

    auto addSubModuleRow = [compLayout, groupComponents](
        QCheckBox *&chk, QLabel *&badge,
        const QString &title, const QString &desc
    ) {
        auto *rowWidget = new QWidget(groupComponents);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(24, 1, 0, 1);
        rowLayout->setSpacing(8);

        auto *colText = new QVBoxLayout();
        colText->setSpacing(1);

        chk = new QCheckBox(title, rowWidget);
        chk->setStyleSheet("font-weight: 500; font-size: 12px; color: #cbd5e1;");
        colText->addWidget(chk);

        auto *lblDesc = new QLabel(desc, rowWidget);
        lblDesc->setStyleSheet("color: #64748b; font-size: 10px; margin-left: 20px;");
        lblDesc->setWordWrap(true);
        colText->addWidget(lblDesc);

        badge = new QLabel(rowWidget);
        badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        badge->setTextFormat(Qt::RichText);

        rowLayout->addLayout(colText, 1);
        rowLayout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);

        compLayout->addWidget(rowWidget);
    };

    // Category 1: Documents & Office
    addCategoryHeader(tr("📄 Documents & Office"));
    addModuleRow(
        m_chkModulePdf, m_badgePdf,
        tr("PDF Documents & Digital Signatures"),
        tr("Vector rendering pipeline (Poppler) and cryptographic signature validation (OpenSSL).")
    );
    addSubModuleRow(
        m_chkSubOcr, m_badgeOcr,
        tr("Optical Character Recognition (Tesseract OCR)"),
        tr("Extract selectable UTF-8 text from scanned pages and visual screen snippets.")
    );
    addSubModuleRow(
        m_chkSubQpdf, m_badgeQpdf,
        tr("PDF Repair & Advanced Tools (QPDF / pdfimages)"),
        tr("Fix corrupted PDF streams, linearize for instant loading, and extract embedded lossless images.")
    );
    addModuleRow(
        m_chkModuleOffice, m_badgeOffice,
        tr("Office Document Preview"),
        tr("Preview Microsoft Office (.docx, .xlsx, .pptx) and OpenDocument files via LibreOffice.")
    );

    // Category 2: Multimedia & Audio
    addCategoryHeader(tr("🎬 Multimedia & Audio"));
    addModuleRow(
        m_chkModuleMedia, m_badgeMedia,
        tr("Multimedia Video & Audio Player (MPV)"),
        tr("Hardware GPU video decoding (VA-API/NVDEC) and high-fidelity bit-perfect audio streaming.")
    );
    addSubModuleRow(
        m_chkSubEqualizer, m_badgeEqualizer,
        tr("10-Band Equalizer & Visualizer (FFTW3)"),
        tr("Real-time stereo spectrum visualizer and studio sound equalization.")
    );

    // Category 3: Compressed Archives
    addCategoryHeader(tr("📦 Compressed Archives"));
    addModuleRow(
        m_chkModuleArchive, m_badgeArchive,
        tr("Archive Manager (LibArchive)"),
        tr("Universal archive browsing, cataloging, and direct extraction for ZIP, TAR, 7Z, and ISO.")
    );
    addSubModuleRow(
        m_chkSubSevenZip, m_badgeSevenZip,
        tr("Multi-Core 7-Zip & zstd Compression"),
        tr("Parallel multi-threaded compression across all CPU cores for ZIP, 7Z, and modern zstd archives.")
    );

    layout->addWidget(groupComponents);

    // 3. Bottom bar with hint and settings button
    auto *bottomRow = new QHBoxLayout();
    m_lblModulesHint = new QLabel(
        tr("ℹ️ Settings can be customized or recompiled at any time from Settings → Extensions."),
        container
    );
    m_lblModulesHint->setStyleSheet("color: #64748b; font-size: 11px;");
    m_lblModulesHint->setWordWrap(true);
    bottomRow->addWidget(m_lblModulesHint, 1);

    m_btnConfigureExtensions = new QPushButton(tr("⚙️ Advanced Settings..."), container);
    m_btnConfigureExtensions->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnConfigureExtensions);
    layout->addLayout(bottomRow);

    scrollArea->setWidget(container);
    pageLayout->addWidget(scrollArea);

    connect(m_btnConfigureExtensions, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extensions);
        dlg.exec();
        updateModuleStatusBadges();
    });

    // Wire preset radio buttons to applyPreset
    connect(m_radioPresetFull, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) applyPreset(0);
    });
    connect(m_radioPresetDocs, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) applyPreset(1);
    });
    connect(m_radioPresetMedia, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) applyPreset(2);
    });
    connect(m_radioPresetMinimal, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) applyPreset(3);
    });

    // Sub-feature enabling linked to parent checkbox
    connect(m_chkModulePdf, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubOcr) m_chkSubOcr->setEnabled(checked);
        if (m_chkSubQpdf) m_chkSubQpdf->setEnabled(checked);
    });
    connect(m_chkModuleMedia, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubEqualizer) m_chkSubEqualizer->setEnabled(checked);
    });
    connect(m_chkModuleArchive, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubSevenZip) m_chkSubSevenZip->setEnabled(checked);
    });

    // Wire checkboxes to evaluatePresetFromCheckboxes
    auto onCheckChanged = [this](bool) {
        evaluatePresetFromCheckboxes();
    };
    connect(m_chkModulePdf, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubOcr, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubQpdf, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleOffice, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleMedia, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubEqualizer, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleArchive, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubSevenZip, &QCheckBox::toggled, this, onCheckChanged);

    // Initial state: populate labels, badges, and activate Complete Suite by default
    updateModuleOptionTexts();
    updateModuleStatusBadges();
    m_radioPresetFull->setChecked(true);
    applyPreset(0);

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

void FirstRunWizard::applyPreset(int presetIndex)
{
    m_syncingModules = true;
    switch (presetIndex) {
        case 0: // Complete Suite
            if (m_chkModulePdf) m_chkModulePdf->setChecked(true);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(true);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(true);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(true);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(true);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(true);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 1: // Documents & Office
            if (m_chkModulePdf) m_chkModulePdf->setChecked(true);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(true);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(true);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(true);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(false);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(false);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 2: // Media & Entertainment
            if (m_chkModulePdf) m_chkModulePdf->setChecked(false);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(false);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(false);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(false);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(true);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(true);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 3: // Minimal Core
            if (m_chkModulePdf) m_chkModulePdf->setChecked(true);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(false);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(false);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(false);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(false);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(false);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(false);
            break;
        default:
            break;
    }
    m_syncingModules = false;
}

void FirstRunWizard::evaluatePresetFromCheckboxes()
{
    if (m_syncingModules) return;

    bool pdf = m_chkModulePdf && m_chkModulePdf->isChecked();
    bool ocr = m_chkSubOcr && m_chkSubOcr->isChecked();
    bool qpdf = m_chkSubQpdf && m_chkSubQpdf->isChecked();
    bool off = m_chkModuleOffice && m_chkModuleOffice->isChecked();
    bool med = m_chkModuleMedia && m_chkModuleMedia->isChecked();
    bool eq = m_chkSubEqualizer && m_chkSubEqualizer->isChecked();
    bool arc = m_chkModuleArchive && m_chkModuleArchive->isChecked();
    bool sz = m_chkSubSevenZip && m_chkSubSevenZip->isChecked();

    m_syncingModules = true;
    if (pdf && ocr && qpdf && off && med && eq && arc && sz) {
        if (m_radioPresetFull) m_radioPresetFull->setChecked(true);
    } else if (pdf && ocr && qpdf && off && !med && !eq && arc && sz) {
        if (m_radioPresetDocs) m_radioPresetDocs->setChecked(true);
    } else if (!pdf && !ocr && !qpdf && !off && med && eq && arc && sz) {
        if (m_radioPresetMedia) m_radioPresetMedia->setChecked(true);
    } else if (pdf && !ocr && !qpdf && !off && !med && !eq && arc && !sz) {
        if (m_radioPresetMinimal) m_radioPresetMinimal->setChecked(true);
    } else {
        if (m_radioPresetCustom) m_radioPresetCustom->setChecked(true);
    }
    m_syncingModules = false;
}

void FirstRunWizard::updateModuleStatusBadges()
{
    if (m_badgePdf) {
        bool ready = BuildConfig::isPkgInstalled("poppler-qt6") || !BuildConfig::pdfimagesExecutable().isEmpty() || BuildConfig::instance().staticPdf();
        m_badgePdf->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                  : tr("<span style='color: #f59e0b;'>○ Available</span>"));
    }
    if (m_badgeOcr) {
        bool ready = BuildConfig::isTesseractAvailable();
        m_badgeOcr->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                  : tr("<span style='color: #64748b;'>○ Optional</span>"));
    }
    if (m_badgeQpdf) {
        bool ready = BuildConfig::isQpdfAvailable();
        m_badgeQpdf->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                   : tr("<span style='color: #64748b;'>○ Optional</span>"));
    }
    if (m_badgeOffice) {
        bool ready = !QStandardPaths::findExecutable("libreoffice").isEmpty();
        m_badgeOffice->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                     : tr("<span style='color: #64748b;'>○ Optional: libreoffice</span>"));
    }
    if (m_badgeMedia) {
        bool ready = BuildConfig::isPkgInstalled("mpv") || !QStandardPaths::findExecutable("mpv").isEmpty();
        m_badgeMedia->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                    : tr("<span style='color: #64748b;'>○ Available</span>"));
    }
    if (m_badgeEqualizer) {
        m_badgeEqualizer->setText(tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>"));
    }
    if (m_badgeArchive) {
        bool ready = BuildConfig::isPkgInstalled("libarchive");
        m_badgeArchive->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                      : tr("<span style='color: #64748b;'>○ Available</span>"));
    }
    if (m_badgeSevenZip) {
        bool ready = BuildConfig::isSevenZipAvailable();
        m_badgeSevenZip->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                       : tr("<span style='color: #64748b;'>○ Optional: 7zip</span>"));
    }
}

void FirstRunWizard::updateSummaryText()
{
    if (!m_lblSummaryDetails) return;

    QString shortcutText = (m_chkUseCtrlSpace && m_chkUseCtrlSpace->isChecked()) ? "Ctrl+Space (trigger)" : "Space";
    QString cpuText = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked())
        ? tr("🚀 High Performance (Compiled from Source)")
        : tr("📦 Balanced (System Packages)");

    QString modulesPresetText = tr("Custom Configuration");
    if (m_radioPresetFull && m_radioPresetFull->isChecked()) {
        modulesPresetText = tr("Complete Suite (All modules enabled)");
    } else if (m_radioPresetDocs && m_radioPresetDocs->isChecked()) {
        modulesPresetText = tr("Documents & Office");
    } else if (m_radioPresetMedia && m_radioPresetMedia->isChecked()) {
        modulesPresetText = tr("Media & Entertainment");
    } else if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        modulesPresetText = tr("Minimal Core");
    }

    QStringList activeModulesList;
    if (m_chkModulePdf && m_chkModulePdf->isChecked()) {
        QString pdfStr = tr("PDF");
        QStringList pdfExtras;
        if (m_chkSubOcr && m_chkSubOcr->isChecked()) pdfExtras << "OCR";
        if (m_chkSubQpdf && m_chkSubQpdf->isChecked()) pdfExtras << "QPDF";
        if (!pdfExtras.isEmpty()) {
            pdfStr += QString(" (%1)").arg(pdfExtras.join("+"));
        }
        activeModulesList << pdfStr;
    }
    if (m_chkModuleOffice && m_chkModuleOffice->isChecked()) activeModulesList << tr("Office Preview");
    if (m_chkModuleMedia && m_chkModuleMedia->isChecked()) {
        QString medStr = tr("Media");
        if (m_chkSubEqualizer && m_chkSubEqualizer->isChecked()) medStr += tr(" (+Equalizer)");
        activeModulesList << medStr;
    }
    if (m_chkModuleArchive && m_chkModuleArchive->isChecked()) {
        QString arcStr = tr("Archives");
        if (m_chkSubSevenZip && m_chkSubSevenZip->isChecked()) arcStr += tr(" (+7-Zip)");
        activeModulesList << arcStr;
    }
    QString activeModulesStr = activeModulesList.isEmpty() ? tr("None") : activeModulesList.join(", ");

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
        "<li><b>%16:</b> %17</li>"
        "</ul>"
        "<p>%18</p>"
    ).arg(
        tr("Summary of Your Choices:"),
        tr("Primary Shortcut"), shortcutText,
        tr("Hardware Performance Profile"), cpuText,
        tr("Modules Preset"), modulesPresetText,
        tr("Enabled Modules"), activeModulesStr,
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
    if (!m_radioPresetFull || !m_radioPresetDocs || !m_radioPresetMedia || !m_radioPresetMinimal) return;

    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());

    if (isHighPerformance) {
        m_radioPresetFull->setText(
            tr("🚀 Complete Suite — Host Compiled\n"
               "   All features compiled from source with aggressive CPU optimizations (-O3, -march=native).")
        );
        m_radioPresetDocs->setText(
            tr("📄 Documents & Office — Host Compiled\n"
               "   Optimized PDF vector rendering, LibreOffice preview, OCR, and digital signatures.")
        );
        m_radioPresetMedia->setText(
            tr("🎬 Media & Entertainment\n"
               "   Hardware GPU video playback, 10-band audio equalizer, and archive management.")
        );
        m_radioPresetMinimal->setText(
            tr("🪶 Minimal Core (Zero Extra Dependencies)\n"
               "   Fast native image, code, and folder preview with instant cold startup.")
        );
    } else {
        m_radioPresetFull->setText(
            tr("🚀 Complete Suite — System Packages\n"
               "   All features using distribution precompiled packages with zero compile wait time.")
        );
        m_radioPresetDocs->setText(
            tr("📄 Documents & Office — System Packages\n"
               "   PDF viewing, LibreOffice integration, OCR, and cryptographic signatures.")
        );
        m_radioPresetMedia->setText(
            tr("🎬 Media & Entertainment\n"
               "   Hardware GPU video playback, 10-band audio equalizer, and archive management.")
        );
        m_radioPresetMinimal->setText(
            tr("🪶 Minimal Core (Zero Extra Dependencies)\n"
               "   Fast native image, code, and folder preview with instant cold startup.")
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

    // 3. Execution of Compilation / Package Installation based on chosen modules
    bool enablePdf = m_chkModulePdf && m_chkModulePdf->isChecked();
    bool enableOcr = m_chkSubOcr && m_chkSubOcr->isChecked();
    bool enableQpdf = m_chkSubQpdf && m_chkSubQpdf->isChecked();
    bool enableOffice = m_chkModuleOffice && m_chkModuleOffice->isChecked();
    bool enableMedia = m_chkModuleMedia && m_chkModuleMedia->isChecked();
    bool enableEqualizer = m_chkSubEqualizer && m_chkSubEqualizer->isChecked();
    bool enableArchive = m_chkModuleArchive && m_chkModuleArchive->isChecked();
    bool enableSevenZip = m_chkSubSevenZip && m_chkSubSevenZip->isChecked();

    settings.setValue("modules/pdf_enabled", enablePdf);
    settings.setValue("modules/ocr_enabled", enableOcr);
    settings.setValue("modules/qpdf_enabled", enableQpdf);
    settings.setValue("modules/office_enabled", enableOffice);
    settings.setValue("modules/media_enabled", enableMedia);
    settings.setValue("modules/equalizer_enabled", enableEqualizer);
    settings.setValue("modules/archive_enabled", enableArchive);
    settings.setValue("modules/sevenzip_enabled", enableSevenZip);

    cfg.setEnableInternalOffice(enableOffice);
    settings.setValue("office_use_libreoffice", enableOffice);

    if (isHighPerformance) {
        // High Performance Profile: compile BubezView and enabled plugins with host optimizations
        if (enablePdf || enableArchive || enableMedia) {
            BuildConsoleDialog console(this);
            console.setWindowTitle(tr("Compiling BubezView — High Performance Profile"));
            console.startBuild();
            console.exec();
        }

        // For advanced tools, verify optional system tools (QPDF, Tesseract OCR, 7-Zip)
        QStringList missingExts;
        if (enableOcr && !PackageManagerHelper::isComponentInstalled("tesseract")) {
            missingExts << PackageManagerHelper::packageNameForComponent("tesseract");
        }
        if (enableQpdf && !PackageManagerHelper::isComponentInstalled("qpdf")) {
            missingExts << PackageManagerHelper::packageNameForComponent("qpdf");
        }
        if (enableSevenZip && !PackageManagerHelper::isComponentInstalled("7zip")) {
            missingExts << PackageManagerHelper::packageNameForComponent("7zip");
        }
        if (!missingExts.isEmpty()) {
            PackageManagerHelper::runPrivilegedInstall(missingExts, this);
        }
    } else {
        // Balanced Profile: check and install missing system packages without compiling
        QStringList componentsToCheck;
        if (enablePdf) componentsToCheck << "engine-poppler";
        if (enableArchive) componentsToCheck << "engine-libarchive";
        if (enableOcr) componentsToCheck << "tesseract";
        if (enableQpdf) componentsToCheck << "qpdf";
        if (enableSevenZip) componentsToCheck << "7zip";
        if (enableMedia) componentsToCheck << "mpv";

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
