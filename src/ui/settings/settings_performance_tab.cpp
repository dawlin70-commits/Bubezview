/**
 * @file settings_performance_tab.cpp
 * @brief Implementation of the performance, hardware profiles, and compiler settings tab.
 */

#include "settings_performance_tab.h"
#include "build_config.h"
#include "preload_cache_manager.h"
#include "compiler_wizard_dialog.h"
#include "fallback_notice_dialog.h"
#include "cpu_info.h"
#include "gpu_acceleration_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QFrame>
#include <QMessageBox>
#include <QSettings>
#include <QScrollArea>

SettingsPerformanceTab::SettingsPerformanceTab(QWidget *parent)
    : QWidget(parent)
    , m_optLevel("O2")
{
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.load();
    m_optLevel = bcfg.optimizationLevel();
    if (m_optLevel.isEmpty()) {
        m_optLevel = "O2";
    }
    m_initialOptLevel = m_optLevel;
    m_initialCpuProfile = bcfg.cpuProfile();
    m_initialCustomFlags = bcfg.customCompilerFlags();

    m_activeProfile = detectActiveProfile();
    m_initialProfile = m_activeProfile;

    setupUi();
}

SettingsPerformanceTab::Profile SettingsPerformanceTab::detectActiveProfile() const
{
    BuildConfig &bcfg = BuildConfig::instance();
    BuildConfig::HardwareProfile hp = bcfg.activeHardwareProfile();
    switch (hp) {
    case BuildConfig::HardwareProfile::HighPerformance:
        return Profile::Desktop;
    case BuildConfig::HardwareProfile::Balanced:
        return Profile::Balanced;
    case BuildConfig::HardwareProfile::Custom:
        return Profile::Custom;
    }
    return Profile::Balanced;
}

void SettingsPerformanceTab::updateProfileButtons(Profile active)
{
    m_activeProfile = active;

    const QString activeStyle =
        "QPushButton {"
        "  background-color: #0284c7;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "  border: 1px solid #0284c7;"
        "  padding: 8px 12px;"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "}";

    const QString inactiveStyle =
        "QPushButton {"
        "  background-color: rgba(100, 116, 139, 0.08);"
        "  color: #475569;"
        "  font-weight: 500;"
        "  border: 1px solid rgba(100, 116, 139, 0.2);"
        "  padding: 8px 12px;"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(100, 116, 139, 0.16);"
        "  color: #1e293b;"
        "}";

    if (m_btnProfileDesktop) m_btnProfileDesktop->setStyleSheet(active == Profile::Desktop ? activeStyle : inactiveStyle);
    if (m_btnProfileBalanced) m_btnProfileBalanced->setStyleSheet(active == Profile::Balanced ? activeStyle : inactiveStyle);
    if (m_btnProfileCustom) m_btnProfileCustom->setStyleSheet(active == Profile::Custom ? activeStyle : inactiveStyle);

    if (m_lblProfileDesc) {
        switch (active) {
        case Profile::Desktop:
            m_lblProfileDesc->setText(
                tr("🚀 <b>High Performance (Fully Compiled from Source):</b> All engines and components "
                   "(PDF, Archives, QPDF, OCR) compiled from source with extreme optimizations (-O3, -march=native, LTO, CRC32). "
                   "Expanded 512 MB RAM cache for maximum responsiveness.")
            );
            break;
        case Profile::Balanced:
            m_lblProfileDesc->setText(
                tr("📦 <b>Balanced (System Packages):</b> Uses precompiled distribution packages and shared libraries "
                   "provided by your Linux distro. Zero compilation time, low CPU usage, and balanced memory footprint.")
            );
            break;
        case Profile::Custom:
            m_lblProfileDesc->setText(
                tr("🧙 <b>Custom Configuration:</b> Microarchitecture CPU flags, instruction sets, and component build modes "
                   "manually tuned via the Compiler Wizard.")
            );
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
    case Profile::Desktop:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::HighPerformance);
        break;
    case Profile::Balanced:
        bcfg.applyHardwareProfile(BuildConfig::HardwareProfile::Balanced);
        break;
    default:
        break;
    }

    // Refresh preload checkbox to reflect profile cache settings
    if (m_chkEnablePreload) {
        QSettings settings("Bubez", "BubezView");
        int curDist = settings.value("prefetch_distance", 1).toInt();
        m_chkEnablePreload->setChecked(curDist > 0);
    }

    updateProfileButtons(profile);
    emit pendingStateChanged();
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
    layout->setSpacing(14);
    layout->setContentsMargins(14, 14, 14, 14);

    // 1. Hardware Performance Profile Group
    QGroupBox *grpCompiler = new QGroupBox(tr("Hardware Performance Profile && CPU Tuning"), content);
    QVBoxLayout *layComp = new QVBoxLayout(grpCompiler);
    layComp->setSpacing(10);
    layComp->setContentsMargins(12, 10, 12, 12);

    QFrame *frameComp = new QFrame(grpCompiler);
    frameComp->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.05); border: 1px solid rgba(100, 116, 139, 0.15); border-radius: 8px; }");
    QVBoxLayout *layFrame = new QVBoxLayout(frameComp);
    layFrame->setContentsMargins(12, 12, 12, 12);
    layFrame->setSpacing(10);

    QHBoxLayout *rowButtons = new QHBoxLayout();
    rowButtons->setSpacing(8);

    m_btnProfileDesktop = new QPushButton(tr("🚀 High Performance (Compiled)"), frameComp);
    m_btnProfileDesktop->setCursor(Qt::PointingHandCursor);
    m_btnProfileDesktop->setToolTip(tr("All engines compiled from source with extreme optimization and 512 MB expanded RAM cache."));
    connect(m_btnProfileDesktop, &QPushButton::clicked, this, [this]() {
        selectProfile(Profile::Desktop);
    });

    m_btnProfileBalanced = new QPushButton(tr("📦 Balanced (Packages)"), frameComp);
    m_btnProfileBalanced->setCursor(Qt::PointingHandCursor);
    m_btnProfileBalanced->setToolTip(tr("All components based on precompiled system packages with zero build waiting time."));
    connect(m_btnProfileBalanced, &QPushButton::clicked, this, [this]() {
        selectProfile(Profile::Balanced);
    });

    m_btnProfileCustom = new QPushButton(tr("🧙 Custom..."), frameComp);
    m_btnProfileCustom->setCursor(Qt::PointingHandCursor);
    m_btnProfileCustom->setToolTip(tr("Custom configuration: opens the Compiler Wizard to choose fine-grained options."));
    connect(m_btnProfileCustom, &QPushButton::clicked, this, [this]() {
        selectProfile(Profile::Custom);
    });

    rowButtons->addWidget(m_btnProfileDesktop);
    rowButtons->addWidget(m_btnProfileBalanced);
    rowButtons->addWidget(m_btnProfileCustom);
    layFrame->addLayout(rowButtons);

    m_lblProfileDesc = new QLabel(frameComp);
    m_lblProfileDesc->setWordWrap(true);
    m_lblProfileDesc->setStyleSheet("color: #475569; font-size: 11px; padding: 4px 2px;");
    layFrame->addWidget(m_lblProfileDesc);

    layComp->addWidget(frameComp);

    QLabel *lblOptHint = new QLabel(tr("Changes to compiler optimization will take effect upon recompile."), grpCompiler);
    lblOptHint->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 2px;");
    layComp->addWidget(lblOptHint);

    layout->addWidget(grpCompiler);

    updateProfileButtons(m_activeProfile);

    // 2. Adjacent File Pre-caching (The Preload Trick)
    QGroupBox *grpPreload = new QGroupBox(tr("Adjacent File Pre-caching (Instant Next/Prev)"), content);
    QVBoxLayout *layPreload = new QVBoxLayout(grpPreload);
    layPreload->setSpacing(6);
    layPreload->setContentsMargins(12, 10, 12, 12);

    m_chkEnablePreload = new QCheckBox(tr("Enable instant adjacent file pre-caching (256 MB RAM cache)"), grpPreload);
    m_chkEnablePreload->setCursor(Qt::PointingHandCursor);
    m_chkEnablePreload->setStyleSheet("QCheckBox { font-weight: bold; }");

    int curDist = PreloadCacheManager::instance().prefetchDistance();
    int curBudget = PreloadCacheManager::instance().budgetMb();
    bool isPreloadActive = (curDist > 0 && curBudget > 0);
    m_chkEnablePreload->setChecked(isPreloadActive);

    QLabel *lblPreloadDesc = new QLabel(
        tr("Silently preloads the previous and next file in memory in the background so switching between files is instantaneous (0ms latency). Disabling saves RAM on low-memory devices."),
        grpPreload
    );
    lblPreloadDesc->setStyleSheet("color: #64748b; font-size: 11px;");
    lblPreloadDesc->setWordWrap(true);

    connect(m_chkEnablePreload, &QCheckBox::toggled, this, [](bool checked) {
        int dist = checked ? 1 : 0;
        int budget = checked ? 256 : 0;
        PreloadCacheManager::instance().setPrefetchDistance(dist);
        PreloadCacheManager::instance().setBudgetMb(budget);
        QSettings settings("Bubez", "BubezView");
        settings.setValue("prefetch_distance", dist);
        settings.setValue("cache_budget_mb", budget);
    });

    layPreload->addWidget(m_chkEnablePreload);
    layPreload->addWidget(lblPreloadDesc);
    layout->addWidget(grpPreload);

    // 3. Background Standby (Instant Reopen)
    QGroupBox *grpStandby = new QGroupBox(tr("Background Standby on Close (0ms Reopening)"), content);
    QVBoxLayout *layStandby = new QVBoxLayout(grpStandby);
    layStandby->setSpacing(8);
    layStandby->setContentsMargins(12, 10, 12, 12);

    QHBoxLayout *rowStandby = new QHBoxLayout();
    QLabel *lblStandbyTitle = new QLabel(tr("Standby Duration:"), grpStandby);
    lblStandbyTitle->setStyleSheet("font-weight: bold;");

    m_btnStandbyMinus = new QPushButton("−", grpStandby);
    m_btnStandbyMinus->setFixedSize(28, 28);
    m_btnStandbyMinus->setCursor(Qt::PointingHandCursor);

    m_btnStandbyPlus = new QPushButton("+", grpStandby);
    m_btnStandbyPlus->setFixedSize(28, 28);
    m_btnStandbyPlus->setCursor(Qt::PointingHandCursor);

    QString stepperBtnStyle =
        "QPushButton { font-size: 15px; font-weight: bold; border-radius: 4px; background-color: rgba(100, 116, 139, 0.15); border: 1px solid rgba(100, 116, 139, 0.25); color: #334155; } "
        "QPushButton:hover { background-color: rgba(2, 132, 199, 0.2); border-color: #0284c7; color: #0284c7; } "
        "QPushButton:disabled { background-color: rgba(100, 116, 139, 0.06); border-color: rgba(100, 116, 139, 0.1); color: #94a3b8; }";
    m_btnStandbyMinus->setStyleSheet(stepperBtnStyle);
    m_btnStandbyPlus->setStyleSheet(stepperBtnStyle);

    m_lblStandbyValue = new QLabel(grpStandby);
    m_lblStandbyValue->setStyleSheet("font-weight: bold; color: #0284c7; font-size: 12px; margin-left: 6px;");

    QSettings settings("Bubez", "BubezView");
    m_standbyMinutes = settings.value("background_keep_alive_minutes", 0).toInt();
    if (m_standbyMinutes < 0) m_standbyMinutes = 0;
    if (m_standbyMinutes > 10) m_standbyMinutes = 10;

    auto updateStandbyUi = [this]() {
        m_btnStandbyMinus->setEnabled(m_standbyMinutes > 0);
        m_btnStandbyPlus->setEnabled(m_standbyMinutes < 10);
        if (m_standbyMinutes == 0) {
            m_lblStandbyValue->setText(tr("0 min (Disabled / Quits immediately)"));
        } else {
            m_lblStandbyValue->setText(QString(tr("%1 min (Instant reopen)")).arg(m_standbyMinutes));
        }
    };
    updateStandbyUi();

    connect(m_btnStandbyMinus, &QPushButton::clicked, this, [this, updateStandbyUi]() {
        if (m_standbyMinutes > 0) {
            m_standbyMinutes--;
            updateStandbyUi();
            QSettings s("Bubez", "BubezView");
            s.setValue("background_keep_alive_minutes", m_standbyMinutes);
        }
    });

    connect(m_btnStandbyPlus, &QPushButton::clicked, this, [this, updateStandbyUi]() {
        if (m_standbyMinutes < 10) {
            m_standbyMinutes++;
            updateStandbyUi();
            QSettings s("Bubez", "BubezView");
            s.setValue("background_keep_alive_minutes", m_standbyMinutes);
        }
    });

    rowStandby->addWidget(lblStandbyTitle);
    rowStandby->addSpacing(4);
    rowStandby->addWidget(m_btnStandbyMinus);
    rowStandby->addWidget(m_btnStandbyPlus);
    rowStandby->addWidget(m_lblStandbyValue);
    rowStandby->addStretch();
    layStandby->addLayout(rowStandby);

    QLabel *lblStandbyHint = new QLabel(
        tr("When set above 0 min, closing BubezView keeps the process silently in RAM for instant reopening when pressing Space in your file manager."),
        grpStandby
    );
    lblStandbyHint->setStyleSheet("color: #64748b; font-size: 11px;");
    lblStandbyHint->setWordWrap(true);
    layStandby->addWidget(lblStandbyHint);

    layout->addWidget(grpStandby);

    // 4. Hardware GPU Acceleration (OpenGL)
    QGroupBox *grpGpu = new QGroupBox(tr("Hardware GPU Acceleration (OpenGL)"), content);
    QVBoxLayout *layGpu = new QVBoxLayout(grpGpu);
    layGpu->setSpacing(8);
    layGpu->setContentsMargins(12, 10, 12, 12);

    m_chkEnableGpu = new QCheckBox(tr("Enable GPU Hardware Acceleration"), grpGpu);
    m_chkEnableGpu->setCursor(Qt::PointingHandCursor);
    m_chkEnableGpu->setStyleSheet("QCheckBox { font-weight: bold; }");
    m_chkEnableGpu->setChecked(GpuAccelerationManager::instance().isGpuAccelerationEnabled());

    QFrame *frameGpu = new QFrame(grpGpu);
    frameGpu->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.05); border: 1px solid rgba(100, 116, 139, 0.15); border-radius: 8px; }");
    QGridLayout *gridGpu = new QGridLayout(frameGpu);
    gridGpu->setContentsMargins(10, 10, 10, 10);
    gridGpu->setHorizontalSpacing(14);
    gridGpu->setVerticalSpacing(6);

    auto *lblGpuTitle = new QLabel(tr("Graphics Device:"), frameGpu);
    lblGpuTitle->setStyleSheet("font-weight: 600; color: #64748b; font-size: 11px;");
    gridGpu->addWidget(lblGpuTitle, 0, 0);

    m_lblGpuRenderer = new QLabel(GpuAccelerationManager::instance().gpuRenderer(), frameGpu);
    m_lblGpuRenderer->setStyleSheet("font-weight: bold; color: #0284c7; font-size: 11px;");
    gridGpu->addWidget(m_lblGpuRenderer, 0, 1);

    auto *lblDriverTitle = new QLabel(tr("Driver / API:"), frameGpu);
    lblDriverTitle->setStyleSheet("font-weight: 600; color: #64748b; font-size: 11px;");
    gridGpu->addWidget(lblDriverTitle, 1, 0);

    m_lblGpuDriver = new QLabel(GpuAccelerationManager::instance().glVersion(), frameGpu);
    m_lblGpuDriver->setStyleSheet("color: #475569; font-size: 11px;");
    gridGpu->addWidget(m_lblGpuDriver, 1, 1);

    auto *lblVramTitle = new QLabel(tr("VRAM Footprint:"), frameGpu);
    lblVramTitle->setStyleSheet("font-weight: 600; color: #64748b; font-size: 11px;");
    gridGpu->addWidget(lblVramTitle, 2, 0);

    m_lblGpuVramNotice = new QLabel(
        tr("<b>~40–80 MB</b> (Lightweight UI textures only — zero interference with local AI / LLM models)"),
        frameGpu
    );
    m_lblGpuVramNotice->setStyleSheet("color: #16a34a; font-size: 11px;");
    gridGpu->addWidget(m_lblGpuVramNotice, 2, 1);

    layGpu->addWidget(m_chkEnableGpu);
    layGpu->addWidget(frameGpu);

    QLabel *lblGpuDesc = new QLabel(
        tr("Hardware acceleration uses the GPU for instant bilinear image zooming, panning, video playback, and smooth 60–144 Hz rendering."),
        grpGpu
    );
    lblGpuDesc->setStyleSheet("color: #64748b; font-size: 11px;");
    lblGpuDesc->setWordWrap(true);
    layGpu->addWidget(lblGpuDesc);

    connect(m_chkEnableGpu, &QCheckBox::toggled, this, [](bool checked) {
        GpuAccelerationManager::instance().setGpuAccelerationEnabled(checked);
    });

    layout->addWidget(grpGpu);

    // 5. Performance Notices Management
    QGroupBox *grpNotices = new QGroupBox(tr("Optimization Alerts"), content);
    QVBoxLayout *layNotices = new QVBoxLayout(grpNotices);
    layNotices->setSpacing(8);
    layNotices->setContentsMargins(12, 10, 12, 12);

    QLabel *lblNoticesDesc = new QLabel(
        tr("When using the High-Performance profile, BubezView alerts you if an individual tool (like OCR or PDF repair) is running via an unoptimized system package rather than a custom-compiled source binary."),
        grpNotices
    );
    lblNoticesDesc->setStyleSheet("color: #64748b; font-size: 11px;");
    lblNoticesDesc->setWordWrap(true);
    layNotices->addWidget(lblNoticesDesc);

    QPushButton *btnResetNotices = new QPushButton(tr("Reset All Optimization Alerts"), grpNotices);
    btnResetNotices->setCursor(Qt::PointingHandCursor);
    connect(btnResetNotices, &QPushButton::clicked, this, [this]() {
        const QStringList components = {"tesseract", "qpdf", "pdfimages", "engine-poppler", "engine-libarchive"};
        for (const QString &c : components) {
            FallbackNoticeDialog::setNoticeSuppressed(c, false);
        }
        QMessageBox::information(this, tr("Alerts Reset"), tr("All performance optimization alerts have been re-enabled."));
    });
    layNotices->addWidget(btnResetNotices, 0, Qt::AlignLeft);

    layout->addWidget(grpNotices);
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
        updateProfileButtons(m_activeProfile);
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
    updateProfileButtons(m_activeProfile);

    BuildConfig &bcfg = BuildConfig::instance();
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
