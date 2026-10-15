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
#include <QThreadPool>
#include <QPointer>
#include <QMenu>

ExtensionRowWidget::ExtensionRowWidget(const ExtensionModuleInfo &mod, QWidget *parent)
    : QFrame(parent)
    , m_mod(mod)
    , m_updateAvailable(false)
{
    m_sourceConfig = BuildConfig::instance().customSourceConfig(m_mod.id);
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
    QHBoxLayout *mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(12, 6, 12, 6);
    mainLay->setSpacing(10);

    // ==============================================================
    // LEFT: Component Badge (Plugin / Extension) + Name + Info (i)
    // ==============================================================
    m_lblTitle = new QLabel(this);
    if (m_mod.isEngine) {
        m_lblTitle->setText(QString("<span style='background-color: #0284c7; color: white; font-size: 10px; font-weight: bold; padding: 2px 7px; border-radius: 4px; letter-spacing: 0.5px;'>%1</span> &nbsp;<b style='font-size: 12px; color: #0f172a;'>%2</b>")
            .arg(tr("PLUGIN"), m_mod.displayName));
    } else {
        m_lblTitle->setText(QString("<span style='background-color: rgba(148, 163, 184, 0.22); color: #475569; font-size: 10px; font-weight: 600; padding: 2px 6px; border-radius: 4px;'>%1</span> &nbsp;<b style='font-size: 12px; color: #1e293b;'>%2</b>")
            .arg(tr("EXTENSION"), m_mod.displayName));
    }
    m_lblTitle->setTextInteractionFlags(Qt::NoTextInteraction);
    mainLay->addWidget(m_lblTitle);

    m_btnInfo = new QPushButton("ⓘ", this);
    m_btnInfo->setFixedSize(20, 20);
    m_btnInfo->setCursor(Qt::PointingHandCursor);
    m_btnInfo->setToolTip(tr("Show details, descriptions and technical capabilities"));
    m_btnInfo->setStyleSheet(
        "QPushButton {"
        "    font-weight: bold; border-radius: 10px; background-color: rgba(100, 116, 139, 0.12); color: #475569; border: 1px solid rgba(100, 116, 139, 0.22); font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0284c7; color: white; border-color: #0284c7;"
        "}"
    );
    connect(m_btnInfo, &QPushButton::clicked, this, &ExtensionRowWidget::showFeatureDetails);
    mainLay->addWidget(m_btnInfo);

    mainLay->addStretch(1);

    // ==============================================================
    // RIGHT: Clickable State Badge + [ Action ] + [ 🔧 Tools ]
    // ==============================================================

    // 1. Clickable Status Badge (pill shape)
    m_btnStateBadge = new QPushButton(this);
    m_btnStateBadge->setCursor(Qt::PointingHandCursor);
    m_btnStateBadge->setFixedHeight(24);
    connect(m_btnStateBadge, &QPushButton::clicked, this, [this]() {
        if (!m_mod.canCompile) return;
        auto curMode = BuildConfig::instance().componentSourceMode(m_mod.id);
        if (curMode == BuildConfig::ComponentSourceMode::LocalSource) {
            BuildConfig::instance().setComponentSourceMode(m_mod.id, BuildConfig::ComponentSourceMode::SystemBinary);
        } else {
            BuildConfig::instance().setComponentSourceMode(m_mod.id, BuildConfig::ComponentSourceMode::LocalSource);
        }
        refreshState();
        emit statusChanged();
    });
    mainLay->addWidget(m_btnStateBadge);

    // 2. Primary Action Button [ Installa / Disinstalla / Aggiorna ]
    m_btnAction = new QPushButton(this);
    m_btnAction->setCursor(Qt::PointingHandCursor);
    m_btnAction->setFixedHeight(26);
    m_btnAction->setMinimumWidth(82);
    connect(m_btnAction, &QPushButton::clicked, this, &ExtensionRowWidget::handleActionClicked);
    mainLay->addWidget(m_btnAction);

    // 3. Wrench Tools Button [ 🔧 ]
    m_btnWrench = new QPushButton(this);
    m_btnWrench->setCursor(Qt::PointingHandCursor);
    m_btnWrench->setFixedSize(26, 26);
    m_btnWrench->setIcon(QIcon(":/icons/wrench.svg"));
    m_btnWrench->setIconSize(QSize(15, 15));
    m_btnWrench->setToolTip(m_mod.canCompile ? tr("Source options, git repository & diagnostics")
                                             : tr("System package information & diagnostic verification"));
    m_btnWrench->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(100, 116, 139, 0.08);"
        "    border: 1px solid rgba(100, 116, 139, 0.22);"
        "    border-radius: 6px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(2, 132, 199, 0.15);"
        "    border-color: #0284c7;"
        "}"
    );
    connect(m_btnWrench, &QPushButton::clicked, this, &ExtensionRowWidget::openCustomSourceDialog);
    mainLay->addWidget(m_btnWrench);

    updateSourceToggleStyles();
}

void ExtensionRowWidget::updateSourceToggleStyles()
{
    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);

    if (!hasSys && !hasSrc) {
        if (activeMode == BuildConfig::ComponentSourceMode::LocalSource && m_mod.canCompile) {
            m_btnStateBadge->setEnabled(true);
            m_btnStateBadge->setText(tr("⚡ Source (Pending Build)"));
            m_btnStateBadge->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(2, 132, 199, 0.12); color: #0284c7; border: 1px dashed rgba(2, 132, 199, 0.6);"
                "    border-radius: 12px; padding: 2px 9px; font-size: 11px; font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(2, 132, 199, 0.22); border-color: #0284c7;"
                "}"
            );
            m_btnStateBadge->setToolTip(tr("Source build mode active (pending compilation).\nClick to switch to system package mode."));
        } else {
            m_btnStateBadge->setEnabled(m_mod.canCompile);
            m_btnStateBadge->setText(tr("Not installed"));
            m_btnStateBadge->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(148, 163, 184, 0.12); color: #94a3b8; border: 1px solid rgba(148, 163, 184, 0.25);"
                "    border-radius: 12px; padding: 2px 9px; font-size: 11px; font-weight: 500;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(148, 163, 184, 0.22); color: #475569;"
                "}"
            );
            m_btnStateBadge->setToolTip(m_mod.canCompile ? tr("Not installed.\nClick to switch to source compilation mode.")
                                                         : tr("This component is neither installed nor compilable on this system."));
        }
    } else if (activeMode == BuildConfig::ComponentSourceMode::LocalSource) {
        m_btnStateBadge->setEnabled(m_mod.canCompile);
        m_btnStateBadge->setText(hasSrc ? tr("⚡ Compiled") : tr("⚡ Source (Pending Build)"));
        m_btnStateBadge->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(2, 132, 199, 0.12); color: #0284c7; border: 1px solid rgba(2, 132, 199, 0.4);"
            "    border-radius: 12px; padding: 2px 9px; font-size: 11px; font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "    background-color: rgba(2, 132, 199, 0.22); border-color: #0284c7;"
            "}"
        );
        m_btnStateBadge->setToolTip(hasSrc ? tr("In use: built from source with native CPU optimization (-O3).\nClick to switch to system package.")
                                           : tr("Source build mode active (pending compilation).\nClick to switch to system package."));
    } else {
        m_btnStateBadge->setEnabled(m_mod.canCompile);
        m_btnStateBadge->setText(tr("📦 System"));
        m_btnStateBadge->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(16, 185, 129, 0.12); color: #059669; border: 1px solid rgba(16, 185, 129, 0.4);"
            "    border-radius: 12px; padding: 2px 9px; font-size: 11px; font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "    background-color: rgba(16, 185, 129, 0.22); border-color: #059669;"
            "}"
        );
        m_btnStateBadge->setToolTip(m_mod.canCompile ? tr("In use: official distribution package.\nClick to switch to source compilation.")
                                                     : tr("In use: official distribution package."));
    }
}

void ExtensionRowWidget::refreshState()
{
    ExtensionFeatureRegistry::refreshModuleStatus(m_mod);

    updateSourceToggleStyles();

    m_updateAvailable = false;
    m_candidateVersion.clear();
    updateActionButtons();

    // Query candidate version asynchronously or from cache so the GUI thread never stalls
    if (m_mod.isInstalled && !m_mod.isStaticCompiled) {
        if (PackageManagerHelper::isCandidateVersionCached(m_mod.id)) {
            m_candidateVersion = PackageManagerHelper::queryPackageCandidateVersion(m_mod.id);
            m_updateAvailable = !m_candidateVersion.isEmpty();
            updateActionButtons();
        } else {
            QPointer<ExtensionRowWidget> self = this;
            const QString compId = m_mod.id;
            QThreadPool::globalInstance()->start([self, compId]() {
                QString candVer = PackageManagerHelper::queryPackageCandidateVersion(compId);
                if (!candVer.isEmpty()) {
                    QMetaObject::invokeMethod(self, [self, candVer]() {
                        if (self) {
                            self->m_candidateVersion = candVer;
                            self->m_updateAvailable = true;
                            self->updateActionButtons();
                        }
                    }, Qt::QueuedConnection);
                }
            });
        }
    }
}

void ExtensionRowWidget::updateActionButtons()
{
    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);

    if (activeMode == BuildConfig::ComponentSourceMode::LocalSource) {
        // Local source mode: button is "Compile" (if not compiled) or "Remove" (if compiled)
        if (hasSrc) {
            m_btnAction->setText(tr("Remove"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(100, 116, 139, 0.08); color: #475569; font-weight: 500; border: 1px solid rgba(100, 116, 139, 0.22); padding: 2px 10px; border-radius: 5px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: rgba(239, 68, 68, 0.1); color: #dc2626; border-color: rgba(239, 68, 68, 0.35); }"
            );
            m_btnAction->setToolTip(tr("Removes locally compiled binaries from ~/.local/bin"));
        } else {
            m_btnAction->setText(tr("Compile"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: #0284c7; color: white; font-weight: 600; border: 1px solid #0284c7; padding: 2px 10px; border-radius: 5px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #0369a1; }"
            );
            m_btnAction->setToolTip(tr("Compile this component from official source code with high-performance flags"));
        }
    } else {
        // System binary mode: button is "Install", "Uninstall", or "Update"
        if (m_updateAvailable) {
            m_btnAction->setText(QString(tr("Update ↑ %1")).arg(m_candidateVersion));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: #059669; color: white; font-weight: 600; border: 1px solid #059669; padding: 2px 10px; border-radius: 5px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #047857; }"
            );
            m_btnAction->setToolTip(tr("An update (%1) is available in system repositories.").arg(m_candidateVersion));
        } else if (hasSys) {
            m_btnAction->setText(tr("Uninstall"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(100, 116, 139, 0.08); color: #475569; font-weight: 500; border: 1px solid rgba(100, 116, 139, 0.22); padding: 2px 10px; border-radius: 5px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: rgba(239, 68, 68, 0.1); color: #dc2626; border-color: rgba(239, 68, 68, 0.35); }"
            );
            m_btnAction->setToolTip(tr("Removes the package from the operating system"));
        } else {
            m_btnAction->setText(tr("Install"));
            m_btnAction->setStyleSheet(
                "QPushButton {"
                "    background-color: #0284c7; color: white; font-weight: 600; border: 1px solid #0284c7; padding: 2px 10px; border-radius: 5px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #0369a1; }"
            );
            m_btnAction->setToolTip(tr("Installs the official package via the system package manager"));
        }
    }
}

void ExtensionRowWidget::handleActionClicked()
{
    bool success = false;
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);

    if (activeMode == BuildConfig::ComponentSourceMode::LocalSource) {
        if (hasSrc) {
            // Remove locally compiled build
            if (QMessageBox::question(this, tr("Delete Compiled Binaries"),
                    tr("Do you want to remove the compiled binaries of '%1' from ~/.local/bin?").arg(m_mod.displayName),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                BuildConfig::removeLocalSourceBuild(m_mod.id);
                success = true;
            }
        } else {
            // Trigger compilation
            QString targetId = m_mod.id;
            if (PackageManagerHelper::isCompilableEngine(m_mod.id)) {
                targetId = m_mod.id;
            } else if (m_mod.category == "PDF") {
                targetId = "engine-poppler";
            } else if (m_mod.category == "Archive") {
                targetId = "engine-libarchive";
            }
            success = PackageManagerHelper::triggerEngineCompilation(targetId, this);
        }
    } else {
        // System package mode
        if (m_updateAvailable) {
            // Upgrade from repo
            success = PackageManagerHelper::installComponentInteractive(m_mod.id, this);
        } else if (hasSys) {
            // Uninstall system package (with safety checks)
            success = PackageManagerHelper::uninstallComponentInteractive(m_mod.id, this);
        } else {
            // Install system package
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
    QProcess *proc = new QProcess(this);
    QString cmd = m_mod.diagnosticCommand;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, cmd](int exitCode, QProcess::ExitStatus status) {
        QString output = QString::fromUtf8(proc->readAllStandardOutput() + proc->readAllStandardError()).trimmed();
        if (output.isEmpty()) output = (exitCode == 0) ? tr("Diagnostic command executed successfully with no errors.") : tr("Diagnostic command exited with error code %1.").arg(exitCode);

        if (status == QProcess::NormalExit && exitCode == 0) {
            QMessageBox::information(this, tr("Diagnostic Passed"),
                tr("<p><b>%1</b> is working properly.</p><pre>%2</pre>").arg(m_mod.displayName, output.toHtmlEscaped()));
        } else {
            QMessageBox::warning(this, tr("Diagnostic Failed"),
                tr("<p>Could not verify <b>%1</b>.</p><p>Command: <code>%2</code></p><pre>%3</pre>").arg(m_mod.displayName, cmd, output.toHtmlEscaped()));
        }
        proc->deleteLater();
    });

    QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        proc->deleteLater();
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
    m_sourceConfig = BuildConfig::instance().customSourceConfig(m_mod.id);
    CustomSourceDialog dlg(m_mod.id, m_sourceConfig, m_mod.diagnosticCommand, m_mod.canCompile, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (m_mod.canCompile) {
            m_sourceConfig = dlg.config();
            BuildConfig::instance().setCustomSourceConfig(m_mod.id, m_sourceConfig);
            updateSourceToggleStyles();
            updateActionButtons();
            emit statusChanged();
        }
    }
}
