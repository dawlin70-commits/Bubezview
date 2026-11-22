#include "desktop_integrator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>
#include <QDomDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

static void runKbuildsycoca()
{
    const QString k6 = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca6"));
    if (!k6.isEmpty()) {
        QProcess::execute(k6, QStringList() << QStringLiteral("--noincremental"));
        return;
    }
    const QString k5 = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca5"));
    if (!k5.isEmpty()) {
        QProcess::execute(k5, QStringList() << QStringLiteral("--noincremental"));
    }
}

DesktopIntegrator::DesktopEnv DesktopIntegrator::currentDesktop()
{
    QString de = qgetenv("XDG_CURRENT_DESKTOP").toUpper();
    QString session = qgetenv("DESKTOP_SESSION").toUpper();

    if (de.contains("KDE") || de.contains("PLASMA") || session.contains("KDE") || session.contains("PLASMA"))
        return DesktopEnv::KDE;
    if (de.contains("CINNAMON") || session.contains("CINNAMON"))
        return DesktopEnv::Cinnamon;
    if (de.contains("GNOME") || de.contains("UBUNTU") || session.contains("GNOME"))
        return DesktopEnv::GNOME;
    if (de.contains("XFCE") || session.contains("XFCE"))
        return DesktopEnv::XFCE;

    return DesktopEnv::Other;
}

QString DesktopIntegrator::desktopName()
{
    switch (currentDesktop()) {
    case DesktopEnv::KDE: return "KDE Plasma";
    case DesktopEnv::GNOME: return "GNOME";
    case DesktopEnv::Cinnamon: return "Cinnamon / Linux Mint";
    case DesktopEnv::XFCE: return "XFCE";
    default: return "Linux Desktop";
    }
}

static QString getAppBinaryPath()
{
    return QCoreApplication::applicationFilePath();
}

static QString getIconPath()
{
    return "bubezview";
}

// ----------------------------------------------------
// 1. KDE DOLPHIN
// ----------------------------------------------------

static void updateXmlGuiFile(const QString &filePath, bool enable, const QString &shortcut = "Space")
{
    QFile file(filePath);
    QDomDocument doc;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        doc.setContent(&file);
        file.close();
    }

    QDomElement root = doc.documentElement();
    if (root.isNull()) {
        root = doc.createElement("gui");
        root.setAttribute("name", "dolphin");
        root.setAttribute("version", "49");
        doc.appendChild(root);
    } else {
        // Ensure KXmlGui version is at least 49 so Dolphin 26+ never discards local overrides as stale
        const int curVer = root.attribute("version").toInt();
        if (curVer < 49) {
            root.setAttribute("version", "49");
        }
    }

    QList<QDomElement> apList;
    QDomNodeList apNodes = root.elementsByTagName("ActionProperties");
    bool hasStandard = false;
    bool hasDefault = false;
    for (int i = 0; i < apNodes.count(); ++i) {
        QDomElement el = apNodes.at(i).toElement();
        if (el.attribute("scheme") == "Default") {
            hasDefault = true;
        } else {
            hasStandard = true;
        }
        apList.append(el);
    }

    if (!hasStandard) {
        QDomElement apStandard = doc.createElement("ActionProperties");
        root.appendChild(apStandard);
        apList.append(apStandard);
    }
    if (!hasDefault) {
        QDomElement apDefault = doc.createElement("ActionProperties");
        apDefault.setAttribute("scheme", "Default");
        root.appendChild(apDefault);
        apList.append(apDefault);
    }

    for (QDomElement &ap : apList) {
        QDomNodeList actions = ap.elementsByTagName("Action");
        QList<QDomNode> toRemove;
        for (int j = 0; j < actions.count(); ++j) {
            QDomElement act = actions.at(j).toElement();
            QString name = act.attribute("name");
            if (name == "servicemenu_bubezview.desktop::bubezview" || name == "toggle_selection_mode") {
                toRemove.append(actions.at(j));
            }
        }
        for (QDomNode n : toRemove) {
            ap.removeChild(n);
        }

        if (enable) {
            QDomElement actBubez = doc.createElement("Action");
            actBubez.setAttribute("name", "servicemenu_bubezview.desktop::bubezview");
            actBubez.setAttribute("shortcut", shortcut);
            ap.appendChild(actBubez);

            // Only mute toggle_selection_mode if using Space shortcut
            if (shortcut == "Space") {
                QDomElement actToggle = doc.createElement("Action");
                actToggle.setAttribute("name", "toggle_selection_mode");
                actToggle.setAttribute("shortcut", "");
                ap.appendChild(actToggle);
            }
        }
    }

    QFileInfo fi(filePath);
    QDir().mkpath(fi.dir().absolutePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&file);
        doc.save(stream, 2);
        file.close();
    }
}

/**
 * @brief Synchronizes Dolphin's [Shortcuts] section in ~/.config/dolphinrc
 *        so Spacebar persists across tab switches and KXmlGui session saves.
 */
static void updateDolphinRcShortcuts(bool enable, const QString &shortcut = QStringLiteral("Space"))
{
    const QString rcPath = QDir::homePath() + QStringLiteral("/.config/dolphinrc");
    QSettings dolphinRc(rcPath, QSettings::IniFormat);
    dolphinRc.beginGroup(QStringLiteral("Shortcuts"));
    if (enable) {
        dolphinRc.setValue(QStringLiteral("servicemenu_bubezview.desktop::bubezview"), shortcut);
        if (shortcut == QLatin1String("Space")) {
            dolphinRc.setValue(QStringLiteral("toggle_selection_mode"), QStringLiteral("none"));
        } else {
            dolphinRc.remove(QStringLiteral("toggle_selection_mode"));
        }
    } else {
        dolphinRc.remove(QStringLiteral("servicemenu_bubezview.desktop::bubezview"));
        dolphinRc.remove(QStringLiteral("toggle_selection_mode"));
    }
    dolphinRc.endGroup();
    dolphinRc.sync();
}

bool DesktopIntegrator::isDolphinEnabled()
{
    QString home = QDir::homePath();
    QString sm = home + "/.local/share/kio/servicemenus/bubezview.desktop";
    if (!QFile::exists(sm)) return false;

    QStringList rcFiles = {
        home + "/.local/share/kxmlgui5/dolphin/dolphinui.rc",
        home + "/.local/share/kxmlgui6/dolphin/dolphinui.rc"
    };

    for (const QString &rc : rcFiles) {
        if (!QFile::exists(rc)) continue;
        QFile f(rc);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = f.readAll();
            if (content.contains("servicemenu_bubezview.desktop::bubezview")) {
                return true;
            }
        }
    }
    return false;
}

QString DesktopIntegrator::currentDolphinShortcut()
{
    const QString home = QDir::homePath();

    // 1. Check ~/.config/dolphinrc [Shortcuts] first (authoritative for Plasma 6 / Dolphin 26+)
    const QString rcPath = home + QStringLiteral("/.config/dolphinrc");
    if (QFile::exists(rcPath)) {
        QSettings dolphinRc(rcPath, QSettings::IniFormat);
        dolphinRc.beginGroup(QStringLiteral("Shortcuts"));
        const QString sc = dolphinRc.value(QStringLiteral("servicemenu_bubezview.desktop::bubezview")).toString().trimmed();
        dolphinRc.endGroup();
        if (!sc.isEmpty() && sc.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0) {
            return sc;
        }
    }

    // 2. Fallback to KXmlGui dolphinui.rc
    const QStringList rcFiles = {
        home + QStringLiteral("/.local/share/kxmlgui6/dolphin/dolphinui.rc"),
        home + QStringLiteral("/.local/share/kxmlgui5/dolphin/dolphinui.rc")
    };

    static const QRegularExpression scRegex(
        QStringLiteral(R"(name=\"servicemenu_bubezview\.desktop::bubezview\"[^>]*shortcut=\"([^\"]+)\")"));

    for (const QString &rc : rcFiles) {
        if (!QFile::exists(rc)) continue;
        QFile f(rc);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(f.readAll());
            const auto match = scRegex.match(content);
            if (match.hasMatch() && !match.captured(1).trimmed().isEmpty()) {
                return match.captured(1).trimmed();
            }
        }
    }
    return QStringLiteral("Space");
}

bool DesktopIntegrator::enableDolphin(const QString &shortcut)
{
    QString home = QDir::homePath();
    QString appPath = getAppBinaryPath();
    QString icon = getIconPath();

    QStringList menuDirs = {
        home + "/.local/share/kio/servicemenus",
        home + "/.local/share/kservices5/ServiceMenus"
    };

    QString triggerPath = home + "/.local/share/bubezview/trigger_bubez.sh";
    if (!QFile::exists(triggerPath)) {
        triggerPath = home + "/.local/share/BubezView/trigger_bubez.sh";
    }
    if (!QFile::exists(triggerPath)) {
        QString appDir = QCoreApplication::applicationDirPath();
        if (QFile::exists(appDir + "/../trigger_bubez.sh")) {
            triggerPath = QFileInfo(appDir + "/../trigger_bubez.sh").canonicalFilePath();
        } else if (QFile::exists(appDir + "/../../trigger_bubez.sh")) {
            triggerPath = QFileInfo(appDir + "/../../trigger_bubez.sh").canonicalFilePath();
        }
    }
    QString execCmd = QFile::exists(triggerPath) ? (QStringLiteral("\"%1\" %F").arg(triggerPath))
                                                 : (QStringLiteral("\"%1\" %F").arg(appPath));

    QString desktopContent = QString(
        "[Desktop Entry]\n"
        "Type=Service\n"
        "MimeType=all/allfiles;inode/directory;\n"
        "Actions=bubezview;\n"
        "X-KDE-ServiceTypes=KonqPopupMenu/Plugin\n"
        "X-KDE-Priority=TopLevel\n"
        "X-KDE-Shortcuts=%3\n"
        "StartupNotify=false\n"
        "X-KDE-StartupNotify=false\n\n"
        "[Desktop Action bubezview]\n"
        "Name=BubezView (Quick Preview)\n"
        "Name[it]=BubezView (Anteprima rapida)\n"
        "Icon=%1\n"
        "Exec=%2\n"
        "StartupNotify=false\n"
        "X-KDE-StartupNotify=false\n"
    ).arg(icon, execCmd, shortcut);

    for (const QString &mDir : menuDirs) {
        QDir().mkpath(mDir);
        QFile file(mDir + "/bubezview.desktop");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(desktopContent.toUtf8());
            file.close();
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                QFileDevice::ReadOther | QFileDevice::ExeOther);
        }
    }

    updateXmlGuiFile(home + "/.local/share/kxmlgui5/dolphin/dolphinui.rc", true, shortcut);
    updateXmlGuiFile(home + "/.local/share/kxmlgui6/dolphin/dolphinui.rc", true, shortcut);
    updateDolphinRcShortcuts(true, shortcut);

    runKbuildsycoca();
    QProcess::execute("killall", QStringList() << "dolphin");

    return true;
}

bool DesktopIntegrator::disableDolphin()
{
    QString home = QDir::homePath();

    QFile::remove(home + "/.local/share/kio/servicemenus/bubezview.desktop");
    QFile::remove(home + "/.local/share/kservices5/ServiceMenus/bubezview.desktop");

    updateXmlGuiFile(home + "/.local/share/kxmlgui5/dolphin/dolphinui.rc", false);
    updateXmlGuiFile(home + "/.local/share/kxmlgui6/dolphin/dolphinui.rc", false);
    updateDolphinRcShortcuts(false);

    runKbuildsycoca();
    QProcess::execute("killall", QStringList() << "dolphin");

    return true;
}

// ----------------------------------------------------
// 2. GNOME NAUTILUS & CINNAMON NEMO (DBUS + DUMMY BINS)
// ----------------------------------------------------

bool DesktopIntegrator::isGnomeNemoEnabled()
{
    QString home = QDir::homePath();
    QString userDbus = home + "/.local/share/dbus-1/services/org.gnome.NautilusPreviewer.service";
    QString sushiDummy = home + "/.local/bin/sushi";

    return (QFile::exists(userDbus) || QFile::exists(sushiDummy));
}

bool DesktopIntegrator::enableGnomeNemo()
{
    QString home = QDir::homePath();
    QString appPath = getAppBinaryPath();

    // 1. Install DBus service files pointing to BubezView
    QString dbusDir = home + "/.local/share/dbus-1/services";
    QDir().mkpath(dbusDir);

    QStringList services = {
        "org.gnome.NautilusPreviewer",
        "org.nemo.Preview",
        "org.nemo.Previewer"
    };

    // Avoid surrounding quotes when path has no spaces for strict dbus-broker compatibility (Fedora / Ubuntu)
    const QString execBin = appPath.contains(QLatin1Char(' '))
        ? QStringLiteral("\"%1\"").arg(appPath)
        : appPath;

    for (const QString &svc : services) {
        QString sFile = dbusDir + "/" + svc + ".service";
        QFile f(sFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString content = QString(
                "[D-BUS Service]\n"
                "Name=%1\n"
                "Exec=%2 --previewer\n"
            ).arg(svc, execBin);
            f.write(content.toUtf8());
            f.close();
        }
    }

    // 2. Install dummy sushi & nemo-preview executable scripts in ~/.local/bin
    QString binDir = home + "/.local/bin";
    QDir().mkpath(binDir);

    QString dummyContent = QString(
        "#!/bin/sh\n"
        "# Forward Nautilus / Nemo preview request directly to BubezView C++\n"
        "if [ -n \"$1\" ]; then\n"
        "    exec \"%1\" \"$@\"\n"
        "fi\n"
        "exit 0\n"
    ).arg(appPath);

    for (const QString &binName : QStringList{"sushi", "nemo-preview"}) {
        QString p = binDir + "/" + binName;
        QFile f(p);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(dummyContent.toUtf8());
            f.close();
            f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                             QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                             QFileDevice::ReadOther | QFileDevice::ExeOther);
        }
    }

    // Reload DBus and restart file managers
    QProcess::execute("dbus-send", QStringList() << "--session" << "--type=method_call" << "--dest=org.freedesktop.DBus" << "/" << "org.freedesktop.DBus.ReloadConfig");
    QProcess::execute("nautilus", QStringList() << "-q");
    QProcess::execute("nemo", QStringList() << "-q");

    return true;
}

bool DesktopIntegrator::disableGnomeNemo()
{
    QString home = QDir::homePath();

    QStringList services = {
        "org.gnome.NautilusPreviewer",
        "org.nemo.Preview",
        "org.nemo.Previewer"
    };
    for (const QString &svc : services) {
        QFile::remove(home + "/.local/share/dbus-1/services/" + svc + ".service");
    }

    QFile::remove(home + "/.local/bin/sushi");
    QFile::remove(home + "/.local/bin/nemo-preview");

    QProcess::execute("dbus-send", QStringList() << "--session" << "--type=method_call" << "--dest=org.freedesktop.DBus" << "/" << "org.freedesktop.DBus.ReloadConfig");
    QProcess::execute("nautilus", QStringList() << "-q");
    QProcess::execute("nemo", QStringList() << "-q");

    return true;
}

// ----------------------------------------------------
// 3. CONTEXT MENU (RIGHT CLICK)
// ----------------------------------------------------

bool DesktopIntegrator::isContextMenuEnabled()
{
    QString home = QDir::homePath();
    bool nautilusScript = QFile::exists(home + "/.local/share/nautilus/scripts/Preview with BubezView") ||
                          QFile::exists(home + "/.local/share/nautilus/scripts/Anteprima con BubezView");
    bool nemoAction = QFile::exists(home + "/.local/share/nemo/actions/bubezview.nemo_action");
    bool dolphinMenu = QFile::exists(home + "/.local/share/kio/servicemenus/bubezview.desktop");

    return (nautilusScript || nemoAction || dolphinMenu);
}

bool DesktopIntegrator::enableContextMenu()
{
    QString home = QDir::homePath();
    QString appPath = getAppBinaryPath();
    QString icon = getIconPath();

    // 1. Nautilus Script
    QString nautilusDir = home + "/.local/share/nautilus/scripts";
    QDir().mkpath(nautilusDir);
    QString scriptPath = nautilusDir + "/Preview with BubezView";
    QFile fn(scriptPath);
    if (fn.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString c = QString(
            "#!/bin/bash\n"
            "TARGET=\"${NAUTILUS_SCRIPT_SELECTED_FILE_PATHS%%$\\n*}\"\n"
            "if [ -n \"$TARGET\" ]; then\n"
            "    \"%1\" \"$TARGET\"\n"
            "fi\n"
        ).arg(appPath);
        fn.write(c.toUtf8());
        fn.close();
        fn.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                          QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                          QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    // 2. Nemo Action
    QString nemoDir = home + "/.local/share/nemo/actions";
    QDir().mkpath(nemoDir);
    QString actionPath = nemoDir + "/bubezview.nemo_action";
    QFile fne(actionPath);
    if (fne.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString c = QString(
            "[Nemo Action]\n"
            "Name=BubezView (Quick Preview)\n"
            "Name[it]=BubezView (Anteprima rapida)\n"
            "Comment=Instant preview of images, text, and documents\n"
            "Comment[it]=Anteprima istantanea di immagini, testi e documenti\n"
            "Exec=\"%1\" %F\n"
            "Icon-Name=%2\n"
            "Selection=any\n"
            "Extensions=any;\n"
            "EscapeSpaces=false\n"
        ).arg(appPath, icon);
        fne.write(c.toUtf8());
        fne.close();
    }

    // 3. Dolphin ServiceMenu (also ensures right-click context entry in Dolphin)
    enableDolphin();

    return true;
}

bool DesktopIntegrator::disableContextMenu()
{
    QString home = QDir::homePath();
    QFile::remove(home + "/.local/share/nautilus/scripts/Preview with BubezView");
    QFile::remove(home + "/.local/share/nautilus/scripts/Anteprima con BubezView");
    QFile::remove(home + "/.local/share/nemo/actions/bubezview.nemo_action");
    return true;
}

// ----------------------------------------------------
// 4. APPLICATION LAUNCHER MENU ENTRY
// ----------------------------------------------------

bool DesktopIntegrator::isAppMenuInstalled()
{
    QString home = QDir::homePath();
    return QFile::exists(home + "/.local/share/applications/bubezview.desktop");
}

bool DesktopIntegrator::installAppMenu()
{
    QString home = QDir::homePath();
    QString appPath = getAppBinaryPath();
    QString icon = getIconPath();

    QString appDir = home + "/.local/share/applications";
    QDir().mkpath(appDir);
    QString desktopPath = appDir + "/bubezview.desktop";

    QFile f(desktopPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString content = QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=BubezView\n"
            "GenericName=File and Document Viewer\n"
            "GenericName[it]=Visualizzatore di file e documenti\n"
            "Comment=Instant ultra-fast native preview of images, text, and documents\n"
            "Comment[it]=Anteprima ultra-rapida nativa di immagini, testi e documenti\n"
            "Exec=\"%1\" %F\n"
            "Icon=%2\n"
            "Terminal=false\n"
            "Categories=Graphics;Viewer;Utility;Qt;\n"
            "MimeType=image/png;image/jpeg;image/webp;image/svg+xml;image/bmp;image/gif;text/plain;text/markdown;application/pdf;\n"
            "StartupNotify=true\n"
        ).arg(appPath, icon);
        f.write(content.toUtf8());
        f.close();
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                         QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                         QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    QProcess::execute("update-desktop-database", QStringList() << appDir);
    runKbuildsycoca();

    return true;
}

bool DesktopIntegrator::uninstallAppMenu()
{
    QString home = QDir::homePath();
    QString appDir = home + "/.local/share/applications";
    QFile::remove(appDir + "/bubezview.desktop");

    QProcess::execute("update-desktop-database", QStringList() << appDir);
    runKbuildsycoca();

    return true;
}
