/**
 * @file custom_source_dialog.cpp
 * @brief Implementation of custom Git repository sources and build flags configuration dialog.
 */

#include "custom_source_dialog.h"
#include "core/system/package_manager_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QProcess>
#include <QMessageBox>

#include <QTabWidget>
#include <QTextBrowser>

CustomSourceDialog::CustomSourceDialog(const QString &componentId,
                                       const CustomSourceConfig &initialConfig,
                                       const QString &diagnosticCommand,
                                       bool canCompile,
                                       QWidget *parent)
    : QDialog(parent)
    , m_componentId(componentId)
    , m_config(initialConfig)
    , m_diagnosticCommand(diagnosticCommand)
    , m_canCompile(canCompile)
    , m_tabs(nullptr)
    , m_comboPreset(nullptr)
    , m_editGitUrl(nullptr)
    , m_editBranchTag(nullptr)
    , m_editBuildFlags(nullptr)
    , m_lblPreview(nullptr)
    , m_btnTestConnection(nullptr)
    , m_btnReset(nullptr)
    , m_btnApply(nullptr)
    , m_txtDiagnosticOutput(nullptr)
    , m_btnRunDiagnostic(nullptr)
{
    if (m_canCompile && m_config.gitUrl.isEmpty()) {
        m_config = BuildConfig::instance().customSourceConfig(m_componentId);
    }

    if (m_canCompile) {
        setWindowTitle(tr("%1 — Configuration & Diagnostics").arg(m_componentId));
        resize(580, 460);
    } else {
        setWindowTitle(tr("%1 — Diagnostics & Package Status").arg(m_componentId));
        resize(560, 400);
    }
    setupUi();
}

void CustomSourceDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    if (!m_canCompile) {
        // Non-compilable / system package component view
        QGroupBox *grpInfo = new QGroupBox(tr("System Package Information"), this);
        QVBoxLayout *infoLay = new QVBoxLayout(grpInfo);
        infoLay->setSpacing(6);
        infoLay->setContentsMargins(12, 10, 12, 12);

        QString pkgName = PackageManagerHelper::packageNameForComponent(m_componentId);
        QLabel *lblPkg = new QLabel(
            tr("<p><b>Component:</b> <code>%1</code><br>"
               "<b>Distribution Package:</b> <code>%2</code></p>"
               "<p style='color: #64748b; font-size: 11px;'>This component is managed directly as a native system package by your Linux distribution package manager. It does not require custom Git repository cloning or source compilation.</p>")
            .arg(m_componentId, pkgName),
            grpInfo
        );
        lblPkg->setWordWrap(true);
        lblPkg->setTextInteractionFlags(Qt::TextSelectableByMouse);
        infoLay->addWidget(lblPkg);
        mainLayout->addWidget(grpInfo);

        // Verification Check & Diagnostics
        QGroupBox *grpDiag = new QGroupBox(tr("Diagnostic & Runtime Status"), this);
        QVBoxLayout *diagLay = new QVBoxLayout(grpDiag);
        diagLay->setSpacing(8);
        diagLay->setContentsMargins(12, 10, 12, 12);

        m_txtDiagnosticOutput = new QTextBrowser(grpDiag);
        m_txtDiagnosticOutput->setStyleSheet(
            "QTextBrowser {"
            "  background-color: #0f172a;"
            "  color: #e2e8f0;"
            "  font-family: 'JetBrains Mono', 'Fira Code', monospace;"
            "  font-size: 11px;"
            "  border: 1px solid #334155;"
            "  border-radius: 4px;"
            "  padding: 8px;"
            "}"
        );
        diagLay->addWidget(m_txtDiagnosticOutput, 1);

        QHBoxLayout *diagBtnLay = new QHBoxLayout();
        m_btnRunDiagnostic = new QPushButton(tr("▶ Run Verification Check"), grpDiag);
        m_btnRunDiagnostic->setCursor(Qt::PointingHandCursor);
        m_btnRunDiagnostic->setStyleSheet("QPushButton { font-weight: bold; padding: 4px 12px; }");
        connect(m_btnRunDiagnostic, &QPushButton::clicked, this, &CustomSourceDialog::runDiagnostic);
        diagBtnLay->addWidget(m_btnRunDiagnostic);
        diagBtnLay->addStretch();
        diagLay->addLayout(diagBtnLay);

        mainLayout->addWidget(grpDiag, 1);

        // Bottom Close button
        QHBoxLayout *bottomBtnLay = new QHBoxLayout();
        bottomBtnLay->addStretch();
        QPushButton *btnClose = new QPushButton(tr("Close"), this);
        btnClose->setCursor(Qt::PointingHandCursor);
        btnClose->setMinimumWidth(90);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        bottomBtnLay->addWidget(btnClose);
        mainLayout->addLayout(bottomBtnLay);

        runDiagnostic();
        return;
    }

    m_tabs = new QTabWidget(this);

    // ==========================================
    // TAB 1: Source & Build Options
    // ==========================================
    QWidget *tabSource = new QWidget();
    QVBoxLayout *srcLayout = new QVBoxLayout(tabSource);
    srcLayout->setSpacing(10);
    srcLayout->setContentsMargins(12, 12, 12, 12);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 1. Preset Selector
    m_comboPreset = new QComboBox(tabSource);
    m_comboPreset->addItem(tr("Official Stable"), "stable");
    m_comboPreset->addItem(tr("Development Git (main/master)"), "git");
    m_comboPreset->addItem(tr("Custom Git URL"), "custom");

    int presetIdx = 0;
    if (m_config.preset == "git") presetIdx = 1;
    else if (m_config.preset == "custom") presetIdx = 2;
    m_comboPreset->setCurrentIndex(presetIdx);

    formLayout->addRow(tr("Preset:"), m_comboPreset);

    // 2. Git URL
    m_editGitUrl = new QLineEdit(tabSource);
    m_editGitUrl->setPlaceholderText("https://github.com/...");
    m_editGitUrl->setText(m_config.gitUrl);
    formLayout->addRow(tr("Git Repository:"), m_editGitUrl);

    // 3. Branch / Tag
    m_editBranchTag = new QLineEdit(tabSource);
    m_editBranchTag->setPlaceholderText("master / main / v1.2.3");
    m_editBranchTag->setText(m_config.branchTag);
    formLayout->addRow(tr("Branch / Tag:"), m_editBranchTag);

    // 4. Extra Build Flags
    m_editBuildFlags = new QTextEdit(tabSource);
    m_editBuildFlags->setMaximumHeight(54);
    m_editBuildFlags->setPlaceholderText("-DCMAKE_BUILD_TYPE=Release -DENABLE_EXTRA=ON");
    m_editBuildFlags->setText(m_config.buildFlags);
    formLayout->addRow(tr("Build Flags:"), m_editBuildFlags);

    srcLayout->addLayout(formLayout);

    // Connect inputs to live preview
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CustomSourceDialog::onPresetChanged);
    connect(m_editGitUrl, &QLineEdit::textChanged, this, &CustomSourceDialog::updateEffectivePreview);
    connect(m_editBranchTag, &QLineEdit::textChanged, this, &CustomSourceDialog::updateEffectivePreview);
    connect(m_editBuildFlags, &QTextEdit::textChanged, this, &CustomSourceDialog::updateEffectivePreview);

    // Effective compilation parameters preview box
    QGroupBox *grpPreview = new QGroupBox(tr("Effective Compilation Parameters Preview"), tabSource);
    QVBoxLayout *prevLay = new QVBoxLayout(grpPreview);
    prevLay->setContentsMargins(8, 6, 8, 6);
    m_lblPreview = new QLabel(tabSource);
    m_lblPreview->setStyleSheet(
        "QLabel {"
        "  background-color: #0f172a;"
        "  color: #38bdf8;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
        "  font-size: 11px;"
        "  padding: 6px;"
        "  border: 1px solid #334155;"
        "  border-radius: 4px;"
        "}"
    );
    m_lblPreview->setWordWrap(true);
    m_lblPreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    prevLay->addWidget(m_lblPreview);
    srcLayout->addWidget(grpPreview);

    // Sub-buttons inside Source tab
    QHBoxLayout *srcBtnLay = new QHBoxLayout();
    m_btnTestConnection = new QPushButton(tr("Test Connection"), tabSource);
    m_btnTestConnection->setCursor(Qt::PointingHandCursor);
    connect(m_btnTestConnection, &QPushButton::clicked, this, &CustomSourceDialog::testConnection);
    srcBtnLay->addWidget(m_btnTestConnection);

    m_btnReset = new QPushButton(tr("↺ Reset Defaults"), tabSource);
    m_btnReset->setCursor(Qt::PointingHandCursor);
    connect(m_btnReset, &QPushButton::clicked, this, &CustomSourceDialog::resetToDefaults);
    srcBtnLay->addWidget(m_btnReset);
    srcBtnLay->addStretch();
    srcLayout->addLayout(srcBtnLay);

    m_tabs->addTab(tabSource, tr("🔧 Source & Git Settings"));

    // ==========================================
    // TAB 2: Diagnostics & Integrity
    // ==========================================
    QWidget *tabDiag = new QWidget();
    QVBoxLayout *diagLay = new QVBoxLayout(tabDiag);
    diagLay->setSpacing(10);
    diagLay->setContentsMargins(12, 12, 12, 12);

    QLabel *lblDiagDesc = new QLabel(
        tr("Executes the official verification command to inspect binary version, system libraries, and runtime status."),
        tabDiag
    );
    lblDiagDesc->setStyleSheet("color: #64748b; font-size: 11px;");
    lblDiagDesc->setWordWrap(true);
    diagLay->addWidget(lblDiagDesc);

    m_txtDiagnosticOutput = new QTextBrowser(tabDiag);
    m_txtDiagnosticOutput->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #0f172a;"
        "  color: #e2e8f0;"
        "  font-family: 'JetBrains Mono', 'Fira Code', monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #334155;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "}"
    );
    diagLay->addWidget(m_txtDiagnosticOutput, 1);

    QHBoxLayout *diagBtnLay = new QHBoxLayout();
    m_btnRunDiagnostic = new QPushButton(tr("▶ Run Verification Check"), tabDiag);
    m_btnRunDiagnostic->setCursor(Qt::PointingHandCursor);
    m_btnRunDiagnostic->setStyleSheet("QPushButton { font-weight: bold; padding: 4px 12px; }");
    connect(m_btnRunDiagnostic, &QPushButton::clicked, this, &CustomSourceDialog::runDiagnostic);
    diagBtnLay->addWidget(m_btnRunDiagnostic);
    diagBtnLay->addStretch();
    diagLay->addLayout(diagBtnLay);

    m_tabs->addTab(tabDiag, tr("🔍 Diagnostics & Status"));

    mainLayout->addWidget(m_tabs, 1);

    // Global bottom buttons
    QHBoxLayout *bottomBtnLay = new QHBoxLayout();
    bottomBtnLay->addStretch();

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomBtnLay->addWidget(btnCancel);

    m_btnApply = new QPushButton(tr("Save Configuration"), this);
    m_btnApply->setCursor(Qt::PointingHandCursor);
    m_btnApply->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 5px 14px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }");
    connect(m_btnApply, &QPushButton::clicked, this, [this]() {
        m_config.preset = m_comboPreset->currentData().toString();
        m_config.gitUrl = m_editGitUrl->text().trimmed();
        m_config.branchTag = m_editBranchTag->text().trimmed();
        m_config.buildFlags = m_editBuildFlags->toPlainText().trimmed();
        BuildConfig::instance().setCustomSourceConfig(m_componentId, m_config);
        accept();
    });
    bottomBtnLay->addWidget(m_btnApply);

    mainLayout->addLayout(bottomBtnLay);

    onPresetChanged(presetIdx);
    updateEffectivePreview();
    runDiagnostic();
}

void CustomSourceDialog::onPresetChanged(int index)
{
    QString mode = m_comboPreset->itemData(index).toString();
    CustomSourceConfig def = BuildConfig::defaultSourceConfig(m_componentId);
    if (mode == "stable") {
        m_editGitUrl->setText(def.gitUrl);
        m_editBranchTag->setText(def.branchTag);
        m_editBuildFlags->setText(def.buildFlags);
        m_editGitUrl->setEnabled(false);
    } else if (mode == "git") {
        m_editGitUrl->setText(def.gitUrl);
        m_editBranchTag->setText("master");
        m_editGitUrl->setEnabled(false);
    } else {
        m_editGitUrl->setEnabled(true);
    }
    updateEffectivePreview();
}

void CustomSourceDialog::resetToDefaults()
{
    CustomSourceConfig def = BuildConfig::defaultSourceConfig(m_componentId);
    m_comboPreset->setCurrentIndex(0); // stable
    m_editGitUrl->setText(def.gitUrl);
    m_editBranchTag->setText(def.branchTag);
    m_editBuildFlags->setText(def.buildFlags);
    m_editGitUrl->setEnabled(false);
    updateEffectivePreview();
}

void CustomSourceDialog::updateEffectivePreview()
{
    if (!m_lblPreview) return;

    QString git = m_editGitUrl ? m_editGitUrl->text().trimmed() : QString();
    QString branch = m_editBranchTag ? m_editBranchTag->text().trimmed() : QString();
    QString flags = m_editBuildFlags ? m_editBuildFlags->toPlainText().trimmed() : QString();
    QString globalFlags = BuildConfig::instance().toCMakeArgs().join(" ");

    QString text = QString(
        "<b>Target Module:</b> %1<br>"
        "<b>Source Repo:</b> %2 &nbsp;[<b>Branch/Tag:</b> %3]<br>"
        "<b>Global System Flags:</b> <code>%4</code><br>"
        "<b>Component Extra Flags:</b> <code>%5</code><br>"
        "<b>Install Prefix:</b> <code>~/.local</code>"
    ).arg(m_componentId,
          git.isEmpty() ? "<i>(Official Repository)</i>" : git,
          branch.isEmpty() ? "<i>(Default Branch)</i>" : branch,
          globalFlags.isEmpty() ? "<i>(Standard)</i>" : globalFlags,
          flags.isEmpty() ? "<i>(None)</i>" : flags);

    m_lblPreview->setText(text);
}

void CustomSourceDialog::testConnection()
{
    QString url = m_editGitUrl->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("Test Connection"), tr("Please specify a valid Git repository URL."));
        return;
    }

    m_btnTestConnection->setEnabled(false);
    m_btnTestConnection->setText(tr("Testing..."));

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        m_btnTestConnection->setEnabled(true);
        m_btnTestConnection->setText(tr("Test Connection"));

        if (status == QProcess::NormalExit && exitCode == 0) {
            QMessageBox::information(this, tr("Connection Successful"),
                tr("Successfully contacted the Git remote repository!"));
        } else {
            QMessageBox::warning(this, tr("Connection Failed"),
                tr("Failed to contact Git remote repository.<br>Please check the network connection and repository URL."));
        }
        proc->deleteLater();
    });

    proc->start("git", QStringList() << "ls-remote" << "--heads" << url);
}

CustomSourceConfig CustomSourceDialog::config() const
{
    return m_config;
}

void CustomSourceDialog::runDiagnostic()
{
    if (!m_txtDiagnosticOutput) return;

    QString cmd = m_diagnosticCommand;
    if (cmd.isEmpty()) {
        cmd = m_componentId + " --version";
    }

    m_txtDiagnosticOutput->setPlainText(tr("Executing diagnostic test: %1 ...\n").arg(cmd));

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, cmd](int exitCode, QProcess::ExitStatus status) {
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();

        QString report = tr("Command: %1\n").arg(cmd);
        report += tr("Exit Status: %1 (code %2)\n\n").arg(status == QProcess::NormalExit ? tr("Normal") : tr("Crash"), QString::number(exitCode));

        if (!out.isEmpty()) {
            report += tr("=== Standard Output ===\n%1\n\n").arg(out);
        }
        if (!err.isEmpty()) {
            report += tr("=== Standard Error / Warnings ===\n%1\n\n").arg(err);
        }
        if (out.isEmpty() && err.isEmpty()) {
            report += tr("Command finished with no output. Component executable appears functional.");
        }

        m_txtDiagnosticOutput->setPlainText(report);
        proc->deleteLater();
    });

    if (cmd.contains('|') || cmd.contains('>') || cmd.contains('&') || cmd.contains(';')) {
        proc->start("/bin/sh", QStringList() << "-c" << cmd);
    } else {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            QString prog = parts.takeFirst();
            proc->start(prog, parts);
        } else {
            m_txtDiagnosticOutput->setPlainText(tr("No diagnostic command available for %1.").arg(m_componentId));
        }
    }
}
