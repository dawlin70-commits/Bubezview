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
#include <QGroupBox>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>

CompilerWizardDialog::CompilerWizardDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("🧙 Compiler Wizard — Advanced CPU & Instruction Tuning"));
    resize(840, 700);
    setMinimumSize(740, 580);

    initDatabase();
    setupUi();

    // Check if there are already saved flags in BuildConfig
    QString savedFlags = BuildConfig::instance().customCompilerFlags().trimmed();
    m_isUpdatingFlags = true;
    if (!savedFlags.isEmpty()) {
        QStringList flagTokens = savedFlags.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < m_flags.size(); ++i) {
            bool found = flagTokens.contains(m_flags[i].flag);
            m_checkboxes[i]->setChecked(found);
        }
    } else {
        // Reflect active build profile by default
        QString cpuProfile = BuildConfig::instance().cpuProfile();
        for (int i = 0; i < m_flags.size(); ++i) {
            bool active = false;
            if (cpuProfile == QLatin1String("native")) {
                if (m_flags[i].flag == QLatin1String("-march=native") ||
                    m_flags[i].flag == QLatin1String("-pipe") ||
                    m_flags[i].flag == QLatin1String("-flto=auto") ||
                    m_flags[i].flag == QLatin1String("-fdevirtualize-at-ltrans") ||
                    m_flags[i].flag == QLatin1String("-fno-semantic-interposition") ||
                    m_flags[i].flag == QLatin1String("-fomit-frame-pointer")) {
                    active = true;
                }
            } else {
                if (m_flags[i].flag == QLatin1String("-march=x86-64-v3") ||
                    m_flags[i].flag == QLatin1String("-pipe") ||
                    m_flags[i].flag == QLatin1String("-fno-semantic-interposition")) {
                    active = true;
                }
            }
            m_checkboxes[i]->setChecked(active);
        }
    }
    m_isUpdatingFlags = false;

    if (m_comboPreset) {
        m_comboPreset->setCurrentIndex(0); // "Active Configuration in Use"
    }
    updatePreview();
}

CompilerWizardDialog::~CompilerWizardDialog() = default;

void CompilerWizardDialog::initDatabase()
{
    m_flags.clear();

    // =========================================================================
    // 1. PROCESSOR & MICROARCHITECTURE
    // =========================================================================
    m_flags.append({
        QStringLiteral("-march=native"),
        tr("Tailored CPU Performance Tuning"),
        tr("1. Processor & Microarchitecture"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Highly Recommended.</b> Emits instructions tailored specifically to your CPU model, maximizing execution speed across the user interface and file processing."),
        QStringLiteral("all"),
        true,
        QStringLiteral("arch")
    });

    // =========================================================================
    // 2. PROGRAM CORE & INTERFACE (BubezView Only — No Plugins)
    // =========================================================================
    m_flags.append({
        QStringLiteral("-flto=auto"),
        tr("Whole-Program Optimization (Lower RAM)"),
        tr("2. Program Core & Interface (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Performs whole-program optimization across all units at link time. Eliminates dead code, reduces RAM footprint, and ensures fast cold launch."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fdevirtualize-at-ltrans"),
        tr("Instant Switching between Viewers"),
        tr("2. Program Core & Interface (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Eliminates intermediate dispatch overhead when switching tabs between Images, PDF, Text, and Archives."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fno-semantic-interposition"),
        tr("Instant Responsiveness for Menus & Buttons"),
        tr("2. Program Core & Interface (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Removes delays when clicking toolbars, menus, and shortcuts by linking internal functions directly."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fomit-frame-pointer"),
        tr("Extra CPU Registers for UI & Rendering"),
        tr("2. Program Core & Interface (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Frees up an extra CPU register normally at rest and dedicates it to accelerating interface animation and window drawing."),
        QStringLiteral("all"),
        true,
        QString()
    });

    m_flags.append({
        QStringLiteral("-pipe"),
        tr("In-RAM Fast Compilation (Less SSD Wear)"),
        tr("2. Program Core & Interface (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended.</b> Uses system RAM pipes during compilation instead of temporary disk files, speeding up builds and protecting SSD lifespan."),
        QStringLiteral("all"),
        true,
        QString()
    });

    // =========================================================================
    // 3. IMAGE VIEWER & GRAPHICS CANVAS (BubezView Only — No Plugins)
    // =========================================================================
    m_flags.append({
        QStringLiteral("-ftree-vectorize"),
        tr("Parallel Pixel Processing (SIMD)"),
        tr("3. Image Viewer & Graphics Canvas (BubezView Only)"),
        FlagSafetyLevel::Recommended,
        tr("🟢 <b>Recommended for Images.</b> Processes multiple pixels simultaneously in a single clock cycle, dramatically speeding up zooming and rendering."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-funroll-loops"),
        tr("Fast Photo Filters & Blur"),
        tr("3. Image Viewer & Graphics Canvas (BubezView Only)"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Fast Image Filters.</b> Unrolls repetitive pixel loops, accelerating image convolution filters, blur, sharpening, and color adjustments."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fvect-cost-model=dynamic"),
        tr("Adaptive Resolution Scaling (Icons to 8K)"),
        tr("3. Image Viewer & Graphics Canvas (BubezView Only)"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Adaptive Processing.</b> Evaluates vectorization dynamically based on real image dimensions, ensuring peak performance on both small icons and huge 8K photos."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fno-math-errno"),
        tr("Direct Hardware Math (Zoom & Rotation)"),
        tr("3. Image Viewer & Graphics Canvas (BubezView Only)"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Hardware Math Acceleration.</b> Skips setting legacy errno variables to compute smooth zoom, proportions, and rotations directly on the CPU."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-falign-loops=32"),
        tr("Smooth Pixel Traversal in CPU Cache"),
        tr("3. Image Viewer & Graphics Canvas (BubezView Only)"),
        FlagSafetyLevel::Conditional,
        tr("🟡 <b>Cache Optimization.</b> Aligns loop entry points to 32-byte CPU cache boundaries, eliminating stutter during horizontal pixel traversal."),
        QStringLiteral("all"),
        false,
        QString()
    });

    // =========================================================================
    // 4. ENGINE PLUGINS & HEAVY FORMATS (Requires Plugin Recompilation)
    // =========================================================================
    m_flags.append({
        QStringLiteral("-mcrc32"),
        tr("Fast Archive Decompression (ZIP, 7Z, TAR)"),
        tr("4. Engine Plugins & Heavy Formats (Requires Plugin Build)"),
        FlagSafetyLevel::Recommended,
        tr("🧩 <b>Requires Plugin Build.</b> Emits hardware CRC32 CPU instructions to dramatically speed up archive extraction and integrity verification in LibArchive."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-ffp-contract=fast"),
        tr("Smooth PDF Page & Vector Curve Rendering"),
        tr("4. Engine Plugins & Heavy Formats (Requires Plugin Build)"),
        FlagSafetyLevel::Recommended,
        tr("🧩 <b>Requires Plugin Build.</b> Fuses floating-point multiplication and addition via hardware FMA, accelerating complex PDF pages and diagrams in Poppler."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-ftree-slp-vectorize"),
        tr("Fast PDF Text & Annotation Rasterization"),
        tr("4. Engine Plugins & Heavy Formats (Requires Plugin Build)"),
        FlagSafetyLevel::Recommended,
        tr("🧩 <b>Requires Plugin Build.</b> Accelerates glyph rasterization, highlighter blending, and markup rendering in high-resolution PDF pages."),
        QStringLiteral("all"),
        false,
        QString()
    });

    m_flags.append({
        QStringLiteral("-fno-strict-aliasing"),
        tr("Direct Byte Stream for Decompression Buffers"),
        tr("4. Engine Plugins & Heavy Formats (Requires Plugin Build)"),
        FlagSafetyLevel::Recommended,
        tr("🧩 <b>Requires Plugin Build.</b> Enables extraction and media demuxing engines to read packed byte streams at maximum bus bandwidth without conversion overhead."),
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

    // 1. Host Hardware Info Card
    QFrame *cardCpu = new QFrame(this);
    cardCpu->setStyleSheet("QFrame { background-color: rgba(100, 116, 139, 0.08); border-radius: 8px; border: 1px solid rgba(100, 116, 139, 0.2); }");
    QVBoxLayout *layCard = new QVBoxLayout(cardCpu);
    layCard->setContentsMargins(14, 12, 14, 12);
    layCard->setSpacing(6);

    QHBoxLayout *rowTitle = new QHBoxLayout();
    QLabel *lblCardTitle = new QLabel(tr("🖥️ <b>Host CPU Architecture & Active Build State</b>"), cardCpu);
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
    m_lblCpuCores = new QLabel(QString(tr("%1 threads")).arg(CpuInfo::logicalCores()), cardCpu);
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

    // 2. Presets Toolbar
    QHBoxLayout *rowToolbar = new QHBoxLayout();
    QLabel *lblPreset = new QLabel(tr("<b>Quick Preset:</b>"), this);
    lblPreset->setStyleSheet("font-size: 12px; color: #334155;");
    rowToolbar->addWidget(lblPreset);

    m_comboPreset = new QComboBox(this);
    m_comboPreset->setCursor(Qt::PointingHandCursor);
    m_comboPreset->addItem(tr("🔄 Active Configuration in Use"), 0);
    m_comboPreset->addItem(tr("🟢 Recommended for Host CPU (Zero Risk)"), 1);
    m_comboPreset->addItem(tr("🎨 Optimized for Images & Graphics"), 2);
    m_comboPreset->addItem(tr("⚡ Maximum Performance (UI + Images + Plugins)"), 3);
    m_comboPreset->addItem(tr("⚙️ Custom Configuration"), 4);
    m_comboPreset->addItem(tr("🧹 Clear All"), 5);

    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompilerWizardDialog::applyPreset);
    rowToolbar->addWidget(m_comboPreset, 1);

    mainLayout->addLayout(rowToolbar);

    // 3. Scrollable List of Flag Checkboxes grouped by category
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(12);
    scrollLayout->setContentsMargins(4, 4, 4, 4);

    QString currentCategory;
    QVBoxLayout *catLayout = nullptr;
    QGroupBox *catGroup = nullptr;

    m_checkboxes.clear();

    for (int i = 0; i < m_flags.size(); ++i) {
        const CompilerFlagDef &def = m_flags[i];

        if (def.category != currentCategory) {
            currentCategory = def.category;
            catGroup = new QGroupBox(currentCategory, scrollContent);
            catLayout = new QVBoxLayout(catGroup);
            catLayout->setSpacing(6);
            catLayout->setContentsMargins(12, 10, 12, 10);
            scrollLayout->addWidget(catGroup);
        }

        QHBoxLayout *rowFlag = new QHBoxLayout();
        QCheckBox *chk = new QCheckBox(QString("%1 (%2)").arg(def.title, def.flag), catGroup);
        chk->setCursor(Qt::PointingHandCursor);

        // Hardware microarchitecture capability check
        bool hardwareSupported = true;
        QString hostTier = CpuInfo::microarchitectureTier(); // "v1", "v2", "v3", "v4"
        if (def.requiredTier == QLatin1String("v4") && hostTier != QLatin1String("v4")) {
            hardwareSupported = false;
        } else if (def.requiredTier == QLatin1String("v3") && hostTier != QLatin1String("v3") && hostTier != QLatin1String("v4")) {
            hardwareSupported = false;
        }

        if (!hardwareSupported) {
            chk->setEnabled(false);
            chk->setChecked(false);
            chk->setToolTip(tr("⚠️ Unsupported by your processor (%1). Enabling this tier would cause crashes (SIGILL / Illegal Instruction).")
                               .arg(CpuInfo::modelName()));
            chk->setText(QString("%1 (%2) — 🚫 %3").arg(def.title, def.flag, tr("Unsupported by CPU")));
            chk->setStyleSheet("QCheckBox { font-size: 12px; color: #94a3b8; font-style: italic; }");
        } else {
            chk->setStyleSheet("QCheckBox { font-size: 12px; font-weight: 500; }");
        }

        connect(chk, &QCheckBox::toggled, this, [this, i](bool checked) {
            onFlagToggled(i, checked);
        });
        m_checkboxes.append(chk);

        QPushButton *btnInfo = new QPushButton("i", catGroup);
        btnInfo->setFixedSize(22, 22);
        btnInfo->setCursor(Qt::PointingHandCursor);
        btnInfo->setToolTip(tr("Detailed information about %1").arg(def.flag));
        btnInfo->setStyleSheet("QPushButton { font-weight: bold; border-radius: 11px; background-color: rgba(100, 116, 139, 0.15); color: #475569; } QPushButton:hover { background-color: #0284c7; color: white; }");
        connect(btnInfo, &QPushButton::clicked, this, [this, i]() {
            showFlagInfo(i);
        });

        rowFlag->addWidget(chk, 1);
        rowFlag->addWidget(btnInfo);
        catLayout->addLayout(rowFlag);
    }

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // 4. Preview Text Box
    QLabel *lblPreview = new QLabel(tr("<b>Generated Compiler Flags:</b>"), this);
    lblPreview->setStyleSheet("font-size: 11px; color: #475569;");
    mainLayout->addWidget(lblPreview);

    m_txtPreview = new QTextEdit(this);
    m_txtPreview->setReadOnly(true);
    m_txtPreview->setFixedHeight(50);
    m_txtPreview->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0f172a;"
        "  color: #38bdf8;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #334155;"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "}"
    );
    mainLayout->addWidget(m_txtPreview);

    m_lblScopeHint = new QLabel(this);
    m_lblScopeHint->setWordWrap(true);
    mainLayout->addWidget(m_lblScopeHint);

    // 5. Bottom Action Buttons
    QHBoxLayout *rowActions = new QHBoxLayout();

    m_btnCopy = new QPushButton(tr("📋 Copy Flags"), this);
    m_btnCopy->setCursor(Qt::PointingHandCursor);
    m_btnCopy->setStyleSheet("QPushButton { padding: 6px 14px; border-radius: 4px; font-size: 11px; }");
    connect(m_btnCopy, &QPushButton::clicked, this, &CompilerWizardDialog::copyToClipboard);
    rowActions->addWidget(m_btnCopy);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet("QPushButton { padding: 6px 14px; border-radius: 4px; font-size: 11px; }");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    rowActions->addWidget(m_btnCancel);

    rowActions->addStretch();

    m_btnSave = new QPushButton(tr("💾 Save Configuration"), this);
    m_btnSave->setCursor(Qt::PointingHandCursor);
    m_btnSave->setStyleSheet("QPushButton { background-color: rgba(2, 132, 199, 0.12); color: #0284c7; font-weight: bold; padding: 6px 16px; border-radius: 4px; border: 1px solid rgba(2, 132, 199, 0.35); font-size: 11px; } QPushButton:hover { background-color: #0284c7; color: white; }");
    connect(m_btnSave, &QPushButton::clicked, this, &CompilerWizardDialog::onSaveClicked);
    rowActions->addWidget(m_btnSave);

    m_btnSaveRecompile = new QPushButton(tr("⚙️ Save && Recompile Now"), this);
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
        m_txtPreview->setText(QStringLiteral("# Standard build baseline (no additional custom compiler flags)"));
    } else {
        m_txtPreview->setText(QString("-DCUSTOM_CXX_FLAGS=\"%1\"").arg(flags));
    }

    if (m_lblScopeHint) {
        if (requiresPluginRecompilation()) {
            m_lblScopeHint->setText(tr("🧩 <b>Plugin Recompilation Required:</b> This configuration extends deep optimizations to the underlying media and document engines. Recompiling will prompt you to update plugins (<code>bubez_pdf.so</code>, <code>bubez_archive.so</code>) or manage them manually."));
            m_lblScopeHint->setStyleSheet("QLabel { background-color: rgba(234, 179, 8, 0.12); border: 1px solid rgba(234, 179, 8, 0.35); border-radius: 5px; padding: 6px 10px; color: #854d0e; font-size: 11px; }");
        } else {
            m_lblScopeHint->setText(tr("⚡ <b>Fast UI & Image Build:</b> Only BubezView main executable will be recompiled (~2s). Modular plugins will not be touched."));
            m_lblScopeHint->setStyleSheet("QLabel { background-color: rgba(34, 197, 94, 0.10); border: 1px solid rgba(34, 197, 94, 0.3); border-radius: 5px; padding: 6px 10px; color: #166534; font-size: 11px; }");
        }
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

    // Like an audio equalizer / mixer: switch preset selector to "Custom" (index 4) immediately on manual edit
    if (m_comboPreset && m_comboPreset->currentIndex() != 4) {
        bool prevBlock = m_comboPreset->blockSignals(true);
        m_comboPreset->setCurrentIndex(4);
        m_comboPreset->blockSignals(prevBlock);
    }

    updatePreview();
}

void CompilerWizardDialog::applyPreset(int index)
{
    m_isUpdatingFlags = true;

    if (index == 0) {
        // Active configuration in use
        QString savedFlags = BuildConfig::instance().customCompilerFlags().trimmed();
        if (!savedFlags.isEmpty()) {
            QStringList tokens = savedFlags.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (int i = 0; i < m_flags.size(); ++i) {
                m_checkboxes[i]->setChecked(tokens.contains(m_flags[i].flag));
            }
        } else {
            QString cpuProfile = BuildConfig::instance().cpuProfile();
            for (int i = 0; i < m_flags.size(); ++i) {
                bool active = false;
                if (cpuProfile == QLatin1String("native")) {
                    if (m_flags[i].flag == QLatin1String("-march=native") ||
                        m_flags[i].flag == QLatin1String("-pipe") ||
                        m_flags[i].flag == QLatin1String("-flto=auto") ||
                        m_flags[i].flag == QLatin1String("-fdevirtualize-at-ltrans") ||
                        m_flags[i].flag == QLatin1String("-fno-semantic-interposition") ||
                        m_flags[i].flag == QLatin1String("-fomit-frame-pointer")) {
                        active = true;
                    }
                } else {
                    if (m_flags[i].flag == QLatin1String("-march=x86-64-v3") ||
                        m_flags[i].flag == QLatin1String("-pipe") ||
                        m_flags[i].flag == QLatin1String("-fno-semantic-interposition")) {
                        active = true;
                    }
                }
                m_checkboxes[i]->setChecked(m_checkboxes[i]->isEnabled() && active);
            }
        }
    } else if (index == 1) {
        // Recommended for Host CPU: native, pipe, lto, devirtualize, bypass plt, omit frame pointer
        for (int i = 0; i < m_flags.size(); ++i) {
            if (!m_checkboxes[i]->isEnabled()) {
                m_checkboxes[i]->setChecked(false);
                continue;
            }
            const QString &f = m_flags[i].flag;
            bool active = (f == QLatin1String("-march=native") ||
                           f == QLatin1String("-pipe") ||
                           f == QLatin1String("-flto=auto") ||
                           f == QLatin1String("-fdevirtualize-at-ltrans") ||
                           f == QLatin1String("-fno-semantic-interposition") ||
                           f == QLatin1String("-fomit-frame-pointer"));
            m_checkboxes[i]->setChecked(active);
        }
    } else if (index == 2) {
        // Optimized for Images & Graphics
        for (int i = 0; i < m_flags.size(); ++i) {
            if (!m_checkboxes[i]->isEnabled()) {
                m_checkboxes[i]->setChecked(false);
                continue;
            }
            const QString &f = m_flags[i].flag;
            bool active = (f == QLatin1String("-march=native") ||
                           f == QLatin1String("-pipe") ||
                           f == QLatin1String("-ftree-vectorize") ||
                           f == QLatin1String("-funroll-loops") ||
                           f == QLatin1String("-fvect-cost-model=dynamic") ||
                           f == QLatin1String("-fno-math-errno") ||
                           f == QLatin1String("-falign-loops=32"));
            m_checkboxes[i]->setChecked(active);
        }
    } else if (index == 3) {
        // Maximum Performance (UI + Images + Plugins)
        for (int i = 0; i < m_flags.size(); ++i) {
            if (!m_checkboxes[i]->isEnabled()) {
                m_checkboxes[i]->setChecked(false);
                continue;
            }
            const QString &f = m_flags[i].flag;
            bool active = (f != QLatin1String("-march=x86-64-v3") &&
                           f != QLatin1String("-march=x86-64-v4"));
            m_checkboxes[i]->setChecked(active);
        }
    } else if (index == 4) {
        // Custom Configuration: preserves current manual selection
    } else if (index == 5) {
        // Clear all
        for (int i = 0; i < m_flags.size(); ++i) {
            m_checkboxes[i]->setChecked(false);
        }
        // Switch combo selection to Custom (index 4) without retriggering preset
        if (m_comboPreset) {
            bool prevBlock = m_comboPreset->blockSignals(true);
            m_comboPreset->setCurrentIndex(4);
            m_comboPreset->blockSignals(prevBlock);
        }
    }

    m_isUpdatingFlags = false;
    updatePreview();
}

bool CompilerWizardDialog::requiresPluginRecompilation() const
{
    // Preset 1 (CPU Host) and Preset 2 (Images & Graphics) are specifically scoped to BubezView UI and Canvas.
    int presetIdx = m_comboPreset ? m_comboPreset->currentIndex() : -1;
    if (presetIdx == 1 || presetIdx == 2) {
        return false;
    }
    if (presetIdx == 3) {
        return true; // Preset 3: Maximum Performance (UI + Images + Plugins)
    }

    // Inspect active flags: if any Section 4 flag (Engine Plugins) is selected, plugin rebuild is required
    for (int i = 0; i < m_flags.size(); ++i) {
        if (m_checkboxes[i]->isChecked()) {
            const QString &cat = m_flags[i].category;
            if (cat.startsWith(QLatin1String("4.")) || cat.contains(QLatin1String("Plugins"), Qt::CaseInsensitive)) {
                return true;
            }
        }
    }

    return false;
}

void CompilerWizardDialog::showFlagInfo(int flagIndex)
{
    if (flagIndex < 0 || flagIndex >= m_flags.size()) return;

    const CompilerFlagDef &f = m_flags[flagIndex];
    QString safetyBadge = (f.safety == FlagSafetyLevel::Recommended)
        ? tr("<span style='color: #4ade80; font-weight: bold;'>🟢 Recommended for BubezView</span>")
        : tr("<span style='color: #facc15; font-weight: bold;'>🟡 Situational / Specialized</span>");

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
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(text);
        QMessageBox::information(this, tr("Copied"), tr("Compiler flags copied to clipboard."));
    }
}

void CompilerWizardDialog::onSaveClicked()
{
    QString flags = generatedFlags();
    BuildConfig::instance().setCustomCompilerFlags(flags);
    BuildConfig::instance().save();
    emit configurationSaved();
    accept();
}

void CompilerWizardDialog::onSaveAndRecompileClicked()
{
    QString flags = generatedFlags();
    BuildConfig::instance().setCustomCompilerFlags(flags);
    BuildConfig::instance().save();
    emit configurationSaved();

    if (requiresPluginRecompilation()) {
        QMessageBox msg(this);
        msg.setWindowTitle(tr("Advanced Plugin Optimization"));
        msg.setText(tr("<h3>Advanced Optimization Options Selected</h3>"
                       "<p>These advanced computation options will deliver their full performance potential once modular engine plugins are also recompiled.</p>"
                       "<p><b>Components affected by recompilation:</b><br>"
                       "• Main application (<code>bubezview</code>)<br>"
                       "• PDF viewer plugin (<code>bubez_pdf.so</code>)<br>"
                       "• Archive manager plugin (<code>bubez_archive.so</code>)</p>"
                       "<p>How would you like to proceed?</p>"));

        QPushButton *btnAll = msg.addButton(tr("⚙️ Recompile All (BubezView + Plugins)"), QMessageBox::AcceptRole);
        QPushButton *btnManual = msg.addButton(tr("🧩 Manual Management..."), QMessageBox::ActionRole);
        msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msg.setDefaultButton(btnAll);

        msg.exec();

        if (msg.clickedButton() == btnAll) {
            emit recompileRequested(QString()); // Full build including plugins
            accept();
        } else if (msg.clickedButton() == btnManual) {
            emit openExtensionsTabRequested();
            accept();
        } else {
            // Cancelled
            return;
        }
    } else {
        // Fast recompilation of main binary only
        emit recompileRequested(QStringLiteral("bubezview"));
        accept();
    }
}
