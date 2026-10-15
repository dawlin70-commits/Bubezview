/**
 * @file media_plugin.h
 * @brief Qt plugin interface implementation for the MPV media player engine.
 */

#pragma once

#include <QObject>
#include "core/interfaces/viewer_plugin_interface.h"

/**
 * @class MediaPlugin
 * @brief Engine plugin exposing MPV hardware-accelerated audio/video playback to BubezView.
 */
class MediaPlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "media_plugin.json")
    Q_INTERFACES(ViewerPluginInterface)

public:
    explicit MediaPlugin(QObject *parent = nullptr);
    ~MediaPlugin() override = default;

    QString pluginId() const override { return QStringLiteral("media"); }
    QString displayName() const override { return tr("MPV Audio & Video Engine"); }
    QString description() const override {
        return tr("Hardware-accelerated video rendering and high-fidelity audio playback.");
    }
    FileCategory category() const override { return FileCategory::Media; }
    QStringList supportedExtensions() const override {
        return {
            QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
            QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
            QStringLiteral("wmv"), QStringLiteral("m4v"), QStringLiteral("3gp"),
            QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
            QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("aac"),
            QStringLiteral("wma"), QStringLiteral("opus")
        };
    }

    IViewerWidget *createViewer(QWidget *parent = nullptr) override;
};
