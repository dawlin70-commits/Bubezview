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
#include "pdf_image_dialog.h"
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
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QXmlStreamReader>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QToolTip>
#include <QVariantAnimation>
#include <QEasingCurve>
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

        if (m_fitToWidth && availWidth > 120 && origSize.width() > 0) {
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
            m_transformBox->setGeometry(m_pageLayouts[m_activePageIndex].rect);
        }
    }
}

void PdfPageWidget::setActivePage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pageLayouts.size()) return;
    if (m_activePageIndex == pageIndex) return;

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

void PdfPageWidget::setToolMode(PdfToolMode mode)
{
    if (m_toolMode == mode) return;

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
        setCursor(Qt::CrossCursor);
        break;
    case PdfToolMode::Typewriter:
    case PdfToolMode::Signature:
    case PdfToolMode::InsertImage:
    case PdfToolMode::Watermark:
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
    setToolMode(PdfToolMode::Typewriter);

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = visibleCenter();
    }

    if (m_transformBox) {
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
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showImage(image, targetPos, defSize, PdfTransformItemType::Image);
    }
}

void PdfPageWidget::cutSelection()
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    // 1. Copy text to system clipboard
    copySelectionToClipboard();

    // 2. Identify bounding rectangles of the selected text boxes and apply whiteout
    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageWPt = m_pageLayouts[m_activePageIndex].rect.width() / m_scaleX;
        pageHPt = m_pageLayouts[m_activePageIndex].rect.height() / m_scaleY;
    }

    if (pageWPt > 0 && pageHPt > 0) {
        QList<QPair<QImage, QRectF>> batchStamps;
        for (int idx : m_selectedIndices) {
            if (idx >= 0 && idx < static_cast<int>(m_textBoxes.size())) {
                const auto &box = m_textBoxes[idx];
                QRectF ptRect = box->boundingBox();

                // Expand by 1pt to guarantee clean whiteout coverage over font anti-aliasing
                QRectF normRect(std::max(0.0, (ptRect.x() - 1.0) / pageWPt),
                                std::max(0.0, (ptRect.y() - 1.0) / pageHPt),
                                std::min(1.0, (ptRect.width() + 2.0) / pageWPt),
                                std::min(1.0, (ptRect.height() + 2.0) / pageHPt));

                int pixW = std::max(4, qRound((ptRect.width() + 2.0) * m_scaleX));
                int pixH = std::max(4, qRound((ptRect.height() + 2.0) * m_scaleY));
                QImage whiteImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
                whiteImg.fill(Qt::white);

                batchStamps.append(qMakePair(whiteImg, normRect));
            }
        }
        if (!batchStamps.isEmpty()) {
            emit addStampAnnotationsRequested(batchStamps);
        }
    }

    // 3. Clear selection and update display
    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);

    QToolTip::showText(QCursor::pos(), tr("✂️ Text cut and copied to clipboard"));
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
        return;
    }
}

void PdfPageWidget::startWatermark(const QString &text, qreal angle, qreal opacity,
                                   const QColor &color, const QPoint &initialPos, int style)
{
    setToolMode(PdfToolMode::Watermark);

    QPoint targetPos = initialPos;
    if (targetPos.isNull()) {
        targetPos = visibleCenter();
    }

    if (m_transformBox) {
        m_transformBox->setGeometry(pageRect(m_activePageIndex));
        m_transformBox->showWatermark(text, angle, opacity, color, targetPos, style);
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

void PdfPageWidget::startSnapshot()
{
    setToolMode(PdfToolMode::Snapshot);
}

void PdfPageWidget::startRedaction()
{
    setToolMode(PdfToolMode::Redaction);
}

void PdfPageWidget::startHighlighter(const QColor &color, double opacity)
{
    m_highlighterColor = color.isValid() ? color : QColor(255, 235, 59);
    m_highlighterOpacity = qBound(0.05, opacity, 1.0);
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
}

void PdfPageWidget::highlightSelectedText(const QColor &color, double opacity)
{
    if (m_selectedIndices.empty() || m_textBoxes.empty()) return;

    QColor hlColor = color.isValid() ? color : m_highlighterColor;
    double hlOpacity = qBound(0.05, opacity, 1.0);

    double pageWPt = width() / m_scaleX;
    double pageHPt = height() / m_scaleY;
    if (m_activePageIndex >= 0 && m_activePageIndex < m_pageLayouts.size()) {
        pageWPt = m_pageLayouts[m_activePageIndex].rect.width() / m_scaleX;
        pageHPt = m_pageLayouts[m_activePageIndex].rect.height() / m_scaleY;
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

            int pixW = std::max(4, qRound(ptRect.width() * m_scaleX * 2.0));
            int pixH = std::max(4, qRound(ptRect.height() * m_scaleY * 2.0));
            QImage hlImg(pixW, pixH, QImage::Format_ARGB32_Premultiplied);
            QColor c = hlColor;
            c.setAlphaF(hlOpacity);
            hlImg.fill(c);

            batchStamps.append(qMakePair(hlImg, normRect));
        }
    }

    if (!batchStamps.isEmpty()) {
        emit addStampAnnotationsRequested(batchStamps);
    }

    m_selectedIndices.clear();
    update();
    emit selectionChanged(false);
}

void PdfPageWidget::cancelActiveTool()
{
    if (m_transformBox) {
        m_transformBox->dismiss();
    }
    m_isDraggingTool = false;
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

    case PdfTransformItemType::Text: {
        emit addTextAnnotationRequested(m_transformBox->text(),
                                        m_transformBox->textFont(),
                                        m_transformBox->textColor(),
                                        normRect,
                                        m_transformBox->isWhiteout());
        m_transformBox->dismiss();
        setToolMode(PdfToolMode::SelectText);
        update();
        return;
    }

    default:
        break;
    }

    if (!annImg.isNull()) {
        emit addStampAnnotationRequested(annImg, normRect);
    }

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
    m_transformBox->dismiss();
    setToolMode(PdfToolMode::SelectText);
    update();
}

void PdfPageWidget::onTransformBoxEditRequested()
{
    if (!m_transformBox) return;

    QPoint currentCenter = m_transformBox->boxRect().center();

    switch (m_transformBox->itemType()) {
    case PdfTransformItemType::Text: {
        PdfTypewriterDialog dlg(window());
        dlg.setText(m_transformBox->text());
        dlg.setSelectedFont(m_transformBox->textFont());
        dlg.setTextColor(m_transformBox->textColor());
        dlg.setWhiteout(m_transformBox->isWhiteout());

        if (dlg.exec() == QDialog::Accepted) {
            m_transformBox->showText(dlg.text(), dlg.selectedFont(), dlg.textColor(), dlg.isWhiteout(), currentCenter);
            m_transformBox->setTextModified(true);
            m_lastCopiedTextFormat.isTextModified = true;
        }
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
        PdfImageDialog dlg(window());
        if (dlg.exec() == QDialog::Accepted) {
            QImage img = dlg.selectedImage();
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
        if (targetRect.width() <= 2 || targetRect.height() <= 2) return;
    }

    QImage shapeImg = PdfCanvasTools::renderShapeStamp(m_shapeType, m_shapeStrokeWidth,
                                                       m_shapeStrokeColor, m_shapeFillColor,
                                                       targetRect.size(), localStart, localEnd);
    if (shapeImg.isNull()) return;

    QRectF normRect(static_cast<double>(targetRect.x()) / pageW,
                    static_cast<double>(targetRect.y()) / pageH,
                    static_cast<double>(targetRect.width()) / pageW,
                    static_cast<double>(targetRect.height()) / pageH);

    emit addStampAnnotationRequested(shapeImg, normRect);
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
    QStringList args;
    args << QStringLiteral("-xml")
         << QStringLiteral("-stdout")
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

                double pt = (h > 4.0) ? (h / 1.35) : h;
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
                    m_lastCopiedTextFormat.font.setFamily(bestEl->family);
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
                m_lastCopiedTextFormat.font.setFamily(selectedFamily);
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
    if (preset == ThemePreset::SystemNative) {
        canvasBg = QColor("#e4e7eb");
    } else if (preset == ThemePreset::DarkSlate) {
        canvasBg = QColor("#181a1d");
    } else {
        canvasBg = QColor("#232629");
    }
    painter.fillRect(clip, canvasBg);

    // Single-page fallback mode (when setPageContent was called directly)
    if (m_pageLayouts.isEmpty()) {
        if (!m_pixmap.isNull()) {
            painter.fillRect(rect(), Qt::white);
            painter.drawPixmap(0, 0, m_pixmap);
            if (preset == ThemePreset::SystemNative) {
                painter.setPen(QColor("#c5cad0"));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(rect().adjusted(0, 0, -1, -1));
            }
            PdfCanvasTools::drawAcroFormHighlights(painter, m_formFields, m_scaleX, m_scaleY);
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

        // AcroForms & selection for active page
        if (pl.pageIndex == m_activePageIndex) {
            painter.save();
            painter.translate(pageR.topLeft());
            PdfCanvasTools::drawAcroFormHighlights(painter, m_formFields, pl.scaleX, pl.scaleY);
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
                     : (m_toolMode == PdfToolMode::Highlight) ? 5 : 0;
        QColor hlColor = m_highlighterColor;
        hlColor.setAlphaF(m_highlighterOpacity);
        PdfCanvasTools::drawRubberBand(painter, r, modeCode, snapW, snapH,
                                       m_shapeType, m_shapeStrokeWidth, m_shapeStrokeColor,
                                       (m_toolMode == PdfToolMode::Highlight ? hlColor : m_shapeFillColor),
                                       m_dragStart, m_dragCurrent);
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

        // 1. Tool Drag Modes (SelectArea, Snapshot, Redaction, Shape, Highlight)
        if (m_toolMode == PdfToolMode::SelectArea ||
            m_toolMode == PdfToolMode::Snapshot ||
            m_toolMode == PdfToolMode::Redaction ||
            m_toolMode == PdfToolMode::Shape ||
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
        if (m_toolMode == PdfToolMode::SelectText && !m_formFields.empty()) {
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

    if (m_isDraggingTool) {
        m_dragCurrent = localPos;

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
            if (r.width() >= 6 && r.height() >= 6) {
                if (m_transformBox) {
                    m_transformBox->setGeometry(activePR);
                    m_transformBox->showRedaction(r);
                }
            } else if (!m_continuousMode) {
                setToolMode(PdfToolMode::SelectText);
            }

        } else if (m_toolMode == PdfToolMode::Shape) {
            bool validShape = false;
            if (m_shapeType == 2 || m_shapeType == 3) {
                validShape = ((m_dragCurrent - m_dragStart).manhattanLength() >= 4);
            } else {
                validShape = (r.width() >= 4 && r.height() >= 4);
            }

            if (validShape) {
                commitShapeDrawing(r, m_dragStart, m_dragCurrent);
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
        }

        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        m_selectionEnd = localPos;
        updateSelection(m_selectionStart, m_selectionEnd);
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
                m_selectedIndices = {idx};
                update();
                emit selectionChanged(true);
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
            m_transformBox->dismiss();
            setToolMode(PdfToolMode::SelectText);
            update();
            event->accept();
            return;
        }
        if (m_hasAreaSelection) {
            clearAreaSelection();
            event->accept();
            return;
        }
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
        if (m_hasAreaSelection) {
            cutSelectedArea();
        } else if (hasSelection()) {
            cutSelection();
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
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
    if (preset != ThemePreset::SystemNative) {
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
    }

    if (m_hasAreaSelection) {
        QAction *actCut = menu.addAction(QIcon::fromTheme("edit-cut"), tr("Cut Area"), this, &PdfPageWidget::cutSelectedArea);
        actCut->setShortcut(QKeySequence::Cut);

        QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Area"), this, &PdfPageWidget::copySelectedArea);
        actCopy->setShortcut(QKeySequence::Copy);

        menu.addAction(tr("Clear Area Selection"), this, &PdfPageWidget::clearAreaSelection);
        menu.addSeparator();
    } else if (hasSelection()) {
        QAction *actCopy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Text"), this, &PdfPageWidget::copySelectionToClipboard);
        actCopy->setShortcut(QKeySequence::Copy);

        QAction *actCut = menu.addAction(QIcon::fromTheme("edit-cut"), tr("Cut Text"), this, &PdfPageWidget::cutSelection);
        actCut->setShortcut(QKeySequence::Cut);

        menu.addAction(QIcon::fromTheme("draw-highlight"), tr("Highlight Selected Text"), this, [this]() {
            highlightSelectedText(m_highlighterColor, m_highlighterOpacity);
        });
        menu.addSeparator();
    }

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

    QAction *actCopyPage = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Page Text"), this, &PdfPageWidget::copyPageTextToClipboard);
    actCopyPage->setEnabled(!m_textBoxes.empty());

    menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Entire Document Text"), this, &PdfPageWidget::copyDocumentRequested);

    menu.addSeparator();

    QAction *actSelectAll = menu.addAction(QIcon::fromTheme("edit-select-all"), tr("Select All"), this, &PdfPageWidget::selectAll);
    actSelectAll->setEnabled(!m_textBoxes.empty());
    actSelectAll->setShortcut(QKeySequence::SelectAll);

    menu.exec(event->globalPos());
}

