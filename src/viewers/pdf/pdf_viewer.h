#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeView>
#include <QStandardItemModel>
#include <QImage>
#include <memory>
#include "pdf_page_widget.h"

class QLabel;
class QPushButton;
class QToolButton;
class QSpinBox;

namespace Poppler {
    class Document;
    class Page;
    class OutlineItem;
}

#include "core/interfaces/i_pdf_viewer.h"

/**
 * @brief Native PDF viewer utilizing the Poppler-Qt6 engine.
 *
 * Provides sub-second page rendering, crisp anti-aliased font rasterization,
 * interactive zoom/fit-to-width modes, page rotation, document-wide text search,
 * collapsible table of contents (TOC / Outlines), interactive mouse text selection,
 * and high-resolution image extraction.
 */
class PdfViewer : public QWidget, public IPdfViewer
{
    Q_OBJECT

public:
    explicit PdfViewer(QWidget *parent = nullptr);
    ~PdfViewer() override;

    QWidget *widget() override { return this; }

    /**
     * @brief Loads and parses a PDF document from disk.
     * @param filePath Absolute path of the PDF file.
     * @param preserveHistory If true, preserves existing Undo/Redo history stacks across reload.
     * @return True if opened and unlocked successfully; false on error.
     */
    bool loadFile(const QString &filePath) override { return loadFile(filePath, false); }
    bool loadFile(const QString &filePath, bool preserveHistory);

    /**
     * @brief Loads a pre-parsed Poppler document directly, optionally with a pre-rendered initial page image.
     * @param doc Pre-parsed Poppler document.
     * @param filePath The absolute path of the file on disk.
     * @param initialPageImage Optional pre-rendered image of the first page.
     * @return True if opened and unlocked successfully; false on error.
     */
    bool loadDocument(std::unique_ptr<Poppler::Document> doc, const QString &filePath = QString(), const QImage &initialPageImage = QImage());

    /**
     * @brief Closes the active Poppler document and frees page caches.
     */
    void clear();

    int currentPage() const { return m_currentPage; }
    int pageCount() const { return m_pageCount; }
    QSize pageSize() const;

    /**
     * @brief Extracts text from all pages in the PDF document.
     * @return Formatted multi-page text string.
     */
    QString extractAllText() const;

    /**
     * @brief Returns whether the document contains an outline (table of contents).
     * @return True if outline items exist.
     */
    bool hasOutline() const { return m_hasOutline; }

    /**
     * @brief Checks if the table of contents sidebar is currently visible.
     * @return True if TOC sidebar is visible.
     */
    bool isTocVisible() const;

    /**
     * @brief Copies currently selected text to clipboard.
     */
    void copySelectedText();

    /**
     * @brief Cuts currently selected text: copies text to clipboard and applies a whiteout erasure.
     */
    void cutSelectedText();

    /**
     * @brief Pastes text from clipboard onto the current page.
     */
    void pasteText();
    void paste() override { pasteText(); }
    void goToPage(int page) override { setPage(page); }

    /**
     * @brief Checks if text is currently selected on the active page.
     * @return True if words are highlighted.
     */
    bool hasTextSelection() const;

    /**
     * @brief Extracts embedded images to the given destination directory.
     * @param targetDir Directory where extracted images will be written.
     * @return Number of extracted images, or -1 on failure.
     */
    int extractEmbeddedImages(const QString &targetDir);

    /**
     * @brief Renders a specific page to a high-resolution QImage suitable for OCR processing.
     * @param pageIndex 0-based index of the page to render.
     * @param dpi Target raster resolution (defaults to 300 DPI for optimal character recognition).
     * @return Rasterized page image.
     */
    QImage renderPageForOcr(int pageIndex, int dpi = 300) const;

    /**
     * @brief Renders the current page to a high-resolution QImage suitable for OCR processing.
     * @param dpi Target raster resolution (defaults to 300 DPI for optimal character recognition).
     * @return Rasterized page image.
     */
    QImage renderCurrentPageForOcr(int dpi = 300) const;

    bool isModified() const { return m_isModified; }
    void setModified(bool mod);

    /**
     * @brief Saves modifications to the active PDF file.
     * @return True if saved successfully.
     */
    bool save();

    /**
     * @brief Saves modifications to a specified destination PDF path.
     * @param targetPath Destination file path.
     * @return True if saved successfully.
     */
    bool saveAs(const QString &targetPath);

    /**
     * @brief Exports the currently viewed page to the given image or text format.
     * @param format Destination format extension ("png", "jpg", "webp", "tiff", "bmp", "txt").
     * @param outputPath Target file path on disk.
     * @param dpi Target raster resolution (defaults to 300 DPI).
     * @return True on success; false on failure.
     */
    bool exportCurrentPage(const QString &format, const QString &outputPath, int dpi = 300);

    PdfPageWidget *pageWidget() const { return m_pageWidget; }
    double zoomFactor() const { return m_zoomFactor; }
    bool isFitToWidth() const { return m_fitToWidthMode; }
    bool hasAreaSelection() const;

    /**
     * @brief Checks if an undo action is available.
     * @return True if previous document state exists.
     */
    bool canUndo() const;

    /**
     * @brief Checks if a redo action is available.
     * @return True if next document state exists.
     */
    bool canRedo() const;

public slots:
    /**
     * @brief Reverts the last document modification (stamp, cut, push-down, blank page).
     */
    void undo();

    /**
     * @brief Re-applies the previously undone modification.
     */
    void redo();

    void nextPage() override;
    void prevPage() override;
    void setPage(int page);
    void scrollToPage(int pageIndex);
    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;
    void fitToWidth() override;
    void rotateClockwise();
    void rotateCounterClockwise();
    void zoomAt(const QPoint &viewportPos, double factor);
    void toggleZoomTool(bool active) override;
    bool isZoomToolActive() const override { return m_zoomToolActive; }
    void toggleTocSidebar() override;
    void setTocVisible(bool visible);

    // PDF Annotation & Tool Launchers
    void startHighlighter(const QColor &color = QColor(255, 235, 59), double opacity = 0.40);
    void startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);
    void exitContinuousToolMode();
    void openTypewriterDialog();
    void openSignatureDialog();
    void openCryptoSignatureDialog(int defaultTab = 0) override;
    void openInsertImageDialog();
    void openWatermarkDialog();
    void openShapeDialog();
    void startSnapshot();
    void startSelectArea();
    void startRedaction();
    bool insertBlankPage(int afterPageIndex = -1);
    void openPageOrganizer(const QString &target, QWidget *parentWidget = nullptr) override;

    void onAddStampAnnotation(const QImage &image, const QRectF &normalizedRect);
    void onAddStampAnnotations(const QList<QPair<QImage, QRectF>> &stamps);
    void onAddTextAnnotation(const QString &text, const QFont &font, const QColor &color,
                             const QRectF &normalizedRect, bool whiteout);
    void onSnapshotAreaSelected(const QRectF &pointRect);
    void onCutAreaRequested(const QRectF &pointRect);
    void onCopyAreaRequested(const QRectF &pointRect);
    void onFormFieldModified();

    /**
     * @brief Searches forward for the next occurrence of text across document pages.
     * @param text Query text to locate.
     * @param matchCase True to match case sensitively.
     * @param wrapped Optional output boolean set to true if search reached the end and wrapped to page 1.
     * @return True if a match was found; false otherwise.
     */
    bool findNext(const QString &text, bool matchCase, bool *wrapped = nullptr);

    /**
     * @brief Searches backward for the previous occurrence of text across document pages.
     * @param text Query text to locate.
     * @param matchCase True to match case sensitively.
     * @param wrapped Optional output boolean set to true if search reached the start and wrapped to the last page.
     * @return True if a match was found; false otherwise.
     */
    bool findPrev(const QString &text, bool matchCase, bool *wrapped = nullptr);

    /**
     * @brief Refreshes visual appearance and canvas styling based on the active ThemePreset.
     */
    void updateTheme();

signals:
    /**
     * @brief Emitted when the user changes the active viewing page.
     * @param current 1-based current page number.
     * @param total Total number of pages in the PDF.
     */
    void pageChanged(int current, int total);

    /**
     * @brief Emitted when the zoom level changes.
     * @param factor Current zoom scale factor (1.0 = 100%).
     */
    void zoomChanged(double factor);

    /**
     * @brief Emitted when the availability of an outline/TOC changes.
     * @param available True if TOC is present in the document.
     */
    void outlineAvailabilityChanged(bool available);

    /**
     * @brief Emitted when text selection changes.
     * @param hasSelection True if text is selected.
     */
    void selectionChanged(bool hasSelection);

    /**
     * @brief Emitted when document modification status changes.
     * @param modified True if unsaved changes exist.
     */
    void documentModified(bool modified);

    /**
     * @brief Emitted when undo/redo availability or state changes.
     */
    void historyChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTocItemActivated(const QModelIndex &index);
    void onVerticalScrollChanged(int value);
    void updateNavButtons();

private:
    bool handleViewportWheel(QWheelEvent *event, QObject *source);
    void prepareForManualZoom();
    void renderCurrentPage(int transitionDirection = 0);
    void populateOutline();
    void buildOutlineBranch(QStandardItem *parentItem, const QVector<Poppler::OutlineItem> &items);
    void pushUndoState();
    void clearUndoRedoStacks();
    void resetDocument();
    void verifyAndRefreshSignatureBanner();
    QString currentDocumentSourcePath() const;

    // Top Cryptographic Signature Verification Banner
    QWidget *m_sigBanner = nullptr;
    QLabel *m_lblSigBannerText = nullptr;
    QPushButton *m_btnSigBannerDetails = nullptr;
    QPushButton *m_btnSigBannerSign = nullptr;
    QPushButton *m_btnSigBannerClose = nullptr;

    QSplitter *m_splitter;
    QWidget *m_tocContainer;
    QLabel *m_lblTocTitle;
    QPushButton *m_btnCloseToc;
    QTreeView *m_tocView;
    QStandardItemModel *m_tocModel;
    QScrollArea *m_scrollArea;
    PdfPageWidget *m_pageWidget;

    // Native Page Navigation Bar (docked bottom bar)
    QWidget *m_navBar = nullptr;
    QWidget *m_pillContainer = nullptr;
    QToolButton *m_btnPrevPage = nullptr;
    QLabel *m_lblPage = nullptr;
    QSpinBox *m_spinPage = nullptr;
    QLabel *m_lblPageCount = nullptr;
    QToolButton *m_btnNextPage = nullptr;

    std::unique_ptr<Poppler::Document> m_doc;
    QString m_filePath;
    QString m_extractedP7mTempPath;
    int m_currentPage;
    int m_pageCount;
    double m_zoomFactor;
    bool m_fitToWidthMode;
    int m_rotationDegrees;
    bool m_zoomToolActive;
    bool m_hasOutline;
    bool m_isModified;

    QList<QString> m_undoStack;
    QList<QString> m_redoStack;
    QString m_activeSnapshotPath;
    static constexpr int MAX_UNDO_STACK_SIZE = 20;

    int m_wheelAccumulator = 0;
    qint64 m_lastWheelPageTime = 0;
    int m_lastWheelPageDir = 0;
};


