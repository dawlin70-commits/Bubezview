/**
 * @file font_resolution_helper.cpp
 * @brief Implementation of font resolution, distro repository search, and Google Fonts download.
 */

#include "font_resolution_helper.h"
#include "package_manager_helper.h"
#include <QFontDatabase>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageBox>

QString FontResolutionHelper::cleanFontFamilyName(const QString &rawFontName)
{
    QString name = rawFontName.trimmed();
    if (name.isEmpty()) return QStringLiteral("Sans Serif");

    // 1. Remove PDF subset prefix (6 uppercase letters followed by '+', e.g. "DDFNEO+TradeGothic")
    static const QRegularExpression subsetRegex(QStringLiteral("^[A-Z]{6}\\+"));
    name.remove(subsetRegex);

    // 2. Remove common PDF font suffixes
    static const QStringList suffixes = {
        QStringLiteral("-BoldItalic"), QStringLiteral("-BoldOblique"),
        QStringLiteral("-Bold"), QStringLiteral("-Italic"), QStringLiteral("-Oblique"),
        QStringLiteral("-Regular"), QStringLiteral("-Roman"), QStringLiteral("-Light"),
        QStringLiteral("-Medium"), QStringLiteral("-Black"), QStringLiteral("-Heavy"),
        QStringLiteral(",BoldItalic"), QStringLiteral(",Bold"), QStringLiteral(",Italic"),
        QStringLiteral(",Regular"), QStringLiteral("PSMT"), QStringLiteral("MT")
    };

    for (const QString &suffix : suffixes) {
        if (name.endsWith(suffix, Qt::CaseInsensitive)) {
            name.chop(suffix.length());
            break;
        }
    }

    // 3. Normalize common concatenated names (e.g. "TradeGothic" -> "Trade Gothic")
    if (name.compare(QStringLiteral("TradeGothic"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Trade Gothic");
    }
    if (name.compare(QStringLiteral("LeagueGothic"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("League Gothic");
    }
    if (name.compare(QStringLiteral("OpenSans"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Open Sans");
    }
    if (name.compare(QStringLiteral("LiberationSans"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Liberation Sans");
    }
    if (name.compare(QStringLiteral("LiberationSerif"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Liberation Serif");
    }
    if (name.compare(QStringLiteral("LiberationMono"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Liberation Mono");
    }
    if (name.compare(QStringLiteral("TimesNewRoman"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Times New Roman");
    }
    if (name.compare(QStringLiteral("CourierNew"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Courier New");
    }

    return name.trimmed();
}

bool FontResolutionHelper::isFontFamilyInstalled(const QString &familyName)
{
    QString clean = cleanFontFamilyName(familyName);
    const QStringList families = QFontDatabase::families();
    for (const QString &f : families) {
        if (f.compare(clean, Qt::CaseInsensitive) == 0) {
            return true;
        }
        // Also check ignoring spaces
        QString fNoSpace = f;
        fNoSpace.remove(QLatin1Char(' '));
        QString cleanNoSpace = clean;
        cleanNoSpace.remove(QLatin1Char(' '));
        if (fNoSpace.compare(cleanNoSpace, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString FontResolutionHelper::checkMissingGlyphs(const QString &text, const QFont &font)
{
    QFontMetrics fm(font);
    QString missing;
    for (const QChar &ch : text) {
        if (ch.isSpace() || ch == QLatin1Char('\n') || ch == QLatin1Char('\r') || ch == QLatin1Char('\t')) {
            continue;
        }
        if (!fm.inFont(ch)) {
            if (!missing.contains(ch)) {
                missing.append(ch);
            }
        }
    }
    return missing;
}

QString FontResolutionHelper::findDistroFontPackage(const QString &fontFamily)
{
    DistroFamily fam = PackageManagerHelper::detectFamily();
    QString lower = cleanFontFamilyName(fontFamily).toLower();

    // 1. Trade Gothic / League Gothic
    if (lower.contains(QStringLiteral("trade gothic")) || lower.contains(QStringLiteral("tradegothic")) ||
        lower.contains(QStringLiteral("league gothic")) || lower.contains(QStringLiteral("bebas"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-league-gothic");
        if (fam == DistroFamily::Fedora) return QStringLiteral("league-gothic-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("otf-league-gothic");
        return QStringLiteral("fonts-league-gothic");
    }

    // 2. Helvetica / Arial / Times / Courier (Standard Postscript & Liberation)
    if (lower.contains(QStringLiteral("helvetica")) || lower.contains(QStringLiteral("arial")) ||
        lower.contains(QStringLiteral("times")) || lower.contains(QStringLiteral("courier"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-liberation");
        if (fam == DistroFamily::Fedora) return QStringLiteral("liberation-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-liberation");
        return QStringLiteral("fonts-liberation");
    }

    // 3. Calibri / Carlito
    if (lower.contains(QStringLiteral("calibri")) || lower.contains(QStringLiteral("carlito"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-crosextra-carlito");
        if (fam == DistroFamily::Fedora) return QStringLiteral("google-carlito-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-carlito");
        return QStringLiteral("fonts-crosextra-carlito");
    }

    // 4. Cambria / Caladea
    if (lower.contains(QStringLiteral("cambria")) || lower.contains(QStringLiteral("caladea"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-crosextra-caladea");
        if (fam == DistroFamily::Fedora) return QStringLiteral("google-caladea-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-caladea");
        return QStringLiteral("fonts-crosextra-caladea");
    }

    // 5. Roboto
    if (lower.contains(QStringLiteral("roboto"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-roboto");
        if (fam == DistroFamily::Fedora) return QStringLiteral("google-roboto-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-roboto");
        return QStringLiteral("fonts-roboto");
    }

    // 6. Open Sans
    if (lower.contains(QStringLiteral("open sans")) || lower.contains(QStringLiteral("opensans"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-open-sans");
        if (fam == DistroFamily::Fedora) return QStringLiteral("open-sans-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-opensans");
        return QStringLiteral("fonts-open-sans");
    }

    // 7. Noto Sans / Serif
    if (lower.contains(QStringLiteral("noto"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-noto-core");
        if (fam == DistroFamily::Fedora) return QStringLiteral("google-noto-sans-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("noto-fonts");
        return QStringLiteral("fonts-noto-core");
    }

    // 8. DejaVu
    if (lower.contains(QStringLiteral("dejavu"))) {
        if (fam == DistroFamily::Debian) return QStringLiteral("fonts-dejavu-core");
        if (fam == DistroFamily::Fedora) return QStringLiteral("dejavu-sans-fonts");
        if (fam == DistroFamily::Arch) return QStringLiteral("ttf-dejavu");
        return QStringLiteral("fonts-dejavu-core");
    }

    // 9. Standard 35 PDF Fonts (URW Base 35)
    if (fam == DistroFamily::Debian) return QStringLiteral("fonts-urw-base35");
    if (fam == DistroFamily::Fedora) return QStringLiteral("urw-base35-fonts");
    if (fam == DistroFamily::Arch) return QStringLiteral("urw-base35-fonts");
    return QStringLiteral("fonts-urw-base35");
}

QString FontResolutionHelper::closestFreeAlternative(const QString &fontFamily)
{
    QString lower = cleanFontFamilyName(fontFamily).toLower();

    if (lower.contains(QStringLiteral("trade gothic")) || lower.contains(QStringLiteral("tradegothic")) ||
        lower.contains(QStringLiteral("franklin gothic"))) {
        return QStringLiteral("League Gothic");
    }
    if (lower.contains(QStringLiteral("helvetica"))) {
        return QStringLiteral("Liberation Sans");
    }
    if (lower.contains(QStringLiteral("times"))) {
        return QStringLiteral("Liberation Serif");
    }
    if (lower.contains(QStringLiteral("courier"))) {
        return QStringLiteral("Liberation Mono");
    }
    if (lower.contains(QStringLiteral("calibri"))) {
        return QStringLiteral("Carlito");
    }
    if (lower.contains(QStringLiteral("cambria"))) {
        return QStringLiteral("Caladea");
    }
    if (lower.contains(QStringLiteral("futura"))) {
        return QStringLiteral("Jost");
    }
    if (lower.contains(QStringLiteral("garamond"))) {
        return QStringLiteral("EB Garamond");
    }
    if (lower.contains(QStringLiteral("frutiger"))) {
        return QStringLiteral("Hind");
    }
    if (lower.contains(QStringLiteral("bodoni"))) {
        return QStringLiteral("Bodoni Moda");
    }

    return QStringLiteral("Roboto");
}

bool FontResolutionHelper::downloadGoogleFont(const QString &familyName, QString *outLoadedFamily, QString *outError)
{
    QString targetFamily = cleanFontFamilyName(familyName);
    if (targetFamily.isEmpty()) {
        if (outError) *outError = QObject::tr("Font family name is empty.");
        return false;
    }

    // Step 1: Query Google Fonts CSS API to discover direct font file URL
    QString encodedFamily = targetFamily;
    encodedFamily.replace(QLatin1Char(' '), QLatin1Char('+'));
    QUrl cssUrl(QStringLiteral("https://fonts.googleapis.com/css2?family=%1:wght@400;700").arg(encodedFamily));

    QNetworkAccessManager nam;
    QNetworkRequest cssReq(cssUrl);
    cssReq.setHeader(QNetworkRequest::UserAgentHeader,
                     QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(8000);

    QNetworkReply *cssReply = nam.get(cssReq);
    QObject::connect(cssReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (!cssReply->isFinished() || cssReply->error() != QNetworkReply::NoError) {
        if (outError) *outError = QObject::tr("Unable to connect to Google Fonts for '%1'.").arg(targetFamily);
        cssReply->deleteLater();
        return false;
    }

    QString cssContent = QString::fromUtf8(cssReply->readAll());
    cssReply->deleteLater();

    // Extract first .ttf or .woff2 URL from CSS: src: url(...) format(...)
    static const QRegularExpression urlRegex(QStringLiteral("url\\((https://[^\\)]+\\.(?:ttf|otf|woff2))\\)"));
    QRegularExpressionMatch match = urlRegex.match(cssContent);
    if (!match.hasMatch()) {
        if (outError) *outError = QObject::tr("Font '%1' is not available in the Google Fonts catalog.").arg(targetFamily);
        return false;
    }

    QUrl fontUrl(match.captured(1));

    // Step 2: Download the binary font file
    QNetworkRequest fontReq(fontUrl);
    fontReq.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));

    timer.start(10000);
    QNetworkReply *fontReply = nam.get(fontReq);
    QObject::connect(fontReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (!fontReply->isFinished() || fontReply->error() != QNetworkReply::NoError) {
        if (outError) *outError = QObject::tr("Error downloading font file for '%1'.").arg(targetFamily);
        fontReply->deleteLater();
        return false;
    }

    QByteArray fontData = fontReply->readAll();
    fontReply->deleteLater();

    if (fontData.size() < 1024) {
        if (outError) *outError = QObject::tr("Received invalid or corrupted font data.");
        return false;
    }

    // Step 3: Save to local user fonts directory (~/.local/share/fonts/)
    QString fontsDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/fonts");
    QDir().mkpath(fontsDir);

    QString ext = fontUrl.path().endsWith(QStringLiteral(".woff2"), Qt::CaseInsensitive) ? QStringLiteral(".woff2") : QStringLiteral(".ttf");
    QString safeFileName = targetFamily;
    safeFileName.replace(QLatin1Char(' '), QLatin1Char('_'));
    QString destFilePath = fontsDir + QStringLiteral("/") + safeFileName + ext;

    QFile file(destFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outError) *outError = QObject::tr("Unable to save font file to %1.").arg(destFilePath);
        return false;
    }
    file.write(fontData);
    file.close();

    // Step 4: Load into Qt application font database
    return loadFontFromFile(destFilePath, outLoadedFamily);
}

bool FontResolutionHelper::loadFontFromFile(const QString &filePath, QString *outLoadedFamily)
{
    if (!QFile::exists(filePath)) return false;

    int fontId = QFontDatabase::addApplicationFont(filePath);
    if (fontId == -1) return false;

    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) return false;

    if (outLoadedFamily) {
        *outLoadedFamily = families.first();
    }
    return true;
}

bool FontResolutionHelper::installDistroFontPackage(const QString &packageName, QWidget *parent)
{
    if (packageName.trimmed().isEmpty()) return false;
    return PackageManagerHelper::runPrivilegedInstall({packageName.trimmed()}, parent);
}
