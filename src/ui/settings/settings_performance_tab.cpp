/**
 * @file settings_performance_tab.cpp
 * @brief Implementation of the performance and compiler settings tab.
 */

#include "settings_performance_tab.h"
#include "build_config.h"
#include "preload_cache_manager.h"
#include "compiler_wizard_dialog.h"
#include "cpu_info.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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
    m_initialOptLevel = bcfg.isO3() ? "O3" : "O2";
    m_optLevel = m_initialOptLevel;
    m_initialCustomFlags = bcfg.customCompilerFlags();

    setupUi();
}

void SettingsPerformanceTab::applyOptSegmentedStyle(bool isO3)
{
    if (!m_btnOptO2 || !m_btnOptO3) {
        return;
    }
    QString activeStyle = "QPushButton { background-color: #0284c7; color: white; font-weight: bold; border: 1px solid #0284c7; padding: 5px 14px; border-radius: 4px; font-size: 11px; }";
    QString inactiveStyle = "QPushButton { background-color: rgba(100, 116, 139, 0.15); color: #64748b; border: 1px solid rgba(100, 116, 139, 0.25); padding: 5px 14px; border-radius: 4px; font-size: 11px; } QPushButton:hover { background-color: rgba(100, 116, 139, 0.25); color: #334155; }";
    if (isO3) {
        m_btnOptO2->setStyleSheet(inactiveStyle);
        m_btnOptO3->setStyleSheet(activeStyle);
    } else {
        m_btnOptO2->setStyleSheet(activeStyle);
        m_btnOptO3->setStyleSheet(inactiveStyle);
    }
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

    // 1. Compiler Optimization Level (-O2 vs -O3)
    QGroupBox *grpCompiler = new QGroupBox(tr("Compiler Optimization && CPU Tuning"), content);
    QVBoxLayout *layComp = new QVBoxLayout(grpCompiler);
    layComp->setSpacing(8);
    layComp->setContentsMargins(12, 10, 12, 12);

    QFrame *frameComp = new QFrame(grpCompiler);
    frameComp->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.06); border: 1px solid rgba(100, 116, 139, 0.15); border-radius: 6px; }");
    QVBoxLayout *layFrame = new QVBoxLayout(frameComp);
    layFrame->setContentsMargins(10, 8, 10, 8);
    layFrame->setSpacing(8);

    QHBoxLayout *rowOpt = new QHBoxLayout();
    QLabel *lblOpt = new QLabel(tr("Optimization Profile:"), frameComp);
    lblOpt->setStyleSheet("font-weight: bold;");

    m_btnOptO2 = new QPushButton(tr("🛡️ -O2 (Balanced)"), frameComp);
    m_btnOptO2->setCursor(Qt::PointingHandCursor);
    m_btnOptO3 = new QPushButton(tr("🚀 -O3 (Aggressive)"), frameComp);
    m_btnOptO3->setCursor(Qt::PointingHandCursor);

    applyOptSegmentedStyle(m_optLevel == "O3");

    connect(m_btnOptO2, &QPushButton::clicked, this, [this]() {
        m_optLevel = "O2";
        applyOptSegmentedStyle(false);
        emit pendingStateChanged();
    });
    connect(m_btnOptO3, &QPushButton::clicked, this, [this]() {
        m_optLevel = "O3";
        applyOptSegmentedStyle(true);
        emit pendingStateChanged();
    });

    QPushButton *btnOptInfo = new QPushButton("i", frameComp);
    btnOptInfo->setFixedSize(24, 24);
    btnOptInfo->setCursor(Qt::PointingHandCursor);
    btnOptInfo->setToolTip(tr("Detailed information about -O2 vs -O3 optimization"));
    btnOptInfo->setStyleSheet("QPushButton { font-weight: bold; border-radius: 12px; background-color: rgba(100, 116, 139, 0.15); color: #475569; } QPushButton:hover { background-color: #0284c7; color: white; }");
    connect(btnOptInfo, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, tr("Compiler Optimization: -O2 vs -O3"),
            tr("<h3>Compiler Optimization Comparison</h3>"
               "<p><b>🛡️ -O2 (Balanced & Recommended for Laptops):</b><br>"
               "• <b>Best for:</b> Laptops, low-power CPUs, battery operation, and daily use.<br>"
               "• <b>Energy efficiency:</b> Minimizes power draw and lets the CPU quickly return to low-power sleep states.<br>"
               "• <b>Prevents Overheating:</b> Avoids thermal throttling and eliminates loud laptop cooling fan noise.<br>"
               "• <b>Snappy UI & Fast Startup:</b> Generates compact code that easily fits in the CPU's ultra-fast L1 instruction cache, resulting in instant cold launch and lightning-fast navigation for light files.</p>"
               "<p><b>🚀 -O3 (Aggressive / High-Performance Desktop & Large Files):</b><br>"
               "• <b>Best for:</b> Powerful desktop workstations, gaming PCs with robust cooling, and heavy data workflows.<br>"
               "• <b>Heavy Compute & Vectorization:</b> Aggressively vectorizes loops and uses wide SIMD/AVX2 instruction sets.<br>"
               "• <b>Ideal for Large Files:</b> Drastically speeds up rendering of complex multi-page vector PDFs (CAD/architectural drawings), huge RAW/TIFF photos, and heavy spreadsheets.<br>"
               "• <i>Note:</i> On thin laptops, -O3 can trigger thermal throttling, which paradoxically slows down the processor.</p>"));
    });

    rowOpt->addWidget(lblOpt);
    rowOpt->addSpacing(8);
    rowOpt->addWidget(m_btnOptO2);
    rowOpt->addWidget(m_btnOptO3);
    rowOpt->addWidget(btnOptInfo);
    rowOpt->addStretch();
    layFrame->addLayout(rowOpt);

    QHBoxLayout *rowWizard = new QHBoxLayout();
    QVBoxLayout *colWizDesc = new QVBoxLayout();
    colWizDesc->setSpacing(2);
    QLabel *lblWizTitle = new QLabel(tr("<b>Fine-Grained CPU Instructions & Flags:</b>"), frameComp);
    QLabel *lblWizDesc = new QLabel(tr("Customize vector instruction sets (v3/v4), LTO, and hardware tuning."), frameComp);
    lblWizDesc->setStyleSheet("color: #64748b; font-size: 11px;");
    colWizDesc->addWidget(lblWizTitle);
    colWizDesc->addWidget(lblWizDesc);
    rowWizard->addLayout(colWizDesc, 1);

    QPushButton *btnWizard = new QPushButton(tr("🧙 Compiler Wizard..."), frameComp);
    btnWizard->setCursor(Qt::PointingHandCursor);
    btnWizard->setStyleSheet("QPushButton { background-color: rgba(2, 132, 199, 0.1); border: 1px solid rgba(2, 132, 199, 0.35); color: #0284c7; font-weight: bold; padding: 5px 12px; border-radius: 4px; font-size: 11px; } QPushButton:hover { background-color: #0284c7; color: white; }");
    btnWizard->setToolTip(tr("Open interactive wizard to configure fine-grained CPU instructions, v3/v4 tiers, and vectorization flags"));
    connect(btnWizard, &QPushButton::clicked, this, &SettingsPerformanceTab::openCompilerWizard);
    rowWizard->addWidget(btnWizard);
    layFrame->addLayout(rowWizard);

    layComp->addWidget(frameComp);

    QLabel *lblOptHint = new QLabel(tr("Changes to compiler optimization will take effect upon recompile."), grpCompiler);
    lblOptHint->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 2px;");
    layComp->addWidget(lblOptHint);

    layout->addWidget(grpCompiler);

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
    layout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsPerformanceTab::openCompilerWizard()
{
    if (!CpuInfo::isV3OrHigher()) {
        QMessageBox::information(this, tr("Compiler Wizard - CPU Not Supported"),
            tr("<h3>Feature Not Supported for this CPU</h3>"
               "<p>The <b>Compiler Wizard</b> requires a modern 64-bit processor supporting at least the <b>x86-64-v3</b> microarchitecture tier (AVX2, FMA, BMI2) or higher.</p>"
               "<p>Your current CPU tier: <b>x86-64-%1</b><br>"
               "Hardware Model: <i>%2</i></p>"
               "<p>On older processors, fine-grained vector tuning can cause illegal instruction faults or performance regressions. Universal standard compilation (-O2) is used instead.</p>")
               .arg(CpuInfo::microarchitectureTier().toUpper(), CpuInfo::modelName()));
        return;
    }

    CompilerWizardDialog dlg(this);
    connect(&dlg, &CompilerWizardDialog::configurationSaved, this, [this]() {
        emit pendingStateChanged();
    });
    connect(&dlg, &CompilerWizardDialog::recompileRequested, this, &SettingsPerformanceTab::recompileRequested);
    dlg.exec();
}

bool SettingsPerformanceTab::hasPendingChanges() const
{
    bool optChanged = (m_optLevel != m_initialOptLevel);
    bool flagsChanged = (BuildConfig::instance().customCompilerFlags() != m_initialCustomFlags);
    return optChanged || flagsChanged;
}

void SettingsPerformanceTab::revertPendingChanges()
{
    m_optLevel = m_initialOptLevel;
    applyOptSegmentedStyle(m_optLevel == "O3");
    BuildConfig::instance().setCustomCompilerFlags(m_initialCustomFlags);
    BuildConfig::instance().save();
}

void SettingsPerformanceTab::onBuildSucceeded()
{
    m_initialOptLevel = m_optLevel;
    m_initialCustomFlags = BuildConfig::instance().customCompilerFlags();
}
