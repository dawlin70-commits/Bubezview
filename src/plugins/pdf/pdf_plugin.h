/**
 * @file pdf_plugin.h
 * @brief Qt plugin interface implementation for the Poppler PDF viewer engine.
 */

#pragma once

#include <QObject>
#include "core/interfaces/viewer_plugin_interface.h"

/**
 * @class PdfPlugin
 * @brief Engine plugin exposing Poppler-based PDF viewing capabilities to BubezView.
 */
class PdfPlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "pdf_plugin.json")
    Q_INTERFACES(ViewerPluginInterface)

public:
    explicit PdfPlugin(QObject *parent = nullptr);
    ~PdfPlugin() override = default;

    QString pluginId() const override { return QStringLiteral("pdf"); }
    QString displayName() const override { return tr("Poppler Qt6 PDF Document Engine"); }
    QString description() const override {
        return tr("High-resolution vector rendering, annotations, and text extraction for PDF documents.");
    }
    FileCategory category() const override { return FileCategory::Pdf; }
    QStringList supportedExtensions() const override {
        return {QStringLiteral("pdf"), QStringLiteral("ai")};
    }

    IViewerWidget *createViewer(QWidget *parent = nullptr) override;
};
