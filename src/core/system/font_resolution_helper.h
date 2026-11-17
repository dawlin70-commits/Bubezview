/**
 * @file font_resolution_helper.h
 * @brief Detection, distro repository search, Google Fonts download, and fallback resolution for PDF fonts.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QFont>
#include <QWidget>

class FontResolutionHelper {
public:
    /**
     * @brief Cleans a PDF font name by stripping 6-letter subset prefixes (e.g. 'DDFNEO+TradeGothic')
     *        and style suffixes ('-Bold', ',Italic', etc.).
     */
    static QString cleanFontFamilyName(const QString &rawFontName);

    /**
     * @brief Checks if a font family is installed on the host system.
     */
    static bool isFontFamilyInstalled(const QString &familyName);

    /**
     * @brief Returns a string of unique characters in @p text that are not supported by @p font.
     */
    static QString checkMissingGlyphs(const QString &text, const QFont &font);

    /**
     * @brief Maps a font family to the native package name in the active Linux distro.
     */
    static QString findDistroFontPackage(const QString &fontFamily);

    /**
     * @brief Suggests a metric/visual libre equivalent for proprietary fonts.
     */
    static QString closestFreeAlternative(const QString &fontFamily);

    /**
     * @brief Downloads a font family from Google Fonts, saves it to user fonts, and registers with Qt.
     * @param familyName Font family name to fetch (e.g. "Roboto", "League Gothic", "Oswald")
     * @param outLoadedFamily Name of the family registered in Qt
     * @param outError Error message on failure
     * @return True if downloaded and loaded successfully
     */
    static bool downloadGoogleFont(const QString &familyName, QString *outLoadedFamily = nullptr, QString *outError = nullptr);

    /**
     * @brief Loads and registers a font from a local file (.ttf, .otf, .woff2).
     */
    static bool loadFontFromFile(const QString &filePath, QString *outLoadedFamily = nullptr);

    /**
     * @brief Installs a font package via distro package manager (using pkexec).
     */
    static bool installDistroFontPackage(const QString &packageName, QWidget *parent = nullptr);
};
