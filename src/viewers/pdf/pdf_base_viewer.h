/**
 * @file pdf_base_viewer.h
 * @brief Lightweight, high-performance native PDF document viewer for BubezView Core.
 *
 * Provides sub-second page rendering, continuous smooth scrolling, crisp anti-aliased
 * font rasterization, interactive zoom (fit-to-width, fit-to-page, manual factors),
 * orientation rotation, table of contents (TOC/outlines), text selection, search,
 * metadata info inspection, and native printing without heavy external editing tools.
 */

#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeView>
#include <QStandardItemModel>
#include <QImage>
#include <QMap>
#include <memory>

namespace Poppler {
    class Document;
    class Page;
    class OutlineItem;
}

#include "core/interfaces/i_pdf_viewer.h"

class QLabel;
class QPushButton;
class QToolButton;
class QSpinBox;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;
class PdfBaseCanvas;

/**
 * @class PdfBaseViewer
 * @brief Base PDF reading engine integrated directly into BubezView Core.
 */
class PdfBaseViewer : public QWidget, public IPdfViewer
{
    Q_OBJECT

public:
    explicit PdfBaseViewer(QWidget *parent = nullptr);
    ~PdfBaseViewer() override;

    QWidget *widget() override { return this; }

    /**
     * @brief Loads and parses a PDF document from disk.
     * @param filePath Absolute path of the target PDF file.
     * @return True if opened and unlocked successfully; false on error.
     */
    bool loadFile(const QString &filePath) override;

    /**
     * @brief Closes active document and releases page memory caches.
     */
    void clear() override;

    int currentPage() const override { return m_currentPage; }
    int pageCount() const override { return m_pageCount; }
    QSize pageSize() const;

    // --- Navigation ---
    void nextPage() override;
    void prevPage() override;
    void goToPage(int page) override;
    void scrollToPage(int page);

    // --- Zoom & Orientation ---
    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;
    void fitToWidth() override;
    void fitToPage();
    void setZoomFactor(double factor) override;
    double zoomFactor() const override { return m_zoomFactor; }
    void rotateClockwise() override;
    void rotateCounterClockwise() override;

    // --- Outlines & Bookmarks ---
    void toggleTocSidebar() override;
    bool isTocVisible() const override;
    bool hasOutline() const override { return m_hasOutline; }

    // --- Text Search ---
    bool findNext(const QString &text, bool matchCase = false, bool *wrapped = nullptr) override;
    bool findPrev(const QString &text, bool matchCase = false, bool *wrapped = nullptr) override;
    void clearSearch();

    // --- Text Selection & Extraction ---
    bool hasTextSelection() const override;
    QString selectedText() const override;
    void copySelectedText() override;
    void cutSelectedText() override {}
    void paste() override {}
    QString extractAllText() const override;

    // --- OCR Rendering Bridge ---
    QImage renderPageForOcr(int pageIndex, int dpi = 300) const override;
    QImage renderCurrentPageForOcr(int dpi = 300) const override;

    // --- Appearance & Styling ---
    void updateTheme() override;

    // --- Core Read-Only Actions ---
    void showDocumentInfo();
    void printDocument();

    // --- IPdfViewer Studio Stubs / Delegations ---
    void toggleZoomTool(bool active) override { Q_UNUSED(active); }
    bool isZoomToolActive() const override { return false; }
    bool hasAreaSelection() const override { return false; }
    void openTypewriterDialog() override {}
    void startRedaction(const QColor & = Qt::black, bool = false) override {}
    void openWatermarkDialog() override {}
    void openInsertImageDialog() override {}
    void openSignatureDialog() override {}
    bool insertBlankPage(int = -1) override { return false; }
    bool deleteCurrentPage() override { return false; }
    void startSelectArea() override {}
    void setModified(bool) override {}
    void startShape(int, int, const QColor &, const QColor &) override {}
    void startHighlighter(const QColor & = QColor(255, 235, 59), double = 0.40, bool = false) override {}
    void startSnapshot() override {}
    void openCryptoSignatureDialog(int = 0) override {}
    void openPageOrganizer(const QString & = QString(), QWidget * = nullptr) override {}
    void openPdfMergeDialog(QWidget * = nullptr) override {}
    void openPdfRepairDialog(const QString & = QString(), QWidget * = nullptr) override {}
    void openPdfImageExtractDialog(const QString & = QString(), QWidget * = nullptr) override {}
    bool isPageGridOrganizerActive() const override { return false; }
    void hidePageGridOrganizer(int = -1) override {}
    void showPageGridOrganizer() override {}
    void setEditMode(bool) override {}
    bool isEditMode() const override { return false; }
    bool save() override { return false; }
    bool saveAs(const QString &) override { return false; }
    bool isModified() const override { return false; }
    bool saveCurrentPageAsPdf(const QString &, QString * = nullptr) override;
    bool saveCurrentPageAsImage(const QString &outputPath, const QString &format = QStringLiteral("PNG"), int dpi = 300, QString *errorMsg = nullptr);
    void undo() override {}
    void redo() override {}
    bool canUndo() const override { return false; }
    bool canRedo() const override { return false; }
    QList<PdfHistoryStep> historySteps() const override { return {}; }
    void restoreHistoryStep(int) override {}

signals:
    void pageChanged(int currentPage, int totalPages);
    void outlineAvailabilityChanged(bool available);
    void selectionChanged(bool hasSelection);
    void requestStudio();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTocItemActivated(const QModelIndex &index);
    void onCanvasPageScrolled(int pageIndex);
    void onCanvasSelectionChanged(bool hasSelection);

private:
    void populateOutline();
    void buildOutlineBranch(QStandardItem *parentItem, const QVector<Poppler::OutlineItem> &items);
    void updateNavControls();
    void showHud(bool autoHide = true);
    void hideHud();

    QString m_filePath;
    std::unique_ptr<Poppler::Document> m_doc;
    int m_currentPage = 0;
    int m_pageCount = 0;
    double m_zoomFactor = 1.0;
    bool m_fitToWidthMode = true;
    int m_rotationDegrees = 0;
    bool m_hasOutline = false;

    // UI Structure
    QSplitter *m_splitter = nullptr;
    QWidget *m_tocContainer = nullptr;
    QLabel *m_lblTocTitle = nullptr;
    QPushButton *m_btnCloseToc = nullptr;
    QTreeView *m_tocView = nullptr;
    QStandardItemModel *m_tocModel = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    PdfBaseCanvas *m_canvas = nullptr;

    // Floating HUD Navigation Capsule
    QWidget *m_hudPill = nullptr;
    QToolButton *m_btnPrev = nullptr;
    QLabel *m_lblPageText = nullptr;
    QSpinBox *m_spinPage = nullptr;
    QLabel *m_lblPageCount = nullptr;
    QToolButton *m_btnNext = nullptr;

    QToolButton *m_btnZoomOut = nullptr;
    QToolButton *m_btnZoomReset = nullptr;
    QToolButton *m_btnZoomIn = nullptr;
    QToolButton *m_btnFitWidth = nullptr;
    QToolButton *m_btnRotate = nullptr;
    QToolButton *m_btnInfo = nullptr;
    QToolButton *m_btnPrint = nullptr;
    QToolButton *m_btnToc = nullptr;
    QPushButton *m_btnStudio = nullptr;

    QGraphicsOpacityEffect *m_hudOpacity = nullptr;
    QPropertyAnimation *m_hudAnim = nullptr;
    QTimer *m_hudTimer = nullptr;
};
