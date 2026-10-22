/**
 * @file test_exif.cpp
 * @brief Automated unit test for ExifParser and GPS geolocation extraction.
 */

#include <QTest>
#include <QStandardPaths>
#include <QDataStream>
#include <cmath>
#include "core/services/exif_parser.h"

class TestExif : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testEmptyData();
    void testNonExifData();
    void testSyntheticExifAndGps();
    void testHemisphereSigns();
};

void TestExif::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestExif::testEmptyData()
{
    ExifData empty = ExifParser::parseData(QByteArray());
    QVERIFY(!empty.hasExif);
    QVERIFY(!empty.gps.hasGps);
    QCOMPARE(empty.gps.formattedDms(), QString());
    QCOMPARE(empty.gps.formattedDecimal(), QString());
    QVERIFY(empty.gps.openStreetMapUrl().isEmpty());
    QVERIFY(empty.gps.googleMapsUrl().isEmpty());
}

void TestExif::testNonExifData()
{
    QByteArray dummyJpeg;
    dummyJpeg.append('\xFF');
    dummyJpeg.append('\xD8'); // SOI
    dummyJpeg.append('\xFF');
    dummyJpeg.append('\xD9'); // EOI

    ExifData res = ExifParser::parseData(dummyJpeg);
    QVERIFY(!res.hasExif);
    QVERIFY(!res.gps.hasGps);
}

void TestExif::testSyntheticExifAndGps()
{
    // Build a valid Little-Endian TIFF payload with IFD0, ExifSubIFD, and GPSSubIFD
    QByteArray tiff;
    QDataStream stream(&tiff, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // TIFF Header at offset 0:
    stream << quint8('I') << quint8('I');
    stream << quint16(42);
    stream << quint32(8); // Offset to IFD0

    // IFD0 at offset 8:
    // 4 entries: Make (0x010F), Model (0x0110), ExifIFD (0x8769), GPSIFD (0x8825)
    quint16 ifd0Entries = 4;
    stream << ifd0Entries;

    // Entry 1: Make (0x010F), ASCII (2), count 6, offset 100
    stream << quint16(0x010F) << quint16(2) << quint32(6) << quint32(100);
    // Entry 2: Model (0x0110), ASCII (2), count 8, offset 110
    stream << quint16(0x0110) << quint16(2) << quint32(8) << quint32(110);
    // Entry 3: ExifSubIFD (0x8769), LONG (4), count 1, offset 130
    stream << quint16(0x8769) << quint16(4) << quint32(1) << quint32(130);
    // Entry 4: GPSInfo (0x8825), LONG (4), count 1, offset 200
    stream << quint16(0x8825) << quint16(4) << quint32(1) << quint32(200);
    stream << quint32(0); // Next IFD (none)

    // Pad to offset 100 for strings
    while (tiff.size() < 100) tiff.append('\0');
    // Offset 100: Make string "Canon\0"
    tiff.append("Canon\0", 6);

    // Offset 110: Model string "EOS R6\0"
    while (tiff.size() < 110) tiff.append('\0');
    tiff.append("EOS R6\0", 8);

    // Offset 130: ExifSubIFD
    while (tiff.size() < 130) tiff.append('\0');
    quint16 exifEntries = 3;
    stream.device()->seek(130);
    stream << exifEntries;
    // ExposureTime (0x829A), RATIONAL (5), count 1, offset 170
    stream << quint16(0x829A) << quint16(5) << quint32(1) << quint32(170);
    // FNumber (0x829D), RATIONAL (5), count 1, offset 178
    stream << quint16(0x829D) << quint16(5) << quint32(1) << quint32(178);
    // ISOSpeedRatings (0x8827), SHORT (3), count 1, value 100 inline
    stream << quint16(0x8827) << quint16(3) << quint32(1) << quint16(100) << quint16(0);
    stream << quint32(0); // Next IFD

    // Offset 170: ExposureTime 1/250
    while (tiff.size() < 170) tiff.append('\0');
    stream.device()->seek(170);
    stream << quint32(1) << quint32(250);
    // Offset 178: FNumber 28/10 (2.8)
    stream << quint32(28) << quint32(10);

    // Offset 200: GPSSubIFD
    while (tiff.size() < 200) tiff.append('\0');
    quint16 gpsEntries = 4;
    stream.device()->seek(200);
    stream << gpsEntries;
    // GPSLatitudeRef (0x0001), ASCII (2), count 2, value "N\0" inline
    stream << quint16(0x0001) << quint16(2) << quint32(2) << quint8('N') << quint8(0) << quint16(0);
    // GPSLatitude (0x0002), RATIONAL (5), count 3, offset 260
    stream << quint16(0x0002) << quint16(5) << quint32(3) << quint32(260);
    // GPSLongitudeRef (0x0003), ASCII (2), count 2, value "E\0" inline
    stream << quint16(0x0003) << quint16(2) << quint32(2) << quint8('E') << quint8(0) << quint16(0);
    // GPSLongitude (0x0004), RATIONAL (5), count 3, offset 284
    stream << quint16(0x0004) << quint16(5) << quint32(3) << quint32(284);
    stream << quint32(0); // Next IFD

    // Offset 260: Latitude rationals (45/1 deg, 27/1 min, 512/10 sec -> 45° 27' 51.2")
    while (tiff.size() < 260) tiff.append('\0');
    stream.device()->seek(260);
    stream << quint32(45) << quint32(1);
    stream << quint32(27) << quint32(1);
    stream << quint32(512) << quint32(10);

    // Offset 284: Longitude rationals (9/1 deg, 11/1 min, 224/10 sec -> 9° 11' 22.4")
    while (tiff.size() < 284) tiff.append('\0');
    stream.device()->seek(284);
    stream << quint32(9) << quint32(1);
    stream << quint32(11) << quint32(1);
    stream << quint32(224) << quint32(10);

    // Wrap in standard JPEG APP1 marker structure
    QByteArray jpeg;
    jpeg.append('\xFF'); jpeg.append('\xD8'); // SOI
    jpeg.append('\xFF'); jpeg.append('\xE1'); // APP1
    quint16 app1Len = static_cast<quint16>(2 + 6 + tiff.size());
    jpeg.append(static_cast<char>((app1Len >> 8) & 0xFF));
    jpeg.append(static_cast<char>(app1Len & 0xFF));
    jpeg.append("Exif\0\0", 6);
    jpeg.append(tiff);
    jpeg.append('\xFF'); jpeg.append('\xD9'); // EOI

    ExifData parsed = ExifParser::parseData(jpeg);
    QVERIFY(parsed.hasExif);
    QCOMPARE(parsed.make, QString("Canon"));
    QCOMPARE(parsed.model, QString("EOS R6"));
    QCOMPARE(parsed.isoSpeed, 100);
    QCOMPARE(parsed.fNumber, QString("f/2.8"));
    QCOMPARE(parsed.exposureTime, QString("1/250 s"));

    // Verify GPS
    QVERIFY(parsed.gps.hasGps);
    QCOMPARE(parsed.gps.latitudeRef, QChar('N'));
    QCOMPARE(parsed.gps.longitudeRef, QChar('E'));

    double expectedLat = 45.0 + (27.0 / 60.0) + (51.2 / 3600.0);
    double expectedLon = 9.0 + (11.0 / 60.0) + (22.4 / 3600.0);
    QVERIFY(std::abs(parsed.gps.latitude - expectedLat) < 0.0001);
    QVERIFY(std::abs(parsed.gps.longitude - expectedLon) < 0.0001);

    // Verify formatted outputs and map URLs
    QVERIFY(parsed.gps.formattedDms().contains("45° 27'"));
    QVERIFY(parsed.gps.formattedDms().contains("9° 11'"));
    QVERIFY(parsed.gps.openStreetMapUrl().toString().contains("openstreetmap.org"));
    QVERIFY(parsed.gps.openStreetMapUrl().toString().contains("mlat=45.4642"));
    QVERIFY(parsed.gps.googleMapsUrl().toString().contains("google.com/maps"));
}

void TestExif::testHemisphereSigns()
{
    ExifGpsData southWest;
    southWest.hasGps = true;
    southWest.latitude = -33.8688; // Sydney
    southWest.longitude = -70.6693; // Santiago
    southWest.latitudeRef = 'S';
    southWest.longitudeRef = 'W';

    QVERIFY(southWest.formattedDecimal().startsWith("-33.8688"));
    QVERIFY(southWest.openStreetMapUrl().toString().contains("mlat=-33.8688"));
    QVERIFY(southWest.googleMapsUrl().toString().contains("-33.8688"));
    QVERIFY(southWest.googleMapsUrl().toString().contains("-70.6693"));
}

QTEST_MAIN(TestExif)
#include "test_exif.moc"
