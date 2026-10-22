#pragma once

#include <QString>
#include <QByteArray>
#include <QUrl>
#include <QChar>

/**
 * @brief Represents extracted GPS geographic coordinates from EXIF metadata.
 */
struct ExifGpsData {
    bool hasGps = false;             ///< True if valid GPS coordinates were extracted.
    double latitude = 0.0;           ///< Signed decimal latitude (-90.0 to +90.0).
    double longitude = 0.0;          ///< Signed decimal longitude (-180.0 to +180.0).
    double altitude = 0.0;           ///< Altitude in meters.
    bool belowSeaLevel = false;      ///< True if altitude reference is below sea level.
    QChar latitudeRef = 'N';         ///< 'N' or 'S'
    QChar longitudeRef = 'E';        ///< 'E' or 'W'

    /**
     * @brief Formats coordinates in Degrees, Minutes, Seconds (DMS).
     * Example: 45° 27' 51.2" N, 9° 11' 22.4" E
     */
    QString formattedDms() const;

    /**
     * @brief Formats coordinates in decimal notation.
     * Example: 45.464222, 9.189556
     */
    QString formattedDecimal() const;

    /**
     * @brief Generates an OpenStreetMap URL centered on the coordinates with a marker pin.
     */
    QUrl openStreetMapUrl() const;

    /**
     * @brief Generates a Google Maps URL centered on the coordinates.
     */
    QUrl googleMapsUrl() const;
};

/**
 * @brief Structured EXIF photography and device metadata extracted from image files.
 */
struct ExifData {
    bool hasExif = false;            ///< True if valid EXIF metadata was discovered.
    QString make;                    ///< Camera / Smartphone manufacturer (e.g. Apple, Sony, Canon).
    QString model;                   ///< Camera / Smartphone device model (e.g. iPhone 15 Pro, ILCE-7M4).
    QString software;                ///< Firmware or editing software version.
    QString dateTimeOriginal;        ///< Timestamp when the photo was originally taken.
    QString exposureTime;            ///< Shutter speed formatted string (e.g. "1/250 s" or "2.5 s").
    QString fNumber;                 ///< Aperture / f-stop string (e.g. "f/1.8").
    int isoSpeed = 0;                ///< ISO sensitivity rating (e.g. 100, 800, 3200).
    double focalLength = 0.0;        ///< Lens focal length in millimeters (e.g. 24.0 mm).
    int focalLength35mm = 0;         ///< 35mm equivalent focal length in millimeters.
    QString lensModel;               ///< Lens identification string.
    int flash = -1;                  ///< Flash state (-1 = unknown, 0 = not fired, 1 = fired).
    int orientation = 1;             ///< EXIF orientation tag (1 to 8).
    ExifGpsData gps;                 ///< Geographic location metadata.
};

/**
 * @brief Lightweight, safe, zero-dependency C++ parser for EXIF and GPS metadata.
 *
 * Supports JPEG (APP1 marker), TIFF, and WebP EXIF payloads with strict bounds checking.
 */
class ExifParser {
public:
    /**
     * @brief Parses EXIF metadata directly from a file on disk.
     * @param filePath Absolute path to the image file.
     * @return Extracted ExifData structure.
     */
    static ExifData parseFile(const QString &filePath);

    /**
     * @brief Parses EXIF metadata from raw binary buffer.
     * @param data Binary payload containing image headers or raw EXIF stream.
     * @return Extracted ExifData structure.
     */
    static ExifData parseData(const QByteArray &data);

private:
    /**
     * @brief Parses TIFF-formatted IFD structures starting at the given offset.
     * @param data Full raw buffer.
     * @param tiffStart Byte index in buffer where TIFF header begins.
     * @param exif Output structure populated with parsed values.
     */
    static void parseTiffPayload(const QByteArray &data, int tiffStart, ExifData &exif);
};
