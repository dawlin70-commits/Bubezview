#include "ocr_manager.h"
#include "build_config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QProgressDialog>
#include <QApplication>
#include <QDebug>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <vector>
#include <unistd.h>

OcrManager &OcrManager::instance()
{
    static OcrManager s_instance;
    return s_instance;
}

OcrManager::OcrManager(QObject *parent)
    : QObject(parent)
    , m_netManager(std::make_unique<QNetworkAccessManager>(this))
{
    populateCatalog();
    initTessdataDir();
}

QString OcrManager::tessdataDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.local/share/bubezview";
    }
    return base + "/tessdata";
}

QString OcrManager::systemTessdataDirectory() const
{
    return QStringLiteral("/usr/share/tessdata");
}

bool OcrManager::initTessdataDir()
{
    QDir dir(tessdataDirectory());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create tessdata directory:" << tessdataDirectory();
            return false;
        }
    }
    return true;
}

bool OcrManager::isTesseractInstalled() const
{
    return !tesseractExecutablePath().isEmpty();
}

QString OcrManager::tesseractExecutablePath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString localApp = appDir + "/tesseract";
    if (QFile::exists(localApp) && QFileInfo(localApp).isExecutable()) return localApp;
    const QString localBin = appDir + "/bin/tesseract";
    if (QFile::exists(localBin) && QFileInfo(localBin).isExecutable()) return localBin;
    const QString homeBin = QDir::homePath() + "/.local/bin/tesseract";
    if (QFile::exists(homeBin) && QFileInfo(homeBin).isExecutable()) return homeBin;

    const QString projectRoot = BuildConfig::projectDevRoot();
    if (!projectRoot.isEmpty()) {
        const QString containerBin = projectRoot + "/container/.local/bin/tesseract";
        if (QFile::exists(containerBin) && QFileInfo(containerBin).isExecutable()) return containerBin;
    }

    return QStandardPaths::findExecutable("tesseract");
}

QStringList OcrManager::installedLanguages()
{
    initTessdataDir();
    QSet<QString> langs;

    QDir d(tessdataDirectory());
    if (d.exists()) {
        const QStringList files = d.entryList(QStringList() << "*.traineddata", QDir::Files);
        for (const QString &f : files) {
            QString code = f.section('.', 0, 0);
            if (!code.isEmpty() && code != "osd") {
                langs.insert(code);
            }
        }
    }

    QStringList result = langs.values();
    result.sort();
    return result;
}

bool OcrManager::isLanguageInstalled(const QString &langCode)
{
    if (langCode.isEmpty()) return false;
    QString localFile = tessdataDirectory() + "/" + langCode + ".traineddata";
    return QFile::exists(localFile);
}

QString OcrManager::detectSystemLanguageCode() const
{
    QString sysLang = QLocale::system().name().toLower();
    QString langPrefix = sysLang.split('_').first();

    static const QMap<QString, QString> s_map = {
        {"it", "ita"},
        {"en", "eng"},
        {"es", "spa"},
        {"de", "deu"},
        {"fr", "fra"},
        {"pt", "por"},
        {"ru", "rus"},
        {"zh", "chi_sim"},
        {"ja", "jpn"},
        {"nl", "nld"},
        {"pl", "pol"},
        {"uk", "ukr"},
        {"ar", "ara"},
        {"hi", "hin"},
        {"el", "ell"},
        {"tr", "tur"},
        {"ro", "ron"},
        {"sv", "swe"},
        {"da", "dan"},
        {"fi", "fin"},
        {"cs", "ces"},
        {"hu", "hun"}
    };

    return s_map.value(langPrefix, "eng");
}

void OcrManager::populateCatalog()
{
    m_catalog.clear();
    auto add = [this](const QString &code, const QString &en, const QString &native) {
        LanguageInfo info;
        info.code = code;
        info.englishName = en;
        info.nativeName = native;
        info.isInstalled = false;
        info.fileSizeBytes = 0;
        m_catalog.insert(code, info);
    };

    add("ita", "Italian", "Italiano");
    add("eng", "English", "English");
    add("spa", "Spanish", "Español");
    add("deu", "German", "Deutsch");
    add("fra", "French", "Français");
    add("por", "Portuguese", "Português");
    add("rus", "Russian", "Русский");
    add("chi_sim", "Chinese (Simplified)", "简体中文");
    add("chi_tra", "Chinese (Traditional)", "繁體中文");
    add("jpn", "Japanese", "日本語");
    add("nld", "Dutch", "Nederlands");
    add("pol", "Polish", "Polski");
    add("ukr", "Ukrainian", "Українська");
    add("ara", "Arabic", "العربية");
    add("hin", "Hindi", "हिन्दी");
    add("ell", "Greek", "Ελληνικά");
    add("tur", "Turkish", "Türkçe");
    add("ron", "Romanian", "Română");
    add("swe", "Swedish", "Svenska");
    add("dan", "Danish", "Dansk");
    add("fin", "Finnish", "Suomi");
    add("ces", "Czech", "Čeština");
    add("hun", "Hungarian", "Magyar");
}

OcrManager::LanguageInfo OcrManager::languageInfo(const QString &langCode) const
{
    if (m_catalog.contains(langCode)) {
        LanguageInfo info = m_catalog.value(langCode);
        QString localFile = tessdataDirectory() + "/" + langCode + ".traineddata";
        if (QFile::exists(localFile)) {
            info.isInstalled = true;
            info.fileSizeBytes = QFileInfo(localFile).size();
        }
        return info;
    }

    LanguageInfo custom;
    custom.code = langCode;
    custom.englishName = langCode.toUpper();
    custom.nativeName = langCode.toUpper();
    QString localFile = tessdataDirectory() + "/" + langCode + ".traineddata";
    if (QFile::exists(localFile)) {
        custom.isInstalled = true;
        custom.fileSizeBytes = QFileInfo(localFile).size();
    }
    return custom;
}

QList<OcrManager::LanguageInfo> OcrManager::popularLanguagesCatalog()
{
    QList<LanguageInfo> list;
    const QStringList installed = installedLanguages();

    for (auto it = m_catalog.begin(); it != m_catalog.end(); ++it) {
        LanguageInfo info = it.value();
        info.isInstalled = installed.contains(info.code);
        list.append(info);
    }
    return list;
}

void OcrManager::downloadLanguage(const QString &langCode)
{
    if (langCode.isEmpty()) return;
    initTessdataDir();

    QUrl url(QString("https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/%1.traineddata").arg(langCode));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_netManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, langCode](qint64 rec, qint64 total) {
        emit downloadProgress(langCode, rec, total);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, langCode]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFinished(langCode, false, reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        if (data.size() < 1024) { // Suspiciously small (probably a 404 text response)
            emit downloadFinished(langCode, false, tr("Received invalid data from server."));
            return;
        }

        QString targetPath = tessdataDirectory() + "/" + langCode + ".traineddata";
        QFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFinished(langCode, false, tr("Failed to write to file: %1").arg(targetPath));
            return;
        }

        file.write(data);
        file.close();

        emit downloadFinished(langCode, true, QString());
        emit languagesUpdated();
    });
}

bool OcrManager::downloadLanguageBlocking(const QString &langCode,
                                          std::function<void(qint64, qint64)> progressCallback,
                                          QString *outError)
{
    if (langCode.isEmpty()) return false;
    initTessdataDir();

    QUrl url(QString("https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/%1.traineddata").arg(langCode));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkAccessManager mgr;
    QNetworkReply *reply = mgr.get(request);

    if (progressCallback) {
        connect(reply, &QNetworkReply::downloadProgress, [progressCallback](qint64 rec, qint64 tot) {
            progressCallback(rec, tot);
        });
    }

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (outError) *outError = reply->errorString();
        reply->deleteLater();
        return false;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.size() < 1024) {
        if (outError) *outError = tr("File received from repository is invalid or not found.");
        return false;
    }

    QString targetPath = tessdataDirectory() + "/" + langCode + ".traineddata";
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outError) *outError = tr("Unable to save model file: %1").arg(file.errorString());
        return false;
    }

    file.write(data);
    file.close();

    emit languagesUpdated();
    return true;
}

bool OcrManager::installTesseractSystemPackage(QWidget *parent, QString *outError)
{
    QString cmd;
    if (!QStandardPaths::findExecutable("apt-get").isEmpty()) {
        cmd = "apt-get update && apt-get install -y tesseract-ocr";
    } else if (!QStandardPaths::findExecutable("pacman").isEmpty()) {
        cmd = "pacman -Sy --noconfirm tesseract";
    } else if (!QStandardPaths::findExecutable("dnf").isEmpty()) {
        cmd = "dnf install -y tesseract";
    } else if (!QStandardPaths::findExecutable("zypper").isEmpty()) {
        cmd = "zypper --non-interactive in tesseract-ocr";
    } else {
        if (outError) *outError = tr("No supported package manager found (apt, pacman, dnf, zypper).");
        return false;
    }

    QString program;
    QStringList args;

    if (geteuid() == 0) {
        program = "/bin/sh";
        args << "-c" << cmd;
    } else if (!QStandardPaths::findExecutable("pkexec").isEmpty()) {
        program = "pkexec";
        args << "/bin/sh" << "-c" << cmd;
    } else {
        if (outError) *outError = tr("Root privileges required but pkexec is not available.");
        return false;
    }

    QProgressDialog progress(tr("Installing Tesseract OCR engine..."), QString(), 0, 0, parent);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QApplication::processEvents();

    QProcess proc;
    proc.start(program, args);
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForFinished(100);
        QApplication::processEvents();
    }

    progress.close();

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err.isEmpty()) {
            err = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }
        if (outError) *outError = err;
        return false;
    }

    return isTesseractInstalled();
}

bool OcrManager::runOcr(const QString &imagePath, const QString &langCode, QString &outText, QString &outError, int threadLimit)
{
    outText.clear();
    outError.clear();

    QString tesseract = tesseractExecutablePath();
    if (tesseract.isEmpty()) {
        outError = tr("Tesseract OCR executable not found on system.");
        return false;
    }

    if (!QFile::exists(imagePath)) {
        outError = tr("Input image file not found: %1").arg(imagePath);
        return false;
    }

    initTessdataDir();

    QStringList args;
    args << imagePath << "stdout";

    // Use local tessdata directory if available
    QString localTessDir = tessdataDirectory();
    if (QDir(localTessDir).exists()) {
        args << "--tessdata-dir" << localTessDir;
    }

    args << "-l" << (langCode.isEmpty() ? "eng" : langCode);

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (threadLimit > 0) {
        // Explicit thread limit (used in parallel batch processing to prevent thread oversaturation)
        env.insert("OMP_NUM_THREADS", QString::number(threadLimit));
        env.insert("OMP_THREAD_LIMIT", QString::number(threadLimit));
    } else {
        // Standalone image: leverage available CPU cores via OpenMP
        int ideal = QThread::idealThreadCount();
        int ompThreads = std::clamp(ideal > 0 ? ideal : 4, 1, 8);
        env.insert("OMP_NUM_THREADS", QString::number(ompThreads));
        env.insert("OMP_THREAD_LIMIT", QString::number(ompThreads));
        args << "-c" << "tessedit_parallelize=1";
    }
    proc.setProcessEnvironment(env);

    proc.start(tesseract, args);
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        outError = tr("Tesseract OCR process timed out (exceeded 30s).");
        return false;
    }

    if (proc.exitCode() != 0) {
        outError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return false;
    }

    outText = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return true;
}

QVector<OcrManager::PageOcrResult> OcrManager::runBatchOcr(const QStringList &imagePaths,
                                                           const QString &langCode,
                                                           std::function<void(int completed, int total)> progressCallback,
                                                           std::atomic<bool> *cancelFlag)
{
    const int total = imagePaths.size();
    QVector<PageOcrResult> results(total);
    if (total == 0) return results;

    for (int i = 0; i < total; ++i) {
        results[i].pageIndex = i;
    }

    int numCores = QThread::idealThreadCount();
    if (numCores <= 0) numCores = 4;
    // Scale worker count up to available CPU cores or total page count
    int workerCount = std::clamp(total, 1, numCores);

    std::atomic<int> nextIndex{0};
    std::atomic<int> completedCount{0};
    std::mutex progressMutex;

    auto workerFunc = [&]() {
        while (true) {
            if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) {
                break;
            }
            int idx = nextIndex.fetch_add(1);
            if (idx >= total) {
                break;
            }

            const QString &imgPath = imagePaths[idx];
            if (imgPath.isEmpty() || !QFile::exists(imgPath)) {
                results[idx].error = tr("Image file missing for page %1").arg(idx + 1);
                results[idx].success = false;
            } else {
                QString outText, outError;
                // When running concurrently across multiple CPU cores, limit each worker's
                // Tesseract process to 1 thread to maximize throughput without CPU core contention.
                bool ok = runOcr(imgPath, langCode, outText, outError, 1);
                results[idx].text = outText;
                results[idx].error = outError;
                results[idx].success = ok;
            }

            int done = completedCount.fetch_add(1) + 1;
            if (progressCallback) {
                std::lock_guard<std::mutex> lock(progressMutex);
                progressCallback(done, total);
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (int w = 0; w < workerCount; ++w) {
        workers.emplace_back(workerFunc);
    }

    for (auto &w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    return results;
}
