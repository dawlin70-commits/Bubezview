/**
 * @file pdf_base_viewer.cpp
 * @brief Implementation of PdfBaseViewer lightweight native PDF reading engine.
 */

#include "pdf_base_viewer.h"
#include "core/services/theme_manager.h"

#include <poppler/qt6/poppler-qt6.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QPrintDialog>
#include <QPrinter>
#include <QFileInfo>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QPdfWriter>
#include <QPageSize>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDebug>
#include <cmath>

// ============================================================================
#include "pdf_base_canvas.h"
// PdfBaseViewer Main Engine Implementation
// ============================================================================
PdfBaseViewer::PdfBaseViewer(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    // 1. Table of Contents (TOC) Sidebar
    m_tocContainer = new QWidget(m_splitter);
    QVBoxLayout *tocLayout = new QVBoxLayout(m_tocContainer);
    tocLayout->setContentsMargins(6, 6, 6, 6);
    tocLayout->setSpacing(6);

    QHBoxLayout *tocHeader = new QHBoxLayout();
    m_lblTocTitle = new QLabel(tr("📑 Document Outline"), m_tocContainer);
    m_lblTocTitle->setStyleSheet("font-weight: 700; font-size: 11px;");
    tocHeader->addWidget(m_lblTocTitle);
    tocHeader->addStretch(1);

    m_btnCloseToc = new QPushButton(QStringLiteral("✕"), m_tocContainer);
    m_btnCloseToc->setFixedSize(22, 22);
    m_btnCloseToc->setFlat(true);
    m_btnCloseToc->setCursor(Qt::PointingHandCursor);
    connect(m_btnCloseToc, &QPushButton::clicked, this, &PdfBaseViewer::toggleTocSidebar);
    tocHeader->addWidget(m_btnCloseToc);
    tocLayout->addLayout(tocHeader);

    m_tocView = new QTreeView(m_tocContainer);
    m_tocModel = new QStandardItemModel(this);
    m_tocView->setModel(m_tocModel);
    m_tocView->setHeaderHidden(true);
    m_tocView->setAnimated(true);
    m_tocView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_tocView, &QTreeView::clicked, this, &PdfBaseViewer::onTocItemActivated);
    tocLayout->addWidget(m_tocView);

    m_splitter->addWidget(m_tocContainer);
    m_tocContainer->hide();

    // 2. Central Scroll Area & Canvas
    m_scrollArea = new QScrollArea(m_splitter);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setMouseTracking(true);

    m_canvas = new PdfBaseCanvas(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);
    m_splitter->addWidget(m_scrollArea);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(m_splitter);

    // Canvas signals
    connect(m_canvas, &PdfBaseCanvas::selectionChanged, this, &PdfBaseViewer::onCanvasSelectionChanged);
    connect(m_canvas, &PdfBaseCanvas::zoomWheelRequested, this, [this](double factor) {
        setZoomFactor(m_zoomFactor * factor);
    });
    connect(m_canvas, &PdfBaseCanvas::showInfoRequested, this, &PdfBaseViewer::showDocumentInfo);
    connect(m_canvas, &PdfBaseCanvas::printRequested, this, &PdfBaseViewer::printDocument);
    connect(m_canvas, &PdfBaseCanvas::rotateRequested, this, &PdfBaseViewer::rotateClockwise);
    connect(m_canvas, &PdfBaseCanvas::fitWidthRequested, this, &PdfBaseViewer::fitToWidth);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (m_canvas && m_pageCount > 0) {
            int pageIdx = m_canvas->pageIndexAtY(value + 40);
            if (pageIdx != m_currentPage) {
                onCanvasPageScrolled(pageIdx);
            }
        }
    });

    // 3. Floating HUD Capsule Pill (Overlay)
    m_hudPill = new QWidget(this);
    m_hudPill->setObjectName(QStringLiteral("PdfBaseHudPill"));
    m_hudPill->setFixedHeight(36);
    m_hudPill->setAttribute(Qt::WA_Hover, true);

    QHBoxLayout *hudLayout = new QHBoxLayout(m_hudPill);
    hudLayout->setContentsMargins(10, 3, 10, 3);
    hudLayout->setSpacing(6);

    auto createPillBtn = [this](const QString &text, const QString &tip) -> QToolButton* {
        auto *b = new QToolButton(m_hudPill);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };

    m_btnPrev = createPillBtn(QStringLiteral("◀"), tr("Previous Page (Page Up / Left Arrow)"));
    connect(m_btnPrev, &QToolButton::clicked, this, &PdfBaseViewer::prevPage);

    m_lblPageText = new QLabel(tr("Page"), m_hudPill);
    m_spinPage = new QSpinBox(m_hudPill);
    m_spinPage->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spinPage->setRange(1, 1);
    m_spinPage->setValue(1);
    m_spinPage->setAlignment(Qt::AlignCenter);
    m_spinPage->setFixedWidth(42);
    m_spinPage->setFixedHeight(24);
    connect(m_spinPage, &QSpinBox::valueChanged, this, [this](int val) {
        if (val - 1 != m_currentPage) {
            scrollToPage(val - 1);
        }
    });

    m_lblPageCount = new QLabel(tr("of 0"), m_hudPill);

    m_btnNext = createPillBtn(QStringLiteral("▶"), tr("Next Page (Page Down / Right Arrow)"));
    connect(m_btnNext, &QToolButton::clicked, this, &PdfBaseViewer::nextPage);

    auto addSep = [hudLayout, this]() {
        auto *sep = new QFrame(m_hudPill);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        sep->setStyleSheet(QStringLiteral("color: rgba(148, 163, 184, 0.4); margin: 3px 2px;"));
        hudLayout->addWidget(sep);
    };

    m_btnZoomOut = createPillBtn(QStringLiteral("➖"), tr("Zoom Out (-)"));
    connect(m_btnZoomOut, &QToolButton::clicked, this, &PdfBaseViewer::zoomOut);

    m_btnZoomReset = createPillBtn(QStringLiteral("1:1"), tr("Reset Zoom to 100%"));
    connect(m_btnZoomReset, &QToolButton::clicked, this, &PdfBaseViewer::resetZoom);

    m_btnZoomIn = createPillBtn(QStringLiteral("➕"), tr("Zoom In (+)"));
    connect(m_btnZoomIn, &QToolButton::clicked, this, &PdfBaseViewer::zoomIn);

    m_btnFitWidth = createPillBtn(QStringLiteral("↔"), tr("Fit to Viewport Width"));
    connect(m_btnFitWidth, &QToolButton::clicked, this, &PdfBaseViewer::fitToWidth);

    m_btnRotate = createPillBtn(QStringLiteral("⟳"), tr("Rotate 90° Clockwise"));
    connect(m_btnRotate, &QToolButton::clicked, this, &PdfBaseViewer::rotateClockwise);

    m_btnInfo = createPillBtn(QStringLiteral("ℹ️"), tr("Document Metadata & Information"));
    connect(m_btnInfo, &QToolButton::clicked, this, &PdfBaseViewer::showDocumentInfo);

    m_btnPrint = createPillBtn(QStringLiteral("🖨️"), tr("Print Document... (Ctrl+P)"));
    connect(m_btnPrint, &QToolButton::clicked, this, &PdfBaseViewer::printDocument);

    m_btnToc = createPillBtn(QStringLiteral("📑"), tr("Toggle Table of Contents"));
    connect(m_btnToc, &QToolButton::clicked, this, &PdfBaseViewer::toggleTocSidebar);
    m_btnToc->setVisible(false);

    m_btnStudio = new QPushButton(tr("✏️ Studio"), m_hudPill);
    m_btnStudio->setToolTip(tr("Open PDF Studio for signatures, page organizing, and repairs"));
    m_btnStudio->setCursor(Qt::PointingHandCursor);
    m_btnStudio->setFixedHeight(26);
    m_btnStudio->setStyleSheet(
        "QPushButton { background-color: rgba(56, 189, 248, 0.15); color: #0284c7; "
        "border: 1px solid rgba(56, 189, 248, 0.35); border-radius: 4px; padding: 2px 10px; font-weight: 600; font-size: 11px; }"
        "QPushButton:hover { background-color: #0284c7; color: #ffffff; }"
    );
    connect(m_btnStudio, &QPushButton::clicked, this, &PdfBaseViewer::requestStudio);

    hudLayout->addWidget(m_btnPrev);
    hudLayout->addWidget(m_lblPageText);
    hudLayout->addWidget(m_spinPage);
    hudLayout->addWidget(m_lblPageCount);
    hudLayout->addWidget(m_btnNext);
    addSep();
    hudLayout->addWidget(m_btnZoomOut);
    hudLayout->addWidget(m_btnZoomReset);
    hudLayout->addWidget(m_btnZoomIn);
    hudLayout->addWidget(m_btnFitWidth);
    addSep();
    hudLayout->addWidget(m_btnRotate);
    hudLayout->addWidget(m_btnInfo);
    hudLayout->addWidget(m_btnPrint);
    hudLayout->addWidget(m_btnToc);
    addSep();
    hudLayout->addWidget(m_btnStudio);

    m_hudOpacity = new QGraphicsOpacityEffect(m_hudPill);
    m_hudOpacity->setOpacity(0.0);
    m_hudPill->setGraphicsEffect(m_hudOpacity);

    m_hudAnim = new QPropertyAnimation(m_hudOpacity, "opacity", this);
    m_hudAnim->setDuration(180);

    m_hudTimer = new QTimer(this);
    m_hudTimer->setSingleShot(true);
    m_hudTimer->setInterval(2500);
    connect(m_hudTimer, &QTimer::timeout, this, &PdfBaseViewer::hideHud);

    installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);
    m_canvas->installEventFilter(this);

    updateTheme();
}

PdfBaseViewer::~PdfBaseViewer()
{
    clear();
}

bool PdfBaseViewer::loadFile(const QString &filePath)
{
    clear();
    m_filePath = filePath;

    m_doc = Poppler::Document::load(filePath);
    if (!m_doc || m_doc->isLocked()) {
        clear();
        return false;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = 0;
    m_rotationDegrees = 0;
    m_zoomFactor = 1.0;
    m_fitToWidthMode = true;

    populateOutline();
    fitToWidth();
    updateNavControls();

    showHud(true);
    emit pageChanged(m_currentPage + 1, m_pageCount);
    return true;
}

void PdfBaseViewer::clear()
{
    m_filePath.clear();
    m_doc.reset();
    m_currentPage = 0;
    m_pageCount = 0;
    m_hasOutline = false;
    if (m_tocModel) m_tocModel->clear();
    if (m_tocContainer) m_tocContainer->hide();
    if (m_canvas) m_canvas->clear();
    updateNavControls();
    hideHud();
}

QSize PdfBaseViewer::pageSize() const
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) {
        return QSize();
    }
    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    return p ? p->pageSize() : QSize();
}

void PdfBaseViewer::nextPage()
{
    if (m_currentPage + 1 < m_pageCount) {
        scrollToPage(m_currentPage + 1);
    }
}

void PdfBaseViewer::prevPage()
{
    if (m_currentPage > 0) {
        scrollToPage(m_currentPage - 1);
    }
}

void PdfBaseViewer::goToPage(int page)
{
    scrollToPage(page);
}

void PdfBaseViewer::scrollToPage(int page)
{
    if (!m_doc || page < 0 || page >= m_pageCount) return;

    m_currentPage = page;
    QRect pRect = m_canvas->pageRect(page);
    if (!pRect.isEmpty() && m_scrollArea) {
        m_scrollArea->verticalScrollBar()->setValue(pRect.top() - 10);
    }
    updateNavControls();
    showHud(true);
    emit pageChanged(m_currentPage + 1, m_pageCount);
}

void PdfBaseViewer::zoomIn()
{
    m_fitToWidthMode = false;
    setZoomFactor(m_zoomFactor * 1.25);
}

void PdfBaseViewer::zoomOut()
{
    m_fitToWidthMode = false;
    setZoomFactor(m_zoomFactor / 1.25);
}

void PdfBaseViewer::resetZoom()
{
    m_fitToWidthMode = false;
    setZoomFactor(1.0);
}

void PdfBaseViewer::fitToWidth()
{
    if (!m_doc || m_pageCount <= 0 || !m_scrollArea) return;

    m_fitToWidthMode = true;
    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    if (!p) return;

    QSizeF pSize = p->pageSize();
    if (m_rotationDegrees == 90 || m_rotationDegrees == 270) {
        pSize.transpose();
    }

    int viewW = std::max(200, m_scrollArea->viewport()->width() - 48);
    double targetZoom = (double)viewW / std::max(10.0, pSize.width());
    setZoomFactor(std::clamp(targetZoom, 0.25, 4.0));
}

void PdfBaseViewer::fitToPage()
{
    if (!m_doc || m_pageCount <= 0 || !m_scrollArea) return;

    m_fitToWidthMode = false;
    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    if (!p) return;

    QSizeF pSize = p->pageSize();
    if (m_rotationDegrees == 90 || m_rotationDegrees == 270) {
        pSize.transpose();
    }

    int viewW = std::max(200, m_scrollArea->viewport()->width() - 48);
    int viewH = std::max(200, m_scrollArea->viewport()->height() - 48);

    double zoomW = (double)viewW / std::max(10.0, pSize.width());
    double zoomH = (double)viewH / std::max(10.0, pSize.height());
    setZoomFactor(std::clamp(std::min(zoomW, zoomH), 0.2, 3.5));
}

void PdfBaseViewer::setZoomFactor(double factor)
{
    m_zoomFactor = std::clamp(factor, 0.15, 6.0);
    if (m_canvas && m_scrollArea) {
        int availW = m_scrollArea->viewport()->width();
        m_canvas->setDocument(m_doc.get(), m_zoomFactor, m_rotationDegrees, availW);
    }
    showHud(true);
}

void PdfBaseViewer::rotateClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 90) % 360;
    if (m_canvas && m_scrollArea) {
        int availW = m_scrollArea->viewport()->width();
        m_canvas->setDocument(m_doc.get(), m_zoomFactor, m_rotationDegrees, availW);
    }
    showHud(true);
}

void PdfBaseViewer::rotateCounterClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 270) % 360;
    if (m_canvas && m_scrollArea) {
        int availW = m_scrollArea->viewport()->width();
        m_canvas->setDocument(m_doc.get(), m_zoomFactor, m_rotationDegrees, availW);
    }
    showHud(true);
}

void PdfBaseViewer::toggleTocSidebar()
{
    if (!m_hasOutline || !m_tocContainer) return;
    m_tocContainer->setVisible(!m_tocContainer->isVisible());
}

bool PdfBaseViewer::isTocVisible() const
{
    return m_tocContainer && m_tocContainer->isVisible();
}

bool PdfBaseViewer::findNext(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (!m_doc || text.isEmpty() || m_pageCount <= 0) return false;

    Poppler::Page::SearchFlags flags = Poppler::Page::NoSearchFlags;
    if (!matchCase) flags |= Poppler::Page::IgnoreCase;

    int startPage = (m_currentPage + 1) % m_pageCount;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage + step) % m_pageCount;
        std::unique_ptr<Poppler::Page> page(m_doc->page(pageIdx));
        if (page) {
            QList<QRectF> matches = page->search(text, flags);
            if (!matches.isEmpty()) {
                if (wrapped && pageIdx <= m_currentPage && step > 0) {
                    *wrapped = true;
                }
                m_canvas->setSearchHighlight(pageIdx, matches.first());
                scrollToPage(pageIdx);
                return true;
            }
        }
    }
    return false;
}

bool PdfBaseViewer::findPrev(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (!m_doc || text.isEmpty() || m_pageCount <= 0) return false;

    Poppler::Page::SearchFlags flags = Poppler::Page::NoSearchFlags;
    if (!matchCase) flags |= Poppler::Page::IgnoreCase;

    int startPage = (m_currentPage - 1 + m_pageCount) % m_pageCount;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage - step + m_pageCount) % m_pageCount;
        std::unique_ptr<Poppler::Page> page(m_doc->page(pageIdx));
        if (page) {
            QList<QRectF> matches = page->search(text, flags);
            if (!matches.isEmpty()) {
                if (wrapped && pageIdx >= m_currentPage && step > 0) {
                    *wrapped = true;
                }
                m_canvas->setSearchHighlight(pageIdx, matches.last());
                scrollToPage(pageIdx);
                return true;
            }
        }
    }
    return false;
}

void PdfBaseViewer::clearSearch()
{
    if (m_canvas) {
        m_canvas->clearSearchHighlight();
    }
}

bool PdfBaseViewer::hasTextSelection() const
{
    return m_canvas && m_canvas->hasTextSelection();
}

QString PdfBaseViewer::selectedText() const
{
    return m_canvas ? m_canvas->selectedText() : QString();
}

void PdfBaseViewer::copySelectedText()
{
    QString txt = selectedText();
    if (!txt.isEmpty()) {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(txt);
        }
    }
}

QString PdfBaseViewer::extractAllText() const
{
    if (!m_doc || m_pageCount <= 0) return QString();

    QString result;
    for (int i = 0; i < m_pageCount; ++i) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(i));
        if (p) {
            result += p->text(QRectF()) + QStringLiteral("\n\n");
        }
    }
    return result;
}

QImage PdfBaseViewer::renderPageForOcr(int pageIndex, int dpi) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return QImage();

    std::unique_ptr<Poppler::Page> page(m_doc->page(pageIndex));
    if (!page) return QImage();

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    return page->renderToImage(dpi, dpi, -1, -1, -1, -1, r);
}

QImage PdfBaseViewer::renderCurrentPageForOcr(int dpi) const
{
    return renderPageForOcr(m_currentPage, dpi);
}

void PdfBaseViewer::showDocumentInfo()
{
    if (!m_doc) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Document Metadata & Information"));
    dlg.resize(480, 420);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();
    form->setSpacing(8);

    auto addRow = [form](const QString &label, const QString &val) {
        QLabel *v = new QLabel(val.isEmpty() ? QStringLiteral("—") : val);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setWordWrap(true);
        form->addRow(QString("<b>%1:</b>").arg(label), v);
    };

    QFileInfo fi(m_filePath);
    addRow(tr("File Name"), fi.fileName());
    addRow(tr("Location"), fi.absolutePath());
    addRow(tr("File Size"), QString("%1 KB").arg(fi.size() / 1024));
    addRow(tr("Total Pages"), QString::number(m_pageCount));

    std::unique_ptr<Poppler::Page> p(m_doc->page(m_currentPage));
    if (p) {
        QSizeF sz = p->pageSize();
        double wMm = sz.width() * 25.4 / 72.0;
        double hMm = sz.height() * 25.4 / 72.0;
        addRow(tr("Page Dimensions"), QString("%1 × %2 mm (%3 × %4 pt)")
            .arg(wMm, 0, 'f', 1).arg(hMm, 0, 'f', 1)
            .arg(sz.width(), 0, 'f', 0).arg(sz.height(), 0, 'f', 0));
    }

    addRow(tr("Title"), m_doc->info("Title"));
    addRow(tr("Author"), m_doc->info("Author"));
    addRow(tr("Subject"), m_doc->info("Subject"));
    addRow(tr("Keywords"), m_doc->info("Keywords"));
    addRow(tr("Creator"), m_doc->info("Creator"));
    addRow(tr("Producer"), m_doc->info("Producer"));

    QDateTime cDate = m_doc->creationDate();
    if (cDate.isValid()) addRow(tr("Created"), cDate.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    QDateTime mDate = m_doc->modificationDate();
    if (mDate.isValid()) addRow(tr("Modified"), mDate.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    auto ver = m_doc->getPdfVersion();
    addRow(tr("PDF Version"), QString("v%1.%2").arg(ver.major).arg(ver.minor));
    addRow(tr("Fast Web View"), m_doc->isLinearized() ? tr("Yes (Linearized)") : tr("No"));
    addRow(tr("Encrypted"), m_doc->isEncrypted() ? tr("Yes (Password Protected)") : tr("No"));

    dlgLayout->addLayout(form);
    dlgLayout->addStretch(1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *btnClose = new QPushButton(tr("Close"), &dlg);
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(btnClose);
    dlgLayout->addLayout(btnRow);

    dlg.exec();
}

void PdfBaseViewer::printDocument()
{
    if (!m_doc || m_pageCount <= 0) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QFileInfo(m_filePath).fileName());

    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print Document"));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter(&printer);
    int fromPage = (printer.fromPage() > 0) ? printer.fromPage() - 1 : 0;
    int toPage = (printer.toPage() > 0 && printer.toPage() <= m_pageCount) ? printer.toPage() - 1 : m_pageCount - 1;

    for (int i = fromPage; i <= toPage; ++i) {
        if (i > fromPage) {
            printer.newPage();
        }
        std::unique_ptr<Poppler::Page> page(m_doc->page(i));
        if (!page) continue;

        QRect pageRect = printer.pageLayout().paintRectPixels(printer.resolution());
        double dpiX = (double)pageRect.width() / (page->pageSize().width() / 72.0);
        double dpiY = (double)pageRect.height() / (page->pageSize().height() / 72.0);
        double dpi = std::min(dpiX, dpiY);

        QImage img = page->renderToImage(dpi, dpi);
        if (!img.isNull()) {
            painter.drawImage(pageRect.topLeft(), img);
        }
    }
}

bool PdfBaseViewer::saveCurrentPageAsPdf(const QString &outputPath, QString *errorMsg)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) {
        if (errorMsg) *errorMsg = tr("No active page to export.");
        return false;
    }

    std::unique_ptr<Poppler::Page> page(m_doc->page(m_currentPage));
    if (!page) {
        if (errorMsg) *errorMsg = tr("Could not access current page.");
        return false;
    }

    QSizeF pSizePoints = page->pageSize();
    if (m_rotationDegrees == 90 || m_rotationDegrees == 270) {
        pSizePoints.transpose();
    }

    QPdfWriter writer(outputPath);
    writer.setPageSize(QPageSize(pSizePoints, QPageSize::Point));
    writer.setResolution(300);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (errorMsg) *errorMsg = tr("Failed to initialize PDF writer.");
        return false;
    }

    QImage img = renderCurrentPageForOcr(300);
    if (img.isNull()) {
        if (errorMsg) *errorMsg = tr("Failed to render page image.");
        return false;
    }

    painter.drawImage(writer.pageLayout().paintRectPixels(300), img);
    painter.end();
    return true;
}

bool PdfBaseViewer::saveCurrentPageAsImage(const QString &outputPath, const QString &format, int dpi, QString *errorMsg)
{
    QImage img = renderCurrentPageForOcr(dpi);
    if (img.isNull()) {
        if (errorMsg) *errorMsg = tr("Failed to render page.");
        return false;
    }
    bool ok = img.save(outputPath, format.toLatin1().constData());
    if (!ok && errorMsg) {
        *errorMsg = tr("Failed to write image file.");
    }
    return ok;
}

void PdfBaseViewer::updateTheme()
{
    bool isDark = ThemeManager::isDarkTheme();
    QString bgCanvas = isDark ? QStringLiteral("#0f172a") : QStringLiteral("#f1f5f9");
    QString bgHud = isDark ? QStringLiteral("rgba(15, 23, 42, 0.92)") : QStringLiteral("rgba(255, 255, 255, 0.95)");
    QString borderHud = isDark ? QStringLiteral("rgba(51, 65, 85, 0.8)") : QStringLiteral("rgba(203, 213, 225, 0.8)");
    QString textHud = isDark ? QStringLiteral("#f8fafc") : QStringLiteral("#0f172a");

    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }").arg(bgCanvas));
    }

    if (m_hudPill) {
        m_hudPill->setStyleSheet(QString(
            "QWidget#PdfBaseHudPill {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 18px;"
            "}"
            "QToolButton, QLabel {"
            "  color: %3;"
            "  border: none;"
            "  background: transparent;"
            "  font-size: 11px;"
            "}"
            "QToolButton:hover {"
            "  background-color: rgba(56, 189, 248, 0.20);"
            "  border-radius: 14px;"
            "}"
            "QSpinBox {"
            "  background-color: transparent;"
            "  color: %3;"
            "  border: 1px solid %2;"
            "  border-radius: 4px;"
            "  font-weight: 600;"
            "}"
        ).arg(bgHud, borderHud, textHud));
    }
}

void PdfBaseViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWidthMode) {
        fitToWidth();
    }
    if (m_hudPill) {
        m_hudPill->raise();
        int hudW = m_hudPill->sizeHint().width() + 10;
        int x = std::max(20, (width() - hudW) / 2);
        int y = height() - m_hudPill->height() - 20;
        m_hudPill->setGeometry(x, y, hudW, m_hudPill->height());
    }
}

bool PdfBaseViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Wheel) {
        showHud(true);
    }
    return QWidget::eventFilter(watched, event);
}

void PdfBaseViewer::onTocItemActivated(const QModelIndex &index)
{
    QVariant pageData = index.data(Qt::UserRole);
    if (pageData.isValid()) {
        int targetPage = pageData.toInt();
        if (targetPage >= 1 && targetPage <= m_pageCount) {
            scrollToPage(targetPage - 1);
        }
    }
}

void PdfBaseViewer::onCanvasPageScrolled(int pageIndex)
{
    if (pageIndex >= 0 && pageIndex < m_pageCount) {
        m_currentPage = pageIndex;
        updateNavControls();
        emit pageChanged(m_currentPage + 1, m_pageCount);
    }
}

void PdfBaseViewer::onCanvasSelectionChanged(bool hasSelection)
{
    emit selectionChanged(hasSelection);
}

void PdfBaseViewer::populateOutline()
{
    m_tocModel->clear();
    if (!m_doc) {
        m_hasOutline = false;
        m_btnToc->setVisible(false);
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    QVector<Poppler::OutlineItem> rootItems = m_doc->outline();
    if (rootItems.isEmpty()) {
        m_hasOutline = false;
        m_btnToc->setVisible(false);
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    m_hasOutline = true;
    m_btnToc->setVisible(true);
    buildOutlineBranch(m_tocModel->invisibleRootItem(), rootItems);
    emit outlineAvailabilityChanged(true);
}

void PdfBaseViewer::buildOutlineBranch(QStandardItem *parentItem, const QVector<Poppler::OutlineItem> &items)
{
    for (const auto &item : items) {
        if (item.isNull()) continue;

        QStandardItem *treeItem = new QStandardItem(item.name());
        auto dest = item.destination();
        if (dest) {
            treeItem->setData(dest->pageNumber(), Qt::UserRole);
        }
        parentItem->appendRow(treeItem);

        if (item.hasChildren()) {
            buildOutlineBranch(treeItem, item.children());
        }
    }
}

void PdfBaseViewer::updateNavControls()
{
    if (m_pageCount > 0) {
        QSignalBlocker b(m_spinPage);
        m_spinPage->setRange(1, m_pageCount);
        m_spinPage->setValue(m_currentPage + 1);
        m_lblPageCount->setText(QString("of %1").arg(m_pageCount));
        m_btnPrev->setEnabled(m_currentPage > 0);
        m_btnNext->setEnabled(m_currentPage + 1 < m_pageCount);
    } else {
        m_spinPage->setRange(1, 1);
        m_spinPage->setValue(1);
        m_lblPageCount->setText(QStringLiteral("of 0"));
        m_btnPrev->setEnabled(false);
        m_btnNext->setEnabled(false);
    }
}

void PdfBaseViewer::showHud(bool autoHide)
{
    if (!m_hudPill) return;

    m_hudPill->raise();
    int hudW = m_hudPill->sizeHint().width() + 10;
    int x = std::max(20, (width() - hudW) / 2);
    int y = height() - m_hudPill->height() - 20;
    m_hudPill->setGeometry(x, y, hudW, m_hudPill->height());

    if (m_hudOpacity->opacity() < 0.95) {
        m_hudAnim->stop();
        m_hudAnim->setStartValue(m_hudOpacity->opacity());
        m_hudAnim->setEndValue(1.0);
        m_hudAnim->start();
    }

    if (autoHide && m_hudTimer) {
        m_hudTimer->start();
    }
}

void PdfBaseViewer::hideHud()
{
    if (!m_hudPill || m_hudPill->underMouse()) return;

    m_hudAnim->stop();
    m_hudAnim->setStartValue(m_hudOpacity->opacity());
    m_hudAnim->setEndValue(0.0);
    m_hudAnim->start();
}
