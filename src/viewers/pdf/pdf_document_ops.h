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
#include <QColor>
#include <memory>
#include <vector>

namespace Poppler {
    class Document;
    class Page;
}
class QWidget;

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
                               QString &errorMessage);

    /**
     * @brief Deletes a specific 0-based page index from source PDF using QPDF.
     * @param sourceFilePath Path to the source PDF.
     * @param totalPages Document page count.
     * @param pageIndex 0-based index of the page to delete.
     * @param outFilePath Output destination.
     * @param errorMessage Output error string if the operation fails.
     * @return True on success; false on failure.
     */
    static bool deletePage(const QString &sourceFilePath,
                           int totalPages,
                           int pageIndex,
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
     * @brief Extracts a single 0-based page index to an independent single-page PDF document.
     * @param sourceFilePath Path to source multi-page PDF.
     * @param pageIndex0Based 0-based index of page to extract.
     * @param outFilePath Output file path.
     * @param errorMessage Detailed failure reason on error.
     * @return True on success; false on failure.
     */
    static bool extractSinglePageToPdf(const QString &sourceFilePath,
                                      int pageIndex0Based,
                                      const QString &outFilePath,
                                      QString &errorMessage);

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

    /**
     * @brief Individual shape or stamp raster item to permanently bake into a PDF.
     */
    struct ImageStampItem {
        int pageIndex = 0;
        QImage image;
        QRectF normalizedRect; // 0.0 to 1.0 relative to visible page rect
    };

    /**
     * @brief Writes and bakes shape and stamp raster images permanently into a PDF document using PyMuPDF (fitz).
     * @param sourceFilePath Path to the source PDF.
     * @param items List of image stamps with their page index and visible normalized bounding boxes.
     * @param outFilePath Output modified PDF path on success.
     * @param errorMessage Detailed failure reason on error.
     * @return True on success; false on failure.
     */
    static bool applyStampsWithMuPdf(const QString &sourceFilePath,
                                     const QList<ImageStampItem> &items,
                                     QString &outFilePath,
                                     QString &errorMessage);

    /**
     * @brief Removes vector text glyphs from a specific page within pointRect without touching vector graphics or images.
     * @param sourceFilePath Path to the source PDF.
     * @param pageIndex 0-based page index.
     * @param pointRect Bounding rectangle in 72 DPI page point coordinates.
     * @param outFilePath Output modified PDF path on success.
     * @param errorMessage Output error string if the operation fails.
     * @return True on success; false on failure.
     */
    static bool removeVectorTextFromPage(const QString &sourceFilePath,
                                        int pageIndex,
                                        const QRectF &pointRect,
                                        QString &outFilePath,
                                        QString &errorMessage);

    /**
     * @brief Detailed text formatting attributes extracted directly from vector font streams.
     */
    struct TextFormatInfo {
        bool valid = false;
        QString fontName;
        QString family;
        double pointSize = 12.0;
        QColor color = Qt::black;
        bool isBold = false;
        bool isItalic = false;
        double ascender = 0.0;
        double descender = 0.0;
        bool hasMultipleStyles = false;
    };

    /**
     * @brief Inspects native vector text format (font, size, color, weight) within a bounding box on a PDF page.
     * @param sourceFilePath Path to the source PDF.
     * @param pageIndex 0-based page index.
     * @param pointRect Bounding rectangle in 72 DPI page point coordinates.
     * @param outFormat Output text format metadata on success.
     * @param errorMessage Detailed failure reason on error.
     * @return True on success; false on failure.
     */
    static bool inspectTextFormat(const QString &sourceFilePath,
                                  int pageIndex,
                                  const QRectF &pointRect,
                                  TextFormatInfo &outFormat,
                                  QString &errorMessage);

    /**
     * @brief Inspects native vector text format for the first occurrence of searchText on a PDF page.
     * @param sourceFilePath Path to the source PDF.
     * @param pageIndex 0-based page index.
     * @param searchText Text snippet to locate on page.
     * @param outFormat Output text format metadata on success.
     * @param errorMessage Detailed failure reason on error.
     * @return True on success; false on failure.
     */
    static bool inspectTextFormatByText(const QString &sourceFilePath,
                                        int pageIndex,
                                        const QString &searchText,
                                        TextFormatInfo &outFormat,
                                        QString &errorMessage);

    /**
     * @brief Normalizes a raw PDF font name to a clean system-available family name.
     */
    static QString resolveFontFamily(const QString &rawFontName, bool &outBold, bool &outItalic);

    /**
     * @brief Displays a standardized, rich document properties and metadata dialog.
     */
    static void showDocumentInfoDialog(QWidget *parent,
                                       Poppler::Document *doc,
                                       const QString &filePath,
                                       int currentPage,
                                       int pageCount);
};
