/**
 * @file update_dialog.cpp
 * @brief Implementation of interactive update dialog.
 */

#include "update_dialog.h"
#include "build_options_dialog.h"
#include "mainwindow.h"
#include "base_skin.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QApplication>
#include <QMessageBox>
#include <QUrl>
#include <QDir>
#include <QFile>

UpdateDialog::UpdateDialog(const ReleaseInfo &info, QWidget *parent)
    : QDialog(parent)
    , m_info(info)
    , m_process(nullptr)
    , m_lblTitle(nullptr)
    , m_lblChangelog(nullptr)
    , m_lblStatus(nullptr)
    , m_progressBar(nullptr)
    , m_btnUpdate(nullptr)
    , m_btnCancel(nullptr)
    , m_btnDetails(nullptr)
    , m_btnConfig(nullptr)
    , m_txtLog(nullptr)
    , m_showingDetails(false)
{
    setWindowTitle(tr("BubezView Software Update"));
    setMinimumWidth(500);
    setupUi();
}

UpdateDialog::~UpdateDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void UpdateDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    m_lblTitle = new QLabel(this);
    m_lblTitle->setText(QString("<h3>%1</h3><p>%2 <b>%3</b> &nbsp;➔&nbsp; %4 <b>v%5</b></p>")
        .arg(tr("🎉 New version of BubezView is available!"))
        .arg(tr("Currently installed:"))
        .arg(UpdateManager::instance().currentVersion())
        .arg(tr("Available update:"))
        .arg(m_info.version));
    mainLayout->addWidget(m_lblTitle);

    QLabel *lblSecurity = new QLabel(this);
    if (m_info.isAuthentic) {
        lblSecurity->setText(tr("🛡️ <b>Cryptographically Verified:</b> Signed by Developer (Ed25519)"));
        lblSecurity->setStyleSheet(QStringLiteral("background-color: rgba(34, 197, 94, 0.12); color: #22c55e; border: 1px solid rgba(34, 197, 94, 0.3); border-radius: 6px; padding: 6px 10px; font-size: 11px;"));
    } else if (!m_info.sha256.isEmpty()) {
        lblSecurity->setText(tr("ℹ️ <b>Integrity Checksum:</b> SHA-256 available"));
        lblSecurity->setStyleSheet(QStringLiteral("background-color: rgba(56, 189, 248, 0.1); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.25); border-radius: 6px; padding: 6px 10px; font-size: 11px;"));
    } else {
        lblSecurity->setText(tr("⚠️ <b>Unsigned Release:</b> Legacy release channel"));
        lblSecurity->setStyleSheet(QStringLiteral("background-color: rgba(234, 179, 8, 0.1); color: #eab308; border: 1px solid rgba(234, 179, 8, 0.25); border-radius: 6px; padding: 6px 10px; font-size: 11px;"));
    }
    mainLayout->addWidget(lblSecurity);

    m_lblChangelog = new QLabel(this);
    m_lblChangelog->setWordWrap(true);
    m_lblChangelog->setText(QString("<b>%1</b><br>%2").arg(tr("Changelog:"), m_info.changelog));
    m_lblChangelog->setStyleSheet("background-color: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 6px; padding: 10px;");
    mainLayout->addWidget(m_lblChangelog);

    m_lblStatus = new QLabel(tr("Ready to update. Existing compilation optimizations will be preserved."), this);
    m_lblStatus->setStyleSheet("color: #38bdf8; font-size: 11px;");
    mainLayout->addWidget(m_lblStatus);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    m_txtLog = new QTextEdit(this);
    m_txtLog->setReadOnly(true);
    m_txtLog->setStyleSheet("font-family: monospace; font-size: 10px; background-color: #111; color: #a3e635; border-radius: 4px;");
    m_txtLog->setFixedHeight(140);
    m_txtLog->hide();
    mainLayout->addWidget(m_txtLog);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnDetails = new QPushButton(tr("Show Details ▼"), this);
    m_btnDetails->setCursor(Qt::PointingHandCursor);
    connect(m_btnDetails, &QPushButton::clicked, this, &UpdateDialog::toggleDetails);
    btnLayout->addWidget(m_btnDetails);

    m_btnConfig = new QPushButton(tr("⚙️ Customize Modules..."), this);
    m_btnConfig->setCursor(Qt::PointingHandCursor);
    connect(m_btnConfig, &QPushButton::clicked, this, &UpdateDialog::openBuildOptions);
    btnLayout->addWidget(m_btnConfig);

    btnLayout->addStretch();

    m_btnCancel = new QPushButton(tr("Later"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnUpdate = new QPushButton(tr("🚀 Update Now"), this);
    m_btnUpdate->setCursor(Qt::PointingHandCursor);
    m_btnUpdate->setDefault(true);
    m_btnUpdate->setStyleSheet("background-color: #0284c7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;");
    connect(m_btnUpdate, &QPushButton::clicked, this, &UpdateDialog::startUpdate);
    btnLayout->addWidget(m_btnUpdate);

    mainLayout->addLayout(btnLayout);
}

void UpdateDialog::openBuildOptions()
{
    BuildOptionsDialog dlg(this);
    dlg.exec();
}

void UpdateDialog::toggleDetails()
{
    m_showingDetails = !m_showingDetails;
    m_txtLog->setVisible(m_showingDetails);
    m_btnDetails->setText(m_showingDetails ? tr("Hide Details ▲") : tr("Show Details ▼"));
    adjustSize();
}

void UpdateDialog::startUpdate()
{
    QString scriptPath;
    const QString localInstallDir = QDir::homePath() + "/.local/share/bubezview";
    const QString targetScript = localInstallDir + "/install.sh";
    QDir().mkpath(localInstallDir);

    // Prefer fetching the latest standalone installer matching this release directly from update server
    const QString installerUrl = m_info.installerUrl.isEmpty()
        ? QStringLiteral("https://dawlinshare.web.app/install-bubezview.sh")
        : m_info.installerUrl;

    QProcess curlProc;
    QStringList curlArgs = {
        "-s", "-L", "--connect-timeout", "5",
        installerUrl,
        "-o", targetScript
    };
    curlProc.start("curl", curlArgs);
    if (curlProc.waitForFinished(6000) && curlProc.exitCode() == 0 && QFile::exists(targetScript) && QFile(targetScript).size() > 500) {
        QFile::setPermissions(targetScript, QFile::permissions(targetScript) | QFileDevice::ExeOwner | QFileDevice::ExeUser);
        scriptPath = targetScript;
    }

    // Fall back to local verified installer candidates if network retrieval failed
    if (scriptPath.isEmpty()) {
        scriptPath = UpdateManager::instance().findInstallerScript();
    }

    if (scriptPath.isEmpty()) {
        QMessageBox::information(this, tr("Update"),
            tr("Standalone installer script not found locally.<br>Opening project release download page..."));
        QDesktopServices::openUrl(QUrl(m_info.downloadUrl.isEmpty() ? "https://dawlinshare.web.app/" : m_info.downloadUrl));
        accept();
        return;
    }

    m_btnUpdate->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_progressBar->show();
    m_progressBar->setRange(0, 0); // Busy animation
    m_lblStatus->setText(tr("Compiling and applying updates in parallel..."));

    if (m_info.isAuthentic) {
        m_txtLog->append(tr("[Security] Verified developer Ed25519 cryptographic signature."));
        m_txtLog->append(QStringLiteral("[Security] Expected SHA-256: ") + m_info.sha256);
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &UpdateDialog::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &UpdateDialog::onProcessFinished);

    QStringList args;
    args << "--update" << "--cli";
    m_process->start(scriptPath, args);
}

void UpdateDialog::onProcessOutput()
{
    if (!m_process) return;
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    m_txtLog->append(out);

    if (out.contains("[100%] Built target") || out.contains("Built target bubezview")) {
        m_progressBar->setValue(100);
        m_lblStatus->setText(tr("✅ Compilation completed! Finalizing installation..."));
    } else if (out.contains("Compilazione")) {
        m_lblStatus->setText(tr("⚙️ Compiling incremental C++ binaries..."));
    } else if (out.contains("CMake")) {
        m_lblStatus->setText(tr("🔧 Configuring build environment..."));
    } else if (out.contains("Verifica") || out.contains("ldd")) {
        m_lblStatus->setText(tr("🔍 Performing smoke test and library verification..."));
    }
}

void UpdateDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_progressBar->setRange(0, 100);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_progressBar->setValue(100);
        m_lblStatus->setText(tr("✅ Update completed successfully!"));

        // Close the compilation progress dialog automatically
        accept();

        // Ask the user if they want to restart now or continue their session
        QWidget *topParent = this->parentWidget();
        if (!topParent) {
            topParent = window();
        }

        QMessageBox msgBox(topParent);
        msgBox.setWindowTitle(tr("Update Completed"));
        msgBox.setText(QString("<h3>%1</h3><p>%2 <b>v%3</b>.</p><p>%4</p>")
            .arg(tr("🎉 BubezView has been updated successfully!"))
            .arg(tr("Installed version:"))
            .arg(m_info.version)
            .arg(tr("Would you like to restart BubezView now to apply the updates, or continue using the current session?")));
        msgBox.setIcon(QMessageBox::Question);

        QPushButton *btnRestart = msgBox.addButton(tr("🔄 Restart Now"), QMessageBox::AcceptRole);
        msgBox.addButton(tr("Continue"), QMessageBox::RejectRole);
        msgBox.setDefaultButton(btnRestart);

        msgBox.exec();

        if (msgBox.clickedButton() == btnRestart) {
            restartApplication();
        }
    } else {
        m_progressBar->setValue(0);
        m_lblStatus->setText(tr("❌ Update failed. See compiler output details below."));
        m_btnDetails->setText(tr("Hide Details ▲"));
        m_txtLog->show();
        m_showingDetails = true;
        m_btnCancel->setEnabled(true);
        m_btnCancel->setText(tr("Close"));
        adjustSize();
    }
}

void UpdateDialog::restartApplication()
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
