/**
 * @file build_console_dialog.cpp
 * @brief Implementation of the dedicated floating build console.
 */

#include "build_console_dialog.h"
#include "update_manager.h"
#include "build_config.h"
#include "base_skin.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QTextCursor>

BuildConsoleDialog::BuildConsoleDialog(QWidget *parent)
    : QDialog(parent)
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
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BuildConsoleDialog::setupUi()
{
    setWindowTitle(tr("Compilation Console — BubezView"));
    resize(700, 480);
    setMinimumSize(560, 380);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);

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
    m_progressBar->setRange(0, 0); // Indeterminate / busy
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

bool BuildConsoleDialog::startBuildTarget(const QString &targetName)
{
    QString scriptPath = UpdateManager::instance().findInstallerScript();
    if (scriptPath.isEmpty()) {
        m_lblStatus->setText(tr("❌ Installer script not found."));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_txtLog->append(tr("[Error] Could not locate 'Installa' recompilation script in system or source repository."));
        m_btnCancel->hide();
        m_btnClose->show();
        return false;
    }

    m_succeeded = false;
    m_txtLog->clear();
    m_txtLog->append(tr(">>> Starting compilation: %1 --cli --update\n").arg(scriptPath));

    m_progressBar->setRange(0, 0);
    m_lblStatus->setText(tr("⚙️ Configuring CMake build environment..."));
    m_btnCancel->show();
    m_btnRestart->hide();
    m_btnClose->hide();

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildConsoleDialog::onReadyReadStandardOutput);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &BuildConsoleDialog::onProcessFinished);
    }

    QStringList args;
    args << "--cli" << "--update";
    if (!targetName.isEmpty()) {
        args << "--target" << targetName;
    }
    args.append(BuildConfig::instance().toCMakeArgs());
    m_process->start(scriptPath, args);
    return true;
}

bool BuildConsoleDialog::startBuildForComponent(const QString &componentId)
{
    QString scriptPath = UpdateManager::instance().findInstallerScript();
    if (scriptPath.isEmpty()) {
        m_lblStatus->setText(tr("❌ Installer script not found."));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_txtLog->append(tr("[Error] Could not locate 'Installa' recompilation script in system or source repository."));
        m_btnCancel->hide();
        m_btnClose->show();
        return false;
    }

    m_succeeded = false;
    m_txtLog->clear();
    m_txtLog->append(tr(">>> Starting compilation: %1 --cli --update\n").arg(scriptPath));

    m_progressBar->setRange(0, 0);
    m_lblStatus->setText(tr("⚙️ Configuring CMake build environment..."));
    m_btnCancel->show();
    m_btnRestart->hide();
    m_btnClose->hide();

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildConsoleDialog::onReadyReadStandardOutput);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &BuildConsoleDialog::onProcessFinished);
    }

    QStringList args;
    args << "--cli" << "--update";
    QString normComp = componentId.trimmed().toLower();
    if (!normComp.isEmpty()) {
        args << "--build-component" << normComp;
        CustomSourceConfig srcCfg = BuildConfig::instance().customSourceConfig(normComp);
        if (!srcCfg.gitUrl.isEmpty()) {
            args << "--git-url" << srcCfg.gitUrl;
        }
        if (!srcCfg.branchTag.isEmpty()) {
            args << "--git-branch" << srcCfg.branchTag;
        }
        if (!srcCfg.buildFlags.isEmpty()) {
            args << "--extra-build-flags" << srcCfg.buildFlags;
        }
    }
    args.append(BuildConfig::instance().toCMakeArgs());
    m_process->start(scriptPath, args);
    return true;
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
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1500);
        }
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
    if (auto *skin = qobject_cast<BaseSkin*>(top)) {
        curPath = skin->currentFilePath();
    } else {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *bs = qobject_cast<BaseSkin*>(w)) {
                curPath = bs->currentFilePath();
                if (!curPath.isEmpty()) break;
            }
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
            if (m_process) {
                m_process->kill();
                m_process->waitForFinished(1000);
            }
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}
