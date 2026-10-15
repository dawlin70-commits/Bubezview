/**
 * @file pdf_plugin.cpp
 * @brief Implementation of PdfPlugin creating native PdfViewer instances.
 */

#include "pdf_plugin.h"
#include "viewers/pdf/pdf_viewer.h"

PdfPlugin::PdfPlugin(QObject *parent)
    : QObject(parent)
{
}

IViewerWidget *PdfPlugin::createViewer(QWidget *parent)
{
    return new PdfViewer(parent);
}
