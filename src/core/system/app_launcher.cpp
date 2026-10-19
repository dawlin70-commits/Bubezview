#include "app_launcher.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QUrl>
#include <QLocale>
#include <QStandardPaths>
#include <QSet>
#include <algorithm>

QIcon AppInfo::icon() const
{
    if (!iconName.isEmpty()) {
        QIcon ic = QIcon::fromTheme(iconName);
        if (!ic.isNull()) return ic;
        if (QFile::exists(iconName)) return QIcon(iconName);
    }
    return QIcon::fromTheme("application-x-executable");
}

bool AppLauncher::parseDesktopFile(const QString &desktopPath, AppInfo &outApp)
{
    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Check if valid [Desktop Entry]
    if (!content.contains("[Desktop Entry]")) return false;

    QString lang = QLocale::system().name(); // e.g. "it_IT"
    QString langShort = lang.split('_').first(); // "it"

    QString name, nameLang, nameShort;
    QString genericName, genericLang;
    QString comment, commentLang;
    QString icon;
    QString exec;
    bool noDisplay = false;
    bool hidden = false;
    QString type;
    QStringList mimes;

    const QStringList lines = content.split('\n');
    bool inDesktopEntry = false;

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inDesktopEntry = (trimmed == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || trimmed.startsWith('#') || !trimmed.contains('=')) continue;

        int eq = trimmed.indexOf('=');
        QString key = trimmed.left(eq).trimmed();
        QString val = trimmed.mid(eq + 1).trimmed();

        if (key == "Type") {
            type = val;
        } else if (key == "NoDisplay" && (val.compare("true", Qt::CaseInsensitive) == 0 || val == "1")) {
            noDisplay = true;
        } else if (key == "Hidden" && (val.compare("true", Qt::CaseInsensitive) == 0 || val == "1")) {
            hidden = true;
        } else if (key == "Name") {
            name = val;
        } else if (key == "Name[" + lang + "]") {
            nameLang = val;
        } else if (key == "Name[" + langShort + "]") {
            nameShort = val;
        } else if (key == "GenericName") {
            genericName = val;
        } else if (key == "GenericName[" + lang + "]" || key == "GenericName[" + langShort + "]") {
            genericLang = val;
        } else if (key == "Comment") {
            comment = val;
        } else if (key == "Comment[" + lang + "]" || key == "Comment[" + langShort + "]") {
            commentLang = val;
        } else if (key == "Icon") {
            icon = val;
        } else if (key == "Exec") {
            exec = val;
        } else if (key == "MimeType") {
            mimes = val.split(';', Qt::SkipEmptyParts);
        }
    }

    if (type != "Application" || noDisplay || hidden || exec.isEmpty()) {
        return false;
    }

    // Do not show self in "Open With"
    QFileInfo fi(desktopPath);
    if (fi.fileName().contains("bubezview", Qt::CaseInsensitive) || exec.contains("bubezview", Qt::CaseInsensitive)) {
        return false;
    }

    outApp.id = fi.fileName();
    outApp.desktopPath = desktopPath;
    outApp.name = !nameLang.isEmpty() ? nameLang : (!nameShort.isEmpty() ? nameShort : name);
    outApp.genericName = !genericLang.isEmpty() ? genericLang : genericName;
    outApp.comment = !commentLang.isEmpty() ? commentLang : comment;
    outApp.iconName = icon;
    outApp.execCmd = exec;
    outApp.mimeTypes = mimes;

    return !outApp.name.isEmpty();
}

QList<AppInfo> AppLauncher::getAssociatedApps(const QString &filePath)
{
    QList<AppInfo> result;
    if (filePath.isEmpty() || !QFile::exists(filePath)) return result;

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    QString mimeName = mime.name();
    QStringList allMimes = mime.aliases();
    allMimes.prepend(mimeName);

    QStringList appDirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/local/share/applications",
        "/usr/share/applications",
        "/var/lib/flatpak/exports/share/applications",
        QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
    };

    QSet<QString> seenIds;
    QList<QString> priorityIds;

    // 1. Read user mimeapps.list
    QStringList mimeAppsFiles = {
        QDir::homePath() + "/.config/mimeapps.list",
        "/usr/share/applications/mimeapps.list",
        "/etc/xdg/mimeapps.list"
    };

    for (const QString &mPath : mimeAppsFiles) {
        if (!QFile::exists(mPath)) continue;
        QSettings s(mPath, QSettings::IniFormat);
        for (const QString &m : allMimes) {
            QString defVal = s.value("Default Applications/" + m).toString();
            for (const QString &id : defVal.split(';', Qt::SkipEmptyParts)) {
                if (!priorityIds.contains(id)) priorityIds.append(id);
            }
            QString addVal = s.value("Added Associations/" + m).toString();
            for (const QString &id : addVal.split(';', Qt::SkipEmptyParts)) {
                if (!priorityIds.contains(id)) priorityIds.append(id);
            }
        }
    }

    // Resolve priority IDs first
    for (const QString &id : priorityIds) {
        if (seenIds.contains(id)) continue;
        for (const QString &dir : appDirs) {
            QString p = dir + "/" + id;
            if (QFile::exists(p)) {
                AppInfo app;
                if (parseDesktopFile(p, app)) {
                    seenIds.insert(id);
                    result.append(app);
                    break;
                }
            }
        }
    }

    // 2. Scan desktop files supporting this MIME type
    for (const QString &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        const QStringList entries = dir.entryList(QStringList() << "*.desktop", QDir::Files);
        for (const QString &entry : entries) {
            if (seenIds.contains(entry)) continue;

            QString fullPath = dir.absoluteFilePath(entry);
            AppInfo app;
            if (parseDesktopFile(fullPath, app)) {
                bool supports = false;
                for (const QString &m : allMimes) {
                    if (app.mimeTypes.contains(m)) {
                        supports = true;
                        break;
                    }
                }

                if (supports) {
                    seenIds.insert(entry);
                    result.append(app);
                }
            }
        }
    }

    return result;
}

QList<AppInfo> AppLauncher::getAllApps()
{
    QList<AppInfo> result;
    QStringList appDirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/share/applications",
        "/var/lib/flatpak/exports/share/applications",
        QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
    };

    QSet<QString> seenIds;

    for (const QString &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        const QStringList entries = dir.entryList(QStringList() << "*.desktop", QDir::Files);
        for (const QString &entry : entries) {
            if (seenIds.contains(entry)) continue;

            QString fullPath = dir.absoluteFilePath(entry);
            AppInfo app;
            if (parseDesktopFile(fullPath, app)) {
                seenIds.insert(entry);
                result.append(app);
            }
        }
    }

    // Sort alphabetically by name
    std::sort(result.begin(), result.end(), [](const AppInfo &a, const AppInfo &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return result;
}

bool AppLauncher::launchApp(const AppInfo &app, const QString &filePath)
{
    if (app.execCmd.isEmpty()) return false;

    QString nativeFilePath = QDir::toNativeSeparators(filePath);
    QString urlStr = QUrl::fromLocalFile(filePath).toString();

    QStringList args = QProcess::splitCommand(app.execCmd);
    if (args.isEmpty()) return false;

    QString program = args.takeFirst();
    bool replaced = false;

    for (int i = 0; i < args.size(); ++i) {
        if (args[i].contains("%f") || args[i].contains("%F")) {
            args[i].replace("%f", nativeFilePath).replace("%F", nativeFilePath);
            replaced = true;
        } else if (args[i].contains("%u") || args[i].contains("%U")) {
            args[i].replace("%u", urlStr).replace("%U", urlStr);
            replaced = true;
        } else if (args[i] == "%i" || args[i] == "%c" || args[i] == "%k" || args[i] == "%m") {
            args.removeAt(i--);
        }
    }

    if (!replaced && !filePath.isEmpty()) {
        args.append(nativeFilePath);
    }

    return QProcess::startDetached(program, args);
}
