/**
 * @file extension_row_widget.cpp
 * @brief Implementation of the two-level compact row widget.
 */

#include "extension_row_widget.h"
#include "core/package_manager_helper.h"
#include "core/build_config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>

ExtensionRowWidget::ExtensionRowWidget(const ExtensionModuleInfo &mod, QWidget *parent)
    : QFrame(parent)
    , m_mod(mod)
    , m_updateAvailable(false)
{
    m_sourceConfig.preset = "stable";
    setObjectName("ExtensionRowWidget");
    if (m_mod.isEngine) {
        setStyleSheet(
            "#ExtensionRowWidget {"
            "    background-color: rgba(2, 132, 199, 0.05);"
            "    border: 1px solid rgba(2, 132, 199, 0.28);"
            "    border-left: 4px solid #0284c7;"
            "    border-radius: 6px;"
            "    margin-bottom: 4px;"
            "}"
            "#ExtensionRowWidget:hover {"
            "    background-color: rgba(2, 132, 199, 0.09);"
            "    border-color: rgba(2, 132, 199, 0.45);"
            "    border-left: 4px solid #0284c7;"
            "}"
        );
    } else {
        setStyleSheet(
            "#ExtensionRowWidget {"
            "    background-color: rgba(255, 255, 255, 0.7);"
            "    border: 1px solid rgba(148, 163, 184, 0.25);"
            "    border-left: 3px solid #94a3b8;"
            "    border-radius: 6px;"
            "    margin-bottom: 3px;"
            "}"
            "#ExtensionRowWidget:hover {"
            "    background-color: rgba(248, 250, 252, 0.95);"
            "    border-color: rgba(100, 116, 139, 0.4);"
            "    border-left: 3px solid #64748b;"
            "}"
        );
    }

    setupUi();
    refreshState();
}

void ExtensionRowWidget::setupUi()
{
    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 8, 12, 8);
    mainLay->setSpacing(4);

    // ==============================================================
    // LEVEL 1: Full-Width Title + (i) Info Button
    // ==============================================================
    QHBoxLayout *topLay = new QHBoxLayout();
    topLay->setSpacing(6);
    topLay->setContentsMargins(0, 0, 0, 0);

    m_lblTitle = new QLabel(this);
    if (m_mod.isEngine) {
        m_lblTitle->setText(QString("<span style='background-color: #0284c7; color: white; font-size: 10px; font-weight: bold; padding: 2px 7px; border-radius: 3px; letter-spacing: 0.5px;'>⚡ %1</span> &nbsp;<b style='font-size: 13px; color: #0f172a;'>%2</b>")
            .arg(tr("CORE ENGINE"), m_mod.displayName));
    } else {
        m_lblTitle->setText(QString("<span style='background-color: rgba(148, 163, 184, 0.25); color: #475569; font-size: 10px; font-weight: 600; padding: 2px 6px; border-radius: 3px;'>%1</span> &nbsp;<b style='font-size: 12px; color: #1e293b;'>%2</b>")
            .arg(tr("EXTENSION"), m_mod.displayName));
    }
    m_lblTitle->setTextInteractionFlags(Qt::NoTextInteraction);
    topLay->addWidget(m_lblTitle, 1);

    m_btnInfo = new QPushButton("ⓘ", this);
    m_btnInfo->setFixedSize(22, 22);
    m_btnInfo->setCursor(Qt::PointingHandCursor);
    m_btnInfo->setToolTip(tr("Show enabled C++ classes and user interface integrations"));
    m_btnInfo->setStyleSheet(
        "QPushButton {"
        "    font-weight: bold; border-radius: 11px; background-color: rgba(100, 116, 139, 0.12); color: #475569; border: 1px solid rgba(100, 116, 139, 0.25); font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0284c7; color: white; border-color: #0284c7;"
        "}"
    );
    connect(m_btnInfo, &QPushButton::clicked, this, &ExtensionRowWidget::showFeatureDetails);
    topLay->addWidget(m_btnInfo);

    mainLay->addLayout(topLay);

    // ==============================================================
    // LEVEL 2: Left Sub-Info (Human Description) | Right Control Cluster
    // ==============================================================
    QHBoxLayout *botLay = new QHBoxLayout();
    botLay->setSpacing(8);
    botLay->setContentsMargins(0, 0, 0, 0);

    // Sub-info on left (Clean human-friendly description + package tag)
    QString summaryText = m_mod.summary.isEmpty() ? m_mod.displayName : m_mod.summary;
    m_lblSubInfo = new QLabel(
        QString("<span style='color: #475569; font-size: 11px;'>%1</span> &nbsp;<span style='color: #94a3b8; font-size: 10px;'>[<code>%2</code>]</span>")
            .arg(summaryText, m_mod.systemPackage),
        this
    );
    m_lblSubInfo->setTextInteractionFlags(Qt::NoTextInteraction);
    m_lblSubInfo->setWordWrap(true);
    botLay->addWidget(m_lblSubInfo, 1);

    // Right side compact controls
    // 1. Segmented Source Toggle [Binary | Source ✎]
    QHBoxLayout *srcToggleLay = new QHBoxLayout();
    srcToggleLay->setSpacing(1);

    m_btnSourceBinary = new QPushButton(tr("Binary"), this);
    m_btnSourceBinary->setCursor(Qt::PointingHandCursor);
    m_btnSourceBinary->setFixedHeight(24);

    m_btnSourceBuild = new QPushButton(tr("Source"), this);
    m_btnSourceBuild->setCursor(Qt::PointingHandCursor);
    m_btnSourceBuild->setFixedHeight(24);

    m_btnEditSource = new QPushButton(tr("✎"), this);
    m_btnEditSource->setCursor(Qt::PointingHandCursor);
    m_btnEditSource->setFixedSize(22, 24);
    m_btnEditSource->setToolTip(tr("Configure custom Git repository and build flags"));
    m_btnEditSource->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(145, 155, 163, 0.12); color: #64748b; border: 1px solid rgba(145, 155, 163, 0.25); border-radius: 3px; font-size: 10px;"
        "}"
        "QPushButton:hover { background-color: rgba(145, 155, 163, 0.25); color: #334155; }"
    );
    connect(m_btnEditSource, &QPushButton::clicked, this, &ExtensionRowWidget::openCustomSourceDialog);

    updateSourceToggleStyles();

    connect(m_btnSourceBinary, &QPushButton::clicked, this, [this]() {
        m_mod.isStaticCompiled = false;
        if (m_mod.id == "engine-poppler") {
            BuildConfig::instance().setStaticPdf(false);
            BuildConfig::instance().save();
        } else if (m_mod.id == "engine-libarchive") {
            BuildConfig::instance().setStaticArchive(false);
            BuildConfig::instance().save();
        } else if (m_mod.id == "engine-office") {
            BuildConfig::instance().setStaticOffice(false);
            BuildConfig::instance().save();
        }
        updateSourceToggleStyles();
        updateActionButtons();
        emit statusChanged();
    });

    connect(m_btnSourceBuild, &QPushButton::clicked, this, [this]() {
        m_mod.isStaticCompiled = true;
        if (m_mod.id == "engine-poppler") {
            BuildConfig::instance().setStaticPdf(true);
            BuildConfig::instance().save();
        } else if (m_mod.id == "engine-libarchive") {
            BuildConfig::instance().setStaticArchive(true);
            BuildConfig::instance().save();
        } else if (m_mod.id == "engine-office") {
            BuildConfig::instance().setStaticOffice(true);
            BuildConfig::instance().save();
        }
        updateSourceToggleStyles();
        updateActionButtons();
        emit statusChanged();
    });

    srcToggleLay->addWidget(m_btnSourceBinary);
    srcToggleLay->addWidget(m_btnSourceBuild);
    srcToggleLay->addWidget(m_btnEditSource);

    bool canCompile = m_mod.canCompile;
    m_btnSourceBinary->setVisible(canCompile);
    m_btnSourceBuild->setVisible(canCompile);
    m_btnEditSource->setVisible(canCompile);

    botLay->addLayout(srcToggleLay);

    // 2. Verification Button / Status
    m_btnVerify = new QPushButton(tr("Verify"), this);
    m_btnVerify->setCursor(Qt::PointingHandCursor);
    m_btnVerify->setFixedHeight(24);
    m_btnVerify->setStyleSheet(
        "QPushButton {"
        "    font-size: 11px; padding: 2px 9px; border-radius: 4px; background-color: rgba(100, 116, 139, 0.1); color: #334155; border: 1px solid rgba(100, 116, 139, 0.25); font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: rgba(2, 132, 199, 0.15); color: #0284c7; border-color: rgba(2, 132, 199, 0.4); }"
    );
    m_btnVerify->setToolTip(tr("Run integrity check (%1)").arg(m_mod.diagnosticCommand));
    connect(m_btnVerify, &QPushButton::clicked, this, &ExtensionRowWidget::runDiagnosticTest);
    botLay->addWidget(m_btnVerify);

    // 3. Compact Action Button [Install / Uninstall / Update]
    m_btnAction = new QPushButton(this);
    m_btnAction->setCursor(Qt::PointingHandCursor);
    m_btnAction->setFixedHeight(24);
    m_btnAction->setMinimumWidth(82);
    connect(m_btnAction, &QPushButton::clicked, this, &ExtensionRowWidget::handleActionClicked);
    botLay->addWidget(m_btnAction);

    mainLay->addLayout(botLay);
}

void ExtensionRowWidget::updateSourceToggleStyles()
{
    QString activeStyle = "QPushButton { background-color: #0284c7; color: white; font-weight: bold; border: 1px solid #0284c7; padding: 2px 8px; border-radius: 3px; font-size: 11px; }";
    QString inactiveStyle = "QPushButton { background-color: rgba(100, 116, 139, 0.12); color: #475569; border: 1px solid rgba(100, 116, 139, 0.25); padding: 2px 8px; border-radius: 3px; font-size: 11px; } QPushButton:hover { background-color: rgba(100, 116, 139, 0.22); color: #0f172a; }";

    if (m_mod.isStaticCompiled) {
        m_btnSourceBinary->setStyleSheet(inactiveStyle);
        m_btnSourceBuild->setStyleSheet(activeStyle);
    } else {
        m_btnSourceBinary->setStyleSheet(activeStyle);
        m_btnSourceBuild->setStyleSheet(inactiveStyle);
    }
}

void ExtensionRowWidget::refreshState()
{
    ExtensionFeatureRegistry::refreshModuleStatus(m_mod);

    // Check candidate version from system package manager if installed via source
    if (m_mod.isInstalled && m_mod.isStaticCompiled) {
        m_candidateVersion = PackageManagerHelper::queryPackageCandidateVersion(m_mod.id);
        // If candidate version is available and local build is static, present update option
        m_updateAvailable = !m_candidateVersion.isEmpty();
    } else {
        m_updateAvailable = false;
    }

    updateActionButtons();
}

void ExtensionRowWidget::updateActionButtons()
{
    if (m_updateAvailable) {
        m_btnAction->setText(QString(tr("Update ↑ %1")).arg(m_candidateVersion));
        m_btnAction->setStyleSheet(
            "QPushButton {"
            "    background-color: #059669; color: white; font-weight: bold; border: 1px solid #059669; padding: 2px 8px; border-radius: 4px; font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: #047857; }"
        );
        m_btnAction->setToolTip(tr("A newer version (%1) is available in your operating system repository.").arg(m_candidateVersion));
    } else if (m_mod.isInstalled) {
        m_btnAction->setText(tr("Uninstall"));
        m_btnAction->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(100, 116, 139, 0.1); color: #475569; font-weight: 500; border: 1px solid rgba(100, 116, 139, 0.25); padding: 2px 8px; border-radius: 4px; font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: rgba(239, 68, 68, 0.1); color: #dc2626; border-color: rgba(239, 68, 68, 0.35); }"
        );
        m_btnAction->setToolTip(tr("Remove this component from the system"));
    } else {
        if (m_mod.isStaticCompiled) {
            m_btnAction->setText(tr("Build"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: #0284c7; color: white; font-weight: bold; border: 1px solid #0284c7; padding: 2px 8px; border-radius: 4px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #0369a1; }"
            );
            m_btnAction->setToolTip(tr("Compile and embed this component from source"));
        } else {
            m_btnAction->setText(tr("Install"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: #0284c7; color: white; font-weight: bold; border: 1px solid #0284c7; padding: 2px 8px; border-radius: 4px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #0369a1; }"
            );
            m_btnAction->setToolTip(tr("Install official package '%1' via package manager").arg(m_mod.systemPackage));
        }
    }
}

void ExtensionRowWidget::handleActionClicked()
{
    bool success = false;
    if (m_updateAvailable) {
        // Upgrade from repo
        success = PackageManagerHelper::installComponentInteractive(m_mod.id, this);
        if (success) {
            m_mod.isStaticCompiled = false;
            updateSourceToggleStyles();
        }
    } else if (m_mod.isStaticCompiled) {
        // Trigger source compile
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

void ExtensionRowWidget::runDiagnosticTest()
{
    m_btnVerify->setEnabled(false);
    m_btnVerify->setText(tr("Verifying..."));

    QProcess *proc = new QProcess(this);
    QString cmd = m_mod.diagnosticCommand;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        m_btnVerify->setEnabled(true);
        if (status == QProcess::NormalExit && exitCode == 0) {
            m_btnVerify->setText(tr("✓ Verified"));
            m_btnVerify->setStyleSheet(
                "QPushButton {"
                "    font-size: 11px; padding: 2px 9px; border-radius: 3px; background-color: rgba(16, 185, 129, 0.15); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.35); font-weight: bold;"
                "}"
            );
        } else {
            m_btnVerify->setText(tr("❌ Failed"));
            m_btnVerify->setStyleSheet(
                "QPushButton {"
                "    font-size: 11px; padding: 2px 9px; border-radius: 3px; background-color: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.35); font-weight: bold;"
                "}"
            );
        }
        proc->deleteLater();
    });

    QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        m_btnVerify->setEnabled(true);
        m_btnVerify->setText(tr("Verify"));
        return;
    }

    QString prog = parts.takeFirst();
    proc->start(prog, parts);
}

void ExtensionRowWidget::showFeatureDetails()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("%1 - Details & C++ Hooks").arg(m_mod.displayName));
    dlg.resize(480, 360);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setSpacing(10);
    lay->setContentsMargins(14, 14, 14, 14);

    QLabel *lblHdr = new QLabel(QString("<h3>%1</h3><p style='color: #64748b;'>%2</p>")
                                .arg(m_mod.displayName, m_mod.summary), &dlg);
    lblHdr->setWordWrap(true);
    lay->addWidget(lblHdr);

    QTextBrowser *txt = new QTextBrowser(&dlg);
    txt->setOpenExternalLinks(true);
    QString html = "<h4>" + tr("Integrated C++ Methods & Capabilities:") + "</h4><ul>";
    for (const auto &feat : m_mod.features) {
        html += QString("<li><b>%1</b>: %2<br><span style='color: #64748b;'>UI:</span> %3<br><code style='color: #ef4444;'>%4</code></li><br>")
                    .arg(feat.title, feat.description, feat.menuLocation, feat.codeHook);
    }
    html += "</ul>";

    if (!m_mod.subDependencies.isEmpty()) {
        html += "<h4>" + tr("Optional Sub-Dependencies:") + "</h4><ul>";
        for (const auto &sub : m_mod.subDependencies) {
            html += QString("<li><b>%1</b> (<code>%2</code>): %3 - <i>%4</i></li>")
                        .arg(sub.name, sub.systemPackage, sub.hint, sub.isInstalled ? tr("Installed") : tr("Missing"));
        }
        html += "</ul>";
    }

    txt->setHtml(html);
    lay->addWidget(txt, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    lay->addWidget(box);

    dlg.exec();
}

void ExtensionRowWidget::openCustomSourceDialog()
{
    CustomSourceDialog dlg(m_mod.id, m_sourceConfig, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_sourceConfig = dlg.config();
        m_mod.isStaticCompiled = true;
        updateSourceToggleStyles();
        updateActionButtons();
        emit statusChanged();
    }
}
