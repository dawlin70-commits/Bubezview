/**
 * @file compiler_wizard_dialog.cpp
 * @brief Implementation of the interactive Compiler Wizard and flag inspector.
 */

#include "compiler_wizard_dialog.h"
#include "cpu_info.h"
#include "build_config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QTimer>

CompilerWizardDialog::CompilerWizardDialog(QWidget *parent)
    : QDialog(parent)
    , m_lblCpuModel(nullptr)
    , m_lblCpuTier(nullptr)
    , m_lblCpuCores(nullptr)
    , m_lblCpuCache(nullptr)
    , m_comboPreset(nullptr)
    , m_txtPreview(nullptr)
    , m_btnCopy(nullptr)
    , m_btnSave(nullptr)
    , m_btnSaveRecompile(nullptr)
    , m_btnCancel(nullptr)
{
    setWindowTitle(tr("🧙 Compiler Wizard - Advanced CPU & Instruction Tuning"));
    resize(820, 680);
    setMinimumSize(720, 560);

    initDatabase();
    setupUi();

    // Check if there are already saved flags in BuildConfig
    QString savedFlags = BuildConfig::instance().customCompilerFlags().trimmed();
    if (!savedFlags.isEmpty()) {
        QStringList flagTokens = savedFlags.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < m_flags.size(); ++i) {
            bool found = flagTokens.contains(m_flags[i].flag);
            m_checkboxes[i]->setChecked(found);
        }
    } else {
        // Default to Recommended preset
        applyPreset(1);
    }

    updatePreview();
}

CompilerWizardDialog::~CompilerWizardDialog() = default;

void CompilerWizardDialog::initDatabase()
{
    m_flags.clear();

    // -------------------------------------------------------------------------
    // 1. GREEN FLAGS (Recommended, Safe, and Pertinent to BubezView)
    // -------------------------------------------------------------------------
    m_flags.append({
        QStringLiteral("-march=native"),
        tr("Native Host Microarchitecture"),
        tr("Architecture & CPU Tuning"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Highly Recommended.</b> Optimizes BubezView specifically for your CPU model. Makes zooming, scrolling, and image loading noticeably faster and smoother."),
        QStringLiteral("all"),
        true,
        QStringLiteral("arch")
    });

    m_flags.append({
        QStringLiteral("-O2"),
        tr("Standard Balanced Optimization"),
        tr("Optimization Levels"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Balanced & Safe.</b> Standard compiler optimization recommended for all systems. Keeps the application fast, responsive, and resource-efficient."),
        QStringLiteral("all"),
        false,
        QStringLiteral("opt")
    });

    m_flags.append({
        QStringLiteral("-O3"),
        tr("Aggressive Optimization for Heavy Files"),
        tr("Optimization Levels"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended for Heavy Files.</b> Maximizes processing speed. Highly recommended if you frequently open high-resolution photos, large RAW/TIFF files, or complex PDF documents."),
        QStringLiteral("all"),
        true,
        QStringLiteral("opt")
    });

    m_flags.append({
        QStringLiteral("-flto=auto"),
        tr("Link-Time Optimization (LTO)"),
        tr("Linker & Whole-Program"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Optimizes the entire application during the final linking step. Produces a smaller binary and makes the user interface faster and more responsive to clicks."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fno-semantic-interposition"),
        tr("Direct Function Calling (Bypass PLT)"),
        tr("C++ & Compiler Internals"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Speeds up internal function calls between UI elements and viewer components, ensuring instant window and menu response."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-pipe"),
        tr("In-Memory Compilation Pipes"),
        tr("Build Speed"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Safe & Fast.</b> Speeds up compilation and reduces disk wear by keeping temporary build data in RAM instead of writing temporary files to disk."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fomit-frame-pointer"),
        tr("Omit Stack Frame Pointer"),
        tr("Register Allocation"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Frees up internal processor resources to dedicate more computing power to image processing and pixel rendering."),
        QStringLiteral("all"),
        true,
        QString()
    });

    // -------------------------------------------------------------------------
    // 2. YELLOW FLAGS (Conditional, Specialized Use-Cases, Minor Trade-offs)
    // -------------------------------------------------------------------------
    m_flags.append({
        QStringLiteral("-funroll-loops"),
        tr("Aggressive Loop Unrolling"),
        tr("Vectorization & Loops"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Optional.</b> Speeds up repetitive pixel operations (like filters and blur) by unrolling loops, but slightly increases executable size. Recommended if you frequently process very large images."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fno-math-errno"),
        tr("Fast Math Without Error Checking"),
        tr("Floating-Point Math"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Optional.</b> Accelerates geometry calculations during zoom and rotation by skipping redundant system error checks. Safe for BubezView because image boundaries and data are already validated."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fdevirtualize-at-ltrans"),
        tr("Aggressive LTO Devirtualization"),
        tr("Linker & C++ Polymorphism"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Optional.</b> Can slightly improve switching speed between different viewers, but requires more RAM during compilation."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-march=x86-64-v3"),
        tr("Generic x86-64-v3 Microarchitecture Baseline"),
        tr("Portability"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Portable Alternative.</b> Use this only if you plan to copy the compiled binary to another modern PC. If compiling solely for your own computer, -march=native is strictly superior."),
        QStringLiteral("v3"),
        false,
        QStringLiteral("arch")
    });

    m_flags.append({
        QStringLiteral("-march=x86-64-v4"),
        tr("Generic x86-64-v4 Microarchitecture Baseline (AVX-512)"),
        tr("Portability"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>AVX-512 Portable.</b> Appropriate for Zen 4/5 or modern Intel systems if you plan to distribute the binary to other computers equipped with AVX-512 instructions."),
        QStringLiteral("v4"),
        false,
        QStringLiteral("arch")
    });

    m_flags.append({
        QStringLiteral("-fvect-cost-model=dynamic"),
        tr("Dynamic Vectorization Heuristics"),
        tr("Vectorization Models"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Optional.</b> Optimizes CPU workload dynamically when processing images of varying or unpredictable dimensions."),
        QStringLiteral("all"),
        false,
        QString()
    });

    // -------------------------------------------------------------------------
    // 3. RED FLAGS (Hazardous, Precision Loss, Potential Instability)
    // -------------------------------------------------------------------------
    m_flags.append({
        QStringLiteral("-ffast-math"),
        tr("Non-Standard Fast Math (Precision Trade-off)"),
        tr("Math & Precision (Hazardous)"),
        FlagSafetyLevel::Hazardous,
        tr("🔴 <b>Caution (Precision Trade-off).</b> Delivers high calculation speed but bypasses standard math rules: may cause slight pixel color variations, line misalignment in vector PDFs, or glitches with damaged files."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-Ofast"),
        tr("Extreme Optimization (-O3 + Fast Math)"),
        tr("Aggressive Profiles"),
        FlagSafetyLevel::Hazardous,
        tr("🔴 <b>Caution.</b> Combines -O3 with non-standard fast math. Not recommended if you need pixel-perfect rendering and exact color accuracy."),
        QStringLiteral("all"),
        false,
        QStringLiteral("opt")
    });

    m_flags.append({
        QStringLiteral("-fno-finite-math-only"),
        tr("Assume Unchecked Finite Numbers"),
        tr("Math & Precision (Hazardous)"),
        FlagSafetyLevel::Hazardous,
        tr("🔴 <b>Hazardous.</b> Can cause unexpected freezes or crashes if damaged image files or invalid PDF streams encounter division by zero."),
        QStringLiteral("all"),
        false,
        QString()
    });
}

void CompilerWizardDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // -------------------------------------------------------------------------
    // 1. Host Hardware Info Card
    // -------------------------------------------------------------------------
    QFrame *cardCpu = new QFrame(this);
    cardCpu->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.08); border-radius: 8px; border: 1px solid rgba(100, 116, 139, 0.2); }");
    QVBoxLayout *layCard = new QVBoxLayout(cardCpu);
    layCard->setContentsMargins(14, 12, 14, 12);
    layCard->setSpacing(6);

    QHBoxLayout *rowTitle = new QHBoxLayout();
    QLabel *lblCardTitle = new QLabel(tr("🖥️ <b>Host CPU Architecture & Microarchitecture Introspection</b>"), cardCpu);
    lblCardTitle->setStyleSheet("font-size: 13px;");
    rowTitle->addWidget(lblCardTitle);
    rowTitle->addStretch();

    QString detectedTier = CpuInfo::microarchitectureTier().toUpper();
    QLabel *lblBadgeTier = new QLabel(QString(" Tier: x86-64-%1 ").arg(detectedTier), cardCpu);
    lblBadgeTier->setStyleSheet("background-color: #0284c7; color: white; border-radius: 4px; font-weight: bold; font-size: 11px; padding: 2px 6px;");
    rowTitle->addWidget(lblBadgeTier);
    layCard->addLayout(rowTitle);

    QGridLayout *gridInfo = new QGridLayout();
    gridInfo->setHorizontalSpacing(16);
    gridInfo->setVerticalSpacing(4);

    QLabel *lblModelTitle = new QLabel(tr("Model:"), cardCpu);
    lblModelTitle->setStyleSheet("color: #64748b; font-size: 12px;");
    m_lblCpuModel = new QLabel(CpuInfo::modelName(), cardCpu);
    m_lblCpuModel->setStyleSheet("font-weight: bold; font-size: 12px;");

    QLabel *lblCoresTitle = new QLabel(tr("Logical Cores:"), cardCpu);
    lblCoresTitle->setStyleSheet("color: #64748b; font-size: 12px;");
    m_lblCpuCores = new QLabel(QString("%1 threads").arg(CpuInfo::logicalCores()), cardCpu);
    m_lblCpuCores->setStyleSheet("font-size: 12px;");

    QLabel *lblCacheTitle = new QLabel(tr("Cache Hierarchy:"), cardCpu);
    lblCacheTitle->setStyleSheet("color: #64748b; font-size: 12px;");
    m_lblCpuCache = new QLabel(CpuInfo::cacheSummary(), cardCpu);
    m_lblCpuCache->setStyleSheet("color: #0284c7; font-family: monospace; font-size: 11px; font-weight: bold;");

    gridInfo->addWidget(lblModelTitle, 0, 0);
    gridInfo->addWidget(m_lblCpuModel, 0, 1);
    gridInfo->addWidget(lblCoresTitle, 0, 2);
    gridInfo->addWidget(m_lblCpuCores, 0, 3);
    gridInfo->addWidget(lblCacheTitle, 1, 0);
    gridInfo->addWidget(m_lblCpuCache, 1, 1, 1, 3);

    layCard->addLayout(gridInfo);
    mainLayout->addWidget(cardCpu);

    // -------------------------------------------------------------------------
    // 2. Legend & Presets Toolbar
    // -------------------------------------------------------------------------
    QHBoxLayout *rowToolbar = new QHBoxLayout();

    QHBoxLayout *layLeg = new QHBoxLayout();
    layLeg->setSpacing(4);
    QLabel *lblLegTitle = new QLabel(tr("<b>Legend:</b>"), this);
    lblLegTitle->setStyleSheet("font-size: 11px; color: #64748b; margin-right: 2px;");
    layLeg->addWidget(lblLegTitle);

    QLabel *dotG = new QLabel("●", this);
    dotG->setStyleSheet("color: #22c55e; font-size: 14px;");
    QLabel *txtG = new QLabel(tr("Safe & Recommended"), this);
    txtG->setStyleSheet("font-size: 11px; color: #64748b; margin-right: 6px;");
    layLeg->addWidget(dotG);
    layLeg->addWidget(txtG);

    QLabel *dotY = new QLabel("●", this);
    dotY->setStyleSheet("color: #eab308; font-size: 14px;");
    QLabel *txtY = new QLabel(tr("Conditional"), this);
    txtY->setStyleSheet("font-size: 11px; color: #64748b; margin-right: 6px;");
    layLeg->addWidget(dotY);
    layLeg->addWidget(txtY);

    QLabel *dotR = new QLabel("●", this);
    dotR->setStyleSheet("color: #ef4444; font-size: 14px;");
    QLabel *txtR = new QLabel(tr("Hazardous"), this);
    txtR->setStyleSheet("font-size: 11px; color: #64748b;");
    layLeg->addWidget(dotR);
    layLeg->addWidget(txtR);

    rowToolbar->addLayout(layLeg);
    rowToolbar->addStretch();

    QLabel *lblPreset = new QLabel(tr("Quick Profile:"), this);
    lblPreset->setStyleSheet("font-size: 12px; color: #64748b;");
    rowToolbar->addWidget(lblPreset);

    m_comboPreset = new QComboBox(this);
    m_comboPreset->setCursor(Qt::PointingHandCursor);
    m_comboPreset->addItem(tr("🛡️ Standard Baseline (-O2)"), 0);
    m_comboPreset->addItem(tr("⚡ Performance Recommended (Green Only)"), 1);
    m_comboPreset->addItem(tr("🚀 Extreme Enthusiast (Green + Yellow)"), 2);
    m_comboPreset->addItem(tr("🧹 Custom / Clear All"), 3);
    m_comboPreset->setCurrentIndex(1);
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompilerWizardDialog::applyPreset);
    rowToolbar->addWidget(m_comboPreset);

    mainLayout->addLayout(rowToolbar);

    // -------------------------------------------------------------------------
    // 3. Scrollable List of Compiler Flags
    // -------------------------------------------------------------------------
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *layFlags = new QVBoxLayout(scrollContent);
    layFlags->setContentsMargins(4, 4, 4, 4);
    layFlags->setSpacing(6);

    QString currentCategory;

    for (int i = 0; i < m_flags.size(); ++i) {
        const CompilerFlagDef &f = m_flags[i];

        if (f.category != currentCategory) {
            currentCategory = f.category;
            QLabel *lblCat = new QLabel(QString("<b>%1</b>").arg(currentCategory), scrollContent);
            lblCat->setStyleSheet("font-size: 12px; color: #0284c7; margin-top: 8px; margin-bottom: 2px;");
            layFlags->addWidget(lblCat);
        }

        QFrame *rowFrame = new QFrame(scrollContent);
        rowFrame->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.05); border-radius: 6px; border: 1px solid rgba(100, 116, 139, 0.15); padding: 2px; }"
                                "QFrame:hover { border: 1px solid rgba(2, 132, 199, 0.4); background-color: rgba(100, 116, 139, 0.09); }");
        QHBoxLayout *layRow = new QHBoxLayout(rowFrame);
        layRow->setContentsMargins(8, 6, 8, 6);
        layRow->setSpacing(8);

        // Safety Badge Circle Dot (CSS styled, crisp across all distros)
        QLabel *lblDot = new QLabel(rowFrame);
        lblDot->setFixedSize(14, 14);
        if (f.safety == FlagSafetyLevel::Recommended) {
            lblDot->setStyleSheet("background-color: #22c55e; border-radius: 7px; border: 1px solid #16a34a;");
            lblDot->setToolTip(tr("Recommended & Safe: Highly pertinent to BubezView"));
        } else if (f.safety == FlagSafetyLevel::Conditional) {
            lblDot->setStyleSheet("background-color: #eab308; border-radius: 7px; border: 1px solid #ca8a04;");
            lblDot->setToolTip(tr("Conditional: Situational trade-offs"));
        } else {
            lblDot->setStyleSheet("background-color: #ef4444; border-radius: 7px; border: 1px solid #dc2626;");
            lblDot->setToolTip(tr("Hazardous: Potential instability or non-standard math"));
        }
        layRow->addWidget(lblDot);

        // Checkbox with Flag
        QCheckBox *chk = new QCheckBox(f.flag, rowFrame);
        chk->setCursor(Qt::PointingHandCursor);
        chk->setStyleSheet("QCheckBox { font-family: monospace; font-weight: bold; font-size: 12px; }");
        int flagIdx = i;
        connect(chk, &QCheckBox::toggled, this, [this, flagIdx](bool checked) {
            onFlagToggled(flagIdx, checked);
        });
        m_checkboxes.append(chk);
        layRow->addWidget(chk);

        // Title and Tier Badge
        QLabel *lblDesc = new QLabel(QString("— %1").arg(f.title), rowFrame);
        lblDesc->setStyleSheet("color: #64748b; font-size: 11px;");
        layRow->addWidget(lblDesc, 1);

        if (f.requiredTier != "all") {
            QLabel *lblTier = new QLabel(QString(" %1 ").arg(f.requiredTier.toUpper()), rowFrame);
            lblTier->setStyleSheet("background-color: rgba(2, 132, 199, 0.15); color: #0284c7; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 1px 5px;");
            layRow->addWidget(lblTier);
        }

        // Info Button matching SettingsDialog
        QPushButton *btnInfo = new QPushButton("i", rowFrame);
        btnInfo->setFixedSize(24, 24);
        btnInfo->setCursor(Qt::PointingHandCursor);
        btnInfo->setStyleSheet("QPushButton { font-weight: bold; border-radius: 12px; background-color: rgba(100, 116, 139, 0.15); color: #475569; } QPushButton:hover { background-color: #0284c7; color: white; }");
        btnInfo->setToolTip(tr("Detailed technical information about %1").arg(f.flag));
        connect(btnInfo, &QPushButton::clicked, this, [this, i]() {
            showFlagInfo(i);
        });
        layRow->addWidget(btnInfo);

        layFlags->addWidget(rowFrame);
    }

    layFlags->addStretch();
    scrollContent->setLayout(layFlags);
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // -------------------------------------------------------------------------
    // 4. Real-time CMAKE_CXX_FLAGS Preview Box
    // -------------------------------------------------------------------------
    QFrame *previewCard = new QFrame(this);
    previewCard->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.08); border: 1px solid rgba(100, 116, 139, 0.2); border-radius: 6px; }");
    QVBoxLayout *layPreview = new QVBoxLayout(previewCard);
    layPreview->setContentsMargins(10, 8, 10, 8);
    layPreview->setSpacing(6);

    QHBoxLayout *rowPrevTitle = new QHBoxLayout();
    QLabel *lblPrevTitle = new QLabel(tr("<b>Generated Compiler Command (CMAKE_CXX_FLAGS):</b>"), previewCard);
    lblPrevTitle->setStyleSheet("font-size: 11px; color: #64748b;");
    rowPrevTitle->addWidget(lblPrevTitle);
    rowPrevTitle->addStretch();

    m_btnCopy = new QPushButton(tr("📋 Copy"), previewCard);
    m_btnCopy->setFixedHeight(24);
    m_btnCopy->setCursor(Qt::PointingHandCursor);
    m_btnCopy->setStyleSheet("QPushButton { background-color: rgba(100, 116, 139, 0.15); color: inherit; border: 1px solid rgba(100, 116, 139, 0.25); padding: 2px 10px; border-radius: 4px; font-size: 11px; } QPushButton:hover { background-color: rgba(100, 116, 139, 0.25); }");
    connect(m_btnCopy, &QPushButton::clicked, this, &CompilerWizardDialog::copyToClipboard);
    rowPrevTitle->addWidget(m_btnCopy);
    layPreview->addLayout(rowPrevTitle);

    m_txtPreview = new QTextEdit(previewCard);
    m_txtPreview->setReadOnly(true);
    m_txtPreview->setFixedHeight(48);
    m_txtPreview->setStyleSheet("background: transparent; border: none; color: #0284c7; font-family: monospace; font-size: 12px; font-weight: bold;");
    layPreview->addWidget(m_txtPreview);

    mainLayout->addWidget(previewCard);

    // -------------------------------------------------------------------------
    // 5. Action Buttons (Save, Recompile, Cancel)
    // -------------------------------------------------------------------------
    QHBoxLayout *rowActions = new QHBoxLayout();

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet("QPushButton { background-color: rgba(100, 116, 139, 0.15); color: inherit; border: 1px solid rgba(100, 116, 139, 0.25); padding: 6px 14px; border-radius: 4px; font-size: 11px; } QPushButton:hover { background-color: rgba(100, 116, 139, 0.25); }");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    rowActions->addWidget(m_btnCancel);

    rowActions->addStretch();

    m_btnSave = new QPushButton(tr("💾 Save to Build Config"), this);
    m_btnSave->setCursor(Qt::PointingHandCursor);
    m_btnSave->setStyleSheet("QPushButton { background-color: rgba(2, 132, 199, 0.12); color: #0284c7; font-weight: bold; padding: 6px 16px; border-radius: 4px; border: 1px solid rgba(2, 132, 199, 0.35); font-size: 11px; } QPushButton:hover { background-color: #0284c7; color: white; }");
    connect(m_btnSave, &QPushButton::clicked, this, &CompilerWizardDialog::onSaveClicked);
    rowActions->addWidget(m_btnSave);

    m_btnSaveRecompile = new QPushButton(tr("🚀 Save && Recompile Now"), this);
    m_btnSaveRecompile->setCursor(Qt::PointingHandCursor);
    m_btnSaveRecompile->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; border: 1px solid #0284c7; font-size: 11px; } QPushButton:hover { background-color: #0369a1; }");
    connect(m_btnSaveRecompile, &QPushButton::clicked, this, &CompilerWizardDialog::onSaveAndRecompileClicked);
    rowActions->addWidget(m_btnSaveRecompile);

    mainLayout->addLayout(rowActions);
}

QString CompilerWizardDialog::generatedFlags() const
{
    QStringList active;
    for (int i = 0; i < m_checkboxes.size(); ++i) {
        if (m_checkboxes[i]->isChecked()) {
            active.append(m_flags[i].flag);
        }
    }
    return active.join(QLatin1Char(' '));
}

void CompilerWizardDialog::updatePreview()
{
    QString flags = generatedFlags();
    if (flags.isEmpty()) {
        m_txtPreview->setText(QStringLiteral("# No custom compiler flags selected (default build options apply)"));
    } else {
        m_txtPreview->setText(QString("-DCMAKE_CXX_FLAGS=\"%1\"").arg(flags));
    }
}

void CompilerWizardDialog::onFlagToggled(int flagIndex, bool checked)
{
    if (m_isUpdatingFlags) return;

    if (checked && flagIndex >= 0 && flagIndex < m_flags.size()) {
        const QString &group = m_flags[flagIndex].mutualGroup;
        if (!group.isEmpty()) {
            m_isUpdatingFlags = true;
            for (int i = 0; i < m_flags.size(); ++i) {
                if (i != flagIndex && m_flags[i].mutualGroup == group) {
                    m_checkboxes[i]->setChecked(false);
                }
            }
            m_isUpdatingFlags = false;
        }
    }

    updatePreview();
}

void CompilerWizardDialog::applyPreset(int index)
{
    // 0: Standard Baseline (-O2)
    // 1: Performance Recommended (Green only)
    // 2: Extreme Enthusiast (Green + Yellow)
    // 3: Custom / Clear All

    m_isUpdatingFlags = true;
    for (int i = 0; i < m_flags.size(); ++i) {
        const CompilerFlagDef &f = m_flags[i];
        bool shouldCheck = false;

        if (index == 0) {
            // Standard safe baseline: only -O2
            shouldCheck = (f.flag == QLatin1String("-O2"));
        } else if (index == 1) {
            // Green only (excluding -O2 since -O3 is active)
            if (f.flag == QLatin1String("-O2")) {
                shouldCheck = false;
            } else {
                shouldCheck = (f.safety == FlagSafetyLevel::Recommended);
            }
        } else if (index == 2) {
            // Green + Yellow
            if (f.flag == QLatin1String("-O2")) {
                shouldCheck = false;
            } else if (f.flag == QLatin1String("-march=x86-64-v3") || f.flag == QLatin1String("-march=x86-64-v4")) {
                shouldCheck = false;
            } else {
                shouldCheck = (f.safety == FlagSafetyLevel::Recommended ||
                               f.safety == FlagSafetyLevel::Conditional);
            }
        } else if (index == 3) {
            // Clear all
            shouldCheck = false;
        }

        m_checkboxes[i]->setChecked(shouldCheck);
    }
    m_isUpdatingFlags = false;
    updatePreview();
}

void CompilerWizardDialog::showFlagInfo(int flagIndex)
{
    if (flagIndex < 0 || flagIndex >= m_flags.size()) return;

    const CompilerFlagDef &f = m_flags[flagIndex];

    QString safetyBadge;
    if (f.safety == FlagSafetyLevel::Recommended) {
        safetyBadge = tr("<span style='color: #4ade80; font-weight: bold;'>🟢 Recommended for BubezView</span>");
    } else if (f.safety == FlagSafetyLevel::Conditional) {
        safetyBadge = tr("<span style='color: #facc15; font-weight: bold;'>🟡 Optional / Situational</span>");
    } else {
        safetyBadge = tr("<span style='color: #f87171; font-weight: bold;'>🔴 Hazardous / Potential Instability</span>");
    }

    QString content = QString(
        "<h3>%1 (<code>%2</code>)</h3>"
        "<p><b>%3:</b> %4<br>"
        "<b>%5:</b> %6</p>"
        "<hr>"
        "<p style='font-size: 13px; line-height: 1.5;'>%7</p>"
    ).arg(f.title, f.flag, tr("Category"), f.category, tr("Status"), safetyBadge, f.description);

    QMessageBox::information(this, tr("Compiler Flag Details: %1").arg(f.flag), content);
}

void CompilerWizardDialog::copyToClipboard()
{
    QString text = m_txtPreview->toPlainText();
    QGuiApplication::clipboard()->setText(text);
    m_btnCopy->setText(tr("✅ Copied!"));
    QTimer::singleShot(1500, this, [this]() {
        if (m_btnCopy) m_btnCopy->setText(tr("📋 Copy"));
    });
}

void CompilerWizardDialog::onSaveClicked()
{
    QString flags = generatedFlags();
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.setCustomCompilerFlags(flags);
    bcfg.save();

    emit configurationSaved();
    accept();
}

void CompilerWizardDialog::onSaveAndRecompileClicked()
{
    QString flags = generatedFlags();
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.setCustomCompilerFlags(flags);
    bcfg.save();

    emit configurationSaved();
    emit recompileRequested();
    accept();
}
