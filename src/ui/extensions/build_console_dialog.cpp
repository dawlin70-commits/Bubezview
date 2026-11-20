/**
 * @file build_console_dialog.cpp
 * @brief Implementation of the dedicated floating build console.
 */

#include "build_console_dialog.h"
#include "update_manager.h"
#include "build_config.h"
#include "package_manager_helper.h"
#include "core/services/plugin_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QRegularExpression>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QTextCursor>

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <unistd.h>
#endif

BuildConsoleDialog::BuildConsoleDialog(QWidget *parent)
    : FramelessDialog(parent)
    , m_lblTitle(nullptr)
    , m_lblStatus(nullptr)
    , m_progressBar(nullptr)
    , m_txtLog(nullptr)
    , m_btnCancel(nullptr)
    , m_btnRestart(nullptr)
    , m_btnClose(nullptr)
    , m_process(nullptr)
    , m_succeeded(false)
{
    setupUi();
}

BuildConsoleDialog::~BuildConsoleDialog()
{
    terminateProcessGroup();
}

void BuildConsoleDialog::terminateProcessGroup()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }
#if defined(Q_OS_UNIX)
    qint64 pid = m_process->processId();
    if (pid > 0) {
        ::kill(-static_cast<pid_t>(pid), SIGTERM);
        if (!m_process->waitForFinished(500)) {
            ::kill(-static_cast<pid_t>(pid), SIGKILL);
            m_process->waitForFinished(500);
        }
        return;
    }
#endif
    m_process->kill();
    m_process->waitForFinished(500);
}

void BuildConsoleDialog::setupUi()
{
    setWindowTitle(tr("Compilation Console — BubezView"));
    resize(700, 480);
    setMinimumSize(560, 380);

    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Header section
    m_lblTitle = new QLabel(tr("<b>⚙️ Recompiling BubezView</b>"), this);
    m_lblTitle->setStyleSheet("font-size: 14px;");
    mainLayout->addWidget(m_lblTitle);

    m_lblStatus = new QLabel(tr("Initializing build environment..."), this);
    m_lblStatus->setStyleSheet("color: #0284c7; font-size: 12px; font-weight: bold;");
    mainLayout->addWidget(m_lblStatus);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet("QProgressBar { background-color: rgba(100, 116, 139, 0.2); border: none; border-radius: 3px; }"
                                 "QProgressBar::chunk { background-color: #0284c7; border-radius: 3px; }");
    mainLayout->addWidget(m_progressBar);

    // Terminal log view
    m_txtLog = new QTextEdit(this);
    m_txtLog->setReadOnly(true);
    m_txtLog->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0f172a;"
        "  color: #38bdf8;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'Source Code Pro', 'DejaVu Sans Mono', monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #334155;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    );
    mainLayout->addWidget(m_txtLog, 1);

    // Bottom action row
    QHBoxLayout *botLayout = new QHBoxLayout();
    botLayout->addStretch();

    m_btnCancel = new QPushButton(tr("Cancel Build"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet("QPushButton { background-color: rgba(239, 68, 68, 0.15); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.3); border-radius: 4px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #ef4444; color: white; }");
    connect(m_btnCancel, &QPushButton::clicked, this, &BuildConsoleDialog::cancelBuild);
    botLayout->addWidget(m_btnCancel);

    m_btnRestart = new QPushButton(tr("🚀 Restart BubezView Now"), this);
    m_btnRestart->setCursor(Qt::PointingHandCursor);
    m_btnRestart->setStyleSheet("QPushButton { background-color: #10b981; color: white; border-radius: 4px; padding: 6px 16px; font-weight: bold; } QPushButton:hover { background-color: #059669; }");
    m_btnRestart->hide();
    connect(m_btnRestart, &QPushButton::clicked, this, &BuildConsoleDialog::restartApplication);
    botLayout->addWidget(m_btnRestart);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet("QPushButton { padding: 6px 16px; border-radius: 4px; }");
    m_btnClose->hide();
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    botLayout->addWidget(m_btnClose);

    mainLayout->addLayout(botLayout);
}

bool BuildConsoleDialog::startBuild()
{
    return startBuildForComponent(QString());
}

bool BuildConsoleDialog::launchBuildProcess(const QStringList &extraArgs)
{
    if (isBuilding()) {
        return false;
    }

    const QString scriptPath = UpdateManager::instance().findInstallerScript();
    if (scriptPath.isEmpty()) {
        m_lblStatus->setText(tr("❌ Installer script not found."));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_txtLog->append(tr("[Error] Could not locate 'Install' script in system or source repository."));
        m_btnCancel->hide();
        m_btnClose->show();
        return false;
    }

    m_succeeded = false;
    m_txtLog->clear();
    m_txtLog->append(tr(">>> Starting compilation: %1 --cli --update\n").arg(scriptPath));

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(5);
    m_lblStatus->setText(tr("⚙️ Configuring CMake build environment..."));
    m_btnCancel->show();
    m_btnRestart->hide();
    m_btnClose->hide();

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
#if defined(Q_OS_UNIX)
        // Put the installer and all its spawned ninja/g++ children in a dedicated process group
        // so terminateProcessGroup() can cleanly abort the entire tree.
        m_process->setChildProcessModifier([]() {
            ::setpgid(0, 0);
        });
#endif
        connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildConsoleDialog::onReadyReadStandardOutput);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &BuildConsoleDialog::onProcessFinished);
    }

    QStringList args;
    args << QStringLiteral("--cli") << QStringLiteral("--update");
    const auto hwProf = BuildConfig::instance().activeHardwareProfile();
    if (hwProf != BuildConfig::HardwareProfile::SystemPackages) {
        args << QStringLiteral("--no-sudo");
    }
    args.append(extraArgs);
    args.append(BuildConfig::instance().toCMakeArgs());
    m_process->start(scriptPath, args);
    return true;
}

bool BuildConsoleDialog::startBuildTarget(const QString &targetName)
{
    QStringList extraArgs;
    if (!targetName.isEmpty()) {
        extraArgs << QStringLiteral("--target") << targetName;
    }
    return launchBuildProcess(extraArgs);
}

bool BuildConsoleDialog::startBuildForComponent(const QString &componentId)
{
    QStringList extraArgs;
    const QString normComp = componentId.trimmed().toLower();
    if (!normComp.isEmpty()) {
        extraArgs << QStringLiteral("--build-component") << normComp;
        const CustomSourceConfig srcCfg = BuildConfig::instance().customSourceConfig(normComp);
        if (!srcCfg.gitUrl.isEmpty()) {
            extraArgs << QStringLiteral("--git-url") << srcCfg.gitUrl;
        }
        if (!srcCfg.branchTag.isEmpty()) {
            extraArgs << QStringLiteral("--git-branch") << srcCfg.branchTag;
        }
        if (!srcCfg.buildFlags.isEmpty()) {
            extraArgs << QStringLiteral("--extra-build-flags") << srcCfg.buildFlags;
        }
    }
    return launchBuildProcess(extraArgs);
}

bool BuildConsoleDialog::isBuilding() const
{
    return (m_process && m_process->state() != QProcess::NotRunning);
}

void BuildConsoleDialog::onReadyReadStandardOutput()
{
    if (!m_process || !m_txtLog) return;
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    m_txtLog->insertPlainText(out);
    m_txtLog->moveCursor(QTextCursor::End);

    if (m_progressBar) {
        // Parse deterministic CMake/Make ([ 42%]) or Ninja ([12/48]) progress so the bar
        // advances smoothly instead of bouncing back and forth in indeterminate mode.
        static const QRegularExpression kMakePctRx(QStringLiteral(R"(\[\s*(\d{1,3})%\])"));
        static const QRegularExpression kNinjaFracRx(QStringLiteral(R"(\[\s*(\d+)\s*/\s*(\d+)\s*\])"));
        int parsedPct = -1;
        auto itPct = kMakePctRx.globalMatch(out);
        while (itPct.hasNext()) {
            const int v = itPct.next().captured(1).toInt();
            if (v >= 0 && v <= 100) parsedPct = qMax(parsedPct, v);
        }
        if (parsedPct < 0) {
            auto itFrac = kNinjaFracRx.globalMatch(out);
            while (itFrac.hasNext()) {
                const auto m = itFrac.next();
                const int cur = m.captured(1).toInt();
                const int tot = m.captured(2).toInt();
                if (tot > 0 && cur >= 0 && cur <= tot) {
                    parsedPct = qMax(parsedPct, (cur * 100) / tot);
                }
            }
        }
        if (parsedPct >= 0) {
            const int mapped = qBound(10, 10 + (parsedPct * 88) / 100, 98);
            if (mapped > m_progressBar->value()) {
                m_progressBar->setValue(mapped);
            }
        } else if (out.contains(QLatin1String("Configuring done")) && m_progressBar->value() < 12) {
            m_progressBar->setValue(12);
        } else if ((out.contains(QLatin1String("Linking")) || out.contains(QLatin1String("collegamento"))) && m_progressBar->value() < 92) {
            m_progressBar->setValue(92);
        }
    }

    if (m_lblStatus) {
        if (out.contains("Compilazione") || out.contains("Building") || out.contains("ninja")) {
            m_lblStatus->setText(tr("⚙️ Compiling updated modules in parallel..."));
        } else if (out.contains("CMake")) {
            m_lblStatus->setText(tr("🔧 Generating CMake build environment..."));
        } else if (out.contains("Linking") || out.contains("collegamento")) {
            m_lblStatus->setText(tr("🔗 Linking executable binary..."));
        }
    }
}

bool BuildConsoleDialog::startBuildComponents(const QStringList &componentIds)
{
    m_pendingComponents = componentIds;
    if (m_pendingComponents.isEmpty()) {
        return startBuild();
    }
    QString nextComp = m_pendingComponents.takeFirst();
    return startBuildForComponent(nextComp);
}

void BuildConsoleDialog::onProcessFinished(int exitCode, int exitStatus)
{
    m_progressBar->setRange(0, 100);

    if (exitStatus == 0 && exitCode == 0) { // QProcess::NormalExit == 0
        // If there are more components queued, compile the next one
        if (!m_pendingComponents.isEmpty()) {
            QString nextComp = m_pendingComponents.takeFirst();
            m_txtLog->append(tr("\n>>> Component build completed. Starting next component: %1...\n").arg(nextComp));
            startBuildForComponent(nextComp);
            return;
        }

        m_succeeded = true;
        m_progressBar->setValue(100);
        m_lblStatus->setText(tr("✅ Compilation completed successfully! Changes will take effect the next time you start BubezView."));
        m_lblStatus->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold;");
        m_txtLog->append(tr("\n[Success] Recompilation finished successfully!"));

        PluginManager::instance().discoverPlugins();
        PackageManagerHelper::clearCaches();

        m_btnCancel->hide();
        m_btnRestart->hide();
        m_btnClose->show();
        m_btnClose->setText(tr("Close"));
        emit buildSucceeded();
    } else {
        m_pendingComponents.clear();
        m_succeeded = false;
        m_progressBar->setValue(0);
        m_lblStatus->setText(QString(tr("❌ Compilation failed (exit code %1).")).arg(exitCode));
        m_lblStatus->setStyleSheet("color: #ef4444; font-size: 12px; font-weight: bold;");
        m_txtLog->append(QString(tr("\n[Failed] Build script terminated with exit code %1.")).arg(exitCode));

        m_btnCancel->hide();
        m_btnRestart->hide();
        m_btnClose->show();
        emit buildFailed(exitCode);
    }
}

void BuildConsoleDialog::cancelBuild()
{
    if (!isBuilding()) {
        reject();
        return;
    }

    QMessageBox::StandardButton res = QMessageBox::question(
        this,
        tr("Cancel Compilation"),
        tr("Are you sure you want to cancel the active build process?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        m_pendingComponents.clear();
        terminateProcessGroup();
        m_lblStatus->setText(tr("⚠️ Compilation cancelled by user."));
        m_lblStatus->setStyleSheet("color: #f59e0b; font-size: 12px; font-weight: bold;");
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_btnCancel->hide();
        m_btnClose->show();
    }
}

void BuildConsoleDialog::restartApplication()
{
    QString appPath = QApplication::applicationFilePath();
    QStringList args;

    // Detect if there is an active file open to restore it on relaunch
    QString curPath;
    QWidget *top = window();
    if (top) {
        curPath = top->property("currentFilePath").toString();
    }
    if (curPath.isEmpty()) {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            curPath = w->property("currentFilePath").toString();
            if (!curPath.isEmpty()) break;
        }
    }

    if (!curPath.isEmpty()) {
        args << curPath;
    }

    QProcess::startDetached(appPath, args);
    QApplication::quit();
}

void BuildConsoleDialog::closeEvent(QCloseEvent *event)
{
    if (isBuilding()) {
        QMessageBox::StandardButton res = QMessageBox::question(
            this,
            tr("Build in Progress"),
            tr("Compilation is currently running. Do you want to cancel it and close?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (res == QMessageBox::Yes) {
            m_pendingComponents.clear();
            terminateProcessGroup();
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}
