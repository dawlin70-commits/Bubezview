/**
 * @file settings_performance_tab.cpp
 * @brief Implementation of the performance, hardware profiles, and compiler settings tab.
 */

#include "settings_performance_tab.h"
#include "settings_appearance_tab.h"
#include "build_config.h"
#include "preload_cache_manager.h"
#include "compiler_wizard_dialog.h"
#include "fallback_notice_dialog.h"
#include "cpu_info.h"
#include "gpu_acceleration_manager.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QListView>
#include <QFrame>
#include <QMessageBox>
#include <QSettings>
#include <QScrollArea>
#include <QSignalBlocker>

static QFrame *createDivider(QWidget *parent)
{
    QFrame *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("RowDivider"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    return line;
}

SettingsPerformanceTab::SettingsPerformanceTab(QWidget *parent)
    : QWidget(parent)
    , m_optLevel(QStringLiteral("O2"))
{
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.load();
    m_optLevel = bcfg.optimizationLevel();
    if (m_optLevel.isEmpty()) {
        m_optLevel = QStringLiteral("O2");
    }
    m_initialOptLevel = m_optLevel;
    m_initialCpuProfile = bcfg.cpuProfile();
    m_initialCustomFlags = bcfg.customCompilerFlags();

    m_activeProfile = detectActiveProfile();
    m_initialProfile = m_activeProfile;

    setupUi();
    syncProfileUi();
    updateThemeStyles();
}

SettingsPerformanceTab::Profile SettingsPerformanceTab::detectActiveProfile() const
{
    BuildConfig &bcfg = BuildConfig::instance();
    BuildConfig::HardwareProfile hp = bcfg.activeHardwareProfile();
    switch (hp) {
    case BuildConfig::HardwareProfile::SystemPackages:
        return Profile::SystemPackages;
    case BuildConfig::HardwareProfile::MissingOnly:
        return Profile::MissingOnly;
    case BuildConfig::HardwareProfile::AllStable:
        return Profile::AllStable;
    case BuildConfig::HardwareProfile::AllSource:
        return Profile::AllSource;
    case BuildConfig::HardwareProfile::Custom:
        return Profile::Custom;
    }
    return Profile::MissingOnly;
}

void SettingsPerformanceTab::syncProfileUi()
{
    if (!m_comboProfile) return;
    const QSignalBlocker blocker(m_comboProfile);

    int customIdx = m_comboProfile->findData(static_cast<int>(Profile::Custom));
    if (m_activeProfile == Profile::Custom) {
        if (customIdx < 0) {
            m_comboProfile->addItem(tr("Custom (Compiler Wizard)"), static_cast<int>(Profile::Custom));
        }
    } else {
        if (customIdx >= 0) {
            m_comboProfile->removeItem(customIdx);
        }
    }

    int idx = m_comboProfile->findData(static_cast<int>(m_activeProfile));
    if (idx >= 0) {
        m_comboProfile->setCurrentIndex(idx);
    }

    if (m_btnWizard) {
        bool enableWizard = (m_activeProfile != Profile::SystemPackages);
        m_btnWizard->setEnabled(enableWizard);
        if (!enableWizard) {
            m_btnWizard->setToolTip(tr("Compilation is disabled under System Packages profile."));
        } else {
            m_btnWizard->setToolTip(tr("Open the interactive Compiler Wizard to select fine-grained instructions and optimization flags."));
        }
    }

    if (m_lblOptHint) {
        switch (m_activeProfile) {
        case Profile::SystemPackages:
            m_lblOptHint->setText(tr("Uses precompiled system packages. Zero compilation required."));
            break;
        case Profile::MissingOnly:
            m_lblOptHint->setText(tr("Compiles only missing engines into ~/.local, without touching the system."));
            break;
        case Profile::AllStable:
            m_lblOptHint->setText(tr("Compiles stable engines into ~/.local with CPU optimizations, without touching the system."));
            break;
        case Profile::AllSource:
            m_lblOptHint->setText(tr("Compiles all components from source into ~/.local, keeping system clean."));
            break;
        case Profile::Custom:
            m_lblOptHint->setText(tr("Custom compiler flags applied to local components in ~/.local."));
            break;
        }
    }
}

void SettingsPerformanceTab::selectProfile(Profile profile)
{
    if (profile == Profile::Custom) {
        openCompilerWizard();
        return;
    }

    BuildConfig &bcfg = BuildConfig::instance();

    switch (profile) {
    case Profile::SystemPackages:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::SystemPackages);
        break;
    case Profile::MissingOnly:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::MissingOnly);
        break;
    case Profile::AllStable:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::AllStable);
        break;
    case Profile::AllSource:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::AllSource);
        break;
    default:
        break;
    }

    m_optLevel = bcfg.optimizationLevel();
    m_activeProfile = profile;
    syncProfileUi();

    if (m_switchPreload) {
        QSettings settings("Bubez", "BubezView");
        int curDist = settings.value("prefetch_distance", 1).toInt();
        m_switchPreload->setCheckedAnimated(curDist > 0);
    }

    emit pendingStateChanged();
}

void SettingsPerformanceTab::onProfileComboChanged(int index)
{
    if (index < 0 || !m_comboProfile) return;
    int data = m_comboProfile->itemData(index).toInt();
    Profile prof = static_cast<Profile>(data);
    if (prof == Profile::Custom) {
        openCompilerWizard();
    } else {
        selectProfile(prof);
    }
}

void SettingsPerformanceTab::onStandbyComboChanged(int index)
{
    Q_UNUSED(index);
    if (!m_comboStandby) return;
    int minutes = m_comboStandby->currentData().toInt();
    QSettings settings("Bubez", "BubezView");
    settings.setValue("background_keep_alive_minutes", minutes);
}

QWidget *SettingsPerformanceTab::createSettingRow(const QString &title, QWidget *control, QWidget *parent, const QString &subtitle)
{
    QWidget *rowWidget = new QWidget(parent);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(14);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    QLabel *lblTitle = new QLabel(title, rowWidget);
    lblTitle->setObjectName(QStringLiteral("RowTitle"));
    textLayout->addWidget(lblTitle);

    if (!subtitle.trimmed().isEmpty()) {
        QLabel *lblSub = new QLabel(subtitle, rowWidget);
        lblSub->setObjectName(QStringLiteral("RowSubtitle"));
        lblSub->setWordWrap(true);
        textLayout->addWidget(lblSub);
    }

    rowLayout->addLayout(textLayout, 1);
    if (control) {
        rowLayout->addWidget(control, 0, Qt::AlignVCenter);
    }

    return rowWidget;
}

void SettingsPerformanceTab::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget(scrollArea);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setSpacing(8);
    layout->setContentsMargins(14, 4, 14, 6);

    // =========================================================================
    // Card 1: Component Optimization
    // =========================================================================
    m_grpCompiler = new QGroupBox(tr("Component Optimization"), content);
    QVBoxLayout *layComp = new QVBoxLayout(m_grpCompiler);
    layComp->setContentsMargins(12, 10, 12, 8);
    layComp->setSpacing(4);

    QWidget *rowCompControl = new QWidget(m_grpCompiler);
    QHBoxLayout *layCompControl = new QHBoxLayout(rowCompControl);
    layCompControl->setContentsMargins(0, 0, 0, 0);
    layCompControl->setSpacing(8);

    m_comboProfile = new QComboBox(rowCompControl);
    m_comboProfile->setView(new QListView(m_comboProfile));
    m_comboProfile->setCursor(Qt::PointingHandCursor);
    m_comboProfile->addItem(tr("System Packages (No Compilation)"), static_cast<int>(Profile::SystemPackages));
    m_comboProfile->addItem(tr("Missing Components Only"), static_cast<int>(Profile::MissingOnly));
    m_comboProfile->addItem(tr("All Stable Components"), static_cast<int>(Profile::AllStable));
    m_comboProfile->addItem(tr("All From Source (Including Unrecommended)"), static_cast<int>(Profile::AllSource));
    if (m_activeProfile == Profile::Custom) {
        m_comboProfile->addItem(tr("Custom (Compiler Wizard)"), static_cast<int>(Profile::Custom));
    }
    connect(m_comboProfile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPerformanceTab::onProfileComboChanged);

    m_btnWizard = new QPushButton(tr("Wizard..."), rowCompControl);
    m_btnWizard->setCursor(Qt::PointingHandCursor);
    m_btnWizard->setToolTip(tr("Open the interactive Compiler Wizard to select fine-grained instructions and optimization flags."));
    connect(m_btnWizard, &QPushButton::clicked, this, &SettingsPerformanceTab::openCompilerWizard);

    layCompControl->addWidget(m_comboProfile, 1);
    layCompControl->addWidget(m_btnWizard);

    QWidget *rowComp = createSettingRow(
        tr("Select which components to optimize"),
        rowCompControl,
        m_grpCompiler
    );

    m_lblOptHint = new QLabel(this);
    m_lblOptHint->setObjectName(QStringLiteral("RowSubtitle"));

    m_lblIsolationNotice = new QLabel(
        tr("🔒 All optimized components reside in your user folder (~/.local), without touching system files or requiring root permissions."),
        m_grpCompiler
    );
    m_lblIsolationNotice->setObjectName(QStringLiteral("RowSubtitle"));
    m_lblIsolationNotice->setWordWrap(true);

    layComp->addWidget(rowComp);
    layComp->addWidget(m_lblOptHint);
    layComp->addWidget(m_lblIsolationNotice);
    layout->addWidget(m_grpCompiler);

    // =========================================================================
    // Card 2: Responsiveness & Hardware Cache (RAM & GPU)
    // =========================================================================
    m_grpAcceleration = new QGroupBox(tr("Responsiveness && Hardware Cache"), content);
    QVBoxLayout *layAccel = new QVBoxLayout(m_grpAcceleration);
    layAccel->setContentsMargins(12, 10, 12, 8);
    layAccel->setSpacing(4);

    m_switchPreload = new CapsuleToggleSwitch(m_grpAcceleration);
    int curDist = PreloadCacheManager::instance().prefetchDistance();
    int curBudget = PreloadCacheManager::instance().budgetMb();
    m_switchPreload->setCheckedAnimated(curDist > 0 && curBudget > 0);
    connect(m_switchPreload, &QAbstractButton::toggled, this, [](bool checked) {
        int dist = checked ? 1 : 0;
        int budget = checked ? 256 : 0;
        PreloadCacheManager::instance().setPrefetchDistance(dist);
        PreloadCacheManager::instance().setBudgetMb(budget);
        QSettings settings("Bubez", "BubezView");
        settings.setValue("prefetch_distance", dist);
        settings.setValue("cache_budget_mb", budget);
    });

    QWidget *rowPreload = createSettingRow(
        tr("Adjacent file pre-caching (256 MB RAM cache)"),
        m_switchPreload,
        m_grpAcceleration
    );

    m_switchGpu = new CapsuleToggleSwitch(m_grpAcceleration);
    m_switchGpu->setCheckedAnimated(GpuAccelerationManager::instance().isGpuAccelerationEnabled());
    connect(m_switchGpu, &QAbstractButton::toggled, this, [](bool checked) {
        GpuAccelerationManager::instance().setGpuAccelerationEnabled(checked);
    });

    QString renderer = GpuAccelerationManager::instance().gpuRenderer();
    QString glVer = GpuAccelerationManager::instance().glVersion();
    QString gpuSub;
    if (!renderer.isEmpty() && renderer != QLatin1String("Unknown")) {
        gpuSub = QStringLiteral("%1 (%2)").arg(renderer, glVer);
    }

    QWidget *rowGpu = createSettingRow(
        tr("Hardware GPU acceleration (OpenGL)"),
        m_switchGpu,
        m_grpAcceleration,
        gpuSub
    );

    layAccel->addWidget(rowPreload);
    layAccel->addWidget(createDivider(m_grpAcceleration));
    layAccel->addWidget(rowGpu);
    layout->addWidget(m_grpAcceleration);

    // =========================================================================
    // Card 3: Fast Restart (Background Standby in RAM)
    // =========================================================================
    m_grpStandby = new QGroupBox(tr("Background Standby"), content);
    QVBoxLayout *layStandby = new QVBoxLayout(m_grpStandby);
    layStandby->setContentsMargins(12, 10, 12, 8);
    layStandby->setSpacing(4);

    QWidget *rowStandbyControl = new QWidget(m_grpStandby);
    QHBoxLayout *layStandbyControl = new QHBoxLayout(rowStandbyControl);
    layStandbyControl->setContentsMargins(0, 0, 0, 0);
    layStandbyControl->setSpacing(8);

    m_comboStandby = new QComboBox(rowStandbyControl);
    m_comboStandby->setView(new QListView(m_comboStandby));
    m_comboStandby->setCursor(Qt::PointingHandCursor);
    m_comboStandby->addItem(tr("Disabled"), 0);
    m_comboStandby->addItem(tr("10 minutes"), 10);
    m_comboStandby->addItem(tr("30 minutes"), 30);
    m_comboStandby->addItem(tr("1 hour"), 60);
    m_comboStandby->addItem(tr("Keep alive"), -1);

    QSettings settings("Bubez", "BubezView");
    int curMinutes = settings.value("background_keep_alive_minutes", 0).toInt();
    int sIdx = m_comboStandby->findData(curMinutes);
    if (sIdx >= 0) {
        m_comboStandby->setCurrentIndex(sIdx);
    } else {
        m_comboStandby->setCurrentIndex(0);
    }
    connect(m_comboStandby, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPerformanceTab::onStandbyComboChanged);

    m_btnResetNotices = new QPushButton(tr("Reset Alerts"), rowStandbyControl);
    m_btnResetNotices->setCursor(Qt::PointingHandCursor);
    m_btnResetNotices->setToolTip(tr("Re-enables any performance fallback notices that were previously silenced."));
    connect(m_btnResetNotices, &QPushButton::clicked, this, [this]() {
        const QStringList components = {"tesseract", "qpdf", "pdfimages", "engine-poppler", "engine-libarchive"};
        for (const QString &c : components) {
            FallbackNoticeDialog::setNoticeSuppressed(c, false);
        }
        QMessageBox::information(this, tr("Alerts Reset"), tr("All performance optimization alerts have been re-enabled."));
    });

    layStandbyControl->addWidget(m_comboStandby, 1);
    layStandbyControl->addWidget(m_btnResetNotices);

    QWidget *rowStandby = createSettingRow(
        tr("Fast Restart"),
        rowStandbyControl,
        m_grpStandby
    );

    layStandby->addWidget(rowStandby);
    layout->addWidget(m_grpStandby);

    layout->addStretch();
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsPerformanceTab::openCompilerWizard()
{
    if (!CpuInfo::isV3OrHigher()) {
        QMessageBox::information(this, tr("Compiler Wizard — CPU Architecture"),
            tr("<h3>Feature Not Supported for this CPU</h3>"
               "<p>The <b>Compiler Wizard</b> requires a 64-bit processor supporting at least the <b>x86-64-v3</b> microarchitecture tier (AVX2, FMA, BMI2) or higher.</p>"
               "<p>Your current CPU tier: <b>x86-64-%1</b><br>"
               "Hardware Model: <i>%2</i></p>"
               "<p>On older processors, fine-grained vector tuning can cause illegal instruction faults. Universal standard compilation (-O2) is used instead.</p>")
               .arg(CpuInfo::microarchitectureTier().toUpper(), CpuInfo::modelName()));
        return;
    }

    CompilerWizardDialog dlg(this);
    connect(&dlg, &CompilerWizardDialog::configurationSaved, this, [this]() {
        m_activeProfile = detectActiveProfile();
        m_optLevel = BuildConfig::instance().optimizationLevel();
        syncProfileUi();
        emit pendingStateChanged();
    });
    connect(&dlg, &CompilerWizardDialog::recompileRequested, this, [this](const QString &target) {
        emit recompileRequested(target);
    });
    connect(&dlg, &CompilerWizardDialog::openExtensionsTabRequested, this, &SettingsPerformanceTab::openExtensionsTabRequested);

    dlg.exec();
}

bool SettingsPerformanceTab::hasPendingChanges() const
{
    bool profileChanged = (m_activeProfile != m_initialProfile);
    bool flagsChanged = (BuildConfig::instance().customCompilerFlags() != m_initialCustomFlags);
    bool cpuChanged = (BuildConfig::instance().cpuProfile() != m_initialCpuProfile);
    bool optChanged = (BuildConfig::instance().optimizationLevel() != m_initialOptLevel);
    return profileChanged || flagsChanged || cpuChanged || optChanged;
}

void SettingsPerformanceTab::revertPendingChanges()
{
    m_activeProfile = m_initialProfile;
    m_optLevel = m_initialOptLevel;
    syncProfileUi();

    BuildConfig &bcfg = BuildConfig::instance();
    switch (m_initialProfile) {
    case Profile::SystemPackages:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::SystemPackages);
        break;
    case Profile::MissingOnly:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::MissingOnly);
        break;
    case Profile::AllStable:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::AllStable);
        break;
    case Profile::AllSource:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::AllSource);
        break;
    case Profile::Custom:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::Custom);
        break;
    }
    bcfg.setCpuProfile(m_initialCpuProfile);
    bcfg.setOptimizationLevel(m_initialOptLevel);
    bcfg.setCustomCompilerFlags(m_initialCustomFlags);
    bcfg.save();
}

void SettingsPerformanceTab::onBuildSucceeded()
{
    m_initialProfile = m_activeProfile;
    m_initialOptLevel = BuildConfig::instance().optimizationLevel();
    m_initialCpuProfile = BuildConfig::instance().cpuProfile();
    m_initialCustomFlags = BuildConfig::instance().customCompilerFlags();
}

void SettingsPerformanceTab::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool dark = ThemeManager::isDarkTheme();

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString titleColor;
    QString subColor;
    QString dividerColor;
    QString comboBg;
    QString comboBorder;
    QString comboPopupBg;
    QString comboSelectionBg;
    QString comboSelectionText;
    QString hoverBorder;
    QString btnBg;
    QString btnHover;

    if (active == ThemePreset::ZincGray) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        titleColor = QStringLiteral("#ffffff");
        subColor = QStringLiteral("rgba(255, 255, 255, 0.70)");
        dividerColor = QStringLiteral("rgba(255, 255, 255, 0.09)");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        comboBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        comboPopupBg = QStringLiteral("#2a2b2e");
        comboSelectionBg = QStringLiteral("#585960");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("rgba(255, 255, 255, 0.40)");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        btnHover = QStringLiteral("#63646c");
    } else if (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.09)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#334155");
        titleColor = QStringLiteral("#1e293b");
        subColor = QStringLiteral("#64748b");
        dividerColor = QStringLiteral("rgba(15, 23, 42, 0.08)");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0)");
        comboBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        comboPopupBg = QStringLiteral("#f8fafc");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#0284c7");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnHover = QStringLiteral("#0ea5e9");
    } else {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        titleColor = QStringLiteral("#f8fafc");
        subColor = QStringLiteral("rgba(226, 232, 240, 0.72)");
        dividerColor = QStringLiteral("rgba(255, 255, 255, 0.08)");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        comboBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        comboPopupBg = QStringLiteral("#151c28");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#38bdf8");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnHover = QStringLiteral("#0ea5e9");
    }

    const QString style = QString(
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "  background: %3;"
        "  margin-top: 10px;"
        "  padding: 10px 12px 8px 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 12px;"
        "  top: 1px;"
        "  padding: 1px 10px;"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel#RowTitle {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  background: transparent;"
        "}"
        "QLabel#RowSubtitle {"
        "  color: %7;"
        "  font-size: 11px;"
        "  font-weight: 400;"
        "  background: transparent;"
        "}"
        "QFrame#RowDivider {"
        "  background-color: %8;"
        "  max-height: 1px;"
        "  min-height: 1px;"
        "  border: none;"
        "  margin: 2px 0px;"
        "}"
        "QPushButton {"
        "  background: %13;"
        "  color: #ffffff;"
        "  border: 1px solid %6;"
        "  border-radius: 8px;"
        "  padding: 4px 12px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  min-height: 22px;"
        "}"
        "QPushButton:hover {"
        "  background: %14;"
        "  border-color: %8;"
        "}"
        "QComboBox {"
        "  background: %7_combo;"
        "  color: %1;"
        "  border: 1px solid %6;"
        "  border-radius: 8px;"
        "  padding: 4px 28px 4px 10px;"
        "  font-size: 12px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %8;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 22px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(%9);"
        "  width: 11px;"
        "  height: 11px;"
        "  margin-right: 5px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %10;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  selection-background-color: %11;"
        "  selection-color: %12;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  min-height: 24px;"
        "  padding: 4px 10px;"
        "  border-radius: 6px;"
        "  color: %1;"
        "  background: transparent;"
        "}"
        "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
        "  background-color: %11;"
        "  color: %12;"
        "}"
    ).replace("%7_combo", comboBg)
     .arg(titleColor, cardBorder, cardBg, pillBg, pillColor, pillBorder, subColor, dividerColor,
          dark ? QStringLiteral(":/icons/chevron_down_light.svg") : QStringLiteral(":/icons/chevron_down_dark.svg"),
          comboPopupBg, comboSelectionBg, comboSelectionText, btnBg, btnHover);

    setStyleSheet(style);

    const QString popupViewStyle = QString(
        "QListView {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QListView::item {"
        "  min-height: 24px;"
        "  padding: 4px 10px;"
        "  border-radius: 6px;"
        "  color: %2;"
        "  background: transparent;"
        "}"
        "QListView::item:hover, QListView::item:selected {"
        "  background-color: %4;"
        "  color: %5;"
        "}"
    ).arg(comboPopupBg, titleColor, cardBorder, comboSelectionBg, comboSelectionText);

    if (m_comboProfile && m_comboProfile->view()) {
        m_comboProfile->view()->setStyleSheet(popupViewStyle);
    }
    if (m_comboStandby && m_comboStandby->view()) {
        m_comboStandby->view()->setStyleSheet(popupViewStyle);
    }

    if (m_switchPreload) m_switchPreload->update();
    if (m_switchGpu) m_switchGpu->update();
}
