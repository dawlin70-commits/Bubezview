/**
 * @file test_image.cpp
 * @brief Automated unit test for ImageViewer transparency removal and modes.
 */

#include <QTest>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include "viewers/image/image_viewer.h"

class TestImage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testTransparencyModeDefault();
    void testSetTransparencyMode();
    void testRemoveTransparency();
    void testUndoRedoTransparency();
    void testViewportOpacity();
};

void TestImage::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    // Clean initial state for tests
    QSettings settings("Bubez", "BubezView");
    settings.remove("image/transparency_mode");
}

void TestImage::testTransparencyModeDefault()
{
    ImageViewer viewer;
    // Default should be White (solid opaque background removing transparency)
    QCOMPARE(viewer.transparencyMode(), ImageViewer::TransparencyMode::White);
}

void TestImage::testSetTransparencyMode()
{
    ImageViewer viewer;
    viewer.setTransparencyMode(ImageViewer::TransparencyMode::Checkerboard);
    QCOMPARE(viewer.transparencyMode(), ImageViewer::TransparencyMode::Checkerboard);

    viewer.setTransparencyMode(ImageViewer::TransparencyMode::Theme);
    QCOMPARE(viewer.transparencyMode(), ImageViewer::TransparencyMode::Theme);

    viewer.setTransparencyMode(ImageViewer::TransparencyMode::Black);
    QCOMPARE(viewer.transparencyMode(), ImageViewer::TransparencyMode::Black);

    viewer.setTransparencyMode(ImageViewer::TransparencyMode::White);
    QCOMPARE(viewer.transparencyMode(), ImageViewer::TransparencyMode::White);
}

void TestImage::testRemoveTransparency()
{
    ImageViewer viewer;

    // Create an image with an alpha channel
    QImage transparentImg(100, 100, QImage::Format_ARGB32);
    transparentImg.fill(Qt::transparent);
    {
        QPainter p(&transparentImg);
        p.fillRect(10, 10, 80, 80, Qt::red);
    }
    QVERIFY(transparentImg.hasAlphaChannel());

    QVERIFY(viewer.loadImage(transparentImg));
    QVERIFY(viewer.originalImage().hasAlphaChannel());
    QVERIFY(!viewer.canUndo());

    // Permanently remove transparency (flatten to white)
    viewer.removeTransparency(Qt::white);

    // Alpha channel must now be eliminated
    QVERIFY(!viewer.originalImage().hasAlphaChannel());
    QCOMPARE(viewer.originalImage().format(), QImage::Format_RGB32);
    // Background pixel (0,0) must now be pure white (0xffffffff)
    QCOMPARE(viewer.originalImage().pixelColor(0, 0), QColor(Qt::white));
    // Foreground pixel (50,50) must remain red
    QCOMPARE(viewer.originalImage().pixelColor(50, 50), QColor(Qt::red));
    QVERIFY(viewer.canUndo());
}

void TestImage::testUndoRedoTransparency()
{
    ImageViewer viewer;

    QImage transparentImg(50, 50, QImage::Format_ARGB32);
    transparentImg.fill(Qt::transparent);
    viewer.loadImage(transparentImg);
    QVERIFY(viewer.originalImage().hasAlphaChannel());

    // Remove transparency
    viewer.removeTransparency(Qt::white);
    QVERIFY(!viewer.originalImage().hasAlphaChannel());
    QVERIFY(viewer.canUndo());

    // Undo -> transparency restored
    viewer.undo();
    QVERIFY(viewer.originalImage().hasAlphaChannel());
    QVERIFY(viewer.canRedo());

    // Redo -> transparency removed again
    viewer.redo();
    QVERIFY(!viewer.originalImage().hasAlphaChannel());
}

void TestImage::testViewportOpacity()
{
    ImageViewer viewer;
    QVERIFY(viewer.scrollArea() != nullptr);
    QVERIFY(viewer.scrollArea()->viewport() != nullptr);
    // Viewport must auto fill background so that underlying desktop or parents never show through
    QVERIFY(viewer.scrollArea()->viewport()->autoFillBackground());
    // Viewport must NOT set WA_OpaquePaintEvent (which skips background clearing and smears transition overlays)
    QVERIFY(!viewer.scrollArea()->viewport()->testAttribute(Qt::WA_OpaquePaintEvent));
    // Canvas background color must have full alpha (255)
    QCOMPARE(viewer.canvasBackgroundColor().alpha(), 255);
}

QTEST_MAIN(TestImage)
#include "test_image.moc"
