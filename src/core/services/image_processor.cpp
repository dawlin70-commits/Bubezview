/**
 * @file image_processor.cpp
 * @brief High-performance image processing engine implementation for BubezView.
 */

#include "image_processor.h"
#include <QCoreApplication>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {

/**
 * @brief Fast integer conversion from RGB to HSV.
 */
inline void rgbToHsv(int r, int g, int b, int &h, int &s, int &v)
{
    int cmax = std::max({r, g, b});
    int cmin = std::min({r, g, b});
    int delta = cmax - cmin;
    v = cmax;
    if (cmax == 0 || delta == 0) {
        s = 0;
        h = 0;
        return;
    }
    s = (255 * delta) / cmax;
    if (r == cmax) {
        h = (60 * (g - b)) / delta;
    } else if (g == cmax) {
        h = 120 + (60 * (b - r)) / delta;
    } else {
        h = 240 + (60 * (r - g)) / delta;
    }
    if (h < 0) {
        h += 360;
    }
}

/**
 * @brief Fast integer conversion from HSV to RGB.
 */
inline void hsvToRgb(int h, int s, int v, int &r, int &g, int &b)
{
    if (s == 0) {
        r = g = b = v;
        return;
    }
    h = (h % 360 + 360) % 360;
    int hi = h / 60;
    int f = h % 60;
    int p = (v * (255 - s)) / 255;
    int q = (v * (255 * 60 - s * f)) / (255 * 60);
    int t = (v * (255 * 60 - s * (60 - f))) / (255 * 60);
    switch (hi) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

} // anonymous namespace

QImage ImageProcessor::adjust(const QImage &src,
                             int brightness, int contrast, int saturation,
                             int hue, int temperature,
                             int red, int green, int blue,
                             int highlights, int shadows)
{
    if (src.isNull()) return QImage();
    if (brightness == 0 && contrast == 0 && saturation == 0 &&
        hue == 0 && temperature == 0 && red == 0 && green == 0 && blue == 0 &&
        highlights == 0 && shadows == 0) {
        return src;
    }

    const QImage::Format targetFormat = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage result = (src.format() == targetFormat) ? src.copy() : src.convertToFormat(targetFormat);
    const int width = result.width();
    const int height = result.height();

    // 1. Precalculate 256-byte Look-Up Tables (LUT)
    unsigned char lutR[256];
    unsigned char lutG[256];
    unsigned char lutB[256];

    double contrastFactor = (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast));
    double brightnessDelta = (brightness * 255.0) / 100.0;

    double tempR = (temperature >= 0) ? (temperature * 0.65) : (temperature * 0.50);
    double tempG = (temperature >= 0) ? (temperature * 0.15) : (temperature * 0.05);
    double tempB = -temperature * 0.65;

    double effRed = red + tempR;
    double effGreen = green + tempG;
    double effBlue = blue + tempB;

    double redDelta = (effRed * 255.0) / 100.0;
    double greenDelta = (effGreen * 255.0) / 100.0;
    double blueDelta = (effBlue * 255.0) / 100.0;

    for (int i = 0; i < 256; ++i) {
        double val = contrastFactor * (i - 128) + 128 + brightnessDelta;

        // Shadow lift (boosts low values i < 128)
        if (shadows != 0 && i < 128) {
            double shadowWeight = (128.0 - i) / 128.0;
            val += (shadows * 0.5) * shadowWeight;
        }

        // Highlight recovery (attenuates high values i > 128)
        if (highlights != 0 && i > 128) {
            double highlightWeight = (i - 128.0) / 128.0;
            val -= (highlights * 0.5) * highlightWeight;
        }

        int vr = static_cast<int>(std::round(val + redDelta));
        int vg = static_cast<int>(std::round(val + greenDelta));
        int vb = static_cast<int>(std::round(val + blueDelta));

        lutR[i] = static_cast<unsigned char>(qBound(0, vr, 255));
        lutG[i] = static_cast<unsigned char>(qBound(0, vg, 255));
        lutB[i] = static_cast<unsigned char>(qBound(0, vb, 255));
    }

    const bool adjustSat = (saturation != 0);
    const int satFactorFP = ((100 + saturation) * 1024) / 100;
    const bool adjustHue = (hue != 0);

    // 2. Scanline processing with fixed-point arithmetic
    for (int y = 0; y < height; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb pixel = line[x];
            int a = qAlpha(pixel);
            int r = lutR[qRed(pixel)];
            int g = lutG[qGreen(pixel)];
            int b = lutB[qBlue(pixel)];

            if (adjustHue) {
                int hVal, sVal, vVal;
                rgbToHsv(r, g, b, hVal, sVal, vVal);
                hVal = (hVal + hue + 360) % 360;
                hsvToRgb(hVal, sVal, vVal, r, g, b);
            }

            if (adjustSat) {
                // ITU-R BT.601 luminance in 10-bit fixed-point (306 + 601 + 117 = 1024)
                int gray = (306 * r + 601 * g + 117 * b) >> 10;
                r = qBound(0, gray + (((r - gray) * satFactorFP + 512) >> 10), 255);
                g = qBound(0, gray + (((g - gray) * satFactorFP + 512) >> 10), 255);
                b = qBound(0, gray + (((b - gray) * satFactorFP + 512) >> 10), 255);
            }

            line[x] = qRgba(r, g, b, a);
        }
    }

    return result;
}

QImage ImageProcessor::blend(const QImage &imgA, const QImage &imgB, double factor)
{
    if (imgA.isNull()) return imgB;
    if (imgB.isNull()) return imgA;
    if (factor <= 0.001) return imgA;
    if (std::abs(factor - 1.0) < 0.001) return imgB;

    if (imgA.size() != imgB.size()) return imgA;

    const QImage::Format targetFormat = imgA.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage a = (imgA.format() == targetFormat) ? imgA : imgA.convertToFormat(targetFormat);
    QImage b = (imgB.format() == targetFormat) ? imgB : imgB.convertToFormat(targetFormat);

    QImage result(a.size(), targetFormat);
    const int width = a.width();
    const int height = a.height();

    // Fixed-point 10-bit blending factor (supports factor > 1.0 up to 1.50 for punchy extrapolation)
    const int factorFP = static_cast<int>(factor * 1024.0);
    const int invFactorFP = 1024 - factorFP;

    for (int y = 0; y < height; ++y) {
        const QRgb *lineA = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const QRgb *lineB = reinterpret_cast<const QRgb*>(b.constScanLine(y));
        QRgb *lineOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < width; ++x) {
            QRgb pa = lineA[x];
            QRgb pb = lineB[x];

            int r = (qRed(pa) * invFactorFP + qRed(pb) * factorFP) >> 10;
            int g = (qGreen(pa) * invFactorFP + qGreen(pb) * factorFP) >> 10;
            int bl = (qBlue(pa) * invFactorFP + qBlue(pb) * factorFP) >> 10;
            int al = (qAlpha(pa) * invFactorFP + qAlpha(pb) * factorFP) >> 10;

            lineOut[x] = qRgba(qBound(0, r, 255), qBound(0, g, 255), qBound(0, bl, 255), qBound(0, al, 255));
        }
    }

    return result;
}

QImage ImageProcessor::applyPreset(const QImage &src, FilterPreset preset)
{
    if (src.isNull() || preset == FilterPreset::Original) {
        return src;
    }

    switch (preset) {
        case FilterPreset::TealAndOrange: {
            // Cool shadows (teal), warm highlights (orange), punchy contrast
            QImage adjusted = adjust(src, 5, 20, 25, 0, 15, 12, 0, -8);
            // Scanline tone curve: boost cyan in shadows, orange in highlights
            const int width = adjusted.width();
            const int height = adjusted.height();
            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(adjusted.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    int r = qRed(p);
                    int g = qGreen(p);
                    int b = qBlue(p);
                    int luma = (306 * r + 601 * g + 117 * b) >> 10;
                    if (luma < 128) {
                        // Shadows: push cyan/teal
                        int shadowWeight = (128 - luma);
                        b = qBound(0, b + (shadowWeight * 25) / 128, 255);
                        g = qBound(0, g + (shadowWeight * 12) / 128, 255);
                        r = qBound(0, r - (shadowWeight * 15) / 128, 255);
                    } else {
                        // Highlights: push warm amber/orange
                        int hlWeight = (luma - 128);
                        r = qBound(0, r + (hlWeight * 30) / 128, 255);
                        g = qBound(0, g + (hlWeight * 10) / 128, 255);
                        b = qBound(0, b - (hlWeight * 20) / 128, 255);
                    }
                    line[x] = qRgba(r, g, b, qAlpha(p));
                }
            }
            return adjusted;
        }

        case FilterPreset::VintageSepia: {
            const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
            QImage result = src.convertToFormat(fmt);
            const int width = result.width();
            const int height = result.height();

            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    int r = qRed(p);
                    int g = qGreen(p);
                    int b = qBlue(p);

                    int sr = static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b);
                    int sg = static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b);
                    int sb = static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b);

                    line[x] = qRgba(qBound(0, sr, 255), qBound(0, sg, 255), qBound(0, sb, 255), qAlpha(p));
                }
            }
            return adjust(result, 5, 10, 0);
        }

        case FilterPreset::RetroFilm: {
            // Lifted blacks (faded matte shadows), warm tone, subtle desaturation
            QImage adjusted = adjust(src, 2, -5, -15, 0, 25, 8, 4, -10);
            const int width = adjusted.width();
            const int height = adjusted.height();
            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(adjusted.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    int r = qRed(p);
                    int g = qGreen(p);
                    int b = qBlue(p);
                    // Lift deepest blacks to 24 (matte film look)
                    r = 24 + ((r * (255 - 24)) >> 8);
                    g = 24 + ((g * (255 - 24)) >> 8);
                    b = 28 + ((b * (255 - 28)) >> 8);
                    line[x] = qRgba(r, g, b, qAlpha(p));
                }
            }
            return applyVignette(adjusted, 30, 80);
        }

        case FilterPreset::FilmNoir: {
            // High contrast, deep crushed blacks, pure monochrome
            return adjust(src, -5, 35, -100);
        }

        case FilterPreset::RedFilterBW: {
            // Optical red filter simulation: dramatically darkens blue sky, brightens reds
            const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
            QImage result = src.convertToFormat(fmt);
            const int width = result.width();
            const int height = result.height();

            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    // Red filter weighting: 70% red, 20% green, 10% blue
                    int gray = (716 * qRed(p) + 205 * qGreen(p) + 103 * qBlue(p)) >> 10;
                    line[x] = qRgba(gray, gray, gray, qAlpha(p));
                }
            }
            return adjust(result, 0, 25, 0);
        }

        case FilterPreset::OrangeFilterBW: {
            // Optical orange filter simulation: smooths human skin, moderate sky darkening
            const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
            QImage result = src.convertToFormat(fmt);
            const int width = result.width();
            const int height = result.height();

            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    // Orange filter weighting: 55% red, 40% green, 5% blue
                    int gray = (563 * qRed(p) + 410 * qGreen(p) + 51 * qBlue(p)) >> 10;
                    line[x] = qRgba(gray, gray, gray, qAlpha(p));
                }
            }
            return adjust(result, 5, 15, 0);
        }

        case FilterPreset::GoldenHour: {
            // Rich sunset amber, boosted warmth and vibrancy
            return adjust(src, 5, 12, 25, 0, 45, 18, 5, -20);
        }

        case FilterPreset::NordicCool: {
            // Crisp glacial cyan, reduced saturation, clean contrast
            return adjust(src, 0, 15, -15, 0, -45, -12, 2, 25);
        }

        case FilterPreset::CrossProcess: {
            // High saturation, cross-processed film look
            return adjust(src, 5, 30, 40, 0, -10, 15, -5, 20);
        }

        case FilterPreset::Cyberpunk: {
            // High contrast neon shift (magenta/cyan)
            return adjust(src, 0, 25, 35, 130, 0, 15, -10, 25);
        }

        case FilterPreset::DocumentEnhance: {
            // Document scanner enhancer: high contrast, sharp text, clean paper white
            const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
            QImage result = src.convertToFormat(fmt);
            const int width = result.width();
            const int height = result.height();

            for (int y = 0; y < height; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    QRgb p = line[x];
                    int gray = (306 * qRed(p) + 601 * qGreen(p) + 117 * qBlue(p)) >> 10;
                    // Dynamic contrast expansion with high threshold
                    if (gray > 165) {
                        gray = qBound(0, gray + ((gray - 165) * 2), 255);
                    } else {
                        gray = qBound(0, (gray * 180) / 255, 255);
                    }
                    line[x] = qRgba(gray, gray, gray, qAlpha(p));
                }
            }
            return applySharpen(result, 60);
        }

        default:
            return src;
    }
}

QImage ImageProcessor::applySharpen(const QImage &src, int amount)
{
    if (src.isNull() || amount <= 0) return src;

    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage in = (src.format() == fmt) ? src : src.convertToFormat(fmt);
    QImage out(in.size(), fmt);

    const int width = in.width();
    const int height = in.height();
    if (width < 3 || height < 3) return src;

    // Fixed point factor: k = amount / 100.0
    // Center pixel = 1.0 + 4*k, 4 neighbors = -k
    const int kFP = (amount * 256) / 100;
    const int centerFP = 256 + 4 * kFP;

    // Copy top and bottom borders
    memcpy(out.scanLine(0), in.constScanLine(0), width * sizeof(QRgb));
    memcpy(out.scanLine(height - 1), in.constScanLine(height - 1), width * sizeof(QRgb));

    for (int y = 1; y < height - 1; ++y) {
        const QRgb *prev = reinterpret_cast<const QRgb*>(in.constScanLine(y - 1));
        const QRgb *curr = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        const QRgb *next = reinterpret_cast<const QRgb*>(in.constScanLine(y + 1));
        QRgb *outLine = reinterpret_cast<QRgb*>(out.scanLine(y));

        outLine[0] = curr[0];
        outLine[width - 1] = curr[width - 1];

        for (int x = 1; x < width - 1; ++x) {
            QRgb c = curr[x];
            QRgb u = prev[x];
            QRgb d = next[x];
            QRgb l = curr[x - 1];
            QRgb r = curr[x + 1];

            int redVal = (qRed(c) * centerFP - (qRed(u) + qRed(d) + qRed(l) + qRed(r)) * kFP) >> 8;
            int greenVal = (qGreen(c) * centerFP - (qGreen(u) + qGreen(d) + qGreen(l) + qGreen(r)) * kFP) >> 8;
            int blueVal = (qBlue(c) * centerFP - (qBlue(u) + qBlue(d) + qBlue(l) + qBlue(r)) * kFP) >> 8;

            outLine[x] = qRgba(qBound(0, redVal, 255),
                               qBound(0, greenVal, 255),
                               qBound(0, blueVal, 255),
                               qAlpha(c));
        }
    }

    return out;
}

QImage ImageProcessor::applyBlur(const QImage &src, int radius)
{
    if (src.isNull() || radius <= 0) return src;
    radius = qBound(1, radius, 30);

    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage img = (src.format() == fmt) ? src : src.convertToFormat(fmt);
    const int width = img.width();
    const int height = img.height();

    // Fast horizontal blur pass
    QImage horiz(img.size(), fmt);
    const int window = 2 * radius + 1;

    for (int y = 0; y < height; ++y) {
        const QRgb *inLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        QRgb *outLine = reinterpret_cast<QRgb*>(horiz.scanLine(y));

        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -radius; i <= radius; ++i) {
            int cx = qBound(0, i, width - 1);
            QRgb p = inLine[cx];
            sumR += qRed(p);
            sumG += qGreen(p);
            sumB += qBlue(p);
            sumA += qAlpha(p);
        }

        for (int x = 0; x < width; ++x) {
            outLine[x] = qRgba(sumR / window, sumG / window, sumB / window, sumA / window);

            int xRemove = qBound(0, x - radius, width - 1);
            int xAdd = qBound(0, x + radius + 1, width - 1);

            QRgb pRemove = inLine[xRemove];
            QRgb pAdd = inLine[xAdd];

            sumR += qRed(pAdd) - qRed(pRemove);
            sumG += qGreen(pAdd) - qGreen(pRemove);
            sumB += qBlue(pAdd) - qBlue(pRemove);
            sumA += qAlpha(pAdd) - qAlpha(pRemove);
        }
    }

    // Fast vertical blur pass
    QImage result(img.size(), fmt);
    for (int x = 0; x < width; ++x) {
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -radius; i <= radius; ++i) {
            int cy = qBound(0, i, height - 1);
            QRgb p = reinterpret_cast<const QRgb*>(horiz.constScanLine(cy))[x];
            sumR += qRed(p);
            sumG += qGreen(p);
            sumB += qBlue(p);
            sumA += qAlpha(p);
        }

        for (int y = 0; y < height; ++y) {
            reinterpret_cast<QRgb*>(result.scanLine(y))[x] = qRgba(sumR / window, sumG / window, sumB / window, sumA / window);

            int yRemove = qBound(0, y - radius, height - 1);
            int yAdd = qBound(0, y + radius + 1, height - 1);

            QRgb pRemove = reinterpret_cast<const QRgb*>(horiz.constScanLine(yRemove))[x];
            QRgb pAdd = reinterpret_cast<const QRgb*>(horiz.constScanLine(yAdd))[x];

            sumR += qRed(pAdd) - qRed(pRemove);
            sumG += qGreen(pAdd) - qGreen(pRemove);
            sumB += qBlue(pAdd) - qBlue(pRemove);
            sumA += qAlpha(pAdd) - qAlpha(pRemove);
        }
    }

    return result;
}

QImage ImageProcessor::applyVignette(const QImage &src, int intensity, int radius)
{
    if (src.isNull() || intensity == 0) return src;

    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage result = (src.format() == fmt) ? src.copy() : src.convertToFormat(fmt);
    const int width = result.width();
    const int height = result.height();

    const double centerX = width / 2.0;
    const double centerY = height / 2.0;
    const double maxDist = std::sqrt(centerX * centerX + centerY * centerY);
    const double innerDist = maxDist * (qBound(10, radius, 100) / 100.0);
    const double featherDist = maxDist - innerDist;
    if (featherDist <= 0.0) return src;

    const double innerDistSq = innerDist * innerDist;
    const double strength = intensity / 100.0;

    for (int y = 0; y < height; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        double dy = y - centerY;
        double dySq = dy * dy;

        for (int x = 0; x < width; ++x) {
            double dx = x - centerX;
            double distSq = dx * dx + dySq;

            // Fast bypass: pixels inside the inner radius circle remain unaffected
            if (distSq > innerDistSq) {
                double dist = std::sqrt(distSq);
                double factor = (dist - innerDist) / featherDist;
                factor = std::min(1.0, factor);
                // Smooth cosine curve
                double smoothFactor = 0.5 * (1.0 - std::cos(factor * M_PI));
                double darkFactor = 1.0 - (strength * smoothFactor);
                darkFactor = std::max(0.0, darkFactor);

                QRgb p = line[x];
                int r = static_cast<int>(qRed(p) * darkFactor);
                int g = static_cast<int>(qGreen(p) * darkFactor);
                int b = static_cast<int>(qBlue(p) * darkFactor);

                line[x] = qRgba(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), qAlpha(p));
            }
        }
    }

    return result;
}

QImage ImageProcessor::applyInvert(const QImage &src)
{
    if (src.isNull()) return src;
    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage result = (src.format() == fmt) ? src.copy() : src.convertToFormat(fmt);
    const int width = result.width();
    const int height = result.height();

    for (int y = 0; y < height; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb p = line[x];
            line[x] = qRgba(255 - qRed(p), 255 - qGreen(p), 255 - qBlue(p), qAlpha(p));
        }
    }

    return result;
}

QImage ImageProcessor::autoEnhance(const QImage &src)
{
    return autoEnhance(src, AutoEnhanceOptions());
}

QImage ImageProcessor::autoEnhance(const QImage &src, const AutoEnhanceOptions &options)
{
    if (src.isNull()) return QImage();
    if (options.intensity <= 0) return src;

    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage result = src.convertToFormat(fmt);
    const int width = result.width();
    const int height = result.height();
    const qint64 totalPixels = static_cast<qint64>(width) * height;
    if (totalPixels == 0) return src;

    // Region of interest bounding
    QRect roi = options.roiRect.isValid() ? options.roiRect.intersected(result.rect()) : result.rect();
    if (roi.isEmpty()) roi = result.rect();

    const double centerX = roi.center().x();
    const double centerY = roi.center().y();
    const double centerRadius = 0.45 * std::min(roi.width(), roi.height());
    const double centerRadiusSq = centerRadius * centerRadius;

    QVector<double> histLuma(256, 0.0);
    double totalWeight = 0.0;

    double sumR = 0.0, sumG = 0.0, sumB = 0.0;
    double wbWeightSum = 0.0;

    double wpSumR = 0.0, wpSumG = 0.0, wpSumB = 0.0;
    double wpWeightSum = 0.0;

    for (int y = roi.top(); y <= roi.bottom(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(result.constScanLine(y));
        double dy = y - centerY;
        double dySq = dy * dy;

        for (int x = roi.left(); x <= roi.right(); ++x) {
            QRgb p = line[x];
            int r = qRed(p);
            int g = qGreen(p);
            int b = qBlue(p);
            int luma = (306 * r + 601 * g + 117 * b) >> 10;

            double weight = 1.0;
            if (options.centerWeighted && centerRadiusSq > 0.0) {
                double dx = x - centerX;
                double distSq = dx * dx + dySq;
                // Photographic center-weighted priority: ~6.0x at optical center, decaying to ~0.05x at periphery
                weight = 0.05 + 5.95 * std::exp(-2.5 * (distSq / centerRadiusSq));
            }

            histLuma[luma] += weight;
            totalWeight += weight;

            // Highlight Exclusion for White Balance:
            // Exclude pixels near pure black (< 15) and blown-out highlights (> 228)
            // so lamps, light fixtures, or sun reflections do NOT skew the scene color!
            if (options.protectHighlights) {
                if (luma >= 15 && luma <= 228) {
                    sumR += r * weight;
                    sumG += g * weight;
                    sumB += b * weight;
                    wbWeightSum += weight;
                }
            } else {
                if (luma >= 5 && luma <= 250) {
                    sumR += r * weight;
                    sumG += g * weight;
                    sumB += b * weight;
                    wbWeightSum += weight;
                }
            }

            // White Patch (Retinex illuminant candidate): bright near-neutral highlights
            if (luma >= 170 && luma <= 235) {
                int maxC = std::max({r, g, b});
                int minC = std::min({r, g, b});
                if ((maxC - minC) < 110) {
                    wpSumR += r * weight;
                    wpSumG += g * weight;
                    wpSumB += b * weight;
                    wpWeightSum += weight;
                }
            }
        }
    }

    if (totalWeight <= 0.0) totalWeight = 1.0;

    // 2. Color Balance Gains
    double gainR = 1.0, gainG = 1.0, gainB = 1.0;
    double sampleRefR = -1.0, sampleRefG = -1.0, sampleRefB = -1.0;

    if (options.neutralSamplePoint.x() >= 0 && options.neutralSamplePoint.y() >= 0 &&
        result.rect().contains(options.neutralSamplePoint)) {
        // Manual Eyedropper Calibration: sample 3x3 neighborhood around sample point
        int sx = options.neutralSamplePoint.x();
        int sy = options.neutralSamplePoint.y();
        int sampleCount = 0;
        double sr = 0, sg = 0, sb = 0;

        for (int ny = std::max(0, sy - 1); ny <= std::min(height - 1, sy + 1); ++ny) {
            const QRgb *nLine = reinterpret_cast<const QRgb*>(result.constScanLine(ny));
            for (int nx = std::max(0, sx - 1); nx <= std::min(width - 1, sx + 1); ++nx) {
                QRgb np = nLine[nx];
                sr += qRed(np);
                sg += qGreen(np);
                sb += qBlue(np);
                ++sampleCount;
            }
        }
        if (sampleCount > 0 && sr > 0 && sg > 0 && sb > 0) {
            sr /= sampleCount; sg /= sampleCount; sb /= sampleCount;
            sampleRefR = sr; sampleRefG = sg; sampleRefB = sb;
            double grayTarget = (sr + sg + sb) / 3.0;
            gainR = qBound(0.40, grayTarget / sr, 2.50);
            gainG = qBound(0.50, grayTarget / sg, 2.00);
            gainB = qBound(0.40, grayTarget / sb, 2.80);
        }
    } else if (options.correctWhiteBalance && wbWeightSum > 0.0) {
        // Hybrid Grayworld + White Patch (Retinex) Automatic White Balance
        double avgR = sumR / wbWeightSum;
        double avgG = sumG / wbWeightSum;
        double avgB = sumB / wbWeightSum;
        double grayTarget = (avgR + avgG + avgB) / 3.0;

        double gwR = grayTarget / std::max(1.0, avgR);
        double gwG = grayTarget / std::max(1.0, avgG);
        double gwB = grayTarget / std::max(1.0, avgB);

        if (wpWeightSum > 100.0) {
            double wpAvgR = wpSumR / wpWeightSum;
            double wpAvgG = wpSumG / wpWeightSum;
            double wpAvgB = wpSumB / wpWeightSum;
            double wpTarget = (wpAvgR + wpAvgG + wpAvgB) / 3.0;
            double wpR = wpTarget / std::max(1.0, wpAvgR);
            double wpG = wpTarget / std::max(1.0, wpAvgG);
            double wpB = wpTarget / std::max(1.0, wpAvgB);

            // 50% Grayworld + 50% White Patch
            gainR = 0.50 * gwR + 0.50 * wpR;
            gainG = 0.50 * gwG + 0.50 * wpG;
            gainB = 0.50 * gwB + 0.50 * wpB;
        } else {
            gainR = gwR;
            gainG = gwG;
            gainB = gwB;
        }

        // Expanded bounds for tungsten/indoor casts (indoor tungsten requires blue gain up to 2.2x)
        gainR = qBound(0.55, gainR, 1.45);
        gainG = qBound(0.65, gainG, 1.35);
        gainB = qBound(0.55, gainB, 2.40);
    }

    // 3. Percentile-based Dynamic Range Analysis
    double cumWeight = 0.0;
    int pLow = 0;
    int pMid = 128;
    int pHigh = 255;

    const double targetLow = totalWeight * 0.008;  // 0.8% percentile
    const double targetMid = totalWeight * 0.500;  // 50% median
    const double targetHigh = totalWeight * 0.992; // 99.2% percentile

    bool foundLow = false;
    bool foundMid = false;

    for (int i = 0; i < 256; ++i) {
        cumWeight += histLuma[i];
        if (!foundLow && cumWeight >= targetLow) {
            pLow = i;
            foundLow = true;
        }
        if (!foundMid && cumWeight >= targetMid) {
            pMid = i;
            foundMid = true;
        }
        if (cumWeight >= targetHigh) {
            pHigh = i;
            break;
        }
    }

    pLow = qBound(0, pLow, 40);
    pHigh = qBound(210, pHigh, 255);
    if (pHigh <= pLow + 10) {
        pHigh = 255;
        pLow = 0;
    }

    // 4. Adaptive Gamma for Shadow Recovery and Subject Key
    double gamma = 1.0;
    if (options.recoverShadows) {
        if (options.profile == AutoEnhanceOptions::Profile::BacklightShadows) {
            gamma = 0.70; // Strong shadow recovery
        } else if (pMid < 125) {
            // Adaptive gamma: darker scenes and backlit central subjects get stronger midtone lift
            gamma = 0.72 + (static_cast<double>(pMid) / 125.0) * 0.28;
        } else if (options.centerWeighted && pMid > 135) {
            // Central subject is brightly illuminated; protect from overexposure wash
            gamma = 1.0 + std::min(0.15, ((pMid - 135.0) / 120.0) * 0.15);
        }
    }

    if (options.profile == AutoEnhanceOptions::Profile::DehazeClear) {
        gamma = std::min(gamma, 0.92);
        pLow = std::max(pLow, 6);
    }

    // 5. Build 256-entry Look-Up Tables with Highlight Roll-off
    unsigned char lutR[256];
    unsigned char lutG[256];
    unsigned char lutB[256];

    const double invRange = 255.0 / (pHigh - pLow);

    for (int i = 0; i < 256; ++i) {
        // Contrast stretch
        double v = (i - pLow) * invRange;
        v = qBound(0.0, v, 255.0);

        // Adaptive gamma
        if (gamma < 0.999 && v > 0.0) {
            v = 255.0 * std::pow(v / 255.0, gamma);
        }

        // Highlight Soft Shoulder (Roll-off):
        // If protectHighlights is active, compress values above 200 smoothly
        // so blown lights never clamp brutally or produce harsh digital halos!
        if (options.protectHighlights && v > 200.0) {
            double excess = v - 200.0;
            // Smooth asymptotic compression towards 255
            v = 200.0 + 55.0 * (excess / (excess + 25.0));
        }

        // Apply channel gains with highlight roll-off (at v=255 gains blend to 1.0)
        double hlRoll = std::max(0.0, (v - 210.0) / 45.0);
        double effGainR = gainR * (1.0 - hlRoll) + 1.0 * hlRoll;
        double effGainG = gainG * (1.0 - hlRoll) + 1.0 * hlRoll;
        double effGainB = gainB * (1.0 - hlRoll) + 1.0 * hlRoll;

        lutR[i] = static_cast<unsigned char>(qBound(0, static_cast<int>(std::round(v * effGainR)), 255));
        lutG[i] = static_cast<unsigned char>(qBound(0, static_cast<int>(std::round(v * effGainG)), 255));
        lutB[i] = static_cast<unsigned char>(qBound(0, static_cast<int>(std::round(v * effGainB)), 255));
    }

    const bool boostVivid = (options.profile == AutoEnhanceOptions::Profile::Vivid);
    const int vividSatFP = boostVivid ? ((100 + 16) * 1024) / 100 : 1024;

    // 6. Fast scanline rendering with skin tone protection
    for (int y = 0; y < height; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb p = line[x];
            int r = qRed(p);
            int g = qGreen(p);
            int b = qBlue(p);

            int outR = lutR[r];
            int outG = lutG[g];
            int outB = lutB[b];

            if (boostVivid) {
                int gray = (306 * outR + 601 * outG + 117 * outB) >> 10;
                outR = qBound(0, gray + (((outR - gray) * vividSatFP + 512) >> 10), 255);
                outG = qBound(0, gray + (((outG - gray) * vividSatFP + 512) >> 10), 255);
                outB = qBound(0, gray + (((outB - gray) * vividSatFP + 512) >> 10), 255);
            }

            bool isNeutralSample = (sampleRefR >= 0.0 &&
                                    std::abs(r - sampleRefR) <= 12 &&
                                    std::abs(g - sampleRefG) <= 12 &&
                                    std::abs(b - sampleRefB) <= 12);

            // Skin Tone Protection:
            // Detect real human skin melanin chromaticity (excludes neutral sample points, yellow walls, and yellow lights)
            if (options.protectSkinTones && !isNeutralSample && (r > g + 8) && (g > b + 4) && ((r - b) < 2.8 * (r - g))) {
                int hVal, sVal, vVal;
                rgbToHsv(r, g, b, hVal, sVal, vVal);
                // Human skin tone hue range is tightly bounded: 8° - 38° or 350° - 360°
                if ((hVal <= 38 || hVal >= 350) && sVal >= 25 && sVal <= 180 && vVal >= 55 && vVal <= 245) {
                    // Calculate skin confidence factor (peaked at classic skin hue ~22°)
                    int hueDiff = (hVal <= 38) ? std::abs(hVal - 22) : std::abs(hVal - 360 - 22);
                    double skinConfidence = qBound(0.0, 1.0 - (hueDiff / 20.0), 1.0);

                    // Compute healthy warm melanin skin target:
                    // Preserve natural peach/golden warmth: R has healthy vitality, B is controlled relative to G
                    int targetR = qBound(0, static_cast<int>(outR * 1.08 + 8), 255);
                    int targetG = outG;
                    int targetB = qBound(0, std::min(outB, static_cast<int>(outG * 0.82)), 255);

                    // If auto white balance stripped too much color difference (outR - outB), restore vitality
                    if (outR - outB < 35) {
                        targetR = std::min(255, targetR + 6);
                        targetB = std::max(0, targetB - 8);
                    }

                    double blendWeight = 0.70 * skinConfidence;
                    outR = static_cast<int>(outR * (1.0 - blendWeight) + targetR * blendWeight);
                    outG = static_cast<int>(outG * (1.0 - blendWeight) + targetG * blendWeight);
                    outB = static_cast<int>(outB * (1.0 - blendWeight) + targetB * blendWeight);
                }
            }

            line[x] = qRgba(outR, outG, outB, qAlpha(p));
        }
    }

    // 7. Edge-Preserving Bilateral Shadow Denoise (cleans sensor grain & chroma noise in lifted darks)
    if (options.autoDenoiseShadows) {
        // Targets shadow regions (luma < 90) where digital sensor grain & color speckles appear
        QImage denoised = result;
        for (int y = 1; y < height - 1; ++y) {
            const QRgb *prev = reinterpret_cast<const QRgb*>(result.constScanLine(y - 1));
            const QRgb *curr = reinterpret_cast<const QRgb*>(result.constScanLine(y));
            const QRgb *next = reinterpret_cast<const QRgb*>(result.constScanLine(y + 1));
            QRgb *outLine = reinterpret_cast<QRgb*>(denoised.scanLine(y));

            for (int x = 1; x < width - 1; ++x) {
                QRgb p = curr[x];
                int r = qRed(p);
                int g = qGreen(p);
                int b = qBlue(p);
                int luma = (306 * r + 601 * g + 117 * b) >> 10;

                if (luma < 90) {
                    // Shadow strength factor: 1.0 at deep blacks down to 0.0 at luma 90
                    double shadowStrength = (90.0 - luma) / 90.0;

                    // Bilateral filter: samples 3x3 neighborhood, weighting neighbors by luminance similarity
                    // Smooths sensor grain and chroma speckles while keeping sharp edges crisp
                    int sumR = r * 3;
                    int sumG = g * 3;
                    int sumB = b * 3;
                    int totalW = 3;

                    const QRgb neighbors[8] = {
                        prev[x - 1], prev[x], prev[x + 1],
                        curr[x - 1],          curr[x + 1],
                        next[x - 1], next[x], next[x + 1]
                    };

                    for (int n = 0; n < 8; ++n) {
                        QRgb np = neighbors[n];
                        int nr = qRed(np);
                        int ng = qGreen(np);
                        int nb = qBlue(np);
                        int nLuma = (306 * nr + 601 * ng + 117 * nb) >> 10;

                        int diff = std::abs(luma - nLuma);
                        if (diff < 36) {
                            int w = 36 - diff;
                            sumR += nr * w;
                            sumG += ng * w;
                            sumB += nb * w;
                            totalW += w;
                        }
                    }

                    int cleanR = sumR / totalW;
                    int cleanG = sumG / totalW;
                    int cleanB = sumB / totalW;

                    double blendWeight = shadowStrength * 0.90;
                    int finalR = static_cast<int>(r * (1.0 - blendWeight) + cleanR * blendWeight);
                    int finalG = static_cast<int>(g * (1.0 - blendWeight) + cleanG * blendWeight);
                    int finalB = static_cast<int>(b * (1.0 - blendWeight) + cleanB * blendWeight);

                    outLine[x] = qRgba(finalR, finalG, finalB, qAlpha(p));
                }
            }
        }
        result = denoised;
    }

    // 8. Intensity Blending (0% = untouched, 100% = full, up to 150%)
    if (options.intensity != 100) {
        double factor = options.intensity / 100.0;
        result = blend(src, result, factor);
    }

    return result;
}

QImage ImageProcessor::pixelateRegion(const QImage &src, const QRect &region, int blockSize)
{
    if (src.isNull() || region.isEmpty()) return src;
    blockSize = qBound(4, blockSize, 64);

    const QImage::Format fmt = src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage result = src.convertToFormat(fmt);

    QRect bounds = region.intersected(result.rect());
    if (bounds.isEmpty()) return result;

    for (int y = bounds.top(); y <= bounds.bottom(); y += blockSize) {
        for (int x = bounds.left(); x <= bounds.right(); x += blockSize) {
            QRect block(x, y, blockSize, blockSize);
            block = block.intersected(bounds);

            // Calculate average color inside the block
            qint64 sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            int pixelCount = 0;

            for (int by = block.top(); by <= block.bottom(); ++by) {
                const QRgb *line = reinterpret_cast<const QRgb*>(result.constScanLine(by));
                for (int bx = block.left(); bx <= block.right(); ++bx) {
                    QRgb p = line[bx];
                    sumR += qRed(p);
                    sumG += qGreen(p);
                    sumB += qBlue(p);
                    sumA += qAlpha(p);
                    ++pixelCount;
                }
            }

            if (pixelCount > 0) {
                QRgb avgColor = qRgba(sumR / pixelCount, sumG / pixelCount, sumB / pixelCount, sumA / pixelCount);
                for (int by = block.top(); by <= block.bottom(); ++by) {
                    QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(by));
                    for (int bx = block.left(); bx <= block.right(); ++bx) {
                        line[bx] = avgColor;
                    }
                }
            }
        }
    }

    return result;
}

QImage ImageProcessor::blurRegion(const QImage &src, const QRect &region, int radius)
{
    if (src.isNull() || region.isEmpty()) return src;
    QRect bounds = region.intersected(src.rect());
    if (bounds.isEmpty()) return src;

    QImage sub = src.copy(bounds);
    QImage blurredSub = applyBlur(sub, radius);

    QImage result = src;
    QPainter p(&result);
    p.drawImage(bounds.topLeft(), blurredSub);
    p.end();

    return result;
}

void ImageProcessor::computeHistogram(const QImage &src,
                                     QVector<int> &outRed,
                                     QVector<int> &outGreen,
                                     QVector<int> &outBlue,
                                     QVector<int> &outLuma,
                                     bool &outClippedShadows,
                                     bool &outClippedHighlights)
{
    outRed.fill(0, 256);
    outGreen.fill(0, 256);
    outBlue.fill(0, 256);
    outLuma.fill(0, 256);
    outClippedShadows = false;
    outClippedHighlights = false;

    if (src.isNull()) return;

    const QImage img = (src.format() == QImage::Format_RGB32 ||
                        src.format() == QImage::Format_ARGB32 ||
                        src.format() == QImage::Format_ARGB32_Premultiplied)
                       ? src : src.convertToFormat(QImage::Format_RGB32);

    const int width = img.width();
    const int height = img.height();
    const qint64 totalPixels = static_cast<qint64>(width) * height;
    if (totalPixels == 0) return;

    int shadowCount = 0;
    int highlightCount = 0;

    for (int y = 0; y < height; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb p = line[x];
            int r = qRed(p);
            int g = qGreen(p);
            int b = qBlue(p);
            int luma = (306 * r + 601 * g + 117 * b) >> 10;

            outRed[r]++;
            outGreen[g]++;
            outBlue[b]++;
            outLuma[luma]++;

            if (luma <= 3) ++shadowCount;
            if (luma >= 252) ++highlightCount;
        }
    }

    // Flag clipping if more than 0.8% of pixels are pinned at extremes
    const int clipThreshold = static_cast<int>(totalPixels * 0.008);
    outClippedShadows = (shadowCount > clipThreshold);
    outClippedHighlights = (highlightCount > clipThreshold);
}

QString ImageProcessor::presetDisplayName(FilterPreset preset)
{
    switch (preset) {
        case FilterPreset::Original:        return QCoreApplication::translate("ImageProcessor", "Original");
        case FilterPreset::TealAndOrange:   return QCoreApplication::translate("ImageProcessor", "Teal & Orange");
        case FilterPreset::VintageSepia:    return QCoreApplication::translate("ImageProcessor", "Vintage Sepia");
        case FilterPreset::RetroFilm:       return QCoreApplication::translate("ImageProcessor", "Retro Film");
        case FilterPreset::FilmNoir:        return QCoreApplication::translate("ImageProcessor", "Film Noir (B&W)");
        case FilterPreset::RedFilterBW:     return QCoreApplication::translate("ImageProcessor", "Red Filter (Sky B&W)");
        case FilterPreset::OrangeFilterBW:  return QCoreApplication::translate("ImageProcessor", "Orange Filter (Skin B&W)");
        case FilterPreset::GoldenHour:      return QCoreApplication::translate("ImageProcessor", "Golden Hour");
        case FilterPreset::NordicCool:      return QCoreApplication::translate("ImageProcessor", "Nordic Cool");
        case FilterPreset::CrossProcess:    return QCoreApplication::translate("ImageProcessor", "Cross Process");
        case FilterPreset::Cyberpunk:       return QCoreApplication::translate("ImageProcessor", "Cyberpunk Shift");
        case FilterPreset::DocumentEnhance: return QCoreApplication::translate("ImageProcessor", "Document & Text");
    }
    return QString();
}

QString ImageProcessor::presetDescription(FilterPreset preset)
{
    switch (preset) {
        case FilterPreset::Original:        return QCoreApplication::translate("ImageProcessor", "No filter applied");
        case FilterPreset::TealAndOrange:   return QCoreApplication::translate("ImageProcessor", "Hollywood cinematic color grading");
        case FilterPreset::VintageSepia:    return QCoreApplication::translate("ImageProcessor", "Classic antique sepia toning");
        case FilterPreset::RetroFilm:       return QCoreApplication::translate("ImageProcessor", "Analog matte shadows and soft warmth");
        case FilterPreset::FilmNoir:        return QCoreApplication::translate("ImageProcessor", "High contrast dramatic monochrome");
        case FilterPreset::RedFilterBW:     return QCoreApplication::translate("ImageProcessor", "Deep sky contrast and bright foliage");
        case FilterPreset::OrangeFilterBW:  return QCoreApplication::translate("ImageProcessor", "Smooth flattering portrait skin tones");
        case FilterPreset::GoldenHour:      return QCoreApplication::translate("ImageProcessor", "Warm amber sunset glow");
        case FilterPreset::NordicCool:      return QCoreApplication::translate("ImageProcessor", "Glacial atmosphere with cyan undertones");
        case FilterPreset::CrossProcess:    return QCoreApplication::translate("ImageProcessor", "High saturation punchy lomo look");
        case FilterPreset::Cyberpunk:       return QCoreApplication::translate("ImageProcessor", "Vibrant neon magenta and blue night shift");
        case FilterPreset::DocumentEnhance: return QCoreApplication::translate("ImageProcessor", "Sharp black text on clean white paper");
    }
    return QString();
}
