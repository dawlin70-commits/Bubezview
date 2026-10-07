/**
 * @file pdf_document_ops.h
 * @brief Modular backend operations for PDF documents: QPDF stream manipulations,
 * blank page insertion, visual content push-down analysis, image extraction, and page export.
 */

#pragma once

#include <QString>
#include <QImage>
#include <QRectF>
#include <QSizeF>
#include <memory>
#include <vector>

namespace Poppler {
    class Document;
    class Page;
}
/**
 * @brief High-performance utility class encapsulating filesystem and QPDF stream modifications.
 */
class PdfDocumentOps {
public:
    /**
     * @brief Generates an empty A4 page and inserts it after targetIndex using QPdfWriter and QPDF.
     * @param sourceFilePath Path to the source PDF file.
     * @param totalPages Total page count of the document.
     * @param targetIndex 0-based page index after which the blank page should be inserted.
     * @param outFilePath Path where the modified PDF file was written on success.
     * @param errorMessage Output error string if the operation fails.
     * @return True if page insertion succeeded; false on failure.
     */
    static bool insertBlankPage(const QString &sourceFilePath,
                               int totalPages,
                               int targetIndex,
                               QString &outFilePath,
                               QString &errorMessage);    /**
     * @brief Exports a single Poppler page to text or image formats (png, jpg, webp, bmp, tiff).
     * @param page Poppler page to export.
     * @param format Destination format string ("png", "jpg", "txt", etc.).
     * @param outputPath Target file path on disk.
     * @param dpi Target rasterization resolution (e.g. 150 or 300 DPI).
     * @param rotationDegrees Viewport rotation degrees (0, 90, 180, 270).
     * @return True on success; false on failure.
     */
    static bool exportPage(Poppler::Page *page,
                          const QString &format,
                          const QString &outputPath,
                          int dpi,
                          int rotationDegrees);

    /**
     * @brief Extracts embedded raster images using the system's pdfimages utility.
     * @param filePath Path to the PDF file.
     * @param targetDir Destination folder for extracted images.
     * @return Number of extracted images, or -1 on error.
     */
    static int extractEmbeddedImages(const QString &filePath, const QString &targetDir);

    /**
     * @brief Saves document changes to disk, baking viewport rotation if necessary.
     * @param doc Active Poppler document.
     * @param currentFilePath Active document path on disk.
     * @param targetFilePath Destination file path on disk.
     * @param rotationDegrees Rotation to permanently bake into page dictionaries.
     * @return True on success; false on failure.
     */
    static bool saveDocumentWithRotation(Poppler::Document *doc,
                                        const QString &currentFilePath,
                                        const QString &targetFilePath,
                                        int &rotationDegrees);
};
