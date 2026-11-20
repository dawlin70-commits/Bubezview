#include <QApplication>
#include <QVBoxLayout>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QTranslator>
#include "folder_viewer.h"
#include "minimal_header_bar.h"
#include "theme_manager.h"
#include "plugin_manager.h"

static void pumpEvents(int ms = 120)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTranslator translator;
    if (translator.load(QStringLiteral(":/translations/bubezview_it.qm"))) {
        app.installTranslator(&translator);
    }
    const QString outDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/de42cb8b-5fce-4765-a706-7a5140fa0405/scratch");
    QDir().mkpath(outDir);

    PluginManager::instance().discoverPlugins();

    QWidget host;
    host.resize(1280, 800);
    auto *lay = new QVBoxLayout(&host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *header = new MinimalHeaderBar(&host);
    auto *fv = new FolderViewer(&host);
    lay->addWidget(header);
    lay->addWidget(fv, 1);

    header->setCategory(FileCategory::Folder);
    header->setTitle(QStringLiteral("appunti"));
    fv->loadFolder(QStringLiteral("/home/davide/Progetti/BubezView/appunti"), true);

    QObject::connect(header, &MinimalHeaderBar::folderStudioToolRequested, fv, &FolderViewer::setStudioTool);

    host.show();
    pumpEvents(250);

    // 1. FolderViewer Normal Mode across themes
    auto applyAllThemes = [&](ThemePreset p) {
        ThemeManager::setPreset(p);
        header->applyThemePreset(p);
        fv->updateTheme();
        pumpEvents(100);
    };

    applyAllThemes(ThemePreset::DarkSlate);
    host.grab().save(outDir + QStringLiteral("/folder_normal_dark.png"));

    applyAllThemes(ThemePreset::ModernLight);
    host.grab().save(outDir + QStringLiteral("/folder_normal_light.png"));

    applyAllThemes(ThemePreset::ZincGray);
    host.grab().save(outDir + QStringLiteral("/folder_normal_zinc.png"));

    // 2. Folder Studio Edit Mode (bubez_folder.so)
    applyAllThemes(ThemePreset::DarkSlate);
    header->setEditMode(true);
    fv->setEditMode(true);
    pumpEvents(150);
    host.grab().save(outDir + QStringLiteral("/folder_studio_duplicates_dark.png"));

    fv->setStudioTool(1);
    header->setActiveFolderStudioTool(1);
    pumpEvents(100);
    host.grab().save(outDir + QStringLiteral("/folder_studio_recovery_dark.png"));

    fv->setStudioTool(2);
    header->setActiveFolderStudioTool(2);
    pumpEvents(100);
    host.grab().save(outDir + QStringLiteral("/folder_studio_organize_dark.png"));

    fv->setStudioTool(3);
    header->setActiveFolderStudioTool(3);
    pumpEvents(100);
    host.grab().save(outDir + QStringLiteral("/folder_studio_batch_dark.png"));

    applyAllThemes(ThemePreset::ModernLight);
    host.grab().save(outDir + QStringLiteral("/folder_studio_light.png"));

    return 0;
}
