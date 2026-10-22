#pragma once

#include <QImage>
#include <QRect>
#include <QPoint>
#include <QVector>
#include <QString>

/**
 * @brief High-performance image processing engine for BubezView.
 *
 * Provides fast integer and Look-Up Table (LUT) based photo adjustments,
 * artistic and film filter presets, convolution filters (sharpen, blur, vignette),
 * histogram computation, auto-enhancement, and privacy tools (pixelate/blur).
 *
 * All operations are deterministic, multi-threaded where appropriate, and 100% free
 * of heavy AI/neural network runtimes.
 */
class ImageProcessor {
public:
    /**
     * @brief Predefined artistic, film, and creative filter presets.
     */
    enum class FilterPreset {
        Original,        ///< Original untouched image
        TealAndOrange,   ///< Cinematic teal shadows & warm orange highlights
        VintageSepia,    ///< Classic aged sepia tone with warm darks
        RetroFilm,       ///< Faded matte shadows, subtle grain, and analog warmth
        FilmNoir,        ///< Dramatic high-contrast black & white
        RedFilterBW,     ///< Optical red filter simulation (dark dramatic skies)
        OrangeFilterBW,  ///< Optical orange filter simulation (smooth skin tones)
        GoldenHour,      ///< Warm amber sunset glow
        NordicCool,      ///< Cold glacial atmosphere with cyan/blue tint
        CrossProcess,    ///< High-saturation, punchy retro lomo style
        Cyberpunk,       ///< Neon purple, magenta, and cyan night shift
        DocumentEnhance  ///< High-contrast binarized scan enhancer for text/paper
    };

    /**
     * @brief Adjusts light, tone, color temperature, and RGB channel balances.
     * @param src Source image.
     * @param brightness Level in range [-100, 100].
     * @param contrast Level in range [-100, 100].
     * @param saturation Level in range [-100, 100].
     * @param hue Hue shift in degrees [-180, 180].
     * @param temperature Warm/cool temperature shift [-100, 100].
     * @param red Red channel gain [-100, 100].
     * @param green Green channel gain [-100, 100].
     * @param blue Blue channel gain [-100, 100].
     * @param highlights High-key highlight recovery [-100, 100].
     * @param shadows Low-key shadow lift [-100, 100].
     * @return Processed image.
     */
    static QImage adjust(const QImage &src,
                         int brightness, int contrast, int saturation,
                         int hue = 0, int temperature = 0,
                         int red = 0, int green = 0, int blue = 0,
                         int highlights = 0, int shadows = 0);

    /**
     * @brief Applies an artistic filter preset with full effect strength.
     * @param src Source image.
     * @param preset The filter preset to apply.
     * @return Filtered image.
     */
    static QImage applyPreset(const QImage &src, FilterPreset preset);

    /**
     * @brief Blends two images using fast fixed-point linear interpolation.
     * @param imgA Base image (factor = 0.0).
     * @param imgB Target image (factor = 1.0).
     * @param factor Blend factor in range [0.0, 1.0].
     * @return Blended image.
     */
    static QImage blend(const QImage &imgA, const QImage &imgB, double factor);

    /**
     * @brief Applies Unsharp Masking / sharpening to enhance edge micro-contrast.
     * @param src Source image.
     * @param amount Sharpening intensity in range [0, 100].
     * @return Sharpened image.
     */
    static QImage applySharpen(const QImage &src, int amount = 50);

    /**
     * @brief Applies fast separable Gaussian/box blur.
     * @param src Source image.
     * @param radius Blur radius in pixels [1, 50].
     * @return Blurred image.
     */
    static QImage applyBlur(const QImage &src, int radius = 5);

    /**
     * @brief Applies smooth radial vignetting to darken or lighten image corners.
     * @param src Source image.
     * @param intensity Vignette intensity in range [-100, 100] (negative = white, positive = dark).
     * @param radius Inner radius percentage [20, 100].
     * @return Vignetted image.
     */
    static QImage applyVignette(const QImage &src, int intensity = 50, int radius = 80);

    /**
     * @brief Inverts RGB colors of the image (negative effect).
     * @param src Source image.
     * @return Inverted image.
     */
    static QImage applyInvert(const QImage &src);

    /**
     * @brief Options for advanced intelligent auto-enhancement.
     */
    struct AutoEnhanceOptions {
        int intensity = 100;                 ///< Effect strength in percent [0, 150]
        bool protectHighlights = true;      ///< Compresses highlights to prevent blowing out already bright lights (>230)
        bool recoverShadows = true;          ///< Uses adaptive gamma curve to lift dark areas & backlit subjects
        bool correctWhiteBalance = true;     ///< Color cast neutralization
        bool protectSkinTones = true;        ///< Preserves natural human skin hues from oversaturation
        bool autoDenoiseShadows = true;      ///< Subtle noise reduction on lifted shadow regions
        bool centerWeighted = true;          ///< Weight central region more heavily in scene metering
        QPoint neutralSamplePoint = QPoint(-1, -1); ///< Optional manual neutral eyedropper calibration point
        QRect roiRect = QRect();             ///< Optional region-of-interest for exposure/color metering

        enum class Profile {
            Natural,                         ///< Balanced realistic enhancement (default)
            Vivid,                           ///< Punchier contrast & saturated landscape colors
            BacklightShadows,                ///< Maximum shadow lift for backlit scenes & dark faces
            DehazeClear                      ///< Maximum contrast & atmospheric clarity
        } profile = Profile::Natural;
    };

    /**
     * @brief Advanced automatic white balance and contrast enhancement.
     * Analyzes image statistics, protects blown lights, recovers shadows, and optionally calibrates neutral point.
     * @param src Source image.
     * @param options Tuning parameters and profile.
     * @return Auto-enhanced image.
     */
    static QImage autoEnhance(const QImage &src);
    static QImage autoEnhance(const QImage &src, const AutoEnhanceOptions &options);

    /**
     * @brief Pixelates a rectangular area (mosaic effect) for privacy redaction (e.g. license plates, faces).
     * @param src Source image.
     * @param region Area to pixelate in image coordinates.
     * @param blockSize Block size in pixels (default 16).
     * @return Image with pixelated region.
     */
    static QImage pixelateRegion(const QImage &src, const QRect &region, int blockSize = 16);

    /**
     * @brief Strongly blurs a rectangular area for sensitive privacy redaction.
     * @param src Source image.
     * @param region Area to blur in image coordinates.
     * @param radius Blur radius in pixels.
     * @return Image with blurred region.
     */
    static QImage blurRegion(const QImage &src, const QRect &region, int radius = 12);

    /**
     * @brief Computes 256-bin RGB and Luminance histograms along with clipping diagnostics.
     * @param src Source image.
     * @param outRed 256-element array receiving Red channel histogram counts.
     * @param outGreen 256-element array receiving Green channel histogram counts.
     * @param outBlue 256-element array receiving Blue channel histogram counts.
     * @param outLuma 256-element array receiving Luminance channel histogram counts.
     * @param outClippedShadows True if significant pixel counts exist at pure black (0..3).
     * @param outClippedHighlights True if significant pixel counts exist at pure white (252..255).
     */
    static void computeHistogram(const QImage &src,
                                 QVector<int> &outRed,
                                 QVector<int> &outGreen,
                                 QVector<int> &outBlue,
                                 QVector<int> &outLuma,
                                 bool &outClippedShadows,
                                 bool &outClippedHighlights);

    /**
     * @brief Returns a localized, human-readable display name for a filter preset.
     */
    static QString presetDisplayName(FilterPreset preset);

    /**
     * @brief Returns an icon or emoji hint for the preset.
     */
    static QString presetDescription(FilterPreset preset);
};
