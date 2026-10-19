#pragma once

#include <QString>
#include <QApplication>
#include <QMainWindow>
#include <QSettings>

/**
 * @brief Available visual theme presets for BubezView.
 */
enum class ThemePreset {
    SystemNative, ///< Follow the desktop environment's native Qt palette and widget styling.
    DarkSlate,    ///< Elegant custom dark slate theme with high contrast borders.
    BreezeDark    ///< KDE Breeze-inspired dark styling.
};

/**
 * @brief Visual transition animation effects when switching between sibling files.
 */
enum class TransitionEffect {
    ZoomFade,       ///< Smooth Zoom & Fade (Poetic, emergent transition).
    ParallaxSlide,  ///< Parallax Slide & Fade (Modern QuickLook style).
    CrossFade,      ///< Classic Soft Cross-Fade.
    None            ///< Pure Instant Jump with 0ms delay.
};

/**
 * @brief Manages application-wide theme presets, transition styles, and user preferences.
 * Persists configuration using QSettings under standard system config locations.
 */
class ThemeManager {
public:
    /**
     * @brief Reads the currently configured theme preset from settings.
     * @return Active ThemePreset.
     */
    static ThemePreset currentPreset();

    /**
     * @brief Saves the selected theme preset to persistent settings.
     * @param preset The new ThemePreset to activate.
     */
    static void setPreset(ThemePreset preset);

    /**
     * @brief Applies stylesheet and palette corresponding to currentPreset() to the application and main window.
     * @param app Active QApplication instance.
     * @param window Main window instance to receive widget-specific styling.
     */
    static void applyTheme(QApplication *app, QMainWindow *window);

    static QString presetToString(ThemePreset preset);
    static ThemePreset stringToPreset(const QString &str);

    /**
     * @brief Reads the active transition effect for file switching.
     * @return Active TransitionEffect.
     */
    static TransitionEffect currentTransition();

    /**
     * @brief Saves the chosen transition effect.
     * @param effect The transition effect to activate.
     */
    static void setTransition(TransitionEffect effect);

    static QString transitionToString(TransitionEffect effect);
    static TransitionEffect stringToTransition(const QString &str);

    /**
     * @brief Checks if background LibreOffice conversion is enabled for rich office previews.
     * @return True if LibreOffice background rendering is enabled.
     */
    static bool useLibreOffice();

    /**
     * @brief Toggles LibreOffice background rendering preference.
     * @param enable True to enable; false to rely solely on fast internal C++ fallback parsers.
     */
    static void setUseLibreOffice(bool enable);

    /**
     * @brief Generates a high-contrast QScrollBar stylesheet for vertical and horizontal scrollbars.
     *
     * Ensures handles/thumbs are significantly darker in light mode and significantly lighter in dark mode,
     * with comfortable 12px dimensions and rounded pill geometry.
     *
     * @param preset The target ThemePreset.
     * @return Formatted Qt stylesheet string for QScrollBar.
     */
    static QString scrollBarStyleSheet(ThemePreset preset);
};
