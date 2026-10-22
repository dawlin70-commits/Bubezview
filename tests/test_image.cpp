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
#include "core/services/image_processor.h"

class TestImage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testTransparencyModeDefault();
    void testSetTransparencyMode();
    void testRemoveTransparency();
    void testUndoRedoTransparency();
    void testResizeImage();
    void testResizeUndoRedo();
    void testAdjustImage();
    void testAdjustUndoRedo();
    void testViewportOpacity();
    void testImageProcessorPresets();
    void testImageProcessorBlend();
    void testImageProcessorConvolutions();
    void testImageProcessorPrivacy();
    void testImageProcessorHistogram();
    void testImageProcessorAutoEnhance();
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

void TestImage::testResizeImage()
{
    ImageViewer viewer;
    QImage img(400, 200, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QVERIFY(viewer.loadImage(img));
    QCOMPARE(viewer.originalImage().width(), 400);
    QCOMPARE(viewer.originalImage().height(), 200);
    QVERIFY(!viewer.canUndo());

    // Scale to 200x100
    viewer.resizeImage(200, 100, Qt::SmoothTransformation);
    QCOMPARE(viewer.originalImage().width(), 200);
    QCOMPARE(viewer.originalImage().height(), 100);
    QCOMPARE(viewer.isFitToWindow(), false);
    QCOMPARE(viewer.zoomFactor(), 1.0);
    QVERIFY(viewer.isModified());
    QVERIFY(viewer.canUndo());
}

void TestImage::testResizeUndoRedo()
{
    ImageViewer viewer;
    QImage img(300, 300, QImage::Format_RGB32);
    img.fill(Qt::green);
    viewer.loadImage(img);

    // Resize
    viewer.resizeImage(150, 150);
    QCOMPARE(viewer.originalImage().width(), 150);
    QCOMPARE(viewer.originalImage().height(), 150);

    // Undo
    viewer.undo();
    QCOMPARE(viewer.originalImage().width(), 300);
    QCOMPARE(viewer.originalImage().height(), 300);
    QVERIFY(viewer.canRedo());

    // Redo
    viewer.redo();
    QCOMPARE(viewer.originalImage().width(), 150);
    QCOMPARE(viewer.originalImage().height(), 150);
}

void TestImage::testAdjustImage()
{
    ImageViewer viewer;
    // 1. Test Brightness
    QImage grayImg(50, 50, QImage::Format_ARGB32);
    grayImg.fill(QColor(128, 128, 128));
    viewer.loadImage(grayImg);

    viewer.adjustImage(20, 0, 0); // Increase brightness
    QRgb p1 = viewer.originalImage().pixel(0, 0);
    QVERIFY(qRed(p1) > 128);
    QVERIFY(qGreen(p1) > 128);
    QVERIFY(qBlue(p1) > 128);
    QVERIFY(viewer.isModified());
    QVERIFY(viewer.canUndo());

    // 2. Test Saturation: -100 must result in a pure grayscale image (r == g == b)
    QImage colorImg(50, 50, QImage::Format_ARGB32);
    colorImg.fill(QColor(220, 110, 40));
    viewer.loadImage(colorImg);

    viewer.adjustImage(0, 0, -100);
    QRgb p2 = viewer.originalImage().pixel(0, 0);
    QCOMPARE(qRed(p2), qGreen(p2));
    QCOMPARE(qGreen(p2), qBlue(p2));

    // 3. Test Hue rotation: shifting pure red by 120° yields green
    QImage redImg(50, 50, QImage::Format_ARGB32);
    redImg.fill(QColor(255, 0, 0));
    viewer.loadImage(redImg);
    viewer.adjustImage(0, 0, 0, 120); // Hue +120°
    QRgb pHue = viewer.originalImage().pixel(0, 0);
    QVERIFY(qGreen(pHue) > qRed(pHue));
    QVERIFY(qGreen(pHue) > qBlue(pHue));

    // 4. Test RGB Channels & Warm/Cool Temperature
    QImage neutralImg(50, 50, QImage::Format_RGB32);
    neutralImg.fill(QColor(100, 100, 100));
    viewer.loadImage(neutralImg);
    viewer.adjustImage(0, 0, 0, 0, 50); // Warm temperature (+50)
    QRgb pWarm = viewer.originalImage().pixel(0, 0);
    QVERIFY(qRed(pWarm) > 100);
    QVERIFY(qBlue(pWarm) < 100);
    // Opaque RGB32 images must remain non-alpha after adjustment
    QVERIFY(!viewer.originalImage().hasAlphaChannel());

    viewer.loadImage(neutralImg);
    viewer.adjustImage(0, 0, 0, 0, -50); // Cool temperature (-50)
    QRgb pCool = viewer.originalImage().pixel(0, 0);
    QVERIFY(qBlue(pCool) > 100);
    QVERIFY(qRed(pCool) < 100);
}

void TestImage::testAdjustUndoRedo()
{
    ImageViewer viewer;
    QImage colorImg(50, 50, QImage::Format_ARGB32);
    colorImg.fill(QColor(100, 150, 200));
    viewer.loadImage(colorImg);

    viewer.adjustImage(10, 10, 10);
    QRgb modifiedPix = viewer.originalImage().pixel(0, 0);
    QVERIFY(modifiedPix != QColor(100, 150, 200).rgb());

    // Undo -> restored to exact original
    viewer.undo();
    QCOMPARE(viewer.originalImage().pixel(0, 0), QColor(100, 150, 200).rgb());
    QVERIFY(viewer.canRedo());

    // Redo -> modified state restored
    viewer.redo();
    QCOMPARE(viewer.originalImage().pixel(0, 0), modifiedPix);
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

void TestImage::testImageProcessorPresets()
{
    QImage base(60, 60, QImage::Format_RGB32);
    base.fill(QColor(120, 160, 200));

    // 1. Original preset must return unchanged image
    QImage orig = ImageProcessor::applyPreset(base, ImageProcessor::FilterPreset::Original);
    QCOMPARE(orig.pixel(0, 0), base.pixel(0, 0));

    // 2. FilmNoir must be pure monochrome (r == g == b)
    QImage noir = ImageProcessor::applyPreset(base, ImageProcessor::FilterPreset::FilmNoir);
    QRgb pNoir = noir.pixel(0, 0);
    QCOMPARE(qRed(pNoir), qGreen(pNoir));
    QCOMPARE(qGreen(pNoir), qBlue(pNoir));

    // 3. Vintage Sepia must have warm tone: Red > Green > Blue
    QImage sepia = ImageProcessor::applyPreset(base, ImageProcessor::FilterPreset::VintageSepia);
    QRgb pSepia = sepia.pixel(0, 0);
    QVERIFY(qRed(pSepia) >= qGreen(pSepia));
    QVERIFY(qGreen(pSepia) >= qBlue(pSepia));

    // 4. RedFilterBW vs base
    QImage redFilter = ImageProcessor::applyPreset(base, ImageProcessor::FilterPreset::RedFilterBW);
    QRgb pRed = redFilter.pixel(0, 0);
    QCOMPARE(qRed(pRed), qGreen(pRed));
    QCOMPARE(qGreen(pRed), qBlue(pRed));

    // 5. Preset display name must not be empty
    QVERIFY(!ImageProcessor::presetDisplayName(ImageProcessor::FilterPreset::TealAndOrange).isEmpty());
    QVERIFY(!ImageProcessor::presetDescription(ImageProcessor::FilterPreset::TealAndOrange).isEmpty());
}

void TestImage::testImageProcessorBlend()
{
    QImage imgA(40, 40, QImage::Format_RGB32);
    imgA.fill(QColor(100, 100, 100));

    QImage imgB(40, 40, QImage::Format_RGB32);
    imgB.fill(QColor(200, 200, 200));

    // Factor 0.0 -> imgA
    QImage b0 = ImageProcessor::blend(imgA, imgB, 0.0);
    QCOMPARE(qRed(b0.pixel(0, 0)), 100);

    // Factor 1.0 -> imgB
    QImage b1 = ImageProcessor::blend(imgA, imgB, 1.0);
    QCOMPARE(qRed(b1.pixel(0, 0)), 200);

    // Factor 0.5 -> ~150
    QImage bMid = ImageProcessor::blend(imgA, imgB, 0.5);
    int midVal = qRed(bMid.pixel(0, 0));
    QVERIFY(midVal >= 148 && midVal <= 152);
}

void TestImage::testImageProcessorConvolutions()
{
    QImage base(50, 50, QImage::Format_RGB32);
    base.fill(QColor(128, 128, 128));

    // Invert: 128 -> 127
    QImage inv = ImageProcessor::applyInvert(base);
    QCOMPARE(qRed(inv.pixel(0, 0)), 127);

    // Vignette: center must be unchanged, corners must be darkened
    QImage vig = ImageProcessor::applyVignette(base, 50, 40);
    QRgb centerPix = vig.pixel(25, 25);
    QRgb cornerPix = vig.pixel(0, 0);
    QCOMPARE(qRed(centerPix), 128);
    QVERIFY(qRed(cornerPix) < 128);

    // Sharpen must preserve dimensions
    QImage sharp = ImageProcessor::applySharpen(base, 50);
    QCOMPARE(sharp.size(), base.size());

    // Blur must preserve dimensions
    QImage blur = ImageProcessor::applyBlur(base, 5);
    QCOMPARE(blur.size(), base.size());
}

void TestImage::testImageProcessorPrivacy()
{
    QImage base(80, 80, QImage::Format_RGB32);
    base.fill(QColor(255, 255, 255));
    // Draw distinct pattern in rect(20, 20, 40, 40)
    for (int y = 20; y < 60; ++y) {
        for (int x = 20; x < 60; ++x) {
            base.setPixel(x, y, qRgb(x * 4, y * 4, 100));
        }
    }

    // Pixelate region
    QImage pix = ImageProcessor::pixelateRegion(base, QRect(20, 20, 40, 40), 16);
    QCOMPARE(pix.size(), base.size());
    // Outside region must remain pure white
    QCOMPARE(pix.pixel(5, 5), qRgb(255, 255, 255));
    // Inside region must have grouped blocks
    QRgb b1 = pix.pixel(22, 22);
    QRgb b2 = pix.pixel(25, 25);
    QCOMPARE(b1, b2);

    // Blur region
    QImage blurred = ImageProcessor::blurRegion(base, QRect(20, 20, 40, 40), 8);
    QCOMPARE(blurred.size(), base.size());
    QCOMPARE(blurred.pixel(5, 5), qRgb(255, 255, 255));
}

void TestImage::testImageProcessorHistogram()
{
    QImage base(50, 50, QImage::Format_RGB32);
    base.fill(QColor(0, 0, 0)); // Pure black image

    QVector<int> r, g, b, luma;
    bool shadowsClipped = false, hlClipped = false;
    ImageProcessor::computeHistogram(base, r, g, b, luma, shadowsClipped, hlClipped);

    QCOMPARE(r[0], 2500); // 50x50 pixels at 0
    QCOMPARE(luma[0], 2500);
    QVERIFY(shadowsClipped);
    QVERIFY(!hlClipped);

    // Pure white image
    base.fill(QColor(255, 255, 255));
    ImageProcessor::computeHistogram(base, r, g, b, luma, shadowsClipped, hlClipped);
    QCOMPARE(luma[255], 2500);
    QVERIFY(!shadowsClipped);
    QVERIFY(hlClipped);
}

void TestImage::testImageProcessorAutoEnhance()
{
    // 1. Null image handling
    QImage nullImg;
    QImage resNull = ImageProcessor::autoEnhance(nullImg);
    QVERIFY(resNull.isNull());

    // 2. Intensity = 0 returns identical image
    QImage testImg(40, 40, QImage::Format_RGB32);
    testImg.fill(QColor(80, 100, 120));
    ImageProcessor::AutoEnhanceOptions optsZero;
    optsZero.intensity = 0;
    QImage resZero = ImageProcessor::autoEnhance(testImg, optsZero);
    QCOMPARE(resZero.pixel(20, 20), testImg.pixel(20, 20));

    // 3. Highlight protection:
    // Create an image with gradient range and a bright blown-out spot (> 230)
    QImage hlImg(50, 50, QImage::Format_RGB32);
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            hlImg.setPixel(x, y, qRgb(10 + x * 3, 10 + x * 3, 10 + x * 3));
        }
    }
    // Blown light spot
    hlImg.setPixel(25, 25, qRgb(252, 252, 252));

    ImageProcessor::AutoEnhanceOptions optsProtected;
    optsProtected.protectHighlights = true;
    QImage resProtected = ImageProcessor::autoEnhance(hlImg, optsProtected);

    // Blown highlight must be smoothly rolled off without clipping or overflow
    QRgb pProt = resProtected.pixel(25, 25);
    QVERIFY(qRed(pProt) >= 200 && qRed(pProt) <= 255);
    QVERIFY(qGreen(pProt) >= 200 && qGreen(pProt) <= 255);
    QVERIFY(qBlue(pProt) >= 200 && qBlue(pProt) <= 255);

    // Midtone pixel must be valid and non-zero
    QRgb midProt = resProtected.pixel(20, 20);
    QVERIFY(qRed(midProt) > 0);

    // 4. Intensity blending (50% intensity is halfway between 0% and 100%)
    ImageProcessor::AutoEnhanceOptions opts100;
    opts100.intensity = 100;
    QImage res100 = ImageProcessor::autoEnhance(testImg, opts100);

    ImageProcessor::AutoEnhanceOptions opts50;
    opts50.intensity = 50;
    QImage res50 = ImageProcessor::autoEnhance(testImg, opts50);

    int origR = qRed(testImg.pixel(20, 20));
    int fullR = qRed(res100.pixel(20, 20));
    int halfR = qRed(res50.pixel(20, 20));
    int expectedHalf = origR + (fullR - origR) / 2;
    QVERIFY(std::abs(halfR - expectedHalf) <= 3);

    // 5. Neutral sample point calibration (Eyedropper)
    QImage castImg(30, 30, QImage::Format_RGB32);
    castImg.fill(QColor(160, 120, 90)); // Heavy warm cast
    ImageProcessor::AutoEnhanceOptions optsEyedropper;
    optsEyedropper.neutralSamplePoint = QPoint(15, 15);
    QImage resEyedropper = ImageProcessor::autoEnhance(castImg, optsEyedropper);

    QRgb calibratedPix = resEyedropper.pixel(15, 15);
    int calibR = qRed(calibratedPix);
    int calibG = qGreen(calibratedPix);
    int calibB = qBlue(calibratedPix);
    // After calibration to sample point, color difference across channels must be greatly reduced
    int origDiff = std::abs(160 - 90);
    int calibDiff = std::max({std::abs(calibR - calibG), std::abs(calibG - calibB), std::abs(calibR - calibB)});
    QVERIFY(calibDiff < origDiff);

    // 6. Profiles smoke tests
    ImageProcessor::AutoEnhanceOptions optsProfile;
    optsProfile.profile = ImageProcessor::AutoEnhanceOptions::Profile::Natural;
    QVERIFY(!ImageProcessor::autoEnhance(testImg, optsProfile).isNull());

    optsProfile.profile = ImageProcessor::AutoEnhanceOptions::Profile::Vivid;
    QVERIFY(!ImageProcessor::autoEnhance(testImg, optsProfile).isNull());

    optsProfile.profile = ImageProcessor::AutoEnhanceOptions::Profile::BacklightShadows;
    QVERIFY(!ImageProcessor::autoEnhance(testImg, optsProfile).isNull());

    optsProfile.profile = ImageProcessor::AutoEnhanceOptions::Profile::DehazeClear;
    QVERIFY(!ImageProcessor::autoEnhance(testImg, optsProfile).isNull());

    // 7. Skin Tone Protection Verification
    QImage skinImg(40, 40, QImage::Format_RGB32);
    skinImg.fill(QColor(195, 142, 112)); // Natural skin tone
    ImageProcessor::AutoEnhanceOptions optsSkinOff;
    optsSkinOff.protectSkinTones = false;
    QImage resSkinOff = ImageProcessor::autoEnhance(skinImg, optsSkinOff);

    ImageProcessor::AutoEnhanceOptions optsSkinOn;
    optsSkinOn.protectSkinTones = true;
    QImage resSkinOn = ImageProcessor::autoEnhance(skinImg, optsSkinOn);

    QRgb pixOff = resSkinOff.pixel(20, 20);
    QRgb pixOn = resSkinOn.pixel(20, 20);
    // Skin Tone Protection must actively enhance warmth and vitality (higher R, controlled B)
    QVERIFY(qRed(pixOn) >= qRed(pixOff));
    QVERIFY(pixOn != pixOff);

    // 8. Shadow Denoise Verification
    QImage noisyShadowImg(30, 30, QImage::Format_RGB32);
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 30; ++x) {
            int noise = ((x + y) % 2 == 0) ? 4 : -4;
            noisyShadowImg.setPixel(x, y, qRgb(40 + noise, 40 + noise, 40 + noise));
        }
    }
    ImageProcessor::AutoEnhanceOptions optsDenoiseOff;
    optsDenoiseOff.autoDenoiseShadows = false;
    optsDenoiseOff.correctWhiteBalance = false;
    QImage resDenoiseOff = ImageProcessor::autoEnhance(noisyShadowImg, optsDenoiseOff);

    ImageProcessor::AutoEnhanceOptions optsDenoiseOn;
    optsDenoiseOn.autoDenoiseShadows = true;
    optsDenoiseOn.correctWhiteBalance = false;
    QImage resDenoiseOn = ImageProcessor::autoEnhance(noisyShadowImg, optsDenoiseOn);

    int diffOff = std::abs(qRed(resDenoiseOff.pixel(15, 15)) - qRed(resDenoiseOff.pixel(16, 15)));
    int diffOn = std::abs(qRed(resDenoiseOn.pixel(15, 15)) - qRed(resDenoiseOn.pixel(16, 15)));
    // Denoised image must have significantly smaller local delta between adjacent pixels
    QVERIFY(diffOn < diffOff);

    // 9. Center-Weighted Metering Verification
    // Create high-contrast backlit scene: dark center subject (50) surrounded by bright background (230)
    QImage backlitImg(60, 60, QImage::Format_RGB32);
    backlitImg.fill(QColor(230, 230, 230)); // Bright ambient background
    for (int y = 15; y < 45; ++y) {
        for (int x = 15; x < 45; ++x) {
            backlitImg.setPixel(x, y, qRgb(50, 50, 50)); // Dark central subject
        }
    }

    ImageProcessor::AutoEnhanceOptions optsGlobal;
    optsGlobal.centerWeighted = false;
    optsGlobal.correctWhiteBalance = false;
    QImage resGlobal = ImageProcessor::autoEnhance(backlitImg, optsGlobal);

    ImageProcessor::AutoEnhanceOptions optsCenter;
    optsCenter.centerWeighted = true;
    optsCenter.correctWhiteBalance = false;
    QImage resCenter = ImageProcessor::autoEnhance(backlitImg, optsCenter);

    // Center-weighted mode detects underexposed subject in center and lifts its midtones/shadows more
    int centerLumaGlobal = qRed(resGlobal.pixel(30, 30));
    int centerLumaCenter = qRed(resCenter.pixel(30, 30));
    QVERIFY(centerLumaCenter > centerLumaGlobal);
}

QTEST_MAIN(TestImage)
#include "test_image.moc"
