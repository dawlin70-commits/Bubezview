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
#include <QPixmap>
#include <QDateTime>
#include <QList>
#include <functional>
#include <utility>

class QWidget;

/**
 * @enum PdfTextBgMode
 * @brief Strategy for handling background coverage behind edited PDF text.
 */
enum class PdfTextBgMode {
    Auto = 0,        ///< Automatically sampled background color from document
    Transparent = 1, ///< No background fill, transparent overlay
    Feathered = 2,   ///< Feathered / soft gradient blend around borders
    Custom = 3       ///< Custom user-chosen solid color
};

/**
 * @struct PdfHistoryStep
 * @brief Represents a discrete state snapshot in the visual PDF modification timeline.
 */
struct PdfHistoryStep {
    int index = 0;
    QString snapshotPath;
    QString description;
    QDateTime timestamp;
    QPixmap thumbnail;
    int pageIndex = 0;
    int scrollX = 0;
    int scrollY = 0;
    bool isCurrent = false;
};

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
     * @brief Fits document page zoom so the entire page fits inside the window viewport (width & height).
     */
    virtual void fitToPage() { fitToWidth(); }

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
     * @brief Sets absolute zoom factor (1.0 = 100%).
     * @param factor Target zoom level.
     */
    virtual void setZoomFactor(double factor) { Q_UNUSED(factor); }

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
     * @brief Returns currently selected text as plain string.
     */
    virtual QString selectedText() const { return QString(); }

    /**
     * @brief Initiates contextual edit / replacement on currently selected text.
     */
    virtual void editSelectedTextWithWhiteout() {}

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
     * @param color Redaction fill color (e.g. Qt::black for blackout, Qt::white for whiteout).
     * @param permanent If true, wipes underlying text and vector stream data (real sanitization).
     */
    virtual void startRedaction(const QColor &color = Qt::black, bool permanent = false) = 0;

    /**
     * @brief Opens interactive Watermark configuration dialog.
     */
    virtual void openWatermarkDialog() = 0;

    /**
     * @brief Activates the Watermark / Rubber Stamp placement tool.
     * @param text Watermark or stamp string (e.g. "APPROVED", "CONFIDENTIAL").
     * @param angle Rotation angle in degrees (e.g. 0.0, -15.0, 45.0).
     * @param opacity Translucency factor (0.1 to 1.0).
     * @param color Stamp font and border color.
     * @param initialPos Center point in widget coordinates.
     * @param style Stamp graphic style (0: ClassicRounded, 1: OfficialDouble, 2: Dotted, 3: PlainText, 4: Solid).
     * @param fontSize Stamp font size in points (default 28).
     */
    virtual void startWatermark(const QString &text = QStringLiteral("COPY"), qreal angle = 45.0,
                               qreal opacity = 0.35, const QColor &color = QColor(QStringLiteral("#DC2626")),
                               const QPoint &initialPos = QPoint(), int style = 0, int fontSize = 28) {
        Q_UNUSED(text); Q_UNUSED(angle); Q_UNUSED(opacity); Q_UNUSED(color); Q_UNUSED(initialPos); Q_UNUSED(style); Q_UNUSED(fontSize);
    }

    /**
     * @brief Updates visual and textual properties of currently selected rubber stamp.
     */
    virtual void updateSelectedStampProperties(const QString &text, const QColor &color, qreal angle, int style, int fontSize) {
        Q_UNUSED(text); Q_UNUSED(color); Q_UNUSED(angle); Q_UNUSED(style); Q_UNUSED(fontSize);
    }

    /**
     * @brief Commits any pending interactive stamp modifications to the PDF document.
     */
    virtual void commitSelectedStampProperties() {}

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
    virtual bool deleteCurrentPage() = 0;

    /**
     * @brief Initiates point-and-click direct typewriter tool on canvas.
     * @param font Desired text font.
     * @param color Text color.
     * @param whiteout Whether to paint an opaque white background under text.
     */
    virtual void startDirectTypewriter(const QFont &font, const QColor &color, bool whiteout) {
        Q_UNUSED(font); Q_UNUSED(color); Q_UNUSED(whiteout);
    }

    /**
     * @brief Updates live typewriter formatting properties for inline editor and subsequent placements.
     * @param font Text font.
     * @param color Text color.
     * @param whiteout Whether whiteout patch is active.
     */
    virtual void updateTypewriterProperties(const QFont &font, const QColor &color, bool whiteout) {
        Q_UNUSED(font); Q_UNUSED(color); Q_UNUSED(whiteout);
    }

    /**
     * @brief Updates live shape drawing properties for active transform box and subsequent shapes.
     * @param strokeWidth Line width in points.
     * @param strokeColor Stroke contour color.
     * @param fillColor Interior fill color.
     */
    virtual void updateShapeProperties(int strokeWidth, const QColor &strokeColor, const QColor &fillColor) {
        Q_UNUSED(strokeWidth); Q_UNUSED(strokeColor); Q_UNUSED(fillColor);
    }



    /**
     * @brief Starts interactive rectangular area selection tool.
     */
    virtual void startSelectArea() = 0;

    /**
     * @brief Saves the current PDF page as an independent single-page PDF document.
     * @param targetPath Destination file path.
     * @param errorMessage Detailed failure reason on error.
     * @return True on success; false on failure.
     */
    virtual bool saveCurrentPageAsPdf(const QString &targetPath, QString *errorMessage = nullptr) {
        Q_UNUSED(targetPath);
        Q_UNUSED(errorMessage);
        return false;
    }

    /**
     * @brief Sets or resets modified state of document.
     * @param mod True if modified.
     */
    virtual void setModified(bool mod) = 0;

    /**
     * @brief Toggles the PDF Studio side panel.
     */
    virtual void toggleStudioPanel() {}

    /**
     * @brief Displays the comprehensive PDF document properties and metadata dialog.
     */
    virtual void showDocumentInfo() {}
    virtual void openStudioPanel(int tabIndex = 0) { Q_UNUSED(tabIndex); }
    virtual void closeStudioPanel() {}
    virtual bool isStudioPanelVisible() const { return false; }

    /**
     * @brief Starts the highlighter tool with the given color, opacity, and spotlight mode.
     * @param color Highlight color (default translucent yellow).
     * @param opacity Opacity factor between 0.0 and 1.0 (default 0.40).
     * @param isSpotlight True to enable page-dimming spotlight cutout focus.
     */
    virtual void startHighlighter(const QColor &color = QColor(255, 235, 59), double opacity = 0.40, bool isSpotlight = false) {
        Q_UNUSED(color); Q_UNUSED(opacity); Q_UNUSED(isSpotlight);
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
     * @brief Opens interactive PDF page organizer for page reordering and rotation.
     * @param target File path of the PDF to organize.
     * @param parentWidget Parent widget for hierarchy.
     */
    virtual void openPageOrganizer(const QString &target, QWidget *parentWidget = nullptr) {
        Q_UNUSED(target); Q_UNUSED(parentWidget);
    }

    /**
     * @brief Shows the integrated in-window PDF page grid organizer view.
     */
    virtual void showPageGridOrganizer() {}

    /**
     * @brief Hides the integrated in-window PDF page grid organizer and returns to page view.
     * @param targetPageIndex Optional 0-based page index to navigate to. If -1, uses selected organizer page.
     */
    virtual void hidePageGridOrganizer(int targetPageIndex = -1) { Q_UNUSED(targetPageIndex); }

    /**
     * @brief Checks if the PDF page grid organizer is currently active.
     */
    virtual bool isPageGridOrganizerActive() const { return false; }

    virtual void organizerRotate() {}
    virtual void organizerDelete() {}
    virtual void organizerInsertBlank() {}
    virtual void organizerAddPdf() {}
    virtual void organizerReverse() {}
    virtual void organizerReset() {}

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

    /**
     * @brief Registers callback invoked when user requests to save current page as single-page PDF.
     * @param callback Function receiving notification.
     */
    virtual void setOnSavePageAsPdfRequested(std::function<void()> callback) {
        m_savePageAsPdfRequestedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when user requests to save current page as image.
     * @param callback Function receiving notification.
     */
    virtual void setOnSavePageAsImageRequested(std::function<void()> callback) {
        m_savePageAsImageRequestedCallback = std::move(callback);
    }

    /**
     * @brief Returns the discrete history snapshots recorded during the editing session.
     * @return List of history steps including page thumbnails and action descriptions.
     */
    virtual QList<PdfHistoryStep> historySteps() const { return {}; }

    /**
     * @brief Restores the PDF document state to a specific history snapshot index.
     * @param index Target 0-based snapshot step index.
     */
    virtual void restoreHistoryStep(int index) { Q_UNUSED(index); }

    /**
     * @brief Replaces selected text directly from the contextual action bar.
     */
    virtual void replaceSelectedText(const QString &text, const QFont &font, const QColor &color, bool whiteout) {
        Q_UNUSED(text); Q_UNUSED(font); Q_UNUSED(color); Q_UNUSED(whiteout);
    }

    /**
     * @brief Updates color and opacity of active highlight live.
     */
    virtual void updateHighlighterProperties(const QColor &color, double opacity) {
        Q_UNUSED(color); Q_UNUSED(opacity);
    }

    /**
     * @brief Updates text background mode and tint during in-place text editing.
     */
    virtual void updateTextBgProperties(PdfTextBgMode mode, const QColor &color) {
        Q_UNUSED(mode); Q_UNUSED(color);
    }

    /**
     * @brief Commits in-place text editing currently active on the page canvas.
     */
    virtual void commitInPlaceTextEdit() {}

    /**
     * @brief Registers callback invoked when text is selected for contextual editing.
     */
    virtual void setOnTextSelectedForContextualEdit(std::function<void(const QString &, const QFont &, const QColor &, bool)> callback) {
        m_textSelectedForEditCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when in-place text editing starts directly on canvas.
     */
    virtual void setOnInPlaceTextEditStarted(std::function<void(const QString &, const QFont &, const QColor &, PdfTextBgMode, const QColor &)> callback) {
        m_inPlaceTextEditStartedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when in-place text editing finishes.
     */
    virtual void setOnInPlaceTextEditFinished(std::function<void()> callback) {
        m_inPlaceTextEditFinishedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when a rubber stamp is selected for contextual editing.
     */
    virtual void setOnStampSelectedForContextualEdit(std::function<void(const QString &, const QColor &, qreal, int, int)> callback) {
        m_stampSelectedForEditCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when page grid organizer is opened.
     */
    virtual void setOnPageOrganizerOpened(std::function<void()> callback) {
        m_pageOrganizerOpenedCallback = std::move(callback);
    }

    /**
     * @brief Registers callback invoked when page grid organizer is closed.
     * @param callback Function receiving selected target page index (-1 if default).
     */
    virtual void setOnPageOrganizerClosed(std::function<void(int)> callback) {
        m_pageOrganizerClosedCallback = std::move(callback);
    }

    /**
     * @brief Launches the PDF merge dialog allowing user to combine multiple documents.
     * @param parent Optional parent widget.
     */
    virtual void openPdfMergeDialog(QWidget *parent = nullptr) { Q_UNUSED(parent); }

    /**
     * @brief Launches the PDF repair dialog to salvage damaged PDF structures via QPDF.
     * @param filePath Path to corrupt PDF file.
     * @param parent Optional parent widget.
     */
    virtual void openPdfRepairDialog(const QString &filePath, QWidget *parent = nullptr) { Q_UNUSED(filePath); Q_UNUSED(parent); }

    /**
     * @brief Launches the PDF image extraction dialog.
     * @param filePath Path to target PDF file.
     * @param parent Optional parent widget.
     */
    virtual void openPdfImageExtractDialog(const QString &filePath, QWidget *parent = nullptr) { Q_UNUSED(filePath); Q_UNUSED(parent); }


protected:
    std::function<void(bool)> m_docModifiedCallback;
    std::function<void(int, int)> m_pageChangedCallback;
    std::function<void()> m_selectionChangedCallback;
    std::function<void()> m_historyChangedCallback;
    std::function<void()> m_continuousModeExitedCallback;
    std::function<void(const QString &)> m_fileOrganizedCallback;
    std::function<void(double)> m_zoomChangedCallback;
    std::function<void()> m_savePageAsPdfRequestedCallback;
    std::function<void()> m_savePageAsImageRequestedCallback;
    std::function<void(const QString &, const QFont &, const QColor &, bool)> m_textSelectedForEditCallback;
    std::function<void(const QString &, const QFont &, const QColor &, PdfTextBgMode, const QColor &)> m_inPlaceTextEditStartedCallback;
    std::function<void()> m_inPlaceTextEditFinishedCallback;
    std::function<void(const QString &, const QColor &, qreal, int, int)> m_stampSelectedForEditCallback;
    std::function<void()> m_pageOrganizerOpenedCallback;
    std::function<void(int)> m_pageOrganizerClosedCallback;
};
