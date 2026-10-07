#include "share_manager.h"
#include "share_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QCheckBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QDesktopServices>
#include <QIcon>
#include <QAction>

ShareManager::ShareManager(QWidget *parentWidget, std::function<QString()> getFilePathCallback)
    : QObject(parentWidget)
    , m_parent(parentWidget)
    , m_getFilePath(std::move(getFilePathCallback))
{
}

bool ShareManager::isAvailable(const QStringList &commandsOrFlatpaks)
{
    static QMap<QString, bool> s_cache;
    QString flatpakBin;
    bool checkedFlatpak = false;

    for (const QString &cmd : commandsOrFlatpaks) {
        if (s_cache.contains(cmd)) {
            if (s_cache[cmd]) return true;
            continue;
        }

        if (!QStandardPaths::findExecutable(cmd).isEmpty()) {
            s_cache[cmd] = true;
            return true;
        }
        if (cmd.contains('.')) {
            if (!checkedFlatpak) {
                flatpakBin = QStandardPaths::findExecutable("flatpak");
                checkedFlatpak = true;
            }
            if (!flatpakBin.isEmpty()) {
                QProcess p;
                p.start(flatpakBin, QStringList() << "info" << cmd);
                if (p.waitForFinished(1000) && p.exitCode() == 0) {
                    s_cache[cmd] = true;
                    return true;
                }
            }
        }
        s_cache[cmd] = false;
    }
    return false;
}

QStringList ShareManager::getExecutable(const QStringList &commandsOrFlatpaks)
{
    static QMap<QString, QStringList> s_cmdCache;
    QString flatpakBin;
    bool checkedFlatpak = false;

    for (const QString &cmd : commandsOrFlatpaks) {
        if (s_cmdCache.contains(cmd)) {
            const QStringList &cached = s_cmdCache[cmd];
            if (!cached.isEmpty()) return cached;
            continue;
        }

        QString exe = QStandardPaths::findExecutable(cmd);
        if (!exe.isEmpty()) {
            QStringList res = QStringList() << exe;
            s_cmdCache[cmd] = res;
            return res;
        }
        if (cmd.contains('.')) {
            if (!checkedFlatpak) {
                flatpakBin = QStandardPaths::findExecutable("flatpak");
                checkedFlatpak = true;
            }
            if (!flatpakBin.isEmpty()) {
                QProcess p;
                p.start(flatpakBin, QStringList() << "info" << cmd);
                if (p.waitForFinished(1000) && p.exitCode() == 0) {
                    QStringList res = QStringList() << flatpakBin << "run" << cmd;
                    s_cmdCache[cmd] = res;
                    return res;
                }
            }
        }
        s_cmdCache[cmd] = QStringList();
    }
    return QStringList();
}

void ShareManager::populateMenu(QMenu *menu)
{
    if (!menu) return;

    menu->clear();
    QStyle *st = QApplication::style();

    // 1. KDE Connect (if installed)
    if (isAvailable({"kdeconnect-cli", "org.kde.kdeconnect"})) {
        QIcon icon = QIcon::fromTheme("kdeconnect", st->standardIcon(QStyle::SP_DriveNetIcon));
        QAction *act = menu->addAction(icon, tr("Send to device (KDE Connect)..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareKdeConnect(path, m_parent);
        });
    }

    // 2. LocalSend (if installed)
    if (isAvailable({"localsend_app", "org.localsend.localsend_app", "localsend"})) {
        QIcon icon = QIcon::fromTheme("localsend", QIcon::fromTheme("network-workgroup", st->standardIcon(QStyle::SP_DriveNetIcon)));
        QAction *act = menu->addAction(icon, tr("Send with LocalSend..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareLocalSend(path, m_parent);
        });
    }

    // 3. Bluetooth (if available)
    if (isAvailable({"bluetooth-sendto", "blueman-sendto"})) {
        QIcon icon = QIcon::fromTheme("preferences-system-bluetooth", st->standardIcon(QStyle::SP_DriveNetIcon));
        QAction *act = menu->addAction(icon, tr("Send via Bluetooth..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareBluetooth(path, m_parent);
        });
    }

    // 4. Telegram Desktop (if available)
    if (isAvailable({"telegram-desktop", "org.telegram.desktop"})) {
        QIcon icon = QIcon::fromTheme("telegram", st->standardIcon(QStyle::SP_ArrowUp));
        QAction *act = menu->addAction(icon, tr("Send with Telegram Desktop..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareTelegram(path, m_parent);
        });
    }

    // 5. Email (always available)
    {
        QIcon icon = QIcon::fromTheme("mail-message-new", st->standardIcon(QStyle::SP_FileDialogDetailedView));
        QAction *act = menu->addAction(icon, tr("Send via Email..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareEmail(path, m_parent);
        });
    }

    // 6. QR Code for smartphones (always available with built-in HTTP server & libqrencode)
    {
        QIcon icon = QIcon::fromTheme("view-barcode-qr", st->standardIcon(QStyle::SP_MessageBoxInformation));
        QAction *act = menu->addAction(icon, tr("Generate QR Code for smartphone..."));
        connect(act, &QAction::triggered, this, [this]() {
            QString path = m_getFilePath ? m_getFilePath() : QString();
            shareQrCode(path, m_parent);
        });
    }
}

QMenu* ShareManager::createShareMenu(QWidget *parent)
{
    QWidget *targetParent = parent ? parent : m_parent;
    QMenu *menu = new QMenu(tr("Share"), targetParent);
    menu->setIcon(QIcon::fromTheme("document-share"));
    populateMenu(menu);
    return menu;
}

void ShareManager::shareKdeConnect(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QStringList cmd = getExecutable({"kdeconnect-cli", "org.kde.kdeconnect"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append("--share");
        cmd.append(filePath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("KDE Connect"), tr("KDE Connect does not appear to be available on this system."));
        }
    }
}

void ShareManager::shareLocalSend(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QStringList cmd = getExecutable({"localsend_app", "org.localsend.localsend_app", "localsend"});
    if (cmd.isEmpty()) {
        if (parent) {
            QMessageBox::information(
                parent,
                tr("LocalSend Not Found"),
                tr("LocalSend does not appear to be installed on the system.\nYou can install it via your package manager (e.g. pacman -S localsend-bin) or via Flatpak.")
            );
        }
        return;
    }

    QString targetPath = filePath;
    bool isFlatpak = false;
    for (const QString &c : cmd) {
        if (c.contains("flatpak") || c.contains("org.localsend")) {
            isFlatpak = true;
            break;
        }
    }

    if (isFlatpak) {
        QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (downloadDir.isEmpty() || !QDir(downloadDir).exists()) {
            downloadDir = QDir::homePath() + "/Downloads";
            if (!QDir(downloadDir).exists()) {
                downloadDir = QDir::homePath() + "/Scaricati";
            }
        }

        QString tempShareDir = downloadDir + "/BubezShare_Temp";
        QDir().mkpath(tempShareDir);

        QDir tempDir(tempShareDir);
        const QStringList oldFiles = tempDir.entryList(QDir::Files);
        for (const QString &f : oldFiles) {
            tempDir.remove(f);
        }

        QFileInfo fi(filePath);
        QString newPath = tempShareDir + "/" + fi.fileName();
        if (QFile::copy(filePath, newPath)) {
            targetPath = newPath;
        } else {
            targetPath = filePath;
        }
    }

    QString program = cmd.takeFirst();
    cmd.append(targetPath);
    bool started = QProcess::startDetached(program, cmd);
    if (!started && parent) {
        QMessageBox::warning(parent, tr("Launch Error"), QString(tr("Unable to start LocalSend:\n%1")).arg(program));
    }
}

void ShareManager::shareBluetooth(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QStringList cmd = getExecutable({"blueman-sendto", "bluetooth-sendto"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append(filePath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("Bluetooth"), tr("No Bluetooth file transfer utility (blueman-sendto or bluetooth-sendto) found."));
        }
    }
}

void ShareManager::shareTelegram(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QStringList cmd = getExecutable({"telegram-desktop", "org.telegram.desktop"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append("-sendpath");
        cmd.append(filePath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("Telegram"), tr("Telegram Desktop does not appear to be installed on the system."));
        }
    }
}

void ShareManager::shareEmail(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QFileInfo fi(filePath);
    QString fileName = fi.fileName();

    // Copy absolute path to clipboard
    QApplication::clipboard()->setText(filePath);

    QSettings settings("BubezView", "BubezView");
    bool hideHint = settings.value("hide_email_share_hint", false).toBool();

    if (!hideHint) {
        QMessageBox msgBox(parent);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setWindowTitle(tr("Send via Email"));
        msgBox.setText(
            tr("Opening default email client...\n\n"
               "💡 Tip for Webmail (e.g. Gmail, Outlook in Firefox or Chrome):\n"
               "The full file path has been copied to your clipboard.\n\n"
               "When you click 'Attach' in your browser, simply press Ctrl+V (or Paste) in the file picker to select it instantly.")
        );
        QCheckBox *chk = new QCheckBox(tr("Do not show this message again"), &msgBox);
        msgBox.setCheckBox(chk);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();

        if (chk->isChecked()) {
            settings.setValue("hide_email_share_hint", true);
        }
    }

    bool started = false;
    QString xdgEmail = QStandardPaths::findExecutable("xdg-email");
    if (!xdgEmail.isEmpty()) {
        started = QProcess::startDetached(xdgEmail, QStringList() << "--attach" << filePath << "--subject" << QString(tr("Sending file: %1")).arg(fileName));
    }
    if (!started) {
        QUrl mailtoUrl(QString("mailto:?subject=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(tr("Sending file: ") + fileName))));
        QDesktopServices::openUrl(mailtoUrl);
    }
}

void ShareManager::shareQrCode(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    ShareDialog dlg(filePath, parent);
    dlg.exec();
}
