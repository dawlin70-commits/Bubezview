#include "exif_parser.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>

QString ExifGpsData::formattedDms() const
{
    if (!hasGps) return QString();

    auto toDms = [](double coord, int &deg, int &min, double &sec) {
        double absCoord = std::abs(coord);
        deg = static_cast<int>(absCoord);
        double minFull = (absCoord - deg) * 60.0;
        min = static_cast<int>(minFull);
        sec = (minFull - min) * 60.0;
        if (std::round(sec * 10.0) >= 600.0) {
            sec = 0.0;
            min++;
            if (min >= 60) {
                min = 0;
                deg++;
            }
        }
    };

    int latDeg = 0, latMin = 0, lonDeg = 0, lonMin = 0;
    double latSec = 0.0, lonSec = 0.0;
    toDms(latitude, latDeg, latMin, latSec);
    toDms(longitude, lonDeg, lonMin, lonSec);

    return QString("%1° %2' %3\" %4, %5° %6' %7\" %8")
        .arg(latDeg)
        .arg(latMin)
        .arg(latSec, 0, 'f', 1)
        .arg(latitudeRef)
        .arg(lonDeg)
        .arg(lonMin)
        .arg(lonSec, 0, 'f', 1)
        .arg(longitudeRef);
}

QString ExifGpsData::formattedDecimal() const
{
    if (!hasGps) return QString();
    return QString("%1, %2")
        .arg(QString::number(latitude, 'f', 6),
             QString::number(longitude, 'f', 6));
}

QUrl ExifGpsData::openStreetMapUrl() const
{
    if (!hasGps) return QUrl();
    const QString latStr = QString::number(latitude, 'f', 6);
    const QString lonStr = QString::number(longitude, 'f', 6);
    return QUrl(QString("https://www.openstreetmap.org/?mlat=%1&mlon=%2#map=16/%1/%2")
                    .arg(latStr, lonStr));
}

QUrl ExifGpsData::googleMapsUrl() const
{
    if (!hasGps) return QUrl();
    const QString latStr = QString::number(latitude, 'f', 6);
    const QString lonStr = QString::number(longitude, 'f', 6);
    return QUrl(QString("https://www.google.com/maps/search/?api=1&query=%1,%2")
                    .arg(latStr, lonStr));
}

ExifData ExifParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ExifData();
    }

    // Read header chunk (256 KB is sufficient to encompass almost any EXIF APP1 payload)
    const QByteArray headerData = file.read(262144);
    file.close();

    return parseData(headerData);
}

ExifData ExifParser::parseData(const QByteArray &data)
{
    ExifData exif;
    const int sz = data.size();
    if (sz < 16) return exif;

    const unsigned char *buf = reinterpret_cast<const unsigned char*>(data.constData());

    // 1. JPEG Detection (0xFFD8)
    if (buf[0] == 0xFF && buf[1] == 0xD8) {
        int offset = 2;
        while (offset + 4 <= sz) {
            if (buf[offset] != 0xFF) {
                offset++;
                continue;
            }
            unsigned char marker = buf[offset + 1];
            // Stop at Start of Scan (SOS) or End of Image (EOI)
            if (marker == 0xDA || marker == 0xD9) {
                break;
            }
            if (marker == 0x00 || (marker >= 0xD0 && marker <= 0xD7)) {
                offset += 2;
                continue;
            }

            int markerLen = (static_cast<int>(buf[offset + 2]) << 8) | static_cast<int>(buf[offset + 3]);
            if (markerLen < 2 || offset + 2 + markerLen > sz) {
                break;
            }

            // APP1 Marker (0xFFE1) -> Check for "Exif\0\0"
            if (marker == 0xE1 && markerLen >= 8) {
                if (offset + 10 <= sz &&
                    buf[offset + 4] == 'E' && buf[offset + 5] == 'x' &&
                    buf[offset + 6] == 'i' && buf[offset + 7] == 'f' &&
                    buf[offset + 8] == 0 && buf[offset + 9] == 0) {
                    // TIFF header starts right after "Exif\0\0"
                    parseTiffPayload(data, offset + 10, exif);
                    break;
                }
            }
            offset += 2 + markerLen;
        }
        return exif;
    }

    // 2. Direct TIFF Detection ("II\x2A\x00" or "MM\x00\x2A")
    if ((buf[0] == 'I' && buf[1] == 'I') || (buf[0] == 'M' && buf[1] == 'M')) {
        parseTiffPayload(data, 0, exif);
        return exif;
    }

    // 3. WebP Detection ("RIFF....WEBP")
    if (sz >= 32 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
        buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') {
        int chunkOffset = 12;
        while (chunkOffset + 8 <= sz) {
            const char *fourcc = reinterpret_cast<const char*>(buf + chunkOffset);
            quint32 chunkLen = static_cast<quint32>(buf[chunkOffset + 4]) |
                               (static_cast<quint32>(buf[chunkOffset + 5]) << 8) |
                               (static_cast<quint32>(buf[chunkOffset + 6]) << 16) |
                               (static_cast<quint32>(buf[chunkOffset + 7]) << 24);

            if (qstrncmp(fourcc, "EXIF", 4) == 0) {
                int exifPayloadOffset = chunkOffset + 8;
                if (exifPayloadOffset + 6 <= sz &&
                    buf[exifPayloadOffset] == 'E' && buf[exifPayloadOffset + 1] == 'x' &&
                    buf[exifPayloadOffset + 2] == 'i' && buf[exifPayloadOffset + 3] == 'f' &&
                    buf[exifPayloadOffset + 4] == 0 && buf[exifPayloadOffset + 5] == 0) {
                    parseTiffPayload(data, exifPayloadOffset + 6, exif);
                } else {
                    parseTiffPayload(data, exifPayloadOffset, exif);
                }
                break;
            }
            chunkOffset += 8 + ((chunkLen + 1) & ~1);
        }
    }

    return exif;
}

void ExifParser::parseTiffPayload(const QByteArray &data, int tiffStart, ExifData &exif)
{
    const int sz = data.size();
    if (tiffStart + 8 > sz) return;

    const unsigned char *buf = reinterpret_cast<const unsigned char*>(data.constData());

    bool littleEndian = false;
    if (buf[tiffStart] == 'I' && buf[tiffStart + 1] == 'I') {
        littleEndian = true;
    } else if (buf[tiffStart] == 'M' && buf[tiffStart + 1] == 'M') {
        littleEndian = false;
    } else {
        return;
    }

    // Endian-aware safe reader functions with bounds checking
    auto readU16 = [&](int off) -> quint16 {
        if (off < 0 || off + 2 > sz) return 0;
        if (littleEndian) {
            return static_cast<quint16>(buf[off]) | (static_cast<quint16>(buf[off + 1]) << 8);
        }
        return (static_cast<quint16>(buf[off]) << 8) | static_cast<quint16>(buf[off + 1]);
    };

    auto readU32 = [&](int off) -> quint32 {
        if (off < 0 || off + 4 > sz) return 0;
        if (littleEndian) {
            return static_cast<quint32>(buf[off]) |
                   (static_cast<quint32>(buf[off + 1]) << 8) |
                   (static_cast<quint32>(buf[off + 2]) << 16) |
                   (static_cast<quint32>(buf[off + 3]) << 24);
        }
        return (static_cast<quint32>(buf[off]) << 24) |
               (static_cast<quint32>(buf[off + 1]) << 16) |
               (static_cast<quint32>(buf[off + 2]) << 8) |
               static_cast<quint32>(buf[off + 3]);
    };

    auto readRational = [&](int off, double &val) -> bool {
        if (off < 0 || off + 8 > sz) return false;
        quint32 num = readU32(off);
        quint32 den = readU32(off + 4);
        if (den == 0) return false;
        val = static_cast<double>(num) / static_cast<double>(den);
        return true;
    };

    auto readString = [&](int off, int count) -> QString {
        if (off < 0 || off + count > sz || count <= 0) return QString();
        const char *strBytes = reinterpret_cast<const char*>(buf + off);
        // Find actual null terminator or use full count
        int actualLen = 0;
        while (actualLen < count && strBytes[actualLen] != '\0') {
            actualLen++;
        }
        return QString::fromUtf8(strBytes, actualLen).trimmed();
    };

    // Check TIFF magic number 42 (0x002A)
    if (readU16(tiffStart + 2) != 42) {
        return;
    }

    exif.hasExif = true;

    quint32 ifd0Offset = readU32(tiffStart + 4);
    if (ifd0Offset == 0 || ifd0Offset >= static_cast<quint32>(sz) || tiffStart + static_cast<int>(ifd0Offset) >= sz) {
        return;
    }

    quint32 exifSubIfdOffset = 0;
    quint32 gpsSubIfdOffset = 0;

    // Helper to parse directory entries in an IFD
    auto parseDirectory = [&](int ifdPos, int category) {
        // category: 0 = IFD0, 1 = ExifSubIFD, 2 = GpsSubIFD
        int dirStart = tiffStart + ifdPos;
        if (dirStart < 0 || dirStart + 2 > sz) return;

        quint16 numEntries = readU16(dirStart);
        int currentEntry = dirStart + 2;

        for (int i = 0; i < numEntries && currentEntry + 12 <= sz; ++i, currentEntry += 12) {
            quint16 tag = readU16(currentEntry);
            quint16 type = readU16(currentEntry + 2);
            quint32 count = readU32(currentEntry + 4);

            int valOffset = currentEntry + 8;
            int typeSize = 1;
            if (type == 3) typeSize = 2; // SHORT
            else if (type == 4 || type == 9) typeSize = 4; // LONG / SLONG
            else if (type == 5 || type == 10) typeSize = 8; // RATIONAL

            qint64 totalBytes = static_cast<qint64>(count) * typeSize;
            int dataPos = valOffset;
            if (totalBytes > 4) {
                quint32 rawOff = readU32(valOffset);
                if (rawOff >= static_cast<quint32>(sz)) continue;
                dataPos = tiffStart + static_cast<int>(rawOff);
            }
            if (dataPos < 0 || dataPos >= sz) continue;

            if (category == 0) { // IFD0
                if (tag == 0x010F && (type == 2)) { // Make
                    exif.make = readString(dataPos, count);
                } else if (tag == 0x0110 && (type == 2)) { // Model
                    exif.model = readString(dataPos, count);
                } else if (tag == 0x0131 && (type == 2)) { // Software
                    exif.software = readString(dataPos, count);
                } else if (tag == 0x0132 && (type == 2)) { // DateTime
                    if (exif.dateTimeOriginal.isEmpty()) {
                        exif.dateTimeOriginal = readString(dataPos, count);
                    }
                } else if (tag == 0x0112 && (type == 3)) { // Orientation
                    exif.orientation = readU16(dataPos);
                } else if (tag == 0x8769) { // Exif SubIFD Pointer
                    exifSubIfdOffset = readU32(dataPos);
                } else if (tag == 0x8825) { // GPS SubIFD Pointer
                    gpsSubIfdOffset = readU32(dataPos);
                }
            } else if (category == 1) { // Exif SubIFD
                if (tag == 0x9003 && (type == 2)) { // DateTimeOriginal
                    exif.dateTimeOriginal = readString(dataPos, count);
                } else if (tag == 0x829A && (type == 5)) { // ExposureTime
                    double expTime = 0.0;
                    if (readRational(dataPos, expTime) && expTime > 0.0) {
                        if (expTime < 1.0) {
                            int denom = static_cast<int>(std::round(1.0 / expTime));
                            exif.exposureTime = QString("1/%1 s").arg(denom);
                        } else {
                            exif.exposureTime = QString("%1 s").arg(expTime, 0, 'f', 1);
                        }
                    }
                } else if (tag == 0x829D && (type == 5)) { // FNumber
                    double fn = 0.0;
                    if (readRational(dataPos, fn) && fn > 0.0) {
                        exif.fNumber = QString("f/%1").arg(fn, 0, 'f', 1);
                    }
                } else if (tag == 0x8827 && (type == 3 || type == 4)) { // ISO
                    exif.isoSpeed = (type == 3) ? readU16(dataPos) : static_cast<int>(readU32(dataPos));
                } else if (tag == 0x920A && (type == 5)) { // FocalLength
                    double fl = 0.0;
                    if (readRational(dataPos, fl)) {
                        exif.focalLength = fl;
                    }
                } else if (tag == 0xA405 && (type == 3 || type == 4)) { // 35mm equivalent
                    exif.focalLength35mm = (type == 3) ? readU16(dataPos) : static_cast<int>(readU32(dataPos));
                } else if (tag == 0x9209 && (type == 3)) { // Flash
                    exif.flash = readU16(dataPos) & 0x01;
                } else if (tag == 0xA434 && (type == 2)) { // LensModel
                    exif.lensModel = readString(dataPos, count);
                }
            } else if (category == 2) { // GPS SubIFD
                if (tag == 0x0001 && (type == 2)) { // LatitudeRef
                    QString ref = readString(dataPos, count);
                    if (!ref.isEmpty()) exif.gps.latitudeRef = ref.at(0).toUpper();
                } else if (tag == 0x0002 && (type == 5) && count >= 3) { // Latitude
                    double deg = 0.0, min = 0.0, sec = 0.0;
                    readRational(dataPos, deg);
                    readRational(dataPos + 8, min);
                    readRational(dataPos + 16, sec);
                    exif.gps.latitude = deg + (min / 60.0) + (sec / 3600.0);
                    exif.gps.hasGps = true;
                } else if (tag == 0x0003 && (type == 2)) { // LongitudeRef
                    QString ref = readString(dataPos, count);
                    if (!ref.isEmpty()) exif.gps.longitudeRef = ref.at(0).toUpper();
                } else if (tag == 0x0004 && (type == 5) && count >= 3) { // Longitude
                    double deg = 0.0, min = 0.0, sec = 0.0;
                    readRational(dataPos, deg);
                    readRational(dataPos + 8, min);
                    readRational(dataPos + 16, sec);
                    exif.gps.longitude = deg + (min / 60.0) + (sec / 3600.0);
                    exif.gps.hasGps = true;
                } else if (tag == 0x0005) { // AltitudeRef
                    exif.gps.belowSeaLevel = (buf[dataPos] == 1);
                } else if (tag == 0x0006 && (type == 5)) { // Altitude
                    double alt = 0.0;
                    if (readRational(dataPos, alt)) {
                        exif.gps.altitude = alt;
                    }
                }
            }
        }
    };

    // 1. Parse IFD0
    parseDirectory(static_cast<int>(ifd0Offset), 0);

    // 2. Parse Exif SubIFD if found
    if (exifSubIfdOffset > 0 && exifSubIfdOffset < static_cast<quint32>(sz) &&
        tiffStart + static_cast<int>(exifSubIfdOffset) < sz) {
        parseDirectory(static_cast<int>(exifSubIfdOffset), 1);
    }

    // 3. Parse GPS SubIFD if found
    if (gpsSubIfdOffset > 0 && gpsSubIfdOffset < static_cast<quint32>(sz) &&
        tiffStart + static_cast<int>(gpsSubIfdOffset) < sz) {
        parseDirectory(static_cast<int>(gpsSubIfdOffset), 2);
    }

    // Apply hemisphere signs to GPS decimal coordinates
    if (exif.gps.hasGps) {
        if (exif.gps.latitudeRef == 'S') {
            exif.gps.latitude = -std::abs(exif.gps.latitude);
        } else {
            exif.gps.latitude = std::abs(exif.gps.latitude);
        }
        if (exif.gps.longitudeRef == 'W') {
            exif.gps.longitude = -std::abs(exif.gps.longitude);
        } else {
            exif.gps.longitude = std::abs(exif.gps.longitude);
        }
    }
}
