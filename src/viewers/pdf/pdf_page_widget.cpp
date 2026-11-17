/**
 * @file pdf_page_widget.cpp
 * @brief Implementation of interactive PDF canvas, text selection, annotations, and AcroForms.
 */

#include "pdf_page_widget.h"
#include "pdf_canvas_tools.h"
#include "pdf_document_ops.h"
#include "theme_manager.h"
#include "pdf_typewriter_dialog.h"
#include "pdf_watermark_dialog.h"
#include "pdf_signature_dialog.h"
#include <QFileDialog>
#include <poppler/qt6/poppler-qt6.h>
#include <poppler/qt6/poppler-form.h>

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QTextEdit>
#include <QToolTip>
#include <QStandardPaths>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFontInfo>
#include <QFontDatabase>
#include <QTimer>
#include <algorithm>
#include <cmath>

PdfPageWidget::PdfPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_scaleX(1.0)
    , m_scaleY(1.0)
    , m_toolMode(PdfToolMode::Hand)
    , m_transformBox(nullptr)
    , m_isSelecting(false)
    , m_isPanning(false)
    , m_isDraggingTool(false)
    , m_hasAreaSelection(false)
    , m_shapeType(0)
    , m_shapeStrokeWidth(2)
    , m_shapeStrokeColor(Qt::red)
    , m_shapeFillColor(Qt::transparent)
    , m_highlighterColor(QColor(255, 235, 59))
    , m_highlighterOpacity(0.40)
    , m_continuousMode(false)
    , m_inlineEditor(nullptr)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setAcceptDrops(true);

    // Initialize hardware profile page cache limit
    QSettings settings(QStringLiteral("Bubez"), QStringLiteral("BubezView"));
    m_maxCachePages = (settings.value(QStringLiteral("hardware_profile"), QStringLiteral("balanced")).toString() == QStringLiteral("desktop")) ? 20 : 8;

    // Initialize 8-handle transformation box overlay
    m_transformBox = new PdfTransformBox(this);
    m_transformBox->hide();

    connect(m_transformBox, &PdfTransformBox::applied, this, &PdfPageWidget::onTransformBoxApplied);
    connect(m_transformBox, &PdfTransformBox::applyAndContinue, this, &PdfPageWidget::onTransformBoxApplyAndContinue);
    connect(m_transformBox, &PdfTransformBox::cancelled, this, &PdfPageWidget::onTransformBoxCancelled);
    connect(m_transformBox, &PdfTransformBox::editRequested, this, &PdfPageWidget::onTransformBoxEditRequested);
    connect(m_transformBox, &PdfTransformBox::copyAreaTextRequested, this, &PdfPageWidget::copySelectedAreaText);
    connect(m_transformBox, &PdfTransformBox::copyAreaRequested, this, &PdfPageWidget::copySelectedAreaImage);
    connect(m_transformBox, &PdfTransformBox::ocrAreaRequested, this, &PdfPageWidget::ocrSelectedArea);
    connect(m_transformBox, &PdfTransformBox::undoRequested, this, &PdfPageWidget::undoRequested);
    connect(m_transformBox, &PdfTransformBox::redoRequested, this, &PdfPageWidget::redoRequested);
    connect(m_transformBox, &PdfTransformBox::geometryChanged, this, &PdfPageWidget::updateTransformBoxCollision);
    connect(m_transformBox, &PdfTransformBox::placementModeChanged, this, [this](bool isPlacement) {
        if (isPlacement) {
            emit inPlaceTextEditFinished();
        } else {
            emit inPlaceTextEditStarted(m_transformBox->text(), m_transformBox->textFont(),
                                        m_transformBox->textColor(), m_transformBox->textBgMode(),
                                        m_pendingErasedBgColor);
        }
    });
}

PdfPageWidget::~PdfPageWidget() = default;

void PdfPageWidget::cancelTransitionAnimation()
{
    if (m_transitionAnim) {
        m_transitionAnim->stop();
    }
    m_previousPixmap = QPixmap();
    m_transitionProgress = 1.0;
    m_transitionDirection = 0;
}

void PdfPageWidget::setPageContent(const QPixmap &pixmap,
                                   std::vector<std::unique_ptr<Poppler::TextBox>> &&textBoxes,
                                   std::vector<std::unique_ptr<Poppler::FormField>> &&formFields,
                                   double scaleX, double scaleY,
                                   int transitionDirection)
{
    // If a transition is requested and we have valid existing content of matching size, animate smoothly
    if (transitionDirection != 0 && !m_pixmap.isNull() && !pixmap.isNull() && m_pixmap.size() == pixmap.size()) {
        m_previousPixmap = m_pixmap;
        m_transitionDirection = transitionDirection;
        m_transitionProgress = 0.0;

        if (!m_transitionAnim) {
            m_transitionAnim = new QVariantAnimation(this);
            m_transitionAnim->setDuration(180);
            m_transitionAnim->setEasingCurve(QEasingCurve::OutCubic);
            connect(m_transitionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
                m_transitionProgress = val.toDouble();
                update();
            });
            connect(m_transitionAnim, &QVariantAnimation::finished, this, [this]() {
                m_transitionProgress = 1.0;
                m_previousPixmap = QPixmap();
                m_transitionDirection = 0;
                update();
            });
        } else {
            m_transitionAnim->stop();
        }

        m_transitionAnim->setStartValue(0.0);
        m_transitionAnim->setEndValue(1.0);
        m_transitionAnim->start();
    } else {
        cancelTransitionAnimation();
    }

    m_pixmap = pixmap;
    m_textBoxes = std::move(textBoxes);
    m_formFields = std::move(formFields);
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    m_selectedIndices.clear();
    m_isSelecting = false;
    m_isDraggingTool = false;

    if (m_inlineEditor) {
        m_inlineEditor->deleteLater();
        m_inlineEditor = nullptr;
    }

    setFixedSize(m_pixmap.size());
    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(rect());
    }
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::clear()
{
    cancelTransitionAnimation();
    m_pixmap = QPixmap();
    m_textBoxes.clear();
    m_formFields.clear();
    m_selectedIndices.clear();
    m_isSelecting = false;
    m_isDraggingTool = false;
    m_hasAreaSelection = false;
    m_selectedAreaRect = QRect();

    if (m_inlineEditor) {
        m_inlineEditor->deleteLater();
        m_inlineEditor = nullptr;
    }

    if (m_transformBox) {
        m_transformBox->dismiss();
    }

    m_pageLayouts.clear();
    m_pageCache.clear();
    clearPendingErasedState();
    m_permanentlyErasedBoxIndices.clear();
    m_doc = nullptr;
    setFixedSize(0, 0);
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::setDocument(Poppler::Document *doc, double zoomFactor, bool fitToWidth, int rotationDegrees, int availWidth)
{
    const bool sameParams = (m_doc == doc &&
                             qFuzzyCompare(m_zoomFactor, zoomFactor) &&
                             m_fitToWidth == fitToWidth &&
                             m_rotationDegrees == rotationDegrees &&
                             m_availWidth == availWidth &&
                             !m_pageLayouts.isEmpty());
    if (sameParams) {
        return;
    }

    m_doc = doc;
    m_zoomFactor = zoomFactor;
    m_fitToWidth = fitToWidth;
    m_rotationDegrees = rotationDegrees;
    m_availWidth = availWidth;

    m_pageLayouts.clear();
    m_pageCache.clear();
    cancelTransitionAnimation();

    if (!m_doc || m_doc->numPages() == 0) {
        setFixedSize(0, 0);
        update();
        return;
    }

    int numPages = m_doc->numPages();
    m_pageLayouts.reserve(numPages);

    int maxPageW = 0;
    int currentY = 12;
    const int pageSpacing = 16;

    for (int i = 0; i < numPages; ++i) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(i));
        if (!p) continue;

        QSize origSize = p->pageSize();
        if (m_rotationDegrees == 90 || m_rotationDegrees == 270) {
            origSize.transpose();
        }

        double dpiX = 96.0 * m_zoomFactor;
        double dpiY = 96.0 * m_zoomFactor;

        if (m_fitToWidth && availWidth >= 80 && origSize.width() > 0) {
            double targetDpi = (static_cast<double>(availWidth) / origSize.width()) * 72.0;
            dpiX = targetDpi * m_zoomFactor;
            dpiY = targetDpi * m_zoomFactor;
        }

        dpiX = std::max(20.0, std::min(400.0, dpiX));
        dpiY = std::max(20.0, std::min(400.0, dpiY));

        double scaleX = dpiX / 72.0;
        double scaleY = dpiY / 72.0;

        int pw = qRound(origSize.width() * scaleX);
        int ph = qRound(origSize.height() * scaleY);

        if (pw > maxPageW) {
            maxPageW = pw;
        }

        PageLayout pl;
        pl.pageIndex = i;
        pl.rect = QRect(0, currentY, pw, ph);
        pl.scaleX = scaleX;
        pl.scaleY = scaleY;

        m_pageLayouts.append(pl);
        currentY += ph + pageSpacing;
    }

    int totalW = std::max(maxPageW + 24, availWidth);
    for (auto &pl : m_pageLayouts) {
        int x = (totalW - pl.rect.width()) / 2;
        pl.rect.moveLeft(std::max(12, x));
    }

    int totalH = currentY + 16;
    setFixedSize(totalW, totalH);

    if (m_activePageIndex < 0 || m_activePageIndex >= numPages) {
        m_activePageIndex = 0;
    }

    refreshActivePageData();
    update();
}

void PdfPageWidget::refreshActivePageData()
{
    m_textBoxes.clear();
    m_formFields.clear();
    m_selectedIndices.clear();

    if (!m_doc || m_activePageIndex < 0 || m_activePageIndex >= m_doc->numPages()) {
        return;
    }

    std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
    if (p) {
        m_textBoxes = p->textList();
        m_formFields = p->formFields();
    }

    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        m_scaleX = m_pageLayouts[m_activePageIndex].scaleX;
        m_scaleY = m_pageLayouts[m_activePageIndex].scaleY;
        if (m_transformBox) {
            m_transformBox->setScaleFactor(m_scaleX);
            m_transformBox->setGeometry(m_pageLayouts[m_activePageIndex].rect);
        }
    }
}

void PdfPageWidget::setActivePage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pageLayouts.size()) return;
    if (m_activePageIndex == pageIndex) return;

    clearPendingErasedState();
    m_permanentlyErasedBoxIndices.clear();
    m_activePageIndex = pageIndex;
    refreshActivePageData();
    update();
    emit activePageChanged(m_activePageIndex);
}

int PdfPageWidget::pageIndexAtY(int y) const
{
    for (int i = 0; i < m_pageLayouts.size(); ++i) {
        const auto &pl = m_pageLayouts[i];
        if (y >= pl.rect.top() && y <= pl.rect.bottom() + 16) {
            return i;
        }
    }
    if (!m_pageLayouts.isEmpty()) {
        if (y < m_pageLayouts.first().rect.top()) return 0;
        if (y > m_pageLayouts.last().rect.bottom()) return m_pageLayouts.size() - 1;
    }
    return -1;
}

int PdfPageWidget::pageIndexAtPos(const QPoint &pos) const
{
    if (m_pageLayouts.isEmpty()) return -1;

    for (int i = 0; i < m_pageLayouts.size(); ++i) {
        if (m_pageLayouts[i].rect.contains(pos)) {
            return i;
        }
    }

    // If not strictly inside (e.g. user clicked in side margins or padding between pages),
    // find the page whose vertical range covers pos.y(), or the closest page
    int closest = -1;
    int minDistance = 1e9;
    for (int i = 0; i < m_pageLayouts.size(); ++i) {
        const QRect &r = m_pageLayouts[i].rect;
        if (pos.y() >= r.top() && pos.y() <= r.bottom()) {
            return i;
        }
        int dist = std::min(std::abs(pos.y() - r.top()), std::abs(pos.y() - r.bottom()));
        if (dist < minDistance) {
            minDistance = dist;
            closest = i;
        }
    }
    return closest;
}

int PdfPageWidget::pageTopY(int pageIndex) const
{
    if (pageIndex >= 0 && pageIndex < m_pageLayouts.size()) {
        return std::max(0, m_pageLayouts[pageIndex].rect.top() - 8);
    }
    return 0;
}

QRect PdfPageWidget::pageRect(int pageIndex) const
{
    if (pageIndex >= 0 && pageIndex < m_pageLayouts.size()) {
        return m_pageLayouts[pageIndex].rect;
    }
    return QRect();
}

void PdfPageWidget::invalidatePages()
{
    m_pageCache.clear();
    update();
}

void PdfPageWidget::updateTheme()
{
    QSettings settings(QStringLiteral("Bubez"), QStringLiteral("BubezView"));
    m_maxCachePages = (settings.value(QStringLiteral("hardware_profile"), QStringLiteral("balanced")).toString() == QStringLiteral("desktop")) ? 20 : 8;
    update();
}

void PdfPageWidget::setEditMode(bool editMode)
{
    m_isEditMode = editMode;
    if (!editMode) {
        if (m_inlineEditor) {
            m_inlineEditor->deleteLater();
            m_inlineEditor = nullptr;
        }
        if (m_transformBox) {
            m_transformBox->dismiss();
        }
        setToolMode(PdfToolMode::Hand);
    } else {
        setToolMode(PdfToolMode::SelectText);
    }
    update();
}

void PdfPageWidget::setToolMode(PdfToolMode mode)
{
    if (!m_isEditMode && mode != PdfToolMode::SelectText && mode != PdfToolMode::Hand && mode != PdfToolMode::Zoom && mode != PdfToolMode::Snapshot) {
        return;
    }
    if (m_toolMode == mode) {
        // Ensure cursor is strictly synchronized with mode even if unchanged
        switch (m_toolMode) {
        case PdfToolMode::Hand:
            setCursor(Qt::OpenHandCursor);
            break;
        case PdfToolMode::SelectText:
            setCursor(Qt::IBeamCursor);
            break;
        case PdfToolMode::Zoom: {
            QPixmap zoomPix = QIcon::fromTheme("zoom-in").pixmap(24, 24);
            if (!zoomPix.isNull()) {
                setCursor(QCursor(zoomPix, 8, 8));
            } else {
                setCursor(Qt::CrossCursor);
            }
            break;
        }
        case PdfToolMode::SelectArea:
        case PdfToolMode::Snapshot:
        case PdfToolMode::Redaction:
        case PdfToolMode::Shape:
        case PdfToolMode::Highlight:
        case PdfToolMode::Watermark:
            setCursor(Qt::CrossCursor);
            break;
        case PdfToolMode::Typewriter:
        case PdfToolMode::Signature:
        case PdfToolMode::InsertImage:
            setCursor(Qt::ArrowCursor);
            break;
        }
        return;
    }

    if (m_inlineEditor) {
        m_inlineEditor->deleteLater();
        m_inlineEditor = nullptr;
    }

    m_toolMode = mode;

    switch (m_toolMode) {
    case PdfToolMode::Hand:
        setCursor(Qt::OpenHandCursor);
        break;
    case PdfToolMode::SelectText:
        setCursor(Qt::IBeamCursor);
        break;
    case PdfToolMode::Zoom: {
        QPixmap zoomPix = QIcon::fromTheme("zoom-in").pixmap(24, 24);
        if (!zoomPix.isNull()) {
            setCursor(QCursor(zoomPix, 8, 8));
        } else {
            setCursor(Qt::CrossCursor);
        }
        break;
    }
    case PdfToolMode::SelectArea:
    case PdfToolMode::Snapshot:
    case PdfToolMode::Redaction:
    case PdfToolMode::Shape:
    case PdfToolMode::Highlight:
    case PdfToolMode::Watermark:
        setCursor(Qt::CrossCursor);
        break;
    case PdfToolMode::Typewriter:
    case PdfToolMode::Signature:
    case PdfToolMode::InsertImage:
        setCursor(Qt::ArrowCursor);
        break;
    }

    emit toolModeChanged(m_toolMode);
    update();
}

QPoint PdfPageWidget::visibleCenter() const
{
    QWidget *vp = parentWidget(); // QScrollArea viewport
    if (vp && vp->isVisible() && !m_pageLayouts.isEmpty()) {
        QPoint centerInCanvas = mapFrom(vp, vp->rect().center());
        int pageIdx = pageIndexAtPos(centerInCanvas);
        if (pageIdx < 0) {
            int bestDist = 1e9;
            pageIdx = (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) ? m_activePageIndex : 0;
            for (int i = 0; i < m_pageLayouts.size(); ++i) {
                int dist = std::abs(m_pageLayouts[i].rect.center().y() - centerInCanvas.y());
                if (dist < bestDist) {
                    bestDist = dist;
                    pageIdx = i;
                }
            }
        }
        if (pageIdx >= 0 && pageIdx < m_pageLayouts.size()) {
            if (pageIdx != m_activePageIndex) {
                const_cast<PdfPageWidget*>(this)->setActivePage(pageIdx);
            }
            const auto &pl = m_pageLayouts[pageIdx];
            QPoint centerInPage = centerInCanvas - pl.rect.topLeft();
            int cx = qBound(30, centerInPage.x(), qMax(30, pl.rect.width() - 30));
            int cy = qBound(30, centerInPage.y(), qMax(30, pl.rect.height() - 30));
            return QPoint(cx, cy);
        }
    }
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        return QPoint(m_pageLayouts[m_activePageIndex].rect.width() / 2,
                      m_pageLayouts[m_activePageIndex].rect.height() / 2);
    }
    return rect().center();
}

void PdfPageWidget::startTypewriter(const QString &text, const QFont &font, const QColor &color,
                                    bool whiteout, const QPoint &initialPos)
{
    m_typewriterFont = font;
    m_typewriterColor = color;
    m_typewriterWhiteout = whiteout;

    setToolMode(PdfToolMode::Typewriter);

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = visibleCenter();
    }

    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showText(text, font, color, whiteout, targetPos);
    }
}

void PdfPageWidget::startSignature(const QImage &image, const QPoint &initialPos)
{
    setToolMode(PdfToolMode::Signature);

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = visibleCenter();
    }

    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showImage(image, targetPos);
    }
}

void PdfPageWidget::startInsertImage(const QImage &image, const QPoint &initialPos)
{
    setToolMode(PdfToolMode::InsertImage);

    QRect activePR = pageRect(m_activePageIndex);
    int pageW = activePR.width() > 0 ? activePR.width() : width();
    int pageH = activePR.height() > 0 ? activePR.height() : height();

    // Determine clean proportional display dimensions based on page bounds
    int maxW = (pageW > 100) ? qRound(pageW * 0.82) : 600;
    int maxH = (pageH > 100) ? qRound(pageH * 0.82) : 800;

    QSize defSize(280, 200);
    if (!image.isNull() && image.width() > 0 && image.height() > 0) {
        if (image.width() <= 320 && image.height() <= 320) {
            defSize = image.size();
        } else {
            defSize = image.size().scaled(maxW, maxH, Qt::KeepAspectRatio);
        }
    }

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = activePR.isValid() ? (activePR.center() - activePR.topLeft()) : visibleCenter();
    }

    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showImage(image, targetPos, defSize, PdfTransformItemType::Image);
    }
}

void PdfPageWidget::clearPendingErasedState()
{
    m_pendingErasedPtRect = QRectF();
    m_pendingErasedNormRect = QRectF();
    m_pendingErasedBgColor = Qt::white;
    m_pendingErasedPreviewPatch = QImage();
    m_pendingErasedBoxIndices.clear();
    m_collidingTextPtRects.clear();
    if (m_transformBox) {
        m_transformBox->setHasCollision(false);
    }
}

void PdfPageWidget::updatePendingErasedPreviewPatch()
{
    m_pendingErasedPreviewPatch = QImage();
    if (!m_doc || m_activePageIndex < 0 || m_activePageIndex >= m_doc->numPages() ||
        !m_pendingErasedPtRect.isValid() || m_pendingErasedPtRect.isEmpty()) {
        return;
    }

    double scaleX = m_scaleX;
    double scaleY = m_scaleY;
    if (m_activePageIndex < m_pageLayouts.size()) {
        scaleX = m_pageLayouts[m_activePageIndex].scaleX;
        scaleY = m_pageLayouts[m_activePageIndex].scaleY;
    }
    if (scaleX <= 0.0 || scaleY <= 0.0) {
        return;
    }

    QRect wipedPix(qRound(m_pendingErasedPtRect.x() * scaleX),
                   qRound(m_pendingErasedPtRect.y() * scaleY),
                   qRound(m_pendingErasedPtRect.width() * scaleX),
                   qRound(m_pendingErasedPtRect.height() * scaleY));
    if (wipedPix.width() <= 0 || wipedPix.height() <= 0) {
        return;
    }

    // If the pending erased text is an existing annotation, the underlying vector stream
    // has already been redacted. Bypassing python vector redaction here avoids spurious redactions.
    if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> page(m_doc->page(m_activePageIndex));
        if (page) {
            auto annots = page->annotations();
            for (const auto &ann : annots) {
                if (ann && (ann->subType() == Poppler::Annotation::AText || ann->subType() == Poppler::Annotation::AStamp)) {
                    QRectF b = ann->boundary();
                    if (b.intersects(m_pendingErasedNormRect) || b.contains(m_pendingErasedNormRect.center())) {
                        return;
                    }
                }
            }
        }
    }

    QString srcPath = m_filePath;
    QString tempConvPath;
    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_preview_src_XXXXXX.pdf");
    tempFile.setAutoRemove(false);
    if (tempFile.open()) {
        tempConvPath = tempFile.fileName();
        tempFile.close();
        QFile::remove(tempConvPath);

        std::unique_ptr<Poppler::PDFConverter> conv = m_doc->pdfConverter();
        if (conv) {
            conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
            conv->setOutputFileName(tempConvPath);
            if (conv->convert() && QFile::exists(tempConvPath)) {
                srcPath = tempConvPath;
            }
        }
    }

    if (srcPath.isEmpty() || !QFile::exists(srcPath)) {
        if (!tempConvPath.isEmpty()) {
            QFile::remove(tempConvPath);
        }
        return;
    }

    QString outRedacted;
    QString errMsg;
    if (PdfDocumentOps::removeVectorTextFromPage(srcPath, m_activePageIndex, m_pendingErasedPtRect, outRedacted, errMsg)) {
        std::unique_ptr<Poppler::Document> redDoc = Poppler::Document::load(outRedacted);
        if (redDoc && !redDoc->isLocked() && m_activePageIndex < redDoc->numPages()) {
            redDoc->setRenderHint(Poppler::Document::Antialiasing, true);
            redDoc->setRenderHint(Poppler::Document::TextAntialiasing, true);
            redDoc->setRenderHint(Poppler::Document::ThinLineSolid, true);
            std::unique_ptr<Poppler::Page> redPage = redDoc->page(m_activePageIndex);
            if (redPage) {
                Poppler::Page::Rotation popplerRot = Poppler::Page::Rotate0;
                if (m_rotationDegrees == 90) popplerRot = Poppler::Page::Rotate90;
                else if (m_rotationDegrees == 180) popplerRot = Poppler::Page::Rotate180;
                else if (m_rotationDegrees == 270) popplerRot = Poppler::Page::Rotate270;
                double dpiX = scaleX * 72.0;
                double dpiY = scaleY * 72.0;
                m_pendingErasedPreviewPatch = redPage->renderToImage(
                    dpiX, dpiY,
                    wipedPix.x(), wipedPix.y(), wipedPix.width(), wipedPix.height(),
                    popplerRot);
            }
        }
        redDoc.reset();
        QFile::remove(outRedacted);
    }

    if (!tempConvPath.isEmpty()) {
        QFile::remove(tempConvPath);
    }
}

void PdfPageWidget::updateTransformBoxCollision()
{
    m_collidingTextPtRects.clear();

    if (!m_transformBox || !m_transformBox->isVisible() ||
        m_transformBox->itemType() != PdfTransformItemType::Text ||
        m_textBoxes.empty() || m_scaleX <= 0.0 || m_scaleY <= 0.0) {
        if (m_transformBox) {
            m_transformBox->setHasCollision(false);
        }
        update();
        return;
    }

    QRect boxPix = m_transformBox->boxRect();
    QRectF boxPt(boxPix.x() / m_scaleX,
                 boxPix.y() / m_scaleY,
                 boxPix.width() / m_scaleX,
                 boxPix.height() / m_scaleY);

    // Apply a slight inner tolerance on the placed box so touching baseline margins do not false-trigger
    QRectF shrunkBoxPt = boxPt.adjusted(0.8, 1.2, -0.8, -1.2);
    if (!shrunkBoxPt.isValid() || shrunkBoxPt.isEmpty()) {
        shrunkBoxPt = boxPt;
    }

    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        if (m_pendingErasedBoxIndices.contains(i) || m_permanentlyErasedBoxIndices.contains(i)) {
            continue;
        }
        if (!m_textBoxes[i] || m_textBoxes[i]->text().trimmed().isEmpty()) {
            continue;
        }
        QRectF wordPt = m_textBoxes[i]->boundingBox();
        if (!wordPt.isValid() || wordPt.isEmpty()) {
            continue;
        }
        // Ignore ghost text whose center lies inside the currently wiped block
        if (m_pendingErasedPtRect.isValid() && !m_pendingErasedPtRect.isEmpty() &&
            m_pendingErasedPtRect.contains(wordPt.center())) {
            continue;
        }

        // Shrink candidate word box by 1.2-1.5pt to exclude invisible font ascender/descender padding
        double tolX = std::min(1.2, wordPt.width() * 0.15);
        double tolY = std::min(1.5, wordPt.height() * 0.20);
        QRectF coreWordPt = wordPt.adjusted(tolX, tolY, -tolX, -tolY);
        if (!coreWordPt.isValid() || coreWordPt.isEmpty()) {
            coreWordPt = wordPt;
        }

        if (shrunkBoxPt.intersects(coreWordPt)) {
            m_collidingTextPtRects.append(wordPt);
        }
    }

    m_transformBox->setHasCollision(!m_collidingTextPtRects.isEmpty());
    update();
}

void PdfPageWidget::cutSelection()
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    // 1. Copy text to system clipboard
    copySelectionToClipboard();

    // 2. Identify unified bounding rectangle of the selected text boxes and sample background color
    QRectF unionPtRect;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            m_permanentlyErasedBoxIndices.insert(idx);
            if (unionPtRect.isNull()) {
                unionPtRect = m_textBoxes[idx]->boundingBox();
            } else {
                unionPtRect = unionPtRect.united(m_textBoxes[idx]->boundingBox());
            }
        }
    }

    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageWPt = m_pageLayouts[m_activePageIndex].rect.width() / m_scaleX;
        pageHPt = m_pageLayouts[m_activePageIndex].rect.height() / m_scaleY;
    }

    if (pageWPt > 0 && pageHPt > 0 && unionPtRect.isValid() && !unionPtRect.isEmpty()) {
        // Also mark any overlapping duplicate boxes inside unionPtRect as erased
        QRectF paddedUnion = unionPtRect.adjusted(-1.0, -1.0, 1.0, 1.0);
        for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
            if (m_textBoxes[i] && paddedUnion.contains(m_textBoxes[i]->boundingBox().center())) {
                m_permanentlyErasedBoxIndices.insert(i);
            }
        }

        QRectF normRect(std::max(0.0, (unionPtRect.x() - 1.0) / pageWPt),
                        std::max(0.0, (unionPtRect.y() - 1.0) / pageHPt),
                        std::min(1.0, (unionPtRect.width() + 2.0) / pageWPt),
                        std::min(1.0, (unionPtRect.height() + 2.0) / pageHPt));

        // Erase text cleanly via vector redaction / annotation removal without stamping opaque white
        emit eraseOriginalTextRequested(m_activePageIndex, unionPtRect, normRect);
    }

    // 3. Clear selection and update display
    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);

    QToolTip::showText(QCursor::pos(), tr("✂️ Text cut and copied to clipboard"));
}

QColor PdfPageWidget::sampleBackgroundColorForRect(int pageIndex, const QRectF &ptRect) const
{
    if (pageIndex < 0 || !ptRect.isValid() || ptRect.isEmpty()) {
        return Qt::white;
    }

    QImage pageImg;
    if (m_pageCache.contains(pageIndex) && !m_pageCache[pageIndex].isNull()) {
        pageImg = m_pageCache[pageIndex].toImage();
    } else if (!m_pixmap.isNull()) {
        pageImg = m_pixmap.toImage();
    } else if (m_doc && pageIndex >= 0 && pageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(pageIndex));
        if (p) {
            double dpi = m_scaleX * 72.0;
            pageImg = p->renderToImage(dpi, dpi);
        }
    }

    if (pageImg.isNull()) return Qt::white;

    QRect pixelRect(qRound(ptRect.x() * m_scaleX),
                    qRound(ptRect.y() * m_scaleY),
                    qRound(ptRect.width() * m_scaleX),
                    qRound(ptRect.height() * m_scaleY));

    return PdfCanvasTools::sampleBackgroundColor(pageImg, pixelRect);
}

void PdfPageWidget::launchTextTransform(bool startInEditMode)
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    captureSelectedTextFormat();
    QString text = selectedText();

    clearPendingErasedState();

    QRectF unionPtRect;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            m_pendingErasedBoxIndices.insert(idx);
            if (unionPtRect.isNull()) {
                unionPtRect = m_textBoxes[idx]->boundingBox();
            } else {
                unionPtRect = unionPtRect.united(m_textBoxes[idx]->boundingBox());
            }
        }
    }

    if (unionPtRect.isEmpty() || unionPtRect.width() <= 0.0 || unionPtRect.height() <= 0.0) return;

    // Also include any overlapping or duplicate text boxes inside the unified selection block
    QRectF paddedUnionPt = unionPtRect.adjusted(-1.0, -1.0, 1.0, 1.0);
    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        if (m_textBoxes[i] && paddedUnionPt.contains(m_textBoxes[i]->boundingBox().center())) {
            m_pendingErasedBoxIndices.insert(i);
        }
    }

    m_selectedTextPtRect = unionPtRect;
    m_selectedTextPageIndex = m_activePageIndex;

    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageWPt = m_pageLayouts[m_activePageIndex].rect.width() / m_scaleX;
        pageHPt = m_pageLayouts[m_activePageIndex].rect.height() / m_scaleY;
    }

    if (pageWPt > 0.0 && pageHPt > 0.0) {
        m_pendingErasedPtRect = paddedUnionPt;
        m_pendingErasedNormRect = QRectF(std::max(0.0, paddedUnionPt.x() / pageWPt),
                                         std::max(0.0, paddedUnionPt.y() / pageHPt),
                                         std::min(1.0, paddedUnionPt.width() / pageWPt),
                                         std::min(1.0, paddedUnionPt.height() / pageHPt));
        m_selectedTextNormRect = m_pendingErasedNormRect;
        m_pendingErasedBgColor = sampleBackgroundColorForRect(m_activePageIndex, unionPtRect);
        updatePendingErasedPreviewPatch();
    }

    // Immediately clear text selection and repaint so the selected block is visually wiped on canvas
    m_selectedIndices.clear();
    emit selectionChanged(false);
    update();

    QFont font = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.font : m_typewriterFont;
    QColor color = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.color : m_typewriterColor;

    // Position the transform box directly over the wiped area with tight bounding box
    QRect origPixRect(qRound(unionPtRect.left() * m_scaleX),
                      qRound(unionPtRect.top() * m_scaleY),
                      qRound(unionPtRect.width() * m_scaleX),
                      qRound(unionPtRect.height() * m_scaleY));
    QPoint targetTopLeft(origPixRect.left(), origPixRect.top());
    int suggestedW = origPixRect.width() + 8;

    setToolMode(PdfToolMode::Typewriter);

    PdfTextBgMode bgMode = PdfTextBgMode::Transparent;
    QColor sampledBg = m_pendingErasedBgColor.isValid() ? m_pendingErasedBgColor : Qt::white;

    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showText(text, font, color, bgMode, sampledBg,
                                 QRect(targetTopLeft.x(), targetTopLeft.y(), suggestedW, origPixRect.height()),
                                 targetTopLeft);
        m_transformBox->setTextModified(true);
        if (startInEditMode) {
            m_transformBox->setPlacementMode(false);
            if (m_transformBox->inlineTextEdit()) {
                m_transformBox->inlineTextEdit()->setFocus();
                m_transformBox->inlineTextEdit()->selectAll();
            }
        } else {
            m_transformBox->setPlacementMode(true, QPoint(), suggestedW, targetTopLeft);
        }
    }

    updateTransformBoxCollision();
    if (startInEditMode) {
        emit inPlaceTextEditStarted(text, font, color, bgMode, sampledBg);
    }
}

void PdfPageWidget::startInPlaceTextEdit()
{
    launchTextTransform(true);
}

void PdfPageWidget::updateTextBgProperties(PdfTextBgMode mode, const QColor &color)
{
    if (m_transformBox && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->setTextBgColor(color);
        m_transformBox->setTextBgMode(mode);
    }
}

void PdfPageWidget::commitInPlaceTextEdit()
{
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        onTransformBoxApplied();
    }
}

void PdfPageWidget::editSelectedTextWithWhiteout()
{
    startInPlaceTextEdit();
}

void PdfPageWidget::moveSelectedText()
{
    launchTextTransform(false);
}

void PdfPageWidget::redactSelectedText()
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    QRectF unionPtRect;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            if (unionPtRect.isNull()) {
                unionPtRect = m_textBoxes[idx]->boundingBox();
            } else {
                unionPtRect = unionPtRect.united(m_textBoxes[idx]->boundingBox());
            }
        }
    }

    if (unionPtRect.isEmpty() || unionPtRect.width() <= 0.0 || unionPtRect.height() <= 0.0) return;

    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageWPt = m_pageLayouts[m_activePageIndex].rect.width() / m_scaleX;
        pageHPt = m_pageLayouts[m_activePageIndex].rect.height() / m_scaleY;
    }

    if (pageWPt > 0.0 && pageHPt > 0.0) {
        QRectF normRect(std::max(0.0, (unionPtRect.x() - 1.0) / pageWPt),
                        std::max(0.0, (unionPtRect.y() - 1.0) / pageHPt),
                        std::min(1.0, (unionPtRect.width() + 2.0) / pageWPt),
                        std::min(1.0, (unionPtRect.height() + 2.0) / pageHPt));
        int pixW = std::max(4, qRound((unionPtRect.width() + 2.0) * m_scaleX));
        int pixH = std::max(4, qRound((unionPtRect.height() + 2.0) * m_scaleY));
        QImage blackImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
        blackImg.fill(Qt::black);
        emit addStampAnnotationRequested(blackImg, normRect);
    }

    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);

    QToolTip::showText(QCursor::pos(), tr("⬛ Text redacted"));
}

void PdfPageWidget::updateTypewriterProperties(const QFont &font, const QColor &color, bool whiteout)
{
    m_typewriterFont = font;
    m_typewriterColor = color;
    m_typewriterWhiteout = whiteout;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->updateTextFont(font);
        m_transformBox->updateTextColor(color);
        m_transformBox->updateTextWhiteout(whiteout);
    }
}

void PdfPageWidget::updateShapeProperties(int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeStrokeWidth = strokeWidth;
    m_shapeStrokeColor = strokeColor;
    m_shapeFillColor = fillColor;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->updateShape(m_shapeType, strokeWidth, strokeColor, fillColor);
    }
}

void PdfPageWidget::updateShapeProperties(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeType = shapeType;
    m_shapeStrokeWidth = strokeWidth;
    m_shapeStrokeColor = strokeColor;
    m_shapeFillColor = fillColor;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->updateShape(shapeType, strokeWidth, strokeColor, fillColor);
    }
}

void PdfPageWidget::updateSelectedStampProperties(const QString &text, const QColor &color, qreal angle, int style, int fontSize)
{
    m_watermarkText = text;
    m_watermarkColor = color.isValid() ? color : QColor(QStringLiteral("#dc2626"));
    m_watermarkAngle = angle;
    m_watermarkStyle = style;
    m_watermarkSize = (fontSize > 0) ? fontSize : 28;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Watermark) {
        m_transformBox->updateWatermark(text, angle, 0.90, m_watermarkColor, style);
    }
}

void PdfPageWidget::commitActiveTransformBox()
{
    if (m_transformBox && m_transformBox->isVisible()) {
        onTransformBoxApplied();
    }
}

void PdfPageWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp" || ext == "svg" || ext == "tiff" || ext == "tif") {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void PdfPageWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString path = url.toLocalFile();
            QImage img(path);
            if (!img.isNull()) {
                if (!m_isEditMode) {
                    emit editModeRequested();
                }
                int clickedPage = pageIndexAtPos(event->position().toPoint());
                if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
                    setActivePage(clickedPage);
                }
                QRect activePR = pageRect(m_activePageIndex);
                QPoint localPos = event->position().toPoint() - activePR.topLeft();
                startInsertImage(img, localPos);
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dropEvent(event);
}

void PdfPageWidget::startSelectArea()
{
    if (!m_selectedIndices.empty()) {
        m_selectedIndices.clear();
        emit selectionChanged(false);
    }
    clearAreaSelection();
    setToolMode(PdfToolMode::SelectArea);
}

bool PdfPageWidget::hasAreaSelection() const
{
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        return true;
    }
    return m_hasAreaSelection;
}

void PdfPageWidget::cutSelectedArea()
{
    m_lastClipType = LastClipType::AreaImage;
    m_lastCopiedTextFormat.valid = false;

    QRect r;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        r = m_transformBox->boxRect();
        m_transformBox->dismiss();
    } else if (m_hasAreaSelection && m_selectedAreaRect.width() >= 4 && m_selectedAreaRect.height() >= 4) {
        r = m_selectedAreaRect;
    }
    clearAreaSelection();

    if (r.width() < 4 || r.height() < 4) return;

    QRectF pointRect(r.x() / m_scaleX,
                    r.y() / m_scaleY,
                    r.width() / m_scaleX,
                    r.height() / m_scaleY);

    emit cutAreaRequested(pointRect);
}

void PdfPageWidget::copySelectedArea()
{
    copySelectedAreaImage();
}

void PdfPageWidget::copySelectedAreaImage()
{
    m_lastClipType = LastClipType::AreaImage;
    m_lastCopiedTextFormat.valid = false;

    QRect r;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        r = m_transformBox->boxRect();
    } else if (m_hasAreaSelection && m_selectedAreaRect.width() >= 4 && m_selectedAreaRect.height() >= 4) {
        r = m_selectedAreaRect;
    }

    if (r.width() < 4 || r.height() < 4 || m_scaleX <= 0.0 || m_scaleY <= 0.0) return;

    QRectF pointRect(r.x() / m_scaleX,
                    r.y() / m_scaleY,
                    r.width() / m_scaleX,
                    r.height() / m_scaleY);

    emit copyAreaRequested(pointRect);
}

void PdfPageWidget::copySelectedAreaText()
{
    QRect r;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        r = m_transformBox->boxRect();
    } else if (m_hasAreaSelection && m_selectedAreaRect.width() >= 4 && m_selectedAreaRect.height() >= 4) {
        r = m_selectedAreaRect;
    }

    if (r.width() < 4 || r.height() < 4 || m_scaleX <= 0.0 || m_scaleY <= 0.0) return;

    QRectF areaPtRect(r.x() / m_scaleX, r.y() / m_scaleY, r.width() / m_scaleX, r.height() / m_scaleY);

    QString extractedText;
    std::vector<int> containedIndices;
    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        if (!m_textBoxes[i]) continue;
        QRectF b = m_textBoxes[i]->boundingBox();
        if (areaPtRect.contains(b.center()) ||
            (areaPtRect.intersects(b) && areaPtRect.intersected(b).width() >= b.width() * 0.35)) {
            containedIndices.push_back(i);
        }
    }

    if (!containedIndices.empty()) {
        for (size_t k = 0; k < containedIndices.size(); ++k) {
            int idx = containedIndices[k];
            extractedText += m_textBoxes[idx]->text();
            if (k + 1 < containedIndices.size()) {
                int nextIdx = containedIndices[k + 1];
                QRectF currR = m_textBoxes[idx]->boundingBox();
                QRectF nextR = m_textBoxes[nextIdx]->boundingBox();
                double vertDiff = nextR.center().y() - currR.center().y();
                double lineH = std::max(6.0, currR.height());
                if (vertDiff > lineH * 0.45) {
                    extractedText += (vertDiff > lineH * 1.4) ? QStringLiteral("\n\n") : QStringLiteral("\n");
                } else if (m_textBoxes[idx]->hasSpaceAfter() || nextR.left() > currR.right() + 1.0) {
                    extractedText += QStringLiteral(" ");
                }
            } else if (m_textBoxes[idx]->hasSpaceAfter()) {
                extractedText += QStringLiteral(" ");
            }
        }
    }

    // Direct Poppler text extraction fallback in area rect
    if (extractedText.trimmed().isEmpty() && m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
        if (p) {
            extractedText = p->text(areaPtRect).trimmed();
        }
    }

    QString trimmed = extractedText.trimmed();
    if (!trimmed.isEmpty()) {
        m_lastClipType = LastClipType::Text;
        QApplication::clipboard()->setText(trimmed);
        QToolTip::showText(QCursor::pos(), tr("📋 Copied text from area (%1 characters)").arg(trimmed.length()));
    } else {
        // Fallback to image copy if area contains no selectable text
        copySelectedAreaImage();
    }
}

void PdfPageWidget::ocrSelectedArea()
{
    if (m_transformBox && m_transformBox->isVisible()) {
        if (m_transformBox->itemType() == PdfTransformItemType::Image && !m_transformBox->image().isNull()) {
            emit scanSnippetRequested(m_transformBox->image());
            return;
        }
        if (m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
            QRect r = m_transformBox->boxRect();
            if (r.width() >= 4 && r.height() >= 4 && m_scaleX > 0.0 && m_scaleY > 0.0) {
                QRectF pointRect(r.x() / m_scaleX,
                                r.y() / m_scaleY,
                                r.width() / m_scaleX,
                                r.height() / m_scaleY);
                emit ocrAreaRequested(pointRect);
                return;
            }
        }
    }

    if (m_hasAreaSelection && m_selectedAreaRect.width() >= 4 && m_selectedAreaRect.height() >= 4) {
        QRect r = m_selectedAreaRect;
        if (m_scaleX > 0.0 && m_scaleY > 0.0) {
            QRectF pointRect(r.x() / m_scaleX,
                            r.y() / m_scaleY,
                            r.width() / m_scaleX,
                            r.height() / m_scaleY);
            emit ocrAreaRequested(pointRect);
            return;
        }
    }
}

void PdfPageWidget::clearAreaSelection()
{
    if (m_transformBox && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        m_transformBox->dismiss();
    }
    if (m_hasAreaSelection) {
        m_hasAreaSelection = false;
        m_selectedAreaRect = QRect();
        update();
        emit selectionChanged(!m_selectedIndices.empty());
    }
}

void PdfPageWidget::pasteFromClipboard(const QPoint &targetPos)
{
    const QClipboard *clipboard = QApplication::clipboard();
    QImage img = clipboard->image();
    QString text = clipboard->text();

    bool hasImage = !img.isNull() && img.width() > 0 && img.height() > 0;
    bool hasText = !text.isEmpty();

    if (!hasImage && !hasText) {
        QToolTip::showText(QCursor::pos(), tr("📋 Clipboard is empty"));
        return;
    }

    // Determine target page and convert canvas coordinates to page-local coordinates
    QPoint localPos;
    if (targetPos.isNull()) {
        localPos = visibleCenter();
    } else {
        int pageIdx = pageIndexAtPos(targetPos);
        if (pageIdx >= 0 && pageIdx != m_activePageIndex) {
            setActivePage(pageIdx);
        }
        QRect pr = pageRect(m_activePageIndex);
        if (pr.isValid()) {
            if (pr.contains(targetPos)) {
                localPos = targetPos - pr.topLeft();
            } else if (targetPos.x() >= 0 && targetPos.x() <= pr.width() &&
                       targetPos.y() >= 0 && targetPos.y() <= pr.height()) {
                // targetPos is already in page-local coordinates
                localPos = targetPos;
            } else {
                localPos = targetPos - pr.topLeft();
                localPos.setX(qBound(30, localPos.x(), qMax(30, pr.width() - 30)));
                localPos.setY(qBound(30, localPos.y(), qMax(30, pr.height() - 30)));
            }
        } else {
            localPos = visibleCenter();
        }
    }

    auto extractFormatFromClipboard = [this, clipboard, &text](QFont &outFont, QColor &outColor) {
        bool hasCustomFormat = false;
        const QMimeData *mimeData = clipboard->mimeData();
        if (mimeData && mimeData->hasFormat(QStringLiteral("application/x-bubez-text-format"))) {
            QByteArray raw = mimeData->data(QStringLiteral("application/x-bubez-text-format"));
            QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString fam = obj[QStringLiteral("family")].toString();
                double pt = obj[QStringLiteral("pointSize")].toDouble();
                QString col = obj[QStringLiteral("color")].toString();
                bool b = obj[QStringLiteral("bold")].toBool();
                bool it = obj[QStringLiteral("italic")].toBool();

                if (!fam.isEmpty()) outFont.setFamily(fam);
                if (pt > 0.0) outFont.setPointSizeF(pt);
                outFont.setBold(b);
                outFont.setItalic(it);
                if (b && it) outFont.setStyleName(QStringLiteral("Bold Italic"));
                else if (b) outFont.setStyleName(QStringLiteral("Bold"));
                else if (it) outFont.setStyleName(QStringLiteral("Italic"));
                if (!col.isEmpty()) outColor = QColor(col);
                hasCustomFormat = true;

                m_lastCopiedTextFormat.font = outFont;
                m_lastCopiedTextFormat.color = outColor;
                m_lastCopiedTextFormat.valid = true;
            }
        }

        if (!hasCustomFormat) {
            if (m_lastCopiedTextFormat.valid) {
                outFont = m_lastCopiedTextFormat.font;
                outColor = m_lastCopiedTextFormat.color;
            } else {
                detectTextFormatFromDocument(text);
                outFont = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.font : QFont(QStringLiteral("Sans Serif"), 12);
                outColor = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.color : Qt::black;
            }
        }
    };

    // 1. If an area or image was copied/cut:
    if (hasImage && (m_lastClipType == LastClipType::AreaImage || !hasText)) {
        startInsertImage(img, localPos);
        return;
    }

    // 2. If text was copied/cut:
    if (hasText && (m_lastClipType == LastClipType::Text || !hasImage)) {
        QFont pasteFont(QStringLiteral("Sans Serif"), 12);
        QColor pasteColor = Qt::black;
        extractFormatFromClipboard(pasteFont, pasteColor);

        startTypewriter(text, pasteFont, pasteColor, false, localPos);
        if (m_transformBox) {
            m_transformBox->setPlacementMode(true, QPoint(), 0, localPos);
        }
        updateTransformBoxCollision();
        emit inPlaceTextEditStarted(text, pasteFont, pasteColor, PdfTextBgMode::Transparent, Qt::white);
        return;
    }

    // 3. Fallbacks:
    if (hasImage) {
        startInsertImage(img, localPos);
        return;
    }

    if (hasText) {
        QFont defaultFont(QStringLiteral("Sans Serif"), 12);
        QColor defaultColor = Qt::black;
        extractFormatFromClipboard(defaultFont, defaultColor);

        startTypewriter(text, defaultFont, defaultColor, false, localPos);
        if (m_transformBox) {
            m_transformBox->setPlacementMode(true, QPoint(), 0, localPos);
        }
        updateTransformBoxCollision();
        emit inPlaceTextEditStarted(text, defaultFont, defaultColor, PdfTextBgMode::Transparent, Qt::white);
        return;
    }
}

void PdfPageWidget::startWatermark(const QString &text, qreal angle, qreal opacity,
                                   const QColor &color, const QPoint &initialPos, int style, int fontSize)
{
    m_watermarkText = text.isEmpty() ? tr("CONFIDENTIAL") : text;
    m_watermarkAngle = angle;
    m_watermarkOpacity = opacity;
    m_watermarkColor = color.isValid() ? color : QColor(QStringLiteral("#dc2626"));
    m_watermarkStyle = style;
    m_watermarkSize = (fontSize > 0) ? fontSize : 28;

    if (!m_transformBox) return;

    QPoint targetPos = initialPos.isNull() ? visibleCenter() : initialPos;
    QRect activePR = pageRect(m_activePageIndex);
    int pageW = activePR.width();
    int pageH = activePR.height();
    QRect stampRect = PdfCanvasTools::calculateStampRect(m_watermarkText, m_watermarkAngle,
                                                         m_watermarkSize, targetPos, pageW, pageH);
    m_transformBox->setGeometry(activePR);
    m_transformBox->showWatermark(m_watermarkText, m_watermarkAngle, m_watermarkOpacity,
                                  m_watermarkColor, stampRect.center(), m_watermarkStyle);
    setToolMode(PdfToolMode::Watermark);
    update();
}

void PdfPageWidget::startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeType = shapeType;
    m_shapeStrokeWidth = std::max(1, strokeWidth);
    m_shapeStrokeColor = strokeColor.isValid() ? strokeColor : Qt::red;
    m_shapeFillColor = fillColor;

    setToolMode(PdfToolMode::Shape);

    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->updateShape(m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor);
    }
    update();
}

void PdfPageWidget::setShapeType(int type)
{
    m_shapeType = type;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->setShapeType(type);
    }
}

void PdfPageWidget::setShapeStrokeWidth(int width)
{
    m_shapeStrokeWidth = std::max(1, width);
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->setShapeStrokeWidth(width);
    }
}

void PdfPageWidget::setShapeStrokeColor(const QColor &color)
{
    m_shapeStrokeColor = color;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->setShapeStrokeColor(color);
    }
}

void PdfPageWidget::setShapeFillColor(const QColor &color)
{
    m_shapeFillColor = color;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Shape) {
        m_transformBox->setShapeFillColor(color);
    }
}

void PdfPageWidget::setTypewriterFont(const QFont &font)
{
    m_typewriterFont = font;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->updateTextFont(font);
    }
}

void PdfPageWidget::setTypewriterColor(const QColor &color)
{
    m_typewriterColor = color;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->updateTextColor(color);
    }
}

void PdfPageWidget::setTypewriterWhiteout(bool whiteout)
{
    m_typewriterWhiteout = whiteout;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->updateTextWhiteout(whiteout);
    }
}

void PdfPageWidget::setTypewriterAlignment(Qt::Alignment alignment)
{
    m_typewriterAlignment = alignment;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::Text) {
        m_transformBox->updateTextAlignment(alignment);
    }
}

void PdfPageWidget::startSnapshot()
{
    setToolMode(PdfToolMode::Snapshot);
}

void PdfPageWidget::startRedaction(const QColor &color, bool permanent)
{
    m_redactionColor = color.isValid() ? color : Qt::black;
    m_redactionPermanent = permanent;
    setToolMode(PdfToolMode::Redaction);
}

void PdfPageWidget::startHighlighter(const QColor &color, double opacity, bool /*isSpotlight*/)
{
    m_highlighterColor = color.isValid() ? color : QColor(255, 235, 59);
    m_highlighterOpacity = qBound(0.05, opacity, 1.0);
    m_highlighterSpotlight = false; // Spotlight mode permanently removed
    setToolMode(PdfToolMode::Highlight);
}

void PdfPageWidget::commitHighlightDrawing(const QRect &rect)
{
    if (rect.width() <= 2 || rect.height() <= 2) return;

    int pageW = width();
    int pageH = height();
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageW = m_pageLayouts[m_activePageIndex].rect.width();
        pageH = m_pageLayouts[m_activePageIndex].rect.height();
    }

    if (pageW <= 0 || pageH <= 0) return;

    QRectF normRect(static_cast<double>(rect.x()) / pageW,
                    static_cast<double>(rect.y()) / pageH,
                    static_cast<double>(rect.width()) / pageW,
                    static_cast<double>(rect.height()) / pageH);

    int pixW = std::max(4, qRound(rect.width() * 2.0));
    int pixH = std::max(4, qRound(rect.height() * 2.0));
    QImage highlightImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
    QColor c = m_highlighterColor;
    c.setAlphaF(qBound(0.05, m_highlighterOpacity, 1.0));
    highlightImg.fill(c);

    emit addStampAnnotationRequested(highlightImg, normRect);
    update();
}

void PdfPageWidget::highlightSelectedText(const QColor &color, double opacity)
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    QColor hlColor = color.isValid() ? color : m_highlighterColor;
    double hlOpacity = qBound(0.05, opacity, 1.0);

    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
        if (p) {
            pageWPt = p->pageSize().width();
            pageHPt = p->pageSize().height();
        }
    }

    if (pageWPt <= 0 || pageHPt <= 0) return;

    QList<QPair<QImage, QRectF>> hlStamps;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            const auto &box = m_textBoxes[idx];
            QRectF ptRect = box->boundingBox();

            QRectF normRect(std::max(0.0, ptRect.x() / pageWPt),
                            std::max(0.0, ptRect.y() / pageHPt),
                            std::min(1.0, ptRect.width() / pageWPt),
                            std::min(1.0, ptRect.height() / pageHPt));

            int pixW = std::max(4, qRound(ptRect.width() * m_scaleX * 2.0));
            int pixH = std::max(4, qRound(ptRect.height() * m_scaleY * 2.0));
            QImage hlImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
            QColor c = hlColor;
            c.setAlphaF(hlOpacity);
            hlImg.fill(c);

            hlStamps.append({hlImg, normRect});
        }
    }

    if (!hlStamps.isEmpty()) {
        emit addStampAnnotationsRequested(hlStamps);
    }

    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::updateHighlighterProperties(const QColor &color, double opacity)
{
    m_highlighterColor = color.isValid() ? color : m_highlighterColor;
    m_highlighterOpacity = qBound(0.05, opacity, 1.0);
    update();
}

void PdfPageWidget::clearActiveHighlightSelection()
{
    update();
}

void PdfPageWidget::replaceSelectedText(const QString &newText, const QFont &font, const QColor &color, bool whiteout)
{
    if (newText.isEmpty()) return;

    int pageIdx = (m_selectedTextPageIndex >= 0) ? m_selectedTextPageIndex : m_activePageIndex;
    if (pageIdx < 0) pageIdx = 0;
    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_doc && pageIdx >= 0 && pageIdx < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(pageIdx));
        if (p) {
            pageWPt = p->pageSize().width();
            pageHPt = p->pageSize().height();
        }
    }

    if (pageWPt <= 0 || pageHPt <= 0) return;

    // 1. Erase original text cleanly via vector redaction or annotation removal without whiteout stamps
    if (m_selectedTextPtRect.isValid() && m_selectedTextNormRect.isValid()) {
        emit eraseOriginalTextRequested(pageIdx, m_selectedTextPtRect, m_selectedTextNormRect);
    }

    // 2. Emit persistent text annotation to Poppler
    emit addTextAnnotationRequested(newText, font, color, m_selectedTextNormRect, whiteout);

    m_selectedIndices.clear();
    m_selectedTextPtRect = QRectF();
    m_selectedTextNormRect = QRectF();
    m_selectedTextPageIndex = -1;
    invalidatePages();
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::cancelActiveTool()
{
    if (m_transformBox && m_transformBox->isVisible()) {
        commitActiveTransformBox();
    }
    clearPendingErasedState();
    m_isDraggingTool = false;
    m_selectedTextNormRect = QRectF();
    bool wasContinuous = m_continuousMode;
    m_continuousMode = false;
    setToolMode(PdfToolMode::SelectText);
    update();
    if (wasContinuous) {
        emit continuousModeExited();
    }
}

void PdfPageWidget::onTransformBoxApplied()
{
    QRect box = m_transformBox->boxRect();
    if (box.width() <= 2 || box.height() <= 2) {
        clearPendingErasedState();
        m_transformBox->dismiss();
        setToolMode(PdfToolMode::SelectText);
        return;
    }

    int pageW = width();
    int pageH = height();
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageW = m_pageLayouts[m_activePageIndex].rect.width();
        pageH = m_pageLayouts[m_activePageIndex].rect.height();
    }

    // Normalized coordinates [0.0, 1.0] across active page
    QRectF normRect(static_cast<double>(box.x()) / pageW,
                    static_cast<double>(box.y()) / pageH,
                    static_cast<double>(box.width()) / pageW,
                    static_cast<double>(box.height()) / pageH);

    QImage annImg;
    switch (m_transformBox->itemType()) {
    case PdfTransformItemType::Signature:
    case PdfTransformItemType::Image:
        annImg = m_transformBox->image();
        break;

    case PdfTransformItemType::Redaction: {
        annImg = QImage(box.size(), QImage::Format_ARGB32_Premultiplied);
        annImg.fill(m_transformBox ? m_transformBox->redactionColor() : Qt::black);
        break;
    }

    case PdfTransformItemType::Watermark: {
        annImg = QImage(box.size(), QImage::Format_ARGB32_Premultiplied);
        annImg.fill(Qt::transparent);
        QPainter p(&annImg);
        PdfTransformBox::drawWatermarkStamp(p, QRect(QPoint(0, 0), box.size()),
                                            m_transformBox->text(),
                                            m_transformBox->watermarkAngle(),
                                            m_transformBox->watermarkOpacity(),
                                            m_transformBox->textColor(),
                                            m_transformBox->watermarkStyle());
        break;
    }

    case PdfTransformItemType::Shape: {
        annImg = PdfCanvasTools::renderShapeStamp(m_transformBox->shapeType(),
                                                  m_transformBox->shapeStrokeWidth(),
                                                  m_transformBox->shapeStrokeColor(),
                                                  m_transformBox->shapeFillColor(),
                                                  box.size(),
                                                  m_transformBox->shapeLocalStart(),
                                                  m_transformBox->shapeLocalEnd());
        break;
    }

    case PdfTransformItemType::Text: {
        QString text = m_transformBox->text();
        if (text.trimmed().isEmpty()) {
            if (m_transformBox->textBgMode() != PdfTextBgMode::Transparent) {
                if (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty()) {
                    int patchW = std::max(4, qRound(m_pendingErasedNormRect.width() * pageW));
                    int patchH = std::max(4, qRound(m_pendingErasedNormRect.height() * pageH));
                    QImage patchImg(patchW, patchH, QImage::Format_ARGB32_Premultiplied);
                    patchImg.fill(m_pendingErasedBgColor);
                    m_permanentlyErasedBoxIndices.unite(m_pendingErasedBoxIndices);
                    emit addStampAnnotationRequested(patchImg, m_pendingErasedNormRect);
                }
            } else {
                if (m_pendingErasedPtRect.isValid() && !m_pendingErasedPtRect.isEmpty()) {
                    emit eraseOriginalTextRequested(m_activePageIndex, m_pendingErasedPtRect, m_pendingErasedNormRect);
                }
            }
            clearPendingErasedState();
            m_selectedTextNormRect = QRectF();
            m_transformBox->dismiss();
            setToolMode(PdfToolMode::SelectText);
            emit inPlaceTextEditFinished();
            update();
            return;
        }

        // Real-time collision guardrail: inform user if borders overlap existing neighboring text, but allow placement
        updateTransformBoxCollision();
        if (m_transformBox->hasCollision()) {
            QToolTip::showText(QCursor::pos(), tr("⚠️ Testo applicato con sovrapposizione"));
        }

        m_permanentlyErasedBoxIndices.unite(m_pendingErasedBoxIndices);

        // 1. If modifying, replacing, or moving existing text:
        // When text background mode is NOT Transparent (e.g. Auto, Feathered, Custom),
        // apply the user-selected background patch to cleanly wipe out the original text area.
        if (m_transformBox->textBgMode() != PdfTextBgMode::Transparent) {
            if (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty()) {
                int origW = std::max(4, qRound(m_pendingErasedNormRect.width() * pageW));
                int origH = std::max(4, qRound(m_pendingErasedNormRect.height() * pageH));
                QImage origWipeImg;
                QColor wipeColor = (m_transformBox->textBgColor().isValid()) ? m_transformBox->textBgColor() : m_pendingErasedBgColor;
                if (m_transformBox->textBgMode() == PdfTextBgMode::Feathered) {
                    origWipeImg = PdfCanvasTools::createFeatheredPatch(QSize(origW, origH), wipeColor);
                } else {
                    origWipeImg = QImage(origW, origH, QImage::Format_ARGB32_Premultiplied);
                    origWipeImg.fill(wipeColor);
                }
                emit addStampAnnotationRequested(origWipeImg, m_pendingErasedNormRect);
            }
        } else {
            // When text background mode IS Transparent, NEVER stamp an opaque solid rectangular patch!
            // Instead, request vector text removal so background graphics, grids, and photos remain intact.
            if (m_pendingErasedPtRect.isValid() && !m_pendingErasedPtRect.isEmpty()) {
                emit eraseOriginalTextRequested(m_activePageIndex, m_pendingErasedPtRect, m_pendingErasedNormRect);
            }
        }

        // 2. Generate background patch for the NEW text box ONLY if user explicitly requested a non-transparent background
        QImage bgImg;
        bool needBgPatch = (m_transformBox->textBgMode() != PdfTextBgMode::Transparent);
        QRectF bgNormRect = normRect;

        if (needBgPatch) {
            QColor patchColor = (m_transformBox->textBgColor().isValid()) ? m_transformBox->textBgColor() : m_pendingErasedBgColor;
            int patchW = std::max(4, qRound(bgNormRect.width() * pageW));
            int patchH = std::max(4, qRound(bgNormRect.height() * pageH));
            if (m_transformBox->textBgMode() == PdfTextBgMode::Feathered) {
                bgImg = PdfCanvasTools::createFeatheredPatch(QSize(patchW, patchH), patchColor);
            } else {
                bgImg = QImage(patchW, patchH, QImage::Format_ARGB32_Premultiplied);
                bgImg.fill(patchColor);
            }
        }

        // Give FreeText annotations a generous width and height buffer
        // so Poppler's internal PDF font metrics and clipping rect never prematurely wrap or truncate words at line ends or descenders.
        QRectF textNormRect = normRect;
        if (pageW > 0 && pageH > 0) {
            QFont annotFont = m_transformBox->textFont();
            QFontMetricsF fmPt(annotFont);
            double fontPt = annotFont.pointSizeF() > 0.0 ? annotFont.pointSizeF() : annotFont.pointSize();
            if (fontPt <= 0.0) fontPt = 12.0;

            double pageWPt = (m_scaleX > 0.001) ? (pageW / m_scaleX) : (pageW * 72.0 / 96.0);
            double pageHPt = (m_scaleY > 0.001) ? (pageH / m_scaleY) : (pageH * 72.0 / 96.0);
            double boxWPt = (m_scaleX > 0.001) ? (box.width() / m_scaleX) : box.width();
            double boxHPt = (m_scaleY > 0.001) ? (box.height() / m_scaleY) : box.height();

            const QStringList textLines = text.split(QLatin1Char('\n'));
            double maxTextWPt = 0.0;
            for (const QString &line : textLines) {
                maxTextWPt = std::max(maxTextWPt, fmPt.horizontalAdvance(line));
            }
            double neededHPt = std::max(boxHPt, static_cast<double>(textLines.size()) * fmPt.lineSpacing());

            double bufferWPt = std::max(24.0, fontPt * 1.5);
            double bufferHPt = std::max(12.0, fontPt * 0.8);

            double finalWPt = std::max(boxWPt, maxTextWPt) + bufferWPt;
            double finalHPt = std::max(boxHPt, neededHPt) + bufferHPt;

            textNormRect.setWidth(finalWPt / pageWPt);
            textNormRect.setHeight(finalHPt / pageHPt);

            if (textNormRect.right() > 1.0) {
                textNormRect.setWidth(std::max(normRect.width(), 1.0 - textNormRect.x()));
            }
            if (textNormRect.bottom() > 1.0) {
                textNormRect.setHeight(std::max(normRect.height(), 1.0 - textNormRect.y()));
            }
        }

        clearPendingErasedState();
        m_selectedTextNormRect = QRectF();

        emit addTextAnnotationRequested(text,
                                        m_transformBox->textFont(),
                                        m_transformBox->textColor(),
                                        textNormRect,
                                        needBgPatch,
                                        bgImg,
                                        bgNormRect);
        m_transformBox->dismiss();
        setToolMode(PdfToolMode::SelectText);
        emit inPlaceTextEditFinished();
        update();
        return;
    }

    default:
        break;
    }

    if (!annImg.isNull()) {
        emit addStampAnnotationRequested(annImg, normRect);
    }

    clearPendingErasedState();
    m_transformBox->dismiss();
    if (!m_continuousMode) {
        setToolMode(PdfToolMode::SelectText);
    }
    update();
}

void PdfPageWidget::onTransformBoxApplyAndContinue()
{
    QRect box = m_transformBox->boxRect();
    if (box.width() <= 2 || box.height() <= 2) {
        return;
    }

    // Save parameters before committing so we can replicate the transform box
    PdfTransformItemType itemType = m_transformBox->itemType();
    QImage itemImg = m_transformBox->image();
    QString itemText = m_transformBox->text();
    QFont itemFont = m_transformBox->textFont();
    QColor itemColor = m_transformBox->textColor();
    bool itemWhiteout = m_transformBox->isWhiteout();
    qreal itemAngle = m_transformBox->watermarkAngle();
    qreal itemOpacity = m_transformBox->watermarkOpacity();
    int itemShapeType = m_transformBox->shapeType();
    int itemShapeWidth = m_transformBox->shapeStrokeWidth();
    QColor itemShapeStroke = m_transformBox->shapeStrokeColor();
    QColor itemShapeFill = m_transformBox->shapeFillColor();
    QPoint itemShapeStart = m_transformBox->shapeLocalStart();
    QPoint itemShapeEnd = m_transformBox->shapeLocalEnd();

    // 1. Commit current item to PDF document
    int pageW = width();
    int pageH = height();
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageW = m_pageLayouts[m_activePageIndex].rect.width();
        pageH = m_pageLayouts[m_activePageIndex].rect.height();
    }

    QRectF normRect(static_cast<double>(box.x()) / pageW,
                    static_cast<double>(box.y()) / pageH,
                    static_cast<double>(box.width()) / pageW,
                    static_cast<double>(box.height()) / pageH);

    QImage annImg;
    switch (itemType) {
    case PdfTransformItemType::Signature:
    case PdfTransformItemType::Image:
        annImg = itemImg;
        break;

    case PdfTransformItemType::Redaction: {
        annImg = QImage(box.size(), QImage::Format_ARGB32_Premultiplied);
        annImg.fill(m_transformBox ? m_transformBox->redactionColor() : Qt::black);
        break;
    }

    case PdfTransformItemType::Watermark: {
        annImg = QImage(box.size(), QImage::Format_ARGB32_Premultiplied);
        annImg.fill(Qt::transparent);
        QPainter p(&annImg);
        PdfTransformBox::drawWatermarkStamp(p, QRect(QPoint(0, 0), box.size()),
                                            itemText, itemAngle, itemOpacity, itemColor,
                                            m_transformBox ? m_transformBox->watermarkStyle() : 0);
        break;
    }

    case PdfTransformItemType::Shape: {
        annImg = PdfCanvasTools::renderShapeStamp(itemShapeType, itemShapeWidth, itemShapeStroke,
                                                  itemShapeFill, box.size(), itemShapeStart, itemShapeEnd);
        break;
    }

    case PdfTransformItemType::Text: {
        emit addTextAnnotationRequested(itemText, itemFont, itemColor, normRect, itemWhiteout);
        break;
    }

    default:
        break;
    }

    if (!annImg.isNull()) {
        emit addStampAnnotationRequested(annImg, normRect);
    }

    // 2. Prepare next placement with a slight offset (+30, +30) so the user sees the new item ready
    QPoint nextPos = box.center() + QPoint(30, 30);
    // Keep inside page boundaries
    if (nextPos.x() > pageW - 40) nextPos.setX(box.width() / 2 + 20);
    if (nextPos.y() > pageH - 40) nextPos.setY(box.height() / 2 + 20);

    switch (itemType) {
    case PdfTransformItemType::Signature:
        startSignature(itemImg, nextPos);
        break;
    case PdfTransformItemType::Image:
        startInsertImage(itemImg, nextPos);
        break;
    case PdfTransformItemType::Watermark:
        startWatermark(itemText, itemAngle, itemOpacity, itemColor, nextPos, m_transformBox ? m_transformBox->watermarkStyle() : 0);
        break;
    case PdfTransformItemType::Redaction: {
        QRect nextRect(box.topLeft() + QPoint(30, 30), box.size());
        m_transformBox->showRedaction(nextRect);
        break;
    }
    case PdfTransformItemType::Shape: {
        QRect nextRect(box.topLeft() + QPoint(30, 30), box.size());
        m_transformBox->showShape(itemShapeType, itemShapeWidth, itemShapeStroke, itemShapeFill, nextRect, itemShapeStart, itemShapeEnd);
        break;
    }
    case PdfTransformItemType::Text:
        startTypewriter(itemText, itemFont, itemColor, itemWhiteout, nextPos);
        break;
    default:
        m_transformBox->dismiss();
        setToolMode(PdfToolMode::SelectText);
        break;
    }

    update();
}

void PdfPageWidget::onTransformBoxCancelled()
{
    clearPendingErasedState();
    m_selectedTextNormRect = QRectF();
    bool wasShape = (m_transformBox && m_transformBox->itemType() == PdfTransformItemType::Shape);
    m_transformBox->dismiss();
    if (wasShape) {
        setToolMode(PdfToolMode::Shape);
    } else {
        setToolMode(PdfToolMode::SelectText);
    }
    emit inPlaceTextEditFinished();
    update();
}

void PdfPageWidget::onTransformBoxEditRequested()
{
    if (!m_transformBox) return;

    QPoint currentCenter = m_transformBox->boxRect().center();

    switch (m_transformBox->itemType()) {
    case PdfTransformItemType::Text: {
        m_transformBox->setPlacementMode(false);
        if (m_transformBox->inlineTextEdit()) {
            m_transformBox->inlineTextEdit()->show();
            m_transformBox->inlineTextEdit()->setFocus();
            m_transformBox->inlineTextEdit()->selectAll();
        }
        updateTransformBoxCollision();
        emit inPlaceTextEditStarted(m_transformBox->text(), m_transformBox->textFont(),
                                    m_transformBox->textColor(), m_transformBox->textBgMode(),
                                    m_pendingErasedBgColor);
        break;
    }

    case PdfTransformItemType::AreaSelection: {
        QRect r = m_transformBox->boxRect();
        if (r.width() < 4 || r.height() < 4) {
            r = m_selectedAreaRect;
        }
        clearAreaSelection();
        if (r.width() < 4 || r.height() < 4 || m_scaleX <= 0.0 || m_scaleY <= 0.0) {
            break;
        }

        QRectF areaPtRect(r.x() / m_scaleX, r.y() / m_scaleY, r.width() / m_scaleX, r.height() / m_scaleY);
        clearPendingErasedState();

        // Collect all text boxes whose center or majority area lies inside the selected area rectangle
        m_selectedIndices.clear();
        for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
            if (!m_textBoxes[i]) continue;
            QRectF b = m_textBoxes[i]->boundingBox();
            if (areaPtRect.contains(b.center()) ||
                (areaPtRect.intersects(b) && areaPtRect.intersected(b).width() >= b.width() * 0.35)) {
                m_selectedIndices.push_back(i);
                m_pendingErasedBoxIndices.insert(i);
            }
        }

        if (!m_selectedIndices.empty()) {
            captureSelectedTextFormat();
        }
        QString extractedText = selectedText();
        if (extractedText.isEmpty() && m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
            std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
            if (p) {
                extractedText = p->text(areaPtRect).trimmed();
            }
        }

        double pageWPt = width() / m_scaleX;
        double pageHPt = height() / m_scaleY;
        if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
            std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
            if (p) {
                pageWPt = p->pageSize().width();
                pageHPt = p->pageSize().height();
            }
        }

        m_selectedTextPtRect = areaPtRect;
        m_selectedTextPageIndex = m_activePageIndex;
        m_pendingErasedPtRect = areaPtRect;
        if (pageWPt > 0.0 && pageHPt > 0.0) {
            m_pendingErasedNormRect = QRectF(std::max(0.0, areaPtRect.x() / pageWPt),
                                             std::max(0.0, areaPtRect.y() / pageHPt),
                                             std::min(1.0, areaPtRect.width() / pageWPt),
                                             std::min(1.0, areaPtRect.height() / pageHPt));
            m_selectedTextNormRect = m_pendingErasedNormRect;
        }

        QColor sampledBg = sampleBackgroundColorForRect(m_activePageIndex, areaPtRect);
        m_pendingErasedBgColor = sampledBg;
        updatePendingErasedPreviewPatch();

        QFont font = (!m_selectedIndices.empty() && m_lastCopiedTextFormat.valid) ? m_lastCopiedTextFormat.font : m_typewriterFont;
        QColor color = (!m_selectedIndices.empty() && m_lastCopiedTextFormat.valid) ? m_lastCopiedTextFormat.color : m_typewriterColor;

        m_selectedIndices.clear();
        emit selectionChanged(false);
        update();

        QPoint targetTopLeft(r.left(), r.top());
        int suggestedW = r.width() + 8;
        setToolMode(PdfToolMode::Typewriter);
        PdfTextBgMode bgMode = PdfTextBgMode::Transparent;
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showText(extractedText, font, color, bgMode, sampledBg,
                                 QRect(targetTopLeft.x(), targetTopLeft.y(), suggestedW, r.height()),
                                 targetTopLeft);
        m_transformBox->setTextModified(true);
        if (m_transformBox->inlineTextEdit()) {
            m_transformBox->inlineTextEdit()->setFocus();
            m_transformBox->inlineTextEdit()->selectAll();
        }
        updateTransformBoxCollision();
        emit inPlaceTextEditStarted(extractedText, font, color, bgMode, sampledBg);
        break;
    }

    case PdfTransformItemType::Watermark: {
        PdfWatermarkDialog dlg(window());
        dlg.setWatermarkText(m_transformBox->text());
        dlg.setAngle(m_transformBox->watermarkAngle());
        dlg.setOpacity(m_transformBox->watermarkOpacity());
        dlg.setColor(m_transformBox->textColor());
        dlg.setStyle(m_transformBox->watermarkStyle());

        // Connect real-time update so user sees effects live on canvas!
        connect(&dlg, &PdfWatermarkDialog::watermarkChanged, m_transformBox,
                &PdfTransformBox::updateWatermark);

        QString origText = m_transformBox->text();
        qreal origAngle = m_transformBox->watermarkAngle();
        qreal origOpacity = m_transformBox->watermarkOpacity();
        QColor origColor = m_transformBox->textColor();
        int origStyle = m_transformBox->watermarkStyle();

        if (dlg.exec() != QDialog::Accepted) {
            // Revert live preview if user cancels
            m_transformBox->updateWatermark(origText, origAngle, origOpacity, origColor, origStyle);
        }
        break;
    }

    case PdfTransformItemType::Signature: {
        PdfSignatureDialog dlg(window());
        if (dlg.exec() == QDialog::Accepted) {
            QImage sigImg = dlg.signatureImage();
            if (!sigImg.isNull()) {
                m_transformBox->showImage(sigImg, currentCenter);
            }
        }
        break;
    }

    case PdfTransformItemType::Image: {
        QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (startDir.isEmpty() || !QDir(startDir).exists()) {
            const QString candidate = QDir::homePath() + "/Immagini";
            startDir = QDir(candidate).exists() ? candidate : QDir::homePath();
        }

        QString filePath = QFileDialog::getOpenFileName(
            window(),
            tr("Select Image to Insert"),
            startDir,
            tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg *.tiff);;All Files (*)")
        );
        if (!filePath.isEmpty()) {
            QImage img(filePath);
            if (!img.isNull()) {
                m_transformBox->showImage(img, currentCenter, QSize(200, 200), PdfTransformItemType::Image);
            }
        }
        break;
    }

    default:
        break;
    }
}

void PdfPageWidget::commitShapeDrawing(const QRect &rect, const QPoint &startPt, const QPoint &endPt, int overrideShapeType)
{
    int pageW = width();
    int pageH = height();
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageW = m_pageLayouts[m_activePageIndex].rect.width();
        pageH = m_pageLayouts[m_activePageIndex].rect.height();
    }
    if (pageW <= 0 || pageH <= 0) return;

    int typeToUse = (overrideShapeType >= 0) ? overrideShapeType : m_shapeType;
    QRect targetRect = rect;
    QPoint localStart(-1, -1);
    QPoint localEnd(-1, -1);

    if (typeToUse == 2 || typeToUse == 3 || typeToUse == 8) {
        if (startPt == QPoint(-1, -1) || endPt == QPoint(-1, -1)) {
            if (rect.width() <= 2 || rect.height() <= 2) return;
        } else {
            // Line, Arrow, or Double Arrow: preserve exact direction, vector, and inclination
            int minX = std::min(startPt.x(), endPt.x());
            int maxX = std::max(startPt.x(), endPt.x());
            int minY = std::min(startPt.y(), endPt.y());
            int maxY = std::max(startPt.y(), endPt.y());

            int padding = static_cast<int>(std::ceil(std::max(12.0, static_cast<double>(m_shapeStrokeWidth) * 3.0)));
            int padLeft = std::clamp(padding, 0, minX);
            int padTop = std::clamp(padding, 0, minY);
            int padRight = std::clamp(padding, 0, std::max(0, pageW - 1 - maxX));
            int padBottom = std::clamp(padding, 0, std::max(0, pageH - 1 - maxY));

            targetRect = QRect(minX - padLeft, minY - padTop,
                               (maxX - minX) + padLeft + padRight,
                               (maxY - minY) + padTop + padBottom);

            if (targetRect.width() < 2 || targetRect.height() < 2) return;

            localStart = startPt - targetRect.topLeft();
            localEnd = endPt - targetRect.topLeft();
        }
    } else {
        targetRect = rect.intersected(QRect(0, 0, pageW, pageH));
        if (targetRect.width() <= 2 || targetRect.height() <= 2) return;
    }

    QImage shapeImg = PdfCanvasTools::renderShapeStamp(typeToUse, m_shapeStrokeWidth,
                                                       m_shapeStrokeColor, m_shapeFillColor,
                                                       targetRect.size(), localStart, localEnd);
    if (shapeImg.isNull()) return;

    double nx = std::clamp(static_cast<double>(targetRect.x()) / pageW, 0.0, 1.0);
    double ny = std::clamp(static_cast<double>(targetRect.y()) / pageH, 0.0, 1.0);
    double nw = std::clamp(static_cast<double>(targetRect.width()) / pageW, 0.0, 1.0 - nx);
    double nh = std::clamp(static_cast<double>(targetRect.height()) / pageH, 0.0, 1.0 - ny);
    QRectF normRect(nx, ny, nw, nh);

    emit addStampAnnotationRequested(shapeImg, normRect);
    update();
}

QRect PdfPageWidget::normalizeRect(const QPoint &p1, const QPoint &p2) const
{
    return PdfCanvasTools::normalizeRect(p1, p2);
}

void PdfPageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_transformBox) {
        m_transformBox->setGeometry(rect());
    }
}

bool PdfPageWidget::hasSelection() const
{
    return !m_selectedIndices.empty();
}

QString PdfPageWidget::selectedText() const
{
    if (m_selectedIndices.empty()) return QString();

    std::vector<int> sortedIndices = m_selectedIndices;
    std::sort(sortedIndices.begin(), sortedIndices.end());

    QString result;
    for (size_t i = 0; i < sortedIndices.size(); ++i) {
        int idx = sortedIndices[i];
        if (idx < 0 || idx >= static_cast<int>(m_textBoxes.size())) continue;

        result += m_textBoxes[idx]->text();

        if (i + 1 < sortedIndices.size()) {
            int nextIdx = sortedIndices[i + 1];
            if (nextIdx >= 0 && nextIdx < static_cast<int>(m_textBoxes.size())) {
                QRectF currR = m_textBoxes[idx]->boundingBox();
                QRectF nextR = m_textBoxes[nextIdx]->boundingBox();

                // Check vertical distance to detect line and paragraph breaks
                double vertDiff = nextR.center().y() - currR.center().y();
                double lineH = std::max(6.0, currR.height());

                if (vertDiff > lineH * 0.45) {
                    if (vertDiff > lineH * 1.4) {
                        result += QStringLiteral("\n\n");
                    } else {
                        result += QStringLiteral("\n");
                    }
                } else if (m_textBoxes[idx]->hasSpaceAfter() || nextR.left() > currR.right() + 1.0) {
                    result += QStringLiteral(" ");
                }
            }
        } else if (m_textBoxes[idx]->hasSpaceAfter()) {
            result += QStringLiteral(" ");
        }
    }
    return result.trimmed();
}

void PdfPageWidget::captureSelectedTextFormat()
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    m_lastCopiedTextFormat.hasMultipleStyles = false;

    // 1. Calculate representative point size across the selected text boxes via mode histogram
    QMap<int, int> sizeFrequency; // size in half-points (e.g. 12.0pt -> 24)
    double minH = 1e9;
    double maxH = 0.0;
    int validBoxes = 0;
    QRectF selBoundsPt;

    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            double h = m_textBoxes[idx]->boundingBox().height();
            if (h > 0.0) {
                if (h < minH) minH = h;
                if (h > maxH) maxH = h;
                validBoxes++;

                double pt = h * 0.75;
                int halfPt = qRound(pt * 2.0);
                sizeFrequency[halfPt]++;
            }
            if (selBoundsPt.isEmpty()) {
                selBoundsPt = m_textBoxes[idx]->boundingBox();
            } else {
                selBoundsPt = selBoundsPt.united(m_textBoxes[idx]->boundingBox());
            }
        }
    }

    int bestHalfPt = 24; // default 12pt
    int maxCount = 0;
    for (auto it = sizeFrequency.constBegin(); it != sizeFrequency.constEnd(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            bestHalfPt = it.key();
        }
    }

    double ptSize = bestHalfPt / 2.0;
    if (ptSize >= 6.0 && ptSize <= 96.0) {
        m_lastCopiedTextFormat.font.setPointSizeF(ptSize);
    } else {
        m_lastCopiedTextFormat.font.setPointSize(12);
    }

    // 2. Direct format inspection: first check existing in-memory FreeText annotations
    bool formatMatched = false;
    if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages() && selBoundsPt.isValid()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
        if (p) {
            QSizeF pSize = p->pageSizeF();
            if (pSize.width() > 0 && pSize.height() > 0) {
                auto annots = p->annotations();
                for (const auto &ann : annots) {
                    if (ann && ann->subType() == Poppler::Annotation::AText) {
                        auto *txtAnn = static_cast<Poppler::TextAnnotation*>(ann.get());
                        QRectF bNorm = txtAnn->boundary();
                        QRectF bPt(bNorm.x() * pSize.width(), bNorm.y() * pSize.height(),
                                   bNorm.width() * pSize.width(), bNorm.height() * pSize.height());
                        if (bPt.intersects(selBoundsPt) || bPt.contains(selBoundsPt.center()) || selBoundsPt.contains(bPt.center())) {
                            m_lastCopiedTextFormat.font = txtAnn->textFont();
                            m_lastCopiedTextFormat.color = txtAnn->textColor();
                            m_lastCopiedTextFormat.valid = true;
                            formatMatched = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Direct vector text format inspection using MuPDF (replaces pdftohtml XML parsing)
    if (!formatMatched && !m_filePath.isEmpty() && QFile::exists(m_filePath) && selBoundsPt.isValid() && !selBoundsPt.isEmpty()) {
        PdfDocumentOps::TextFormatInfo fmt;
        QString errMsg;
        if (PdfDocumentOps::inspectTextFormat(m_filePath, m_activePageIndex, selBoundsPt, fmt, errMsg) && fmt.valid) {
            formatMatched = true;
            m_lastCopiedTextFormat.hasMultipleStyles = fmt.hasMultipleStyles;

            if (!fmt.family.isEmpty()) {
                m_lastCopiedTextFormat.font.setFamily(fmt.family);
            }
            m_lastCopiedTextFormat.font.setBold(fmt.isBold);
            m_lastCopiedTextFormat.font.setWeight(fmt.isBold ? QFont::Bold : QFont::Normal);
            m_lastCopiedTextFormat.font.setItalic(fmt.isItalic);
            if (fmt.isBold && fmt.isItalic) {
                m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold Italic"));
            } else if (fmt.isBold) {
                m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold"));
            } else if (fmt.isItalic) {
                m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Italic"));
            }
            if (fmt.color.isValid()) {
                m_lastCopiedTextFormat.color = fmt.color;
            }
            if (fmt.pointSize > 0.0) {
                m_lastCopiedTextFormat.font.setPointSizeF(fmt.pointSize);
            }
        }
    }

    // 3. Graceful fallback to Poppler::FontIterator if inspection wasn't available
    if (!formatMatched) {
        if (validBoxes > 1 && (maxH - minH) > 4.5) {
            m_lastCopiedTextFormat.hasMultipleStyles = true;
        }

        if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
            std::unique_ptr<Poppler::FontIterator> it = m_doc->newFontIterator(m_activePageIndex);
            if (it && it->hasNext()) {
                QList<Poppler::FontInfo> fonts = it->next();
                QString selectedFamily;
                bool isBold = false;
                bool isItalic = false;

                for (const Poppler::FontInfo &fi : fonts) {
                    QString fontName = fi.name();
                    int plusIdx = fontName.indexOf('+');
                    if (plusIdx != -1) {
                        fontName = fontName.mid(plusIdx + 1);
                    }

                    QString lower = fontName.toLower();
                    if (lower.contains(QStringLiteral("emoji")) || lower.contains(QStringLiteral("symbol")) ||
                        lower.contains(QStringLiteral("dingbat")) || lower.contains(QStringLiteral("wingding")) ||
                        lower.contains(QStringLiteral("icon")) || lower.contains(QStringLiteral("pua")) ||
                        lower.contains(QStringLiteral("fontcustom"))) {
                        continue;
                    }

                    if (selectedFamily.isEmpty()) {
                        selectedFamily = PdfDocumentOps::resolveFontFamily(fontName, isBold, isItalic);
                    }
                }

                if (!selectedFamily.isEmpty()) {
                    QString resolvedFamily = QFontInfo(QFont(selectedFamily)).family();
                    m_lastCopiedTextFormat.font.setFamily(!resolvedFamily.isEmpty() ? resolvedFamily : selectedFamily);
                    m_lastCopiedTextFormat.font.setBold(isBold);
                    m_lastCopiedTextFormat.font.setItalic(isItalic);
                    if (isBold && isItalic) {
                        m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold Italic"));
                    } else if (isBold) {
                        m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold"));
                    } else if (isItalic) {
                        m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Italic"));
                    }
                }
            }
            m_lastCopiedTextFormat.color = Qt::black;
        }
    }

    m_lastCopiedTextFormat.sourceDocumentPath = m_filePath;
    m_lastCopiedTextFormat.isTextModified = false;
    m_lastCopiedTextFormat.valid = true;
}

bool PdfPageWidget::detectTextFormatFromDocument(const QString &text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || !m_doc || m_filePath.isEmpty() || !QFile::exists(m_filePath)) {
        return false;
    }

    PdfDocumentOps::TextFormatInfo fmt;
    QString errMsg;
    if (PdfDocumentOps::inspectTextFormatByText(m_filePath, m_activePageIndex, trimmed, fmt, errMsg) && fmt.valid) {
        if (!fmt.family.isEmpty()) {
            m_lastCopiedTextFormat.font.setFamily(fmt.family);
        }
        m_lastCopiedTextFormat.font.setBold(fmt.isBold);
        m_lastCopiedTextFormat.font.setWeight(fmt.isBold ? QFont::Bold : QFont::Normal);
        m_lastCopiedTextFormat.font.setItalic(fmt.isItalic);
        if (fmt.isBold && fmt.isItalic) {
            m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold Italic"));
        } else if (fmt.isBold) {
            m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold"));
        } else if (fmt.isItalic) {
            m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Italic"));
        }
        if (fmt.color.isValid()) {
            m_lastCopiedTextFormat.color = fmt.color;
        }
        if (fmt.pointSize > 0.0) {
            m_lastCopiedTextFormat.font.setPointSizeF(fmt.pointSize);
        }
        m_lastCopiedTextFormat.sourceDocumentPath = m_filePath;
        m_lastCopiedTextFormat.isTextModified = false;
        m_lastCopiedTextFormat.valid = true;
        return true;
    }

    return false;
}

void PdfPageWidget::copySelectionToClipboard()
{
    QString text = selectedText();
    if (text.isEmpty()) return;

    captureSelectedTextFormat();

    m_lastClipType = LastClipType::Text;

    QMimeData *mimeData = new QMimeData();
    mimeData->setText(text);

    QString family = m_lastCopiedTextFormat.font.family();
    double ptSize = m_lastCopiedTextFormat.font.pointSizeF();
    if (ptSize <= 0.0) {
        ptSize = m_lastCopiedTextFormat.font.pointSize() > 0 ? m_lastCopiedTextFormat.font.pointSize() : 12.0;
    }
    QString colorHex = m_lastCopiedTextFormat.color.isValid() ? m_lastCopiedTextFormat.color.name() : QStringLiteral("#000000");
    bool isBold = m_lastCopiedTextFormat.font.bold();
    bool isItalic = m_lastCopiedTextFormat.font.italic();

    QString html = QStringLiteral("<span style=\"font-family:'%1'; font-size:%2pt; color:%3; font-weight:%4; font-style:%5;\">%6</span>")
        .arg(family)
        .arg(ptSize)
        .arg(colorHex)
        .arg(isBold ? QStringLiteral("bold") : QStringLiteral("normal"))
        .arg(isItalic ? QStringLiteral("italic") : QStringLiteral("normal"))
        .arg(text.toHtmlEscaped());
    mimeData->setHtml(html);

    QJsonObject formatObj;
    formatObj[QStringLiteral("family")] = family;
    formatObj[QStringLiteral("pointSize")] = ptSize;
    formatObj[QStringLiteral("color")] = colorHex;
    formatObj[QStringLiteral("bold")] = isBold;
    formatObj[QStringLiteral("italic")] = isItalic;
    formatObj[QStringLiteral("sourceDocument")] = m_filePath;
    formatObj[QStringLiteral("pageIndex")] = m_activePageIndex;
    QJsonDocument doc(formatObj);
    mimeData->setData(QStringLiteral("application/x-bubez-text-format"), doc.toJson(QJsonDocument::Compact));

    QApplication::clipboard()->setMimeData(mimeData);
}

void PdfPageWidget::copyPageTextToClipboard()
{
    if (m_textBoxes.empty()) return;

    // Capture text format across all page boxes
    m_lastClipType = LastClipType::Text;
    m_selectedIndices.clear();
    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        m_selectedIndices.push_back(i);
    }
    captureSelectedTextFormat();
    m_selectedIndices.clear();

    QString result;
    for (size_t i = 0; i < m_textBoxes.size(); ++i) {
        result += m_textBoxes[i]->text();
        if (i + 1 < m_textBoxes.size()) {
            QRectF currR = m_textBoxes[i]->boundingBox();
            QRectF nextR = m_textBoxes[i + 1]->boundingBox();
            double vertDiff = nextR.center().y() - currR.center().y();
            double lineH = std::max(6.0, currR.height());

            if (vertDiff > lineH * 0.45) {
                if (vertDiff > lineH * 1.4) {
                    result += QStringLiteral("\n\n");
                } else {
                    result += QStringLiteral("\n");
                }
            } else if (m_textBoxes[i]->hasSpaceAfter() || nextR.left() > currR.right() + 1.0) {
                result += QStringLiteral(" ");
            }
        } else if (m_textBoxes[i]->hasSpaceAfter()) {
            result += QStringLiteral(" ");
        }
    }
    QString trimmed = result.trimmed();
    if (!trimmed.isEmpty()) {
        QMimeData *mimeData = new QMimeData();
        mimeData->setText(trimmed);

        QString family = m_lastCopiedTextFormat.font.family();
        double ptSize = m_lastCopiedTextFormat.font.pointSizeF();
        if (ptSize <= 0.0) {
            ptSize = m_lastCopiedTextFormat.font.pointSize() > 0 ? m_lastCopiedTextFormat.font.pointSize() : 12.0;
        }
        QString colorHex = m_lastCopiedTextFormat.color.isValid() ? m_lastCopiedTextFormat.color.name() : QStringLiteral("#000000");
        bool isBold = m_lastCopiedTextFormat.font.bold();
        bool isItalic = m_lastCopiedTextFormat.font.italic();

        QString html = QStringLiteral("<span style=\"font-family:'%1'; font-size:%2pt; color:%3; font-weight:%4; font-style:%5;\">%6</span>")
            .arg(family)
            .arg(ptSize)
            .arg(colorHex)
            .arg(isBold ? QStringLiteral("bold") : QStringLiteral("normal"))
            .arg(isItalic ? QStringLiteral("italic") : QStringLiteral("normal"))
            .arg(trimmed.toHtmlEscaped());
        mimeData->setHtml(html);

        QJsonObject formatObj;
        formatObj[QStringLiteral("family")] = family;
        formatObj[QStringLiteral("pointSize")] = ptSize;
        formatObj[QStringLiteral("color")] = colorHex;
        formatObj[QStringLiteral("bold")] = isBold;
        formatObj[QStringLiteral("italic")] = isItalic;
        formatObj[QStringLiteral("sourceDocument")] = m_filePath;
        formatObj[QStringLiteral("pageIndex")] = m_activePageIndex;
        QJsonDocument doc(formatObj);
        mimeData->setData(QStringLiteral("application/x-bubez-text-format"), doc.toJson(QJsonDocument::Compact));

        QApplication::clipboard()->setMimeData(mimeData);
    }
}

void PdfPageWidget::selectAll()
{
    m_selectedIndices.clear();
    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        m_selectedIndices.push_back(i);
    }
    update();
    emit selectionChanged(hasSelection());
    if (hasSelection()) {
        captureSelectedTextFormat();
    }
}

void PdfPageWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRect clip = event->rect();

    // 1. Draw Liquid Glass canvas background matching active theme
    ThemePreset preset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    QLinearGradient canvasBg(0, 0, width(), height());
    if (preset == ThemePreset::ZincGray) {
        canvasBg.setColorAt(0.0, QColor("#3e3f44"));
        canvasBg.setColorAt(0.5, QColor("#343539"));
        canvasBg.setColorAt(1.0, QColor("#2b2c30"));
    } else if (preset == ThemePreset::DarkSlate) {
        canvasBg.setColorAt(0.0, QColor("#1e2538"));
        canvasBg.setColorAt(0.5, QColor("#151a28"));
        canvasBg.setColorAt(1.0, QColor("#0e121c"));
    } else if (preset == ThemePreset::BreezeDark) {
        canvasBg.setColorAt(0.0, QColor("#1a1f2c"));
        canvasBg.setColorAt(0.5, QColor("#131620"));
        canvasBg.setColorAt(1.0, QColor("#0b0d13"));
    } else {
        canvasBg.setColorAt(0.0, QColor("#edf0f4"));
        canvasBg.setColorAt(0.5, QColor("#e2e6eb"));
        canvasBg.setColorAt(1.0, QColor("#d6dbe2"));
    }
    painter.fillRect(clip, canvasBg);

    // Single-page fallback mode (when setPageContent was called directly)
    if (m_pageLayouts.isEmpty()) {
        if (!m_pixmap.isNull()) {
            const qreal rRadius = 12.0;
            const QRectF rF(rect());
            painter.save();
            QPainterPath singleClip;
            singleClip.addRoundedRect(rF, rRadius, rRadius);
            painter.setClipPath(singleClip);
            painter.fillRect(rect(), Qt::white);
            painter.drawPixmap(0, 0, m_pixmap);
            painter.restore();
            painter.setPen(QPen(QColor(148, 163, 184, 75), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rF.adjusted(0.5, 0.5, -0.5, -0.5), rRadius, rRadius);
            if (m_isEditMode && !m_formFields.empty()) {
                PdfCanvasTools::drawAcroFormHighlights(painter, m_formFields, m_scaleX, m_scaleY);
            }
            PdfCanvasTools::drawTextSelection(painter, m_textBoxes, m_selectedIndices, m_scaleX, m_scaleY);
        }
        return;
    }

    if (!m_doc) return;

    Poppler::Page::Rotation popplerRot = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) popplerRot = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) popplerRot = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) popplerRot = Poppler::Page::Rotate270;

    // 2. Render each visible page with 12px rounded corners & soft drop shadow
    for (const auto &pl : m_pageLayouts) {
        if (!pl.rect.intersects(clip)) {
            continue;
        }

        QRect pageR = pl.rect;
        const qreal pageRadius = 12.0;
        const QRectF pageF(pageR);

        // Soft multi-layer rounded drop shadow around page
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 14));
        painter.drawRoundedRect(pageF.adjusted(-4, -1, 4, 7), pageRadius + 3.0, pageRadius + 3.0);
        painter.setBrush(QColor(0, 0, 0, 26));
        painter.drawRoundedRect(pageF.adjusted(-2, 1, 2, 4), pageRadius + 1.5, pageRadius + 1.5);

        // Fetch or render page image from LRU cache
        QPixmap px;
        if (m_pageCache.contains(pl.pageIndex)) {
            px = m_pageCache.value(pl.pageIndex);
        } else {
            std::unique_ptr<Poppler::Page> p(m_doc->page(pl.pageIndex));
            if (p) {
                const double dpr = (devicePixelRatioF() > 0.0) ? devicePixelRatioF() : 1.0;
                const double baseDpi = pl.scaleX * 72.0;
                // Render at high-density (minimum 192 DPI or 1.75x supersample) so inserted images,
                // stamps, and vector text are rasterized with full detail and never suffer from Splash downsampling blur.
                const double targetDpi = std::clamp(std::max(192.0, baseDpi * 1.75) * dpr, 72.0, 500.0);
                const double effectiveDpr = targetDpi / std::max(1.0, baseDpi);
                QImage img = p->renderToImage(targetDpi, targetDpi, -1, -1, -1, -1, popplerRot);
                if (!img.isNull()) {
                    img.setDevicePixelRatio(effectiveDpr);
                    px = QPixmap::fromImage(img);
                    m_pageCache.insert(pl.pageIndex, px);

                    // Keep cache bounded using cached hardware profile setting
                    if (m_pageCache.size() > m_maxCachePages) {
                        int furthest = pl.pageIndex;
                        int maxDist = -1;
                        for (auto it = m_pageCache.begin(); it != m_pageCache.end(); ++it) {
                            int dist = std::abs(it.key() - m_activePageIndex);
                            if (dist > maxDist) {
                                maxDist = dist;
                                furthest = it.key();
                            }
                        }
                        m_pageCache.remove(furthest);
                    }
                }
            }
        }

        // Clip page paper and rendered content to 12px rounded corners
        painter.save();
        QPainterPath pageClipPath;
        pageClipPath.addRoundedRect(pageF, pageRadius, pageRadius);
        painter.setClipPath(pageClipPath);
        painter.fillRect(pageR, Qt::white);
        if (!px.isNull()) {
            painter.drawPixmap(pageR.topLeft(), px);
        }
        painter.restore();

        // Crisp rounded inner rim
        painter.setPen(QPen(QColor(148, 163, 184, 75), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(pageF.adjusted(0.5, 0.5, -0.5, -0.5), pageRadius, pageRadius);

        // AcroForms, pending background wipe, collision highlights & selection for active page
        if (pl.pageIndex == m_activePageIndex) {
            painter.save();
            painter.translate(pageR.topLeft());
            if (m_pendingErasedPtRect.isValid() && !m_pendingErasedPtRect.isEmpty()) {
                QRect wipedPix(qRound(m_pendingErasedPtRect.x() * pl.scaleX),
                               qRound(m_pendingErasedPtRect.y() * pl.scaleY),
                               qRound(m_pendingErasedPtRect.width() * pl.scaleX),
                               qRound(m_pendingErasedPtRect.height() * pl.scaleY));
                if (!m_transformBox || m_transformBox->textBgMode() != PdfTextBgMode::Transparent) {
                    painter.fillRect(wipedPix, m_pendingErasedBgColor);
                } else if (!m_pendingErasedPreviewPatch.isNull()) {
                    painter.drawImage(wipedPix, m_pendingErasedPreviewPatch);
                }
            }
            if (!m_collidingTextPtRects.isEmpty()) {
                painter.setPen(QPen(QColor(239, 68, 68), 1.5));
                painter.setBrush(QColor(239, 68, 68, 65));
                for (const QRectF &colPt : m_collidingTextPtRects) {
                    QRect colPix(qRound(colPt.x() * pl.scaleX),
                                 qRound(colPt.y() * pl.scaleY),
                                 qRound(colPt.width() * pl.scaleX),
                                 qRound(colPt.height() * pl.scaleY));
                    painter.drawRect(colPix);
                }
            }
            if (m_isEditMode && !m_formFields.empty()) {
                PdfCanvasTools::drawAcroFormHighlights(painter, m_formFields, pl.scaleX, pl.scaleY);
            }
            PdfCanvasTools::drawTextSelection(painter, m_textBoxes, m_selectedIndices, pl.scaleX, pl.scaleY);
            painter.restore();
        }
    }

    // 3. Active Tool Rubber-band
    if (m_isDraggingTool && m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        const auto &pl = m_pageLayouts[m_activePageIndex];
        painter.save();
        painter.translate(pl.rect.topLeft());
        if (m_toolMode == PdfToolMode::Shape && m_shapeType == 9) {
            // Draw freehand stroke for Smart Shape
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(m_shapeStrokeColor, m_shapeStrokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (m_smartShapePoints.size() >= 2) {
                painter.drawPolyline(m_smartShapePoints.constData(), m_smartShapePoints.size());
            }
        } else {
            QPoint currentPt = m_dragCurrent;
            if (m_toolMode == PdfToolMode::Shape &&
                (m_shapeType == 2 || m_shapeType == 3 || m_shapeType == 8) &&
                (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                int dx = currentPt.x() - m_dragStart.x();
                int dy = currentPt.y() - m_dragStart.y();
                double dist = std::hypot(dx, dy);
                if (dist > 1.0) {
                    double angle = std::atan2(dy, dx);
                    double snappedAngle = std::round(angle / (M_PI / 4.0)) * (M_PI / 4.0);
                    currentPt = m_dragStart + QPoint(qRound(dist * std::cos(snappedAngle)),
                                                     qRound(dist * std::sin(snappedAngle)));
                }
            }
            QRect r = PdfCanvasTools::normalizeRect(m_dragStart, currentPt);
            int snapW = qRound(r.width() * 200.0 / (72.0 * pl.scaleX));
            int snapH = qRound(r.height() * 200.0 / (72.0 * pl.scaleY));
            int modeCode = (m_toolMode == PdfToolMode::Snapshot || m_toolMode == PdfToolMode::SelectArea) ? 1
                         : (m_toolMode == PdfToolMode::Redaction) ? 3
                         : (m_toolMode == PdfToolMode::Shape) ? 4
                         : (m_toolMode == PdfToolMode::Highlight) ? (m_highlighterSpotlight ? 6 : 5) : 0;
            QColor hlColor = m_highlighterColor;
            hlColor.setAlphaF(m_highlighterOpacity);
            PdfCanvasTools::drawRubberBand(painter, r, modeCode, snapW, snapH,
                                           m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor,
                                           (m_toolMode == PdfToolMode::Highlight ? hlColor : m_shapeFillColor),
                                           m_dragStart, currentPt);
        }
        painter.restore();
    }
}

int PdfPageWidget::findClosestBox(const QPoint &pt) const
{
    return PdfCanvasTools::findClosestTextBox(pt, m_textBoxes, m_scaleX, m_scaleY);
}

void PdfPageWidget::updateSelection(const QPoint &startPt, const QPoint &endPt)
{
    if ((endPt - startPt).manhattanLength() < 4) {
        if (!m_selectedIndices.empty()) {
            m_selectedIndices.clear();
            update();
            emit selectionChanged(false);
        }
        return;
    }

    int startIdx = findClosestBox(startPt);
    int endIdx = findClosestBox(endPt);

    if (startIdx >= 0 && endIdx >= 0) {
        int fromIdx = std::min(startIdx, endIdx);
        int toIdx = std::max(startIdx, endIdx);

        std::vector<int> newSelection;
        newSelection.reserve(toIdx - fromIdx + 1);
        for (int i = fromIdx; i <= toIdx; ++i) {
            newSelection.push_back(i);
        }

        if (newSelection != m_selectedIndices) {
            m_selectedIndices = std::move(newSelection);
            update();
            emit selectionChanged(hasSelection());
        }
    }
}

void PdfPageWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();

    // Commit any open AcroForm inline editor
    if (m_inlineEditor && !m_inlineEditor->geometry().contains(event->pos())) {
        m_inlineEditor->deleteLater();
        m_inlineEditor = nullptr;
    }

    // Zoom Tool click handling
    if (m_toolMode == PdfToolMode::Zoom) {
        if (event->button() == Qt::LeftButton) {
            emit zoomWheelRequested(event->pos(), 1.25);
            event->accept();
            return;
        } else if (event->button() == Qt::RightButton) {
            emit zoomWheelRequested(event->pos(), 1.0 / 1.25);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        // Detect clicked page in multi-page mode
        int clickedPage = pageIndexAtPos(event->pos());
        if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
            setActivePage(clickedPage);
        }

        QRect activePR = pageRect(m_activePageIndex);
        QPoint localPos = event->pos() - activePR.topLeft();

        // Area Selection: clear selection if clicked outside active area
        if (m_hasAreaSelection) {
            if (!m_selectedAreaRect.contains(localPos)) {
                clearAreaSelection();
            }
        }

        // 0. Typewriter / Inline Text Insertion
        if (m_toolMode == PdfToolMode::Typewriter) {
            clearAreaSelection();
            if (m_transformBox) {
                if (m_transformBox->isVisible()) {
                    if (m_transformBox->boxRect().contains(localPos)) {
                        event->accept();
                        return;
                    }
                    if (!m_transformBox->text().trimmed().isEmpty()) {
                        onTransformBoxApplied();
                        // If collision guardrail blocked the commit, keep placement box active
                        if (m_transformBox->isVisible()) {
                            event->accept();
                            return;
                        }
                        if (!m_continuousMode) {
                            setToolMode(PdfToolMode::SelectText);
                            event->accept();
                            return;
                        }
                    }
                }
                m_transformBox->setGeometry(activePR);
                m_transformBox->showText(QString(), m_typewriterFont, m_typewriterColor, m_typewriterWhiteout, localPos);
                updateTransformBoxCollision();
            }
            event->accept();
            return;
        }

        // 0a-1. Modular Transform Box click-to-place: if clicking on a page outside the transform box
        if (m_transformBox && m_transformBox->isVisible()) {
            if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
                setActivePage(clickedPage);
                activePR = pageRect(m_activePageIndex);
                localPos = event->pos() - activePR.topLeft();
                m_transformBox->setGeometry(activePR);
            }
            if (!m_transformBox->boxRect().adjusted(-16, -16, 16, 16).contains(localPos)) {
                if (m_toolMode == PdfToolMode::Shape || m_transformBox->itemType() == PdfTransformItemType::Shape) {
                    m_transformBox->dismiss();
                    m_toolMode = PdfToolMode::Shape;
                    // Fall through to block 0b to immediately begin dragging the new shape
                } else {
                    QRect bRect = m_transformBox->boxRect();
                    bRect.moveCenter(localPos);
                    int pw = activePR.width();
                    int ph = activePR.height();
                    if (pw > bRect.width()) {
                        if (bRect.left() < 4) bRect.moveLeft(4);
                        if (bRect.right() > pw - 4) bRect.moveRight(pw - 4);
                    }
                    if (ph > bRect.height()) {
                        if (bRect.top() < 4) bRect.moveTop(4);
                        if (bRect.bottom() > ph - 4) bRect.moveBottom(ph - 4);
                    }
                    m_transformBox->setBoxRect(bRect);
                    update();
                    event->accept();
                    return;
                }
            } else {
                QWidget::mousePressEvent(event);
                return;
            }
        }

        // 0b. Tool Drag Modes for Shapes & Gestures (Smart Shape, Vector Shapes, Arrows, Lines)
        if (m_toolMode == PdfToolMode::Shape) {
            clearAreaSelection();
            m_isDraggingTool = true;
            m_dragStart = localPos;
            m_dragCurrent = localPos;
            m_smartShapePoints.clear();
            if (m_shapeType == 9) {
                m_smartShapePoints.append(localPos);
            }
            update();
            event->accept();
            return;
        }

        if (m_toolMode == PdfToolMode::Watermark) {
            clearAreaSelection();
            startWatermark(m_watermarkText, m_watermarkAngle, m_watermarkOpacity,
                           m_watermarkColor, localPos, m_watermarkStyle, m_watermarkSize);
            event->accept();
            return;
        }

        // 1. Tool Drag Modes (SelectArea, Snapshot, Redaction, Highlight)
        if (m_toolMode == PdfToolMode::SelectArea ||
            m_toolMode == PdfToolMode::Snapshot ||
            m_toolMode == PdfToolMode::Redaction ||
            m_toolMode == PdfToolMode::Highlight) {
            clearAreaSelection();
            m_isDraggingTool = true;
            m_dragStart = localPos;
            m_dragCurrent = localPos;
            update();
            event->accept();
            return;
        }

        // 2. AcroForms Interactive Field Interaction
        if (m_isEditMode && m_toolMode == PdfToolMode::SelectText && !m_formFields.empty()) {
            for (const auto &field : m_formFields) {
                if (!field || !field->isVisible()) continue;
                QRectF f = field->rect();
                QRect pixelRect(qRound(f.x() * m_scaleX),
                                 qRound(f.y() * m_scaleY),
                                 qRound(f.width() * m_scaleX),
                                 qRound(f.height() * m_scaleY));

                if (pixelRect.contains(localPos)) {
                    QRect canvasRect = pixelRect.translated(activePR.topLeft());
                    if (field->type() == Poppler::FormField::FormText) {
                        auto *tf = static_cast<Poppler::FormFieldText *>(field.get());
                        m_inlineEditor = new QLineEdit(this);
                        m_inlineEditor->setGeometry(canvasRect);
                        m_inlineEditor->setText(tf->text());
                        m_inlineEditor->setStyleSheet("background: #ffffff; color: #000000; border: 2px solid #3b82f6; border-radius: 2px; padding: 1px 3px; font-size: 12px;");
                        m_inlineEditor->show();
                        m_inlineEditor->setFocus();
                        m_inlineEditor->selectAll();

                        connect(m_inlineEditor, &QLineEdit::editingFinished, this, [this, tf]() {
                            if (m_inlineEditor) {
                                tf->setText(m_inlineEditor->text());
                                tf->setAppearanceText(m_inlineEditor->text());
                                m_inlineEditor->deleteLater();
                                m_inlineEditor = nullptr;
                                emit formFieldModified();
                                update();
                            }
                        });
                        event->accept();
                        return;
                    } else if (field->type() == Poppler::FormField::FormButton) {
                        auto *btn = static_cast<Poppler::FormFieldButton *>(field.get());
                        btn->setState(!btn->state());
                        emit formFieldModified();
                        update();
                        event->accept();
                        return;
                    }
                }
            }
        }

        // Hand / Pan Tool
        if (m_toolMode == PdfToolMode::Hand) {
            m_isPanning = true;
            m_panStartPos = event->globalPosition().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        // 3. Standard Text Selection
        if (m_toolMode == PdfToolMode::SelectText) {
            m_isSelecting = true;
            m_selectionStart = localPos;
            m_selectionEnd = localPos;
            m_selectedIndices.clear();
            update();
            emit selectionChanged(false);
            event->accept();
            return;
        }

    } else if (event->button() == Qt::MiddleButton || (m_spacePressed && event->button() == Qt::LeftButton)) {
        m_isPanning = true;
        m_panStartPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PdfPageWidget::mouseMoveEvent(QMouseEvent *event)
{
    QRect activePR = pageRect(m_activePageIndex);
    QPoint localPos = event->pos() - activePR.topLeft();

    if (m_isDraggingTool) {
        m_dragCurrent = localPos;
        if (m_toolMode == PdfToolMode::Shape && m_shapeType == 9) {
            m_smartShapePoints.append(localPos);
        }

        // Autoscroll viewport when dragging near top/bottom edges of the scroll area
        QScrollArea *sa = nullptr;
        QWidget *p = parentWidget();
        while (p) {
            if ((sa = qobject_cast<QScrollArea *>(p))) break;
            p = p->parentWidget();
        }
        if (sa && sa->verticalScrollBar()) {
            QPoint vpPt = sa->viewport()->mapFromGlobal(event->globalPosition().toPoint());
            int margin = 30;
            if (vpPt.y() < margin) {
                int scrollStep = qBound(5, (margin - vpPt.y()) / 2, 25);
                sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - scrollStep);
            } else if (vpPt.y() > sa->viewport()->height() - margin) {
                int scrollStep = qBound(5, (vpPt.y() - (sa->viewport()->height() - margin)) / 2, 25);
                sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() + scrollStep);
            }
        }

        update();
        event->accept();
        return;
    }

    if (m_isSelecting) {
        m_selectionEnd = localPos;
        updateSelection(m_selectionStart, m_selectionEnd);
        event->accept();
        return;
    }

    if (m_isPanning) {
        QPoint currentPos = event->globalPosition().toPoint();
        QPoint delta = currentPos - m_panStartPos;
        m_panStartPos = currentPos;

        QScrollArea *sa = nullptr;
        QWidget *p = parentWidget();
        while (p) {
            if ((sa = qobject_cast<QScrollArea *>(p))) break;
            p = p->parentWidget();
        }
        if (sa) {
            if (sa->horizontalScrollBar()) {
                sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - delta.x());
            }
            if (sa->verticalScrollBar()) {
                sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - delta.y());
            }
        }
        event->accept();
        return;
    }

    if (m_toolMode == PdfToolMode::SelectText) {
        setCursor(Qt::IBeamCursor);
    } else if (m_toolMode == PdfToolMode::SelectArea || m_toolMode == PdfToolMode::Snapshot ||
               m_toolMode == PdfToolMode::Redaction || m_toolMode == PdfToolMode::Highlight ||
               m_toolMode == PdfToolMode::Shape) {
        setCursor(Qt::CrossCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void PdfPageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QRect activePR = pageRect(m_activePageIndex);
    QPoint localPos = event->pos() - activePR.topLeft();

    if (event->button() == Qt::LeftButton && m_isDraggingTool) {
        m_isDraggingTool = false;
        QRect r = normalizeRect(m_dragStart, m_dragCurrent);

        if (m_toolMode == PdfToolMode::SelectArea) {
            if (r.width() >= 8 && r.height() >= 8) {
                if (m_transformBox) {
                    m_transformBox->setGeometry(activePR);
                    m_transformBox->showAreaSelection(r);
                }
                m_hasAreaSelection = true;
                m_selectedAreaRect = r;
                emit selectionChanged(true);
            } else {
                clearAreaSelection();
            }
            setToolMode(PdfToolMode::SelectText);

        } else if (m_toolMode == PdfToolMode::Snapshot) {
            if (r.width() >= 6 && r.height() >= 6) {
                // Convert pixel coordinates to 72 DPI point coordinates
                QRectF pointRect(r.x() / m_scaleX, r.y() / m_scaleY,
                                r.width() / m_scaleX, r.height() / m_scaleY);
                emit snapshotAreaSelected(pointRect);
            }
            setToolMode(PdfToolMode::SelectText);

        } else if (m_toolMode == PdfToolMode::Redaction) {
            if (r.width() >= 4 && r.height() >= 4) {
                int pageW = activePR.width();
                int pageH = activePR.height();
                if (pageW > 0 && pageH > 0) {
                    QRectF normRect(static_cast<double>(r.x()) / pageW,
                                   static_cast<double>(r.y()) / pageH,
                                   static_cast<double>(r.width()) / pageW,
                                   static_cast<double>(r.height()) / pageH);

                    QImage redactImg(r.size(), QImage::Format_ARGB32_Premultiplied);
                    redactImg.fill(m_redactionColor);

                    if (m_redactionPermanent) {
                        for (auto it = m_textBoxes.begin(); it != m_textBoxes.end();) {
                            QRectF b = (*it)->boundingBox();
                            QRect boxPix(qRound(b.x() * m_scaleX),
                                         qRound(b.y() * m_scaleY),
                                         qRound(b.width() * m_scaleX),
                                         qRound(b.height() * m_scaleY));
                            if (r.intersects(boxPix)) {
                                it = m_textBoxes.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        QRectF ptRect(r.x() / m_scaleX, r.y() / m_scaleY,
                                      r.width() / m_scaleX, r.height() / m_scaleY);
                        emit eraseOriginalTextRequested(m_activePageIndex, ptRect, normRect);
                    }

                    emit addStampAnnotationRequested(redactImg, normRect);
                    update();
                }
            }
            if (!m_continuousMode) {
                setToolMode(PdfToolMode::SelectText);
            }

        } else if (m_toolMode == PdfToolMode::Highlight) {
            if (r.width() >= 4 && r.height() >= 4) {
                commitHighlightDrawing(r);
            }
            if (!m_continuousMode) {
                setToolMode(PdfToolMode::SelectText);
            }

        } else if (m_toolMode == PdfToolMode::Shape) {
            int finalShapeType = m_shapeType;
            QRect shapeRect = r;
            QPoint startPt = m_dragStart;
            QPoint endPt = m_dragCurrent;

            if (m_shapeType == 9) { // Smart Shape gesture recognition
                RecognizedShapeResult rec = PdfCanvasTools::recognizeSmartShape(m_smartShapePoints);
                finalShapeType = rec.shapeType;
                shapeRect = rec.boundingBox;
                startPt = rec.startPt;
                endPt = rec.endPt;
                m_smartShapePoints.clear();
                if (!rec.name.isEmpty()) {
                    QToolTip::showText(mapToGlobal(localPos), tr("✨ Recognized shape: %1").arg(rec.name));
                }
            }

            if ((event->modifiers() & Qt::ShiftModifier) &&
                (finalShapeType == 2 || finalShapeType == 3 || finalShapeType == 8)) {
                int dx = endPt.x() - startPt.x();
                int dy = endPt.y() - startPt.y();
                double dist = std::hypot(dx, dy);
                if (dist > 1.0) {
                    double angle = std::atan2(dy, dx);
                    double snappedAngle = std::round(angle / (M_PI / 4.0)) * (M_PI / 4.0);
                    endPt = startPt + QPoint(qRound(dist * std::cos(snappedAngle)),
                                             qRound(dist * std::sin(snappedAngle)));
                }
            }

            if (finalShapeType == 2 || finalShapeType == 3 || finalShapeType == 8) { // Arrow, Line, Double Arrow
                int dragDist = (endPt - startPt).manhattanLength();
                if (dragDist < 10) {
                    // Clic singolo senza trascinamento: linea/freccia orizzontale da 160px
                    startPt = m_dragStart;
                    endPt = m_dragStart + QPoint(160, 0);
                }

                int padding = std::max(16, m_shapeStrokeWidth * 4);
                int minX = std::min(startPt.x(), endPt.x()) - padding;
                int maxX = std::max(startPt.x(), endPt.x()) + padding;
                int minY = std::min(startPt.y(), endPt.y()) - padding;
                int maxY = std::max(startPt.y(), endPt.y()) + padding;
                QRect box(minX, minY, std::max(30, maxX - minX), std::max(30, maxY - minY));

                QPoint localStart = startPt - box.topLeft();
                QPoint localEnd = endPt - box.topLeft();

                if (m_transformBox) {
                    m_transformBox->setGeometry(activePR);
                    m_transformBox->showShape(finalShapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor, box, localStart, localEnd);
                }
            } else {
                if (shapeRect.width() < 12 && shapeRect.height() < 12) {
                    shapeRect = QRect(m_dragStart.x() - 80, m_dragStart.y() - 50, 160, 100);
                }
                if (m_transformBox) {
                    m_transformBox->setGeometry(activePR);
                    m_transformBox->showShape(finalShapeType, m_shapeStrokeWidth, m_shapeStrokeColor, m_shapeFillColor, shapeRect);
                }
            }
        }

        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        m_selectionEnd = localPos;
        updateSelection(m_selectionStart, m_selectionEnd);
        if (hasSelection()) {
            QRectF unionPtRect;
            for (int idx : m_selectedIndices) {
                if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
                    if (unionPtRect.isNull()) {
                        unionPtRect = m_textBoxes[idx]->boundingBox();
                    } else {
                        unionPtRect = unionPtRect.united(m_textBoxes[idx]->boundingBox());
                    }
                }
            }
            if (!unionPtRect.isNull() && unionPtRect.width() > 0 && unionPtRect.height() > 0) {
                m_selectedTextPtRect = unionPtRect;
                m_selectedTextPageIndex = m_activePageIndex;

                double pageWPt = width() / m_scaleX;
                double pageHPt = height() / m_scaleY;
                if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
                    std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
                    if (p) {
                        pageWPt = p->pageSize().width();
                        pageHPt = p->pageSize().height();
                    }
                }
                if (pageWPt > 0 && pageHPt > 0) {
                    m_selectedTextNormRect = QRectF(m_selectedTextPtRect.x() / pageWPt,
                                                    m_selectedTextPtRect.y() / pageHPt,
                                                    m_selectedTextPtRect.width() / pageWPt,
                                                    m_selectedTextPtRect.height() / pageHPt);
                }
            }
            captureSelectedTextFormat();
        }
        event->accept();
        return;
    }

    if ((event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) && m_isPanning) {
        m_isPanning = false;
        setCursor((m_toolMode == PdfToolMode::Hand || m_spacePressed) ? Qt::OpenHandCursor : (m_toolMode == PdfToolMode::SelectText ? Qt::IBeamCursor : Qt::ArrowCursor));
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PdfPageWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_toolMode == PdfToolMode::SelectText) {
        int clickedPage = pageIndexAtPos(event->pos());
        if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
            setActivePage(clickedPage);
        }

        QRect activePR = pageRect(m_activePageIndex);
        QPoint localPos = event->pos() - activePR.topLeft();
        int idx = findClosestBox(localPos);
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            const auto &box = m_textBoxes[idx];
            QRectF pixelRect(box->boundingBox().x() * m_scaleX,
                             box->boundingBox().y() * m_scaleY,
                             box->boundingBox().width() * m_scaleX,
                             box->boundingBox().height() * m_scaleY);
            if (pixelRect.adjusted(-5, -5, 5, 5).contains(localPos)) {
                // If there's already a multi-word selection containing this idx, keep it; otherwise select this word
                bool isPartOfExisting = false;
                for (int s : m_selectedIndices) {
                    if (s == idx) { isPartOfExisting = true; break; }
                }
                if (!isPartOfExisting) {
                    m_selectedIndices = {idx};
                }
                update();
                emit selectionChanged(true);

                // If currently in Edit Mode, double-click immediately activates direct In-Place text editing!
                if (m_isEditMode) {
                    startInPlaceTextEdit();
                }

                event->accept();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PdfPageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_toolMode == PdfToolMode::Zoom || (event->modifiers() & Qt::ControlModifier)) {
        double factor = (event->angleDelta().y() > 0) ? 1.25 : (1.0 / 1.25);
        emit zoomWheelRequested(event->position().toPoint(), factor);
        event->accept();
        return;
    }
    // Propagate wheel event to parent scroll area so vertical scrolling works seamlessly
    event->ignore();
}

void PdfPageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_toolMode != PdfToolMode::SelectText || m_continuousMode || m_hasAreaSelection) {
            cancelActiveTool();
            clearAreaSelection();
            emit continuousModeExited();
            event->accept();
            return;
        }
    }

    if (event->matches(QKeySequence::Undo)) {
        if (m_transformBox && m_transformBox->isVisible()) {
            onTransformBoxCancelled();
            event->accept();
            return;
        }
        if (m_hasAreaSelection) {
            clearAreaSelection();
            event->accept();
            return;
        }
        m_permanentlyErasedBoxIndices.clear();
        emit undoRequested();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        emit redoRequested();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        if (m_hasAreaSelection) {
            copySelectedArea();
        } else if (hasSelection()) {
            copySelectionToClipboard();
        } else {
            copyPageTextToClipboard();
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        if (!m_isEditMode) {
            event->ignore();
            return;
        }
        if (m_hasAreaSelection) {
            cutSelectedArea();
        } else if (hasSelection()) {
            cutSelection();
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        if (!m_isEditMode) {
            event->ignore();
            return;
        }
        pasteFromClipboard(mapFromGlobal(QCursor::pos()));
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (m_hasAreaSelection) {
            clearAreaSelection();
            event->accept();
            return;
        }
        cancelActiveTool();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PdfPageWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        if (!m_isPanning) {
            setCursor(m_toolMode == PdfToolMode::SelectText ? Qt::IBeamCursor : Qt::ArrowCursor);
        }
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void PdfPageWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int clickedPage = pageIndexAtPos(event->pos());
    if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
        setActivePage(clickedPage);
    }
    QRect activePR = pageRect(m_activePageIndex);
    QPoint localPos = event->pos() - activePR.topLeft();

    if (!m_hasAreaSelection && m_selectedIndices.empty()) {
        int clickedIdx = findClosestBox(localPos);
        if (clickedIdx >= 0 && clickedIdx < static_cast<int>(m_textBoxes.size())) {
            const auto &box = m_textBoxes[clickedIdx];
            QRectF pixelRect(box->boundingBox().x() * m_scaleX,
                             box->boundingBox().y() * m_scaleY,
                             box->boundingBox().width() * m_scaleX,
                             box->boundingBox().height() * m_scaleY);
            if (pixelRect.adjusted(-6, -4, 6, 4).contains(localPos)) {
                m_selectedIndices = {clickedIdx};
                update();
                emit selectionChanged(true);
            }
        }
    }

    if (hasSelection()) {
        captureSelectedTextFormat();
    }

    QMenu menu(this);
    ThemePreset preset = ThemeManager::currentPreset();
    if (ThemeManager::isDark(preset)) {
        menu.setStyleSheet(
            "QMenu {"
            "   background-color: #21252b;"
            "   color: #e2e8f0;"
            "   border: 1px solid #3e4451;"
            "   border-radius: 6px;"
            "   padding: 4px 0px;"
            "}"
            "QMenu::item {"
            "   padding: 6px 24px 6px 12px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #38bdf8;"
            "   color: #ffffff;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: #3e4451;"
            "   margin: 4px 8px;"
            "}"
        );
    } else {
        menu.setStyleSheet(
            "QMenu {"
            "   background-color: #ffffff;"
            "   color: #1d1d1f;"
            "   border: 1px solid #d2d2d7;"
            "   border-radius: 6px;"
            "   padding: 4px 0px;"
            "}"
            "QMenu::item {"
            "   padding: 6px 24px 6px 12px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #007aff;"
            "   color: #ffffff;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: #e5e5ea;"
            "   margin: 4px 8px;"
            "}"
        );
    }

    if (m_hasAreaSelection) {
        menu.addAction(QIcon::fromTheme("edit-copy"), tr("📋 Copy Text from Area"), this, &PdfPageWidget::copySelectedAreaText);
        menu.addAction(QIcon::fromTheme("image-x-generic"), tr("🖼️ Copy Area as Image"), this, &PdfPageWidget::copySelectedAreaImage);
        menu.addAction(QIcon::fromTheme("system-search"), tr("🔍 Scan Area Text (OCR)..."), this, &PdfPageWidget::ocrSelectedArea);
        menu.addSeparator();
        menu.addAction(tr("Clear Area Selection"), this, &PdfPageWidget::clearAreaSelection);
        menu.addSeparator();
    } else if (hasSelection()) {
        if (m_isEditMode) {
            menu.addAction(tr("✏️ Edit / Replace Text..."), this, &PdfPageWidget::editSelectedTextWithWhiteout);
            menu.addAction(tr("✥ Move Selected Text..."), this, &PdfPageWidget::moveSelectedText);
            menu.addSeparator();

            QAction *actCut = menu.addAction(QIcon::fromTheme("edit-cut"), tr("Cut Text"), this, &PdfPageWidget::cutSelection);
            actCut->setShortcut(QKeySequence::Cut);

            QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Text"), this, &PdfPageWidget::copySelectionToClipboard);
            actCopy->setShortcut(QKeySequence::Copy);
        } else {
            QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Text"), this, &PdfPageWidget::copySelectionToClipboard);
            actCopy->setShortcut(QKeySequence::Copy);
        }
        menu.addSeparator();
    }

    if (m_isEditMode) {
        const QClipboard *clipboard = QApplication::clipboard();
        bool hasImg = !clipboard->image().isNull() && clipboard->image().width() > 0;
        bool hasTxt = !clipboard->text().trimmed().isEmpty();
        if (hasImg || hasTxt) {
            QPoint clickPos = event->pos();
            QAction *actPaste = menu.addAction(QIcon::fromTheme("edit-paste"), tr("Paste"), this, [this, clickPos]() {
                pasteFromClipboard(clickPos);
            });
            actPaste->setShortcut(QKeySequence::Paste);
            menu.addSeparator();
        }
    }

    if (!m_hasAreaSelection) {
        menu.addAction(QIcon::fromTheme("edit-select"), tr("Select Area..."), this, &PdfPageWidget::startSelectArea);
    }

    menu.addAction(tr("🔍 Scansiona testo..."), this, &PdfPageWidget::scanPageRequested);

    menu.addSeparator();

    QAction *actCopyPage = menu.addAction(QIcon::fromTheme("edit-copy"), tr("📋 Copy Page Text"), this, &PdfPageWidget::copyPageTextToClipboard);
    actCopyPage->setEnabled(!m_textBoxes.empty());

    menu.exec(event->globalPos());
}

