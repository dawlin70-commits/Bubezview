#include "theme_manager.h"
#include "ui/vector_icons.h"
#include <QStyleFactory>
#include <QStyle>
#include <QPalette>
#include <QColor>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QToolButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QPushButton>
#include <QHeaderView>
#include <QTreeView>
#include <QListView>

ThemePreset ThemeManager::currentPreset()
{
    QSettings settings("Bubez", "BubezView");
    QString val = settings.value("theme_preset", "dark_slate").toString();
    return stringToPreset(val);
}

void ThemeManager::setPreset(ThemePreset preset)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("theme_preset", presetToString(preset));
    settings.sync();
}

QString ThemeManager::presetToString(ThemePreset preset)
{
    switch (preset) {
    case ThemePreset::DarkSlate:   return "dark_slate";
    case ThemePreset::BreezeDark:  return "breeze_dark";
    case ThemePreset::ModernLight: return "modern_light";
    case ThemePreset::BreezeLight: return "breeze_light";
    }
    return "dark_slate";
}

ThemePreset ThemeManager::stringToPreset(const QString &str)
{
    if (str == "dark_slate")   return ThemePreset::DarkSlate;
    if (str == "breeze_dark")  return ThemePreset::BreezeDark;
    if (str == "modern_light") return ThemePreset::ModernLight;
    if (str == "breeze_light") return ThemePreset::BreezeLight;
    // Default fallback for legacy "native" or unknown values:
    return ThemePreset::DarkSlate;
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
    settings.sync();
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
    settings.sync();
}


namespace {

class BubezFileIconProvider : public QFileIconProvider
{
public:
    QIcon icon(IconType type) const override {
        switch (type) {
        case Folder: return VectorIcons::createIcon(VectorIcons::Type::Folder);
        case Desktop: return VectorIcons::createIcon(VectorIcons::Type::FolderDesktop);
        case Trashcan: return VectorIcons::createIcon(VectorIcons::Type::Folder);
        case Computer: return VectorIcons::createIcon(VectorIcons::Type::Folder);
        case Drive: return VectorIcons::createIcon(VectorIcons::Type::Folder);
        default: return QFileIconProvider::icon(type);
        }
    }

    QIcon icon(const QFileInfo &info) const override {
        if (info.isDir()) {
            const QString name = info.fileName().toLower();
            if (name == "documents" || name == "documenti" || name == "docs") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderDocuments);
            }
            if (name == "pictures" || name == "immagini" || name == "photos" || name == "foto") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderPictures);
            }
            if (name == "downloads" || name == "scaricati" || name == "download") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderDownloads);
            }
            if (name == "music" || name == "musica" || name == "audio") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderMusic);
            }
            if (name == "videos" || name == "video" || name == "movies" || name == "film") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderVideos);
            }
            if (name == "desktop" || name == "scrivania") {
                return VectorIcons::createIcon(VectorIcons::Type::FolderDesktop);
            }
            return VectorIcons::createIcon(VectorIcons::Type::Folder);
        }

        VectorIcons::Type t = VectorIcons::detectType(info.filePath(), false);
        return VectorIcons::createIcon(t);
    }
};

class FileDialogEventFilter : public QObject
{
public:
    explicit FileDialogEventFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            if (auto *dlg = qobject_cast<QFileDialog*>(watched)) {
                ThemeManager::configureFileDialog(dlg);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

void ThemeManager::applyTheme(QApplication *app, QMainWindow *window)
{
    static bool s_filterInstalled = false;
    if (app && !s_filterInstalled) {
        app->installEventFilter(new FileDialogEventFilter(app));
        s_filterInstalled = true;
    }

    ThemePreset preset = currentPreset();
    static ThemePreset s_lastAppliedAppPreset = static_cast<ThemePreset>(-1);
    const bool shouldUpdateApp = (app != nullptr && s_lastAppliedAppPreset != preset);
    if (shouldUpdateApp) {
        s_lastAppliedAppPreset = preset;
    }

    if (preset == ThemePreset::ModernLight) {
        if (shouldUpdateApp) {
            // macOS / Cupertino-inspired Warm Light Theme (#ececf0 / #ffffff)
            app->setStyle(QStyleFactory::create("Fusion"));

            QPalette lightPalette;
            lightPalette.setColor(QPalette::Window, QColor("#ececf0"));
            lightPalette.setColor(QPalette::WindowText, QColor("#1d1d1f"));
            lightPalette.setColor(QPalette::Base, QColor("#ffffff"));
            lightPalette.setColor(QPalette::AlternateBase, QColor("#f4f4f7"));
            lightPalette.setColor(QPalette::ToolTipBase, QColor("#1d1d1f"));
            lightPalette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
            lightPalette.setColor(QPalette::Text, QColor("#1d1d1f"));
            lightPalette.setColor(QPalette::Button, QColor("#ffffff"));
            lightPalette.setColor(QPalette::ButtonText, QColor("#1d1d1f"));
            lightPalette.setColor(QPalette::BrightText, Qt::red);
            lightPalette.setColor(QPalette::Link, QColor("#007aff"));
            lightPalette.setColor(QPalette::Highlight, QColor("#007aff"));
            lightPalette.setColor(QPalette::HighlightedText, Qt::white);
            app->setPalette(lightPalette);

            // Application-wide styling for Dialogs, Segmented Tabs, Cards, and Controls
            app->setStyleSheet(
                "QDialog {"
                "   background-color: #ececf0;"
                "   color: #1d1d1f;"
                "}"
                "QTabWidget::pane {"
                "   border: 1px solid rgba(0, 0, 0, 0.08);"
                "   background-color: #ffffff;"
                "   border-radius: 10px;"
                "   padding: 10px;"
                "}"
                "QTabBar {"
                "   background-color: #e2e2e7;"
                "   border-radius: 8px;"
                "   padding: 3px;"
                "}"
                "QTabBar::tab {"
                "   background: transparent;"
                "   color: #636366;"
                "   border: none;"
                "   border-radius: 6px;"
                "   padding: 6px 14px;"
                "   font-weight: 500;"
                "   font-size: 12px;"
                "   margin: 1px;"
                "}"
                "QTabBar::tab:selected {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   font-weight: 600;"
                "}"
                "QTabBar::tab:hover:!selected {"
                "   background-color: rgba(255, 255, 255, 0.55);"
                "   color: #1d1d1f;"
                "}"
                "QGroupBox {"
                "   font-weight: 600;"
                "   font-size: 13px;"
                "   color: #1d1d1f;"
                "   border: 1px solid rgba(0, 0, 0, 0.08);"
                "   border-radius: 10px;"
                "   background-color: #ffffff;"
                "   margin-top: 14px;"
                "   padding: 18px 12px 12px 12px;"
                "}"
                "QGroupBox::title {"
                "   subcontrol-origin: margin;"
                "   subcontrol-position: top left;"
                "   left: 12px;"
                "   padding: 0 4px;"
                "   background-color: transparent;"
                "}"
                "QComboBox {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   border: 1px solid rgba(0, 0, 0, 0.15);"
                "   border-radius: 6px;"
                "   padding: 5px 12px;"
                "   font-size: 12px;"
                "   font-weight: 500;"
                "   min-height: 22px;"
                "}"
                "QComboBox:hover {"
                "   background-color: #fafafc;"
                "   border-color: rgba(0, 0, 0, 0.28);"
                "}"
                "QComboBox:focus {"
                "   border-color: #007aff;"
                "}"
                "QComboBox::drop-down {"
                "   border: none;"
                "   subcontrol-origin: padding;"
                "   subcontrol-position: top right;"
                "   width: 22px;"
                "}"
                "QComboBox::down-arrow {"
                "   image: url(:/icons/chevron_down_dark.svg);"
                "   width: 12px;"
                "   height: 12px;"
                "   margin-right: 6px;"
                "}"
                "QPushButton {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   border: 1px solid rgba(0, 0, 0, 0.15);"
                "   border-radius: 6px;"
                "   padding: 6px 16px;"
                "   font-size: 12px;"
                "   font-weight: 500;"
                "}"
                "QPushButton:hover {"
                "   background-color: #fafafc;"
                "   border-color: rgba(0, 0, 0, 0.28);"
                "}"
                "QPushButton:pressed {"
                "   background-color: #e5e5ea;"
                "}"
                "QLineEdit {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   border: 1px solid rgba(0, 0, 0, 0.15);"
                "   border-radius: 6px;"
                "   padding: 5px 10px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:focus {"
                "   border-color: #007aff;"
                "}\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
            );
        }

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background-color: #ececf0;"
                "}"
                "QMenuBar {"
                "   background-color: #ececf0;"
                "   color: #1d1d1f;"
                "   border-bottom: 1px solid #dcdce2;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: #e2e2e7;"
                "   color: #1d1d1f;"
                "   border-radius: 4px;"
                "}"
                "QMenu {"
                "   background-color: #ffffff;"
                "   color: #1d1d1f;"
                "   border: 1px solid #dcdce2;"
                "   padding: 4px;"
                "   border-radius: 6px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px;"
                "   border-radius: 4px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #007aff;"
                "   color: #ffffff;"
                "}"
                "QToolBar {"
                "   background-color: #ececf0;"
                "   border-bottom: 1px solid #dcdce2;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #1d1d1f;"
                "   border-radius: 4px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: #e2e2e7;"
                "   color: #1d1d1f;"
                "}"
                "QToolButton:pressed {"
                "   background-color: #d2d2d7;"
                "}"
                "QStatusBar {"
                "   background-color: #e5e5ea;"
                "   color: #636366;"
                "   border-top: 1px solid #dcdce2;"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    } else if (preset == ThemePreset::BreezeLight) {
        if (shouldUpdateApp) {
            // KDE Breeze-inspired Clean Light Theme (#eff0f1 / #fcfcfc)
            app->setStyle(QStyleFactory::create("Fusion"));

            QPalette breezeLightPalette;
            breezeLightPalette.setColor(QPalette::Window, QColor("#eff0f1"));
            breezeLightPalette.setColor(QPalette::WindowText, QColor("#232629"));
            breezeLightPalette.setColor(QPalette::Base, QColor("#fcfcfc"));
            breezeLightPalette.setColor(QPalette::AlternateBase, QColor("#eff0f1"));
            breezeLightPalette.setColor(QPalette::ToolTipBase, QColor("#31363b"));
            breezeLightPalette.setColor(QPalette::ToolTipText, QColor("#eff0f1"));
            breezeLightPalette.setColor(QPalette::Text, QColor("#232629"));
            breezeLightPalette.setColor(QPalette::Button, QColor("#fcfcfc"));
            breezeLightPalette.setColor(QPalette::ButtonText, QColor("#232629"));
            breezeLightPalette.setColor(QPalette::BrightText, Qt::red);
            breezeLightPalette.setColor(QPalette::Link, QColor("#3daee9"));
            breezeLightPalette.setColor(QPalette::Highlight, QColor("#3daee9"));
            breezeLightPalette.setColor(QPalette::HighlightedText, Qt::white);
            app->setPalette(breezeLightPalette);

            app->setStyleSheet(
                "QDialog { background-color: #eff0f1; color: #232629; }"
                "QTabWidget::pane { border: 1px solid #bdc3c7; background-color: #fcfcfc; border-radius: 6px; padding: 6px; }"
                "QTabBar::tab { background: #e2e4e7; color: #232629; border: 1px solid #bdc3c7; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; padding: 6px 14px; }"
                "QTabBar::tab:selected { background: #fcfcfc; border-color: #3daee9; }"
                "QGroupBox { font-weight: bold; border: 1px solid #bdc3c7; border-radius: 6px; background-color: #fcfcfc; margin-top: 12px; padding: 14px 10px 10px 10px; }"
                "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }"
                "QComboBox { background-color: #fcfcfc; color: #232629; border: 1px solid #bdc3c7; border-radius: 4px; padding: 4px 24px 4px 8px; }"
                "QComboBox:hover { border-color: #3daee9; }"
                "QComboBox::drop-down { border: none; subcontrol-origin: padding; subcontrol-position: top right; width: 22px; }"
                "QComboBox::down-arrow { image: url(:/icons/chevron_down_dark.svg); width: 12px; height: 12px; margin-right: 6px; }"
                "QPushButton { background-color: #fcfcfc; color: #232629; border: 1px solid #bdc3c7; border-radius: 4px; padding: 5px 14px; }"
                "QPushButton:hover { background-color: #eff0f1; border-color: #3daee9; }\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
            );
        }

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background-color: #eff0f1;"
                "}"
                "QMenuBar {"
                "   background-color: #eff0f1;"
                "   color: #232629;"
                "   border-bottom: 1px solid #bdc3c7;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: #d8dbde;"
                "   color: #232629;"
                "   border-radius: 4px;"
                "}"
                "QMenu {"
                "   background-color: #fcfcfc;"
                "   color: #232629;"
                "   border: 1px solid #bdc3c7;"
                "   padding: 4px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px;"
                "   border-radius: 4px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #3daee9;"
                "   color: #ffffff;"
                "}"
                "QToolBar {"
                "   background-color: #eff0f1;"
                "   border-bottom: 1px solid #bdc3c7;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #232629;"
                "   border-radius: 4px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: #d8dbde;"
                "   color: #232629;"
                "}"
                "QToolButton:pressed {"
                "   background-color: #bdc3c7;"
                "}"
                "QStatusBar {"
                "   background-color: #e2e4e7;"
                "   color: #626569;"
                "   border-top: 1px solid #bdc3c7;"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    } else if (preset == ThemePreset::DarkSlate) {
        if (shouldUpdateApp) {
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

        // Application-wide styling for Dark Slate Dialogs, Segmented Tabs, and Cards
        app->setStyleSheet(
            "QDialog {"
            "   background-color: #22262b;"
            "   color: #eff0f1;"
            "}"
            "QTabWidget::pane {"
            "   border: 1px solid #2d333b;"
            "   background-color: #1a1d21;"
            "   border-radius: 10px;"
            "   padding: 10px;"
            "}"
            "QTabBar {"
            "   background-color: #16181b;"
            "   border-radius: 8px;"
            "   padding: 3px;"
            "}"
            "QTabBar::tab {"
            "   background: transparent;"
            "   color: #8b949e;"
            "   border: none;"
            "   border-radius: 6px;"
            "   padding: 6px 14px;"
            "   font-weight: 500;"
            "   font-size: 12px;"
            "   margin: 1px;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: #282c32;"
            "   color: #f0f6fc;"
            "   font-weight: 600;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "   color: #f0f6fc;"
            "}"
            "QGroupBox {"
            "   font-weight: 600;"
            "   font-size: 13px;"
            "   color: #f0f6fc;"
            "   border: 1px solid #2d333b;"
            "   border-radius: 10px;"
            "   background-color: #1a1d21;"
            "   margin-top: 14px;"
            "   padding: 18px 12px 12px 12px;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 12px;"
            "   padding: 0 4px;"
            "   background-color: transparent;"
            "}"
            "QComboBox {"
            "   background-color: #24292f;"
            "   color: #f0f6fc;"
            "   border: 1px solid #30363d;"
            "   border-radius: 6px;"
            "   padding: 5px 12px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   min-height: 22px;"
            "}"
            "QComboBox:hover {"
            "   border-color: #38bdf8;"
            "}"
            "QComboBox:focus {"
            "   border-color: #38bdf8;"
            "}"
            "QComboBox::drop-down {"
            "   border: none;"
            "   subcontrol-origin: padding;"
            "   subcontrol-position: top right;"
            "   width: 22px;"
            "}"
            "QComboBox::down-arrow {"
            "   image: url(:/icons/chevron_down_light.svg);"
            "   width: 12px;"
            "   height: 12px;"
            "   margin-right: 6px;"
            "}"
            "QPushButton {"
            "   background-color: #24292f;"
            "   color: #f0f6fc;"
            "   border: 1px solid #30363d;"
            "   border-radius: 6px;"
            "   padding: 6px 16px;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2f353d;"
            "   border-color: #38bdf8;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1a1d21;"
            "}"
            "QLineEdit {"
            "   background-color: #16191c;"
            "   color: #f0f6fc;"
            "   border: 1px solid #30363d;"
            "   border-radius: 6px;"
            "   padding: 5px 10px;"
            "   font-size: 12px;"
            "}"
            "QLineEdit:focus {"
            "   border-color: #38bdf8;"
            "}\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
        );
        }

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
        if (shouldUpdateApp) {
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

            app->setStyleSheet(
                "QDialog { background-color: #181b1d; color: #eff0f1; }"
                "QTabWidget::pane { border: 1px solid #282e34; background-color: #151719; border-radius: 6px; padding: 6px; }"
                "QTabBar::tab { background: #23272b; color: #eff0f1; border: 1px solid #282e34; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; padding: 6px 14px; }"
                "QTabBar::tab:selected { background: #151719; border-color: #3daee9; }"
                "QGroupBox { font-weight: bold; border: 1px solid #282e34; border-radius: 6px; background-color: #151719; margin-top: 12px; padding: 14px 10px 10px 10px; }"
                "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }"
                "QComboBox { background-color: #23272b; color: #eff0f1; border: 1px solid #363d45; border-radius: 4px; padding: 4px 24px 4px 8px; }"
                "QComboBox:hover { border-color: #3daee9; }"
                "QComboBox::drop-down { border: none; subcontrol-origin: padding; subcontrol-position: top right; width: 22px; }"
                "QComboBox::down-arrow { image: url(:/icons/chevron_down_light.svg); width: 12px; height: 12px; margin-right: 6px; }"
                "QPushButton { background-color: #23272b; color: #eff0f1; border: 1px solid #363d45; border-radius: 4px; padding: 5px 14px; }"
                "QPushButton:hover { background-color: #2e343b; border-color: #3daee9; }\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
            );
        }

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
    if (preset == ThemePreset::ModernLight) {
        // High-contrast Modern Light scrollbars: frosted satin track with smooth gray pill handle
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #f0f0f2;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #9e9ea7;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #787882;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #565660;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #f0f0f2;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #9e9ea7;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #787882;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #565660;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "   background: none;"
            "}"
        );
    } else if (preset == ThemePreset::BreezeLight) {
        // High-contrast Breeze Light scrollbars: clean soft track with distinct Breeze gray handle
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #eff0f1;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #a5adb5;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #7d8792;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #5c6570;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #eff0f1;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #a5adb5;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #7d8792;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #5c6570;"
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

/**
 * @brief Generates cohesive, modern stylesheet rules for QFileDialog and file browser views.
 *
 * Provides elegant dark/light theme styling for the file picker tree, places sidebar,
 * headers, path bars, and action buttons.
 *
 * @param preset The target ThemePreset.
 * @return Formatted Qt stylesheet string for QFileDialog and its sub-widgets.
 */
QString ThemeManager::fileDialogStyleSheet(ThemePreset preset)
{
    const bool dark = isDark(preset);
    if (dark) {
        const QString accent = (preset == ThemePreset::DarkSlate) ? QStringLiteral("#0284c7") : QStringLiteral("#3daee9");
        const QString accentHover = (preset == ThemePreset::DarkSlate) ? QStringLiteral("#0369a1") : QStringLiteral("#2980b9");
        const QString bg = (preset == ThemePreset::DarkSlate) ? QStringLiteral("#22262b") : QStringLiteral("#181b1d");
        const QString border = (preset == ThemePreset::DarkSlate) ? QStringLiteral("#2d333b") : QStringLiteral("#282e34");

        return QString(
            "QFileDialog { background-color: %1; color: #eff0f1; }\n"
            "QFileDialog QLabel { color: #eff0f1; font-size: 12px; }\n"
            "QFileDialog QSplitter::handle { background-color: %2; width: 2px; height: 2px; }\n"
            "QFileDialog QSidebar, QFileDialog QListWidget#sidebar, QFileDialog QListView#sidebar {\n"
            "   background-color: #151719;\n"
            "   color: #eff0f1;\n"
            "   border: none;\n"
            "   border-right: 1px solid %2;\n"
            "   outline: none;\n"
            "   padding: 4px;\n"
            "}\n"
            "QFileDialog QSidebar::item, QFileDialog QListWidget#sidebar::item, QFileDialog QListView#sidebar::item {\n"
            "   padding: 6px 10px;\n"
            "   border-radius: 6px;\n"
            "   margin: 1px 4px;\n"
            "   color: #eff0f1;\n"
            "}\n"
            "QFileDialog QSidebar::item:hover, QFileDialog QListWidget#sidebar::item:hover, QFileDialog QListView#sidebar::item:hover {\n"
            "   background-color: #24292f;\n"
            "}\n"
            "QFileDialog QSidebar::item:selected, QFileDialog QListWidget#sidebar::item:selected, QFileDialog QListView#sidebar::item:selected {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "   font-weight: 500;\n"
            "}\n"
            "QFileDialog QTreeView, QFileDialog QListView {\n"
            "   background-color: #151719;\n"
            "   alternate-background-color: #1a1d20;\n"
            "   color: #eff0f1;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   outline: none;\n"
            "   padding: 2px;\n"
            "}\n"
            "QFileDialog QTreeView::item, QFileDialog QListView::item {\n"
            "   padding: 4px 6px;\n"
            "   border-radius: 4px;\n"
            "   color: #eff0f1;\n"
            "}\n"
            "QFileDialog QTreeView::item:hover, QFileDialog QListView::item:hover {\n"
            "   background-color: #24292f;\n"
            "}\n"
            "QFileDialog QTreeView::item:selected, QFileDialog QListView::item:selected {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "}\n"
            "QFileDialog QHeaderView::section {\n"
            "   background-color: #1a1d20;\n"
            "   color: #8a9098;\n"
            "   border: none;\n"
            "   border-bottom: 1px solid %2;\n"
            "   padding: 5px 8px;\n"
            "   font-weight: 600;\n"
            "   font-size: 11px;\n"
            "}\n"
            "QFileDialog QToolButton {\n"
            "   background-color: #23272b;\n"
            "   color: #eff0f1;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px;\n"
            "   min-width: 34px;\n"
            "   min-height: 30px;\n"
            "}\n"
            "QFileDialog QToolButton:hover {\n"
            "   background-color: #2e343b;\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QToolButton:pressed {\n"
            "   background-color: #1d2125;\n"
            "}\n"
            "QFileDialog QToolButton:checked {\n"
            "   background-color: %3;\n"
            "   border-color: %3;\n"
            "   color: #ffffff;\n"
            "}\n"
            "QFileDialog QLineEdit {\n"
            "   background-color: #16191c;\n"
            "   color: #f0f6fc;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px 8px;\n"
            "   font-size: 12px;\n"
            "   min-height: 24px;\n"
            "}\n"
            "QFileDialog QLineEdit:focus {\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QComboBox {\n"
            "   background-color: #23272b;\n"
            "   color: #eff0f1;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px 28px 4px 10px;\n"
            "   font-size: 12px;\n"
            "   min-height: 24px;\n"
            "}\n"
            "QFileDialog QComboBox:hover {\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QComboBox::drop-down {\n"
            "   border: none;\n"
            "   subcontrol-origin: padding;\n"
            "   subcontrol-position: top right;\n"
            "   width: 24px;\n"
            "}\n"
            "QFileDialog QComboBox::down-arrow {\n"
            "   image: url(:/icons/chevron_down_light.svg);\n"
            "   width: 12px;\n"
            "   height: 12px;\n"
            "   margin-right: 6px;\n"
            "}\n"
            "QFileDialog QComboBox QAbstractItemView {\n"
            "   background-color: %1;\n"
            "   color: #eff0f1;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   selection-background-color: %3;\n"
            "   selection-color: #ffffff;\n"
            "   padding: 4px;\n"
            "}\n"
            "QFileDialog QPushButton {\n"
            "   background-color: #23272b;\n"
            "   color: #eff0f1;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 6px 18px;\n"
            "   font-weight: 500;\n"
            "   min-width: 76px;\n"
            "   min-height: 28px;\n"
            "}\n"
            "QFileDialog QPushButton:hover {\n"
            "   background-color: #2e343b;\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QPushButton:default {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "   border: none;\n"
            "   font-weight: 600;\n"
            "}\n"
            "QFileDialog QPushButton:default:hover {\n"
            "   background-color: %4;\n"
            "}\n"
        ).arg(bg, border, accent, accentHover);
    } else {
        const QString accent = (preset == ThemePreset::ModernLight) ? QStringLiteral("#007aff") : QStringLiteral("#3daee9");
        const QString accentHover = (preset == ThemePreset::ModernLight) ? QStringLiteral("#0062cc") : QStringLiteral("#2980b9");
        const QString bg = (preset == ThemePreset::ModernLight) ? QStringLiteral("#f5f5f7") : QStringLiteral("#eff0f1");
        const QString border = (preset == ThemePreset::ModernLight) ? QStringLiteral("rgba(0, 0, 0, 0.12)") : QStringLiteral("#bdc3c7");
        const QString viewBg = (preset == ThemePreset::ModernLight) ? QStringLiteral("#ffffff") : QStringLiteral("#fcfcfc");
        const QString viewAltBg = (preset == ThemePreset::ModernLight) ? QStringLiteral("#f9f9fb") : QStringLiteral("#f5f5f5");
        const QString sidebarBg = (preset == ThemePreset::ModernLight) ? QStringLiteral("#ebebef") : QStringLiteral("#e2e4e7");
        const QString headerBg = (preset == ThemePreset::ModernLight) ? QStringLiteral("#f5f5f7") : QStringLiteral("#eff0f1");
        const QString text = (preset == ThemePreset::ModernLight) ? QStringLiteral("#1d1d1f") : QStringLiteral("#232629");
        const QString textDim = (preset == ThemePreset::ModernLight) ? QStringLiteral("#6e6e73") : QStringLiteral("#7f8c8d");

        return QString(
            "QFileDialog { background-color: %1; color: %9; }\n"
            "QFileDialog QLabel { color: %9; font-size: 12px; }\n"
            "QFileDialog QSplitter::handle { background-color: %2; width: 2px; height: 2px; }\n"
            "QFileDialog QSidebar, QFileDialog QListWidget#sidebar, QFileDialog QListView#sidebar {\n"
            "   background-color: %7;\n"
            "   color: %9;\n"
            "   border: none;\n"
            "   border-right: 1px solid %2;\n"
            "   outline: none;\n"
            "   padding: 4px;\n"
            "}\n"
            "QFileDialog QSidebar::item, QFileDialog QListWidget#sidebar::item, QFileDialog QListView#sidebar::item {\n"
            "   padding: 6px 10px;\n"
            "   border-radius: 6px;\n"
            "   margin: 1px 4px;\n"
            "   color: %9;\n"
            "}\n"
            "QFileDialog QSidebar::item:hover, QFileDialog QListWidget#sidebar::item:hover, QFileDialog QListView#sidebar::item:hover {\n"
            "   background-color: rgba(0, 0, 0, 0.05);\n"
            "}\n"
            "QFileDialog QSidebar::item:selected, QFileDialog QListWidget#sidebar::item:selected, QFileDialog QListView#sidebar::item:selected {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "   font-weight: 500;\n"
            "}\n"
            "QFileDialog QTreeView, QFileDialog QListView {\n"
            "   background-color: %5;\n"
            "   alternate-background-color: %6;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   outline: none;\n"
            "   padding: 2px;\n"
            "}\n"
            "QFileDialog QTreeView::item, QFileDialog QListView::item {\n"
            "   padding: 4px 6px;\n"
            "   border-radius: 4px;\n"
            "   color: %9;\n"
            "}\n"
            "QFileDialog QTreeView::item:hover, QFileDialog QListView::item:hover {\n"
            "   background-color: rgba(0, 0, 0, 0.04);\n"
            "}\n"
            "QFileDialog QTreeView::item:selected, QFileDialog QListView::item:selected {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "}\n"
            "QFileDialog QHeaderView::section {\n"
            "   background-color: %8;\n"
            "   color: %10;\n"
            "   border: none;\n"
            "   border-bottom: 1px solid %2;\n"
            "   padding: 5px 8px;\n"
            "   font-weight: 600;\n"
            "   font-size: 11px;\n"
            "}\n"
            "QFileDialog QToolButton {\n"
            "   background-color: %5;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px;\n"
            "   min-width: 34px;\n"
            "   min-height: 30px;\n"
            "}\n"
            "QFileDialog QToolButton:hover {\n"
            "   background-color: rgba(0, 0, 0, 0.05);\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QToolButton:pressed {\n"
            "   background-color: rgba(0, 0, 0, 0.10);\n"
            "}\n"
            "QFileDialog QToolButton:checked {\n"
            "   background-color: %3;\n"
            "   border-color: %3;\n"
            "   color: #ffffff;\n"
            "}\n"
            "QFileDialog QLineEdit {\n"
            "   background-color: %5;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px 8px;\n"
            "   font-size: 12px;\n"
            "   min-height: 24px;\n"
            "}\n"
            "QFileDialog QLineEdit:focus {\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QComboBox {\n"
            "   background-color: %5;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 4px 28px 4px 10px;\n"
            "   font-size: 12px;\n"
            "   min-height: 24px;\n"
            "}\n"
            "QFileDialog QComboBox:hover {\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QComboBox::drop-down {\n"
            "   border: none;\n"
            "   subcontrol-origin: padding;\n"
            "   subcontrol-position: top right;\n"
            "   width: 24px;\n"
            "}\n"
            "QFileDialog QComboBox::down-arrow {\n"
            "   image: url(:/icons/chevron_down_dark.svg);\n"
            "   width: 12px;\n"
            "   height: 12px;\n"
            "   margin-right: 6px;\n"
            "}\n"
            "QFileDialog QComboBox QAbstractItemView {\n"
            "   background-color: %5;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   selection-background-color: %3;\n"
            "   selection-color: #ffffff;\n"
            "   padding: 4px;\n"
            "}\n"
            "QFileDialog QPushButton {\n"
            "   background-color: %5;\n"
            "   color: %9;\n"
            "   border: 1px solid %2;\n"
            "   border-radius: 6px;\n"
            "   padding: 6px 18px;\n"
            "   font-weight: 500;\n"
            "   min-width: 76px;\n"
            "   min-height: 28px;\n"
            "}\n"
            "QFileDialog QPushButton:hover {\n"
            "   background-color: rgba(0, 0, 0, 0.05);\n"
            "   border-color: %3;\n"
            "}\n"
            "QFileDialog QPushButton:default {\n"
            "   background-color: %3;\n"
            "   color: #ffffff;\n"
            "   border: none;\n"
            "   font-weight: 600;\n"
            "}\n"
            "QFileDialog QPushButton:default:hover {\n"
            "   background-color: %4;\n"
            "}\n"
        ).arg(bg, border, accent, accentHover, viewBg, viewAltBg, sidebarBg, headerBg, text, textDim);
    }
}

/**
 * @brief Configures any QFileDialog instance with BubezView places (FolderViewer bookmarks),
 * crisp vector navigation icons, and non-truncated sidebar geometry.
 *
 * @param dialog Pointer to the target QFileDialog to decorate.
 */
void ThemeManager::configureFileDialog(QFileDialog *dialog)
{
    if (!dialog) return;

    // Set vector icon provider matching BubezView FolderViewer design
    dialog->setIconProvider(new BubezFileIconProvider);

    // 1. Configure Places Sidebar with canonical deduplication
    QList<QUrl> sidebarUrls;
    QStringList canonicalAdded;

    auto addPath = [&](const QString &rawPath) {
        if (rawPath.isEmpty()) return;
        QDir dir(rawPath);
        if (!dir.exists()) return;
        const QString canonical = dir.canonicalPath();
        if (canonicalAdded.contains(canonical)) {
            return; // Prevent duplicate entries (e.g. /home/davide and /run/host/home/davide)
        }
        canonicalAdded.append(canonical);
        sidebarUrls << QUrl::fromLocalFile(rawPath);
    };

    QString home = QDir::homePath();
    if (!QDir(home + "/Documenti").exists() && QDir("/home/davide/Documenti").exists()) {
        home = "/home/davide";
    }
    addPath(home);

    auto addDir = [&](QStandardPaths::StandardLocation loc, const QStringList &fallbacks) {
        QString p = QStandardPaths::writableLocation(loc);
        if (!p.isEmpty() && QDir(p).exists()) {
            addPath(p);
            return;
        }
        for (const QString &fb : fallbacks) {
            QString c = home + "/" + fb;
            if (QDir(c).exists()) {
                addPath(c);
                return;
            }
        }
    };

    addDir(QStandardPaths::PicturesLocation, {"Immagini", "Pictures", "Photos", "Foto"});
    addDir(QStandardPaths::DocumentsLocation, {"Documenti", "Documents", "Docs"});
    addDir(QStandardPaths::DownloadLocation, {"Scaricati", "Downloads", "Download"});
    addDir(QStandardPaths::MoviesLocation, {"Video", "Videos", "Movies", "Film"});
    addDir(QStandardPaths::MusicLocation, {"Musica", "Music", "Audio"});
    addDir(QStandardPaths::DesktopLocation, {"Desktop", "Scrivania"});

    // Add custom user bookmarks saved in BubezView settings
    QSettings bubezSettings1("BubezView", "BubezView");
    const QStringList bms1 = bubezSettings1.value("FolderViewer/CustomBookmarks").toStringList();
    for (const QString &bm : bms1) {
        addPath(bm);
    }
    QSettings bubezSettings2("Bubez", "BubezView");
    const QStringList bms2 = bubezSettings2.value("FolderViewer/CustomBookmarks").toStringList();
    for (const QString &bm : bms2) {
        addPath(bm);
    }

    // Root filesystem
    sidebarUrls << QUrl::fromLocalFile(QStringLiteral("/"));
    dialog->setSidebarUrls(sidebarUrls);

    // 2. Set comfortable sidebar width (185px) and 22x22 iconSize
    for (auto *splitter : dialog->findChildren<QSplitter*>()) {
        if (splitter->count() >= 2) {
            const int totalW = splitter->width() > 300 ? splitter->width() : 800;
            splitter->setSizes({185, totalW - 185});
            if (auto *sidebar = qobject_cast<QAbstractItemView*>(splitter->widget(0))) {
                sidebar->setIconSize(QSize(22, 22));
            }
        }
    }

    // 3. Central Tree / List views styling & column width
    for (auto *treeView : dialog->findChildren<QTreeView*>()) {
        treeView->setIconSize(QSize(22, 22));
        if (auto *header = treeView->header()) {
            header->setStretchLastSection(true);
            header->resizeSection(0, 320); // Generous width for Name column
        }
    }
    for (auto *listView : dialog->findChildren<QListView*>()) {
        if (listView->objectName() != QLatin1String("sidebar")) {
            listView->setIconSize(QSize(36, 36));
        }
    }

    // 4. Clean up Open and Cancel buttons (remove small/dark system icons)
    for (auto *pushBtn : dialog->findChildren<QPushButton*>()) {
        pushBtn->setIcon(QIcon());
        pushBtn->setMinimumHeight(32);
    }

    // 5. Large, crisp vector navigation buttons (Back, Forward, Parent, New Folder, List, Detail)
    const bool dark = isDark(currentPreset());
    const QColor navNormal = dark ? QColor("#e2e8f0") : QColor("#232629");
    const QColor navDisabled = dark ? QColor(226, 232, 240, 80) : QColor(35, 38, 41, 80);

    auto makeNavIcon = [&](auto drawFunc) -> QIcon {
        QIcon icon;
        // Normal state (HiDPI 48x48 canvas)
        {
            QPixmap pix(48, 48);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(navNormal, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawFunc(p);
            icon.addPixmap(pix, QIcon::Normal, QIcon::Off);
        }
        // Checked / Active (On) state (White against accent background)
        {
            QPixmap pix(48, 48);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(Qt::white, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawFunc(p);
            icon.addPixmap(pix, QIcon::Normal, QIcon::On);
            icon.addPixmap(pix, QIcon::Active, QIcon::On);
            icon.addPixmap(pix, QIcon::Selected, QIcon::On);
        }
        // Disabled state
        {
            QPixmap pix(48, 48);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(navDisabled, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawFunc(p);
            icon.addPixmap(pix, QIcon::Disabled, QIcon::Off);
        }
        return icon;
    };

    auto configureNavBtn = [&](QToolButton *btn, const QIcon &icon, const QString &tooltip) {
        if (!btn) return;
        btn->setIcon(icon);
        btn->setIconSize(QSize(22, 22));
        btn->setFixedSize(36, 32);
        btn->setToolTip(tooltip);
    };

    configureNavBtn(dialog->findChild<QToolButton*>("backButton"), makeNavIcon([](QPainter &p) {
        p.drawLine(28, 12, 16, 24);
        p.drawLine(16, 24, 28, 36);
    }), QObject::tr("Back"));

    configureNavBtn(dialog->findChild<QToolButton*>("forwardButton"), makeNavIcon([](QPainter &p) {
        p.drawLine(20, 12, 32, 24);
        p.drawLine(32, 24, 20, 36);
    }), QObject::tr("Forward"));

    configureNavBtn(dialog->findChild<QToolButton*>("toParentButton"), makeNavIcon([](QPainter &p) {
        p.drawLine(12, 26, 24, 14);
        p.drawLine(24, 14, 36, 26);
        p.drawLine(24, 14, 24, 35);
    }), QObject::tr("Parent Directory"));

    configureNavBtn(dialog->findChild<QToolButton*>("newFolderButton"), makeNavIcon([](QPainter &p) {
        // Crisp folder outline with tab
        QPainterPath path;
        path.moveTo(8, 36);
        path.lineTo(8, 15);
        path.lineTo(19, 15);
        path.lineTo(23, 19);
        path.lineTo(40, 19);
        path.lineTo(40, 36);
        path.closeSubpath();
        p.drawPath(path);
        // Centered plus inside folder
        p.drawLine(24, 23, 24, 32);
        p.drawLine(19.5, 27.5, 28.5, 27.5);
    }), QObject::tr("Create New Folder"));

    configureNavBtn(dialog->findChild<QToolButton*>("listModeButton"), makeNavIcon([](QPainter &p) {
        // Grid / tiles representation for Qt List Mode
        p.drawRoundedRect(QRectF(10, 10, 12, 12), 2, 2);
        p.drawRoundedRect(QRectF(26, 10, 12, 12), 2, 2);
        p.drawRoundedRect(QRectF(10, 26, 12, 12), 2, 2);
        p.drawRoundedRect(QRectF(26, 26, 12, 12), 2, 2);
    }), QObject::tr("List View"));

    configureNavBtn(dialog->findChild<QToolButton*>("detailModeButton"), makeNavIcon([](QPainter &p) {
        // Detail table rows representation for Qt Detail Mode
        p.drawLine(10, 15, 38, 15);
        p.drawLine(10, 24, 38, 24);
        p.drawLine(10, 33, 38, 33);
    }), QObject::tr("Detail View"));
}

