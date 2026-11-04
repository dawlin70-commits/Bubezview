#pragma once

#include <QString>
#include <QApplication>
#include <QMainWindow>
#include <QSettings>

class QFileDialog;

/**
 * @brief Available visual theme presets for BubezView.
 */
enum class ThemePreset {
    DarkSlate,   ///< Elegant custom dark slate theme with high contrast borders (#22262b).
    BreezeDark,  ///< KDE Breeze-inspired dark styling (#181b1d).
    ModernLight, ///< macOS/Cupertino-inspired frosted silver light styling (#f5f5f7 / #ffffff).
    BreezeLight  ///< KDE Breeze-inspired clean light styling (#eff0f1 / #fcfcfc).
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
     * @brief Checks whether the given theme preset has a dark visual appearance.
     * @param preset The target ThemePreset.
     * @return True if preset is DarkSlate or BreezeDark.
     */
    static inline bool isDark(ThemePreset preset) {
        return preset == ThemePreset::DarkSlate || preset == ThemePreset::BreezeDark;
    }

    /**
     * @brief Checks whether the currently active theme preset is dark.
     * @return True if active preset is dark.
     */
    static inline bool isDarkTheme() {
        return isDark(currentPreset());
    }

    /**
     * @brief Returns the appropriate contrast icon stroke color for the given preset.
     * @param preset The target ThemePreset.
     * @return Light gray for dark themes, dark charcoal for light themes.
     */
    static inline QColor iconColor(ThemePreset preset) {
        return isDark(preset) ? QColor("#e2e8f0") : QColor("#232629");
    }

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

    /**
     * @brief Generates cohesive, modern stylesheet rules for QFileDialog and file browser views.
     *
     * Provides elegant dark/light theme styling for the file picker tree, places sidebar,
     * headers, path bars, and action buttons.
     *
     * @param preset The target ThemePreset.
     * @return Formatted Qt stylesheet string for QFileDialog and its sub-widgets.
     */
    static QString fileDialogStyleSheet(ThemePreset preset);

    /**
     * @brief Configures any QFileDialog instance with BubezView places (FolderViewer bookmarks),
     * crisp vector navigation icons, and non-truncated sidebar geometry.
     *
     * @param dialog Pointer to the target QFileDialog.
     */
    static void configureFileDialog(QFileDialog *dialog);
};
