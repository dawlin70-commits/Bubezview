/**
 * @file pdf_base_viewer.cpp
 * @brief Implementation of PdfBaseViewer lightweight native PDF reading engine.
 */

#include "pdf_base_viewer.h"
#include "pdf_document_ops.h"
#include "core/services/theme_manager.h"
#include "vector_icons.h"

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
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QFile>
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
    m_canvas->setScrollArea(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);
    m_splitter->addWidget(m_scrollArea);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(m_splitter);

    // Canvas signals
    connect(m_canvas, &PdfBaseCanvas::selectionChanged, this, &PdfBaseViewer::onCanvasSelectionChanged);
    connect(m_canvas, &PdfBaseCanvas::zoomAtRequested, this, &PdfBaseViewer::zoomAt);
    connect(m_canvas, &PdfBaseCanvas::showInfoRequested, this, &PdfBaseViewer::showDocumentInfo);
    connect(m_canvas, &PdfBaseCanvas::printRequested, this, &PdfBaseViewer::printDocument);
    connect(m_canvas, &PdfBaseCanvas::fitWidthRequested, this, &PdfBaseViewer::fitToWidth);
    connect(m_canvas, &PdfBaseCanvas::fitPageRequested, this, &PdfBaseViewer::fitToPage);
    connect(m_canvas, &PdfBaseCanvas::savePageAsPdfRequested, this, &PdfBaseViewer::savePageAsPdfRequested);
    connect(m_canvas, &PdfBaseCanvas::savePageAsImageRequested, this, &PdfBaseViewer::savePageAsImageRequested);
    connect(m_canvas, &PdfBaseCanvas::saveAsRequested, this, &PdfBaseViewer::saveAsRequested);
    connect(m_canvas, &PdfBaseCanvas::exportRequested, this, &PdfBaseViewer::exportRequested);
    connect(m_canvas, &PdfBaseCanvas::scanTextRequested, this, &PdfBaseViewer::scanTextRequested);
    connect(m_canvas, &PdfBaseCanvas::requestStudio, this, &PdfBaseViewer::requestStudio);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (m_canvas && m_pageCount > 0 && m_scrollArea && m_scrollArea->viewport()) {
            int centerY = value + m_scrollArea->viewport()->height() / 2;
            int pageIdx = m_canvas->pageIndexAtY(centerY);
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

    auto createPillBtn = [this](const QString &text, const QString &tip, const QIcon &icon = QIcon()) -> QToolButton* {
        auto *b = new QToolButton(m_hudPill);
        if (!icon.isNull()) {
            b->setIcon(icon);
            b->setIconSize(QSize(16, 16));
        } else {
            b->setText(text);
        }
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFixedSize(28, 28);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };

    m_btnPrev = createPillBtn(QString(), tr("Previous Page (Page Up / Left Arrow)"),
                              VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronLeft, QColor("#94a3b8"), 14));
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

    m_btnNext = createPillBtn(QString(), tr("Next Page (Page Down / Right Arrow)"),
                              VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronRight, QColor("#94a3b8"), 14));
    connect(m_btnNext, &QToolButton::clicked, this, &PdfBaseViewer::nextPage);

    hudLayout->addWidget(m_btnPrev);
    hudLayout->addWidget(m_lblPageText);
    hudLayout->addWidget(m_spinPage);
    hudLayout->addWidget(m_lblPageCount);
    hudLayout->addWidget(m_btnNext);

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
    if (!m_doc) {
        clear();
        return false;
    }

    if (m_doc->isLocked()) {
        bool ok = false;
        while (m_doc->isLocked()) {
            QString password = QInputDialog::getText(
                this,
                tr("Password Protected PDF"),
                tr("This document is encrypted. Please enter the password to open it:"),
                QLineEdit::Password,
                QString(),
                &ok
            );
            if (!ok) {
                // User cancelled
                clear();
                return false;
            }
            QByteArray pwdUtf8 = password.toUtf8();
            if (m_doc->unlock(pwdUtf8, pwdUtf8)) {
                break;
            }
            QMessageBox::warning(
                this,
                tr("Incorrect Password"),
                tr("The password you entered is incorrect. Please try again.")
            );
        }
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = 0;
    m_rotationDegrees = 0;
    m_zoomFactor = 1.0;
    m_fitToWidthMode = true;
    m_fitToPageMode = false;

    populateOutline();
    fitToWidth();
    updateNavControls();

    showHud(true);
    emit pageChanged(m_currentPage + 1, m_pageCount);
    if (m_pageChangedCallback) {
        m_pageChangedCallback(m_currentPage + 1, m_pageCount);
    }
    if (m_zoomChangedCallback) {
        m_zoomChangedCallback(m_zoomFactor);
    }
    return true;
}

void PdfBaseViewer::clear()
{
    m_filePath.clear();
    m_doc.reset();
    m_currentPage = 0;
    m_pageCount = 0;
    m_rotationDegrees = 0;
    clearSearch();
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
    if (!p) return QSize();
    QSize sz = p->pageSize();
    if (m_rotationDegrees == 90 || m_rotationDegrees == 270) {
        sz.transpose();
    }
    return sz;
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
    if (!pRect.isEmpty() && m_scrollArea && m_scrollArea->viewport()) {
        int viewH = m_scrollArea->viewport()->height();
        int targetY = pRect.top() - 16;
        if (m_fitToPageMode && pRect.height() < viewH) {
            targetY = pRect.top() - std::max(16, (viewH - pRect.height()) / 2);
        }
        m_scrollArea->verticalScrollBar()->setValue(targetY);
    }
    updateNavControls();
    showHud(true);
    emit pageChanged(m_currentPage + 1, m_pageCount);
    if (m_pageChangedCallback) {
        m_pageChangedCallback(m_currentPage + 1, m_pageCount);
    }
}

void PdfBaseViewer::zoomIn()
{
    if (m_scrollArea && m_scrollArea->viewport()) {
        QPoint center(m_scrollArea->viewport()->width() / 2, m_scrollArea->viewport()->height() / 2);
        zoomAt(center, 1.25);
    } else {
        setZoomFactor(m_zoomFactor * 1.25);
    }
}

void PdfBaseViewer::zoomOut()
{
    if (m_scrollArea && m_scrollArea->viewport()) {
        QPoint center(m_scrollArea->viewport()->width() / 2, m_scrollArea->viewport()->height() / 2);
        zoomAt(center, 1.0 / 1.25);
    } else {
        setZoomFactor(m_zoomFactor / 1.25);
    }
}

void PdfBaseViewer::resetZoom()
{
    m_fitToWidthMode = false;
    m_fitToPageMode = false;
    setZoomFactor(1.0);
}

void PdfBaseViewer::fitToWidth()
{
    if (!m_doc || m_pageCount <= 0 || !m_scrollArea) return;

    m_fitToWidthMode = true;
    m_fitToPageMode = false;
    updateCanvasLayout();
    showHud(true);
}

void PdfBaseViewer::fitToPage()
{
    if (!m_doc || m_pageCount <= 0 || !m_scrollArea) return;

    m_fitToWidthMode = false;
    m_fitToPageMode = true;
    updateCanvasLayout();
    showHud(true);
}

void PdfBaseViewer::setZoomFactor(double factor)
{
    m_fitToWidthMode = false;
    m_fitToPageMode = false;
    m_zoomFactor = std::clamp(factor, 0.15, 6.0);
    updateCanvasLayout();
    showHud(true);
}

void PdfBaseViewer::zoomAt(const QPoint &viewportPos, double factor)
{
    if (!m_canvas || !m_scrollArea || !m_scrollArea->viewport() || !m_doc) return;

    QPoint canvasPos = m_canvas->mapFrom(m_scrollArea->viewport(), viewportPos);
    int pageIdx = m_canvas->pageIndexAtY(canvasPos.y());
    QRect pRect = m_canvas->pageRect(pageIdx);

    double normX = 0.5;
    double normY = 0.5;
    if (!pRect.isEmpty()) {
        normX = std::clamp((double)(canvasPos.x() - pRect.left()) / pRect.width(), 0.0, 1.0);
        normY = std::clamp((double)(canvasPos.y() - pRect.top()) / pRect.height(), 0.0, 1.0);
    }

    m_fitToWidthMode = false;
    m_fitToPageMode = false;
    m_zoomFactor = std::clamp(m_zoomFactor * factor, 0.15, 6.0);
    updateCanvasLayout();

    QRect newPRect = m_canvas->pageRect(pageIdx);
    if (!newPRect.isEmpty()) {
        int targetCanvasX = newPRect.left() + (int)std::round(normX * newPRect.width());
        int targetCanvasY = newPRect.top() + (int)std::round(normY * newPRect.height());

        int targetScrollX = targetCanvasX - viewportPos.x();
        int targetScrollY = targetCanvasY - viewportPos.y();

        if (m_scrollArea->horizontalScrollBar()) {
            m_scrollArea->horizontalScrollBar()->setValue(targetScrollX);
        }
        if (m_scrollArea->verticalScrollBar()) {
            m_scrollArea->verticalScrollBar()->setValue(targetScrollY);
        }
    }

    showHud(true);
}

void PdfBaseViewer::toggleZoomTool(bool active)
{
    if (m_canvas) {
        m_canvas->setHandToolActive(active);
    }
}

bool PdfBaseViewer::isZoomToolActive() const
{
    return m_canvas ? m_canvas->isHandToolActive() : false;
}

void PdfBaseViewer::rotateClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 90) % 360;
    updateCanvasLayout();
    showHud(true);
}

void PdfBaseViewer::rotateCounterClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 270) % 360;
    updateCanvasLayout();
    showHud(true);
}

void PdfBaseViewer::updateCanvasLayout()
{
    if (!m_canvas || !m_scrollArea) return;

    int availW = 800;
    int availH = 600;
    if (m_scrollArea->viewport() && m_scrollArea->viewport()->width() > 50 && m_scrollArea->viewport()->height() > 50) {
        availW = m_scrollArea->viewport()->width();
        availH = m_scrollArea->viewport()->height();
    } else if (width() > 50 && height() > 50) {
        availW = width();
        availH = height();
    }

    m_canvas->setDocument(m_doc.get(), m_zoomFactor, m_rotationDegrees, availW, availH, m_fitToPageMode, m_fitToWidthMode);

    if (m_currentPage >= 0 && m_currentPage < m_canvas->pageCount()) {
        m_zoomFactor = m_canvas->pageScale(m_currentPage);
    }
    if (m_zoomChangedCallback) {
        m_zoomChangedCallback(m_zoomFactor);
    }
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

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    bool sameQuery = (m_lastSearchText == text && m_lastSearchCase == matchCase);
    m_lastSearchText = text;
    m_lastSearchCase = matchCase;

    // Check if there are further matches on the current page
    if (sameQuery && m_searchHighlightPage >= 0 && m_searchHighlightPage < m_pageCount) {
        std::unique_ptr<Poppler::Page> curPage(m_doc->page(m_searchHighlightPage));
        if (curPage) {
            QList<QRectF> curMatches = curPage->search(text, flags, r);
            if (m_searchMatchIndex + 1 < curMatches.size()) {
                m_searchMatchIndex++;
                m_canvas->setSearchHighlight(m_searchHighlightPage, curMatches[m_searchMatchIndex]);
                scrollToPage(m_searchHighlightPage);
                return true;
            }
        }
    }

    // Search subsequent pages
    int startPage = (m_searchHighlightPage >= 0) ? (m_searchHighlightPage + 1) % m_pageCount : m_currentPage;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage + step) % m_pageCount;
        std::unique_ptr<Poppler::Page> page(m_doc->page(pageIdx));
        if (!page) continue;

        QList<QRectF> matches = page->search(text, flags, r);
        if (!matches.isEmpty()) {
            if (wrapped && pageIdx <= m_currentPage && step > 0) {
                *wrapped = true;
            }
            m_searchHighlightPage = pageIdx;
            m_searchMatchIndex = 0;
            m_canvas->setSearchHighlight(pageIdx, matches.first());
            scrollToPage(pageIdx);
            return true;
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

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    bool sameQuery = (m_lastSearchText == text && m_lastSearchCase == matchCase);
    m_lastSearchText = text;
    m_lastSearchCase = matchCase;

    // Check if there are earlier matches on the current page
    if (sameQuery && m_searchHighlightPage >= 0 && m_searchHighlightPage < m_pageCount && m_searchMatchIndex > 0) {
        std::unique_ptr<Poppler::Page> curPage(m_doc->page(m_searchHighlightPage));
        if (curPage) {
            QList<QRectF> curMatches = curPage->search(text, flags, r);
            if (m_searchMatchIndex - 1 < curMatches.size()) {
                m_searchMatchIndex--;
                m_canvas->setSearchHighlight(m_searchHighlightPage, curMatches[m_searchMatchIndex]);
                scrollToPage(m_searchHighlightPage);
                return true;
            }
        }
    }

    // Search previous pages
    int startPage = (m_searchHighlightPage >= 0) ? (m_searchHighlightPage - 1 + m_pageCount) % m_pageCount : m_currentPage;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage - step + m_pageCount) % m_pageCount;
        std::unique_ptr<Poppler::Page> page(m_doc->page(pageIdx));
        if (!page) continue;

        QList<QRectF> matches = page->search(text, flags, r);
        if (!matches.isEmpty()) {
            if (wrapped && pageIdx >= m_currentPage && step > 0) {
                *wrapped = true;
            }
            m_searchHighlightPage = pageIdx;
            m_searchMatchIndex = matches.size() - 1;
            m_canvas->setSearchHighlight(pageIdx, matches.last());
            scrollToPage(pageIdx);
            return true;
        }
    }
    return false;
}

void PdfBaseViewer::clearSearch()
{
    m_searchHighlightPage = -1;
    m_searchMatchIndex = -1;
    m_lastSearchText.clear();
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

    return page->renderToImage(dpi, dpi, -1, -1, -1, -1, Poppler::Page::Rotate0);
}

QImage PdfBaseViewer::renderCurrentPageForOcr(int dpi) const
{
    return renderPageForOcr(m_currentPage, dpi);
}

void PdfBaseViewer::showDocumentInfo()
{
    PdfDocumentOps::showDocumentInfoDialog(this, m_doc.get(), m_filePath, m_currentPage, m_pageCount);
}

bool PdfBaseViewer::saveAs(const QString &targetFilePath)
{
    if (m_filePath.isEmpty() || !QFile::exists(m_filePath) || targetFilePath.isEmpty()) {
        return false;
    }
    if (QFile::exists(targetFilePath)) {
        QFile::remove(targetFilePath);
    }
    return QFile::copy(m_filePath, targetFilePath);
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

    if (!m_filePath.isEmpty() && QFile::exists(m_filePath)) {
        QString qpdfErr;
        if (PdfDocumentOps::extractSinglePageToPdf(m_filePath, m_currentPage, outputPath, qpdfErr)) {
            return true;
        }
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
    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    bool isDark = ThemeManager::isDark(preset);
    QString bgCanvas;
    QString bgHud;
    QString borderHud;
    QString textHud;

    if (preset == ThemePreset::ZincGray) {
        bgCanvas = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3e3f44, stop:0.5 #343539, stop:1 #2b2c30)");
        bgHud = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(88, 89, 96, 0.94), stop:1 rgba(54, 55, 59, 0.96))");
        borderHud = QStringLiteral("rgba(255, 255, 255, 0.20)");
        textHud = QStringLiteral("#ffffff");
    } else if (isDark) {
        bgCanvas = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e2538, stop:0.5 #151a28, stop:1 #0e121c)");
        bgHud = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.94), stop:1 rgba(15, 23, 42, 0.96))");
        borderHud = QStringLiteral("rgba(255, 255, 255, 0.14)");
        textHud = QStringLiteral("#f8fafc");
    } else {
        bgCanvas = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #edf0f4, stop:0.5 #e2e6eb, stop:1 #d6dbe2)");
        bgHud = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.94), stop:1 rgba(235, 239, 245, 0.96))");
        borderHud = QStringLiteral("rgba(15, 23, 42, 0.12)");
        textHud = QStringLiteral("#1e293b");
    }

    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(
            QString("QScrollArea { background: %1; border: none; }\n").arg(bgCanvas) +
            ThemeManager::scrollBarStyleSheet(preset)
        );
    }

    if (m_hudPill) {
        m_hudPill->setStyleSheet(QString(
            "QWidget#PdfBaseHudPill {"
            "  background: %1;"
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
            "  background-color: rgba(56, 189, 248, 0.22);"
            "  border-radius: 14px;"
            "}"
            "QSpinBox {"
            "  background-color: transparent;"
            "  color: %3;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "  font-weight: 600;"
            "}"
        ).arg(bgHud, borderHud, textHud));

        if (m_btnPrev) {
            m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronLeft, QColor(textHud), 14));
            m_btnPrev->setIconSize(QSize(14, 14));
        }
        if (m_btnNext) {
            m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronRight, QColor(textHud), 14));
            m_btnNext->setIconSize(QSize(14, 14));
        }
    }
}

void PdfBaseViewer::repositionHud()
{
    if (!m_hudPill) return;
    m_hudPill->raise();
    int hudW = m_hudPill->sizeHint().width() + 10;
    int x = std::max(20, (width() - hudW) / 2);
    int y = height() - m_hudPill->height() - 20;
    m_hudPill->setGeometry(x, y, hudW, m_hudPill->height());
}

void PdfBaseViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionHud();
}

void PdfBaseViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (layout()) {
        layout()->activate();
    }
    if (m_fitToPageMode || m_fitToWidthMode) {
        updateCanvasLayout();
        scrollToPage(m_currentPage);
    }
    repositionHud();
}

bool PdfBaseViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (m_scrollArea && watched == m_scrollArea->viewport()) {
        if (event->type() == QEvent::Resize) {
            if (m_fitToPageMode || m_fitToWidthMode) {
                updateCanvasLayout();
            }
        }
    }
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Wheel) {
        if (m_hudPill && (m_hudOpacity->opacity() < 0.95 || (m_hudTimer && !m_hudTimer->isActive()))) {
            showHud(true);
        }
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
    if (pageIndex >= 0 && pageIndex < m_pageCount && pageIndex != m_currentPage) {
        m_currentPage = pageIndex;
        updateNavControls();
        emit pageChanged(m_currentPage + 1, m_pageCount);
        if (m_pageChangedCallback) {
            m_pageChangedCallback(m_currentPage + 1, m_pageCount);
        }
    }
}

void PdfBaseViewer::onCanvasSelectionChanged(bool hasSelection)
{
    emit selectionChanged(hasSelection);
    if (m_selectionChangedCallback) {
        m_selectionChangedCallback();
    }
}

void PdfBaseViewer::populateOutline()
{
    m_tocModel->clear();
    if (!m_doc) {
        m_hasOutline = false;
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    QVector<Poppler::OutlineItem> rootItems = m_doc->outline();
    if (rootItems.isEmpty()) {
        m_hasOutline = false;
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    m_hasOutline = true;
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

void PdfBaseViewer::setEditMode(bool editMode)
{
    m_isEditMode = editMode;
    if (m_canvas) {
        m_canvas->setEditMode(editMode);
    }
    if (m_editorOverlay) {
        m_editorOverlay->setVisible(editMode);
    }
    updateNavControls();
}

void PdfBaseViewer::setEditorOverlay(QWidget *overlay)
{
    m_editorOverlay = overlay;
    if (m_canvas) {
        m_canvas->setEditorOverlay(overlay);
    }
}
