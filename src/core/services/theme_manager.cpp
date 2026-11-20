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
#include <QStyleHints>
#include <QFile>
#include <QElapsedTimer>

namespace {
QPalette s_initialSystemPalette;
bool s_hasInitialSystemPalette = false;
ThemePreset s_lastAppliedAppPreset = ThemePreset::System;
bool s_hasAppliedAppPreset = false;

/**
 * @brief Parses an "R,G,B" comma-separated color entry from KDE kdeglobals.
 */
QColor parseKdeRgb(const QString &raw)
{
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3) return QColor();
    bool okR = false, okG = false, okB = false;
    const int r = parts[0].trimmed().toInt(&okR);
    const int g = parts[1].trimmed().toInt(&okG);
    const int b = parts[2].trimmed().toInt(&okB);
    if (!okR || !okG || !okB) return QColor();
    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
}

/**
 * @brief Reads KDE Plasma's active [Colors:Window] brightness directly from kdeglobals.
 *
 * Reading kdeglobals directly prevents stale widgetStyle plugins (e.g. kvantum-dark)
 * from overriding QGuiApplication::palette() with dark colors when the user's active
 * KDE color scheme (used by Dolphin and Plasma) is light.
 *
 * @param outIsDark Output flag set to true if KDE window scheme is dark.
 * @return True if kdeglobals contained a valid [Colors:Window] BackgroundNormal entry.
 */
bool detectKdeGlobalsDark(bool &outIsDark)
{
    QString kdeGlobalsPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                             + QStringLiteral("/kdeglobals");
    if (!QFile::exists(kdeGlobalsPath)) {
        kdeGlobalsPath = QDir::homePath() + QStringLiteral("/.config/kdeglobals");
    }
    if (!QFile::exists(kdeGlobalsPath)) {
        return false;
    }

    QSettings kdeCfg(kdeGlobalsPath, QSettings::IniFormat);
    kdeCfg.beginGroup(QStringLiteral("Colors:Window"));
    const QString bgRaw = kdeCfg.value(QStringLiteral("BackgroundNormal")).toString();
    const QString fgRaw = kdeCfg.value(QStringLiteral("ForegroundNormal")).toString();
    kdeCfg.endGroup();

    const QColor bg = parseKdeRgb(bgRaw);
    if (!bg.isValid()) {
        return false;
    }
    const QColor fg = parseKdeRgb(fgRaw);
    if (fg.isValid()) {
        outIsDark = (fg.lightness() > bg.lightness());
    } else {
        outIsDark = (bg.lightness() < 128);
    }
    return true;
}
} // namespace

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

void ThemeManager::captureInitialSystemPalette()
{
    if (!s_hasInitialSystemPalette && QGuiApplication::instance()) {
        s_initialSystemPalette = QGuiApplication::palette();
        s_hasInitialSystemPalette = true;
    }
}

ThemePreset ThemeManager::lastAppliedEffectivePreset()
{
    return s_hasAppliedAppPreset ? s_lastAppliedAppPreset : ThemePreset::System;
}

bool ThemeManager::isSystemDark()
{
    // Throttle filesystem/portal checks to at most once every 500ms so frequent UI
    // repaints or cascading theme queries do not repeatedly parse kdeglobals from disk.
    static QElapsedTimer s_cacheTimer;
    static bool s_cachedDark = false;
    static bool s_hasCache = false;
    if (s_hasCache && s_cacheTimer.isValid() && s_cacheTimer.elapsed() < 500) {
        return s_cachedDark;
    }

    auto storeAndReturn = [&](bool dark) -> bool {
        s_cachedDark = dark;
        s_hasCache = true;
        s_cacheTimer.restart();
        return dark;
    };

    // 1. On KDE Plasma, prefer [Colors:Window] in ~/.config/kdeglobals so active color schemes
    //    always match Dolphin/Plasma even if a legacy widgetStyle (e.g. kvantum-dark) is set.
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toUpper();
    if (desktop.contains(QStringLiteral("KDE")) || desktop.contains(QStringLiteral("PLASMA")) ||
        qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
        bool kdeDark = false;
        if (detectKdeGlobalsDark(kdeDark)) {
            return storeAndReturn(kdeDark);
        }
    }

    // 2. Check Qt 6.5+ native platform color scheme hint (XDG Desktop Portal / GNOME / Wayland)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::instance() && QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) return storeAndReturn(true);
        if (scheme == Qt::ColorScheme::Light) return storeAndReturn(false);
    }
#endif

    // 3. Fallback: try kdeglobals even if XDG_CURRENT_DESKTOP is unset (e.g. inside containers)
    {
        bool kdeDark = false;
        if (detectKdeGlobalsDark(kdeDark)) {
            return storeAndReturn(kdeDark);
        }
    }

    // 4. Final fallback: use the original OS QPalette captured before BubezView modified it
    captureInitialSystemPalette();
    const QPalette &pal = s_hasInitialSystemPalette ? s_initialSystemPalette : QGuiApplication::palette();
    const int bgLightness = pal.color(QPalette::Window).lightness();
    const int textLightness = pal.color(QPalette::WindowText).lightness();
    return storeAndReturn(textLightness > bgLightness);
}

QString ThemeManager::presetToString(ThemePreset preset)
{
    switch (preset) {
    case ThemePreset::System:      return "system";
    case ThemePreset::DarkSlate:   return "dark_slate";
    case ThemePreset::ZincGray:    return "zinc_gray";
    case ThemePreset::BreezeDark:  return "breeze_dark";
    case ThemePreset::ModernLight: return "modern_light";
    case ThemePreset::BreezeLight: return "breeze_light";
    }
    return "dark_slate";
}

ThemePreset ThemeManager::stringToPreset(const QString &str)
{
    if (str == "system")       return ThemePreset::System;
    if (str == "dark_slate")   return ThemePreset::DarkSlate;
    if (str == "zinc_gray")    return ThemePreset::ZincGray;
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

int ThemeManager::glassOpacity()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("glass_opacity", 90).toInt();
}

void ThemeManager::setGlassOpacity(int opacityPercent)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("glass_opacity", qBound(50, opacityPercent, 100));
    settings.sync();
}

int ThemeManager::uiScalePercent()
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("ui_scale_factor", 100).toInt();
}

void ThemeManager::setUiScalePercent(int scalePercent)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("ui_scale_factor", qBound(65, scalePercent, 200));
    settings.sync();
}

IconStyle ThemeManager::iconStyle()
{
    QSettings settings("Bubez", "BubezView");
    return static_cast<IconStyle>(settings.value("appearance/icon_style", 0).toInt());
}

void ThemeManager::setIconStyle(IconStyle style)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("appearance/icon_style", static_cast<int>(style));
    settings.sync();
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
    captureInitialSystemPalette();

    static bool s_filterInstalled = false;
    if (app && !s_filterInstalled) {
        app->installEventFilter(new FileDialogEventFilter(app));
        s_filterInstalled = true;
    }

    const ThemePreset preset = effectivePreset(currentPreset());
    const bool shouldUpdateApp = (app != nullptr && (!s_hasAppliedAppPreset || s_lastAppliedAppPreset != preset));
    if (shouldUpdateApp) {
        s_lastAppliedAppPreset = preset;
        s_hasAppliedAppPreset = true;
    }

    if (preset == ThemePreset::ModernLight) {
        if (shouldUpdateApp) {
            // Frosted Satin Light Theme (#e2e5ea / #f1f3f6 satin finish, anti-glare)
            app->setStyle(QStyleFactory::create("Fusion"));

            QPalette lightPalette;
            lightPalette.setColor(QPalette::Window, QColor("#e2e5ea"));
            lightPalette.setColor(QPalette::WindowText, QColor("#1e293b"));
            lightPalette.setColor(QPalette::Base, QColor("#f1f3f6"));
            lightPalette.setColor(QPalette::AlternateBase, QColor("#e5e8ed"));
            lightPalette.setColor(QPalette::ToolTipBase, QColor("#1e293b"));
            lightPalette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
            lightPalette.setColor(QPalette::Text, QColor("#1e293b"));
            lightPalette.setColor(QPalette::Button, QColor("#f8f9fa"));
            lightPalette.setColor(QPalette::ButtonText, QColor("#1e293b"));
            lightPalette.setColor(QPalette::BrightText, Qt::red);
            lightPalette.setColor(QPalette::Link, QColor("#0284c7"));
            lightPalette.setColor(QPalette::Highlight, QColor("#0284c7"));
            lightPalette.setColor(QPalette::HighlightedText, Qt::white);
            app->setPalette(lightPalette);

            // Application-wide styling for Dialogs, Segmented Tabs, Cards, and Controls
            app->setStyleSheet(
                "QDialog {"
                "   background-color: #e2e5ea;"
                "   color: #1e293b;"
                "}"
                "QTabWidget::pane {"
                "   border: 1px solid rgba(0, 0, 0, 0.08);"
                "   background-color: rgba(255, 255, 255, 0.45);"
                "   border-radius: 10px;"
                "   padding: 10px;"
                "}"
                "QTabBar {"
                "   background-color: rgba(0, 0, 0, 0.05);"
                "   border-radius: 8px;"
                "   padding: 3px;"
                "}"
                "QTabBar::tab {"
                "   background: transparent;"
                "   color: #64748b;"
                "   border: none;"
                "   border-radius: 6px;"
                "   padding: 6px 14px;"
                "   font-weight: 500;"
                "   font-size: 12px;"
                "   margin: 1px;"
                "}"
                "QTabBar::tab:selected {"
                "   background-color: #ffffff;"
                "   color: #1e293b;"
                "   font-weight: 600;"
                "   border: 1px solid rgba(0, 0, 0, 0.10);"
                "}"
                "QTabBar::tab:hover:!selected {"
                "   background-color: rgba(255, 255, 255, 0.40);"
                "   color: #1e293b;"
                "}"
                "QGroupBox {"
                "   font-weight: 600;"
                "   font-size: 13px;"
                "   color: #1e293b;"
                "   border: 1px solid rgba(15, 23, 42, 0.09);"
                "   border-radius: 10px;"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60));"
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
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0);"
                "   color: #1e293b;"
                "   border: 1px solid rgba(0, 0, 0, 0.14);"
                "   border-radius: 6px;"
                "   padding: 5px 12px;"
                "   font-size: 12px;"
                "   font-weight: 500;"
                "   min-height: 22px;"
                "}"
                "QComboBox:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e1e5eb);"
                "   border-color: rgba(0, 0, 0, 0.24);"
                "}"
                "QComboBox:focus {"
                "   border-color: #0284c7;"
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
                "QComboBox QAbstractItemView {"
                "   background-color: #f8fafc;"
                "   color: #1e293b;"
                "   border: 1px solid rgba(0, 0, 0, 0.14);"
                "   border-radius: 8px;"
                "   selection-background-color: #0284c7;"
                "   selection-color: #ffffff;"
                "   padding: 4px;"
                "   outline: none;"
                "}"
                "QComboBox QAbstractItemView::item {"
                "   min-height: 24px;"
                "   padding: 5px 10px;"
                "   border-radius: 6px;"
                "   color: #1e293b;"
                "   background-color: transparent;"
                "}"
                "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
                "   background-color: #0284c7;"
                "   color: #ffffff;"
                "}"
                "QPushButton {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0);"
                "   color: #1e293b;"
                "   border: 1px solid rgba(0, 0, 0, 0.14);"
                "   border-radius: 6px;"
                "   padding: 6px 16px;"
                "   font-size: 12px;"
                "   font-weight: 500;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e1e5eb);"
                "   border-color: rgba(0, 0, 0, 0.24);"
                "}"
                "QPushButton:pressed {"
                "   background-color: #d8dce2;"
                "}"
                "QLineEdit {"
                "   background-color: #f8fafc;"
                "   color: #1e293b;"
                "   border: 1px solid rgba(0, 0, 0, 0.14);"
                "   border-radius: 6px;"
                "   padding: 5px 10px;"
                "   font-size: 12px;"
                "}"
                "QLineEdit:focus {"
                "   border-color: #0284c7;"
                "}\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
            );
        }

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background-color: #e2e5ea;"
                "}"
                "QMenuBar {"
                "   background-color: #e2e5ea;"
                "   color: #1e293b;"
                "   border-bottom: 1px solid #d4d8de;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: rgba(0, 0, 0, 0.06);"
                "   color: #1e293b;"
                "   border-radius: 4px;"
                "}"
                "QMenu {"
                "   background-color: #f8fafc;"
                "   color: #1e293b;"
                "   border: 1px solid #d4d8de;"
                "   padding: 4px;"
                "   border-radius: 6px;"
                "}"
                "QMenu::item {"
                "   padding: 6px 20px;"
                "   border-radius: 4px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #0284c7;"
                "   color: #ffffff;"
                "}"
                "QToolBar {"
                "   background-color: #e2e5ea;"
                "   border-bottom: 1px solid #d4d8de;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #1e293b;"
                "   border-radius: 4px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: rgba(0, 0, 0, 0.06);"
                "   color: #1e293b;"
                "}"
                "QToolButton:pressed {"
                "   background-color: #cbcfd6;"
                "}"
                "QStatusBar {"
                "   background-color: #d8dce2;"
                "   color: #64748b;"
                "   border-top: 1px solid #d4d8de;"
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
            "QComboBox QAbstractItemView {"
            "   background-color: #151c28;"
            "   color: #f0f6fc;"
            "   border: 1px solid rgba(56, 189, 248, 0.38);"
            "   border-radius: 8px;"
            "   selection-background-color: #0284c7;"
            "   selection-color: #ffffff;"
            "   padding: 4px;"
            "   outline: none;"
            "}"
            "QComboBox QAbstractItemView::item {"
            "   min-height: 24px;"
            "   padding: 5px 10px;"
            "   border-radius: 6px;"
            "   color: #f0f6fc;"
            "   background-color: transparent;"
            "}"
            "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
            "   background-color: #0284c7;"
            "   color: #ffffff;"
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
    } else if (preset == ThemePreset::ZincGray) {
        if (shouldUpdateApp) {
            // Dark Neutral Zinc Theme (#36373b base with #585960 pills/controls)
            app->setStyle(QStyleFactory::create("Fusion"));

            QPalette darkPalette;
            darkPalette.setColor(QPalette::Window, QColor("#36373b"));
            darkPalette.setColor(QPalette::WindowText, QColor("#f8fafc"));
            darkPalette.setColor(QPalette::Base, QColor("#2a2b2e"));
            darkPalette.setColor(QPalette::AlternateBase, QColor("#323336"));
            darkPalette.setColor(QPalette::ToolTipBase, QColor("#202123"));
            darkPalette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
            darkPalette.setColor(QPalette::Text, QColor("#ffffff"));
            darkPalette.setColor(QPalette::Button, QColor("#4e4f55"));
            darkPalette.setColor(QPalette::ButtonText, QColor("#ffffff"));
            darkPalette.setColor(QPalette::BrightText, Qt::red);
            darkPalette.setColor(QPalette::Link, QColor("#9ca3af"));
            darkPalette.setColor(QPalette::Highlight, QColor("#585960"));
            darkPalette.setColor(QPalette::HighlightedText, Qt::white);
            app->setPalette(darkPalette);

            app->setStyleSheet(
                "QDialog {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3e3f44, stop:0.5 #343539, stop:1 #2b2c30);"
                "   color: #ffffff;"
                "}"
                "QTabWidget::pane {"
                "   border: 1px solid rgba(255, 255, 255, 0.12);"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(52, 53, 58, 0.85), stop:1 rgba(38, 39, 43, 0.90));"
                "   border-radius: 14px;"
                "   padding: 12px;"
                "}"
                "QTabBar {"
                "   background-color: rgba(36, 37, 41, 0.85);"
                "   border: 1px solid rgba(255, 255, 255, 0.10);"
                "   border-radius: 12px;"
                "   padding: 3px;"
                "}"
                "QTabBar::tab {"
                "   background: transparent;"
                "   color: #d1d5db;"
                "   border: none;"
                "   border-radius: 9px;"
                "   padding: 7px 16px;"
                "   font-weight: 500;"
                "   font-size: 12px;"
                "   margin: 1px;"
                "}"
                "QTabBar::tab:selected {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e);"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.25);"
                "   font-weight: 600;"
                "}"
                "QPushButton {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e);"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.20);"
                "   border-radius: 10px;"
                "   padding: 6px 16px;"
                "   font-weight: 500;"
                "   font-size: 12px;"
                "}"
                "QPushButton:hover {"
                "   border-color: rgba(255, 255, 255, 0.35);"
                "   background-color: #63646c;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #3c3d43;"
                "}"
                "QPushButton:default {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #65666e, stop:1 #4e4f55);"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.30);"
                "}"
                "QComboBox {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e);"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.22);"
                "   border-radius: 10px;"
                "   padding: 6px 30px 6px 12px;"
                "   font-size: 12px;"
                "   min-height: 24px;"
                "}"
                "QComboBox:hover {"
                "   border-color: rgba(255, 255, 255, 0.40);"
                "   background-color: #63646c;"
                "}"
                "QComboBox::drop-down {"
                "   border: none;"
                "   subcontrol-origin: padding;"
                "   subcontrol-position: top right;"
                "   width: 24px;"
                "}"
                "QComboBox::down-arrow {"
                "   image: url(:/icons/chevron_down_light.svg);"
                "   width: 12px;"
                "   height: 12px;"
                "   margin-right: 6px;"
                "}"
                "QComboBox QAbstractItemView {"
                "   background-color: #2a2b2e;"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.20);"
                "   border-radius: 8px;"
                "   selection-background-color: #585960;"
                "   selection-color: #ffffff;"
                "   padding: 4px;"
                "   outline: none;"
                "}"
                "QComboBox QAbstractItemView::item {"
                "   min-height: 24px;"
                "   padding: 5px 10px;"
                "   border-radius: 6px;"
                "   color: #ffffff;"
                "   background-color: transparent;"
                "}"
                "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
                "   background-color: #585960;"
                "   color: #ffffff;"
                "}"
                "QLineEdit, QSpinBox, QDoubleSpinBox {"
                "   background-color: #2a2b2e;"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.15);"
                "   border-radius: 10px;"
                "   padding: 5px 10px;"
                "}\n" + scrollBarStyleSheet(preset) + fileDialogStyleSheet(preset)
            );
        }

        if (window) {
            window->setStyleSheet(
                "QMainWindow {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3e3f44, stop:0.5 #343539, stop:1 #2b2c30);"
                "}"
                "QMenuBar {"
                "   background: transparent;"
                "   color: #e5e7eb;"
                "   border: none;"
                "   font-size: 12px;"
                "}"
                "QMenuBar::item {"
                "   padding: 4px 10px;"
                "   background: transparent;"
                "}"
                "QMenuBar::item:selected {"
                "   background-color: rgba(255, 255, 255, 0.12);"
                "   color: #ffffff;"
                "   border-radius: 8px;"
                "}"
                "QMenu {"
                "   background-color: #343539;"
                "   color: #ffffff;"
                "   border: 1px solid rgba(255, 255, 255, 0.15);"
                "   padding: 6px;"
                "   border-radius: 12px;"
                "}"
                "QMenu::item {"
                "   padding: 7px 22px;"
                "   border-radius: 8px;"
                "}"
                "QMenu::item:selected {"
                "   background-color: #585960;"
                "   color: #ffffff;"
                "}"
                "QToolBar {"
                "   background: transparent;"
                "   border: none;"
                "   spacing: 4px;"
                "   padding: 3px 8px;"
                "}"
                "QToolButton {"
                "   background: transparent;"
                "   color: #e5e7eb;"
                "   border-radius: 8px;"
                "   padding: 4px 10px;"
                "   font-size: 12px;"
                "}"
                "QToolButton:hover {"
                "   background-color: rgba(255, 255, 255, 0.10);"
                "   color: #ffffff;"
                "}"
                "QStatusBar {"
                "   background: transparent;"
                "   color: #9ca3af;"
                "   border: none;"
                "   font-size: 11px;"
                "}\n" + scrollBarStyleSheet(preset)
            );
        }
    }
}

QString ThemeManager::scrollBarStyleSheet(ThemePreset preset)
{
    preset = effectivePreset(preset);
    if (preset == ThemePreset::ModernLight) {
        // High-contrast Modern Light scrollbars: frosted satin track with smooth gray pill handle
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #e2e5ea;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #94a3b8;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #64748b;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #475569;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #e2e5ea;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #94a3b8;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #64748b;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #475569;"
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
    } else if (preset == ThemePreset::ZincGray) {
        // High-contrast ZincGray scrollbars: neutral dark zinc track with lighter neutral handle
        return QStringLiteral(
            "QScrollBar:vertical {"
            "   background: #2b2c30;"
            "   width: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #63646c;"
            "   min-height: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #787982;"
            "}"
            "QScrollBar::handle:vertical:pressed {"
            "   background: #8d8e98;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "   background: none;"
            "}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "   background: none;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #2b2c30;"
            "   height: 12px;"
            "   margin: 0px;"
            "   border: none;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #63646c;"
            "   min-width: 28px;"
            "   border-radius: 4px;"
            "   margin: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #787982;"
            "}"
            "QScrollBar::handle:horizontal:pressed {"
            "   background: #8d8e98;"
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
    preset = effectivePreset(preset);
    const bool dark = isDark(preset);
    if (dark) {
        const bool isZinc = (preset == ThemePreset::ZincGray);
        const QString accent = isZinc ? QStringLiteral("#585960")
                                      : ((preset == ThemePreset::DarkSlate) ? QStringLiteral("#0284c7") : QStringLiteral("#3daee9"));
        const QString accentHover = isZinc ? QStringLiteral("#6b6c74")
                                           : ((preset == ThemePreset::DarkSlate) ? QStringLiteral("#0369a1") : QStringLiteral("#2980b9"));
        const QString bg = isZinc ? QStringLiteral("#343539")
                                  : ((preset == ThemePreset::DarkSlate) ? QStringLiteral("#22262b") : QStringLiteral("#181b1d"));
        const QString border = isZinc ? QStringLiteral("rgba(255, 255, 255, 0.14)")
                                      : ((preset == ThemePreset::DarkSlate) ? QStringLiteral("#2d333b") : QStringLiteral("#282e34"));

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

    // Reuse a single static icon provider instance (QFileDialog does not take ownership)
    static BubezFileIconProvider s_iconProvider;
    dialog->setIconProvider(&s_iconProvider);

    // 1. Configure Places Sidebar with canonical deduplication
    QList<QUrl> sidebarUrls;
    QStringList canonicalAdded;

    auto addPath = [&](const QString &rawPath) {
        if (rawPath.isEmpty()) return;
        QDir dir(rawPath);
        if (!dir.exists()) return;
        const QString canonical = dir.canonicalPath();
        if (canonicalAdded.contains(canonical)) {
            return; // Prevent duplicate entries (e.g. /home/user and /run/host/home/user)
        }
        canonicalAdded.append(canonical);
        sidebarUrls << QUrl::fromLocalFile(rawPath);
    };

    const QString home = QDir::homePath();
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

