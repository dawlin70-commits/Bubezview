#include <QApplication>
#include <QDebug>
#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <iostream>

#define private public
#define protected public
#include "viewers/pdf/pdf_page_widget.h"
#include "viewers/pdf/pdf_viewer.h"
#include "viewers/pdf/pdf_transform_box.h"
#include "viewers/pdf/pdf_document_ops.h"

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    PdfViewer viewer;
    viewer.resize(1000, 800);
    bool ok = viewer.loadFile("/home/davide/Progetti/BubezView/421_Guida turistica di Trieste.pdf");
    if (!ok) {
        std::cerr << "Failed to load Trieste\n";
        return 1;
    }
    viewer.setEditMode(true);

    auto pageWidget = viewer.m_pageWidget;
    std::cout << "Initial page count: " << viewer.m_pageCount << " activePage: " << pageWidget->m_activePageIndex << "\n";
    std::cout << "TextBoxes count: " << pageWidget->m_textBoxes.size() << "\n";
    for (size_t i = 0; i < pageWidget->m_textBoxes.size(); ++i) {
        std::cout << "Box " << i << ": " << pageWidget->m_textBoxes[i]->text().toStdString() 
                  << " rect: " << pageWidget->m_textBoxes[i]->boundingBox().x() << "," << pageWidget->m_textBoxes[i]->boundingBox().y()
                  << " " << pageWidget->m_textBoxes[i]->boundingBox().width() << "x" << pageWidget->m_textBoxes[i]->boundingBox().height() << "\n";
    }

    // --- MOVE 1: Move "Trieste" ---
    std::cout << "\n--- STARTING MOVE 1 ---\n";
    pageWidget->m_selectedIndices = {3}; // "Trieste"
    pageWidget->moveSelectedText();

    std::cout << "TransformBox visible: " << pageWidget->m_transformBox->isVisible() 
              << " isPlacement: " << pageWidget->m_transformBox->isPlacementMode()
              << " text: " << pageWidget->m_transformBox->text().toStdString() << "\n";

    // Move box to a new position
    QRect r = pageWidget->m_transformBox->m_boxRect;
    r.moveTopLeft(r.topLeft() + QPoint(50, 50));
    pageWidget->m_transformBox->m_boxRect = r;

    // Apply Move 1
    pageWidget->onTransformBoxApplied();
    std::cout << "Move 1 applied!\n";
    std::cout << "SnapshotPath: " << viewer.m_activeSnapshotPath.toStdString() << "\n";
    std::cout << "New TextBoxes count: " << pageWidget->m_textBoxes.size() << "\n";
    for (size_t i = 0; i < pageWidget->m_textBoxes.size(); ++i) {
        std::cout << "Box " << i << ": " << pageWidget->m_textBoxes[i]->text().toStdString() << "\n";
    }

    // --- MOVE 2: Move "Le" (Box 0) ---
    std::cout << "\n--- STARTING MOVE 2 ---\n";
    if (pageWidget->m_textBoxes.empty()) {
        std::cerr << "Error: No text boxes after Move 1!\n";
        return 1;
    }

    pageWidget->m_selectedIndices = {0}; // "Le"
    pageWidget->moveSelectedText();

    std::cout << "TransformBox visible: " << pageWidget->m_transformBox->isVisible() 
              << " isPlacement: " << pageWidget->m_transformBox->isPlacementMode()
              << " text: " << pageWidget->m_transformBox->text().toStdString() << "\n";

    r = pageWidget->m_transformBox->m_boxRect;
    r.moveTopLeft(r.topLeft() + QPoint(40, 40));
    pageWidget->m_transformBox->m_boxRect = r;

    // Apply Move 2
    pageWidget->onTransformBoxApplied();
    std::cout << "Move 2 applied!\n";
    std::cout << "SnapshotPath: " << viewer.m_activeSnapshotPath.toStdString() << "\n";
    std::cout << "New TextBoxes count: " << pageWidget->m_textBoxes.size() << "\n";
    for (size_t i = 0; i < pageWidget->m_textBoxes.size(); ++i) {
        std::cout << "Box " << i << ": " << pageWidget->m_textBoxes[i]->text().toStdString() << "\n";
    }

    return 0;
}
