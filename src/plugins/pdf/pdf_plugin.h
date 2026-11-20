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

    /** @copydoc ViewerPluginInterface::pluginId */
    QString pluginId() const override { return QStringLiteral("pdf"); }

    /** @copydoc ViewerPluginInterface::displayName */
    QString displayName() const override { return tr("Poppler Qt6 PDF Document Engine"); }

    /** @copydoc ViewerPluginInterface::description */
    QString description() const override {
        return tr("High-resolution vector rendering, annotations, and text extraction for PDF documents.");
    }

    /** @copydoc ViewerPluginInterface::category */
    FileCategory category() const override { return FileCategory::Pdf; }

    /** @copydoc ViewerPluginInterface::supportedExtensions */
    QStringList supportedExtensions() const override {
        return {QStringLiteral("pdf"), QStringLiteral("ai")};
    }

    /** @copydoc ViewerPluginInterface::createViewer */
    IViewerWidget *createViewer(QWidget *parent = nullptr) override;
};
