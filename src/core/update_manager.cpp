/**
 * @file update_manager.cpp
 * @brief Implementation of background asynchronous update checker.
 */

#include "update_manager.h"
#include "build_config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>

UpdateManager &UpdateManager::instance()
{
    static UpdateManager s_instance;
    return s_instance;
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
{
}

#ifndef BUBEZVIEW_VERSION
#define BUBEZVIEW_VERSION "0.1.0"
#endif

QString UpdateManager::developerPublicKey()
{
    return QStringLiteral(
        "-----BEGIN PUBLIC KEY-----\n"
        "MCowBQYDK2VwAyEAfr3yn0QMgvYvEjW+fkfKAF8ZP4EWsn9DOrhtjHPEe1I=\n"
        "-----END PUBLIC KEY-----\n"
    );
}

bool UpdateManager::verifySignature(const QString &hashSha256, const QString &signatureB64)
{
    const QString cleanHash = hashSha256.trimmed().toLower();
    const QString cleanSig = signatureB64.trimmed();

    if (cleanHash.length() != 64 || cleanSig.isEmpty()) {
        return false;
    }

    const QByteArray sigBytes = QByteArray::fromBase64(cleanSig.toUtf8());
    if (sigBytes.size() != 64) {
        return false;
    }

    QTemporaryFile pubKeyFile;
    if (!pubKeyFile.open()) {
        return false;
    }
    pubKeyFile.write(developerPublicKey().toUtf8());
    pubKeyFile.flush();

    QTemporaryFile hashFile;
    if (!hashFile.open()) {
        return false;
    }
    hashFile.write(cleanHash.toUtf8());
    hashFile.flush();

    QTemporaryFile sigFile;
    if (!sigFile.open()) {
        return false;
    }
    sigFile.write(sigBytes);
    sigFile.flush();

    QProcess proc;
    QStringList args;
    args << QStringLiteral("pkeyutl")
         << QStringLiteral("-verify")
         << QStringLiteral("-rawin")
         << QStringLiteral("-pubin")
         << QStringLiteral("-inkey") << pubKeyFile.fileName()
         << QStringLiteral("-in") << hashFile.fileName()
         << QStringLiteral("-sigfile") << sigFile.fileName();

    proc.start(QStringLiteral("openssl"), args);
    if (!proc.waitForFinished(4000)) {
        proc.kill();
        return false;
    }

    return (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
}

bool UpdateManager::isVersionNewer(const QString &remoteVer, const QString &currentVer)
{
    if (remoteVer.trimmed().isEmpty()) return false;
    if (currentVer.trimmed().isEmpty()) return true;

    // Clean leading 'v' or 'V' if present
    QString rClean = remoteVer.trimmed();
    if (rClean.startsWith('v', Qt::CaseInsensitive)) {
        rClean = rClean.mid(1);
    }
    QString cClean = currentVer.trimmed();
    if (cClean.startsWith('v', Qt::CaseInsensitive)) {
        cClean = cClean.mid(1);
    }

    QStringList rParts = rClean.split('.', Qt::SkipEmptyParts);
    QStringList cParts = cClean.split('.', Qt::SkipEmptyParts);

    int maxParts = qMax(rParts.size(), cParts.size());
    for (int i = 0; i < maxParts; ++i) {
        int rNum = (i < rParts.size()) ? rParts[i].toInt() : 0;
        int cNum = (i < cParts.size()) ? cParts[i].toInt() : 0;

        if (rNum > cNum) {
            return true;
        } else if (rNum < cNum) {
            return false;
        }
    }

    return false;
}

QString UpdateManager::currentVersion() const
{
    return BUBEZVIEW_VERSION;
}

QString UpdateManager::findInstallerScript() const
{
    // Check known locations for C++ Installa or install.sh scripts
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/Installa",
        QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../Installa"),
        QCoreApplication::applicationDirPath() + "/install.sh",
        QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../install.sh"),
        QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../install.sh"),
        QDir::homePath() + "/.local/share/bubezview/install.sh"
    };

    const QString devRoot = BuildConfig::projectDevRoot();
    if (!devRoot.isEmpty()) {
        candidates << devRoot + "/install.sh";
        candidates << devRoot + "/bubezview_cpp/Installa";
        candidates << devRoot + "/Installa";
    }

    for (const QString &path : candidates) {
        QFileInfo fi(path);
        if (fi.exists() && fi.isExecutable()) {
            return fi.canonicalFilePath();
        }
    }
    return QString();
}

void UpdateManager::checkForUpdates(bool silent)
{
    QNetworkRequest request(QUrl("https://dawlinshare.web.app/versioni.json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/" + currentVersion());
    request.setTransferTimeout(5000);

    QNetworkReply *reply = m_networkManager.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (!silent) {
                emit checkFailed(reply->errorString());
            }
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            if (!silent) {
                emit checkFailed(tr("Received invalid response from update server."));
            }
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonObject bubezObj = rootObj.value("bubezview").toObject();
        if (bubezObj.isEmpty()) {
            if (!silent) {
                emit checkFailed(tr("BubezView entry not found in version registry."));
            }
            return;
        }

        QString remoteVer = bubezObj.value("versione").toString(bubezObj.value("version").toString());
        QString downloadUrl = bubezObj.value("link_download").toString(bubezObj.value("download_url").toString());
        if (downloadUrl.isEmpty()) {
            downloadUrl = bubezObj.value("link_download_source").toString();
        }
        QString changelog = bubezObj.value("changelog").toString(tr("Bug fixes and performance improvements."));
        QString sha256 = bubezObj.value("sha256").toString().trimmed().toLower();
        QString signature = bubezObj.value("signature").toString().trimmed();

        bool isAuthentic = false;
        if (!signature.isEmpty()) {
            if (sha256.isEmpty()) {
                if (!silent) {
                    emit checkFailed(tr("Security alert: Signature is present but SHA-256 hash is missing. Update blocked."));
                }
                return;
            }

            isAuthentic = verifySignature(sha256, signature);
            if (!isAuthentic) {
                if (!silent) {
                    emit checkFailed(tr("Security alert: Update payload failed cryptographic signature verification! Update blocked."));
                }
                return;
            }
        }

        if (isVersionNewer(remoteVer, currentVersion())) {
            ReleaseInfo info;
            info.version = remoteVer;
            info.changelog = changelog;
            info.downloadUrl = downloadUrl;
            info.sha256 = sha256;
            info.signature = signature;
            info.isAuthentic = isAuthentic;
            emit updateAvailable(info);
        } else {
            emit alreadyUpToDate();
        }
    });
}
