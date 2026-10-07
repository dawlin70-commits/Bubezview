/**
 * @file build_config.cpp
 * @brief Implementation of persistent build configuration manager.
 */

#include "build_config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

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
    , m_popplerVersion("24.08.0")
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
    m_staticPdf = modules.value("static_pdf").toBool(false);
    m_popplerVersion = modules.value("poppler_version").toString("24.08.0");
    m_staticArchive = modules.value("static_archive").toBool(false);
    m_enableInternalOffice = modules.value("enable_internal_office").toBool(false);
    m_staticOffice = modules.value("static_office").toBool(false);

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
        args << QString("-DCMAKE_CXX_FLAGS=%1").arg(m_customCompilerFlags.trimmed());
    }
    return args;
}

bool BuildConfig::isPkgInstalled(const QString &pkgName)
{
    if (pkgName.isEmpty()) return true;

    QProcess proc;
    QStringList args;
    args << "--exists" << pkgName;
    proc.start("pkg-config", args);
    if (!proc.waitForFinished(1000)) {
        return false;
    }
    return (proc.exitCode() == 0);
}

bool BuildConfig::isExecutableAvailable(const QString &exeName)
{
    if (exeName.isEmpty()) return false;
    return !QStandardPaths::findExecutable(exeName).isEmpty();
}

bool BuildConfig::isQpdfAvailable()
{
#ifdef HAVE_LIBQPDF
    return true;
#else
    return isExecutableAvailable("qpdf");
#endif
}

bool BuildConfig::isTesseractAvailable()
{
    return isExecutableAvailable("tesseract");
}

bool BuildConfig::isPdfimagesAvailable()
{
#ifdef POPPLER_BUILTIN_STATIC
    return true;
#else
    return isExecutableAvailable("pdfimages");
#endif
}

bool BuildConfig::isPopplerDataInstalled()
{
    return isPkgInstalled("poppler-data") ||
           QDir("/usr/share/poppler/cMap").exists() ||
           QDir("/usr/local/share/poppler/cMap").exists();
}

int BuildConfig::pdfMergeCount()
{
    QSettings settings;
    return settings.value("pdf_merge_count", 0).toInt();
}

void BuildConfig::incrementPdfMergeCount()
{
    QSettings settings;
    int c = settings.value("pdf_merge_count", 0).toInt() + 1;
    settings.setValue("pdf_merge_count", c);
}

bool BuildConfig::isQpdfTipDismissed()
{
    QSettings settings;
    return settings.value("qpdf_tip_dismissed", false).toBool();
}

void BuildConfig::dismissQpdfTip()
{
    QSettings settings;
    settings.setValue("qpdf_tip_dismissed", true);
}

bool BuildConfig::showUninstalledFeatures()
{
    QSettings settings;
    return settings.value("show_uninstalled_features", false).toBool();
}

void BuildConfig::setShowUninstalledFeatures(bool show)
{
    QSettings settings;
    settings.setValue("show_uninstalled_features", show);
}

bool BuildConfig::showUnsupportedFiles()
{
    QSettings settings;
    return settings.value("show_unsupported_files", false).toBool();
}

void BuildConfig::setShowUnsupportedFiles(bool show)
{
    QSettings settings;
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
