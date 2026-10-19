/**
 * @file package_manager_helper.cpp
 * @brief Implementation of intelligent package manager detection and execution.
 */

#include "package_manager_helper.h"
#include "build_config.h"
#include "build_console_dialog.h"
#include "extension_feature_model.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QProgressDialog>
#include <QApplication>
#include <QRegularExpression>
#include <unistd.h>

bool PackageManagerHelper::isImmutableSystem()
{
    if (QFile::exists("/run/ostree-booted")) return true;
    if (QFile::exists("/etc/NIXOS") || QFile::exists("/nix/store")) return true;
    if (QFile::exists("/etc/system-image/channel.ini")) return true;

    QFile osRelease("/etc/os-release");
    if (osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = osRelease.readAll().toLower();
        if (content.contains("silverblue") || content.contains("kinoite") ||
            content.contains("microos") || content.contains("kalpa") ||
            content.contains("vanilla os") || content.contains("steamos") ||
            content.contains("nixos")) {
            return true;
        }
    }
    return false;
}

bool PackageManagerHelper::isGentoo()
{
    if (QFile::exists("/etc/gentoo-release")) return true;
    QFile osRelease("/etc/os-release");
    if (osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = osRelease.readAll().toLower();
        if (content.contains("gentoo") || content.contains("funtoo")) return true;
    }
    return false;
}

DistroFamily PackageManagerHelper::detectFamily()
{
    static DistroFamily s_cachedFamily = DistroFamily::Unknown;
    static bool s_familyDetected = false;
    if (s_familyDetected) {
        return s_cachedFamily;
    }

    if (isImmutableSystem()) {
        s_cachedFamily = DistroFamily::Immutable;
        s_familyDetected = true;
        return s_cachedFamily;
    }
    if (isGentoo()) {
        s_cachedFamily = DistroFamily::Gentoo;
        s_familyDetected = true;
        return s_cachedFamily;
    }

    QFile osRelease("/etc/os-release");
    if (!osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        s_cachedFamily = DistroFamily::Unknown;
        s_familyDetected = true;
        return s_cachedFamily;
    }

    QString content = osRelease.readAll().toLower();
    if (content.contains("ubuntu") || content.contains("debian") ||
        content.contains("mint") || content.contains("pop!_os") ||
        content.contains("zorin") || content.contains("raspbian") ||
        content.contains("kali")) {
        s_cachedFamily = DistroFamily::Debian;
    } else if (content.contains("arch") || content.contains("manjaro") ||
        content.contains("garuda") || content.contains("endeavouros") ||
        content.contains("cachy") || content.contains("artix")) {
        s_cachedFamily = DistroFamily::Arch;
    } else if (content.contains("fedora") || content.contains("rhel") ||
        content.contains("centos") || content.contains("rocky") ||
        content.contains("almalinux") || content.contains("nobara")) {
        s_cachedFamily = DistroFamily::Fedora;
    } else if (content.contains("opensuse") || content.contains("suse")) {
        s_cachedFamily = DistroFamily::OpenSuse;
    } else if (content.contains("void")) {
        s_cachedFamily = DistroFamily::Void;
    } else if (content.contains("solus")) {
        s_cachedFamily = DistroFamily::Solus;
    } else if (content.contains("alpine")) {
        s_cachedFamily = DistroFamily::Alpine;
    } else {
        s_cachedFamily = DistroFamily::Unknown;
    }

    s_familyDetected = true;
    return s_cachedFamily;
}

QString PackageManagerHelper::distroDisplayName()
{
    DistroFamily fam = detectFamily();
    switch (fam) {
        case DistroFamily::Debian: return "Debian / Ubuntu / Linux Mint";
        case DistroFamily::Arch: return "Arch Linux / Manjaro / Garuda";
        case DistroFamily::Fedora: return "Fedora / RHEL / Nobara";
        case DistroFamily::OpenSuse: return "openSUSE";
        case DistroFamily::Void: return "Void Linux";
        case DistroFamily::Solus: return "Solus";
        case DistroFamily::Alpine: return "Alpine Linux";
        case DistroFamily::Gentoo: return "Gentoo Linux";
        case DistroFamily::Immutable: return QObject::tr("Immutable System (OSTree / NixOS / SteamOS)");
        default: return QObject::tr("Generic Linux");
    }
}

QString PackageManagerHelper::packageNameForComponent(const QString &componentId)
{
    DistroFamily fam = detectFamily();
    QString id = componentId.toLower().trimmed();

    if (id == "engine-poppler" || id == "poppler" || id == "poppler-qt6") {
        if (fam == DistroFamily::Debian) return "libpoppler-qt6-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "poppler-qt6-devel";
        if (fam == DistroFamily::Gentoo) return "app-text/poppler[qt6]";
        return "poppler-qt6"; // Arch
    }
    if (id == "pdfimages" || id == "poppler-utils") {
        if (fam == DistroFamily::Arch) return "poppler";
        if (fam == DistroFamily::OpenSuse) return "poppler-tools";
        if (fam == DistroFamily::Gentoo) return "app-text/poppler[utils]";
        return "poppler-utils"; // Debian, Fedora, Void, Solus, Alpine
    }
    if (id == "poppler-data") {
        if (fam == DistroFamily::Gentoo) return "app-text/poppler-data";
        return "poppler-data";
    }
    if (id == "qpdf") {
        if (fam == DistroFamily::Gentoo) return "app-text/qpdf";
        return "qpdf";
    }
    if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") {
        if (fam == DistroFamily::Debian || fam == DistroFamily::OpenSuse) return "tesseract-ocr";
        if (fam == DistroFamily::Gentoo) return "app-text/tesseract";
        return "tesseract"; // Arch, Fedora, Alpine, Void
    }
    if (id == "poppler-qt6") {
        if (fam == DistroFamily::Debian) return "libpoppler-qt6-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "poppler-qt6-devel";
        if (fam == DistroFamily::Gentoo) return "app-text/poppler[qt6]";
        return "poppler-qt6"; // Arch
    }
    if (id == "libarchive" || id == "engine-libarchive" || id == "archive") {
        if (fam == DistroFamily::Debian) return "libarchive-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "libarchive-devel";
        if (fam == DistroFamily::Gentoo) return "app-arch/libarchive";
        return "libarchive"; // Arch
    }
    if (id == "librevenge") {
        if (fam == DistroFamily::Debian) return "librevenge-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "librevenge-devel";
        if (fam == DistroFamily::Gentoo) return "dev-libs/librevenge";
        return "librevenge"; // Arch
    }
    if (id == "openssl") {
        if (fam == DistroFamily::Debian) return "libssl-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "openssl-devel";
        if (fam == DistroFamily::Gentoo) return "dev-libs/openssl";
        return "openssl"; // Arch
    }
    if (id == "zstd") {
        if (fam == DistroFamily::Debian) return "libzstd-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "libzstd-devel";
        return "zstd";
    }
    if (id == "xz" || id == "liblzma") {
        if (fam == DistroFamily::Debian) return "liblzma-dev";
        if (fam == DistroFamily::Fedora || fam == DistroFamily::OpenSuse) return "xz-devel";
        return "xz";
    }
    if (id == "7zip" || id == "7z" || id == "p7zip") {
        if (fam == DistroFamily::Debian) return "7zip";
        if (fam == DistroFamily::Arch) return "7zip";
        if (fam == DistroFamily::Fedora) return "p7zip";
        if (fam == DistroFamily::OpenSuse) return "7zip";
        return "7zip";
    }
    if (id == "pigz") {
        return "pigz";
    }

    return componentId;
}

QString PackageManagerHelper::getInstallCommand(const QString &componentId)
{
    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);

    switch (fam) {
        case DistroFamily::Debian:
            return QString("sudo apt update && sudo apt install -y %1").arg(pkg);
        case DistroFamily::Arch:
            return QString("sudo pacman -S --needed %1").arg(pkg);
        case DistroFamily::Fedora:
            return QString("sudo dnf install -y %1").arg(pkg);
        case DistroFamily::OpenSuse:
            return QString("sudo zypper install -y %1").arg(pkg);
        case DistroFamily::Void:
            return QString("sudo xbps-install -y %1").arg(pkg);
        case DistroFamily::Solus:
            return QString("sudo eopkg install -y %1").arg(pkg);
        case DistroFamily::Alpine:
            return QString("sudo apk add %1").arg(pkg);
        case DistroFamily::Gentoo:
            return QString("sudo emerge --ask %1").arg(pkg);
        case DistroFamily::Immutable:
            return QString("# Immutable system. Use overlay or compile from source:\nrpm-ostree install %1  # (on Fedora Silverblue)\n# or compile directly from source in BubezView").arg(pkg);
        default:
            return QObject::tr("Install package '%1' using the system package manager.").arg(pkg);
    }
}

bool PackageManagerHelper::isCompilableEngine(const QString &componentId)
{
    QString id = componentId.trimmed().toLower();
    return (id == "poppler" || id == "poppler-qt6" || id == "engine-poppler" || id == "pdf" ||
            id == "libarchive" || id == "engine-libarchive" || id == "archive" ||
            id == "office" || id == "librevenge" || id == "engine-office" ||
            id == "media" || id == "mpv" ||
            id == "zstd" || id == "xz" || id == "7zip" || id == "7z" || id == "7zz" ||
            id == "qpdf" || id == "tesseract" || id == "ocr" || id == "tesseract-ocr");
}

bool PackageManagerHelper::triggerEngineCompilation(const QString &componentId, QWidget *parentWidget)
{
    QString id = componentId.toLower().trimmed();
    BuildConfig &cfg = BuildConfig::instance();

    if (id == "office" || id == "librevenge" || id == "engine-office") {
        cfg.setEnableInternalOffice(true);
        cfg.setStaticOffice(true);
    } else if (id == "pdf" || id == "poppler" || id == "poppler-qt6" || id == "engine-poppler") {
        cfg.setStaticPdf(true);
    } else if (id == "archive" || id == "libarchive" || id == "zstd" || id == "xz" || id == "engine-libarchive") {
        cfg.setStaticArchive(true);
    }
    cfg.save();

    BuildConsoleDialog *console = new BuildConsoleDialog(parentWidget);
    console->setAttribute(Qt::WA_DeleteOnClose);
    console->show();
    console->raise();
    console->activateWindow();
    return console->startBuildForComponent(id);
}

bool PackageManagerHelper::installComponentInteractive(const QString &componentId, QWidget *parentWidget)
{
    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);
    bool compilable = isCompilableEngine(componentId);
    ExtensionModuleInfo mod = ExtensionFeatureRegistry::moduleById(componentId);
    QString displayName = !mod.displayName.isEmpty() ? mod.displayName : componentId;

    if (fam == DistroFamily::Immutable) {
        if (compilable) {
            QMessageBox msg(parentWidget);
            msg.setWindowTitle(QObject::tr("Immutable System Detected"));
            msg.setText(QObject::tr("<h3>Immutable System Detected</h3>"
                                   "<p>Your operating system has a read-only root filesystem.</p>"
                                   "<p>Would you like to compile the <b>%1</b> engine from source in your user directory?</p>").arg(displayName));
            QAbstractButton *btnCompile = msg.addButton(QObject::tr("Compile from Source (Recommended)"), QMessageBox::ActionRole);
            QAbstractButton *btnCancel = msg.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
            Q_UNUSED(btnCancel);
            msg.exec();
            if (msg.clickedButton() == btnCompile) {
                return triggerEngineCompilation(componentId, parentWidget);
            }
            return false;
        }

        QMessageBox::information(
            parentWidget,
            QObject::tr("Immutable System Detected"),
            QObject::tr("<p>Your operating system has a read-only root filesystem.</p>"
                       "<p>Installing runtime packages into <code>/usr</code> is not recommended.</p>"
                       "<p><b>Recommended solution:</b> In BubezView Settings, select "
                       "<b>'Compiled (Source)'</b> to build the module locally in your user folder.</p>"
                       "<p>Alternatively, apply an overlay via terminal:<br>"
                       "<code>rpm-ostree install %1</code></p>").arg(pkg)
        );
        return false;
    }

    if (fam == DistroFamily::Gentoo) {
        QMessageBox::information(
            parentWidget,
            QObject::tr("Gentoo Distribution"),
            QObject::tr("<p>Detected Gentoo Linux environment.</p>"
                       "<p>Run from terminal:</p>"
                       "<p><code>sudo emerge --ask %1</code></p>"
                       "<p>Verify that required USE flags are enabled in <code>/etc/portage/package.use</code>.</p>").arg(pkg)
        );
        return false;
    }

    if (compilable) {
        QMessageBox msg(parentWidget);
        msg.setWindowTitle(QObject::tr("Component Activation"));
        msg.setText(QObject::tr("<h3>Activate Engine: %1</h3>"
                               "<p>This engine requires the system package: <code>%2</code></p>"
                               "<p>Detected distribution: <b>%3</b></p>"
                               "<p>How would you like to proceed?</p>").arg(displayName, pkg, distroDisplayName()));
        QAbstractButton *btnSys = msg.addButton(QObject::tr("Install from System (Recommended)"), QMessageBox::ActionRole);
        QAbstractButton *btnCompile = msg.addButton(QObject::tr("Compile from Source"), QMessageBox::ActionRole);
        QAbstractButton *btnCancel = msg.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
        Q_UNUSED(btnCancel);
        msg.exec();

        if (msg.clickedButton() == btnCompile) {
            return triggerEngineCompilation(componentId, parentWidget);
        } else if (msg.clickedButton() != btnSys) {
            return false;
        }
    } else {
        QMessageBox::StandardButton reply = QMessageBox::question(
            parentWidget,
            QObject::tr("System Package Installation"),
            QObject::tr("<p>The component <b>%1</b> requires the system package: <code>%2</code></p>"
                       "<p>Detected distribution: <b>%3</b></p>"
                       "<p>Would you like to proceed with automated installation using administrator privileges?</p>")
                       .arg(displayName, pkg, distroDisplayName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (reply != QMessageBox::Yes) {
            return false;
        }
    }

    QString installError;
    bool success = runPrivilegedInstall(QStringList() << pkg, parentWidget, &installError);
    if (success) {
        if (componentId == "office" || componentId == "librevenge") {
            BuildConfig &cfg = BuildConfig::instance();
            cfg.setEnableInternalOffice(true);
            cfg.setStaticOffice(false);
            cfg.save();
        }
        return true;
    }

    // Installation failed! If it is a compilable engine, prompt the backup plan
    if (compilable) {
        QMessageBox failMsg(parentWidget);
        failMsg.setWindowTitle(QObject::tr("Installation Failed — Backup Plan"));
        failMsg.setText(QObject::tr("<h3>System installation failed</h3>"
                                   "<p>Could not install package <code>%1</code> via the system package manager.</p>"
                                   "<p><b>Error details:</b> %2</p>"
                                   "<p><b>Backup plan:</b> Would you like to compile the engine from source?</p>")
                                   .arg(pkg, installError.isEmpty() ? QObject::tr("Unknown error") : installError.trimmed().toHtmlEscaped()));
        QAbstractButton *btnCompileFallback = failMsg.addButton(QObject::tr("Compile from Source"), QMessageBox::ActionRole);
        QAbstractButton *btnCancelFallback = failMsg.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
        Q_UNUSED(btnCancelFallback);
        failMsg.exec();

        if (failMsg.clickedButton() == btnCompileFallback) {
            return triggerEngineCompilation(componentId, parentWidget);
        }
    }

    return false;
}

bool PackageManagerHelper::runPrivilegedInstall(const QStringList &packageNames, QWidget *parentWidget, QString *outError)
{
    if (packageNames.isEmpty()) return true;

    DistroFamily fam = detectFamily();
    QString pkgs = packageNames.join(" ");

    QString cmd;
    if (fam == DistroFamily::Debian) {
        cmd = QString("apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Arch) {
        cmd = QString("pacman -S --noconfirm --needed %1").arg(pkgs);
    } else if (fam == DistroFamily::Fedora) {
        cmd = QString("dnf install -y %1").arg(pkgs);
    } else if (fam == DistroFamily::OpenSuse) {
        cmd = QString("zypper --non-interactive in %1").arg(pkgs);
    } else if (fam == DistroFamily::Void) {
        cmd = QString("xbps-install -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Solus) {
        cmd = QString("eopkg install -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Alpine) {
        cmd = QString("apk add %1").arg(pkgs);
    } else {
        QMessageBox::warning(parentWidget, QObject::tr("Unsupported package manager"),
                             QObject::tr("Please install packages manually:\n%1").arg(pkgs));
        return false;
    }

    QProgressDialog progress(
        QObject::tr("Installing %1...\nPlease authorize the operation in the system prompt if requested.").arg(pkgs),
        QObject::tr("Cancel"),
        0, 0,
        parentWidget
    );
    progress.setWindowTitle(QObject::tr("Package Installation"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Ensure SHELL is an absolute path from /etc/shells so pkexec does not reject it
    if (!env.value("SHELL").startsWith("/")) {
        env.insert("SHELL", "/bin/bash");
    }
    proc.setProcessEnvironment(env);

    QStringList args;

    // Check if sudo is available passwordless (e.g. in containers or distrobox)
    bool hasPasswordlessSudo = (!QStandardPaths::findExecutable("sudo").isEmpty() &&
                                QProcess::execute("sudo", QStringList() << "-n" << "true") == 0);

    if (geteuid() == 0) {
        args << "-c" << cmd;
        proc.start("bash", args);
    } else if (hasPasswordlessSudo) {
        args << "bash" << "-c" << cmd;
        proc.start("sudo", args);
    } else if (!QStandardPaths::findExecutable("pkexec").isEmpty()) {
        args << "bash" << "-c" << cmd;
        proc.start("pkexec", args);
    } else if (!QStandardPaths::findExecutable("sudo").isEmpty()) {
        args << "bash" << "-c" << cmd;
        proc.start("sudo", args);
    } else {
        args << "-c" << cmd;
        proc.start("bash", args);
    }

    while (proc.state() != QProcess::NotRunning) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished(1000);
            return false;
        }
        proc.waitForFinished(200);
    }

    progress.close();

    if (proc.exitCode() == 0) {
        clearCaches();
        QMessageBox::information(
            parentWidget,
            QObject::tr("Installation Completed"),
            QObject::tr("The packages (%1) were installed successfully!").arg(pkgs)
        );
        return true;
    } else {
        QString errorOut = QString::fromUtf8(proc.readAllStandardError());
        if (errorOut.trimmed().isEmpty()) {
            errorOut = QString::fromUtf8(proc.readAllStandardOutput());
        }
        if (outError) {
            *outError = errorOut.trimmed();
        } else {
            QMessageBox::warning(
                parentWidget,
                QObject::tr("Installation Failed"),
                QObject::tr("<p>Could not complete automatic installation.</p>"
                           "<p>Manual terminal command:</p>"
                           "<p><code>%1</code></p>"
                           "<p>Error detail: %2</p>")
                           .arg(getInstallCommand(packageNames.first()), errorOut.trimmed().toHtmlEscaped())
            );
        }
        return false;
    }
}

bool PackageManagerHelper::isComponentInstalled(const QString &componentId)
{
    return BuildConfig::isComponentAvailable(componentId);
}

QString PackageManagerHelper::getUninstallCommand(const QString &componentId)
{
    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);

    switch (fam) {
        case DistroFamily::Debian:
            return QString("sudo apt remove %1").arg(pkg);
        case DistroFamily::Arch:
            return QString("sudo pacman -R %1").arg(pkg);
        case DistroFamily::Fedora:
            return QString("sudo dnf remove -y %1").arg(pkg);
        case DistroFamily::OpenSuse:
            return QString("sudo zypper remove -y %1").arg(pkg);
        case DistroFamily::Void:
            return QString("sudo xbps-remove -y %1").arg(pkg);
        case DistroFamily::Solus:
            return QString("sudo eopkg remove -y %1").arg(pkg);
        case DistroFamily::Alpine:
            return QString("sudo apk del %1").arg(pkg);
        case DistroFamily::Gentoo:
            return QString("sudo emerge --unmerge %1").arg(pkg);
        case DistroFamily::Immutable:
            return QString("rpm-ostree uninstall %1").arg(pkg);
        default:
            return QObject::tr("Uninstall package '%1' using the system package manager.").arg(pkg);
    }
}

bool PackageManagerHelper::checkUninstallSafety(const QString &componentId, QStringList &outOtherAffectedPackages)
{
    outOtherAffectedPackages.clear();
    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);

    if (fam == DistroFamily::Debian) {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LANG", "C");
        env.insert("LC_ALL", "C");
        proc.setProcessEnvironment(env);
        proc.start("apt-get", QStringList() << "remove" << "-s" << pkg);
        if (proc.waitForFinished(10000)) {
            QString output = QString::fromUtf8(proc.readAllStandardOutput());
            if (proc.exitCode() != 0) {
                // Package manager rejected removal because it breaks essential/critical system packages
                outOtherAffectedPackages.append(QObject::tr("system-core-dependencies"));
                return false;
            }
            const QStringList lines = output.split('\n');
            for (const QString &line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith("Remv ")) {
                    QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
                    if (parts.size() >= 2) {
                        QString remvPkg = parts[1];
                        if (remvPkg != pkg && !remvPkg.startsWith(pkg + ":")) {
                            outOtherAffectedPackages.append(remvPkg);
                        }
                    }
                }
            }
        } else {
            outOtherAffectedPackages.append(QObject::tr("query-timeout"));
            return false;
        }
    } else if (fam == DistroFamily::Arch) {
        // Query if any installed package lists this package in 'Required By'
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LANG", "C");
        env.insert("LC_ALL", "C");
        proc.setProcessEnvironment(env);
        proc.start("pacman", QStringList() << "-Qi" << pkg);
        if (proc.waitForFinished(5000)) {
            QString output = QString::fromUtf8(proc.readAllStandardOutput());
            const QStringList lines = output.split('\n');
            for (const QString &line : lines) {
                if (line.trimmed().startsWith("Required By")) {
                    QString val = line.section(':', 1).trimmed();
                    if (!val.isEmpty() && val != "None") {
                        QStringList reqs = val.split(' ', Qt::SkipEmptyParts);
                        outOtherAffectedPackages.append(reqs);
                    }
                }
            }
        }
    } else if (fam == DistroFamily::Fedora) {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LANG", "C");
        env.insert("LC_ALL", "C");
        proc.setProcessEnvironment(env);
        proc.start("dnf", QStringList() << "remove" << "--assumeno" << pkg);
        if (proc.waitForFinished(10000)) {
            QString output = QString::fromUtf8(proc.readAllStandardOutput());
            bool inDependentSection = false;
            const QStringList lines = output.split('\n');
            for (const QString &line : lines) {
                if (line.contains("Removing dependent packages:", Qt::CaseInsensitive)) {
                    inDependentSection = true;
                    continue;
                }
                if (inDependentSection) {
                    if (line.trimmed().isEmpty() || line.startsWith("Transaction Summary")) {
                        inDependentSection = false;
                    } else {
                        QString pName = line.trimmed().section(' ', 0, 0);
                        if (!pName.isEmpty() && pName != pkg) {
                            outOtherAffectedPackages.append(pName);
                        }
                    }
                }
            }
        }
    }

    return outOtherAffectedPackages.isEmpty();
}

bool PackageManagerHelper::runPrivilegedUninstall(const QStringList &packageNames, QWidget *parentWidget)
{
    if (packageNames.isEmpty()) return true;

    DistroFamily fam = detectFamily();
    QString pkgs = packageNames.join(" ");

    QString cmd;
    if (fam == DistroFamily::Debian) {
        cmd = QString("DEBIAN_FRONTEND=noninteractive apt-get remove -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Arch) {
        // -R without -c ensures pacman refuses to remove reverse dependencies
        cmd = QString("pacman -R --noconfirm %1").arg(pkgs);
    } else if (fam == DistroFamily::Fedora) {
        cmd = QString("dnf remove -y %1").arg(pkgs);
    } else if (fam == DistroFamily::OpenSuse) {
        cmd = QString("zypper --non-interactive rm %1").arg(pkgs);
    } else if (fam == DistroFamily::Void) {
        cmd = QString("xbps-remove -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Solus) {
        cmd = QString("eopkg remove -y %1").arg(pkgs);
    } else if (fam == DistroFamily::Alpine) {
        cmd = QString("apk del %1").arg(pkgs);
    } else {
        QMessageBox::warning(parentWidget, QObject::tr("Unsupported package manager"),
                             QObject::tr("Please uninstall packages manually:\n%1").arg(pkgs));
        return false;
    }

    QProgressDialog progress(
        QObject::tr("Uninstalling %1...\nPlease authorize the operation in the system prompt if requested.").arg(pkgs),
        QObject::tr("Cancel"),
        0, 0,
        parentWidget
    );
    progress.setWindowTitle(QObject::tr("Package Uninstallation"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.value("SHELL").startsWith("/")) {
        env.insert("SHELL", "/bin/bash");
    }
    proc.setProcessEnvironment(env);

    QStringList args;
    bool hasPasswordlessSudo = (!QStandardPaths::findExecutable("sudo").isEmpty() &&
                                QProcess::execute("sudo", QStringList() << "-n" << "true") == 0);

    if (geteuid() == 0) {
        args << "-c" << cmd;
        proc.start("bash", args);
    } else if (hasPasswordlessSudo) {
        args << "bash" << "-c" << cmd;
        proc.start("sudo", args);
    } else if (!QStandardPaths::findExecutable("pkexec").isEmpty()) {
        args << "bash" << "-c" << cmd;
        proc.start("pkexec", args);
    } else if (!QStandardPaths::findExecutable("sudo").isEmpty()) {
        args << "bash" << "-c" << cmd;
        proc.start("sudo", args);
    } else {
        args << "-c" << cmd;
        proc.start("bash", args);
    }

    while (proc.state() != QProcess::NotRunning) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished(1000);
            return false;
        }
        proc.waitForFinished(200);
    }

    progress.close();

    if (proc.exitCode() == 0) {
        clearCaches();
        QMessageBox::information(
            parentWidget,
            QObject::tr("Uninstallation Completed"),
            QObject::tr("The packages (%1) were removed successfully!").arg(pkgs)
        );
        return true;
    } else {
        QString errorOut = QString::fromUtf8(proc.readAllStandardError());
        if (errorOut.trimmed().isEmpty()) {
            errorOut = QString::fromUtf8(proc.readAllStandardOutput());
        }
        QMessageBox::warning(
            parentWidget,
            QObject::tr("Uninstallation Failed"),
            QObject::tr("<p>Could not complete automatic removal.</p>"
                       "<p>Manual terminal command:</p>"
                       "<p><code>%1</code></p>"
                       "<p>Error details: %2</p>")
                       .arg(getUninstallCommand(packageNames.first()), errorOut.trimmed())
        );
        return false;
    }
}

bool PackageManagerHelper::uninstallComponentInteractive(const QString &componentId, QWidget *parentWidget)
{
    QString pkg = packageNameForComponent(componentId);
    QStringList otherDeps;

    bool isSafe = checkUninstallSafety(componentId, otherDeps);
    if (!isSafe && !otherDeps.isEmpty()) {
        QString listStr = otherDeps.mid(0, 8).join(", ");
        if (otherDeps.size() > 8) listStr += QString(" (+%1 more)").arg(otherDeps.size() - 8);

        QMessageBox::critical(
            parentWidget,
            QObject::tr("Uninstallation Blocked for Safety"),
            QObject::tr("<p>Cannot safely uninstall package <b>%1</b>.</p>"
                       "<p>The following system packages or applications depend on it:</p>"
                       "<p style='color: #dc2626;'><b>%2</b></p>"
                       "<p>Removal was blocked to prevent breaking system stability or removing other installed applications.</p>")
                       .arg(pkg, listStr)
        );
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        parentWidget,
        QObject::tr("Confirm Uninstallation"),
        QObject::tr("<p>Are you sure you want to uninstall package <b>%1</b> from your system?</p>"
                   "<p><i>Safety check passed: no other packages depend on this component.</i></p>")
                   .arg(pkg),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return false;
    }

    return runPrivilegedUninstall(QStringList() << pkg, parentWidget);
}

static QMap<QString, QString> s_candidateVersionCache;

bool PackageManagerHelper::isCandidateVersionCached(const QString &componentId)
{
    return s_candidateVersionCache.contains(componentId.trimmed().toLower());
}

void PackageManagerHelper::clearCaches()
{
    s_candidateVersionCache.clear();
    BuildConfig::clearPackageCache();
}

QString PackageManagerHelper::queryPackageCandidateVersion(const QString &componentId)
{
    const QString compKey = componentId.trimmed().toLower();
    if (s_candidateVersionCache.contains(compKey)) {
        return s_candidateVersionCache.value(compKey);
    }

    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);

    QString program;
    QStringList args;

    switch (fam) {
        case DistroFamily::Debian:
            program = "apt-cache";
            args << "policy" << pkg;
            break;
        case DistroFamily::Arch:
            program = "pacman";
            args << "-Qi" << pkg;
            break;
        case DistroFamily::Fedora:
            program = "dnf";
            args << "info" << pkg;
            break;
        case DistroFamily::OpenSuse:
            program = "zypper";
            args << "info" << pkg;
            break;
        case DistroFamily::Alpine:
            program = "apk";
            args << "info" << pkg;
            break;
        case DistroFamily::Void:
            program = "xbps-query";
            args << "-R" << pkg;
            break;
        default:
            break;
    }

    if (program.isEmpty() || QStandardPaths::findExecutable(program).isEmpty()) {
        s_candidateVersionCache.insert(compKey, QString());
        return QString();
    }

    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        s_candidateVersionCache.insert(compKey, QString());
        return QString();
    }

    if (fam == DistroFamily::Arch && proc.exitCode() != 0) {
        // Fallback to sync repository if package is not yet locally installed
        proc.start("pacman", QStringList() << "-Si" << pkg);
        if (!proc.waitForFinished(2000)) {
            proc.kill();
            s_candidateVersionCache.insert(compKey, QString());
            return QString();
        }
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    if (output.isEmpty()) {
        s_candidateVersionCache.insert(compKey, QString());
        return QString();
    }

    QString foundVersion;
    switch (fam) {
        case DistroFamily::Debian: {
            QRegularExpression instRegex(QStringLiteral(R"((?:Installed|Installato):\s*(\S+))"));
            QRegularExpression candRegex(QStringLiteral(R"((?:Candidate|Candidato):\s*(\S+))"));
            auto instMatch = instRegex.match(output);
            auto candMatch = candRegex.match(output);
            if (instMatch.hasMatch() && candMatch.hasMatch()) {
                QString instVer = instMatch.captured(1).trimmed();
                QString candVer = candMatch.captured(1).trimmed();
                if (instVer != candVer && candVer != "(none)" && candVer != "(nessuno)") {
                    QRegularExpression numRegex(QStringLiteral(R"((\d+\.\d+(?:\.\d+)?))"));
                    auto numMatch = numRegex.match(candVer);
                    if (numMatch.hasMatch()) {
                        QString ver = numMatch.captured(1);
                        if (ver.count('.') == 1) ver += ".0";
                        foundVersion = ver;
                    } else {
                        foundVersion = candVer;
                    }
                }
            }
            break;
        }
        case DistroFamily::Arch:
        case DistroFamily::Fedora:
        case DistroFamily::OpenSuse: {
            QRegularExpression verRegex(QStringLiteral(R"(Version\s*:\s*(\d+\.\d+(?:\.\d+)?))"));
            auto match = verRegex.match(output);
            if (match.hasMatch()) {
                QString ver = match.captured(1);
                if (ver.count('.') == 1) ver += ".0";
                foundVersion = ver;
            }
            break;
        }
        case DistroFamily::Alpine:
        case DistroFamily::Void:
        default: {
            QRegularExpression verRegex(QStringLiteral(R"((\d+\.\d+(?:\.\d+)?))"));
            auto match = verRegex.match(output);
            if (match.hasMatch()) {
                QString ver = match.captured(1);
                if (ver.count('.') == 1) ver += ".0";
                foundVersion = ver;
            }
            break;
        }
    }

    s_candidateVersionCache.insert(compKey, foundVersion);
    return foundVersion;
}

QStringList PackageManagerHelper::knownStableVersions(const QString &componentId)
{
    QString c = componentId.toLower();
    if (c.contains("pdf") || c.contains("poppler")) {
        return QStringList() << "24.08.0" << "24.02.0" << "23.12.0" << "23.08.0" << "22.12.0";
    }
    if (c.contains("archive") || c.contains("tar")) {
        return QStringList() << "3.8.9" << "3.7.4" << "3.7.2" << "3.6.2";
    }
    return QStringList();
}
