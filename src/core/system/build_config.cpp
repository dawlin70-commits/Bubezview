/**
 * @file build_config.cpp
 * @brief Implementation of persistent build configuration manager.
 */

#include "build_config.h"
#include "package_manager_helper.h"
#include "core/services/plugin_manager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QImageReader>

BuildConfig &BuildConfig::instance()
{
    static BuildConfig s_instance;
    return s_instance;
}

BuildConfig::BuildConfig()
    : m_optimizationLevel("O2")
    , m_cpuProfile("native")
    , m_libraryStrategy("dynamic")
    , m_staticPdf(false)
    , m_popplerVersion("26.08.0")
    , m_staticArchive(false)
#ifdef ENABLE_INTERNAL_OFFICE
    , m_enableInternalOffice(true)
#else
    , m_enableInternalOffice(false)
#endif
    , m_staticOffice(false)
{
    load();
}

QString BuildConfig::configFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/bubezview";
    return dataDir + "/build_config.json";
}

bool BuildConfig::load()
{
    QString path = configFilePath();
    QFile file(path);
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    m_optimizationLevel = obj.value("optimization_level").toString("O2");
    m_cpuProfile = obj.value("cpu_profile").toString("native");
    m_libraryStrategy = obj.value("library_strategy").toString("dynamic");
    m_customCompilerFlags = obj.value("custom_cxx_flags").toString();

    QJsonObject modules = obj.value("modules").toObject();
    m_staticPdf = modules.value("static_pdf").toBool(m_staticPdf);
    m_popplerVersion = modules.value("poppler_version").toString("26.08.0");
    m_staticArchive = modules.value("static_archive").toBool(m_staticArchive);
    m_enableInternalOffice = modules.value("enable_internal_office").toBool(false);
    m_staticOffice = modules.value("static_office").toBool(false);

    QJsonObject customSrcObj = obj.value("custom_sources").toObject();
    m_customSources.clear();
    for (auto it = customSrcObj.begin(); it != customSrcObj.end(); ++it) {
        QJsonObject sObj = it.value().toObject();
        CustomSourceConfig sc;
        sc.preset = sObj.value("preset").toString("stable");
        sc.gitUrl = sObj.value("git_url").toString();
        sc.branchTag = sObj.value("branch_tag").toString();
        sc.buildFlags = sObj.value("build_flags").toString();
        m_customSources.insert(it.key(), sc);
    }

    return true;
}

bool BuildConfig::save()
{
    QString path = configFilePath();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QJsonObject root;
    root["optimization_level"] = m_optimizationLevel;
    root["cpu_profile"] = m_cpuProfile;
    root["library_strategy"] = m_libraryStrategy;
    root["custom_cxx_flags"] = m_customCompilerFlags;

    QJsonObject modules;
    modules["static_pdf"] = m_staticPdf;
    modules["poppler_version"] = m_popplerVersion;
    modules["static_archive"] = m_staticArchive;
    modules["enable_internal_office"] = m_enableInternalOffice;
    modules["static_office"] = m_staticOffice;
    root["modules"] = modules;

    QJsonObject customSrcObj;
    for (auto it = m_customSources.begin(); it != m_customSources.end(); ++it) {
        QJsonObject sObj;
        sObj["preset"] = it.value().preset;
        sObj["git_url"] = it.value().gitUrl;
        sObj["branch_tag"] = it.value().branchTag;
        sObj["build_flags"] = it.value().buildFlags;
        customSrcObj[it.key()] = sObj;
    }
    root["custom_sources"] = customSrcObj;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QStringList BuildConfig::toCMakeArgs() const
{
    QStringList args;
    args << QString("-DOPTIMIZATION_LEVEL=%1").arg(m_optimizationLevel);
    args << QString("-DOPTIMIZE_FOR_NATIVE_CPU=%1").arg(m_cpuProfile == "native" ? "ON" : "OFF");
    args << QString("-DSTATIC_PDF=%1").arg(m_staticPdf ? "ON" : "OFF");
    if (m_staticPdf && !m_popplerVersion.isEmpty()) {
        args << QString("-DPOPPLER_VERSION=%1").arg(m_popplerVersion);
    }
    args << QString("-DSTATIC_ARCHIVE=%1").arg(m_staticArchive ? "ON" : "OFF");
    args << QString("-DENABLE_INTERNAL_OFFICE=%1").arg(m_enableInternalOffice ? "ON" : "OFF");
    args << QString("-DSTATIC_OFFICE=%1").arg(m_staticOffice ? "ON" : "OFF");
    if (!m_customCompilerFlags.trimmed().isEmpty()) {
        args << QString("-DCUSTOM_CXX_FLAGS=%1").arg(m_customCompilerFlags.trimmed());
        args << QString("-DCMAKE_CXX_FLAGS=%1").arg(m_customCompilerFlags.trimmed());
    }
    return args;
}

CustomSourceConfig BuildConfig::defaultSourceConfig(const QString &componentId)
{
    QString id = componentId.trimmed().toLower();
    CustomSourceConfig cfg;
    cfg.preset = "stable";

    if (id == "poppler" || id == "engine-poppler" || id == "poppler-qt6") {
        cfg.gitUrl = "https://gitlab.freedesktop.org/poppler/poppler.git";
        cfg.branchTag = "poppler-26.08.0";
        cfg.buildFlags = "-DENABLE_QT6=ON -DENABLE_QT5=OFF -DENABLE_GLIB=OFF -DENABLE_CPP=OFF -DENABLE_BOOST=OFF -DENABLE_UTILS=ON -DENABLE_NSS3=OFF -DENABLE_GPGME=OFF -DENABLE_LIBCURL=OFF -DBUILD_QT6_TESTS=OFF -DBUILD_CPP_TESTS=OFF -DBUILD_MANUAL_TESTS=OFF -DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe -include cstdint\"";
    } else if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") {
        cfg.gitUrl = "https://github.com/tesseract-ocr/tesseract.git";
        cfg.branchTag = "5.4.1";
        cfg.buildFlags = "-DBUILD_TRAINEDDATA=OFF -DBUILD_TRAINING_TOOLS=OFF -DBUILD_TESTS=OFF -DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe -include cstdint\"";
    } else if (id == "qpdf") {
        cfg.gitUrl = "https://github.com/qpdf/qpdf.git";
        cfg.branchTag = "v11.9.1";
        cfg.buildFlags = "-DBUILD_STATIC_LIBS=OFF -DBUILD_DOC=OFF -DREQUIRE_CRYPTO_OPENSSL=ON -DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -pipe -include cstdint\"";
    } else if (id == "7zip" || id == "7z" || id == "7zz") {
        cfg.gitUrl = "https://github.com/ip7z/7zip.git";
        cfg.branchTag = "24.08";
        cfg.buildFlags = "-O3 -march=native -mcrc32 -funroll-loops -falign-loops=32 -flto -fomit-frame-pointer -pipe";
    } else if (id == "zstd") {
        cfg.gitUrl = "https://github.com/facebook/zstd.git";
        cfg.branchTag = "v1.5.6";
        cfg.buildFlags = "-DZSTD_BUILD_PROGRAMS=ON -DZSTD_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -pipe\"";
    } else if (id == "xz") {
        cfg.gitUrl = "https://github.com/tukaani-project/xz.git";
        cfg.branchTag = "v5.6.2";
        cfg.buildFlags = "-DBUILD_TESTING=OFF -DCMAKE_C_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -falign-loops=32 -flto=auto -pipe\"";
    } else if (id == "libarchive" || id == "engine-libarchive" || id == "archive") {
        cfg.gitUrl = "https://github.com/libarchive/libarchive.git";
        cfg.branchTag = "v3.7.4";
        cfg.buildFlags = "-DENABLE_TEST=OFF -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_CAT=OFF -DCMAKE_C_FLAGS_RELEASE=\"-O3 -march=native -mcrc32 -fno-strict-aliasing -funroll-loops -falign-loops=32 -flto=auto -fomit-frame-pointer -fno-semantic-interposition -pipe\"";
    } else if (id == "librevenge" || id == "office" || id == "engine-office") {
        cfg.gitUrl = "https://git.code.sf.net/p/librevenge/git";
        cfg.branchTag = "master";
    } else if (id == "image-formats" || id == "qtimageformats" || id == "qt6-imageformats" || id == "qt6-image-formats-plugins") {
        cfg.gitUrl = "https://github.com/qt/qtimageformats.git";
        cfg.branchTag = "v6.8.0";
        cfg.buildFlags = "-DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -flto=auto -pipe\"";
    } else {
        // Safe default: leave URL empty so invalid components do not falsely inherit tesseract-ocr
        cfg.gitUrl = QString();
        cfg.branchTag = QStringLiteral("main");
        cfg.buildFlags = QStringLiteral("-DCMAKE_CXX_FLAGS_RELEASE=\"-O3 -march=native -funroll-loops -flto=auto -pipe\"");
    }
    return cfg;
}

CustomSourceConfig BuildConfig::customSourceConfig(const QString &componentId) const
{
    QString id = componentId.trimmed().toLower();
    if (m_customSources.contains(id)) {
        return m_customSources.value(id);
    }
    return defaultSourceConfig(id);
}

void BuildConfig::setCustomSourceConfig(const QString &componentId, const CustomSourceConfig &config)
{
    QString id = componentId.trimmed().toLower();
    m_customSources.insert(id, config);
    save();
}

void BuildConfig::applyHardwareProfile(HardwareProfile profile)
{
    QSettings settings("Bubez", "BubezView");

    switch (profile) {
    case HardwareProfile::SystemPackages: {
        // Level 1: System Packages (No Compilation / Zero build)
        m_cpuProfile = "standard";
        m_optimizationLevel = "O2";
        m_customCompilerFlags = "";
        settings.setValue("hardware_profile", "system_packages");

        const QStringList engines = {
            "engine-poppler", "poppler", "pdfimages", "poppler-data",
            "engine-libarchive", "libarchive", "qpdf",
            "tesseract", "tessdata", "tesseract-ocr-ita", "engine-office", "librevenge", "mpv", "engine-mpv",
            "7zip", "image-formats", "zstd", "xz"
        };
        for (const QString &eng : engines) {
            settings.setValue("component_mode_" + eng, "system");
        }
        m_staticPdf = false;
        m_staticArchive = false;
        m_enableInternalOffice = false;

        settings.setValue("prefetch_distance", 1);
        settings.setValue("cache_budget_mb", 256);
        break;
    }
    case HardwareProfile::MissingOnly: {
        // Level 2: Missing Components Only
        m_cpuProfile = "native";
        m_optimizationLevel = "O3";
        m_customCompilerFlags = "";
        settings.setValue("hardware_profile", "missing_only");

        const QStringList engines = {
            "engine-poppler", "poppler", "pdfimages", "poppler-data",
            "engine-libarchive", "libarchive", "qpdf",
            "tesseract", "tessdata", "tesseract-ocr-ita", "engine-office", "librevenge", "mpv", "engine-mpv",
            "7zip", "image-formats", "zstd", "xz"
        };
        for (const QString &eng : engines) {
            bool hasSys = hasSystemBinary(eng);
            settings.setValue("component_mode_" + eng, hasSys ? "system" : "source");
        }
        m_staticPdf = !hasSystemBinary("poppler-qt6");
        m_staticArchive = !hasSystemBinary("libarchive");
        m_enableInternalOffice = false;
        settings.setValue("component_mode_engine-office", "system");

        settings.setValue("prefetch_distance", 2);
        settings.setValue("cache_budget_mb", 384);
        break;
    }
    case HardwareProfile::AllStable: {
        // Level 3: All Stable Components from source
        m_cpuProfile = "native";
        m_optimizationLevel = "O3";
        m_customCompilerFlags = "";
        settings.setValue("hardware_profile", "all_stable");

        const QStringList engines = {
            "engine-poppler", "poppler", "pdfimages", "poppler-data",
            "engine-libarchive", "libarchive", "qpdf",
            "tesseract", "tessdata", "tesseract-ocr-ita", "7zip", "image-formats", "zstd", "xz"
        };
        for (const QString &eng : engines) {
            settings.setValue("component_mode_" + eng, "source");
        }
        m_staticPdf = true;
        m_staticArchive = true;
        m_enableInternalOffice = false;
        settings.setValue("component_mode_engine-office", "system");

        settings.setValue("prefetch_distance", 3);
        settings.setValue("cache_budget_mb", 512);
        break;
    }
    case HardwareProfile::AllSource: {
        // Level 4: All From Source (Including Unrecommended)
        m_cpuProfile = "native";
        m_optimizationLevel = "O3";
        m_customCompilerFlags = "";
        settings.setValue("hardware_profile", "all_source");

        const QStringList engines = {
            "engine-poppler", "poppler", "pdfimages", "poppler-data",
            "engine-libarchive", "libarchive", "qpdf",
            "tesseract", "tessdata", "tesseract-ocr-ita", "engine-office", "librevenge", "mpv", "engine-mpv",
            "7zip", "image-formats", "zstd", "xz"
        };
        for (const QString &eng : engines) {
            settings.setValue("component_mode_" + eng, "source");
        }
        m_staticPdf = true;
        m_staticArchive = true;
        m_enableInternalOffice = true;

        settings.setValue("prefetch_distance", 3);
        settings.setValue("cache_budget_mb", 512);
        break;
    }
    case HardwareProfile::Custom:
        settings.setValue("hardware_profile", "custom");
        break;
    }

    settings.sync();
    save();
}

BuildConfig::HardwareProfile BuildConfig::activeHardwareProfile() const
{
    if (!m_customCompilerFlags.trimmed().isEmpty()) {
        return HardwareProfile::Custom;
    }

    QSettings settings("Bubez", "BubezView");
    QString hw = settings.value("hardware_profile", "").toString();
    if (hw == QLatin1String("system_packages") || hw == QLatin1String("system_only")) {
        return HardwareProfile::SystemPackages;
    }
    if (hw == QLatin1String("missing_only") || hw == QLatin1String("balanced")) {
        return HardwareProfile::MissingOnly;
    }
    if (hw == QLatin1String("all_stable") || hw == QLatin1String("desktop") || hw == QLatin1String("high_performance")) {
        return HardwareProfile::AllStable;
    }
    if (hw == QLatin1String("all_source")) {
        return HardwareProfile::AllSource;
    }

    // Auto-detection fallback:
    if (m_enableInternalOffice) {
        return HardwareProfile::AllSource;
    }
    if (m_staticPdf || m_staticArchive) {
        return HardwareProfile::AllStable;
    }
    return HardwareProfile::MissingOnly;
}

static QMap<QString, bool> s_pkgInstalledCache;

void BuildConfig::clearPackageCache()
{
    s_pkgInstalledCache.clear();
}

bool BuildConfig::isPkgInstalled(const QString &pkgName)
{
    if (pkgName.isEmpty()) return true;

    if (s_pkgInstalledCache.contains(pkgName)) {
        return s_pkgInstalledCache.value(pkgName);
    }

    QProcess proc;
    QStringList args;
    args << "--exists" << pkgName;
    proc.start("pkg-config", args);
    if (!proc.waitForFinished(1000)) {
        s_pkgInstalledCache.insert(pkgName, false);
        return false;
    }
    bool installed = (proc.exitCode() == 0);
    s_pkgInstalledCache.insert(pkgName, installed);
    return installed;
}

QString BuildConfig::findExecutable(const QString &exeName)
{
    if (exeName.isEmpty()) return QString();

    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFile::exists(appDir + "/" + exeName) && QFileInfo(appDir + "/" + exeName).isExecutable()) {
        return appDir + "/" + exeName;
    }
    if (QFile::exists(appDir + "/bin/" + exeName) && QFileInfo(appDir + "/bin/" + exeName).isExecutable()) {
        return appDir + "/bin/" + exeName;
    }

    const QString devRoot = projectDevRoot();
    if (!devRoot.isEmpty()) {
        const QString contBin = devRoot + "/container/.local/bin/" + exeName;
        if (QFile::exists(contBin) && QFileInfo(contBin).isExecutable()) {
            return contBin;
        }
    }

    const QString home = QDir::homePath();
    const QString homeBin = home + "/.local/bin/" + exeName;
    if (QFile::exists(homeBin) && QFileInfo(homeBin).isExecutable()) {
        return homeBin;
    }

    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");
    if (!hostHome.isEmpty()) {
        const QString hostBin = hostHome + "/.local/bin/" + exeName;
        if (QFile::exists(hostBin) && QFileInfo(hostBin).isExecutable()) {
            return hostBin;
        }
        const QString hostContBin = hostHome + "/Progetti/BubezView/container/.local/bin/" + exeName;
        if (QFile::exists(hostContBin) && QFileInfo(hostContBin).isExecutable()) {
            return hostContBin;
        }
    }

    const QString sysFound = QStandardPaths::findExecutable(exeName);
    if (!sysFound.isEmpty()) {
        return sysFound;
    }

    for (const QString &dir : {QStringLiteral("/usr/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/bin")}) {
        const QString p = dir + "/" + exeName;
        if (QFile::exists(p) && QFileInfo(p).isExecutable()) {
            return p;
        }
    }

    return QString();
}

bool BuildConfig::isExecutableAvailable(const QString &exeName)
{
    return !findExecutable(exeName).isEmpty();
}

QString BuildConfig::qpdfExecutable()
{
    ComponentSourceMode mode = instance().componentSourceMode("qpdf");
    const QString home = QDir::homePath();
    const QString projectRoot = projectDevRoot();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");

    QStringList sourceCandidates = {
        home + "/.local/bin/qpdf",
        home + "/Progetti/BubezView/container/.local/bin/qpdf"
    };
    if (!projectRoot.isEmpty()) {
        sourceCandidates.prepend(projectRoot + "/container/.local/bin/qpdf");
    }
    if (!hostHome.isEmpty()) {
        sourceCandidates.append(hostHome + "/.local/bin/qpdf");
        sourceCandidates.append(hostHome + "/Progetti/BubezView/container/.local/bin/qpdf");
    }

    QStringList systemCandidates = {
        "/usr/bin/qpdf",
        "/usr/local/bin/qpdf",
        "/bin/qpdf"
    };

    if (mode == ComponentSourceMode::LocalSource) {
        for (const QString &srcPath : sourceCandidates) {
            if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
                return srcPath;
            }
        }
        for (const QString &sysPath : systemCandidates) {
            if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
                return sysPath;
            }
        }
        return QStandardPaths::findExecutable("qpdf");
    }

    // Default or System mode: use official OS package binary
    for (const QString &sysPath : systemCandidates) {
        if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
            return sysPath;
        }
    }
    QString found = QStandardPaths::findExecutable("qpdf");
    if (!found.isEmpty()) return found;

    // Fallback to source binary if system binary was not found
    for (const QString &srcPath : sourceCandidates) {
        if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
            return srcPath;
        }
    }
    return QString();
}

bool BuildConfig::isQpdfAvailable()
{
    return !qpdfExecutable().isEmpty();
}

bool BuildConfig::isTesseractAvailable()
{
    return isExecutableAvailable("tesseract");
}

QString BuildConfig::pdfimagesExecutable()
{
    ComponentSourceMode mode = instance().componentSourceMode("pdfimages");
    const QString home = QDir::homePath();
    const QString projectRoot = projectDevRoot();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");

    QStringList sourceCandidates = {
        home + "/.local/bin/pdfimages",
        home + "/Progetti/BubezView/container/.local/bin/pdfimages"
    };
    if (!projectRoot.isEmpty()) {
        sourceCandidates.prepend(projectRoot + "/container/.local/bin/pdfimages");
    }
    if (!hostHome.isEmpty()) {
        sourceCandidates.append(hostHome + "/.local/bin/pdfimages");
        sourceCandidates.append(hostHome + "/Progetti/BubezView/container/.local/bin/pdfimages");
    }

    QStringList systemCandidates = {
        "/usr/bin/pdfimages",
        "/usr/local/bin/pdfimages",
        "/bin/pdfimages"
    };

    if (mode == ComponentSourceMode::LocalSource) {
        for (const QString &srcPath : sourceCandidates) {
            if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
                return srcPath;
            }
        }
        for (const QString &sysPath : systemCandidates) {
            if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
                return sysPath;
            }
        }
        return QStandardPaths::findExecutable("pdfimages");
    }

    for (const QString &sysPath : systemCandidates) {
        if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
            return sysPath;
        }
    }
    QString found = QStandardPaths::findExecutable("pdfimages");
    if (!found.isEmpty()) return found;

    for (const QString &srcPath : sourceCandidates) {
        if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
            return srcPath;
        }
    }
    return QString();
}

bool BuildConfig::isPdfimagesAvailable()
{
    return !pdfimagesExecutable().isEmpty();
}

QString BuildConfig::pdfuniteExecutable()
{
    ComponentSourceMode mode = instance().componentSourceMode("pdfimages");
    const QString home = QDir::homePath();
    const QString projectRoot = projectDevRoot();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");

    QStringList sourceCandidates = {
        home + "/.local/bin/pdfunite",
        home + "/Progetti/BubezView/container/.local/bin/pdfunite"
    };
    if (!projectRoot.isEmpty()) {
        sourceCandidates.prepend(projectRoot + "/container/.local/bin/pdfunite");
    }
    if (!hostHome.isEmpty()) {
        sourceCandidates.append(hostHome + "/.local/bin/pdfunite");
        sourceCandidates.append(hostHome + "/Progetti/BubezView/container/.local/bin/pdfunite");
    }

    QStringList systemCandidates = {
        "/usr/bin/pdfunite",
        "/usr/local/bin/pdfunite",
        "/bin/pdfunite"
    };

    if (mode == ComponentSourceMode::LocalSource) {
        for (const QString &srcPath : sourceCandidates) {
            if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
                return srcPath;
            }
        }
        for (const QString &sysPath : systemCandidates) {
            if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
                return sysPath;
            }
        }
        return QStandardPaths::findExecutable("pdfunite");
    }

    for (const QString &sysPath : systemCandidates) {
        if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
            return sysPath;
        }
    }
    QString found = QStandardPaths::findExecutable("pdfunite");
    if (!found.isEmpty()) return found;

    for (const QString &srcPath : sourceCandidates) {
        if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
            return srcPath;
        }
    }
    return QString();
}

bool BuildConfig::isPdfuniteAvailable()
{
    return !pdfuniteExecutable().isEmpty();
}

QString BuildConfig::pdfsigExecutable()
{
    ComponentSourceMode mode = instance().componentSourceMode("pdfimages");
    const QString devRoot = projectDevRoot();
    const QString home = QDir::homePath();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");

    QStringList sourceCandidates = {
        home + "/.local/bin/pdfsig",
        home + "/Progetti/BubezView/container/.local/bin/pdfsig"
    };
    if (!devRoot.isEmpty()) {
        sourceCandidates.prepend(devRoot + "/container/.local/bin/pdfsig");
    }
    if (!hostHome.isEmpty()) {
        sourceCandidates.append(hostHome + "/.local/bin/pdfsig");
        sourceCandidates.append(hostHome + "/Progetti/BubezView/container/.local/bin/pdfsig");
    }

    QStringList systemCandidates = {
        "/usr/bin/pdfsig",
        "/usr/local/bin/pdfsig",
        "/bin/pdfsig"
    };

    if (mode == ComponentSourceMode::LocalSource) {
        for (const QString &srcPath : sourceCandidates) {
            if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
                return srcPath;
            }
        }
        for (const QString &sysPath : systemCandidates) {
            if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
                return sysPath;
            }
        }
        return QStandardPaths::findExecutable("pdfsig");
    }

    for (const QString &sysPath : systemCandidates) {
        if (QFile::exists(sysPath) && QFileInfo(sysPath).isExecutable()) {
            return sysPath;
        }
    }
    QString found = QStandardPaths::findExecutable("pdfsig");
    if (!found.isEmpty()) return found;

    for (const QString &srcPath : sourceCandidates) {
        if (QFile::exists(srcPath) && QFileInfo(srcPath).isExecutable()) {
            return srcPath;
        }
    }
    return QString();
}

bool BuildConfig::isPdfsigAvailable()
{
    return !pdfsigExecutable().isEmpty();
}

QString BuildConfig::opensslExecutable()
{
    QString exe = findExecutable("openssl");
    return exe.isEmpty() ? QStringLiteral("openssl") : exe;
}

QString BuildConfig::sevenZipExecutable()
{
    ComponentSourceMode mode = instance().componentSourceMode("7zip");
    if (mode == ComponentSourceMode::LocalSource) {
        for (const QString &binName : {QStringLiteral("7zz"), QStringLiteral("7z")}) {
            const QString homeBin = QDir::homePath() + "/.local/bin/" + binName;
            if (QFile::exists(homeBin) && QFileInfo(homeBin).isExecutable()) {
                return homeBin;
            }
            const QString projectRoot = QDir::currentPath();
            const QString containerBin = projectRoot + "/container/.local/bin/" + binName;
            if (QFile::exists(containerBin) && QFileInfo(containerBin).isExecutable()) {
                return containerBin;
            }
            const QString appDirBin = QCoreApplication::applicationDirPath() + "/../container/.local/bin/" + binName;
            if (QFile::exists(appDirBin) && QFileInfo(appDirBin).isExecutable()) {
                return appDirBin;
            }
        }
    }

    // Default or System mode: use official OS package binary (7zz, 7z, 7za)
    for (const QString &binName : {QStringLiteral("7zz"), QStringLiteral("7z"), QStringLiteral("7za")}) {
        if (QFile::exists("/usr/bin/" + binName)) return "/usr/bin/" + binName;
        if (QFile::exists("/bin/" + binName)) return "/bin/" + binName;
        if (QFile::exists("/usr/local/bin/" + binName)) return "/usr/local/bin/" + binName;
        QString found = QStandardPaths::findExecutable(binName);
        if (!found.isEmpty()) return found;
    }

    // Fallback: check local user / container binary if system binary was not found
    for (const QString &binName : {QStringLiteral("7zz"), QStringLiteral("7z"), QStringLiteral("7za")}) {
        const QString homeBin = QDir::homePath() + "/.local/bin/" + binName;
        if (QFile::exists(homeBin) && QFileInfo(homeBin).isExecutable()) {
            return homeBin;
        }
        const QString projectRoot = projectDevRoot();
        if (!projectRoot.isEmpty()) {
            const QString containerBin = projectRoot + QStringLiteral("/container/.local/bin/") + binName;
            if (QFile::exists(containerBin) && QFileInfo(containerBin).isExecutable()) {
                return containerBin;
            }
        }
    }

    return QString();
}

bool BuildConfig::isSevenZipAvailable()
{
    return !sevenZipExecutable().isEmpty();
}

bool BuildConfig::isPopplerDataInstalled()
{
    const QString home = QDir::homePath();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");
    return isPkgInstalled("poppler-data") ||
           QDir("/usr/share/poppler/cMap").exists() ||
           QDir("/usr/local/share/poppler/cMap").exists() ||
           QDir(home + "/.local/share/poppler/cMap").exists() ||
           QDir(home + "/.local/share/bubezview/poppler/cMap").exists() ||
           (!hostHome.isEmpty() && QDir(hostHome + "/.local/share/poppler/cMap").exists());
}

bool BuildConfig::isImageFormatsInstalled()
{
    const QString home = QDir::homePath();
    const QStringList pluginPaths = {
        home + "/.local/lib/qt6/plugins/imageformats/libqtga.so",
        home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqtga.so",
        home + "/.local/lib64/qt6/plugins/imageformats/libqtga.so",
        home + "/.local/plugins/imageformats/libqtga.so",
        home + "/.local/lib/qt6/plugins/imageformats/libqwebp.so",
        home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqwebp.so",
        home + "/.local/lib64/qt6/plugins/imageformats/libqwebp.so",
        home + "/.local/plugins/imageformats/libqwebp.so",
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqtga.so",
        "/usr/lib/qt6/plugins/imageformats/libqtga.so",
        "/usr/lib64/qt6/plugins/imageformats/libqtga.so",
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqtiff.so",
        "/usr/lib/qt6/plugins/imageformats/libqtiff.so"
    };
    for (const auto &p : pluginPaths) {
        if (QFile::exists(p)) return true;
    }
    const auto formats = QImageReader::supportedImageFormats();
    return formats.contains("tga") || formats.contains("tiff") || formats.contains("webp") || formats.contains("psd") || formats.contains("jxl");
}

QString BuildConfig::projectDevRoot()
{
    const QString envRoot = qEnvironmentVariable("BUBEZVIEW_DEV_ROOT");
    if (!envRoot.isEmpty() && QDir(envRoot).exists()) {
        return QDir::cleanPath(envRoot);
    }

    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; ++i) {
        if (dir.exists("CMakeLists.txt") && (dir.exists("container") || dir.exists("src"))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) break;
    }

    const QString defaultProjectDir = QDir::homePath() + "/Progetti/BubezView";
    if (QDir(defaultProjectDir).exists("CMakeLists.txt")) {
        return defaultProjectDir;
    }

    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");
    if (!hostHome.isEmpty()) {
        const QString hostProjectDir = hostHome + "/Progetti/BubezView";
        if (QDir(hostProjectDir).exists("CMakeLists.txt")) {
            return hostProjectDir;
        }
    }

    return QString();
}

bool BuildConfig::hasSystemBinary(const QString &componentId)
{
    QString id = componentId.trimmed().toLower();
    if (id == "pdfimages" || id == "poppler-utils") {
        return QFile::exists("/usr/bin/pdfimages") || QFile::exists("/bin/pdfimages") ||
               QFile::exists("/usr/local/bin/pdfimages") || !QStandardPaths::findExecutable("pdfimages").isEmpty();
    }
    if (id == "qpdf") {
        return QFile::exists("/usr/bin/qpdf") || QFile::exists("/bin/qpdf") ||
               QFile::exists("/usr/local/bin/qpdf") || !QStandardPaths::findExecutable("qpdf").isEmpty();
    }
    if (id == "mupdf" || id == "libmupdf" || id == "mupdf-tools") {
        return QFile::exists("/usr/bin/mutool") || QFile::exists("/bin/mutool") ||
               QFile::exists("/usr/local/bin/mutool") || !QStandardPaths::findExecutable("mutool").isEmpty();
    }
    if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") {
        return QFile::exists("/usr/bin/tesseract") || QFile::exists("/bin/tesseract") ||
               QFile::exists("/usr/local/bin/tesseract") || !QStandardPaths::findExecutable("tesseract").isEmpty();
    }
    if (id == "openssl") {
        return QFile::exists("/usr/bin/openssl") || isPkgInstalled("openssl");
    }
    if (id == "7zip" || id == "7z" || id == "7zz") {
        return QFile::exists("/usr/bin/7zz") || QFile::exists("/usr/bin/7z") || QFile::exists("/usr/bin/7za") ||
               !QStandardPaths::findExecutable("7zz").isEmpty() || !QStandardPaths::findExecutable("7z").isEmpty() ||
               isPkgInstalled("7zip") || isPkgInstalled("7zz") || isPkgInstalled("p7zip");
    }
    if (id == "zstd") {
        return QFile::exists("/usr/bin/zstd") || isPkgInstalled("libzstd");
    }
    if (id == "xz" || id == "liblzma") {
        return QFile::exists("/usr/bin/xz") || isPkgInstalled("liblzma");
    }
    if (id == "office" || id == "libreoffice") {
        return !QStandardPaths::findExecutable("libreoffice").isEmpty();
    }
    if (id == "engine-poppler" || id == "poppler" || id == "poppler-qt6") {
        return isPkgInstalled("poppler-qt6") || isPkgInstalled("libpoppler-qt6-3");
    }
    if (id == "engine-libarchive" || id == "libarchive") {
        return isPkgInstalled("libarchive") || isPkgInstalled("libarchive13") || QFile::exists("/usr/bin/bsdtar");
    }
    if (id == "engine-office" || id == "librevenge") {
        return isPkgInstalled("librevenge-0.0") || instance().enableInternalOffice();
    }
    if (id == "engine-mpv" || id == "mpv" || id == "libmpv") {
        return isPkgInstalled("mpv") || isPkgInstalled("libmpv2") || isPkgInstalled("libmpv1") || QFile::exists("/usr/bin/mpv");
    }
    if (id == "fftw3") {
        return isPkgInstalled("fftw3") || isPkgInstalled("libfftw3-3");
    }
    if (id == "taglib") {
        return isPkgInstalled("taglib") || isPkgInstalled("libtag1v5");
    }
    if (id == "poppler-data") {
        return isPkgInstalled("poppler-data") ||
               QFile::exists("/usr/share/poppler/cMap") ||
               QFile::exists("/usr/local/share/poppler/cMap");
    }
    if (id == "tessdata" || id == "tesseract-ocr-ita" || id == "ocr-ita") {
        return isPkgInstalled("tesseract-ocr-ita") ||
               QFile::exists("/usr/share/tesseract-ocr/5/tessdata/ita.traineddata") ||
               QFile::exists("/usr/share/tesseract-ocr/4.00/tessdata/ita.traineddata") ||
               QFile::exists("/usr/share/tessdata/ita.traineddata");
    }
    if (id == "image-formats" || id == "qt6-image-formats-plugins" || id == "qt6-imageformats") {
        return isImageFormatsInstalled();
    }

    return isExecutableAvailable(id) || isPkgInstalled(id);
}

bool BuildConfig::hasSourceBinary(const QString &componentId)
{
    QString id = componentId.trimmed().toLower();
    if (id == "image-formats" || id == "qtimageformats" || id == "qt6-imageformats" || id == "qt6-image-formats-plugins") {
        const QString home = QDir::homePath();
        const QStringList localPlugins = {
            home + "/.local/lib/qt6/plugins/imageformats/libqwebp.so",
            home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqwebp.so",
            home + "/.local/lib64/qt6/plugins/imageformats/libqwebp.so",
            home + "/.local/plugins/imageformats/libqwebp.so",
            home + "/.local/lib/qt6/plugins/imageformats/libqtga.so",
            home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqtga.so",
            home + "/.local/lib64/qt6/plugins/imageformats/libqtga.so",
            home + "/.local/plugins/imageformats/libqtga.so",
            home + "/.local/lib/qt6/plugins/imageformats/libqtiff.so",
            home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats/libqtiff.so",
            home + "/.local/lib64/qt6/plugins/imageformats/libqtiff.so",
            home + "/.local/plugins/imageformats/libqtiff.so"
        };
        for (const QString &lp : localPlugins) {
            if (QFile::exists(lp)) return true;
        }
        return false;
    }

    auto checkPluginExists = [](const QString &pluginName) -> bool {
        const QString fileName = pluginName.endsWith(".so") ? pluginName : (pluginName + ".so");
        const QString home = QDir::homePath();
        const QString devRoot = projectDevRoot();

        QStringList candidateDirs = {
            home + "/.local/lib/bubezview/plugins",
            home + "/.local/lib/x86_64-linux-gnu/bubezview/plugins",
            home + "/.local/lib64/bubezview/plugins",
            home + "/.local/share/bubezview/plugins",
            QCoreApplication::applicationDirPath() + "/plugins",
            QCoreApplication::applicationDirPath() + "/../plugins"
        };

        if (!devRoot.isEmpty()) {
            candidateDirs.append(devRoot + "/container/.local/lib/bubezview/plugins");
            candidateDirs.append(devRoot + "/container/.local/share/bubezview/plugins");
            candidateDirs.append(devRoot + "/build_deb/plugins");
            candidateDirs.append(devRoot + "/build_deb/plugins/pdf");
            candidateDirs.append(devRoot + "/build_deb/plugins/archive");
            candidateDirs.append(devRoot + "/build_deb/plugins/media");
            candidateDirs.append(devRoot + "/build_install/lib/bubezview/plugins");
        }

        for (const QString &dir : candidateDirs) {
            if (QFile::exists(dir + "/" + fileName)) {
                return true;
            }
        }
        return false;
    };

    if (id == "poppler-data") {
        const QString home = QDir::homePath();
        return QDir(home + "/.local/share/poppler/cMap").exists() ||
               QDir(home + "/.local/share/bubezview/poppler/cMap").exists();
    }
    if (id == "tessdata" || id == "tesseract-ocr-ita" || id == "ocr-ita") {
        const QString home = QDir::homePath();
        return QFile::exists(home + "/.local/share/bubezview/tessdata/ita.traineddata") ||
               QFile::exists(home + "/.local/share/tessdata/ita.traineddata");
    }

    if (id == "engine-poppler" || id == "poppler" || id == "poppler-qt6" || id == "pdf") {
        if (checkPluginExists("bubez_pdf.so")) return true;
    } else if (id == "engine-libarchive" || id == "libarchive" || id == "archive") {
        if (checkPluginExists("bubez_archive.so")) return true;
    } else if (id == "engine-mpv" || id == "mpv" || id == "libmpv" || id == "media") {
        if (checkPluginExists("bubez_media.so")) return true;
    } else if (id == "office" || id == "engine-office" || id == "librevenge") {
        return instance().staticOffice() || instance().enableInternalOffice();
    }

    QStringList exes;
    if (id == "pdfimages" || id == "engine-poppler" || id == "poppler" || id == "pdf") {
        exes << "pdfimages" << "pdfunite";
    } else if (id == "qpdf") {
        exes << "qpdf";
    } else if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") {
        exes << "tesseract";
    } else if (id == "openssl") {
        exes << "openssl";
    } else if (id == "7zip" || id == "7z" || id == "7zz") {
        exes << "7zz" << "7z" << "7za";
    } else if (id == "zstd") {
        exes << "zstd";
    } else if (id == "xz" || id == "liblzma") {
        exes << "xz";
    } else if (id == "engine-mpv" || id == "mpv" || id == "libmpv" || id == "media") {
        exes << "mpv";
    } else if (id == "engine-libarchive" || id == "libarchive" || id == "archive") {
        exes << "bsdtar" << "bsdunzip";
    } else {
        exes << id;
    }

    const QString home = QDir::homePath();
    const QString projectRoot = projectDevRoot();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");

    for (const QString &exe : exes) {
        QStringList candidatePaths = {
            home + "/.local/bin/" + exe,
            home + "/Progetti/BubezView/container/.local/bin/" + exe,
            appDir + "/" + exe,
            appDir + "/bin/" + exe
        };
        if (!projectRoot.isEmpty()) {
            candidatePaths.prepend(projectRoot + "/container/.local/bin/" + exe);
        }
        if (!hostHome.isEmpty()) {
            candidatePaths.append(hostHome + "/.local/bin/" + exe);
            candidatePaths.append(hostHome + "/Progetti/BubezView/container/.local/bin/" + exe);
        }

        for (const QString &cand : candidatePaths) {
            QFileInfo fi(cand);
            if (fi.exists() && fi.isFile() && fi.isExecutable()) return true;
        }
    }

    return false;
}

bool BuildConfig::isComponentAvailable(const QString &componentId)
{
    return hasSystemBinary(componentId) || hasSourceBinary(componentId);
}

BuildConfig::ComponentSourceMode BuildConfig::componentSourceMode(const QString &componentId) const
{
    QString id = componentId.trimmed().toLower();

    // If this component cannot be compiled from source (system library/package only),
    // strictly enforce SystemBinary regardless of any stale saved settings.
    if (!PackageManagerHelper::isCompilableEngine(id) && !PackageManagerHelper::isUserSpaceInstallable(id)) {
        return ComponentSourceMode::SystemBinary;
    }

    bool sys = hasSystemBinary(id);
    bool src = hasSourceBinary(id);

    QSettings settings("Bubez", "BubezView");
    QString key = "component_mode_" + id;
    if (settings.contains(key)) {
        QString pref = settings.value(key).toString();
        if (pref == "system") return ComponentSourceMode::SystemBinary;
        if (pref == "source") return ComponentSourceMode::LocalSource;
    }

    // Default presence fallback:
    if (src && !sys) return ComponentSourceMode::LocalSource;
    if (sys && !src) return ComponentSourceMode::SystemBinary;

    // When both or neither are present, respect the user's active hardware profile:
    HardwareProfile hp = activeHardwareProfile();
    if (hp == HardwareProfile::AllSource || hp == HardwareProfile::AllStable) {
        return ComponentSourceMode::LocalSource;
    }
    if (hp == HardwareProfile::MissingOnly) {
        if (!sys) return ComponentSourceMode::LocalSource;
    }

    // Default when both or neither are present
    return src ? ComponentSourceMode::LocalSource : ComponentSourceMode::SystemBinary;
}

void BuildConfig::setComponentSourceMode(const QString &componentId, ComponentSourceMode mode)
{
    QString id = componentId.trimmed().toLower();
    QSettings settings("Bubez", "BubezView");
    QString key = "component_mode_" + id;
    settings.setValue(key, mode == ComponentSourceMode::SystemBinary ? "system" : "source");

    // Sync legacy static module booleans if applicable
    if (id == "pdfimages" || id == "engine-poppler" || id == "poppler" || id == "pdf") {
        setStaticPdf(mode == ComponentSourceMode::LocalSource);
    } else if (id == "engine-libarchive" || id == "archive") {
        setStaticArchive(mode == ComponentSourceMode::LocalSource);
    } else if (id == "engine-office" || id == "office") {
        setStaticOffice(mode == ComponentSourceMode::LocalSource);
    }
    save();
}

bool BuildConfig::removeLocalSourceBuild(const QString &componentId)
{
    QString id = componentId.trimmed().toLower();
    const QString home = QDir::homePath();
    const QString projectRoot = projectDevRoot();

    auto removePluginFiles = [&](const QString &pluginName) {
        const QString fileName = pluginName.endsWith(".so") ? pluginName : (pluginName + ".so");
        QStringList targetDirs = {
            home + "/.local/lib/bubezview/plugins",
            home + "/.local/lib/x86_64-linux-gnu/bubezview/plugins",
            home + "/.local/lib64/bubezview/plugins",
            home + "/.local/share/bubezview/plugins"
        };
        if (!projectRoot.isEmpty()) {
            targetDirs.append(projectRoot + "/container/.local/lib/bubezview/plugins");
            targetDirs.append(projectRoot + "/container/.local/share/bubezview/plugins");
            targetDirs.append(projectRoot + "/build_deb/plugins/pdf");
            targetDirs.append(projectRoot + "/build_deb/plugins/archive");
            targetDirs.append(projectRoot + "/build_deb/plugins/media");
            targetDirs.append(projectRoot + "/build_deb/plugins");
            targetDirs.append(projectRoot + "/build_install/lib/bubezview/plugins");
        }
        for (const QString &dir : targetDirs) {
            QString fullPath = dir + "/" + fileName;
            if (QFile::exists(fullPath)) {
                QFile::remove(fullPath);
            }
        }
    };

    auto removeExecutables = [&](const QStringList &exeList) {
        const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");
        for (const QString &exe : exeList) {
            QStringList toRemove = {
                home + "/.local/bin/" + exe,
                home + "/Progetti/BubezView/container/.local/bin/" + exe
            };
            if (!projectRoot.isEmpty()) {
                toRemove.append(projectRoot + "/container/.local/bin/" + exe);
            }
            if (!hostHome.isEmpty()) {
                toRemove.append(hostHome + "/.local/bin/" + exe);
                toRemove.append(hostHome + "/Progetti/BubezView/container/.local/bin/" + exe);
            }
            for (const QString &f : toRemove) {
                if (QFile::exists(f)) {
                    QFile::remove(f);
                }
            }
        }
    };

    auto removeLibraries = [&](const QString &pattern) {
        QStringList libDirs = {
            home + "/.local/lib",
            home + "/.local/lib64",
            home + "/.local/lib/x86_64-linux-gnu",
            home + "/Progetti/BubezView/container/.local/lib"
        };
        if (!projectRoot.isEmpty()) {
            libDirs.append(projectRoot + "/container/.local/lib");
            libDirs.append(projectRoot + "/container/.local/lib64");
        }
        const QString hostHome = qEnvironmentVariable("DISTROBOX_HOST_HOME");
        if (!hostHome.isEmpty()) {
            libDirs.append(hostHome + "/.local/lib");
            libDirs.append(hostHome + "/Progetti/BubezView/container/.local/lib");
        }
        for (const QString &ld : libDirs) {
            QDir ldir(ld);
            if (ldir.exists()) {
                const auto libs = ldir.entryList({pattern}, QDir::Files);
                for (const QString &l : libs) ldir.remove(l);
            }
        }
    };

    if (id == "image-formats" || id == "qtimageformats" || id == "qt6-imageformats" || id == "qt6-image-formats-plugins") {
        const QStringList localDirs = {
            home + "/.local/lib/qt6/plugins/imageformats",
            home + "/.local/lib/x86_64-linux-gnu/qt6/plugins/imageformats",
            home + "/.local/lib64/qt6/plugins/imageformats",
            home + "/.local/plugins/imageformats"
        };
        for (const QString &dirPath : localDirs) {
            QDir dir(dirPath);
            if (dir.exists()) {
                const auto files = dir.entryList({"libq*.so"}, QDir::Files);
                for (const QString &f : files) {
                    dir.remove(f);
                }
            }
        }
        if (!projectRoot.isEmpty()) {
            QDir depDir(projectRoot + "/build_deps/qtimageformats");
            if (depDir.exists()) depDir.removeRecursively();
            QDir bldDir(projectRoot + "/build_deps/qtimageformats_build");
            if (bldDir.exists()) bldDir.removeRecursively();
        }
    } else if (id == "engine-poppler" || id == "poppler" || id == "poppler-qt6" || id == "pdf") {
        removePluginFiles("bubez_pdf.so");
        removeExecutables({"pdfimages", "pdfunite", "pdftoppm", "pdftotext", "pdfinfo", "pdffonts",
                           "pdfattach", "pdfdetach", "pdfseparate", "pdftops", "pdftohtml"});
        removeLibraries("libpoppler*");
        instance().setStaticPdf(false);
        PluginManager::instance().unloadPlugin(FileCategory::Pdf);
        PluginManager::instance().unloadPlugin("pdf");
    } else if (id == "engine-libarchive" || id == "libarchive" || id == "archive") {
        removePluginFiles("bubez_archive.so");
        removeExecutables({"bsdtar", "bsdunzip"});
        removeLibraries("libarchive*");
        instance().setStaticArchive(false);
        PluginManager::instance().unloadPlugin(FileCategory::Archive);
        PluginManager::instance().unloadPlugin("archive");
    } else if (id == "engine-mpv" || id == "mpv" || id == "libmpv" || id == "media") {
        removePluginFiles("bubez_media.so");
        removeExecutables({"mpv"});
        PluginManager::instance().unloadPlugin(FileCategory::Media);
        PluginManager::instance().unloadPlugin("media");
    } else if (id == "poppler-data") {
        QDir popDir(home + "/.local/share/poppler");
        if (popDir.exists()) popDir.removeRecursively();
    } else if (id == "tessdata" || id == "tesseract-ocr-ita" || id == "ocr-ita") {
        for (const char *f : {"ita.traineddata", "eng.traineddata", "osd.traineddata"}) {
            QString fname = QLatin1String(f);
            QFile::remove(home + "/.local/share/bubezview/tessdata/" + fname);
            QFile::remove(home + "/.local/share/tessdata/" + fname);
        }
    } else if (id == "office" || id == "engine-office" || id == "librevenge") {
        instance().setStaticOffice(false);
        instance().setEnableInternalOffice(false);
    } else if (id == "7zip" || id == "7z" || id == "7zz") {
        removeExecutables({"7zz", "7z", "7za"});
    } else if (id == "qpdf") {
        removeExecutables({"qpdf", "fix-qdf", "zlib-flate"});
        removeLibraries("libqpdf*");
        auto removeSubDir = [](const QString &dPath) { QDir d(dPath); if (d.exists()) d.removeRecursively(); };
        removeSubDir(home + "/.local/include/qpdf");
        removeSubDir(home + "/.local/share/doc/qpdf");
        removeSubDir(home + "/.local/lib/cmake/qpdf");
        if (!projectRoot.isEmpty()) {
            removeSubDir(projectRoot + "/container/.local/include/qpdf");
            removeSubDir(projectRoot + "/container/.local/share/doc/qpdf");
            removeSubDir(projectRoot + "/container/.local/lib/cmake/qpdf");
        }
    } else if (id == "tesseract" || id == "ocr" || id == "tesseract-ocr") {
        removeExecutables({"tesseract"});
        removeLibraries("libtesseract*");
    } else if (id == "zstd") {
        removeExecutables({"zstd", "zstdcat", "unzstd"});
    } else if (id == "xz" || id == "liblzma") {
        removeExecutables({"xz", "xzcat", "unxz", "lzma"});
    } else {
        removeExecutables({id});
    }

    // Clean build directory in build_deps if present
    if (!projectRoot.isEmpty()) {
        QDir depDir(projectRoot + "/build_deps/" + id);
        if (depDir.exists()) {
            depDir.removeRecursively();
        }
        QDir depBldDir(projectRoot + "/build_deps/" + id + "_build");
        if (depBldDir.exists()) {
            depBldDir.removeRecursively();
        }
    }

    // Update settings: if system package exists, switch to system; otherwise remove the override
    // so active hardware profile can take effect cleanly without forcing obsolete system mode.
    QSettings settings("Bubez", "BubezView");
    if (hasSystemBinary(id)) {
        settings.setValue("component_mode_" + id, "system");
    } else {
        settings.remove("component_mode_" + id);
    }

    instance().save();
    PackageManagerHelper::clearCaches();

    return true;
}

int BuildConfig::pdfMergeCount()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("pdf_merge_count", 0).toInt();
}

void BuildConfig::incrementPdfMergeCount()
{
    QSettings settings("Bubez", "BubezView");
    int c = settings.value("pdf_merge_count", 0).toInt() + 1;
    settings.setValue("pdf_merge_count", c);
}

bool BuildConfig::isQpdfTipDismissed()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("qpdf_tip_dismissed", false).toBool();
}

void BuildConfig::dismissQpdfTip()
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("qpdf_tip_dismissed", true);
}

bool BuildConfig::showUninstalledFeatures()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("show_uninstalled_features", false).toBool();
}

void BuildConfig::setShowUninstalledFeatures(bool show)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("show_uninstalled_features", show);
}

bool BuildConfig::showUnsupportedFiles()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("show_unsupported_files", false).toBool();
}

void BuildConfig::setShowUnsupportedFiles(bool show)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("show_unsupported_files", show);
}

QList<ModuleInfo> BuildConfig::availableModules() const
{
    QList<ModuleInfo> list;

    // 1. PDF Viewer
    ModuleInfo pdf;
    pdf.id = "pdf";
    pdf.name = QObject::tr("PDF Document Viewer");
    pdf.description = QObject::tr("High-fidelity vector PDF reader, text selection, page search, and zoom.");
    pdf.isCore = true;
    pdf.isEnabled = true;
    pdf.isStatic = m_staticPdf;
    pdf.requiredPkg = "poppler-qt6";
    pdf.pkgAvailable = isPkgInstalled("poppler-qt6");

    ExtensionInfo extImages;
    extImages.id = "pdfimages";
    extImages.name = QObject::tr("Embedded Image Extraction");
    extImages.description = QObject::tr("Extracts lossless PNG/JPG photos embedded inside PDFs.");
    extImages.isInstalled = isPdfimagesAvailable();
    extImages.installPackage = "poppler-utils";
    pdf.extensions.append(extImages);

    ExtensionInfo extData;
    extData.id = "poppler-data";
    extData.name = QObject::tr("International & CJK Font Encodings (poppler-data)");
    extData.description = QObject::tr("Provides CMAP tables for Asian (Chinese, Japanese, Korean) and complex international character sets.");
    extData.isInstalled = isPopplerDataInstalled();
    extData.installPackage = "poppler-data";
    pdf.extensions.append(extData);

    ExtensionInfo extQpdf;
    extQpdf.id = "qpdf";
    extQpdf.name = QObject::tr("Advanced PDF Merge & Optimizer (QPDF)");
    extQpdf.description = QObject::tr("Font deduplication, bookmark preservation, and stream linearization.");
    extQpdf.isInstalled = isQpdfAvailable();
    extQpdf.installPackage = "qpdf";
    pdf.extensions.append(extQpdf);

    ExtensionInfo extOcr;
    extOcr.id = "ocr";
    extOcr.name = QObject::tr("Optical Character Recognition (OCR)");
    extOcr.description = QObject::tr("Scans documents into searchable text using Tesseract neural models.");
    extOcr.isInstalled = isTesseractAvailable();
    extOcr.installPackage = "tesseract-ocr";
    pdf.extensions.append(extOcr);

    ExtensionInfo extSig;
    extSig.id = "openssl";
    extSig.name = QObject::tr("Cryptographic Digital Signatures (PAdES)");
    extSig.description = QObject::tr("Applies compliant X.509 PKCS#12 digital signatures with visual seals.");
    extSig.isInstalled = isPkgInstalled("openssl");
    extSig.installPackage = "openssl";
    pdf.extensions.append(extSig);

    list.append(pdf);

    // 2. Archive Viewer
    ModuleInfo arch;
    arch.id = "archive";
    arch.name = QObject::tr("Archive Explorer");
    arch.description = QObject::tr("Inspect and extract contents of ZIP, TAR, 7Z, RAR, GZ, and ISO archives.");
    arch.isCore = true;
    arch.isEnabled = true;
    arch.isStatic = m_staticArchive;
    arch.requiredPkg = "libarchive";
    arch.pkgAvailable = isPkgInstalled("libarchive");

    ExtensionInfo extZstd;
    extZstd.id = "zstd";
    extZstd.name = QObject::tr("Zstandard Codec (zstd)");
    extZstd.description = QObject::tr("High-speed real-time compression format.");
    extZstd.isInstalled = isPkgInstalled("libzstd");
    extZstd.installPackage = "libzstd";
    arch.extensions.append(extZstd);

    ExtensionInfo extXz;
    extXz.id = "xz";
    extXz.name = QObject::tr("LZMA / XZ Codec");
    extXz.description = QObject::tr("Maximum density archive compression.");
    extXz.isInstalled = isPkgInstalled("liblzma");
    extXz.installPackage = "xz";
    arch.extensions.append(extXz);

    list.append(arch);

    // 3. Media Viewer
    ModuleInfo media;
    media.id = "media";
    media.name = QObject::tr("Audio & Video Player");
    media.description = QObject::tr("Hardware-accelerated playback for MP4, MKV, MP3, FLAC, and WAV.");
    media.isCore = true;
    media.isEnabled = true;
    media.isStatic = false;
    media.requiredPkg = "mpv";
    media.pkgAvailable = isPkgInstalled("mpv");
    list.append(media);

    // 4. Internal Office Viewer (Optional)
    ModuleInfo office;
    office.id = "office";
    office.name = QObject::tr("Internal Office Document Viewer");
    office.description = QObject::tr("Instant native preview for Word, Excel, and ODT files without relying on external LibreOffice.");
    office.isCore = false;
    office.isEnabled = m_enableInternalOffice;
    office.isStatic = false;
    office.requiredPkg = "librevenge-0.0";
    office.pkgAvailable = isPkgInstalled("librevenge-0.0");
    list.append(office);

    // 5. QR Code Generator & Viewer
    ModuleInfo qr;
    qr.id = "qr";
    qr.name = QObject::tr("QR Code Generator");
    qr.description = QObject::tr("Generates scannable QR codes for WiFi sharing, URLs, and quick mobile transfer.");
    qr.isCore = true;
    qr.isEnabled = true;
    qr.isStatic = false;
    qr.requiredPkg = "libqrencode";
    qr.pkgAvailable = isPkgInstalled("libqrencode");
    list.append(qr);

    return list;
}
