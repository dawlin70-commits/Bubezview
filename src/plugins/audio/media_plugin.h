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

    /** @copydoc ViewerPluginInterface::pluginId */
    QString pluginId() const override { return QStringLiteral("audio"); }

    /** @copydoc ViewerPluginInterface::displayName */
    QString displayName() const override { return tr("Audio Studio (MPV)"); }

    /** @copydoc ViewerPluginInterface::description */
    QString description() const override {
        return tr("High-fidelity 64-bit audio studio: waveform cutter, filters, pitch & channels, equalizer, and tag editor.");
    }

    /** @copydoc ViewerPluginInterface::category */
    FileCategory category() const override { return FileCategory::Media; }

    /** @copydoc ViewerPluginInterface::supportedExtensions */
    QStringList supportedExtensions() const override {
        return {
            // Video Formats
            QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
            QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
            QStringLiteral("wmv"), QStringLiteral("m4v"), QStringLiteral("3gp"),
            QStringLiteral("3g2"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
            QStringLiteral("m2v"), QStringLiteral("ts"), QStringLiteral("m2ts"),
            QStringLiteral("mts"), QStringLiteral("m2t"), QStringLiteral("vob"),
            QStringLiteral("ogv"), QStringLiteral("rm"), QStringLiteral("rmvb"),
            QStringLiteral("asf"), QStringLiteral("divx"), QStringLiteral("f4v"),
            QStringLiteral("wtv"), QStringLiteral("dvr-ms"), QStringLiteral("mxf"),
            QStringLiteral("dv"), QStringLiteral("h264"), QStringLiteral("h265"),
            QStringLiteral("264"), QStringLiteral("265"), QStringLiteral("hevc"),
            QStringLiteral("avc"), QStringLiteral("av1"), QStringLiteral("y4m"),
            QStringLiteral("nut"), QStringLiteral("bik"),
            // Audio Formats
            QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
            QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("m4a"),
            QStringLiteral("aac"), QStringLiteral("wma"), QStringLiteral("opus"),
            QStringLiteral("alac"), QStringLiteral("aiff"), QStringLiteral("aif"),
            QStringLiteral("aifc"), QStringLiteral("dsf"), QStringLiteral("dff"),
            QStringLiteral("dsd"), QStringLiteral("ape"), QStringLiteral("mac"),
            QStringLiteral("wv"), QStringLiteral("tta"), QStringLiteral("tak"),
            QStringLiteral("mpc"), QStringLiteral("mp+"), QStringLiteral("mpp"),
            QStringLiteral("caf"), QStringLiteral("amr"), QStringLiteral("awb"),
            QStringLiteral("voc"), QStringLiteral("qcp"), QStringLiteral("ra"),
            QStringLiteral("ram"), QStringLiteral("mod"), QStringLiteral("xm"),
            QStringLiteral("it"), QStringLiteral("s3m"), QStringLiteral("med"),
            QStringLiteral("okt"), QStringLiteral("669"), QStringLiteral("mtm"),
            QStringLiteral("mid"), QStringLiteral("midi"), QStringLiteral("ac3"),
            QStringLiteral("eac3"), QStringLiteral("dts"), QStringLiteral("dtshd"),
            QStringLiteral("thd"), QStringLiteral("mka")
        };
    }

    /** @copydoc ViewerPluginInterface::createViewer */
    IViewerWidget *createViewer(QWidget *parent = nullptr) override;
};
