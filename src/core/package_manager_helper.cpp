/**
 * @file package_manager_helper.cpp
 * @brief Implementation of intelligent package manager detection and execution.
 */

#include "package_manager_helper.h"
#include "build_config.h"
#include "build_console_dialog.h"
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
    if (isImmutableSystem()) return DistroFamily::Immutable;
    if (isGentoo()) return DistroFamily::Gentoo;

    QFile osRelease("/etc/os-release");
    if (!osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return DistroFamily::Unknown;
    }

    QString content = osRelease.readAll().toLower();
    if (content.contains("ubuntu") || content.contains("debian") ||
        content.contains("mint") || content.contains("pop!_os") ||
        content.contains("zorin") || content.contains("raspbian") ||
        content.contains("kali")) {
        return DistroFamily::Debian;
    }
    if (content.contains("arch") || content.contains("manjaro") ||
        content.contains("garuda") || content.contains("endeavouros") ||
        content.contains("cachy") || content.contains("artix")) {
        return DistroFamily::Arch;
    }
    if (content.contains("fedora") || content.contains("rhel") ||
        content.contains("centos") || content.contains("rocky") ||
        content.contains("almalinux") || content.contains("nobara")) {
        return DistroFamily::Fedora;
    }
    if (content.contains("opensuse") || content.contains("suse")) {
        return DistroFamily::OpenSuse;
    }
    if (content.contains("void")) return DistroFamily::Void;
    if (content.contains("solus")) return DistroFamily::Solus;
    if (content.contains("alpine")) return DistroFamily::Alpine;

    return DistroFamily::Unknown;
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
        case DistroFamily::Immutable: return "Sistema Immutabile (OSTree / NixOS / SteamOS)";
        default: return "Linux Generico";
    }
}

QString PackageManagerHelper::packageNameForComponent(const QString &componentId)
{
    DistroFamily fam = detectFamily();
    QString id = componentId.toLower().trimmed();

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
    if (id == "libarchive") {
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
            return QString("# Sistema immutabile. Usa overlay o compila dai sorgenti:\nrpm-ostree install %1  # (su Fedora Silverblue)\n# oppure compila direttamente da sorgente in BubezView").arg(pkg);
        default:
            return QString("Installa il pacchetto '%1' con il gestore pacchetti di sistema.").arg(pkg);
    }
}

bool PackageManagerHelper::isCompilableEngine(const QString &componentId)
{
    QString id = componentId.toLower().trimmed();
    return (id == "office" || id == "librevenge" ||
            id == "pdf" || id == "poppler" || id == "poppler-qt6" ||
            id == "archive" || id == "libarchive");
}

bool PackageManagerHelper::triggerEngineCompilation(const QString &componentId, QWidget *parentWidget)
{
    QString id = componentId.toLower().trimmed();
    BuildConfig &cfg = BuildConfig::instance();

    if (id == "office" || id == "librevenge") {
        cfg.setEnableInternalOffice(true);
        cfg.setStaticOffice(true);
    } else if (id == "pdf" || id == "poppler" || id == "poppler-qt6" || id == "pdfimages") {
        cfg.setStaticPdf(true);
    } else if (id == "archive" || id == "libarchive" || id == "zstd" || id == "xz") {
        cfg.setStaticArchive(true);
    }
    cfg.save();

    BuildConsoleDialog *console = new BuildConsoleDialog(parentWidget);
    console->setAttribute(Qt::WA_DeleteOnClose);
    console->show();
    console->raise();
    console->activateWindow();
    return console->startBuild();
}

bool PackageManagerHelper::installComponentInteractive(const QString &componentId, QWidget *parentWidget)
{
    DistroFamily fam = detectFamily();
    QString pkg = packageNameForComponent(componentId);
    bool compilable = isCompilableEngine(componentId);

    if (fam == DistroFamily::Immutable) {
        if (compilable) {
            QMessageBox msg(parentWidget);
            msg.setWindowTitle(QObject::tr("Immutable System Detected"));
            msg.setText(QObject::tr("<h3>Immutable System Detected</h3>"
                                   "<p>Your operating system has a read-only root filesystem.</p>"
                                   "<p>Would you like to compile the <b>%1</b> engine from source in your user directory?</p>").arg(componentId));
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
                               "<p>How would you like to proceed?</p>").arg(componentId, pkg, distroDisplayName()));
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
                       .arg(componentId, pkg, distroDisplayName()),
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
        QMessageBox::warning(parentWidget, QObject::tr("Gestore pacchetti non supportato"),
                             QObject::tr("Installa manualmente i pacchetti:\n%1").arg(pkgs));
        return false;
    }

    QProgressDialog progress(
        QObject::tr("Installazione di %1 in corso...\nAutorizza l'operazione nella finestra di sistema se richiesta.").arg(pkgs),
        QObject::tr("Annulla"),
        0, 0,
        parentWidget
    );
    progress.setWindowTitle(QObject::tr("Installazione Pacchetti"));
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
    QString id = componentId.toLower().trimmed();
    if (id == "pdfimages" || id == "poppler-utils") return BuildConfig::isPdfimagesAvailable();
    if (id == "poppler-data") return BuildConfig::isPopplerDataInstalled();
    if (id == "qpdf") return BuildConfig::isQpdfAvailable();
    if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") return BuildConfig::isTesseractAvailable();
    if (id == "openssl") return BuildConfig::isPkgInstalled("openssl") || BuildConfig::isExecutableAvailable("openssl");
    if (id == "zstd") return BuildConfig::isPkgInstalled("libzstd") || BuildConfig::isExecutableAvailable("zstd");
    if (id == "xz" || id == "liblzma") return BuildConfig::isPkgInstalled("liblzma") || BuildConfig::isExecutableAvailable("xz");
    return false;
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
            return QString("Disinstalla il pacchetto '%1' con il gestore pacchetti di sistema.").arg(pkg);
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
        QObject::tr("Disinstallazione di %1 in corso...\nAutorizza l'operazione nella finestra di sistema se richiesta.").arg(pkgs),
        QObject::tr("Annulla"),
        0, 0,
        parentWidget
    );
    progress.setWindowTitle(QObject::tr("Disinstallazione Pacchetti"));
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
        QMessageBox::information(
            parentWidget,
            QObject::tr("Disinstallazione Completata"),
            QObject::tr("I pacchetti (%1) sono stati rimossi con successo!").arg(pkgs)
        );
        return true;
    } else {
        QString errorOut = QString::fromUtf8(proc.readAllStandardError());
        if (errorOut.trimmed().isEmpty()) {
            errorOut = QString::fromUtf8(proc.readAllStandardOutput());
        }
        QMessageBox::warning(
            parentWidget,
            QObject::tr("Disinstallazione Fallita"),
            QObject::tr("<p>Impossibile completare la rimozione automatica.</p>"
                       "<p>Comando manuale da terminale:</p>"
                       "<p><code>%1</code></p>"
                       "<p>Dettaglio errore: %2</p>")
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
        if (otherDeps.size() > 8) listStr += QString(" (+%1 altri)").arg(otherDeps.size() - 8);

        QMessageBox::critical(
            parentWidget,
            QObject::tr("Disinstallazione Bloccata per Sicurezza"),
            QObject::tr("<p>Non è possibile disinstallare in modo sicuro il pacchetto <b>%1</b>.</p>"
                       "<p>I seguenti programmi o componenti di sistema dipendono da esso:</p>"
                       "<p style='color: #dc2626;'><b>%2</b></p>"
                       "<p>La rimozione è stata bloccata per evitare instabilità o la cancellazione accidentale di altre applicazioni di sistema.</p>")
                       .arg(pkg, listStr)
        );
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        parentWidget,
        QObject::tr("Conferma Disinstallazione"),
        QObject::tr("<p>Sei sicuro di voler disinstallare il pacchetto <b>%1</b> dal sistema?</p>"
                   "<p><i>Controllo di sicurezza completato: nessun altro programma dipende da questo componente.</i></p>")
                   .arg(pkg),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return false;
    }

    return runPrivilegedUninstall(QStringList() << pkg, parentWidget);
}

QString PackageManagerHelper::queryPackageCandidateVersion(const QString &componentId)
{
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
            if (QProcess::execute("pacman", QStringList() << "-Qi" << pkg) == 0) {
                args << "-Qi" << pkg;
            } else {
                args << "-Si" << pkg;
            }
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
        return QString();
    }

    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        return QString();
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    if (output.isEmpty()) return QString();

    QRegularExpression verRegex;
    switch (fam) {
        case DistroFamily::Debian:
            verRegex = QRegularExpression(QStringLiteral(R"((?:Candidate|Installed|Candidato):\s*(\d+\.\d+(?:\.\d+)?))"));
            break;
        case DistroFamily::Arch:
        case DistroFamily::Fedora:
        case DistroFamily::OpenSuse:
            verRegex = QRegularExpression(QStringLiteral(R"(Version\s*:\s*(\d+\.\d+(?:\.\d+)?))"));
            break;
        case DistroFamily::Alpine:
            verRegex = QRegularExpression(QStringLiteral(R"((\d+\.\d+(?:\.\d+)?))"));
            break;
        case DistroFamily::Void:
            verRegex = QRegularExpression(QStringLiteral(R"(pkgver:\s*.*?(\d+\.\d+(?:\.\d+)?))"));
            break;
        default:
            verRegex = QRegularExpression(QStringLiteral(R"((\d+\.\d+(?:\.\d+)?))"));
            break;
    }

    QRegularExpressionMatch match = verRegex.match(output);
    if (match.hasMatch()) {
        QString ver = match.captured(1);
        if (ver.count('.') == 1) {
            ver += ".0";
        }
        return ver;
    }

    return QString();
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
