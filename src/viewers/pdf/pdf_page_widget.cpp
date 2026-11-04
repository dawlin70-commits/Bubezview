/**
 * @file pdf_page_widget.cpp
 * @brief Implementation of interactive PDF canvas, text selection, annotations, and AcroForms.
 */

#include "pdf_page_widget.h"
#include "pdf_canvas_tools.h"
#include "theme_manager.h"
#include "pdf_typewriter_dialog.h"
#include "pdf_watermark_dialog.h"
#include "pdf_signature_dialog.h"
#include <QFileDialog>
#include <poppler/qt6/poppler-qt6.h>
#include <poppler/qt6/poppler-form.h>

#include <QPainter>
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
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QXmlStreamReader>
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
#include <QFontInfo>
#include <QTimer>
#include <algorithm>
#include <cmath>

PdfPageWidget::PdfPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_scaleX(1.0)
    , m_scaleY(1.0)
    , m_toolMode(PdfToolMode::SelectText)
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
    setCursor(Qt::IBeamCursor);
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
    connect(m_transformBox, &PdfTransformBox::cutAreaRequested, this, &PdfPageWidget::cutSelectedArea);
    connect(m_transformBox, &PdfTransformBox::copyAreaRequested, this, &PdfPageWidget::copySelectedArea);
    connect(m_transformBox, &PdfTransformBox::undoRequested, this, &PdfPageWidget::undoRequested);
    connect(m_transformBox, &PdfTransformBox::redoRequested, this, &PdfPageWidget::redoRequested);
    connect(m_transformBox, &PdfTransformBox::geometryChanged, this, &PdfPageWidget::updateTransformBoxCollision);
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
    m_placedShapes.clear();
    clearPendingErasedState();
    m_permanentlyErasedBoxIndices.clear();
    m_doc = nullptr;
    setFixedSize(0, 0);
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::setDocument(Poppler::Document *doc, double zoomFactor, bool fitToWidth, int rotationDegrees, int availWidth)
{
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

    // Scale pixel bounds of placed shapes/stamps to match new layout DPI
    for (auto &shp : m_placedShapes) {
        if (shp.pageIndex >= 0 && shp.pageIndex < m_pageLayouts.size()) {
            const auto &pl = m_pageLayouts[shp.pageIndex];
            int pw = pl.rect.width();
            int ph = pl.rect.height();
            shp.rect = QRect(qRound(shp.normRect.x() * pw),
                             qRound(shp.normRect.y() * ph),
                             qRound(shp.normRect.width() * pw),
                             qRound(shp.normRect.height() * ph));
        }
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
    for (int i = 0; i < m_pageLayouts.size(); ++i) {
        if (m_pageLayouts[i].rect.contains(pos)) {
            return i;
        }
    }
    return -1;
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
        if (m_toolMode != PdfToolMode::SelectText && m_toolMode != PdfToolMode::Zoom) {
            setToolMode(PdfToolMode::SelectText);
        }
    }
    update();
}

void PdfPageWidget::setToolMode(PdfToolMode mode)
{
    if (!m_isEditMode && mode != PdfToolMode::SelectText && mode != PdfToolMode::Zoom && mode != PdfToolMode::Snapshot) {
        return;
    }
    if (m_toolMode == mode) return;

    if (m_committingShapeIndex >= 0) {
        commitSelectedStampToPdf();
    }

    if (m_inlineEditor) {
        m_inlineEditor->deleteLater();
        m_inlineEditor = nullptr;
    }

    m_toolMode = mode;

    switch (m_toolMode) {
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
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        const auto &pl = m_pageLayouts[m_activePageIndex];
        if (vp && vp->isVisible()) {
            QPoint centerInCanvas = mapFrom(vp, vp->rect().center());
            QPoint centerInPage = centerInCanvas - pl.rect.topLeft();
            int cx = qBound(30, centerInPage.x(), qMax(30, pl.rect.width() - 30));
            int cy = qBound(30, centerInPage.y(), qMax(30, pl.rect.height() - 30));
            return QPoint(cx, cy);
        }
        return QPoint(pl.rect.width() / 2, pl.rect.height() / 2);
    }
    if (vp && vp->isVisible()) {
        QPoint centerInPage = mapFrom(vp, vp->rect().center());
        int marginX = qBound(20, width() / 6, 120);
        int marginY = qBound(20, height() / 6, 120);
        int cx = qBound(marginX, centerInPage.x(), qMax(marginX, width() - marginX));
        int cy = qBound(marginY, centerInPage.y(), qMax(marginY, height() - marginY));
        return QPoint(cx, cy);
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

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = visibleCenter();
    }

    // Determine clean proportional display dimensions
    QSize defSize(280, 200);
    if (!image.isNull() && image.width() > 0 && image.height() > 0) {
        int w = std::min(350, image.width());
        int h = static_cast<int>(w * static_cast<double>(image.height()) / image.width());
        defSize = QSize(w, std::max(40, h));
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
    m_pendingErasedBoxIndices.clear();
    m_collidingTextPtRects.clear();
    if (m_transformBox) {
        m_transformBox->setHasCollision(false);
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

        QColor sampledBg = sampleBackgroundColorForRect(m_activePageIndex, unionPtRect);
        QRectF normRect(std::max(0.0, (unionPtRect.x() - 1.0) / pageWPt),
                        std::max(0.0, (unionPtRect.y() - 1.0) / pageHPt),
                        std::min(1.0, (unionPtRect.width() + 2.0) / pageWPt),
                        std::min(1.0, (unionPtRect.height() + 2.0) / pageHPt));

        int pixW = std::max(4, qRound((unionPtRect.width() + 2.0) * m_scaleX));
        int pixH = std::max(4, qRound((unionPtRect.height() + 2.0) * m_scaleY));
        QImage patchImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
        patchImg.fill(sampledBg);

        emit addStampAnnotationRequested(patchImg, normRect);
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

void PdfPageWidget::startInPlaceTextEdit()
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
    if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> p(m_doc->page(m_activePageIndex));
        if (p) {
            pageWPt = p->pageSize().width();
            pageHPt = p->pageSize().height();
        }
    }
    if (pageWPt > 0 && pageHPt > 0) {
        m_pendingErasedPtRect = paddedUnionPt;
        m_pendingErasedNormRect = QRectF(std::max(0.0, paddedUnionPt.x() / pageWPt),
                                         std::max(0.0, paddedUnionPt.y() / pageHPt),
                                         std::min(1.0, paddedUnionPt.width() / pageWPt),
                                         std::min(1.0, paddedUnionPt.height() / pageHPt));
        m_selectedTextNormRect = m_pendingErasedNormRect;
    }

    QFont font = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.font : m_typewriterFont;
    QColor color = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.color : m_typewriterColor;

    // Sample background color around the selected text perimeter
    QColor sampledBg = sampleBackgroundColorForRect(m_activePageIndex, m_selectedTextPtRect);
    m_pendingErasedBgColor = sampledBg;

    // Immediately clear text selection and repaint so the selected block is visually wiped on canvas
    m_selectedIndices.clear();
    emit selectionChanged(false);
    update();

    // Position the in-place text editor directly over the wiped area with extra width buffer
    QRect origPixRect(qRound(unionPtRect.left() * m_scaleX),
                      qRound(unionPtRect.top() * m_scaleY),
                      qRound(unionPtRect.width() * m_scaleX),
                      qRound(unionPtRect.height() * m_scaleY));
    QPoint targetTopLeft(origPixRect.left() - 4, origPixRect.top() - 2);
    int suggestedW = origPixRect.width() + 32;

    setToolMode(PdfToolMode::Typewriter);

    PdfTextBgMode bgMode = PdfTextBgMode::Auto;
    if (m_transformBox) {
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showText(text, font, color, bgMode, sampledBg,
                                 QRect(targetTopLeft.x(), targetTopLeft.y(), suggestedW, origPixRect.height() + 4),
                                 targetTopLeft);
        m_transformBox->setTextModified(true);
        if (m_transformBox->inlineTextEdit()) {
            m_transformBox->inlineTextEdit()->setFocus();
            m_transformBox->inlineTextEdit()->selectAll();
        }
    }

    updateTransformBoxCollision();
    emit inPlaceTextEditStarted(text, font, color, bgMode, sampledBg);
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

    QRectF paddedUnionPt = unionPtRect.adjusted(-1.0, -1.0, 1.0, 1.0);
    for (int i = 0; i < static_cast<int>(m_textBoxes.size()); ++i) {
        if (m_textBoxes[i] && paddedUnionPt.contains(m_textBoxes[i]->boundingBox().center())) {
            m_pendingErasedBoxIndices.insert(i);
        }
    }

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
        m_pendingErasedBgColor = sampleBackgroundColorForRect(m_activePageIndex, unionPtRect);
    }

    // Clear selection
    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);

    // Launch movable typewriter box in placement mode with collision detection
    QRect origPixRect(qRound(unionPtRect.left() * m_scaleX),
                      qRound(unionPtRect.top() * m_scaleY),
                      qRound(unionPtRect.width() * m_scaleX),
                      qRound(unionPtRect.height() * m_scaleY));
    QPoint targetTopLeft(origPixRect.left() - 4, origPixRect.top() - 2);
    int suggestedW = origPixRect.width() + 32;

    QFont font = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.font : m_typewriterFont;
    QColor color = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.color : m_typewriterColor;

    startTypewriter(text, font, color, false, targetTopLeft);
    if (m_transformBox) {
        m_transformBox->setPlacementMode(true, QPoint(), suggestedW, targetTopLeft);
    }
    updateTransformBoxCollision();
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
    updateShapeProperties(m_shapeType, strokeWidth, strokeColor, fillColor);
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
    if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_placedShapes.size()) {
        updateSelectedShapeProperties(shapeType, strokeWidth, strokeColor, fillColor);
    }
}

void PdfPageWidget::updateSelectedShapeProperties(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    if (m_selectedShapeIndex < 0 || m_selectedShapeIndex >= m_placedShapes.size()) return;

    auto &shp = m_placedShapes[m_selectedShapeIndex];
    if (shapeType >= 0) {
        shp.shapeType = shapeType;
    }
    shp.strokeWidth = strokeWidth;
    shp.strokeColor = strokeColor;
    shp.fillColor = fillColor;

    shp.stampImage = PdfCanvasTools::renderShapeStamp(shp.shapeType, shp.strokeWidth,
                                                      shp.strokeColor, shp.fillColor,
                                                      shp.rect.size(), shp.startPt, shp.endPt);
    if (!shp.stampImage.isNull()) {
        emit moveStampAnnotationRequested(shp.normRect, shp.normRect, shp.stampImage);
    }
    update();
}

void PdfPageWidget::updateSelectedStampProperties(const QString &text, const QColor &color, qreal angle, int style, int fontSize)
{
    if (m_selectedShapeIndex < 0 || m_selectedShapeIndex >= m_placedShapes.size()) return;
    auto &shp = m_placedShapes[m_selectedShapeIndex];
    if (shp.shapeType != 100) return;

    if (m_committingShapeIndex >= 0 && m_committingShapeIndex != m_selectedShapeIndex) {
        commitSelectedStampToPdf();
    }

    if (m_committingShapeIndex != m_selectedShapeIndex) {
        m_committingShapeIndex = m_selectedShapeIndex;
        m_commitOldNormRect = shp.normRect;
    }

    shp.stampText = text;
    shp.stampColor = color.isValid() ? color : QColor(QStringLiteral("#dc2626"));
    shp.stampAngle = angle;
    shp.stampStyle = style;
    shp.stampSize = (fontSize > 0) ? fontSize : 28;

    QRect activePR = pageRect(shp.pageIndex);
    int pageW = activePR.width();
    int pageH = activePR.height();
    if (pageW <= 0 || pageH <= 0) return;

    QPoint centerPos = shp.rect.center();
    QRect newRect = PdfCanvasTools::calculateStampRect(shp.stampText, shp.stampAngle,
                                                       shp.stampSize, centerPos, pageW, pageH);

    QImage stampImg(newRect.size(), QImage::Format_ARGB32_Premultiplied);
    stampImg.fill(Qt::transparent);
    QPainter p(&stampImg);
    p.setRenderHint(QPainter::Antialiasing, true);
    PdfTransformBox::drawWatermarkStamp(p, QRect(QPoint(0, 0), newRect.size()),
                                        shp.stampText, shp.stampAngle,
                                        shp.stampOpacity, shp.stampColor,
                                        shp.stampStyle);
    p.end();

    QRectF newNorm(static_cast<double>(newRect.x()) / pageW,
                   static_cast<double>(newRect.y()) / pageH,
                   static_cast<double>(newRect.width()) / pageW,
                   static_cast<double>(newRect.height()) / pageH);

    shp.rect = newRect;
    shp.normRect = newNorm;
    shp.stampImage = stampImg;

    update();

    if (!m_stampCommitTimer) {
        m_stampCommitTimer = new QTimer(this);
        m_stampCommitTimer->setSingleShot(true);
        connect(m_stampCommitTimer, &QTimer::timeout, this, &PdfPageWidget::commitSelectedStampToPdf);
    }
    m_stampCommitTimer->start(350);
}

void PdfPageWidget::commitSelectedStampToPdf()
{
    if (m_stampCommitTimer && m_stampCommitTimer->isActive()) {
        m_stampCommitTimer->stop();
    }
    if (m_committingShapeIndex >= 0 && m_committingShapeIndex < m_placedShapes.size()) {
        int idx = m_committingShapeIndex;
        QRectF oldNorm = m_commitOldNormRect;
        QRectF newNorm = m_placedShapes[idx].normRect;
        QImage stampImg = m_placedShapes[idx].stampImage;
        m_committingShapeIndex = -1;
        m_commitOldNormRect = QRectF();

        if (!stampImg.isNull()) {
            emit moveStampAnnotationRequested(oldNorm, newNorm, stampImg);
        }
    } else {
        m_committingShapeIndex = -1;
        m_commitOldNormRect = QRectF();
    }
}

void PdfPageWidget::cancelPendingStampCommit()
{
    if (m_stampCommitTimer && m_stampCommitTimer->isActive()) {
        m_stampCommitTimer->stop();
    }
    m_committingShapeIndex = -1;
    m_commitOldNormRect = QRectF();
}

void PdfPageWidget::deleteSelectedShape()
{
    if (m_selectedShapeIndex < 0 || m_selectedShapeIndex >= m_placedShapes.size()) return;

    QRectF normRect = m_placedShapes[m_selectedShapeIndex].normRect;
    if (m_committingShapeIndex == m_selectedShapeIndex) {
        if (m_stampCommitTimer && m_stampCommitTimer->isActive()) {
            m_stampCommitTimer->stop();
        }
        if (!m_commitOldNormRect.isNull()) {
            normRect = m_commitOldNormRect;
        }
        m_committingShapeIndex = -1;
        m_commitOldNormRect = QRectF();
    }
    m_placedShapes.removeAt(m_selectedShapeIndex);
    m_selectedShapeIndex = -1;
    m_hoveredShapeIndex = -1;
    emit deleteStampAnnotationRequested(normRect);
    update();
}

void PdfPageWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_isEditMode && event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp" || ext == "svg") {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void PdfPageWidget::dropEvent(QDropEvent *event)
{
    if (m_isEditMode && event->mimeData() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto &url : urls) {
            QString path = url.toLocalFile();
            QImage img(path);
            if (!img.isNull()) {
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
    m_lastClipType = LastClipType::AreaImage;
    m_lastCopiedTextFormat.valid = false;

    QRect r;
    if (m_transformBox && m_transformBox->isVisible() && m_transformBox->itemType() == PdfTransformItemType::AreaSelection) {
        r = m_transformBox->boxRect();
    } else if (m_hasAreaSelection && m_selectedAreaRect.width() >= 4 && m_selectedAreaRect.height() >= 4) {
        r = m_selectedAreaRect;
    }

    if (r.width() < 4 || r.height() < 4) return;

    QRectF pointRect(r.x() / m_scaleX,
                    r.y() / m_scaleY,
                    r.width() / m_scaleX,
                    r.height() / m_scaleY);

    emit copyAreaRequested(pointRect);
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

    // 1. If an area or image was copied/cut:
    if (hasImage && (m_lastClipType == LastClipType::AreaImage || !hasText)) {
        startInsertImage(img, localPos);
        return;
    }

    // 2. If text was copied/cut:
    if (hasText && (m_lastClipType == LastClipType::Text || !hasImage)) {
        QFont pasteFont = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.font : QFont(QStringLiteral("Sans Serif"), 12);
        QColor pasteColor = m_lastCopiedTextFormat.valid ? m_lastCopiedTextFormat.color : Qt::black;
        startTypewriter(text, pasteFont, pasteColor, false, localPos);
        if (m_transformBox) {
            m_transformBox->setPlacementMode(true, localPos);
        }
        updateTransformBoxCollision();
        return;
    }

    // 3. Fallbacks:
    if (hasImage) {
        startInsertImage(img, localPos);
        return;
    }

    if (hasText) {
        QFont defaultFont(QStringLiteral("Sans Serif"), 12);
        startTypewriter(text, defaultFont, Qt::black, false, localPos);
        if (m_transformBox) {
            m_transformBox->setPlacementMode(true, localPos);
        }
        updateTransformBoxCollision();
        return;
    }
}

void PdfPageWidget::startWatermark(const QString &text, qreal angle, qreal opacity,
                                   const QColor &color, const QPoint &initialPos, int style, int fontSize)
{
    m_watermarkText = text;
    m_watermarkAngle = angle;
    m_watermarkOpacity = opacity;
    m_watermarkColor = color.isValid() ? color : QColor(QStringLiteral("#dc2626"));
    m_watermarkStyle = style;
    m_watermarkSize = (fontSize > 0) ? fontSize : 28;
    setToolMode(PdfToolMode::Watermark);

    if (!initialPos.isNull() && !m_watermarkText.isEmpty()) {
        QRect activePR = pageRect(m_activePageIndex);
        int pageW = activePR.width();
        int pageH = activePR.height();
        if (pageW > 0 && pageH > 0) {
            QRect stampRect = PdfCanvasTools::calculateStampRect(m_watermarkText, m_watermarkAngle,
                                                                 m_watermarkSize, initialPos, pageW, pageH);

            QImage stampImg(stampRect.size(), QImage::Format_ARGB32_Premultiplied);
            stampImg.fill(Qt::transparent);
            QPainter p(&stampImg);
            p.setRenderHint(QPainter::Antialiasing, true);
            PdfTransformBox::drawWatermarkStamp(p, QRect(QPoint(0, 0), stampRect.size()),
                                                m_watermarkText, m_watermarkAngle,
                                                m_watermarkOpacity, m_watermarkColor,
                                                m_watermarkStyle);
            p.end();

            QRectF normRect(static_cast<double>(stampRect.x()) / pageW,
                           static_cast<double>(stampRect.y()) / pageH,
                           static_cast<double>(stampRect.width()) / pageW,
                           static_cast<double>(stampRect.height()) / pageH);

            PlacedShapeItem item;
            item.id = ++m_shapeIdCounter;
            item.pageIndex = m_activePageIndex;
            item.shapeType = 100;
            item.rect = stampRect;
            item.normRect = normRect;
            item.stampImage = stampImg;
            item.stampText = m_watermarkText;
            item.stampAngle = m_watermarkAngle;
            item.stampOpacity = m_watermarkOpacity;
            item.stampColor = m_watermarkColor;
            item.stampStyle = m_watermarkStyle;
            item.stampSize = m_watermarkSize;
            m_placedShapes.append(item);
            m_selectedShapeIndex = m_placedShapes.size() - 1;

            emit addStampAnnotationRequested(stampImg, normRect);
            update();
        }
    }
}

void PdfPageWidget::startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeType = shapeType;
    m_shapeStrokeWidth = strokeWidth;
    m_shapeStrokeColor = strokeColor;
    m_shapeFillColor = fillColor;

    setToolMode(PdfToolMode::Shape);
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

    PlacedShapeItem item;
    item.id = ++m_shapeIdCounter;
    item.pageIndex = m_activePageIndex;
    item.shapeType = 102; // Highlight
    item.rect = rect;
    item.normRect = normRect;
    item.stampImage = highlightImg;
    m_placedShapes.append(item);
    m_activeHighlightIndex = m_placedShapes.size() - 1;

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

    QList<QPair<QImage, QRectF>> batchStamps;
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            const auto &box = m_textBoxes[idx];
            QRectF ptRect = box->boundingBox();

            QRectF normRect(std::max(0.0, ptRect.x() / pageWPt),
                            std::max(0.0, ptRect.y() / pageHPt),
                            std::min(1.0, ptRect.width() / pageWPt),
                            std::min(1.0, ptRect.height() / pageHPt));

            QRect hlRect(qRound(ptRect.x() * m_scaleX),
                         qRound(ptRect.y() * m_scaleY),
                         std::max(4, qRound(ptRect.width() * m_scaleX)),
                         std::max(4, qRound(ptRect.height() * m_scaleY)));

            int pixW = std::max(4, qRound(ptRect.width() * m_scaleX * 2.0));
            int pixH = std::max(4, qRound(ptRect.height() * m_scaleY * 2.0));
            QImage hlImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
            QColor c = hlColor;
            c.setAlphaF(hlOpacity);
            hlImg.fill(c);

            PlacedShapeItem item;
            item.id = ++m_shapeIdCounter;
            item.pageIndex = m_activePageIndex;
            item.shapeType = 102; // Highlight
            item.rect = hlRect;
            item.normRect = normRect;
            item.stampImage = hlImg;
            m_placedShapes.append(item);

            batchStamps.append(qMakePair(hlImg, normRect));
        }
    }

    if (!m_placedShapes.isEmpty()) {
        m_activeHighlightIndex = m_placedShapes.size() - 1;
    }

    if (!batchStamps.isEmpty()) {
        emit addStampAnnotationsRequested(batchStamps);
    }

    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::updateHighlighterProperties(const QColor &color, double opacity)
{
    m_highlighterColor = color.isValid() ? color : m_highlighterColor;
    m_highlighterOpacity = qBound(0.05, opacity, 1.0);

    // Apply color and opacity globally across all placed highlights on the document
    for (auto &shp : m_placedShapes) {
        if (shp.shapeType == 102) {
            int pixW = std::max(4, shp.rect.width() * 2);
            int pixH = std::max(4, shp.rect.height() * 2);
            QImage newHlImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
            QColor c = m_highlighterColor;
            c.setAlphaF(m_highlighterOpacity);
            newHlImg.fill(c);
            shp.stampImage = newHlImg;
        }
    }
    update();
}

void PdfPageWidget::clearActiveHighlightSelection()
{
    if (m_activeHighlightIndex >= 0) {
        m_activeHighlightIndex = -1;
        update();
    }
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

    // 1. If whiteout requested, place an opaque white block over the original text bounding box
    if (whiteout && m_selectedTextPtRect.isValid()) {
        int pixW = std::max(4, qRound((m_selectedTextPtRect.width() + 4.0) * m_scaleX));
        int pixH = std::max(4, qRound((m_selectedTextPtRect.height() + 4.0) * m_scaleY));
        QImage whiteImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
        whiteImg.fill(Qt::white);

        PlacedShapeItem whiteItem;
        whiteItem.id = ++m_shapeIdCounter;
        whiteItem.pageIndex = pageIdx;
        whiteItem.shapeType = 101; // Redaction / Whiteout
        whiteItem.rect = QRect(qRound((m_selectedTextPtRect.x() - 2.0) * m_scaleX),
                               qRound((m_selectedTextPtRect.y() - 2.0) * m_scaleY),
                               pixW, pixH);
        whiteItem.normRect = m_selectedTextNormRect;
        whiteItem.stampImage = whiteImg;
        m_placedShapes.append(whiteItem);
    }

    // 2. Render new text cleanly at target location
    QFontMetrics fm(font);
    QRect textBound = fm.boundingRect(QRect(0, 0, 4000, 4000), Qt::AlignLeft | Qt::TextWordWrap, newText);
    int txtPixW = std::max(16, textBound.width() + 16);
    int txtPixH = std::max(16, textBound.height() + 8);
    QImage txtImg(txtPixW, txtPixH, QImage::Format_ARGB32_Premultiplied);
    txtImg.fill(Qt::transparent);
    {
        QPainter p(&txtImg);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setFont(font);
        p.setPen(color);
        p.drawText(QRect(4, 2, textBound.width(), textBound.height()), Qt::AlignLeft | Qt::TextWordWrap, newText);
    }

    if (!txtImg.isNull()) {
        PlacedShapeItem txtItem;
        txtItem.id = ++m_shapeIdCounter;
        txtItem.pageIndex = pageIdx;
        txtItem.shapeType = 100; // Text stamp
        txtItem.rect = QRect(qRound(m_selectedTextPtRect.x() * m_scaleX),
                             qRound(m_selectedTextPtRect.y() * m_scaleY),
                             textBound.width(), textBound.height());
        txtItem.normRect = m_selectedTextNormRect;
        txtItem.stampImage = txtImg;
        m_placedShapes.append(txtItem);
    }

    // 3. Emit persistent text annotation to Poppler
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
    if (m_committingShapeIndex >= 0) {
        commitSelectedStampToPdf();
    }
    if (m_transformBox) {
        m_transformBox->dismiss();
    }
    clearPendingErasedState();
    m_isDraggingTool = false;
    m_activeHighlightIndex = -1;
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
            if (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty()) {
                int patchW = std::max(4, qRound(m_pendingErasedNormRect.width() * pageW));
                int patchH = std::max(4, qRound(m_pendingErasedNormRect.height() * pageH));
                QImage patchImg(patchW, patchH, QImage::Format_ARGB32_Premultiplied);
                patchImg.fill(m_pendingErasedBgColor);
                m_permanentlyErasedBoxIndices.unite(m_pendingErasedBoxIndices);
                emit addStampAnnotationRequested(patchImg, m_pendingErasedNormRect);
            }
            clearPendingErasedState();
            m_selectedTextNormRect = QRectF();
            m_transformBox->dismiss();
            setToolMode(PdfToolMode::SelectText);
            emit inPlaceTextEditFinished();
            update();
            return;
        }

        // Real-time collision guardrail: block placement if borders overlap existing neighboring text
        updateTransformBoxCollision();
        if (m_transformBox->hasCollision()) {
            QMessageBox::warning(
                window(),
                tr("Text Overlap Detected"),
                tr("Cannot place the text box here because its borders overlap existing text on the page.\n\n"
                   "Please drag the box to a clear position (or click 'Edit' to shorten the text / reduce the font size) "
                   "so it does not overwrite neighboring text."));
            return;
        }

        if (!m_pendingMoveWhiteoutRect.isEmpty()) {
            QImage whiteImg(16, 16, QImage::Format_RGB32);
            whiteImg.fill(Qt::white);
            emit addStampAnnotationRequested(whiteImg, m_pendingMoveWhiteoutRect);
            m_pendingMoveWhiteoutRect = QRectF();
        }

        m_permanentlyErasedBoxIndices.unite(m_pendingErasedBoxIndices);

        // Determine background wipe rectangle:
        // If replacing an erased block, ensure the original erased block is wiped cleanly.
        QRectF bgNormRect = normRect;
        if (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty()) {
            if (m_transformBox->textBgMode() == PdfTextBgMode::Transparent ||
                m_pendingErasedNormRect.contains(normRect)) {
                bgNormRect = m_pendingErasedNormRect;
            } else if (m_pendingErasedNormRect.intersects(normRect)) {
                bgNormRect = m_pendingErasedNormRect.united(normRect);
            } else {
                // The user dragged the replacement box completely outside the original wiped area:
                // Stamp the original wiped area first, then use normRect for the new box's background.
                int origW = std::max(4, qRound(m_pendingErasedNormRect.width() * pageW));
                int origH = std::max(4, qRound(m_pendingErasedNormRect.height() * pageH));
                QImage origWipeImg(origW, origH, QImage::Format_ARGB32_Premultiplied);
                origWipeImg.fill(m_pendingErasedBgColor);
                emit addStampAnnotationRequested(origWipeImg, m_pendingErasedNormRect);
                bgNormRect = normRect;
            }
        } else if (m_selectedTextNormRect.isValid() && !m_selectedTextNormRect.isEmpty()) {
            bgNormRect = bgNormRect.united(m_selectedTextNormRect);
            bgNormRect.adjust(-0.002, -0.002, 0.002, 0.002);
            bgNormRect.setLeft(std::max(0.0, bgNormRect.left()));
            bgNormRect.setTop(std::max(0.0, bgNormRect.top()));
            bgNormRect.setRight(std::min(1.0, bgNormRect.right()));
            bgNormRect.setBottom(std::min(1.0, bgNormRect.bottom()));
        }

        // Generate background patch image if we have a wiped block or non-transparent background mode
        QImage bgImg;
        bool needBgPatch = (m_transformBox->textBgMode() != PdfTextBgMode::Transparent) ||
                           (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty());
        if (needBgPatch) {
            QColor patchColor = (m_pendingErasedNormRect.isValid() && !m_pendingErasedNormRect.isEmpty())
                                ? m_pendingErasedBgColor
                                : m_transformBox->textBgColor();
            int patchW = std::max(4, qRound(bgNormRect.width() * pageW));
            int patchH = std::max(4, qRound(bgNormRect.height() * pageH));
            if (m_transformBox->textBgMode() == PdfTextBgMode::Feathered) {
                bgImg = PdfCanvasTools::createFeatheredPatch(QSize(patchW, patchH), patchColor);
            } else {
                bgImg = QImage(patchW, patchH, QImage::Format_ARGB32_Premultiplied);
                bgImg.fill(patchColor);
            }
        }

        // Give left-aligned FreeText annotations a right-side width buffer
        // so Poppler's internal PDF font metrics never prematurely wrap words at line ends.
        QRectF textNormRect = normRect;
        if (m_transformBox->textAlignment() == Qt::AlignLeft && pageW > 0) {
            double extraNormW = 28.0 / pageW;
            textNormRect.setWidth(std::min(1.0 - textNormRect.left(), textNormRect.width() + extraNormW));
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
    m_pendingMoveWhiteoutRect = QRectF();
    m_selectedTextNormRect = QRectF();
    m_transformBox->dismiss();
    setToolMode(PdfToolMode::SelectText);
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

        QFont font = (!m_selectedIndices.empty() && m_lastCopiedTextFormat.valid) ? m_lastCopiedTextFormat.font : m_typewriterFont;
        QColor color = (!m_selectedIndices.empty() && m_lastCopiedTextFormat.valid) ? m_lastCopiedTextFormat.color : m_typewriterColor;

        m_selectedIndices.clear();
        emit selectionChanged(false);
        update();

        QPoint targetTopLeft(r.left() - 4, r.top() - 2);
        int suggestedW = r.width() + 32;
        setToolMode(PdfToolMode::Typewriter);
        PdfTextBgMode bgMode = PdfTextBgMode::Auto;
        m_transformBox->setScaleFactor(m_scaleX);
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showText(extractedText, font, color, bgMode, sampledBg,
                                 QRect(targetTopLeft.x(), targetTopLeft.y(), suggestedW, r.height() + 4),
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

void PdfPageWidget::commitShapeDrawing(const QRect &rect, const QPoint &startPt, const QPoint &endPt)
{
    int pageW = width();
    int pageH = height();
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageW = m_pageLayouts[m_activePageIndex].rect.width();
        pageH = m_pageLayouts[m_activePageIndex].rect.height();
    }
    if (pageW <= 0 || pageH <= 0) return;

    QRect targetRect = rect;
    QPoint localStart(-1, -1);
    QPoint localEnd(-1, -1);

    if (m_shapeType == 2 || m_shapeType == 3) {
        if (startPt == QPoint(-1, -1) || endPt == QPoint(-1, -1)) {
            if (rect.width() <= 2 || rect.height() <= 2) return;
        } else {
            // Line or Arrow: preserve exact direction, vector, and inclination
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

    QImage shapeImg = PdfCanvasTools::renderShapeStamp(m_shapeType, m_shapeStrokeWidth,
                                                       m_shapeStrokeColor, m_shapeFillColor,
                                                       targetRect.size(), localStart, localEnd);
    if (shapeImg.isNull()) return;

    double nx = std::clamp(static_cast<double>(targetRect.x()) / pageW, 0.0, 1.0);
    double ny = std::clamp(static_cast<double>(targetRect.y()) / pageH, 0.0, 1.0);
    double nw = std::clamp(static_cast<double>(targetRect.width()) / pageW, 0.0, 1.0 - nx);
    double nh = std::clamp(static_cast<double>(targetRect.height()) / pageH, 0.0, 1.0 - ny);
    QRectF normRect(nx, ny, nw, nh);

    emit addStampAnnotationRequested(shapeImg, normRect);

    PlacedShapeItem item;
    item.id = ++m_shapeIdCounter;
    item.pageIndex = m_activePageIndex;
    item.shapeType = m_shapeType;
    item.strokeWidth = m_shapeStrokeWidth;
    item.strokeColor = m_shapeStrokeColor;
    item.fillColor = m_shapeFillColor;
    item.rect = targetRect;
    item.startPt = localStart;
    item.endPt = localEnd;
    item.normRect = normRect;
    item.stampImage = shapeImg;
    m_placedShapes.append(item);
    m_selectedShapeIndex = m_placedShapes.size() - 1;
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

/**
 * @brief Strips PDF subset prefixes and standard weight suffixes from a font family name.
 */
static QString cleanPdfFontFamily(const QString &rawName, bool &outBold, bool &outItalic)
{
    QString name = rawName;
    int plusIdx = name.indexOf('+');
    if (plusIdx != -1) {
        name = name.mid(plusIdx + 1);
    }

    if (name.contains(QStringLiteral("Bold"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Heavy"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Black"), Qt::CaseInsensitive)) {
        outBold = true;
    }
    if (name.contains(QStringLiteral("Italic"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("Oblique"), Qt::CaseInsensitive)) {
        outItalic = true;
    }

    int hyphenIdx = name.indexOf('-');
    if (hyphenIdx != -1) {
        name = name.left(hyphenIdx);
    }

    static const QStringList suffixes = {
        QStringLiteral(" Regular"), QStringLiteral(" regular"),
        QStringLiteral(" Bold"), QStringLiteral(" bold"),
        QStringLiteral(" Italic"), QStringLiteral(" italic"),
        QStringLiteral(" Oblique"), QStringLiteral(" oblique")
    };
    for (const QString &sfx : suffixes) {
        if (name.endsWith(sfx, Qt::CaseInsensitive)) {
            name.chop(sfx.length());
        }
    }
    QString trimmed = name.trimmed();
    if (trimmed.compare(QStringLiteral("SegoeUI"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Segoe UI");
    } else if (trimmed.compare(QStringLiteral("NotoSans"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Noto Sans");
    } else if (trimmed.compare(QStringLiteral("LiberationSans"), Qt::CaseInsensitive) == 0) {
        trimmed = QStringLiteral("Liberation Sans");
    }
    return trimmed;
}

bool PdfPageWidget::parsePageXmlLayout(int pageIndex, double pageWidthPt, double pageHeightPt)
{
    if (m_filePath.isEmpty() || !QFile::exists(m_filePath)) return false;
    if (m_cachedPageLayout.pageIndex == pageIndex && m_cachedPageLayout.filePath == m_filePath && !m_cachedPageLayout.elements.isEmpty()) {
        return true;
    }

    m_cachedPageLayout = CachedPageLayout();
    m_cachedPageLayout.pageIndex = pageIndex;
    m_cachedPageLayout.filePath = m_filePath;

    int pageNum = pageIndex + 1; // pdftohtml pages are 1-based
    QProcess proc;
    proc.setWorkingDirectory(QDir::tempPath());
    QStringList args;
    args << QStringLiteral("-xml")
         << QStringLiteral("-stdout")
         << QStringLiteral("-i")         // Crucial: ignore images so embedded raster images are never dumped to disk!
         << QStringLiteral("-noframes")
         << QStringLiteral("-nodrm")
         << QStringLiteral("-q")
         << QStringLiteral("-f") << QString::number(pageNum)
         << QStringLiteral("-l") << QString::number(pageNum)
         << m_filePath;

    proc.start(QStringLiteral("pdftohtml"), args);
    if (!proc.waitForFinished(1500)) {
        proc.kill();
        return false;
    }

    QByteArray xmlData = proc.readAllStandardOutput();
    if (xmlData.isEmpty()) return false;

    QXmlStreamReader xml(xmlData);
    double xmlPageW = 0.0;
    double xmlPageH = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            auto tagName = xml.name();
            if (tagName == QLatin1String("page")) {
                xmlPageW = xml.attributes().value(QLatin1String("width")).toDouble();
                xmlPageH = xml.attributes().value(QLatin1String("height")).toDouble();
                if (pageWidthPt > 0.0 && xmlPageW > 0.0) scaleX = xmlPageW / pageWidthPt;
                if (pageHeightPt > 0.0 && xmlPageH > 0.0) scaleY = xmlPageH / pageHeightPt;
            } else if (tagName == QLatin1String("fontspec")) {
                int fontId = xml.attributes().value(QLatin1String("id")).toInt();
                double size = xml.attributes().value(QLatin1String("size")).toDouble();
                QString rawFamily = xml.attributes().value(QLatin1String("family")).toString();
                QString colorStr = xml.attributes().value(QLatin1String("color")).toString();

                PageFontSpec fs;
                bool isBold = false;
                bool isItalic = false;
                fs.family = cleanPdfFontFamily(rawFamily, isBold, isItalic);
                fs.isBold = isBold;
                fs.isItalic = isItalic;
                fs.size = (scaleY > 0.0) ? (size / scaleY) : size;
                if (!colorStr.isEmpty()) {
                    fs.color = QColor(colorStr);
                }
                m_cachedPageLayout.fonts.insert(fontId, fs);
            } else if (tagName == QLatin1String("text")) {
                double top = xml.attributes().value(QLatin1String("top")).toDouble();
                double left = xml.attributes().value(QLatin1String("left")).toDouble();
                double width = xml.attributes().value(QLatin1String("width")).toDouble();
                double height = xml.attributes().value(QLatin1String("height")).toDouble();
                int fontId = xml.attributes().value(QLatin1String("font")).toInt();

                PageTextElement el;
                el.fontId = fontId;
                el.rectPt = QRectF(left / scaleX, top / scaleY, width / scaleX, height / scaleY);

                if (m_cachedPageLayout.fonts.contains(fontId)) {
                    const PageFontSpec &fs = m_cachedPageLayout.fonts.value(fontId);
                    el.family = fs.family;
                    el.size = fs.size;
                    el.color = fs.color;
                    el.isBold = fs.isBold;
                    el.isItalic = fs.isItalic;
                }

                bool innerBold = false;
                bool innerItalic = false;
                QString elText;

                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == QLatin1String("b")) {
                            innerBold = true;
                        } else if (xml.name() == QLatin1String("i")) {
                            innerItalic = true;
                        }
                    } else if (xml.tokenType() == QXmlStreamReader::Characters) {
                        elText += xml.text();
                    } else if (xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QLatin1String("text")) {
                        break;
                    }
                }

                el.text = elText;
                if (innerBold) el.isBold = true;
                if (innerItalic) el.isItalic = true;

                // Ignore empty or whitespace-only elements so they do not pollute font/color extraction
                if (!el.text.trimmed().isEmpty()) {
                    m_cachedPageLayout.elements.append(el);
                }
            }
        }
    }

    return !m_cachedPageLayout.elements.isEmpty();
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

    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
            double h = m_textBoxes[idx]->boundingBox().height();
            if (h > 0.0) {
                if (h < minH) minH = h;
                if (h > maxH) maxH = h;
                validBoxes++;

                double pt = h;
                int halfPt = qRound(pt * 2.0);
                sizeFrequency[halfPt]++;
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

    // 2. Discover precise font family, style, and color using XML layout analysis (pdftohtml)
    bool layoutMatched = false;
    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> page = m_doc->page(m_activePageIndex);
        if (page) {
            pageWPt = page->pageSizeF().width();
            pageHPt = page->pageSizeF().height();
        }
    }

    if (!m_filePath.isEmpty() && QFile::exists(m_filePath) && pageWPt > 0.0 && pageHPt > 0.0) {
        if (parsePageXmlLayout(m_activePageIndex, pageWPt, pageHPt)) {
            QRectF selBoundsPt;
            for (int idx : m_selectedIndices) {
                if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
                    if (selBoundsPt.isEmpty()) {
                        selBoundsPt = m_textBoxes[idx]->boundingBox();
                    } else {
                        selBoundsPt = selBoundsPt.united(m_textBoxes[idx]->boundingBox());
                    }
                }
            }

            QString fullSelected = selectedText().simplified();
            const PageTextElement *bestEl = nullptr;
            int bestScore = -1;
            QSet<QString> matchedStyles;
            QRectF selBoundsAdj = selBoundsPt.adjusted(-1.5, -1.5, 1.5, 1.5);

            for (const PageTextElement &el : m_cachedPageLayout.elements) {
                QString elNorm = el.text.simplified();
                if (elNorm.isEmpty()) continue;

                // An element can only be part of the selection if it spatially intersects the selection bounding box
                bool intersects = el.rectPt.intersects(selBoundsAdj);
                if (!intersects) {
                    continue;
                }

                // Ensure there is meaningful spatial overlap (not just a touching pixel)
                QRectF inter = el.rectPt.intersected(selBoundsAdj);
                if (inter.width() < 1.0 || inter.height() < 1.0) {
                    continue;
                }

                // Track distinct styles among elements intersecting the user's selection
                QString styleKey = QStringLiteral("%1_%2_%3_%4")
                                      .arg(el.family)
                                      .arg(el.color.name())
                                      .arg(el.isBold ? 1 : 0)
                                      .arg(qRound(el.size));
                matchedStyles.insert(styleKey);

                // Scoring: spatial overlap + text match
                int score = qRound(inter.width() * inter.height() * 10.0);
                bool exactText = (!fullSelected.isEmpty() && elNorm == fullSelected);
                bool containsText = (!fullSelected.isEmpty() && (fullSelected.contains(elNorm) || elNorm.contains(fullSelected)));

                if (exactText) {
                    score += 10000;
                } else if (containsText) {
                    score += 5000;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestEl = &el;
                }
            }

            if (bestEl) {
                layoutMatched = true;
                m_lastCopiedTextFormat.hasMultipleStyles = (matchedStyles.size() > 1);

                if (!bestEl->family.isEmpty()) {
                    QString resolvedFamily = QFontInfo(QFont(bestEl->family)).family();
                    m_lastCopiedTextFormat.font.setFamily(!resolvedFamily.isEmpty() ? resolvedFamily : bestEl->family);
                }
                m_lastCopiedTextFormat.font.setBold(bestEl->isBold);
                m_lastCopiedTextFormat.font.setItalic(bestEl->isItalic);
                if (bestEl->isBold && bestEl->isItalic) {
                    m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold Italic"));
                } else if (bestEl->isBold) {
                    m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Bold"));
                } else if (bestEl->isItalic) {
                    m_lastCopiedTextFormat.font.setStyleName(QStringLiteral("Italic"));
                }
                if (bestEl->color.isValid()) {
                    m_lastCopiedTextFormat.color = bestEl->color;
                }
                if (bestEl->size > 0.0) {
                    m_lastCopiedTextFormat.font.setPointSizeF(bestEl->size);
                }
            }
        }
    }

    // 3. Graceful fallback to Poppler::FontIterator if layout analysis wasn't available
    if (!layoutMatched) {
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
                    isBold = fontName.contains(QStringLiteral("Bold"), Qt::CaseInsensitive) ||
                             fontName.contains(QStringLiteral("Heavy"), Qt::CaseInsensitive) ||
                             fontName.contains(QStringLiteral("Black"), Qt::CaseInsensitive);
                    isItalic = fontName.contains(QStringLiteral("Italic"), Qt::CaseInsensitive) ||
                               fontName.contains(QStringLiteral("Oblique"), Qt::CaseInsensitive);

                    QString clean = fontName;
                    int hyphenIdx = clean.indexOf('-');
                    if (hyphenIdx != -1) {
                        clean = clean.left(hyphenIdx);
                    }
                    selectedFamily = clean;
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

void PdfPageWidget::copySelectionToClipboard()
{
    QString text = selectedText();
    if (text.isEmpty()) return;

    captureSelectedTextFormat();

    if (m_lastCopiedTextFormat.hasMultipleStyles) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Multiple Styles Detected"));
        msgBox.setText(tr("Multiple styles detected.\nDo you want to paste as uniform text or copy the original area?"));
        msgBox.setIcon(QMessageBox::Question);
        QPushButton *btnText = msgBox.addButton(tr("📝 Text"), QMessageBox::AcceptRole);
        QPushButton *btnArea = msgBox.addButton(tr("🖼️ Copy Area"), QMessageBox::ActionRole);
        btnText->setMinimumWidth(100);
        btnArea->setMinimumWidth(120);
        btnText->setCursor(Qt::PointingHandCursor);
        btnArea->setCursor(Qt::PointingHandCursor);
        msgBox.setDefaultButton(btnText);
        msgBox.setEscapeButton(btnText);
        msgBox.exec();

        if (msgBox.clickedButton() == btnArea) {
            // Compute the union bounding box of all selected text boxes in PDF points
            QRectF textBoundsPt;
            for (int idx : m_selectedIndices) {
                if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
                    QRectF boxR = m_textBoxes[idx]->boundingBox();
                    if (textBoundsPt.isEmpty()) {
                        textBoundsPt = boxR;
                    } else {
                        textBoundsPt = textBoundsPt.united(boxR);
                    }
                }
            }

            if (!textBoundsPt.isEmpty()) {
                // Add comfortable 4pt padding around text bounds to prevent clipping of ascenders/descenders
                textBoundsPt.adjust(-4.0, -4.0, 4.0, 4.0);
                if (m_doc && m_activePageIndex >= 0 && m_activePageIndex < m_doc->numPages()) {
                    std::unique_ptr<Poppler::Page> page = m_doc->page(m_activePageIndex);
                    if (page) {
                        textBoundsPt = textBoundsPt.intersected(QRectF(QPointF(0, 0), page->pageSizeF()));
                    }
                }

                m_lastClipType = LastClipType::AreaImage;
                m_lastCopiedTextFormat.valid = false;

                // Transfer active text selection into visual canvas area rectangle
                QRect areaRect(qRound(textBoundsPt.x() * m_scaleX),
                               qRound(textBoundsPt.y() * m_scaleY),
                               qRound(textBoundsPt.width() * m_scaleX),
                               qRound(textBoundsPt.height() * m_scaleY));
                m_selectedAreaRect = areaRect;
                m_hasAreaSelection = true;
                m_selectedIndices.clear();
                emit selectionChanged(false);
                update();

                // Trigger 200 DPI high-resolution area snapshot copy
                emit copyAreaRequested(textBoundsPt);
                return;
            }
        }
    }

    m_lastClipType = LastClipType::Text;
    QApplication::clipboard()->setText(text);
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
        QApplication::clipboard()->setText(trimmed);
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
}

void PdfPageWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRect clip = event->rect();

    // 1. Draw canvas background matching active theme
    ThemePreset preset = ThemeManager::currentPreset();
    QColor canvasBg;
    if (preset == ThemePreset::DarkSlate) {
        canvasBg = QColor("#22262b");
    } else if (preset == ThemePreset::BreezeDark) {
        canvasBg = QColor("#181b1d");
    } else if (preset == ThemePreset::ModernLight) {
        canvasBg = QColor("#f5f5f7");
    } else { // BreezeLight
        canvasBg = QColor("#eff0f1");
    }
    painter.fillRect(clip, canvasBg);

    // Single-page fallback mode (when setPageContent was called directly)
    if (m_pageLayouts.isEmpty()) {
        if (!m_pixmap.isNull()) {
            painter.fillRect(rect(), Qt::white);
            painter.drawPixmap(0, 0, m_pixmap);
            if (!ThemeManager::isDark(preset)) {
                painter.setPen(QColor("#c5cad0"));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(rect().adjusted(0, 0, -1, -1));
            }
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

    // 2. Render each visible page
    for (const auto &pl : m_pageLayouts) {
        if (!pl.rect.intersects(clip)) {
            continue;
        }

        QRect pageR = pl.rect;

        // Subtle drop shadow around page
        painter.fillRect(pageR.adjusted(2, 2, 3, 3), QColor(0, 0, 0, 25));
        // Pure white paper background
        painter.fillRect(pageR, Qt::white);

        // Fetch or render page image from LRU cache
        QPixmap px;
        if (m_pageCache.contains(pl.pageIndex)) {
            px = m_pageCache.value(pl.pageIndex);
        } else {
            std::unique_ptr<Poppler::Page> p(m_doc->page(pl.pageIndex));
            if (p) {
                double dpi = pl.scaleX * 72.0;
                QImage img = p->renderToImage(dpi, dpi, -1, -1, -1, -1, popplerRot);
                if (!img.isNull()) {
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

        if (!px.isNull()) {
            painter.drawPixmap(pageR.topLeft(), px);
        }

        // Native subtle page border
        painter.setPen(QColor(0, 0, 0, 45));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(pageR.adjusted(0, 0, -1, -1));

        // Placed shapes, rubber stamps, and redactions on this page
        painter.save();
        painter.translate(pageR.topLeft());
        for (int sIdx = 0; sIdx < m_placedShapes.size(); ++sIdx) {
            if (m_placedShapes[sIdx].pageIndex != pl.pageIndex) continue;
            if (m_isMovingShape && sIdx == m_movingShapeIndex) {
                // Mask out original position while dragging to prevent ghosting
                painter.fillRect(m_moveShapeInitialRect, Qt::white);
                continue; // drawn with live moving overlay below
            }
            if (m_committingShapeIndex == sIdx && !m_commitOldNormRect.isNull() && !m_isMovingShape) {
                // Mask out previous committed position while interactive slider editing is active
                int pw = pl.rect.width();
                int ph = pl.rect.height();
                QRect oldPixRect(qRound(m_commitOldNormRect.x() * pw),
                                 qRound(m_commitOldNormRect.y() * ph),
                                 qRound(m_commitOldNormRect.width() * pw),
                                 qRound(m_commitOldNormRect.height() * ph));
                painter.fillRect(oldPixRect, Qt::white);
            }
            const auto &shp = m_placedShapes[sIdx];
            if (!shp.stampImage.isNull()) {
                painter.drawImage(shp.rect, shp.stampImage);
            }
            if (sIdx == m_activeHighlightIndex && shp.shapeType == 102) {
                QPen hlPen(QColor(0, 122, 255, 220), 1.0, Qt::DashLine);
                painter.setPen(hlPen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(shp.rect.adjusted(-1, -1, 1, 1));
            }
        }

        // Draw selected shape bounding indicator with corner handles
        if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_placedShapes.size() &&
            m_placedShapes[m_selectedShapeIndex].pageIndex == pl.pageIndex && !m_isMovingShape) {
            const auto &selShp = m_placedShapes[m_selectedShapeIndex];
            QRect selRect = selShp.rect.adjusted(-3, -3, 3, 3);
            QPen selPen(QColor(14, 165, 233), 1.5, Qt::DashLine);
            painter.setPen(selPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(selRect);

            // 4 corner handles
            const int hSize = 6;
            painter.setPen(QColor(14, 165, 233));
            painter.setBrush(Qt::white);
            const QPoint handles[4] = {
                selRect.topLeft(),
                selRect.topRight(),
                selRect.bottomLeft(),
                selRect.bottomRight()
            };
            for (const auto &hp : handles) {
                painter.drawRect(hp.x() - hSize / 2, hp.y() - hSize / 2, hSize, hSize);
            }
        }
        painter.restore();

        // AcroForms, pending background wipe, collision highlights & selection for active page
        if (pl.pageIndex == m_activePageIndex) {
            painter.save();
            painter.translate(pageR.topLeft());
            if (m_pendingErasedPtRect.isValid() && !m_pendingErasedPtRect.isEmpty()) {
                QRect wipedPix(qRound(m_pendingErasedPtRect.x() * pl.scaleX),
                               qRound(m_pendingErasedPtRect.y() * pl.scaleY),
                               qRound(m_pendingErasedPtRect.width() * pl.scaleX),
                               qRound(m_pendingErasedPtRect.height() * pl.scaleY));
                painter.fillRect(wipedPix, m_pendingErasedBgColor);
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
        QRect r = PdfCanvasTools::normalizeRect(m_dragStart, m_dragCurrent);
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
                                       m_dragStart, m_dragCurrent);
        painter.restore();
    }

    // 4. Moving Shape Live Overlay or Hover Indicator
    if ((m_toolMode == PdfToolMode::Shape || m_toolMode == PdfToolMode::Watermark || m_toolMode == PdfToolMode::Redaction || m_toolMode == PdfToolMode::Highlight) &&
        m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        const auto &pl = m_pageLayouts[m_activePageIndex];
        painter.save();
        painter.translate(pl.rect.topLeft());

        if (m_isMovingShape && m_movingShapeIndex >= 0 && m_movingShapeIndex < m_placedShapes.size()) {
            const auto &shp = m_placedShapes[m_movingShapeIndex];
            if (!shp.stampImage.isNull()) {
                painter.drawImage(m_moveShapeCurrentRect, shp.stampImage);
            }
            QPen movePen(QColor(0, 122, 255, 220), 1.5, Qt::DashLine);
            painter.setPen(movePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(m_moveShapeCurrentRect);
        } else if (!m_isDraggingTool && m_hoveredShapeIndex >= 0 && m_hoveredShapeIndex < m_placedShapes.size()) {
            QPen hoverPen(QColor(0, 122, 255, 180), 1.0, Qt::DashLine);
            painter.setPen(hoverPen);
            painter.setBrush(QColor(0, 122, 255, 20));
            painter.drawRect(m_placedShapes[m_hoveredShapeIndex].rect);
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
        if (m_committingShapeIndex >= 0) {
            commitSelectedStampToPdf();
        }

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

        // 0a-2. Interactive Highlight Re-selection and Direct Move
        if (m_toolMode == PdfToolMode::Highlight) {
            clearAreaSelection();
            int hlIdx = -1;
            for (int i = m_placedShapes.size() - 1; i >= 0; --i) {
                if (m_placedShapes[i].pageIndex == m_activePageIndex &&
                    m_placedShapes[i].shapeType == 102 &&
                    m_placedShapes[i].rect.contains(localPos)) {
                    hlIdx = i;
                    break;
                }
            }
            if (hlIdx >= 0) {
                m_isMovingShape = true;
                m_movingShapeIndex = hlIdx;
                m_moveShapeStartMouse = localPos;
                m_moveShapeInitialRect = m_placedShapes[hlIdx].rect;
                m_moveShapeCurrentRect = m_moveShapeInitialRect;
                m_activeHighlightIndex = hlIdx;
                setCursor(Qt::SizeAllCursor);
                update();
                event->accept();
                return;
            } else if (m_activeHighlightIndex >= 0) {
                m_activeHighlightIndex = -1;
                update();
            }
        }

        // 0b. Continuous Shape & Stamp Tool: Direct Move or Instant Draw/Placement
        if (m_toolMode == PdfToolMode::Shape || m_toolMode == PdfToolMode::Watermark || (m_isEditMode && m_toolMode == PdfToolMode::SelectText)) {
            clearAreaSelection();

            // Check if clicked on an existing placed shape/stamp using accurate vector hit-testing
            int shapeIdx = -1;
            for (int i = m_placedShapes.size() - 1; i >= 0; --i) {
                const auto &item = m_placedShapes[i];
                if (item.pageIndex != m_activePageIndex) continue;
                if (PdfCanvasTools::hitTestShape(item.shapeType, item.rect, item.startPt, item.endPt,
                                                 item.strokeWidth, item.fillColor, localPos)) {
                    shapeIdx = i;
                    break;
                }
            }

            if (shapeIdx >= 0) {
                m_selectedShapeIndex = shapeIdx;
                m_isMovingShape = true;
                m_movingShapeIndex = shapeIdx;
                m_moveShapeStartMouse = localPos;
                m_moveShapeInitialRect = m_placedShapes[shapeIdx].rect;
                m_moveShapeCurrentRect = m_moveShapeInitialRect;
                setCursor(Qt::SizeAllCursor);

                const auto &clickedItem = m_placedShapes[shapeIdx];
                if (clickedItem.shapeType == 100) {
                    emit stampSelectedForContextualEdit(clickedItem.stampText, clickedItem.stampColor,
                                                        clickedItem.stampAngle, clickedItem.stampStyle,
                                                        clickedItem.stampSize);
                }

                update();
                event->accept();
                return;
            } else if (m_selectedShapeIndex >= 0) {
                m_selectedShapeIndex = -1;
                update();
            }

            if (m_toolMode == PdfToolMode::Watermark) {
                // Clicked on empty space: place rubber stamp directly at clicked position!
                int pageW = activePR.width();
                int pageH = activePR.height();
                if (pageW > 0 && pageH > 0 && !m_watermarkText.isEmpty()) {
                    QRect stampRect = PdfCanvasTools::calculateStampRect(m_watermarkText, m_watermarkAngle,
                                                                         m_watermarkSize, localPos, pageW, pageH);

                    QImage stampImg(stampRect.size(), QImage::Format_ARGB32_Premultiplied);
                    stampImg.fill(Qt::transparent);
                    QPainter p(&stampImg);
                    p.setRenderHint(QPainter::Antialiasing, true);
                    PdfTransformBox::drawWatermarkStamp(p, QRect(QPoint(0, 0), stampRect.size()),
                                                        m_watermarkText, m_watermarkAngle,
                                                        m_watermarkOpacity, m_watermarkColor,
                                                        m_watermarkStyle);
                    p.end();

                    QRectF normRect(static_cast<double>(stampRect.x()) / pageW,
                                   static_cast<double>(stampRect.y()) / pageH,
                                   static_cast<double>(stampRect.width()) / pageW,
                                   static_cast<double>(stampRect.height()) / pageH);

                    PlacedShapeItem item;
                    item.id = ++m_shapeIdCounter;
                    item.pageIndex = m_activePageIndex;
                    item.shapeType = 100;
                    item.rect = stampRect;
                    item.normRect = normRect;
                    item.stampImage = stampImg;
                    item.stampText = m_watermarkText;
                    item.stampAngle = m_watermarkAngle;
                    item.stampOpacity = m_watermarkOpacity;
                    item.stampColor = m_watermarkColor;
                    item.stampStyle = m_watermarkStyle;
                    item.stampSize = m_watermarkSize;
                    m_placedShapes.append(item);
                    m_selectedShapeIndex = m_placedShapes.size() - 1;

                    emit addStampAnnotationRequested(stampImg, normRect);
                    update();
                    event->accept();
                    return;
                }
            }

            if (m_toolMode == PdfToolMode::Shape) {
                // Clicked on empty space: start drawing a new shape
                m_isDraggingTool = true;
                m_dragStart = localPos;
                m_dragCurrent = localPos;
                update();
                event->accept();
                return;
            }
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

    } else if (event->button() == Qt::MiddleButton) {
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

    if (m_isMovingShape && m_movingShapeIndex >= 0 && m_movingShapeIndex < m_placedShapes.size()) {
        QPoint delta = localPos - m_moveShapeStartMouse;
        m_moveShapeCurrentRect = m_moveShapeInitialRect.translated(delta);
        update();
        event->accept();
        return;
    }

    if (m_isDraggingTool) {
        QPoint currentPos = localPos;
        if (m_toolMode == PdfToolMode::Shape && (event->modifiers() & Qt::ShiftModifier)) {
            if (m_shapeType == 0 || m_shapeType == 1 || m_shapeType == 4) { // Rectangle, Ellipse, Rounded Rect -> 1:1 Aspect Ratio
                int dx = currentPos.x() - m_dragStart.x();
                int dy = currentPos.y() - m_dragStart.y();
                int side = std::max(std::abs(dx), std::abs(dy));
                currentPos = m_dragStart + QPoint((dx >= 0 ? 1 : -1) * side, (dy >= 0 ? 1 : -1) * side);
            } else if (m_shapeType == 2 || m_shapeType == 3) { // Arrow or Line -> Snap to 45 degree angles
                double dx = currentPos.x() - m_dragStart.x();
                double dy = currentPos.y() - m_dragStart.y();
                double len = std::hypot(dx, dy);
                if (len > 0) {
                    double angle = std::atan2(dy, dx);
                    double snapped = std::round(angle / (M_PI / 4.0)) * (M_PI / 4.0);
                    currentPos = m_dragStart + QPoint(qRound(len * std::cos(snapped)),
                                                       qRound(len * std::sin(snapped)));
                }
            }
        }
        m_dragCurrent = currentPos;

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

    if ((m_toolMode == PdfToolMode::Shape || m_toolMode == PdfToolMode::Watermark ||
         m_toolMode == PdfToolMode::Redaction || m_toolMode == PdfToolMode::Highlight ||
         (m_isEditMode && m_toolMode == PdfToolMode::SelectText)) &&
        !m_isDraggingTool && !m_isMovingShape) {
        int shapeIdx = -1;
        for (int i = m_placedShapes.size() - 1; i >= 0; --i) {
            const auto &item = m_placedShapes[i];
            if (item.pageIndex != m_activePageIndex) continue;
            if (PdfCanvasTools::hitTestShape(item.shapeType, item.rect, item.startPt, item.endPt,
                                             item.strokeWidth, item.fillColor, localPos)) {
                shapeIdx = i;
                break;
            }
        }
        if (shapeIdx != m_hoveredShapeIndex) {
            m_hoveredShapeIndex = shapeIdx;
            if (m_hoveredShapeIndex >= 0) {
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(m_toolMode == PdfToolMode::SelectText ? Qt::IBeamCursor : Qt::CrossCursor);
            }
            update();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void PdfPageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QRect activePR = pageRect(m_activePageIndex);
    QPoint localPos = event->pos() - activePR.topLeft();

    // 0. Finish Moving Shape
    if (event->button() == Qt::LeftButton && m_isMovingShape) {
        m_isMovingShape = false;
        int idx = m_movingShapeIndex;
        m_movingShapeIndex = -1;
        if (idx >= 0 && idx < m_placedShapes.size()) {
            QPoint delta = localPos - m_moveShapeStartMouse;
            if (delta.manhattanLength() >= 2) {
                int pageW = activePR.width();
                int pageH = activePR.height();
                if (pageW > 0 && pageH > 0) {
                    QRectF oldNorm = m_placedShapes[idx].normRect;
                    QRect newRect = m_moveShapeCurrentRect;
                    QRectF newNorm(static_cast<double>(newRect.x()) / pageW,
                                   static_cast<double>(newRect.y()) / pageH,
                                   static_cast<double>(newRect.width()) / pageW,
                                   static_cast<double>(newRect.height()) / pageH);
                    m_placedShapes[idx].rect = newRect;
                    m_placedShapes[idx].normRect = newNorm;
                    emit moveStampAnnotationRequested(oldNorm, newNorm, m_placedShapes[idx].stampImage);
                }
            }
            m_selectedShapeIndex = idx;
        }
        update();
        event->accept();
        return;
    }

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

                    PlacedShapeItem item;
                    item.id = ++m_shapeIdCounter;
                    item.pageIndex = m_activePageIndex;
                    item.shapeType = 101; // Redaction
                    item.rect = r;
                    item.normRect = normRect;
                    item.stampImage = redactImg;
                    m_placedShapes.append(item);

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
                    }

                    emit addStampAnnotationRequested(redactImg, normRect);
                    update();
                }
            }
            if (!m_continuousMode) {
                setToolMode(PdfToolMode::SelectText);
            }

        } else if (m_toolMode == PdfToolMode::Shape) {
            bool validShape = false;
            if (m_shapeType == 2 || m_shapeType == 3) { // Arrow or Line
                validShape = ((m_dragCurrent - m_dragStart).manhattanLength() >= 4);
            } else {
                validShape = (r.width() >= 4 && r.height() >= 4);
            }

            if (validShape) {
                commitShapeDrawing(r, m_dragStart, m_dragCurrent);
            }
            // Continuous drawing: remain in Shape mode without opening transform box!
        } else if (m_toolMode == PdfToolMode::Highlight) {
            if (r.width() >= 4 && r.height() >= 4) {
                commitHighlightDrawing(r);
            }
            if (!m_continuousMode) {
                setToolMode(PdfToolMode::SelectText);
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
                captureSelectedTextFormat();
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton && m_isPanning) {
        m_isPanning = false;
        setCursor(m_toolMode == PdfToolMode::SelectText ? Qt::IBeamCursor : Qt::ArrowCursor);
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
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < m_placedShapes.size()) {
            deleteSelectedShape();
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Escape) {
        if (m_selectedShapeIndex >= 0) {
            m_selectedShapeIndex = -1;
            update();
            event->accept();
            return;
        }
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

void PdfPageWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int clickedPage = pageIndexAtPos(event->pos());
    if (clickedPage >= 0 && clickedPage != m_activePageIndex) {
        setActivePage(clickedPage);
    }
    QRect activePR = pageRect(m_activePageIndex);
    QPoint localPos = event->pos() - activePR.topLeft();

    // Check if right-clicked on an existing shape
    int rightClickedShapeIdx = -1;
    for (int i = m_placedShapes.size() - 1; i >= 0; --i) {
        const auto &item = m_placedShapes[i];
        if (item.pageIndex != m_activePageIndex) continue;
        if (PdfCanvasTools::hitTestShape(item.shapeType, item.rect, item.startPt, item.endPt,
                                         item.strokeWidth, item.fillColor, localPos)) {
            rightClickedShapeIdx = i;
            break;
        }
    }

    if (rightClickedShapeIdx >= 0) {
        m_selectedShapeIndex = rightClickedShapeIdx;
        update();

        QMenu shapeMenu(this);
        ThemePreset preset = ThemeManager::currentPreset();
        if (ThemeManager::isDark(preset)) {
            shapeMenu.setStyleSheet(
                "QMenu { background-color: #21252b; color: #e2e8f0; border: 1px solid #3e4451; border-radius: 6px; padding: 4px 0px; }"
                "QMenu::item { padding: 6px 24px 6px 12px; }"
                "QMenu::item:selected { background-color: #38bdf8; color: #ffffff; }"
            );
        } else {
            shapeMenu.setStyleSheet(
                "QMenu { background-color: #ffffff; color: #1d1d1f; border: 1px solid #d2d2d7; border-radius: 6px; padding: 4px 0px; }"
                "QMenu::item { padding: 6px 24px 6px 12px; }"
                "QMenu::item:selected { background-color: #007aff; color: #ffffff; }"
            );
        }
        if (m_placedShapes[rightClickedShapeIdx].shapeType == 100) {
            shapeMenu.addAction(QIcon::fromTheme("edit-delete"), tr("🗑️ Delete Stamp"), this, &PdfPageWidget::deleteSelectedShape);
        } else {
            shapeMenu.addAction(QIcon::fromTheme("edit-delete"), tr("🗑️ Delete Shape"), this, &PdfPageWidget::deleteSelectedShape);
        }
        shapeMenu.exec(event->globalPos());
        event->accept();
        return;
    }

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
        if (m_isEditMode) {
            menu.addAction(tr("✏️ Edit / Replace Text in Area..."), this, &PdfPageWidget::onTransformBoxEditRequested);
            menu.addSeparator();
            QAction *actCut = menu.addAction(QIcon::fromTheme("edit-cut"), tr("Cut Area"), this, &PdfPageWidget::cutSelectedArea);
            actCut->setShortcut(QKeySequence::Cut);
        }

        QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Area"), this, &PdfPageWidget::copySelectedArea);
        actCopy->setShortcut(QKeySequence::Copy);

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

            menu.addSeparator();
            menu.addAction(QIcon::fromTheme("draw-highlight"), tr("🖍️ Highlight Selected Text"), this, [this]() {
                highlightSelectedText(m_highlighterColor, m_highlighterOpacity);
            });
            menu.addAction(tr("⬛ Redact / Blackout Text"), this, &PdfPageWidget::redactSelectedText);
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
        }

        menu.addSeparator();

        menu.addAction(QIcon::fromTheme("edit-select"), tr("Select Area..."), this, &PdfPageWidget::startSelectArea);
        menu.addAction(QIcon::fromTheme("insert-image"), tr("Insert Image..."), this, &PdfPageWidget::insertImageRequested);
    }

    QAction *actCopyPage = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Page Text"), this, &PdfPageWidget::copyPageTextToClipboard);
    actCopyPage->setEnabled(!m_textBoxes.empty());

    menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Entire Document Text"), this, &PdfPageWidget::copyDocumentRequested);

    menu.addSeparator();

    QAction *actSelectAll = menu.addAction(QIcon::fromTheme("edit-select-all"), tr("Select All"), this, &PdfPageWidget::selectAll);
    actSelectAll->setEnabled(!m_textBoxes.empty());
    actSelectAll->setShortcut(QKeySequence::SelectAll);

    menu.exec(event->globalPos());
}

