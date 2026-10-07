/**
 * @file update_manager.h
 * @brief Background asynchronous update checker and updater coordinator for BubezView.
 *
 * Checks remote release endpoints (GitHub / Web service) asynchronously without blocking
 * the main GUI thread or delaying the application startup time.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>

/**
 * @brief Structure holding release metadata retrieved from update servers.
 */
struct ReleaseInfo {
    QString version;     ///< Semantic version string, e.g. "1.1.0".
    QString changelog;   ///< Human-readable release notes or summary.
    QString downloadUrl; ///< Direct URL to release artifacts or installer script.
    QString sha256;      ///< Expected SHA-256 cryptographic digest of the release payload.
    QString signature;   ///< Base64-encoded Ed25519 digital signature of sha256 digest.
    bool isAuthentic = false; ///< True if cryptographically verified against developer's public key.
};

/**
 * @brief Manages background version checking and coordinates the update process.
 */
class UpdateManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Singleton instance accessor.
     * @return Shared UpdateManager reference.
     */
    static UpdateManager &instance();

    /**
     * @brief Initiates an asynchronous query to check for newer versions.
     * @param silent If true, suppresses warnings when network is unreachable.
     */
    void checkForUpdates(bool silent = true);

    /**
     * @brief Compares two semantic version strings (e.g. "0.1.3" vs "0.1.2").
     * @param remoteVer Remote version string.
     * @param currentVer Local version string.
     * @return True if remoteVer is strictly newer than currentVer.
     */
    static bool isVersionNewer(const QString &remoteVer, const QString &currentVer);

    /**
     * @brief Cryptographically verifies an SHA-256 hash using the developer's Ed25519 public key.
     * @param hashSha256 Lowercase hexadecimal SHA-256 string.
     * @param signatureB64 Base64-encoded digital signature string.
     * @return True if signature is valid and authentic, false otherwise.
     */
    static bool verifySignature(const QString &hashSha256, const QString &signatureB64);

    /**
     * @brief Returns the embedded developer Ed25519 public key in PEM format.
     * @return PEM public key string.
     */
    static QString developerPublicKey();

    /**
     * @brief Returns the currently installed application version.
     * @return Semantic version string (e.g. "1.0.0").
     */
    QString currentVersion() const;

    /**
     * @brief Returns the path to the BubezView installer script if present.
     * @return Absolute file path to install.sh, or empty string if not found.
     */
    QString findInstallerScript() const;

signals:
    /**
     * @brief Emitted when a newer release is discovered on the server.
     * @param info Detailed release information including version and notes.
     */
    void updateAvailable(const ReleaseInfo &info);

    /**
     * @brief Emitted when the check completes and the application is already up to date.
     */
    void alreadyUpToDate();

    /**
     * @brief Emitted when the version check fails due to network or JSON parsing error.
     * @param errorMsg User-friendly error message.
     */
    void checkFailed(const QString &errorMsg);

private:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override = default;

    UpdateManager(const UpdateManager &) = delete;
    UpdateManager &operator=(const UpdateManager &) = delete;

    QNetworkAccessManager m_networkManager;
};
