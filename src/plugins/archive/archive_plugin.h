/**
 * @file archive_plugin.h
 * @brief Qt plugin interface implementation for the LibArchive viewer engine.
 */

#pragma once

#include <QObject>
#include "core/interfaces/viewer_plugin_interface.h"

/**
 * @class ArchivePlugin
 * @brief Engine plugin exposing LibArchive compressed archive browsing to BubezView.
 */
class ArchivePlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "archive_plugin.json")
    Q_INTERFACES(ViewerPluginInterface)

public:
    explicit ArchivePlugin(QObject *parent = nullptr);
    ~ArchivePlugin() override = default;

    QString pluginId() const override { return QStringLiteral("archive"); }
    QString displayName() const override { return tr("LibArchive Compressed Archive Engine"); }
    QString description() const override {
        return tr("Browsing, integrity testing, and previewing ZIP, TAR, 7Z, and ISO archives.");
    }
    FileCategory category() const override { return FileCategory::Archive; }
    QStringList supportedExtensions() const override {
        return {
            QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"),
            QStringLiteral("tgz"), QStringLiteral("bz2"), QStringLiteral("tbz2"),
            QStringLiteral("xz"), QStringLiteral("txz"), QStringLiteral("zst"),
            QStringLiteral("tzst"), QStringLiteral("7z"),
            QStringLiteral("iso"), QStringLiteral("deb"), QStringLiteral("rpm"),
            QStringLiteral("cpio")
        };
    }

    IViewerWidget *createViewer(QWidget *parent = nullptr) override;
};
