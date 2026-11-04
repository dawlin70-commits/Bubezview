/**
 * @file pdf_page_organizer_dialog.cpp
 * @brief Implementation of visual PDF page organizer and manipulator.
 */

#include "pdf_page_organizer_dialog.h"
#include "package_manager_helper.h"
#include "build_config.h"
#include "fallback_notice_dialog.h"
#include <poppler/qt6/poppler-qt6.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QApplication>
#include <QTransform>
#include <QPainter>
#include <QPageSize>
#include <QtGui/QPdfWriter>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <algorithm>
#include <set>
#include <limits>

namespace {
    constexpr int ROLE_ORIGINAL_PAGE = Qt::UserRole + 1;
    constexpr int ROLE_ROTATION_DEG  = Qt::UserRole + 2;
    constexpr int ROLE_BASE_IMAGE    = Qt::UserRole + 3;
    constexpr int ROLE_SOURCE_PATH   = Qt::UserRole + 4;
    constexpr int ROLE_IS_BLANK      = Qt::UserRole + 5;
    constexpr int ROLE_SOURCE_LABEL  = Qt::UserRole + 6;
}

/* ========================================================================= */
/*                      PdfOrganizerListWidget Implementation               */
/* ========================================================================= */

PdfOrganizerListWidget::PdfOrganizerListWidget(QWidget *parent)
    : QListWidget(parent)
    , m_isDragging(false)
    , m_dragInsertRow(-1)
{
    setViewMode(QListView::IconMode);
    setIconSize(QSize(150, 200));
    setGridSize(QSize(195, 260));
    setSpacing(20);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setUniformItemSizes(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setWrapping(true);
    setWordWrap(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(false);
}

void PdfOrganizerListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    const auto selected = selectedItems();
    if (selected.isEmpty()) return;

    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData();
    mimeData->setData("application/x-bubez-pdf-page", QByteArray("page_drag"));
    drag->setMimeData(mimeData);

    QPixmap pix = selected.first()->icon().pixmap(QSize(90, 120));
    if (!pix.isNull()) {
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    }

    m_isDragging = true;
    m_dragInsertRow = -1;

    drag->exec(Qt::MoveAction);

    m_isDragging = false;
    m_dragInsertRow = -1;
    viewport()->update();
}

void PdfOrganizerListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if ((event->source() == this || event->source() == viewport() || event->source() == nullptr)
        && event->mimeData()->hasFormat("application/x-bubez-pdf-page")) {
        m_isDragging = true;
        m_dragInsertRow = calculateTargetRow(event->position().toPoint());
        viewport()->update();
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void PdfOrganizerListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if ((event->source() == this || event->source() == viewport() || event->source() == nullptr)
        && event->mimeData()->hasFormat("application/x-bubez-pdf-page")) {
        m_isDragging = true;
        const int newTarget = calculateTargetRow(event->position().toPoint());
        if (newTarget != m_dragInsertRow) {
            m_dragInsertRow = newTarget;
            viewport()->update();
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void PdfOrganizerListWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_isDragging = false;
    m_dragInsertRow = -1;
    viewport()->update();
    event->accept();
}

void PdfOrganizerListWidget::moveSelectedItemsTo(int targetRow)
{
    const auto selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    // 1. Single item move: take from fromRow and insert directly at target toRow
    if (selected.size() == 1) {
        int fromRow = row(selected.first());
        int toRow = qBound(0, targetRow, count() - 1);
        if (fromRow == toRow) {
            return;
        }

        QListWidgetItem *it = takeItem(fromRow);
        insertItem(toRow, it);
        it->setSelected(true);
        doItemsLayout();
        scrollToItem(it);
        emit pagesReordered();
        return;
    }

    // 2. Multi-selection move
    std::vector<int> selectedRows;
    selectedRows.reserve(selected.size());
    for (auto *it : selected) {
        selectedRows.push_back(row(it));
    }
    std::sort(selectedRows.begin(), selectedRows.end());

    // If moving a contiguous block onto itself, do nothing
    if (targetRow >= selectedRows.front() && targetRow <= selectedRows.back() + 1) {
        bool contiguous = true;
        for (size_t i = 1; i < selectedRows.size(); ++i) {
            if (selectedRows[i] != selectedRows[i - 1] + 1) {
                contiguous = false;
                break;
            }
        }
        if (contiguous) {
            return;
        }
    }

    int itemsBeforeTarget = 0;
    for (int r : selectedRows) {
        if (r < targetRow) {
            itemsBeforeTarget++;
        }
    }

    QList<QListWidgetItem*> movedItems;
    for (int r : selectedRows) {
        movedItems.append(item(r));
    }

    for (auto it = selectedRows.rbegin(); it != selectedRows.rend(); ++it) {
        takeItem(*it);
    }

    int insertIndex = qBound(0, targetRow - itemsBeforeTarget, count());

    clearSelection();
    for (int i = 0; i < movedItems.size(); ++i) {
        insertItem(insertIndex + i, movedItems.at(i));
        movedItems.at(i)->setSelected(true);
    }

    doItemsLayout();

    if (!movedItems.isEmpty()) {
        scrollToItem(movedItems.first());
    }

    emit pagesReordered();
}

void PdfOrganizerListWidget::dropEvent(QDropEvent *event)
{
    if ((event->source() == this || event->source() == viewport() || event->source() == nullptr)
        && event->mimeData()->hasFormat("application/x-bubez-pdf-page")) {
        const int targetRow = calculateTargetRow(event->position().toPoint());
        moveSelectedItemsTo(targetRow);
        m_isDragging = false;
        m_dragInsertRow = -1;
        viewport()->update();
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
}

int PdfOrganizerListWidget::calculateTargetRow(const QPoint &pos) const
{
    const int total = count();
    if (total == 0) return 0;

    // 1. Direct hit on an item bounding rect (including small margin for easy targeting)
    for (int i = 0; i < total; ++i) {
        if (visualItemRect(item(i)).adjusted(-4, -4, 4, 4).contains(pos)) {
            return i;
        }
    }

    // Direct hit fallback via itemAt
    QListWidgetItem *target = itemAt(pos);
    if (target) {
        return row(target);
    }

    // 2. Dead / empty space bounds
    int maxBottom = 0;
    int minTop = std::numeric_limits<int>::max();
    for (int i = 0; i < total; ++i) {
        QRect r = visualItemRect(item(i));
        maxBottom = std::max(maxBottom, r.bottom());
        minTop = std::min(minTop, r.top());
    }

    if (pos.y() > maxBottom) {
        return total - 1; // Drop at the very end
    }
    if (pos.y() < minTop) {
        return 0; // Drop at the very beginning
    }

    // 3. Row band check (handles empty margin on right or left of a visual row)
    int bandFirst = -1;
    int bandLast = -1;
    for (int i = 0; i < total; ++i) {
        QRect r = visualItemRect(item(i));
        if (pos.y() >= r.top() - 6 && pos.y() <= r.bottom() + 6) {
            if (bandFirst == -1) bandFirst = i;
            bandLast = i;
        }
    }

    if (bandFirst != -1) {
        QRect firstRect = visualItemRect(item(bandFirst));
        if (pos.x() <= firstRect.left()) {
            return bandFirst;
        }
        QRect lastRect = visualItemRect(item(bandLast));
        if (pos.x() >= lastRect.right()) {
            return bandLast;
        }
        // Between cards in this row band
        for (int i = bandFirst; i <= bandLast; ++i) {
            QRect r = visualItemRect(item(i));
            if (pos.x() <= r.right() + spacing()) {
                return i;
            }
        }
    }

    // 4. Euclidean distance fallback
    int bestRow = -1;
    qint64 closestDistSq = std::numeric_limits<qint64>::max();
    for (int i = 0; i < total; ++i) {
        QRect r = visualItemRect(item(i));
        qint64 dx = pos.x() - r.center().x();
        qint64 dy = pos.y() - r.center().y();
        qint64 dist = dx * dx + dy * dy;
        if (dist < closestDistSq) {
            closestDistSq = dist;
            bestRow = i;
        }
    }

    return qBound(0, bestRow < 0 ? total - 1 : bestRow, total - 1);
}

void PdfOrganizerListWidget::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);

    if (!m_isDragging || m_dragInsertRow < 0) {
        return;
    }

    const int total = count();
    if (total == 0) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int insertIdx = qBound(0, m_dragInsertRow, total - 1);
    QListWidgetItem *it = item(insertIdx);
    if (!it) return;

    QRect r = visualItemRect(it);

    // 1. Highlight target card with translucent sky blue box
    QColor targetFill(56, 189, 248, 40);
    QColor targetStroke(56, 189, 248, 200);
    painter.setPen(QPen(targetStroke, 2.0));
    painter.setBrush(targetFill);
    painter.drawRoundedRect(r.adjusted(2, 2, -2, -2), 6, 6);

    // 2. Draw prominent vertical insertion indicator bar
    const QColor indicatorColor(56, 189, 248);
    painter.setPen(QPen(indicatorColor, 3.5, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(indicatorColor);
    int lineX = r.left() - spacing() / 2;
    if (lineX < 4) lineX = r.left() - 2;
    int lineY1 = r.top() + 6;
    int lineY2 = r.bottom() - 6;
    painter.drawLine(lineX, lineY1, lineX, lineY2);
    constexpr int dotRadius = 4;
    painter.drawEllipse(QPoint(lineX, lineY1), dotRadius, dotRadius);
    painter.drawEllipse(QPoint(lineX, lineY2), dotRadius, dotRadius);
}

void PdfOrganizerListWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit escapePressed();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit deletePressed();
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}

/* ========================================================================= */
/*                      PdfPageOrganizerWidget Implementation                */
/* ========================================================================= */

PdfPageOrganizerWidget::PdfPageOrganizerWidget(QWidget *parent, bool showInternalToolbar)
    : QWidget(parent)
    , m_sourcePath()
    , m_isModified(false)
    , m_showInternalToolbar(showInternalToolbar)
    , m_topBar(nullptr)
    , m_lblTitle(nullptr)
    , m_lblStats(nullptr)
    , m_listWidget(nullptr)
    , m_btnBack(nullptr)
    , m_btnRotate(nullptr)
    , m_btnDelete(nullptr)
    , m_btnInsertBlank(nullptr)
    , m_btnAddPdf(nullptr)
    , m_btnReverse(nullptr)
    , m_btnReset(nullptr)
    , m_progressBar(nullptr)
{
    setFocusPolicy(Qt::StrongFocus);
    setupUi();
}

PdfPageOrganizerWidget::~PdfPageOrganizerWidget() = default;

void PdfPageOrganizerWidget::rotateSelectedPages()
{
    onRotate();
}

void PdfPageOrganizerWidget::deleteSelectedPages()
{
    onDeleteSelected();
}

void PdfPageOrganizerWidget::insertBlankPage()
{
    onInsertBlankPage();
}

void PdfPageOrganizerWidget::addFromPdf()
{
    onAddFromPdf();
}

void PdfPageOrganizerWidget::reverseOrder()
{
    onReverseOrder();
}

void PdfPageOrganizerWidget::resetOrder()
{
    onResetOrder();
}

void PdfPageOrganizerWidget::setupUi()
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(6);
    mainLay->setContentsMargins(m_showInternalToolbar ? 10 : 16, m_showInternalToolbar ? 8 : 12,
                                m_showInternalToolbar ? 10 : 16, m_showInternalToolbar ? 8 : 12);

    if (m_showInternalToolbar) {
        // Top Action Bar for standalone dialog mode
        m_topBar = new QWidget(this);
        m_topBar->setObjectName("pageOrganizerTopBar");
        auto *topLay = new QHBoxLayout(m_topBar);
        topLay->setContentsMargins(4, 2, 4, 4);
        topLay->setSpacing(8);

        m_btnBack = new QPushButton(tr("← Back to Document"), m_topBar);
        m_btnBack->setCursor(Qt::PointingHandCursor);
        m_btnBack->setToolTip(tr("Return to full document reading and editing (Esc)"));
        m_btnBack->setStyleSheet(
            "QPushButton { font-weight: bold; background-color: #0284c7; color: white; "
            "padding: 6px 14px; border-radius: 6px; font-size: 12px; } "
            "QPushButton:hover { background-color: #0369a1; }"
        );
        connect(m_btnBack, &QPushButton::clicked, this, &PdfPageOrganizerWidget::backRequested);
        topLay->addWidget(m_btnBack);

        topLay->addSpacing(6);

        m_btnRotate = new QPushButton(tr("⟳ Rotate"), m_topBar);
        m_btnRotate->setToolTip(tr("Rotate selected pages 90° clockwise"));
        m_btnRotate->setCursor(Qt::PointingHandCursor);
        connect(m_btnRotate, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onRotate);
        topLay->addWidget(m_btnRotate);

        m_btnDelete = new QPushButton(tr("🗑️ Remove"), m_topBar);
        m_btnDelete->setToolTip(tr("Remove selected pages from document (Del)"));
        m_btnDelete->setShortcut(QKeySequence::Delete);
        m_btnDelete->setCursor(Qt::PointingHandCursor);
        connect(m_btnDelete, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onDeleteSelected);
        topLay->addWidget(m_btnDelete);

        topLay->addSpacing(4);

        m_btnInsertBlank = new QPushButton(tr("➕ Blank Page"), m_topBar);
        m_btnInsertBlank->setToolTip(tr("Insert an empty blank page after selected item"));
        m_btnInsertBlank->setCursor(Qt::PointingHandCursor);
        connect(m_btnInsertBlank, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onInsertBlankPage);
        topLay->addWidget(m_btnInsertBlank);

        m_btnAddPdf = new QPushButton(tr("📑 Add from PDF..."), m_topBar);
        m_btnAddPdf->setToolTip(tr("Append or insert pages from another PDF file"));
        m_btnAddPdf->setCursor(Qt::PointingHandCursor);
        connect(m_btnAddPdf, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onAddFromPdf);
        topLay->addWidget(m_btnAddPdf);

        topLay->addSpacing(4);

        m_btnReverse = new QPushButton(tr("⇄ Reverse Order"), m_topBar);
        m_btnReverse->setToolTip(tr("Invert the overall page sequence"));
        m_btnReverse->setCursor(Qt::PointingHandCursor);
        connect(m_btnReverse, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onReverseOrder);
        topLay->addWidget(m_btnReverse);

        m_btnReset = new QPushButton(tr("↺ Reset"), m_topBar);
        m_btnReset->setToolTip(tr("Revert all pages and rotations to their original state"));
        m_btnReset->setCursor(Qt::PointingHandCursor);
        connect(m_btnReset, &QPushButton::clicked, this, &PdfPageOrganizerWidget::onResetOrder);
        topLay->addWidget(m_btnReset);

        topLay->addStretch(1);

        m_lblTitle = new QLabel(tr("Tip: Drag thumbnails to reorder • Double-click to open page"), m_topBar);
        m_lblTitle->setStyleSheet("font-size: 11px; color: #94a3b8; font-weight: 500;");
        topLay->addWidget(m_lblTitle);

        mainLay->addWidget(m_topBar);
    }

    // Central Panoramic Thumbnail Grid
    m_listWidget = new PdfOrganizerListWidget(this);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: palette(base);"
        "  color: palette(text);"
        "  border: none;"
        "  padding: 12px;"
        "}"
        "QListWidget::item {"
        "  background-color: palette(alternate-base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 8px;"
        "  margin: 6px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 2px solid palette(highlight);"
        "}"
        "QListWidget::item:hover:!selected {"
        "  border: 1px solid palette(highlight);"
        "}"
    );

    connect(m_listWidget, &PdfOrganizerListWidget::pagesReordered, this, [this]() {
        m_isModified = true;
        updateAllItemLabels(true);
        emit documentStructureChanged();
    });
    connect(m_listWidget, &PdfOrganizerListWidget::escapePressed, this, &PdfPageOrganizerWidget::backRequested);
    connect(m_listWidget, &PdfOrganizerListWidget::deletePressed, this, &PdfPageOrganizerWidget::deleteSelectedPages);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &PdfPageOrganizerWidget::onSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
        if (it) {
            emit pageDoubleClicked(m_listWidget->row(it));
        }
    });

    mainLay->addWidget(m_listWidget, 1);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(4);
    mainLay->addWidget(m_progressBar);

    // Bottom Status Bar
    m_lblStats = new QLabel(this);
    m_lblStats->setStyleSheet("font-size: 11px; color: #64748b; font-weight: 500; padding: 2px 4px;");
    if (!m_showInternalToolbar) {
        m_lblStats->hide();
    }
    mainLay->addWidget(m_lblStats);

    onSelectionChanged();
}

void PdfPageOrganizerWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit backRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

int PdfPageOrganizerWidget::selectedPageIndex() const
{
    const auto selected = m_listWidget->selectedItems();
    if (!selected.isEmpty()) {
        return m_listWidget->row(selected.first());
    }
    return 0;
}

void PdfPageOrganizerWidget::loadDocument(Poppler::Document *doc, const QString &sourcePath, int initialPageIndex)
{
    m_sourcePath = sourcePath;
    m_isModified = false;
    m_extraDocs.clear();
    m_listWidget->clear();

    if (!doc || doc->isLocked()) {
        return;
    }

    doc->setRenderHint(Poppler::Document::Antialiasing, true);
    doc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    int count = doc->numPages();
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, count);

    for (int i = 0; i < count; ++i) {
        m_progressBar->setValue(i + 1);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        std::unique_ptr<Poppler::Page> page(doc->page(i));
        if (!page) continue;

        QImage img = page->renderToImage(72.0, 72.0);
        if (img.isNull()) continue;

        auto *item = new QListWidgetItem(m_listWidget);
        item->setData(ROLE_SOURCE_PATH, m_sourcePath);
        item->setData(ROLE_ORIGINAL_PAGE, i + 1);
        item->setData(ROLE_ROTATION_DEG, 0);
        item->setData(ROLE_BASE_IMAGE, QVariant::fromValue(img));
        item->setData(ROLE_IS_BLANK, false);
        item->setData(ROLE_SOURCE_LABEL, QString());
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

        updateItemVisuals(item);
    }

    m_progressBar->setVisible(false);
    updateAllItemLabels();

    if (m_listWidget->count() > 0) {
        int targetIdx = qBound(0, initialPageIndex, m_listWidget->count() - 1);
        m_listWidget->setCurrentRow(targetIdx);
        m_listWidget->scrollToItem(m_listWidget->item(targetIdx));
    }
}

void PdfPageOrganizerWidget::updateItemVisuals(QListWidgetItem *item, bool updatePixmap)
{
    if (!item) return;

    int origPage = item->data(ROLE_ORIGINAL_PAGE).toInt();
    int rot = item->data(ROLE_ROTATION_DEG).toInt();
    bool isBlank = item->data(ROLE_IS_BLANK).toBool();
    QString srcLabel = item->data(ROLE_SOURCE_LABEL).toString();
    int currentPos = m_listWidget->row(item) + 1;

    if (updatePixmap) {
        QImage baseImg = item->data(ROLE_BASE_IMAGE).value<QImage>();
        QImage displayImg = baseImg;
        if (rot != 0) {
            QTransform t;
            t.rotate(rot);
            displayImg = baseImg.transformed(t, Qt::SmoothTransformation);
        }

        // Scale preview thumbnail smoothly
        QPixmap pix = QPixmap::fromImage(
            displayImg.scaled(150, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        item->setIcon(QIcon(pix));
    }

    if (isBlank) {
        item->setText(rot == 0 ? tr("#%1 • Blank").arg(currentPos)
                               : tr("#%1 • Blank (%2°)").arg(currentPos).arg(rot));
        item->setToolTip(tr("Page #%1 (Empty inserted page)").arg(currentPos));
    } else if (!srcLabel.isEmpty()) {
        item->setText(rot == 0 ? tr("#%1 • %2 (p.%3)").arg(currentPos).arg(srcLabel).arg(origPage)
                               : tr("#%1 • %2 (p.%3, %4°)").arg(currentPos).arg(srcLabel).arg(origPage).arg(rot));
        item->setToolTip(tr("Page #%1 — Source: %2 (Original page %3)").arg(currentPos).arg(srcLabel).arg(origPage));
    } else {
        item->setText(rot == 0 ? tr("#%1 • Page %2").arg(currentPos).arg(origPage)
                               : tr("#%1 • Page %2 (%3°)").arg(currentPos).arg(origPage).arg(rot));
        item->setToolTip(tr("Page #%1 (Original page %2)").arg(currentPos).arg(origPage));
    }
}

void PdfPageOrganizerWidget::updateAllItemLabels(bool labelsOnly)
{
    const int count = m_listWidget->count();
    for (int i = 0; i < count; ++i) {
        updateItemVisuals(m_listWidget->item(i), !labelsOnly);
    }
    updateStatsLabel();
}

QImage PdfPageOrganizerWidget::createBlankPageThumbnail()
{
    QImage img(120, 160, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(255, 255, 255));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPen borderPen(QColor(203, 213, 225), 1, Qt::DashLine);
    p.setPen(borderPen);
    p.drawRect(3, 3, 113, 153);

    QFont font = p.font();
    font.setPointSize(9);
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor(148, 163, 184));
    p.drawText(QRectF(3, 3, 113, 153), Qt::AlignCenter, tr("[ Blank ]"));
    p.end();
    return img;
}

void PdfPageOrganizerWidget::onInsertBlankPage()
{
    int insertRow = m_listWidget->count();
    const auto selected = m_listWidget->selectedItems();
    if (!selected.isEmpty()) {
        insertRow = m_listWidget->row(selected.last()) + 1;
    }

    auto *item = new QListWidgetItem();
    item->setData(ROLE_SOURCE_PATH, QString());
    item->setData(ROLE_ORIGINAL_PAGE, 1);
    item->setData(ROLE_ROTATION_DEG, 0);
    item->setData(ROLE_BASE_IMAGE, QVariant::fromValue(createBlankPageThumbnail()));
    item->setData(ROLE_IS_BLANK, true);
    item->setData(ROLE_SOURCE_LABEL, QString());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

    updateItemVisuals(item);
    m_listWidget->insertItem(insertRow, item);
    m_listWidget->clearSelection();
    item->setSelected(true);
    m_isModified = true;
    updateAllItemLabels();
    m_listWidget->doItemsLayout();
    m_listWidget->scrollToItem(item);
    emit documentStructureChanged();
}

void PdfPageOrganizerWidget::onAddFromPdf()
{
    QString initialDir = QFileInfo(m_sourcePath).canonicalPath();
    if (initialDir.isEmpty() || !QDir(initialDir).exists()) {
        initialDir = QDir::homePath();
    }

    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select PDF Documents to Insert"),
        initialDir,
        tr("PDF Documents (*.pdf)")
    );
    if (files.isEmpty()) return;

    int insertRow = m_listWidget->count();
    const auto selected = m_listWidget->selectedItems();
    if (!selected.isEmpty()) {
        insertRow = m_listWidget->row(selected.last()) + 1;
    }

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);

    QListWidgetItem *firstAdded = nullptr;

    for (const QString &filePath : files) {
        if (!QFile::exists(filePath)) continue;

        auto doc = std::unique_ptr<Poppler::Document>(Poppler::Document::load(filePath));
        if (!doc || doc->isLocked()) {
            QMessageBox::warning(
                this,
                tr("Cannot Load PDF"),
                tr("Could not read document or document is password protected:<br><b>%1</b>")
                    .arg(QFileInfo(filePath).fileName().toHtmlEscaped())
            );
            continue;
        }

        doc->setRenderHint(Poppler::Document::Antialiasing, true);
        doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
        int numPages = doc->numPages();
        QString fileName = QFileInfo(filePath).fileName();

        for (int p = 0; p < numPages; ++p) {
            std::unique_ptr<Poppler::Page> page(doc->page(p));
            if (!page) continue;

            QImage img = page->renderToImage(72.0, 72.0);
            if (img.isNull()) continue;

            auto *item = new QListWidgetItem();
            item->setData(ROLE_SOURCE_PATH, filePath);
            item->setData(ROLE_ORIGINAL_PAGE, p + 1);
            item->setData(ROLE_ROTATION_DEG, 0);
            item->setData(ROLE_BASE_IMAGE, QVariant::fromValue(img));
            item->setData(ROLE_IS_BLANK, false);
            item->setData(ROLE_SOURCE_LABEL, fileName);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

            updateItemVisuals(item);
            m_listWidget->insertItem(insertRow, item);
            if (!firstAdded) firstAdded = item;
            insertRow++;
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        m_extraDocs.push_back(std::move(doc));
    }

    m_progressBar->setVisible(false);
    m_listWidget->doItemsLayout();
    if (firstAdded) {
        m_listWidget->clearSelection();
        firstAdded->setSelected(true);
        m_listWidget->scrollToItem(firstAdded);
    }
    m_isModified = true;
    updateAllItemLabels();
    emit documentStructureChanged();
}

void PdfPageOrganizerWidget::updateStatsLabel()
{
    int total = m_listWidget ? m_listWidget->count() : 0;
    const auto selected = m_listWidget ? m_listWidget->selectedItems() : QList<QListWidgetItem*>();
    int selectedCount = selected.count();

    if (m_lblStats) {
        m_lblStats->setText(
            tr("Total pages: %1 | Selected: %2").arg(total).arg(selectedCount)
        );
    }

    bool hasSelection = selectedCount > 0;

    if (m_btnRotate) m_btnRotate->setEnabled(hasSelection);
    if (m_btnDelete) m_btnDelete->setEnabled(hasSelection && total > 1);

    if (m_btnInsertBlank) m_btnInsertBlank->setEnabled(true);
    if (m_btnAddPdf) m_btnAddPdf->setEnabled(true);
    if (m_btnReverse) m_btnReverse->setEnabled(total > 1);
}

void PdfPageOrganizerWidget::onSelectionChanged()
{
    updateStatsLabel();
}

void PdfPageOrganizerWidget::onRotate()
{
    const auto items = m_listWidget->selectedItems();
    for (auto *item : items) {
        int currentRot = item->data(ROLE_ROTATION_DEG).toInt();
        int newRot = (currentRot + 90) % 360;
        item->setData(ROLE_ROTATION_DEG, newRot);
        updateItemVisuals(item, true);
    }
    m_isModified = true;
    emit documentStructureChanged();
}

void PdfPageOrganizerWidget::onDeleteSelected()
{
    const auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    if (items.count() >= m_listWidget->count()) {
        QMessageBox::warning(
            this,
            tr("Action Denied"),
            tr("A PDF document must contain at least one page. You cannot delete all pages.")
        );
        return;
    }

    for (auto *item : items) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
    }

    m_isModified = true;
    m_listWidget->doItemsLayout();
    updateAllItemLabels();
    emit documentStructureChanged();
}

void PdfPageOrganizerWidget::onReverseOrder()
{
    int count = m_listWidget->count();
    if (count <= 1) return;

    QList<QListWidgetItem*> items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        items.append(m_listWidget->takeItem(0));
    }

    for (int i = items.count() - 1; i >= 0; --i) {
        m_listWidget->addItem(items.at(i));
    }

    m_isModified = true;
    m_listWidget->doItemsLayout();
    updateAllItemLabels();
    emit documentStructureChanged();
}

void PdfPageOrganizerWidget::onResetOrder()
{
    QMessageBox::StandardButton ask = QMessageBox::question(
        this,
        tr("Reset Changes"),
        tr("Are you sure you want to revert all changes, reordering, and rotations?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (ask == QMessageBox::Yes) {
        std::unique_ptr<Poppler::Document> origDoc(Poppler::Document::load(m_sourcePath));
        if (origDoc && !origDoc->isLocked()) {
            loadDocument(origDoc.get(), m_sourcePath);
            m_isModified = true;
            emit documentStructureChanged();
        }
    }
}

bool PdfPageOrganizerWidget::applyChanges(QString &outTempPath)
{
    if (m_listWidget->count() == 0) {
        return false;
    }

    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_organized_XXXXXX.pdf");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        return false;
    }
    QString tempPath = tempFile.fileName();
    tempFile.close();
    QFile::remove(tempPath);

    if (executeQpdfReorder(tempPath)) {
        outTempPath = tempPath;
        return true;
    }
    if (QFile::exists(tempPath)) {
        QFile::remove(tempPath);
    }
    return false;
}

bool PdfPageOrganizerWidget::executeQpdfReorder(const QString &destinationPath)
{
    int count = m_listWidget->count();
    if (count == 0) {
        QMessageBox::warning(this, tr("Empty Document"), tr("Cannot save a PDF with no pages."));
        return false;
    }

    QString qpdfExe = BuildConfig::qpdfExecutable();
    if (qpdfExe.isEmpty()) {
        QMessageBox::StandardButton ask = QMessageBox::question(
            this,
            tr("QPDF Not Installed"),
            tr("Page reorganization requires the system utility <b>qpdf</b>.<br>Would you like to install it now?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (ask == QMessageBox::Yes) {
            if (PackageManagerHelper::installComponentInteractive("qpdf", this)) {
                qpdfExe = BuildConfig::qpdfExecutable();
            }
        }
        if (qpdfExe.isEmpty()) return false;
    } else {
        FallbackNoticeDialog::checkAndPrompt("qpdf", tr("QPDF Engine"), qpdfExe, this);
        qpdfExe = BuildConfig::qpdfExecutable();
    }

    // Check if any blank pages are present; if so, create a 1-page blank PDF temporary file
    bool hasBlank = false;
    for (int i = 0; i < count; ++i) {
        if (m_listWidget->item(i)->data(ROLE_IS_BLANK).toBool()) {
            hasBlank = true;
            break;
        }
    }

    QString blankPdfPath;
    std::unique_ptr<QTemporaryFile> tempBlankFile;
    if (hasBlank) {
        tempBlankFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/bubez_blank_XXXXXX.pdf");
        tempBlankFile->setAutoRemove(true);
        if (tempBlankFile->open()) {
            blankPdfPath = tempBlankFile->fileName();
            tempBlankFile->close();
            QPdfWriter writer(blankPdfPath);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setResolution(72);
            QPainter p(&writer);
            p.end();
        }
    }

    // Build QPDF page specification:
    // qpdf --empty --pages <file1> <range1> <file2> <range2> ... -- [rotate args] destinationPath
    QStringList pagesArgs;
    QStringList rotateArgs;

    QString currentSource;
    QStringList currentRanges;

    auto flushRange = [&]() {
        if (!currentSource.isEmpty() && !currentRanges.isEmpty()) {
            pagesArgs.append(currentSource);
            pagesArgs.append(currentRanges.join(","));
            currentRanges.clear();
            currentSource.clear();
        }
    };

    for (int i = 0; i < count; ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        bool isBlank = item->data(ROLE_IS_BLANK).toBool();
        QString srcPath = isBlank ? blankPdfPath : item->data(ROLE_SOURCE_PATH).toString();
        if (srcPath.isEmpty()) srcPath = m_sourcePath;
        int origPage = isBlank ? 1 : item->data(ROLE_ORIGINAL_PAGE).toInt();
        int rot = item->data(ROLE_ROTATION_DEG).toInt();

        if (srcPath != currentSource) {
            flushRange();
            currentSource = srcPath;
        }
        currentRanges.append(QString::number(origPage));

        if (rot != 0) {
            rotateArgs.append(QString("--rotate=+%1:%2").arg(rot).arg(i + 1));
        }
    }
    flushRange();

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QProcess proc;
    QStringList args;
    args << "--empty" << "--pages";
    args.append(pagesArgs);
    args << "--";
    args.append(rotateArgs);
    args << destinationPath;

    proc.start(qpdfExe, args);
    proc.waitForFinished(-1);

    m_progressBar->setVisible(false);

    int exitCode = proc.exitCode();
    QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    if ((exitCode == 0 || exitCode == 3) && QFile::exists(destinationPath) && QFileInfo(destinationPath).size() > 0) {
        return true;
    }

    QMessageBox::critical(
        this,
        tr("QPDF Processing Error"),
        tr("<p>Failed to generate the organized PDF.</p>"
           "<p><i>Details:</i><br><code>%1</code></p>").arg(stdErr.isEmpty() ? tr("Unknown error") : stdErr.toHtmlEscaped())
    );
    return false;
}

/* ========================================================================= */
/*                      PdfPageOrganizerDialog Implementation               */
/* ========================================================================= */

PdfPageOrganizerDialog::PdfPageOrganizerDialog(const QString &pdfPath, QWidget *parent)
    : QDialog(parent)
    , m_organizerWidget(new PdfPageOrganizerWidget(this, true))
    , m_outPath()
{
    setWindowTitle(tr("PDF Page Organizer & Reorderer"));
    resize(920, 640);
    setMinimumSize(680, 480);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_organizerWidget);

    std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(pdfPath));
    if (doc && !doc->isLocked()) {
        m_organizerWidget->loadDocument(doc.get(), pdfPath);
    }

    connect(m_organizerWidget, &PdfPageOrganizerWidget::backRequested, this, [this]() {
        if (m_organizerWidget->isModified()) {
            if (m_organizerWidget->applyChanges(m_outPath)) {
                emit fileOrganized(m_outPath);
                accept();
                return;
            }
        }
        reject();
    });
}
