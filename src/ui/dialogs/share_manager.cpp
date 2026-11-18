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
#include <QMimeData>
#include <QLabel>
#include <QTimer>
#include <QPointer>

ShareManager::ShareManager(QWidget *parentWidget, std::function<QString()> getFilePathCallback)
    : QObject(parentWidget)
    , m_parent(parentWidget)
    , m_getFilePath(std::move(getFilePathCallback))
{
}

void ShareManager::setImageProvider(std::function<QImage()> getImageCallback, bool isPagedDocument)
{
    m_getImage = std::move(getImageCallback);
    m_isPagedDocument = isPagedDocument;
}

ShareManager::EmailProvider ShareManager::lastEmailProvider()
{
    QSettings settings("Bubez", "BubezView");
    int val = settings.value("last_email_provider", static_cast<int>(EmailProvider::SystemDefault)).toInt();
    if (val < 0 || val > static_cast<int>(EmailProvider::Yahoo)) {
        return EmailProvider::SystemDefault;
    }
    return static_cast<EmailProvider>(val);
}

void ShareManager::setLastEmailProvider(EmailProvider provider)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("last_email_provider", static_cast<int>(provider));
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

namespace {
std::function<bool(const QString &, const QString &, QWidget *)> s_folderCompressor;
}

void ShareManager::setFolderCompressor(std::function<bool(const QString &, const QString &, QWidget *)> compressor)
{
    s_folderCompressor = std::move(compressor);
}

QString ShareManager::ensureShareableFile(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return QString();
    }

    QFileInfo fi(filePath);
    if (!fi.isDir()) {
        return filePath;
    }

    // Target is a directory: compress it on-the-fly into a .zip inside BubezShare_Temp
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDir.isEmpty() || !QDir(downloadDir).exists()) {
        downloadDir = QDir::homePath() + "/Downloads";
        if (!QDir(downloadDir).exists()) {
            downloadDir = QDir::homePath() + "/Scaricati";
        }
        if (!QDir(downloadDir).exists()) {
            downloadDir = QDir::tempPath();
        }
    }

    QString tempShareDir = downloadDir + "/BubezShare_Temp";
    QDir().mkpath(tempShareDir);

    QString folderName = fi.fileName();
    if (folderName.isEmpty()) {
        folderName = QFileInfo(QDir(filePath).canonicalPath()).fileName();
        if (folderName.isEmpty()) folderName = QStringLiteral("SharedFolder");
    }

    QString destZipPath = tempShareDir + "/" + folderName + ".zip";
    QFile::remove(destZipPath);

    bool ok = false;
    if (s_folderCompressor) {
        ok = s_folderCompressor(fi.absoluteFilePath(), destZipPath, parent);
    }

    if (!ok) {
        // CLI fallbacks if libarchive plugin callback was not registered
        QString zipBin = QStandardPaths::findExecutable("zip");
        if (!zipBin.isEmpty()) {
            QProcess p;
            p.setWorkingDirectory(fi.absolutePath());
            p.start(zipBin, QStringList() << "-r" << "-q" << destZipPath << folderName);
            ok = p.waitForFinished(60000) && p.exitCode() == 0 && QFile::exists(destZipPath);
        }
        if (!ok) {
            QString pyBin = QStandardPaths::findExecutable("python3");
            if (!pyBin.isEmpty()) {
                QProcess p;
                p.start(pyBin, QStringList() << "-m" << "zipfile" << "-c" << destZipPath << fi.absoluteFilePath());
                ok = p.waitForFinished(60000) && p.exitCode() == 0 && QFile::exists(destZipPath);
            }
        }
    }

    return (ok && QFile::exists(destZipPath)) ? destZipPath : QString();
}

void ShareManager::populateMenu(QMenu *menu)
{
    if (!menu) return;

    menu->clear();
    QStyle *st = QApplication::style();

    auto addProtocolActions = [this, st](QMenu *targetMenu, std::function<QString()> pathProvider, bool allowImageProvider) {
        // 1. KDE Connect (if installed)
        if (isAvailable({"kdeconnect-cli", "org.kde.kdeconnect"})) {
            QIcon icon = QIcon::fromTheme("kdeconnect", st->standardIcon(QStyle::SP_DriveNetIcon));
            QAction *act = targetMenu->addAction(icon, tr("Send to device (KDE Connect)..."));
            connect(act, &QAction::triggered, this, [this, pathProvider]() {
                shareKdeConnect(pathProvider ? pathProvider() : QString(), m_parent);
            });
        }

        // 2. LocalSend (if installed)
        if (isAvailable({"localsend_app", "org.localsend.localsend_app", "localsend"})) {
            QIcon icon = QIcon::fromTheme("localsend", QIcon::fromTheme("network-workgroup", st->standardIcon(QStyle::SP_DriveNetIcon)));
            QAction *act = targetMenu->addAction(icon, tr("Send with LocalSend..."));
            connect(act, &QAction::triggered, this, [this, pathProvider]() {
                shareLocalSend(pathProvider ? pathProvider() : QString(), m_parent);
            });
        }

        // 3. Bluetooth (if available)
        if (isAvailable({"bluetooth-sendto", "blueman-sendto"})) {
            QIcon icon = QIcon::fromTheme("preferences-system-bluetooth", st->standardIcon(QStyle::SP_DriveNetIcon));
            QAction *act = targetMenu->addAction(icon, tr("Send via Bluetooth..."));
            connect(act, &QAction::triggered, this, [this, pathProvider]() {
                shareBluetooth(pathProvider ? pathProvider() : QString(), m_parent);
            });
        }

        // 4. Telegram Desktop (if available)
        if (isAvailable({"telegram-desktop", "org.telegram.desktop"})) {
            QIcon icon = QIcon::fromTheme("telegram", st->standardIcon(QStyle::SP_ArrowUp));
            QAction *act = targetMenu->addAction(icon, tr("Send with Telegram Desktop..."));
            connect(act, &QAction::triggered, this, [this, pathProvider]() {
                shareTelegram(pathProvider ? pathProvider() : QString(), m_parent);
            });
        }

        // 5. Email (always available: Desktop client + Webmail providers)
        {
            QIcon icon = QIcon::fromTheme("mail-message-new", st->standardIcon(QStyle::SP_FileDialogDetailedView));
            QMenu *emailMenu = targetMenu->addMenu(icon, tr("Send via Email"));
            const EmailProvider lastProv = lastEmailProvider();
            const std::function<QImage()> imgCb = (allowImageProvider && m_getImage) ? m_getImage : nullptr;
            const bool isPaged = allowImageProvider && m_isPagedDocument && static_cast<bool>(imgCb);
            QPointer<QWidget> parentPtr(m_parent);

            auto addProviderEntries = [emailMenu, pathProvider, imgCb, parentPtr, lastProv](QMenu *menuTarget, bool asImageOnly) {
                struct ProvItem {
                    EmailProvider prov;
                    QString label;
                };
                const QList<ProvItem> items = {
                    { EmailProvider::SystemDefault, tr("💻 System Email Client (Desktop)") },
                    { EmailProvider::Gmail,         tr("🔴 Gmail (Browser)") },
                    { EmailProvider::Outlook,       tr("🔵 Outlook / Live (Browser)") },
                    { EmailProvider::Yahoo,         tr("🟣 Yahoo Mail (Browser)") }
                };

                for (const auto &item : items) {
                    QString text = item.label;
                    if (!asImageOnly && item.prov == lastProv) {
                        text = QStringLiteral("★ ") + text;
                    }
                    QAction *act = menuTarget->addAction(text);
                    EmailProvider p = item.prov;
                    QObject::connect(act, &QAction::triggered, [pathProvider, imgCb, parentPtr, p, asImageOnly]() {
                        QString path = pathProvider ? pathProvider() : QString();
                        QImage img = imgCb ? imgCb() : QImage();
                        shareEmail(path, parentPtr.data(), p, img, asImageOnly);
                    });
                }
            };

            addProviderEntries(emailMenu, false);

            if (isPaged) {
                emailMenu->addSeparator();
                QMenu *pageImgMenu = emailMenu->addMenu(
                    QIcon::fromTheme("image-x-generic", st->standardIcon(QStyle::SP_FileIcon)),
                    tr("🖼️ Current Page as Image")
                );
                addProviderEntries(pageImgMenu, true);
            }
        }

        // 6. QR Code for smartphones (always available with built-in HTTP server & libqrencode)
        {
            QIcon icon = QIcon::fromTheme("view-barcode-qr", st->standardIcon(QStyle::SP_MessageBoxInformation));
            QAction *act = targetMenu->addAction(icon, tr("Generate QR Code for smartphone..."));
            connect(act, &QAction::triggered, this, [this, pathProvider]() {
                shareQrCode(pathProvider ? pathProvider() : QString(), m_parent);
            });
        }
    };

    addProtocolActions(menu, m_getFilePath, true);

    // If the current target is a single file (e.g. viewing an image/PDF), also offer sharing its entire parent folder as .zip
    QString currentPath = m_getFilePath ? m_getFilePath() : QString();
    if (!currentPath.isEmpty() && QFileInfo(currentPath).isFile()) {
        QString parentFolder = QFileInfo(currentPath).absolutePath();
        if (!parentFolder.isEmpty() && QDir(parentFolder).exists()) {
            menu->addSeparator();
            QString folderName = QFileInfo(parentFolder).fileName();
            QMenu *folderShareMenu = menu->addMenu(
                QIcon::fromTheme("package-x-generic", st->standardIcon(QStyle::SP_DirIcon)),
                tr("📦 Compress & Share Current Folder (%1.zip)").arg(folderName)
            );
            addProtocolActions(folderShareMenu, [parentFolder]() { return parentFolder; }, false);
        }
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
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QString resolvedPath = ensureShareableFile(filePath, parent);
    if (resolvedPath.isEmpty()) return;

    QStringList cmd = getExecutable({"kdeconnect-cli", "org.kde.kdeconnect"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append("--share");
        cmd.append(resolvedPath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("KDE Connect"), tr("KDE Connect does not appear to be available on this system."));
        }
    }
}

void ShareManager::shareLocalSend(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
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

    QString resolvedPath = ensureShareableFile(filePath, parent);
    if (resolvedPath.isEmpty()) return;

    QString targetPath = resolvedPath;
    bool isFlatpak = false;
    for (const QString &c : cmd) {
        if (c.contains("flatpak") || c.contains("org.localsend")) {
            isFlatpak = true;
            break;
        }
    }

    if (isFlatpak && !QFileInfo( filePath ).isDir()) {
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

        QFileInfo fi(resolvedPath);
        QString newPath = tempShareDir + "/" + fi.fileName();
        if (QFile::copy(resolvedPath, newPath)) {
            targetPath = newPath;
        } else {
            targetPath = resolvedPath;
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
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QString resolvedPath = ensureShareableFile(filePath, parent);
    if (resolvedPath.isEmpty()) return;

    QStringList cmd = getExecutable({"blueman-sendto", "bluetooth-sendto"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append(resolvedPath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("Bluetooth"), tr("No Bluetooth file transfer utility (blueman-sendto or bluetooth-sendto) found."));
        }
    }
}

void ShareManager::shareTelegram(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QString resolvedPath = ensureShareableFile(filePath, parent);
    if (resolvedPath.isEmpty()) return;

    QStringList cmd = getExecutable({"telegram-desktop", "org.telegram.desktop"});
    if (!cmd.isEmpty()) {
        QString program = cmd.takeFirst();
        cmd.append("-sendpath");
        cmd.append(resolvedPath);
        QProcess::startDetached(program, cmd);
    } else {
        if (parent) {
            QMessageBox::information(parent, tr("Telegram"), tr("Telegram Desktop does not appear to be installed on the system."));
        }
    }
}

static void showShareToast(QWidget *parent, const QString &message)
{
    if (!parent) return;
    QWidget *topWin = parent->window();
    if (!topWin) topWin = parent;

    QLabel *toast = new QLabel(message, topWin);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setWindowFlags(Qt::SubWindow);
    toast->setWordWrap(false);
    toast->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: rgba(24, 24, 27, 235);"
        "  color: #f4f4f5;"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 10px;"
        "  padding: 10px 18px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"
    ));
    toast->adjustSize();
    int x = (topWin->width() - toast->width()) / 2;
    int y = topWin->height() - toast->height() - 28;
    toast->move(qMax(12, x), qMax(12, y));
    toast->show();
    toast->raise();

    QPointer<QLabel> toastPtr(toast);
    QTimer::singleShot(5000, toast, [toastPtr]() {
        if (toastPtr) toastPtr->close();
    });
}

void ShareManager::shareEmail(const QString &filePath,
                              QWidget *parent,
                              EmailProvider provider,
                              const QImage &imageToCopy,
                              bool asImageOnly)
{
    if (!asImageOnly && (filePath.isEmpty() || !QFileInfo::exists(filePath))) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QString resolvedPath;
    if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
        resolvedPath = asImageOnly ? filePath : ensureShareableFile(filePath, parent);
        if (!asImageOnly && resolvedPath.isEmpty()) return;
    }

    setLastEmailProvider(provider);

    QFileInfo fi(resolvedPath);
    QString fileName = fi.fileName();
    if (asImageOnly && !fi.completeBaseName().isEmpty()) {
        fileName = fi.completeBaseName() + QStringLiteral(".png");
    } else if (fileName.isEmpty()) {
        fileName = QStringLiteral("image.png");
    }

    const QString subject = tr("Sending: %1").arg(fileName);

    if (provider == EmailProvider::SystemDefault) {
        QString attachPath = resolvedPath;
        if (asImageOnly && !imageToCopy.isNull()) {
            QString tempPng = QDir::tempPath() + QStringLiteral("/BubezShare_") + fileName;
            if (imageToCopy.save(tempPng, "PNG")) {
                attachPath = tempPng;
            }
        }

        if (!attachPath.isEmpty()) {
            QApplication::clipboard()->setText(attachPath);
        }

        bool started = false;
        QString xdgEmail = QStandardPaths::findExecutable("xdg-email");
        if (!xdgEmail.isEmpty() && !attachPath.isEmpty()) {
            started = QProcess::startDetached(
                xdgEmail,
                QStringList() << "--attach" << attachPath << "--subject" << subject
            );
        }
        if (!started) {
            QUrl mailtoUrl(QStringLiteral("mailto:?subject=%1")
                               .arg(QString::fromUtf8(QUrl::toPercentEncoding(subject))));
            QDesktopServices::openUrl(mailtoUrl);
        }
        return;
    }

    // Webmail providers (Gmail, Outlook, Yahoo):
    // Populate polymorphic QMimeData so Ctrl+V works both as inline image and file path
    QMimeData *mime = new QMimeData();
    const bool hasImg = !imageToCopy.isNull();
    if (hasImg) {
        mime->setImageData(imageToCopy);
    }
    if (!resolvedPath.isEmpty() && !asImageOnly) {
        mime->setText(resolvedPath);
        mime->setUrls({ QUrl::fromLocalFile(resolvedPath) });
    } else if (!hasImg && !resolvedPath.isEmpty()) {
        mime->setText(resolvedPath);
        mime->setUrls({ QUrl::fromLocalFile(resolvedPath) });
    }
    QApplication::clipboard()->setMimeData(mime);

    const QString encodedSub = QString::fromUtf8(QUrl::toPercentEncoding(subject));
    QString composeUrl;
    QString providerName;
    switch (provider) {
    case EmailProvider::Gmail:
        composeUrl = QStringLiteral("https://mail.google.com/mail/?view=cm&fs=1&su=%1").arg(encodedSub);
        providerName = QStringLiteral("Gmail");
        break;
    case EmailProvider::Outlook:
        composeUrl = QStringLiteral("https://outlook.live.com/mail/0/deeplink/compose?subject=%1").arg(encodedSub);
        providerName = QStringLiteral("Outlook");
        break;
    case EmailProvider::Yahoo:
        composeUrl = QStringLiteral("https://compose.mail.yahoo.com/?subj=%1").arg(encodedSub);
        providerName = QStringLiteral("Yahoo Mail");
        break;
    case EmailProvider::SystemDefault:
        break;
    }

    QDesktopServices::openUrl(QUrl(composeUrl));

    if (hasImg) {
        showShareToast(parent, tr("📋 Image copied! Opening %1 — press Ctrl+V in the email to paste it.").arg(providerName));
    } else {
        showShareToast(parent, tr("📋 File path copied! Opening %1 — click Attach and press Ctrl+V.").arg(providerName));
    }
}

void ShareManager::shareQrCode(const QString &filePath, QWidget *parent)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (parent) QMessageBox::warning(parent, tr("Share"), tr("No open file to share."));
        return;
    }

    QString resolvedPath = ensureShareableFile(filePath, parent);
    if (resolvedPath.isEmpty()) return;

    ShareDialog dlg(resolvedPath, parent);
    dlg.exec();
}

