/**
 * @file extension_row_widget.cpp
 * @brief Implementation of the compact 2-column tile widget for extensions and modules.
 */

#include "extension_row_widget.h"
#include "diagnostic_report_dialog.h"
#include "core/system/package_manager_helper.h"
#include "core/system/build_config.h"
#include "core/services/theme_manager.h"

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
#include <QAction>
#include <QGuiApplication>
#include <QClipboard>

static QString componentIcon(const QString &id, const QString &category)
{
    if (id == QLatin1String("engine-poppler")) return QStringLiteral("📄");
    if (id == QLatin1String("pdfimages")) return QStringLiteral("🖼️");
    if (id == QLatin1String("poppler-data")) return QStringLiteral("🌐");
    if (id == QLatin1String("qpdf")) return QStringLiteral("🔧");
    if (id == QLatin1String("mupdf") || id == QLatin1String("libmupdf")) return QStringLiteral("⚡");
    if (id == QLatin1String("tesseract")) return QStringLiteral("📝");
    if (id == QLatin1String("openssl")) return QStringLiteral("🔒");
    if (category == QLatin1String("Archive")) return QStringLiteral("📦");
    if (category == QLatin1String("Media")) {
        if (id.contains(QLatin1String("audio")) || id.contains(QLatin1String("taglib")) || id.contains(QLatin1String("fftw")))
            return QStringLiteral("🎵");
        return QStringLiteral("🎬");
    }
    if (category == QLatin1String("Office") || category == QLatin1String("OfficeImage")) {
        if (id.contains(QLatin1String("image")) || id.contains(QLatin1String("format")))
            return QStringLiteral("🎨");
        return QStringLiteral("📑");
    }
    return QStringLiteral("⚙️");
}

ExtensionRowWidget::ExtensionRowWidget(const ExtensionModuleInfo &mod, QWidget *parent)
    : QFrame(parent)
    , m_mod(mod)
    , m_updateAvailable(false)
{
    m_sourceConfig = BuildConfig::instance().customSourceConfig(m_mod.id);
    setObjectName(QStringLiteral("ExtensionRowWidget"));
    setFixedHeight(46);

    setupUi();
    refreshState();
}

void ExtensionRowWidget::setupUi()
{
    QHBoxLayout *mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(12, 4, 8, 4);
    mainLay->setSpacing(8);

    // ==============================================================
    // LEFT: Component Icon & Name
    // ==============================================================
    m_lblIcon = new QLabel(componentIcon(m_mod.id, m_mod.category), this);
    m_lblIcon->setObjectName(QStringLiteral("TileIcon"));
    m_lblIcon->setFixedWidth(24);
    m_lblIcon->setAlignment(Qt::AlignCenter);
    mainLay->addWidget(m_lblIcon);

    m_lblTitle = new QLabel(m_mod.displayName, this);
    m_lblTitle->setObjectName(QStringLiteral("TileTitle"));
    m_lblTitle->setToolTip(m_mod.summary);
    m_lblTitle->setTextInteractionFlags(Qt::NoTextInteraction);
    mainLay->addWidget(m_lblTitle, 1);

    // ==============================================================
    // RIGHT: 3 Direct Action Buttons
    // 1. Install / Uninstall / Health (🟢 / ⚪ / ⚠️)
    // 2. Switch Package (📦) <-> Source (⚡)
    // 3. Advanced Options (···)
    // ==============================================================
    QHBoxLayout *actionLay = new QHBoxLayout();
    actionLay->setContentsMargins(0, 0, 0, 0);
    actionLay->setSpacing(6);

    // Button 1: Install / Uninstall / Health
    m_btnInstall = new QPushButton(this);
    m_btnInstall->setObjectName(QStringLiteral("BtnInstallState"));
    m_btnInstall->setCursor(Qt::PointingHandCursor);
    m_btnInstall->setFixedSize(28, 26);
    connect(m_btnInstall, &QPushButton::clicked, this, &ExtensionRowWidget::handleInstallClicked);
    actionLay->addWidget(m_btnInstall);

    // Button 2: Switch between Package (📦) <-> Source (⚡)
    m_btnSourceSwitch = new QPushButton(this);
    m_btnSourceSwitch->setObjectName(QStringLiteral("BtnSourceSwitch"));
    m_btnSourceSwitch->setCursor(Qt::PointingHandCursor);
    m_btnSourceSwitch->setFixedSize(28, 26);
    connect(m_btnSourceSwitch, &QPushButton::clicked, this, &ExtensionRowWidget::handleSourceSwitchClicked);
    actionLay->addWidget(m_btnSourceSwitch);

    // Button 3: Compact Options Button (···)
    m_btnOptions = new QPushButton(QStringLiteral("···"), this);
    m_btnOptions->setObjectName(QStringLiteral("ExtensionOptionsButton"));
    m_btnOptions->setCursor(Qt::PointingHandCursor);
    m_btnOptions->setFixedSize(28, 26);
    m_btnOptions->setToolTip(tr("Advanced options (diagnostics, details, custom build paths)"));
    connect(m_btnOptions, &QPushButton::clicked, this, &ExtensionRowWidget::openOptionsMenu);
    actionLay->addWidget(m_btnOptions);

    mainLay->addLayout(actionLay, 0);

    updateThemeStyles();
}

void ExtensionRowWidget::updateThemeStyles()
{
    const bool dark = ThemeManager::isDarkTheme();
    const ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    QString tileBg;
    QString tileBorder;
    QString tileHoverBg;
    QString tileHoverBorder;
    QString titleColor;
    QString btnOptionsBg;
    QString btnOptionsText;

    if (preset == ThemePreset::ZincGray) {
        tileBg = QStringLiteral("rgba(255, 255, 255, 0.05)");
        tileBorder = QStringLiteral("rgba(255, 255, 255, 0.10)");
        tileHoverBg = QStringLiteral("rgba(255, 255, 255, 0.09)");
        tileHoverBorder = QStringLiteral("rgba(255, 255, 255, 0.24)");
        titleColor = QStringLiteral("#ffffff");
        btnOptionsBg = QStringLiteral("rgba(255, 255, 255, 0.08)");
        btnOptionsText = QStringLiteral("#f4f4f5");
    } else if (dark) {
        tileBg = QStringLiteral("rgba(30, 41, 59, 0.40)");
        tileBorder = QStringLiteral("rgba(56, 189, 248, 0.16)");
        tileHoverBg = QStringLiteral("rgba(30, 41, 59, 0.65)");
        tileHoverBorder = QStringLiteral("rgba(56, 189, 248, 0.32)");
        titleColor = QStringLiteral("#f8fafc");
        btnOptionsBg = QStringLiteral("rgba(56, 189, 248, 0.10)");
        btnOptionsText = QStringLiteral("#38bdf8");
    } else {
        tileBg = QStringLiteral("rgba(0, 0, 0, 0.03)");
        tileBorder = QStringLiteral("rgba(100, 116, 139, 0.18)");
        tileHoverBg = QStringLiteral("rgba(0, 0, 0, 0.06)");
        tileHoverBorder = QStringLiteral("rgba(2, 132, 199, 0.35)");
        titleColor = QStringLiteral("#0f172a");
        btnOptionsBg = QStringLiteral("rgba(100, 116, 139, 0.08)");
        btnOptionsText = QStringLiteral("#475569");
    }

    const QString rowStyle = QString(
        "QFrame#ExtensionRowWidget {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 9px;"
        "}"
        "QFrame#ExtensionRowWidget:hover {"
        "    background-color: %3;"
        "    border: 1px solid %4;"
        "}"
    ).arg(tileBg, tileBorder, tileHoverBg, tileHoverBorder);

    setStyleSheet(rowStyle);

    if (m_lblTitle) {
        m_lblTitle->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: %1; background: transparent; border: none;").arg(titleColor));
    }
    if (m_lblIcon) {
        m_lblIcon->setStyleSheet(QStringLiteral("font-size: 16px; background: transparent; border: none;"));
    }

    if (m_btnOptions) {
        m_btnOptions->setStyleSheet(QString(
            "QPushButton#ExtensionOptionsButton {"
            "    background-color: %1;"
            "    color: %2;"
            "    border: 1px solid rgba(255, 255, 255, 0.16);"
            "    border-radius: 6px;"
            "    font-weight: bold;"
            "    font-size: 13px;"
            "}"
            "QPushButton#ExtensionOptionsButton:hover {"
            "    background-color: rgba(255, 255, 255, 0.20);"
            "    border-color: rgba(255, 255, 255, 0.35);"
            "}"
        ).arg(btnOptionsBg, btnOptionsText));
    }

    updateButtonStates();
}

void ExtensionRowWidget::updateButtonStates()
{
    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);
    bool isInstalled = m_mod.isInstalled || hasSrc || hasSys;

    // -------------------------------------------------------------
    // Button 1: Install / Uninstall / Health State (🟢 / ⚪ / ⚠️)
    // -------------------------------------------------------------
    if (m_btnInstall) {
        m_btnInstall->setText(QString());
        m_btnInstall->setIconSize(QSize(15, 15));
        if (isInstalled) {
            m_btnInstall->setIcon(QIcon(QStringLiteral(":/icons/status_ok.svg")));
            m_btnInstall->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(16, 185, 129, 0.16);"
                "    border: 1px solid rgba(16, 185, 129, 0.40);"
                "    border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(239, 68, 68, 0.25);"
                "    border-color: rgba(239, 68, 68, 0.50);"
                "}"
            );
            if (hasSrc && (activeMode == BuildConfig::ComponentSourceMode::LocalSource || !hasSys)) {
                m_btnInstall->setToolTip(tr("Installed from source into ~/.local.\nClick to remove compiled build of '%1'.").arg(m_mod.displayName));
            } else {
                m_btnInstall->setToolTip(tr("Installed and operational.\nClick to uninstall '%1'.").arg(m_mod.displayName));
            }
        } else {
            m_btnInstall->setIcon(QIcon(QStringLiteral(":/icons/status_missing.svg")));
            m_btnInstall->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(255, 255, 255, 0.06);"
                "    border: 1px solid rgba(255, 255, 255, 0.16);"
                "    border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(16, 185, 129, 0.25);"
                "    border-color: #10b981;"
                "}"
            );
            bool wantsSource = (activeMode == BuildConfig::ComponentSourceMode::LocalSource);
            auto hwProfile = BuildConfig::instance().activeHardwareProfile();
            if (hwProfile == BuildConfig::HardwareProfile::AllSource ||
                hwProfile == BuildConfig::HardwareProfile::AllStable ||
                (hwProfile == BuildConfig::HardwareProfile::MissingOnly && !hasSys)) {
                wantsSource = true;
            }
            if (wantsSource && (m_mod.canCompile || PackageManagerHelper::isCompilableEngine(m_mod.id))) {
                m_btnInstall->setToolTip(tr("Not installed.\nClick to compile '%1' from source into ~/.local.").arg(m_mod.displayName));
            } else {
                m_btnInstall->setToolTip(tr("Not installed.\nClick to install '%1'.").arg(m_mod.displayName));
            }
        }
    }

    // -------------------------------------------------------------
    // Button 2: Switch between Package (📦) <-> Source (⚡)
    // -------------------------------------------------------------
    if (m_btnSourceSwitch) {
        m_btnSourceSwitch->setText(QString());
        m_btnSourceSwitch->setIconSize(QSize(16, 16));
        if (activeMode == BuildConfig::ComponentSourceMode::LocalSource) {
            m_btnSourceSwitch->setIcon(QIcon(QStringLiteral(":/icons/source_lightning.svg")));
            m_btnSourceSwitch->setEnabled(m_mod.canCompile);
            m_btnSourceSwitch->setStyleSheet(
                "QPushButton {"
                "    background-color: rgba(56, 189, 248, 0.18);"
                "    border: 1px solid rgba(56, 189, 248, 0.45);"
                "    border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(16, 185, 129, 0.25);"
                "    border-color: #10b981;"
                "}"
            );
            m_btnSourceSwitch->setToolTip(tr("Native source build active.\nClick to switch to system distribution package."));
        } else {
            m_btnSourceSwitch->setIcon(QIcon(QStringLiteral(":/icons/binary_package.svg")));
            if (m_mod.canCompile) {
                m_btnSourceSwitch->setEnabled(true);
                m_btnSourceSwitch->setStyleSheet(
                    "QPushButton {"
                    "    background-color: rgba(16, 185, 129, 0.14);"
                    "    border: 1px solid rgba(16, 185, 129, 0.35);"
                    "    border-radius: 6px;"
                    "}"
                    "QPushButton:hover {"
                    "    background-color: rgba(56, 189, 248, 0.25);"
                    "    border-color: #38bdf8;"
                    "}"
                );
                m_btnSourceSwitch->setToolTip(tr("System package active.\nClick to switch to native source build mode."));
            } else {
                m_btnSourceSwitch->setEnabled(false);
                m_btnSourceSwitch->setStyleSheet(
                    "QPushButton {"
                    "    background-color: rgba(255, 255, 255, 0.04);"
                    "    border: 1px solid rgba(255, 255, 255, 0.10);"
                    "    border-radius: 6px;"
                    "}"
                );
                m_btnSourceSwitch->setToolTip(tr("System package active (source build not available for this component)."));
            }
        }
    }
}

void ExtensionRowWidget::refreshState()
{
    ExtensionFeatureRegistry::refreshModuleStatus(m_mod);
    updateButtonStates();

    m_updateAvailable = false;
    m_candidateVersion.clear();

    // Query candidate version asynchronously so the GUI thread never stalls
    if (m_mod.isInstalled && !m_mod.isStaticCompiled) {
        if (PackageManagerHelper::isCandidateVersionCached(m_mod.id)) {
            m_candidateVersion = PackageManagerHelper::queryPackageCandidateVersion(m_mod.id);
            m_updateAvailable = !m_candidateVersion.isEmpty();
            updateButtonStates();
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
                            self->updateButtonStates();
                        }
                    }, Qt::QueuedConnection);
                }
            });
        }
    }
}

void ExtensionRowWidget::handleInstallClicked()
{
    bool success = false;
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);
    bool isInstalled = m_mod.isInstalled || hasSrc || hasSys;

    if (isInstalled) {
        if (hasSrc && hasSys) {
            QMessageBox msg(this);
            msg.setWindowTitle(tr("Uninstall Options: %1").arg(m_mod.displayName));
            msg.setText(tr("<h3>Uninstall Component: %1</h3>"
                           "<p>This component is present both as a locally compiled build and as an OS system package.</p>"
                           "<p>What would you like to remove?</p>").arg(m_mod.displayName));
            QPushButton *btnRemoveSrc = msg.addButton(tr("🗑️ Remove Compiled Build Only"), QMessageBox::ActionRole);
            QPushButton *btnUninstallAll = msg.addButton(tr("❌ Uninstall Completely (Source && System)"), QMessageBox::DestructiveRole);
            msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
            msg.exec();

            if (msg.clickedButton() == btnRemoveSrc) {
                BuildConfig::removeLocalSourceBuild(m_mod.id);
                success = true;
            } else if (msg.clickedButton() == btnUninstallAll) {
                BuildConfig::removeLocalSourceBuild(m_mod.id);
                PackageManagerHelper::uninstallComponentInteractive(m_mod.id, this);
                success = true;
            }
        } else if (hasSrc) {
            if (QMessageBox::question(this, tr("Remove Compiled Build"),
                    tr("Do you want to remove locally compiled binaries for '%1'?").arg(m_mod.displayName),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                BuildConfig::removeLocalSourceBuild(m_mod.id);
                success = true;
            }
        } else if (hasSys) {
            if (QMessageBox::question(this, tr("Uninstall Component"),
                    tr("Do you want to uninstall '%1' from this system?").arg(m_mod.displayName),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                success = PackageManagerHelper::uninstallComponentInteractive(m_mod.id, this);
            }
        }
    } else {
        // Not installed: if user wants compilation and component is compilable, compile directly!
        bool wantsSource = (activeMode == BuildConfig::ComponentSourceMode::LocalSource);
        auto hwProfile = BuildConfig::instance().activeHardwareProfile();
        if (hwProfile == BuildConfig::HardwareProfile::AllSource ||
            hwProfile == BuildConfig::HardwareProfile::AllStable ||
            (hwProfile == BuildConfig::HardwareProfile::MissingOnly && !hasSys)) {
            wantsSource = true;
        }

        if (wantsSource && (m_mod.canCompile || PackageManagerHelper::isCompilableEngine(m_mod.id))) {
            QString targetId = m_mod.id;
            if (!PackageManagerHelper::isCompilableEngine(m_mod.id)) {
                if (m_mod.id == QLatin1String("poppler-data")) targetId = QStringLiteral("poppler-data");
                else if (m_mod.category == QLatin1String("PDF")) targetId = QStringLiteral("engine-poppler");
                else if (m_mod.category == QLatin1String("Archive")) targetId = QStringLiteral("engine-libarchive");
                else if (m_mod.category == QLatin1String("Office")) targetId = QStringLiteral("engine-office");
                else if (m_mod.category == QLatin1String("Media")) targetId = QStringLiteral("engine-mpv");
            }
            success = PackageManagerHelper::triggerEngineCompilation(targetId, this);
        } else {
            success = PackageManagerHelper::installComponentInteractive(m_mod.id, this);
        }
    }

    if (success) {
        refreshState();
        emit statusChanged();
    }
}

void ExtensionRowWidget::handleSourceSwitchClicked()
{
    if (!m_mod.canCompile) return;
    auto curMode = BuildConfig::instance().componentSourceMode(m_mod.id);
    if (curMode == BuildConfig::ComponentSourceMode::LocalSource) {
        BuildConfig::instance().setComponentSourceMode(m_mod.id, BuildConfig::ComponentSourceMode::SystemBinary);
    } else {
        BuildConfig::instance().setComponentSourceMode(m_mod.id, BuildConfig::ComponentSourceMode::LocalSource);
    }
    refreshState();
    emit statusChanged();
}

void ExtensionRowWidget::openOptionsMenu()
{
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("ExtensionOptionsMenu"));

    const bool dark = ThemeManager::isDarkTheme();
    if (dark) {
        menu.setStyleSheet(
            "QMenu#ExtensionOptionsMenu {"
            "    background-color: #1e293b;"
            "    border: 1px solid rgba(255, 255, 255, 0.15);"
            "    border-radius: 8px;"
            "    padding: 4px;"
            "}"
            "QMenu#ExtensionOptionsMenu::item {"
            "    padding: 6px 14px;"
            "    border-radius: 4px;"
            "    color: #f1f5f9;"
            "    font-size: 12px;"
            "}"
            "QMenu#ExtensionOptionsMenu::item:selected {"
            "    background-color: #0284c7;"
            "    color: #ffffff;"
            "}"
            "QMenu#ExtensionOptionsMenu::separator {"
            "    height: 1px;"
            "    background: rgba(255, 255, 255, 0.1);"
            "    margin: 4px 6px;"
            "}"
        );
    } else {
        menu.setStyleSheet(
            "QMenu#ExtensionOptionsMenu {"
            "    background-color: #ffffff;"
            "    border: 1px solid rgba(0, 0, 0, 0.12);"
            "    border-radius: 8px;"
            "    padding: 4px;"
            "}"
            "QMenu#ExtensionOptionsMenu::item {"
            "    padding: 6px 14px;"
            "    border-radius: 4px;"
            "    color: #0f172a;"
            "    font-size: 12px;"
            "}"
            "QMenu#ExtensionOptionsMenu::item:selected {"
            "    background-color: #0284c7;"
            "    color: #ffffff;"
            "}"
            "QMenu#ExtensionOptionsMenu::separator {"
            "    height: 1px;"
            "    background: rgba(0, 0, 0, 0.08);"
            "    margin: 4px 6px;"
            "}"
        );
    }

    bool hasSys = BuildConfig::hasSystemBinary(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    auto curMode = BuildConfig::instance().componentSourceMode(m_mod.id);

    // 1. Diagnostics
    QAction *actHealth = menu.addAction(tr("🩺 Run Diagnostic Verification..."));
    connect(actHealth, &QAction::triggered, this, &ExtensionRowWidget::runRuntimeHealthCheck);

    if (m_mod.canCompile) {
        QAction *actBuild = menu.addAction(tr("🛠️ Check Compiler Prerequisites..."));
        connect(actBuild, &QAction::triggered, this, &ExtensionRowWidget::runBuildReadinessCheck);
    }

    menu.addSeparator();

    // 2. Technical Details & C++ Hooks
    QAction *actDetails = menu.addAction(tr("🔍 Technical Details && C++ Hooks..."));
    connect(actDetails, &QAction::triggered, this, &ExtensionRowWidget::showFeatureDetails);

    // 3. Custom Source Configuration
    if (m_mod.canCompile) {
        QAction *actCustom = menu.addAction(tr("⚙️ Configure Custom Source && Flags..."));
        connect(actCustom, &QAction::triggered, this, &ExtensionRowWidget::openCustomSourceDialog);
    }

    // 4. Copy diagnostic command or info to clipboard
    QAction *actCopy = menu.addAction(tr("📋 Copy Component Information"));
    connect(actCopy, &QAction::triggered, this, [this, hasSys, hasSrc, curMode]() {
        QString info = QString("Component: %1 (%2)\nCategory: %3\nSystem Package: %4\nInstalled: %5 (System: %6, Source: %7)\nActive Mode: %8\nDiagnostic Command: %9")
            .arg(m_mod.displayName, m_mod.id, m_mod.category, m_mod.systemPackage,
                 (m_mod.isInstalled ? "Yes" : "No"), (hasSys ? "Yes" : "No"), (hasSrc ? "Yes" : "No"),
                 (curMode == BuildConfig::ComponentSourceMode::LocalSource ? "LocalSource" : "SystemBinary"),
                 m_mod.diagnosticCommand);
        QGuiApplication::clipboard()->setText(info);
    });

    // 5. Contextual Uninstall/Removal
    if (curMode == BuildConfig::ComponentSourceMode::LocalSource && hasSrc) {
        menu.addSeparator();
        QAction *actRemove = menu.addAction(tr("🗑️ Remove Local Source Build"));
        connect(actRemove, &QAction::triggered, this, &ExtensionRowWidget::handleInstallClicked);
    } else if (hasSys) {
        menu.addSeparator();
        QAction *actUninstall = menu.addAction(tr("🗑️ Uninstall System Package..."));
        connect(actUninstall, &QAction::triggered, this, &ExtensionRowWidget::handleInstallClicked);
    }

    if (m_btnOptions) {
        menu.exec(m_btnOptions->mapToGlobal(QPoint(0, m_btnOptions->height())));
    }
}

void ExtensionRowWidget::runDiagnosticTest()
{
    auto activeMode = BuildConfig::instance().componentSourceMode(m_mod.id);
    bool hasSrc = BuildConfig::hasSourceBinary(m_mod.id);
    if (m_mod.canCompile && activeMode == BuildConfig::ComponentSourceMode::LocalSource && !hasSrc) {
        runBuildReadinessCheck();
    } else {
        runRuntimeHealthCheck();
    }
}

void ExtensionRowWidget::runRuntimeHealthCheck()
{
    QString cmd = m_mod.diagnosticCommand.trimmed();
    QList<QPair<QString, bool>> subResources;
    for (const auto &sub : m_mod.subDependencies) {
        subResources.append({sub.name, sub.isInstalled});
    }

    if (cmd.isEmpty()) {
        DiagnosticReportDialog::showRuntimeReport(
            this,
            m_mod.displayName,
            true,
            QStringLiteral("(internal verification)"),
            tr("Component is installed, active, and operational."),
            subResources,
            0
        );
        return;
    }

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, cmd, subResources](int exitCode, QProcess::ExitStatus status) {
        QString outStr = QString::fromUtf8(proc->readAllStandardOutput());
        QString errStr = QString::fromUtf8(proc->readAllStandardError());
        QString output = (outStr + (errStr.isEmpty() ? "" : ("\n" + errStr))).trimmed();
        if (output.isEmpty()) {
            output = (exitCode == 0) ? tr("Runtime diagnostic command executed successfully.")
                                     : tr("Runtime diagnostic command exited with error code %1.").arg(exitCode);
        }

        bool ok = (status == QProcess::NormalExit && exitCode == 0);
        DiagnosticReportDialog::showRuntimeReport(
            this,
            m_mod.displayName,
            ok,
            cmd,
            output,
            subResources,
            exitCode
        );
        proc->deleteLater();
    });

    proc->start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << cmd);
}

void ExtensionRowWidget::runBuildReadinessCheck()
{
    QStringList missingList;
    QString summaryLog;
    bool ready = ExtensionFeatureRegistry::checkBuildReadiness(m_mod, missingList, summaryLog);

    DiagnosticReportDialog::showBuildReadinessReport(
        this,
        m_mod.displayName,
        ready,
        missingList,
        summaryLog
    );
}

void ExtensionRowWidget::showFeatureDetails()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("%1 - Details & C++ Hooks").arg(m_mod.displayName));
    dlg.resize(520, 420);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setSpacing(10);

    QLabel *hdr = new QLabel(QString("<b>%1</b> (%2)").arg(m_mod.displayName, m_mod.id), &dlg);
    hdr->setStyleSheet(QStringLiteral("font-size: 14px;"));
    lay->addWidget(hdr);

    QLabel *lblSum = new QLabel(m_mod.summary, &dlg);
    lblSum->setWordWrap(true);
    lblSum->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px;"));
    lay->addWidget(lblSum);

    QTextBrowser *tb = new QTextBrowser(&dlg);
    tb->setOpenExternalLinks(true);

    QString html;
    html += QString("<h3>%1</h3><ul>").arg(tr("Features enabled in BubezView:"));
    for (const auto &feat : m_mod.features) {
        html += QString("<li><b>%1</b>: %2<br/><small style='color: #0284c7;'>%3: %4 | %5: %6</small></li>")
            .arg(feat.title, feat.description, tr("Hook"), feat.codeHook, tr("Menu"), feat.menuLocation);
    }
    html += QStringLiteral("</ul>");

    if (!m_mod.subDependencies.isEmpty()) {
        html += QString("<h3>%1</h3><ul>").arg(tr("Sub-dependencies & Language packs:"));
        for (const auto &sub : m_mod.subDependencies) {
            html += QString("<li><b>%1</b> (%2) — %3: <span style='color: %4;'>%5</span><br/><small style='color: #94a3b8;'>%6</small></li>")
                .arg(sub.name, sub.systemPackage, tr("Status"), sub.isInstalled ? "#10b981" : "#ef4444",
                     sub.isInstalled ? tr("Present") : tr("Missing"), sub.hint);
        }
        html += QStringLiteral("</ul>");
    }

    tb->setHtml(html);
    lay->addWidget(tb, 1);

    QDialogButtonBox *bbox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    lay->addWidget(bbox);

    dlg.exec();
}

void ExtensionRowWidget::openCustomSourceDialog()
{
    CustomSourceDialog dlg(m_mod.id, m_sourceConfig, m_mod.diagnosticCommand, m_mod.canCompile, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_sourceConfig = dlg.config();
        BuildConfig::instance().setCustomSourceConfig(m_mod.id, m_sourceConfig);
        refreshState();
        emit statusChanged();
    }
}
