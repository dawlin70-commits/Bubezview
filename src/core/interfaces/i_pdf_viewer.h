/**
 * @file i_pdf_viewer.h
 * @brief Abstract interface for the PDF Document Viewer engine in BubezView.
 *
 * Exposes page navigation, annotations, table of contents, OCR rendering,
 * and vector transformations without coupling the host to Poppler C++ headers.
 */

#pragma once

#include "i_viewer_widget.h"
#include <QColor>
#include <QImage>
#include <functional>
#include <utility>

class QWidget;

/**
 * @class IPdfViewer
 * @brief Abstract interface implemented by the PDF viewer plugin.
 */
class IPdfViewer : public IViewerWidget {
public:
    virtual ~IPdfViewer() = default;

    /**
     * @brief Total number of pages in the current PDF document.
     * @return Integer page count (0 if no document loaded).
     */
    virtual int pageCount() const = 0;

    /**
     * @brief Zero-based index of current active page.
     * @return Current page index.
     */
    virtual int currentPage() const = 0;

    /**
     * @brief Navigates to the next page.
     */
    virtual void nextPage() = 0;

    /**
     * @brief Navigates to the previous page.
     */
    virtual void prevPage() = 0;

    /**
     * @brief Jumps directly to a specific zero-based page index.
     * @param page Target page index.
     */
    virtual void goToPage(int page) = 0;

    /**
     * @brief Fits document page zoom to the viewport width.
     */
    virtual void fitToWidth() = 0;

    /**
     * @brief Zooms in document view.
     */
    virtual void zoomIn() {}

    /**
     * @brief Zooms out document view.
     */
    virtual void zoomOut() {}

    /**
     * @brief Resets zoom to 100%.
     */
    virtual void resetZoom() {}

    /**
     * @brief Current zoom factor (1.0 = 100%).
     */
    virtual double zoomFactor() const { return 1.0; }

    /**
     * @brief Toggles interactive marquee zoom tool on canvas.
     * @param active True to enable zoom tool.
     */
    virtual void toggleZoomTool(bool active) = 0;

    /**
     * @brief Checks if interactive marquee zoom tool is active.
     * @return True if active.
     */
    virtual bool isZoomToolActive() const = 0;

    /**
     * @brief Shows or hides the Table of Contents / Bookmarks sidebar.
     */
    virtual void toggleTocSidebar() = 0;

    /**
     * @brief Checks if Table of Contents sidebar is currently visible.
     * @return True if visible.
     */
    virtual bool isTocVisible() const = 0;

    /**
     * @brief Checks if loaded PDF has an outline or bookmark tree.
     * @return True if outline exists.
     */
    virtual bool hasOutline() const = 0;

    /**
     * @brief Extracts all selectable text from entire document.
     * @return Full text string.
     */
    virtual QString extractAllText() const = 0;

    /**
     * @brief Renders a specific page to QImage at target DPI for OCR text recognition.
     * @param page Zero-based page index.
     * @param dpi Target resolution DPI (e.g. 150 or 300).
     * @return Rendered QImage bitmap.
     */
    virtual QImage renderPageForOcr(int page, int dpi) const = 0;

    /**
     * @brief Renders current page to QImage at target DPI for OCR text recognition.
     * @param dpi Target resolution DPI.
     * @return Rendered QImage bitmap.
     */
    virtual QImage renderCurrentPageForOcr(int dpi) const = 0;

    /**
     * @brief Checks if user has an active text selection.
     * @return True if text is selected.
     */
    virtual bool hasTextSelection() const = 0;

    /**
     * @brief Checks if user has an active marquee rectangular area selection.
     * @return True if area is selected.
     */
    virtual bool hasAreaSelection() const = 0;

    /**
     * @brief Copies currently selected text or image area to system clipboard.
     */
    virtual void copySelectedText() = 0;

    /**
     * @brief Cuts currently selected text or annotation.
     */
    virtual void cutSelectedText() = 0;

    /**
     * @brief Pastes text from clipboard onto active page.
     */
    virtual void pasteText() { paste(); }

    /**
     * @brief Opens Typewriter / Text Annotation creation dialog.
     */
    virtual void openTypewriterDialog() = 0;

    /**
     * @brief Initiates interactive redaction / black-out box tool.
     */
    virtual void startRedaction() = 0;

    /**
     * @brief Opens interactive Watermark configuration dialog.
     */
    virtual void openWatermarkDialog() = 0;

    /**
     * @brief Opens dialog to insert external image onto page canvas.
     */
    virtual void openInsertImageDialog() = 0;

    /**
     * @brief Opens digital or handwritten signature drawing dialog.
     */
    virtual void openSignatureDialog() = 0;

    /**
     * @brief Opens cryptographic PAdES/CAdES digital signature verification and signing dialog.
     * @param defaultTab 0 for Verification, 1 for Signing.
     */
    virtual void openCryptoSignatureDialog(int defaultTab = 0) { Q_UNUSED(defaultTab); }

    /**
     * @brief Inserts a new blank page into current document.
     * @param afterPageIndex Index after which to insert (-1 for current page).
     * @return True if page inserted successfully.
     */
    virtual bool insertBlankPage(int afterPageIndex = -1) = 0;

    /**
     * @brief Starts interactive rectangular area selection tool.
     */
    virtual void startSelectArea() = 0;

    /**
     * @brief Sets or resets modified state of document.
     * @param mod True if modified.
     */
    virtual void setModified(bool mod) = 0;

    /**
     * @brief Starts the highlighter tool with the given color and opacity.
     * @param color Highlight color (default translucent yellow).
     * @param opacity Opacity factor between 0.0 and 1.0 (default 0.40).
     */
    virtual void startHighlighter(const QColor &color = QColor(255, 235, 59), double opacity = 0.40) {
        Q_UNUSED(color); Q_UNUSED(opacity);
    }

    /**
     * @brief Starts interactive geometric shape drawing tool.
     * @param shapeType Identifier for the shape type (rectangle, ellipse, line, arrow).
     * @param strokeWidth Stroke line thickness in points.
     * @param strokeColor Stroke contour color.
     * @param fillColor Fill background color.
     */
    virtual void startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor) {
        Q_UNUSED(shapeType); Q_UNUSED(strokeWidth); Q_UNUSED(strokeColor); Q_UNUSED(fillColor);
    }

    /**
     * @brief Exits persistent/continuous drawing mode for shapes or highlighters.
     */
    virtual void exitContinuousToolMode() {}

    /**
     * @brief Opens shape insertion configuration dialog.
     */
    virtual void openShapeDialog() {}

    /**
     * @brief Initiates interactive snapshot area capture tool.
     */
    virtual void startSnapshot() {}

    /**
     * @brief Registers callback invoked when document modification status changes.
     * @param callback Function receiving boolean modified state.
     */
    virtual void setOnDocumentModified(std::function<void(bool)> callback) {
        m_docModifiedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when current page changes.
     * @param callback Function receiving 0-based page index and total pages.
     */
    virtual void setOnPageChanged(std::function<void(int current, int total)> callback) {
        m_pageChangedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when canvas text or area selection changes.
     * @param callback Function receiving notification.
     */
    virtual void setOnSelectionChanged(std::function<void()> callback) {
        m_selectionChangedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when undo/redo command history changes.
     * @param callback Function receiving notification.
     */
    virtual void setOnHistoryChanged(std::function<void()> callback) {
        m_historyChangedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when continuous tool drawing mode is exited.
     * @param callback Function receiving notification.
     */
    virtual void setOnContinuousModeExited(std::function<void()> callback) {
        m_continuousModeExitedCallback = std::move(callback);
    }

    /**
     * @brief Opens interactive PDF page organizer dialog for page reordering and rotation.
     * @param target File path of the PDF to organize.
     * @param parentWidget Parent widget for dialog hierarchy.
     */
    virtual void openPageOrganizer(const QString &target, QWidget *parentWidget = nullptr) {
        Q_UNUSED(target); Q_UNUSED(parentWidget);
    }

    /**
     * @brief Registers callback invoked when PDF pages are organized and saved.
     * @param callback Function receiving path of organized PDF.
     */
    virtual void setOnFileOrganized(std::function<void(const QString &)> callback) {
        m_fileOrganizedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when document zoom level changes.
     * @param callback Function receiving new zoom factor.
     */
    virtual void setOnZoomChanged(std::function<void(double)> callback) {
        m_zoomChangedCallback = std::move(callback);
    }

protected:
    std::function<void(bool)> m_docModifiedCallback;
    std::function<void(int, int)> m_pageChangedCallback;
    std::function<void()> m_selectionChangedCallback;
    std::function<void()> m_historyChangedCallback;
    std::function<void()> m_continuousModeExitedCallback;
    std::function<void(const QString &)> m_fileOrganizedCallback;
    std::function<void(double)> m_zoomChangedCallback;
};
