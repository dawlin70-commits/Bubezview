#include "theme_manager.h"
#include <QStyleFactory>
#include <QStyle>
#include <QPalette>
#include <QColor>

ThemePreset ThemeManager::currentPreset()
{
    QSettings settings("Bubez", "BubezView");
    QString val = settings.value("theme_preset", "native").toString();
    return stringToPreset(val);
}

void ThemeManager::setPreset(ThemePreset preset)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("theme_preset", presetToString(preset));
}

QString ThemeManager::presetToString(ThemePreset preset)
{
    switch (preset) {
    case ThemePreset::SystemNative: return "native";
    case ThemePreset::DarkSlate:    return "dark_slate";
    case ThemePreset::BreezeDark:   return "breeze_dark";
    }
    return "native";
}

ThemePreset ThemeManager::stringToPreset(const QString &str)
{
    if (str == "dark_slate") return ThemePreset::DarkSlate;
    if (str == "breeze_dark") return ThemePreset::BreezeDark;
    return ThemePreset::SystemNative;
}

TransitionEffect ThemeManager::currentTransition()
{
    QSettings settings("Bubez", "BubezView");
    QString val = settings.value("transition_effect", "zoom_fade").toString();
    return stringToTransition(val);
}

void ThemeManager::setTransition(TransitionEffect effect)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("transition_effect", transitionToString(effect));
}

QString ThemeManager::transitionToString(TransitionEffect effect)
{
    switch (effect) {
    case TransitionEffect::ZoomFade:      return "zoom_fade";
    case TransitionEffect::ParallaxSlide: return "parallax_slide";
    case TransitionEffect::CrossFade:     return "cross_fade";
    case TransitionEffect::None:          return "none";
    }
    return "zoom_fade";
}

TransitionEffect ThemeManager::stringToTransition(const QString &str)
{
    if (str == "parallax_slide") return TransitionEffect::ParallaxSlide;
    if (str == "cross_fade")     return TransitionEffect::CrossFade;
    if (str == "none")           return TransitionEffect::None;
    return TransitionEffect::ZoomFade;
}

bool ThemeManager::useLibreOffice()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("use_libreoffice", false).toBool();
}

void ThemeManager::setUseLibreOffice(bool enable)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("use_libreoffice", enable);
}


void ThemeManager::applyTheme(QApplication *app, QMainWindow *window)
{
    ThemePreset preset = currentPreset();

    if (preset == ThemePreset::SystemNative) {
        // 100% Native Desktop Style
        if (QStyleFactory::keys().contains("Breeze")) {
            app->setStyle(QStyleFactory::create("Breeze"));
        } else if (QStyleFactory::keys().contains("Fusion")) {
            app->setStyle(QStyleFactory::create("Fusion"));
        }
        app->setPalette(app->style()->standardPalette());

        if (window) {
            // Respect native desktop decorations while ensuring visible high-contrast scrollbars
            window->setStyleSheet(
                "QStatusBar {"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    } else if (preset == ThemePreset::DarkSlate) {
        // Custom Modern Slate (#22262b)
        app->setStyle(QStyleFactory::create("Fusion"));

        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor("#22262b"));
        darkPalette.setColor(QPalette::WindowText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Base, QColor("#22262b"));
        darkPalette.setColor(QPalette::AlternateBase, QColor("#282c32"));
        darkPalette.setColor(QPalette::ToolTipBase, QColor("#2a2e34"));
        darkPalette.setColor(QPalette::ToolTipText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Text, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Button, QColor("#282c32"));
        darkPalette.setColor(QPalette::ButtonText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor("#61afef"));
        darkPalette.setColor(QPalette::Highlight, QColor("#383c42"));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        app->setPalette(darkPalette);

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background-color: #22262b;"
                "}"
                "QMenuBar {"
                "   background-color: #22262b;"
                "   color: #c5cbd3;"
                "   border-bottom: 1px solid #2a2f35;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: #2d333b;"
                "   color: #ffffff;"
                "   border-radius: 4px;"
                "}"
                "QMenu {"
                "   background-color: #1e2226;"
                "   color: #eff0f1;"
                "   border: 1px solid #2d333b;"
                "   padding: 4px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px;"
                "   border-radius: 4px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #383c42;"
                "}"
                "QToolBar {"
                "   background-color: #22262b;"
                "   border-bottom: 1px solid #2a2f35;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #c5cbd3;"
                "   border-radius: 4px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: #2d333b;"
                "   color: #ffffff;"
                "}"
                "QToolButton:pressed {"
                "   background-color: #383e46;"
                "}"
                "QStatusBar {"
                "   background-color: #1a1d21;"
                "   color: #959ca7;"
                "   border-top: 1px solid #2a2f35;"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    } else if (preset == ThemePreset::BreezeDark) {
        // Deep Charcoal / Breeze Dark (#181b1d)
        app->setStyle(QStyleFactory::create("Fusion"));

        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor("#181b1d"));
        darkPalette.setColor(QPalette::WindowText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Base, QColor("#181b1d"));
        darkPalette.setColor(QPalette::AlternateBase, QColor("#22262a"));
        darkPalette.setColor(QPalette::ToolTipBase, QColor("#1e2226"));
        darkPalette.setColor(QPalette::ToolTipText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Text, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::Button, QColor("#22262a"));
        darkPalette.setColor(QPalette::ButtonText, QColor("#eff0f1"));
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor("#3daee9"));
        darkPalette.setColor(QPalette::Highlight, QColor("#2a3037"));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        app->setPalette(darkPalette);

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background-color: #181b1d;"
                "}"
                "QMenuBar {"
                "   background-color: #181b1d;"
                "   color: #c5cbd3;"
                "   border-bottom: 1px solid #23272b;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: #24292f;"
                "   color: #ffffff;"
                "   border-radius: 4px;"
                "}"
                "QMenu {"
                "   background-color: #1e2226;"
                "   color: #eff0f1;"
                "   border: 1px solid #2d333b;"
                "   padding: 4px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px;"
                "   border-radius: 4px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #2a3037;"
                "}"
                "QToolBar {"
                "   background-color: #181b1d;"
                "   border-bottom: 1px solid #23272b;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #c5cbd3;"
                "   border-radius: 4px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: #24292f;"
                "   color: #ffffff;"
                "}"
                "QToolButton:pressed {"
                "   background-color: #2d333b;"
                "}"
                "QStatusBar {"
                "   background-color: #141618;"
                "   color: #8a9098;"
                "   border-top: 1px solid #23272b;"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    }
}

QString ThemeManager::scrollBarStyleSheet(ThemePreset preset)
{
    if (preset == ThemePreset::SystemNative) {
        // High-contrast light scrollbars: track is soft light, handle is distinctively darker
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #e6e9ed;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #7b8694;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #5d6774;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #444c56;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #e6e9ed;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #7b8694;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #5d6774;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #444c56;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "   background: none;"
            "}"
        );
    } else if (preset == ThemePreset::DarkSlate) {
        // High-contrast DarkSlate scrollbars: track is dark slate, handle is distinctively lighter
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #181b1f;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #586474;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #728092;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #8d9cb0;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #181b1f;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #586474;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #728092;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #8d9cb0;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "   background: none;"
            "}"
        );
    } else { // BreezeDark
        // High-contrast BreezeDark scrollbars: track is deep charcoal, handle is distinctively lighter
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #121416;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #4e5866;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #677484;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #8392a4;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #121416;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #4e5866;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #677484;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #8392a4;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "   background: none;"
            "}"
        );
    }
}

