/**
 * @file pdf_page_organizer_dialog.h
 * @brief Visual drag-and-drop PDF page organizer, reorderer, rotator, and page remover.
 */

#pragma once

#include "dialogs/frameless_dialog.h"
#include <QString>
#include <QListWidget>
#include <QContextMenuEvent>
#include <memory>
#include <vector>

class QListWidgetItem;
class QPushButton;
class QLabel;
class QProgressBar;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QPaintEvent;

namespace Poppler {
    class Document;
}

/**
 * @class PdfOrganizerListWidget
 * @brief Custom thumbnail grid list widget providing fluid drag-and-drop page reordering
 *        with automatic intermediate page scaling/shifting and dead space drop handling.
 */
class PdfOrganizerListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit PdfOrganizerListWidget(QWidget *parent = nullptr);
    ~PdfOrganizerListWidget() override = default;

    /**
     * @brief Moves currently selected items to targetRow, automatically shifting
     *        intermediate items in the model and emitting pagesReordered.
     * @param targetRow The desired row destination (0 to count()).
     */
    void moveSelectedItemsTo(int targetRow);

signals:
    /**
     * @brief Emitted whenever items have been rearranged via drag-and-drop.
     */
    void pagesReordered();

    /**
     * @brief Emitted when Escape key is pressed inside the list view.
     */
    void escapePressed();

    /**
     * @brief Emitted when Delete or Backspace key is pressed on selected items.
     */
    void deletePressed();

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override { event->accept(); }

private:
    int calculateTargetRow(const QPoint &pos) const;

    bool m_isDragging;
    int m_dragInsertRow;
};

/**
 * @class PdfPageOrganizerWidget
 * @brief Integrated in-window visual page grid organizer for PDF documents.
 *
 * Embeds directly inside PdfViewer as a full-screen/canvas view mode (similar to Apple Preview
 * thumbnail grid), allowing direct real-time reordering, rotation, deletion, and insertion
 * of pages without popping up detached modal dialogs.
 */
class PdfPageOrganizerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PdfPageOrganizerWidget(QWidget *parent = nullptr, bool showInternalToolbar = false);
    ~PdfPageOrganizerWidget() override;

    /**
     * @brief Loads the PDF document pages into the thumbnail grid.
     * @param doc The active Poppler document instance.
     * @param sourcePath File path to the active document snapshot.
     * @param initialPageIndex Optional 0-based page index to focus and select initially.
     */
    void loadDocument(Poppler::Document *doc, const QString &sourcePath, int initialPageIndex = 0);

    /**
     * @brief Executes QPDF to produce an organized PDF temporary snapshot.
     * @param outTempPath Output temporary file path.
     * @return True if generation succeeded.
     */
    bool applyChanges(QString &outTempPath);

    /**
     * @brief Checks if any modifications (reordering, rotation, deletion, additions) were made.
     */
    bool isModified() const { return m_isModified; }

    /**
     * @brief Returns the 0-based index of the currently selected page in the grid,
     *        or 0 if none is selected.
     */
    int selectedPageIndex() const;

    /**
     * @brief Direct pointer to internal list widget (for test assertions).
     */
    PdfOrganizerListWidget *listWidget() const { return m_listWidget; }

public slots:
    void rotateSelectedPages();
    void deleteSelectedPages();
    void insertBlankPage();
    void addFromPdf();
    void reverseOrder();
    void resetOrder();

signals:
    /**
     * @brief Emitted when the user requests returning to normal document view (via Done button or Esc).
     */
    void backRequested();

    /**
     * @brief Emitted when a page card is double-clicked to open directly at that page.
     * @param pageIndex 0-based page index.
     */
    void pageDoubleClicked(int pageIndex);

    /**
     * @brief Emitted whenever page structure or order has changed.
     */
    void documentStructureChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override { event->accept(); }

private slots:
    void onRotate();
    void onDeleteSelected();
    void onInsertBlankPage();
    void onAddFromPdf();
    void onReverseOrder();
    void onResetOrder();
    void onSelectionChanged();

private:
    void setupUi();
    void updateItemVisuals(QListWidgetItem *item, bool updatePixmap = true);
    void updateAllItemLabels(bool labelsOnly = false);
    void updateStatsLabel();
    bool executeQpdfReorder(const QString &destinationPath);
    QImage createBlankPageThumbnail();

    QString m_sourcePath;
    bool m_isModified;
    bool m_showInternalToolbar;
    std::vector<std::unique_ptr<Poppler::Document>> m_extraDocs;

    QWidget *m_topBar;
    QLabel *m_lblTitle;
    QLabel *m_lblStats;
    PdfOrganizerListWidget *m_listWidget;
    QPushButton *m_btnBack;
    QPushButton *m_btnRotate;
    QPushButton *m_btnDelete;
    QPushButton *m_btnInsertBlank;
    QPushButton *m_btnAddPdf;
    QPushButton *m_btnReverse;
    QPushButton *m_btnReset;
    QProgressBar *m_progressBar;
};

/**
 * @class PdfPageOrganizerDialog
 * @brief Standalone dialog wrapper around PdfPageOrganizerWidget.
 */
class PdfPageOrganizerDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit PdfPageOrganizerDialog(const QString &pdfPath, QWidget *parent = nullptr);
    ~PdfPageOrganizerDialog() override = default;

    /**
     * @brief Returns the output path if saved or organized.
     */
    QString outputFilePath() const { return m_outPath; }

    /**
     * @brief Direct pointer to embedded organizer widget.
     */
    PdfPageOrganizerWidget *organizerWidget() const { return m_organizerWidget; }

signals:
    void fileOrganized(const QString &filePath);

private:
    PdfPageOrganizerWidget *m_organizerWidget;
    QString m_outPath;
};
