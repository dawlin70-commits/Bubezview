#ifndef OCR_MANAGER_H
#define OCR_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief Manages Tesseract OCR engine integration, language packs, and background model downloads.
 *
 * OcrManager provides a unified interface for detecting installed Tesseract language models,
 * storing user-space traineddata files in XDG data directories (~/.local/share/bubezview/tessdata),
 * downloading missing language models directly from GitHub (tessdata_fast), and executing
 * text extraction tasks.
 */
class OcrManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Structure containing language metadata.
     */
    struct LanguageInfo {
        QString code;        ///< ISO / Tesseract language code (e.g., "ita", "eng", "fra")
        QString englishName; ///< Full English name (e.g., "Italian", "English")
        QString nativeName;  ///< Native language name (e.g., "Italiano", "English")
        bool isInstalled;    ///< Whether the traineddata model is locally available
        qint64 fileSizeBytes;///< Size of the traineddata file if available
    };

    /**
     * @brief Gets the singleton instance of OcrManager.
     * @return Reference to the global OcrManager instance.
     */
    static OcrManager &instance();

    /**
     * @brief Returns the path to the user's local tessdata directory.
     * @return Absolute path to the user's tessdata folder.
     */
    QString tessdataDirectory() const;

    /**
     * @brief Returns the path to the system-wide tessdata directory.
     * @return System directory path (typically /usr/share/tessdata).
     */
    QString systemTessdataDirectory() const;

    /**
     * @brief Ensures that the user's local tessdata directory exists and links system models.
     * @return True if initialized successfully.
     */
    bool initTessdataDir();

    /**
     * @brief Returns whether Tesseract executable is present on the system PATH.
     * @return True if tesseract is available.
     */
    bool isTesseractInstalled() const;

    /**
     * @brief Returns the full path to the tesseract executable.
     * @return Executable path or empty string if not found.
     */
    QString tesseractExecutablePath() const;

    /**
     * @brief Returns all installed language codes available for OCR.
     * @return Sorted list of language codes (e.g. ["afr", "eng", "ita"]).
     */
    QStringList installedLanguages();

    /**
     * @brief Checks if a specific language model is installed.
     * @param langCode 3-letter Tesseract code (e.g. "ita", "eng").
     * @return True if installed.
     */
    bool isLanguageInstalled(const QString &langCode);

    /**
     * @brief Detects the recommended Tesseract language code matching the system locale.
     * @return 3-letter code (e.g. "ita" for Italian, "eng" for English, "deu" for German).
     */
    QString detectSystemLanguageCode() const;

    /**
     * @brief Returns human-readable metadata for a given language code.
     * @param langCode 3-letter code.
     * @return LanguageInfo struct.
     */
    LanguageInfo languageInfo(const QString &langCode) const;

    /**
     * @brief Returns a curated catalogue of popular world languages supported by Tesseract.
     * @return List of LanguageInfo structs.
     */
    QList<LanguageInfo> popularLanguagesCatalog();

    /**
     * @brief Starts an asynchronous download of a language model from GitHub tessdata_fast.
     * @param langCode 3-letter code to download.
     */
    void downloadLanguage(const QString &langCode);

    /**
     * @brief Performs a synchronous download of a language model with optional progress callback.
     * @param langCode 3-letter code to download.
     * @param progressCallback Optional callback function taking (receivedBytes, totalBytes).
     * @param outError Optional pointer to store error message if download fails.
     * @return True if downloaded and verified successfully.
     */
    bool downloadLanguageBlocking(const QString &langCode,
                                  std::function<void(qint64, qint64)> progressCallback = nullptr,
                                  QString *outError = nullptr);

    /**
     * @brief Installs the system Tesseract package using the native package manager (apt, pacman, dnf, zypper).
     * Uses pkexec if running as unprivileged user, or direct execution if root.
     * @param parent Optional parent widget for progress dialog.
     * @param outError Optional pointer to store error message if installation fails.
     * @return True if installation succeeded.
     */
    bool installTesseractSystemPackage(QWidget *parent = nullptr, QString *outError = nullptr);

    /**
     * @brief Executes Tesseract OCR on a given image file.
     * @param imagePath Absolute path to the input image.
     * @param langCode Language specification (e.g. "ita", "eng", or "ita+eng").
     * @param outText Output string to store extracted text.
     * @param outError Output string to store error diagnostics if execution fails.
     * @return True if OCR succeeded.
     */
    bool runOcr(const QString &imagePath, const QString &langCode, QString &outText, QString &outError);

signals:
    /**
     * @brief Emitted periodically during language model download.
     * @param langCode Code of the language being downloaded.
     * @param bytesReceived Number of bytes received so far.
     * @param bytesTotal Total number of bytes to receive (-1 if unknown).
     */
    void downloadProgress(const QString &langCode, qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief Emitted when a language model download finishes.
     * @param langCode Code of the language.
     * @param success True if the download succeeded and file was verified.
     * @param errorMessage Error description if unsuccessful.
     */
    void downloadFinished(const QString &langCode, bool success, const QString &errorMessage);

    /**
     * @brief Emitted when the list of available languages changes (e.g. after a download).
     */
    void languagesUpdated();

private:
    explicit OcrManager(QObject *parent = nullptr);
    ~OcrManager() override = default;
    OcrManager(const OcrManager &) = delete;
    OcrManager &operator=(const OcrManager &) = delete;

    void populateCatalog();

    std::unique_ptr<QNetworkAccessManager> m_netManager;
    QMap<QString, LanguageInfo> m_catalog;
};

#endif // OCR_MANAGER_H
