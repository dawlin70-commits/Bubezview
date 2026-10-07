/**
 * @file accordion_extension_card.cpp
 * @brief Implementation of the collapsible extension card with feature breakdown, diagnostics, and actions.
 */

#include "accordion_extension_card.h"
#include "core/package_manager_helper.h"
#include "core/build_config.h"
#include <QProcess>
#include <QMessageBox>

AccordionExtensionCard::AccordionExtensionCard(const ExtensionModuleInfo &mod, QWidget *parent)
    : QFrame(parent)
    , m_mod(mod)
    , m_expanded(false)
{
    setObjectName("AccordionCard");
    setStyleSheet(
        "#AccordionCard {"
        "    background-color: rgba(255, 255, 255, 0.75);"
        "    border: 1px solid rgba(100, 116, 139, 0.22);"
        "    border-radius: 8px;"
        "    margin-bottom: 4px;"
        "}"
    );

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(10, 8, 10, 8);
    mainLay->setSpacing(6);

    // ==============================================================
    // 1. HEADER (Always visible)
    // ==============================================================
    QFrame *headerFrame = new QFrame(this);
    headerFrame->setStyleSheet("QFrame { background: transparent; border: none; }");
    headerFrame->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *headerLay = new QHBoxLayout(headerFrame);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(8);

    m_btnToggle = new QPushButton("▶", headerFrame);
    m_btnToggle->setFixedSize(22, 22);
    m_btnToggle->setFlat(true);
    m_btnToggle->setCursor(Qt::PointingHandCursor);
    m_btnToggle->setStyleSheet("QPushButton { font-size: 11px; color: #475569; font-weight: bold; border: none; }");
    headerLay->addWidget(m_btnToggle);

    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);

    m_lblTitle = new QLabel(QString("<b>%1</b> &nbsp;<span style='color: #64748b; font-size: 11px;'>(%2)</span>")
                            .arg(m_mod.displayName, m_mod.systemPackage), headerFrame);
    m_lblSummary = new QLabel(m_mod.summary, headerFrame);
    m_lblSummary->setStyleSheet("color: #64748b; font-size: 11px;");

    titleCol->addWidget(m_lblTitle);
    titleCol->addWidget(m_lblSummary);
    headerLay->addLayout(titleCol, 1);

    m_lblStatusBadge = new QLabel(headerFrame);
    updateHeaderStatusBadge();
    headerLay->addWidget(m_lblStatusBadge);

    mainLay->addWidget(headerFrame);

    // Clicking anywhere on the header toggles the expander
    connect(m_btnToggle, &QPushButton::clicked, this, &AccordionExtensionCard::toggleExpanded);

    // ==============================================================
    // 2. EXPANDABLE DETAILS CONTAINER
    // ==============================================================
    m_detailsContainer = new QFrame(this);
    m_detailsContainer->setStyleSheet(
        "QFrame {"
        "    background-color: rgba(15, 23, 42, 0.03);"
        "    border: 1px solid rgba(100, 116, 139, 0.14);"
        "    border-radius: 6px;"
        "}"
    );

    QVBoxLayout *contLay = new QVBoxLayout(m_detailsContainer);
    contLay->setContentsMargins(10, 8, 10, 8);
    contLay->setSpacing(8);

    // Sub-dependencies container (Proposal 4)
    m_subDepsLayout = new QVBoxLayout();
    m_subDepsLayout->setSpacing(4);
    contLay->addLayout(m_subDepsLayout);

    // Section title
    QLabel *lblFeatHeader = new QLabel(tr("⚡ <b>Enabled Features in Codebase:</b>"), m_detailsContainer);
    lblFeatHeader->setStyleSheet("color: #0284c7; font-size: 11px;");
    contLay->addWidget(lblFeatHeader);

    // Feature list container
    m_featuresLayout = new QVBoxLayout();
    m_featuresLayout->setSpacing(4);
    contLay->addLayout(m_featuresLayout);

    // Separator line
    QFrame *line = new QFrame(m_detailsContainer);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: rgba(100, 116, 139, 0.15);");
    contLay->addWidget(line);

    // ==============================================================
    // 3. FOOTER: Source compilation, live diagnostics, action button
    // ==============================================================
    QHBoxLayout *footerLay = new QHBoxLayout();
    footerLay->setSpacing(8);

    // Compilation options
    m_lblModeTitle = new QLabel(tr("Binary Source:"), m_detailsContainer);
    m_lblModeTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #475569;");
    footerLay->addWidget(m_lblModeTitle);

    m_btnSourceDynamic = new QPushButton(tr("System Package"), m_detailsContainer);
    m_btnSourceDynamic->setCursor(Qt::PointingHandCursor);
    m_btnSourceDynamic->setCheckable(true);

    m_btnSourceStatic = new QPushButton(tr("Compile from Source"), m_detailsContainer);
    m_btnSourceStatic->setCursor(Qt::PointingHandCursor);
    m_btnSourceStatic->setCheckable(true);

    m_btnSourceDynamic->setChecked(!m_mod.isStaticCompiled);
    m_btnSourceStatic->setChecked(m_mod.isStaticCompiled);
    updateCompileToggleStyles();

    m_lblModeTitle->setVisible(m_mod.canCompile);
    m_btnSourceDynamic->setVisible(m_mod.canCompile);
    m_btnSourceStatic->setVisible(m_mod.canCompile);

    connect(m_btnSourceDynamic, &QPushButton::clicked, this, [this]() {
        m_mod.isStaticCompiled = false;
        m_btnSourceDynamic->setChecked(true);
        m_btnSourceStatic->setChecked(false);
        updateCompileToggleStyles();
        updateActionButton();
        emit statusChanged();
    });

    connect(m_btnSourceStatic, &QPushButton::clicked, this, [this]() {
        m_mod.isStaticCompiled = true;
        m_btnSourceStatic->setChecked(true);
        m_btnSourceDynamic->setChecked(false);
        updateCompileToggleStyles();
        updateActionButton();
        emit statusChanged();
    });

    footerLay->addWidget(m_btnSourceDynamic);
    footerLay->addWidget(m_btnSourceStatic);
    footerLay->addStretch(1);

    // Live Diagnostic Check (Proposal 5)
    m_lblDiagResult = new QLabel(m_detailsContainer);
    m_lblDiagResult->setStyleSheet("font-size: 10px; font-family: monospace;");
    m_lblDiagResult->hide();
    footerLay->addWidget(m_lblDiagResult);

    m_btnDiagnose = new QPushButton(tr("🧪 Verify"), m_detailsContainer);
    m_btnDiagnose->setCursor(Qt::PointingHandCursor);
    m_btnDiagnose->setToolTip(tr("Executes diagnostic command '%1' to verify binary integrity and version.")
                              .arg(m_mod.diagnosticCommand));
    m_btnDiagnose->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 10px; border-radius: 4px; background-color: #f1f5f9; color: #334155; font-weight: bold; border: 1px solid #cbd5e1; } QPushButton:hover { background-color: #e2e8f0; }");
    connect(m_btnDiagnose, &QPushButton::clicked, this, &AccordionExtensionCard::runDiagnosticTest);
    footerLay->addWidget(m_btnDiagnose);

    // Action button at end of list
    m_btnAction = new QPushButton(m_detailsContainer);
    m_btnAction->setCursor(Qt::PointingHandCursor);
    updateActionButton();
    connect(m_btnAction, &QPushButton::clicked, this, &AccordionExtensionCard::handleInstallOrUninstall);
    footerLay->addWidget(m_btnAction);

    contLay->addLayout(footerLay);

    m_detailsContainer->setVisible(false);
    mainLay->addWidget(m_detailsContainer);

    refreshState();
}

void AccordionExtensionCard::setExpanded(bool expand)
{
    m_expanded = expand;
    m_detailsContainer->setVisible(m_expanded);
    m_btnToggle->setText(m_expanded ? "▼" : "▶");
}

void AccordionExtensionCard::toggleExpanded()
{
    setExpanded(!m_expanded);
}

void AccordionExtensionCard::refreshState()
{
    ExtensionFeatureRegistry::refreshModuleStatus(m_mod);
    updateHeaderStatusBadge();
    updateActionButton();

    // 1. Populate sub-dependencies (Proposal 4)
    while (m_subDepsLayout->count() > 0) {
        QLayoutItem *item = m_subDepsLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    for (const auto &subDep : m_mod.subDependencies) {
        if (!subDep.isInstalled) {
            QFrame *depBox = new QFrame(m_detailsContainer);
            depBox->setStyleSheet("QFrame { background-color: rgba(245, 158, 11, 0.12); border: 1px solid rgba(245, 158, 11, 0.35); border-radius: 5px; }");
            QHBoxLayout *depLay = new QHBoxLayout(depBox);
            depLay->setContentsMargins(8, 5, 8, 5);
            depLay->setSpacing(8);

            QLabel *lblWarn = new QLabel(QString("⚠️ <b>%1:</b> %2 (<code>%3</code>) — %4")
                                         .arg(tr("Optional Sub-Dependency Missing"))
                                         .arg(subDep.name, subDep.systemPackage, subDep.hint), depBox);
            lblWarn->setStyleSheet("font-size: 11px; color: #b45309;");
            lblWarn->setWordWrap(true);
            depLay->addWidget(lblWarn, 1);

            QPushButton *btnSubInstall = new QPushButton(tr("📥 Install %1").arg(subDep.name), depBox);
            btnSubInstall->setCursor(Qt::PointingHandCursor);
            btnSubInstall->setStyleSheet("QPushButton { font-size: 10px; padding: 3px 8px; border-radius: 3px; background-color: #d97706; color: white; font-weight: bold; } QPushButton:hover { background-color: #b45309; }");
            connect(btnSubInstall, &QPushButton::clicked, this, [this, subDep]() {
                if (PackageManagerHelper::installComponentInteractive(subDep.systemPackage, this)) {
                    refreshState();
                    emit statusChanged();
                }
            });
            depLay->addWidget(btnSubInstall);

            m_subDepsLayout->addWidget(depBox);
        }
    }

    // 2. Populate features
    while (m_featuresLayout->count() > 0) {
        QLayoutItem *item = m_featuresLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }
    m_featureRows.clear();

    for (const auto &feat : m_mod.features) {
        QFrame *fRow = new QFrame(m_detailsContainer);
        fRow->setStyleSheet("background: transparent; border: none;");
        QHBoxLayout *rLay = new QHBoxLayout(fRow);
        rLay->setContentsMargins(4, 2, 4, 2);
        rLay->setSpacing(8);

        QLabel *iconStatus = new QLabel(m_mod.isInstalled ? "🟢" : "⚪", fRow);
        iconStatus->setToolTip(m_mod.isInstalled ? tr("Feature active and available in code") : tr("Feature disabled (requires extension package)"));

        QVBoxLayout *txtLay = new QVBoxLayout();
        txtLay->setSpacing(1);

        QLabel *fTitle = new QLabel(QString("<b>%1</b> — %2").arg(feat.title, feat.description), fRow);
        fTitle->setWordWrap(true);
        fTitle->setStyleSheet(m_mod.isInstalled ? "font-size: 11px; color: #0f172a;" : "font-size: 11px; color: #64748b;");

        QLabel *fHook = new QLabel(QString("<span style='color: #64748b;'>Location:</span> <b>%1</b> &nbsp;•&nbsp; "
                                           "<span style='color: #64748b;'>C++ Hook:</span> <code style='color: #b91c1c; background-color: #f1f5f9; padding: 1px 3px; border-radius: 3px;'>%2</code>")
                                   .arg(feat.menuLocation, feat.codeHook), fRow);
        fHook->setStyleSheet("font-size: 10px;");

        txtLay->addWidget(fTitle);
        txtLay->addWidget(fHook);

        rLay->addWidget(iconStatus, 0, Qt::AlignTop);
        rLay->addLayout(txtLay, 1);

        m_featuresLayout->addWidget(fRow);
        m_featureRows.append(fRow);
    }
}

void AccordionExtensionCard::handleInstallOrUninstall()
{
    bool success = false;
    if (m_mod.isStaticCompiled) {
        // Trigger local engine/extension compilation
        QString targetId = m_mod.id;
        if (PackageManagerHelper::isCompilableEngine(m_mod.id)) {
            targetId = m_mod.id;
        } else if (m_mod.id == "pdfimages" || m_mod.category == "PDF") {
            targetId = "pdf";
        } else if (m_mod.id == "zstd" || m_mod.id == "xz" || m_mod.category == "Archive") {
            targetId = "archive";
        } else if (m_mod.category == "Office") {
            targetId = "office";
        }
        success = PackageManagerHelper::triggerEngineCompilation(targetId, this);
    } else {
        if (m_mod.isInstalled) {
            success = PackageManagerHelper::uninstallComponentInteractive(m_mod.id, this);
        } else {
            success = PackageManagerHelper::installComponentInteractive(m_mod.id, this);
        }
    }

    if (success) {
        refreshState();
        emit statusChanged();
    }
}

void AccordionExtensionCard::runDiagnosticTest()
{
    m_btnDiagnose->setEnabled(false);
    m_btnDiagnose->setText(tr("Verifying..."));
    m_lblDiagResult->hide();

    QProcess *proc = new QProcess(this);
    QString cmd = m_mod.diagnosticCommand;
    
    // Connect process completion
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        m_btnDiagnose->setEnabled(true);
        m_btnDiagnose->setText(tr("🧪 Verify"));

        if (status == QProcess::NormalExit && exitCode == 0) {
            QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            if (out.isEmpty()) {
                out = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            }
            QString firstLine = out.split('\n').value(0);
            if (firstLine.isEmpty()) firstLine = tr("Operational");
            m_lblDiagResult->setText(tr("✅ %1").arg(firstLine.left(45)));
            m_lblDiagResult->setStyleSheet("color: #16a34a; font-weight: bold; font-size: 10px;");
        } else {
            m_lblDiagResult->setText(tr("❌ Unavailable or execution failed"));
            m_lblDiagResult->setStyleSheet("color: #dc2626; font-size: 10px;");
        }
        m_lblDiagResult->show();
        proc->deleteLater();
    });

    QStringList args;
    QString program;
    if (cmd.startsWith("ls ")) {
        program = "ls";
        args = cmd.mid(3).split(' ', Qt::SkipEmptyParts);
    } else {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            program = parts.takeFirst();
            args = parts;
        }
    }

    if (program.isEmpty()) {
        m_btnDiagnose->setEnabled(true);
        m_btnDiagnose->setText(tr("🧪 Verify"));
        return;
    }

    proc->start(program, args);
}

void AccordionExtensionCard::updateHeaderStatusBadge()
{
    if (m_mod.isInstalled) {
        m_lblStatusBadge->setText(tr("🟢 Installed"));
        m_lblStatusBadge->setStyleSheet("color: #16a34a; font-weight: bold; font-size: 11px;");
    } else {
        m_lblStatusBadge->setText(tr("⚪ Not installed"));
        m_lblStatusBadge->setStyleSheet("color: #94a3b8; font-size: 11px;");
    }
}

void AccordionExtensionCard::updateActionButton()
{
    if (m_mod.isStaticCompiled) {
        if (m_mod.isInstalled) {
            m_btnAction->setText(tr("🔨 Recompile from Source"));
            m_btnAction->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 12px; border-radius: 4px; background-color: #0284c7; color: white; font-weight: bold; } QPushButton:hover { background-color: #0369a1; }");
        } else {
            m_btnAction->setText(tr("🔨 Compile from Source"));
            m_btnAction->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 14px; border-radius: 4px; background-color: #0284c7; color: white; font-weight: bold; } QPushButton:hover { background-color: #0369a1; }");
        }
    } else {
        if (m_mod.isInstalled) {
            m_btnAction->setText(tr("🗑️ Uninstall"));
            m_btnAction->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 12px; border-radius: 4px; background-color: rgba(239, 68, 68, 0.12); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.3); font-weight: bold; } QPushButton:hover { background-color: #ef4444; color: white; }");
        } else {
            m_btnAction->setText(tr("📥 Install"));
            m_btnAction->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 14px; border-radius: 4px; background-color: #0284c7; color: white; font-weight: bold; } QPushButton:hover { background-color: #0369a1; }");
        }
    }
}

void AccordionExtensionCard::updateCompileToggleStyles()
{
    QString activeStyle = "QPushButton { font-size: 10px; padding: 3px 8px; border-radius: 3px; background-color: #0284c7; color: white; font-weight: bold; border: none; }";
    QString inactiveStyle = "QPushButton { font-size: 10px; padding: 3px 8px; border-radius: 3px; background-color: #e2e8f0; color: #475569; border: none; } QPushButton:hover { background-color: #cbd5e1; }";
    m_btnSourceDynamic->setStyleSheet(m_btnSourceDynamic->isChecked() ? activeStyle : inactiveStyle);
    m_btnSourceStatic->setStyleSheet(m_btnSourceStatic->isChecked() ? activeStyle : inactiveStyle);
}
