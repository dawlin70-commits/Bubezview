/**
 * @file pdf_transform_box.cpp
 * @brief Implementation of interactive 8-handle transformation overlay for PDF items.
 */

#include "pdf_transform_box.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QColorDialog>
#include <QtMath>
#include <cmath>

PdfTransformBox::PdfTransformBox(QWidget *parent)
    : QWidget(parent)
    , m_itemType(PdfTransformItemType::Text)
    , m_boxRect(100, 100, 200, 80)
    , m_activeHandle(None)
    , m_whiteout(false)
    , m_angle(0.0)
    , m_opacity(1.0)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    // Floating action pill toolbar
    m_toolbar = new QWidget(this);
    m_toolbar->setStyleSheet(
        "QWidget { background-color: #141A28; border: 1px solid #38BDF8; border-radius: 14px; }"
        "QPushButton { background: transparent; border: none; font-size: 11px; font-weight: 700; padding: 4px 8px; }"
        "QPushButton#BtnEdit { color: #38BDF8; }"
        "QPushButton#BtnEdit:hover { background-color: #1E293B; border-radius: 10px; }"
        "QPushButton#BtnApply { color: #4ADE80; }"
        "QPushButton#BtnApply:hover { background-color: #1E293B; border-radius: 10px; }"
        "QPushButton#BtnApplyAndContinue { color: #38BDF8; font-weight: 700; }"
        "QPushButton#BtnApplyAndContinue:hover { background-color: #1E293B; border-radius: 10px; }"
        "QPushButton#BtnCutArea { color: #38BDF8; font-weight: 700; }"
        "QPushButton#BtnCutArea:hover { background-color: #1E293B; border-radius: 10px; }"
        "QPushButton#BtnCopyArea { color: #FBBF24; font-weight: 700; }"
        "QPushButton#BtnCopyArea:hover { background-color: #1E293B; border-radius: 10px; }"
        "QPushButton#BtnCancel { color: #F87171; }"
        "QPushButton#BtnCancel:hover { background-color: #1E293B; border-radius: 10px; }"
    );
    m_toolbar->setFixedHeight(28);

    auto shadow = new QGraphicsDropShadowEffect(m_toolbar);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 3);
    m_toolbar->setGraphicsEffect(shadow);

    QHBoxLayout *tbLayout = new QHBoxLayout(m_toolbar);
    tbLayout->setContentsMargins(6, 2, 6, 2);
    tbLayout->setSpacing(2);

    m_btnEdit = new QPushButton(tr("✏️ Edit"), m_toolbar);
    m_btnEdit->setObjectName(QStringLiteral("BtnEdit"));
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    connect(m_btnEdit, &QPushButton::clicked, this, &PdfTransformBox::editRequested);

    m_btnRedactBlack = new QPushButton(QStringLiteral("⬛"), m_toolbar);
    m_btnRedactBlack->setCursor(Qt::PointingHandCursor);
    m_btnRedactBlack->setToolTip(tr("Black Redaction"));
    m_btnRedactBlack->setVisible(false);
    connect(m_btnRedactBlack, &QPushButton::clicked, this, [this]() { setRedactionColor(Qt::black); });

    m_btnRedactWhite = new QPushButton(QStringLiteral("⬜"), m_toolbar);
    m_btnRedactWhite->setCursor(Qt::PointingHandCursor);
    m_btnRedactWhite->setToolTip(tr("White Redaction"));
    m_btnRedactWhite->setVisible(false);
    connect(m_btnRedactWhite, &QPushButton::clicked, this, [this]() { setRedactionColor(Qt::white); });

    m_btnRedactGray = new QPushButton(QStringLiteral("🩶"), m_toolbar);
    m_btnRedactGray->setCursor(Qt::PointingHandCursor);
    m_btnRedactGray->setToolTip(tr("Gray Redaction"));
    m_btnRedactGray->setVisible(false);
    connect(m_btnRedactGray, &QPushButton::clicked, this, [this]() { setRedactionColor(QColor(128, 128, 128)); });

    m_btnRedactCustom = new QPushButton(QStringLiteral("🎨"), m_toolbar);
    m_btnRedactCustom->setCursor(Qt::PointingHandCursor);
    m_btnRedactCustom->setToolTip(tr("Custom Color Redaction..."));
    m_btnRedactCustom->setVisible(false);
    connect(m_btnRedactCustom, &QPushButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_redactionColor, this, tr("Select Redaction Color"));
        if (chosen.isValid()) {
            setRedactionColor(chosen);
        }
    });

    m_btnApply = new QPushButton(tr("✓ Apply"), m_toolbar);
    m_btnApply->setObjectName(QStringLiteral("BtnApply"));
    m_btnApply->setCursor(Qt::PointingHandCursor);
    connect(m_btnApply, &QPushButton::clicked, this, &PdfTransformBox::applied);

    m_btnApplyAndContinue = new QPushButton(tr("🔄 Stamp Again"), m_toolbar);
    m_btnApplyAndContinue->setObjectName(QStringLiteral("BtnApplyAndContinue"));
    m_btnApplyAndContinue->setCursor(Qt::PointingHandCursor);
    m_btnApplyAndContinue->setToolTip(tr("Apply current item and place another copy ready on canvas"));
    m_btnApplyAndContinue->setVisible(false);
    connect(m_btnApplyAndContinue, &QPushButton::clicked, this, &PdfTransformBox::applyAndContinue);

    m_btnCutArea = new QPushButton(tr("✂️ Cut"), m_toolbar);
    m_btnCutArea->setObjectName(QStringLiteral("BtnCutArea"));
    m_btnCutArea->setCursor(Qt::PointingHandCursor);
    m_btnCutArea->setToolTip(tr("Cut selected area and copy to clipboard (Ctrl+X)"));
    m_btnCutArea->setVisible(false);
    connect(m_btnCutArea, &QPushButton::clicked, this, &PdfTransformBox::cutAreaRequested);

    m_btnCopyArea = new QPushButton(tr("📋 Copy"), m_toolbar);
    m_btnCopyArea->setObjectName(QStringLiteral("BtnCopyArea"));
    m_btnCopyArea->setCursor(Qt::PointingHandCursor);
    m_btnCopyArea->setToolTip(tr("Copy selected area to clipboard at 300 DPI (Ctrl+C)"));
    m_btnCopyArea->setVisible(false);
    connect(m_btnCopyArea, &QPushButton::clicked, this, &PdfTransformBox::copyAreaRequested);

    m_btnCancel = new QPushButton(tr("✕ Cancel"), m_toolbar);
    m_btnCancel->setObjectName(QStringLiteral("BtnCancel"));
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &PdfTransformBox::cancelled);

    tbLayout->addWidget(m_btnEdit);
    tbLayout->addWidget(m_btnRedactBlack);
    tbLayout->addWidget(m_btnRedactWhite);
    tbLayout->addWidget(m_btnRedactGray);
    tbLayout->addWidget(m_btnRedactCustom);
    tbLayout->addWidget(m_btnApply);
    tbLayout->addWidget(m_btnApplyAndContinue);
    tbLayout->addWidget(m_btnCutArea);
    tbLayout->addWidget(m_btnCopyArea);
    tbLayout->addWidget(m_btnCancel);

    updateHandles();
}

PdfTransformBox::~PdfTransformBox() = default;


void PdfTransformBox::showText(const QString &text, const QFont &font, const QColor &textColor,
                              bool whiteout, const QPoint &initialPos)
{
    m_itemType = PdfTransformItemType::Text;
    m_text = text;
    m_font = font;
    m_color = textColor;
    m_whiteout = whiteout;
    m_isTextModified = false;

    QFontMetrics fm(m_font);

    // Target a natural readable width: ~70% of available page width, capped reasonably
    int maxAllowedW = std::max(240, width() - 40);
    int targetW = qBound(280, qRound(width() * 0.70), std::min(580, maxAllowedW));

    // Calculate height needed to enclose the text when wrapped at targetW - 16
    QRect textBounds = fm.boundingRect(QRect(0, 0, targetW - 16, 10000),
                                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_text);
    int w = targetW;
    int h = std::max(36, textBounds.height() + 16);

    // Position box near initialPos, keeping top-left accessible
    int posX = initialPos.x() - 20;
    int posY = initialPos.y() - 15;
    m_boxRect = QRect(posX, posY, w, h);

    if (width() > w) {
        if (m_boxRect.left() < 10) m_boxRect.moveLeft(10);
        if (m_boxRect.right() > width() - 10) m_boxRect.moveRight(width() - 10);
    }
    if (height() > h) {
        if (m_boxRect.top() < 10) m_boxRect.moveTop(10);
        if (m_boxRect.bottom() > height() - 10) m_boxRect.moveBottom(height() - 10);
    }
    if (m_btnEdit) m_btnEdit->setVisible(true);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) m_btnApplyAndContinue->setVisible(true);
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::updateCachedPixmap(bool fast)
{
    if (m_image.isNull() || m_boxRect.width() <= 0 || m_boxRect.height() <= 0) {
        m_cachedScaledPixmap = QPixmap();
        m_lastCachedSize = QSize();
        return;
    }

    if (!fast && m_lastCachedSize == m_boxRect.size() && !m_cachedScaledPixmap.isNull()) {
        return;
    }

    Qt::TransformationMode mode = fast ? Qt::FastTransformation : Qt::SmoothTransformation;
    m_cachedScaledPixmap = QPixmap::fromImage(m_image.scaled(m_boxRect.size(), Qt::IgnoreAspectRatio, mode));
    m_lastCachedSize = m_boxRect.size();
}

void PdfTransformBox::showImage(const QImage &image, const QPoint &initialPos,
                                const QSize &defaultSize, PdfTransformItemType type)
{
    m_itemType = type;
    m_image = image;
    m_whiteout = false;

    int w = defaultSize.width();
    int h = defaultSize.height();
    if (!m_image.isNull() && m_image.width() > 0 && m_image.height() > 0) {
        double aspect = static_cast<double>(m_image.width()) / m_image.height();
        h = static_cast<int>(w / aspect);
    }

    m_boxRect = QRect(initialPos.x() - w / 2, initialPos.y() - h / 2, w, h);
    if (width() > w) {
        if (m_boxRect.left() < 10) m_boxRect.moveLeft(10);
        if (m_boxRect.right() > width() - 10) m_boxRect.moveRight(width() - 10);
    }
    if (height() > h) {
        if (m_boxRect.top() < 30) m_boxRect.moveTop(30);
        if (m_boxRect.bottom() > height() - 10) m_boxRect.moveBottom(height() - 10);
    }
    updateCachedPixmap(false);

    if (m_btnEdit) m_btnEdit->setVisible(false);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) m_btnApplyAndContinue->setVisible(true);
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::showWatermark(const QString &text, qreal angle, qreal opacity,
                                  const QColor &color, const QPoint &initialPos, int style)
{
    m_itemType = PdfTransformItemType::Watermark;
    m_text = text;
    m_angle = angle;
    m_opacity = opacity;
    m_color = color;
    m_watermarkStyle = style;
    m_whiteout = false;
    m_font = QFont(QStringLiteral("Sans Serif"), 36, QFont::Bold);

    int w = 360;
    int h = 140;
    m_boxRect = QRect(initialPos.x() - w / 2, initialPos.y() - h / 2, w, h);
    if (m_btnEdit) m_btnEdit->setVisible(true);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) m_btnApplyAndContinue->setVisible(true);
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::updateWatermark(const QString &text, qreal angle, qreal opacity, const QColor &color, int style)
{
    m_text = text;
    m_angle = angle;
    m_opacity = opacity;
    m_color = color;
    m_watermarkStyle = style;
    update();
}

void PdfTransformBox::setRedactionColor(const QColor &color)
{
    m_redactionColor = color;
    update();
}

void PdfTransformBox::drawWatermarkStamp(QPainter &painter, const QRect &rect,
                                         const QString &text, qreal angle,
                                         qreal opacity, const QColor &color,
                                         int style)
{
    if (rect.width() <= 0 || rect.height() <= 0 || text.isEmpty()) {
        return;
    }

    QFont baseFont(QStringLiteral("Sans Serif"), 36, QFont::Bold);
    QFontMetrics fm(baseFont);
    QRect textBounds = fm.boundingRect(text);

    qreal padH = 40.0;
    qreal padV = 24.0;
    qreal rawW = std::max(120.0, static_cast<double>(textBounds.width()) + padH);
    qreal rawH = std::max(50.0, static_cast<double>(textBounds.height()) + padV);

    qreal rad = qDegreesToRadians(angle);
    qreal cosVal = std::abs(std::cos(rad));
    qreal sinVal = std::abs(std::sin(rad));
    qreal rotW = rawW * cosVal + rawH * sinVal;
    qreal rotH = rawW * sinVal + rawH * cosVal;

    qreal availW = std::max(20.0, static_cast<double>(rect.width() - 16));
    qreal availH = std::max(20.0, static_cast<double>(rect.height() - 16));

    qreal scale = std::min(availW / rotW, availH / rotH);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.center());
    painter.rotate(angle);
    painter.scale(scale, scale);
    painter.setOpacity(opacity);
    painter.setFont(baseFont);
    painter.setPen(QPen(color, 3));

    QRectF stampRect(-rawW / 2.0, -rawH / 2.0, rawW, rawH);

    switch (style) {
        case 1: { // Official Double
            painter.drawRect(stampRect);
            painter.drawRect(stampRect.adjusted(5, 5, -5, -5));
            break;
        }
        case 2: { // Dotted
            QPen dotPen(color, 3, Qt::DotLine);
            painter.setPen(dotPen);
            painter.drawRoundedRect(stampRect, 8, 8);
            painter.setPen(QPen(color, 3));
            break;
        }
        case 3: { // Plain Text (no border)
            break;
        }
        case 0: // Classic Rounded
        default: {
            painter.drawRoundedRect(stampRect, 10, 10);
            break;
        }
    }
    painter.drawText(stampRect, Qt::AlignCenter, text);
    painter.restore();
}

void PdfTransformBox::showRedaction(const QRect &initialRect)
{
    m_itemType = PdfTransformItemType::Redaction;
    m_whiteout = false;
    m_boxRect = initialRect.normalized();
    if (m_boxRect.width() < 30) m_boxRect.setWidth(100);
    if (m_boxRect.height() < 20) m_boxRect.setHeight(40);

    if (m_btnEdit) m_btnEdit->setVisible(false);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(true);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(true);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(true);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(true);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) m_btnApplyAndContinue->setVisible(true);
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::showAreaSelection(const QRect &initialRect)
{
    m_itemType = PdfTransformItemType::AreaSelection;
    m_whiteout = false;
    m_boxRect = initialRect.normalized();
    if (m_boxRect.width() < 10) m_boxRect.setWidth(60);
    if (m_boxRect.height() < 10) m_boxRect.setHeight(40);

    if (m_btnEdit) m_btnEdit->setVisible(false);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(false);
    if (m_btnApplyAndContinue) m_btnApplyAndContinue->setVisible(false);
    if (m_btnCutArea) m_btnCutArea->setVisible(true);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(true);
    if (m_btnCancel) m_btnCancel->setVisible(true);

    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::dismiss()
{
    hide();
    m_activeHandle = None;
}

void PdfTransformBox::updateHandles()
{
    int half = HandleSize / 2;
    int left = m_boxRect.left();
    int right = m_boxRect.right();
    int top = m_boxRect.top();
    int bottom = m_boxRect.bottom();
    int midX = m_boxRect.center().x();
    int midY = m_boxRect.center().y();

    m_handles[0] = QRect(left - half, top - half, HandleSize, HandleSize);       // TopLeft
    m_handles[1] = QRect(midX - half, top - half, HandleSize, HandleSize);       // TopCenter
    m_handles[2] = QRect(right - half, top - half, HandleSize, HandleSize);      // TopRight
    m_handles[3] = QRect(right - half, midY - half, HandleSize, HandleSize);     // MidRight
    m_handles[4] = QRect(right - half, bottom - half, HandleSize, HandleSize);   // BottomRight
    m_handles[5] = QRect(midX - half, bottom - half, HandleSize, HandleSize);    // BottomCenter
    m_handles[6] = QRect(left - half, bottom - half, HandleSize, HandleSize);    // BottomLeft
    m_handles[7] = QRect(left - half, midY - half, HandleSize, HandleSize);      // MidLeft
}

QRect PdfTransformBox::dragHandleRect() const
{
    int hw = qBound(80, m_boxRect.width() / 2, 140);
    int hh = 20;
    int hx = m_boxRect.center().x() - hw / 2;
    int hy = m_boxRect.top() - hh - 2;
    return QRect(hx, hy, hw, hh);
}

void PdfTransformBox::repositionToolbar()
{
    if (!m_toolbar) return;
    m_toolbar->adjustSize();
    int tw = std::max(180, m_toolbar->sizeHint().width());
    int th = std::max(28, m_toolbar->sizeHint().height());

    // 1. Calculate visible viewport rectangle in PdfTransformBox coordinates
    QRect visibleRect = rect();
    QWidget *pw = parentWidget();
    if (pw) {
        QWidget *vp = pw->parentWidget(); // QScrollArea viewport
        if (vp && vp->isVisible()) {
            QPoint vpTopLeftInThis = mapFrom(vp, QPoint(0, 0));
            QRect vpInThis(vpTopLeftInThis, vp->size());
            visibleRect = visibleRect.intersected(vpInThis);
        }
    }

    int minX = visibleRect.isValid() ? visibleRect.left() + 6 : 6;
    int maxX = visibleRect.isValid() ? std::max(minX, visibleRect.right() - tw - 6) : std::max(6, width() - tw - 6);
    int minY = visibleRect.isValid() ? visibleRect.top() + 6 : 6;
    int maxY = visibleRect.isValid() ? std::max(minY, visibleRect.bottom() - th - 6) : std::max(6, height() - th - 6);

    // 2. Horizontal placement: centered on m_boxRect, clamped within visible screen
    int tx = qBound(minX, m_boxRect.center().x() - tw / 2, maxX);

    // 3. Vertical placement: prefer placing above m_boxRect
    int ty = m_boxRect.top() - th - 12;

    // If above m_boxRect is outside visible screen, try below m_boxRect
    if (ty < minY) {
        ty = m_boxRect.bottom() + 10;
    }

    // If below m_boxRect also falls outside visible screen (e.g. box is huge),
    // pin toolbar directly inside the visible viewport so the user ALWAYS sees it!
    if (ty > maxY || ty < minY) {
        ty = maxY - 6; // Pin near bottom of visible area
    }

    m_toolbar->setGeometry(tx, ty, tw, th);
}

PdfTransformBox::Handle PdfTransformBox::handleAt(const QPoint &pos) const
{
    for (int i = 0; i < 8; ++i) {
        if (m_handles[i].contains(pos)) {
            return static_cast<Handle>(i + 1);
        }
    }
    if (dragHandleRect().contains(pos) || m_boxRect.contains(pos)) {
        return Body;
    }
    return None;
}

void PdfTransformBox::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 1. Render item content preview inside m_boxRect
    if (m_itemType == PdfTransformItemType::Text) {
        if (m_whiteout) {
            painter.fillRect(m_boxRect, Qt::white);
        }
        painter.setFont(m_font);
        painter.setPen(m_color);
        // Align Top-Left exactly as Poppler renders native vector TextAnnotation
        painter.drawText(m_boxRect.adjusted(4, 4, -4, -4),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                         m_text);
    } else if (m_itemType == PdfTransformItemType::Signature || m_itemType == PdfTransformItemType::Image) {
        if (!m_cachedScaledPixmap.isNull()) {
            painter.drawPixmap(m_boxRect.topLeft(), m_cachedScaledPixmap);
        } else if (!m_image.isNull()) {
            painter.drawImage(m_boxRect, m_image);
        }
    } else if (m_itemType == PdfTransformItemType::Watermark) {
        drawWatermarkStamp(painter, m_boxRect, m_text, m_angle, m_opacity, m_color, m_watermarkStyle);
    } else if (m_itemType == PdfTransformItemType::Redaction) {
        painter.fillRect(m_boxRect, m_redactionColor);
        if (m_redactionColor.lightness() < 128) {
            painter.setPen(QColor(248, 113, 113));
        } else {
            painter.setPen(QColor(185, 28, 28));
        }
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 9, QFont::Bold));
        painter.drawText(m_boxRect, Qt::AlignCenter, tr("[REDACTED]"));
    } else if (m_itemType == PdfTransformItemType::AreaSelection) {
        painter.fillRect(m_boxRect, QColor(14, 165, 233, 20));
    }

    // 2. Render Selection Outline (Cyan DashLine)
    QPen borderPen(QColor(56, 189, 248), 1.5, Qt::DashLine);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_boxRect);

    // 2b. Render Top Drag Handle Tab ("⠿ Move")
    QRect handleR = dragHandleRect();
    painter.setPen(QPen(QColor(56, 189, 248), 1.2));
    painter.setBrush(QColor(15, 23, 42, 230));
    painter.drawRoundedRect(handleR, 4, 4);
    painter.setPen(QColor(226, 232, 240));
    painter.setFont(QFont("sans-serif", 8, QFont::Bold));
    painter.drawText(handleR, Qt::AlignCenter, tr("⠿ Move"));

    // 3. Render 8 Control Handles
    painter.setPen(QPen(QColor(56, 189, 248), 1.5));
    painter.setBrush(QColor(20, 26, 40));
    for (int i = 0; i < 8; ++i) {
        painter.drawRect(m_handles[i]);
    }
}

void PdfTransformBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_activeHandle = handleAt(event->pos());
        if (m_activeHandle != None) {
            m_dragStartPos = event->pos();
            m_dragStartRect = m_boxRect;
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void PdfTransformBox::mouseMoveEvent(QMouseEvent *event)
{
    if (m_activeHandle == None) {
        Handle h = handleAt(event->pos());
        switch (h) {
            case TopLeft:
            case BottomRight:
                setCursor(Qt::SizeFDiagCursor);
                break;
            case TopRight:
            case BottomLeft:
                setCursor(Qt::SizeBDiagCursor);
                break;
            case TopCenter:
            case BottomCenter:
                setCursor(Qt::SizeVerCursor);
                break;
            case MidLeft:
            case MidRight:
                setCursor(Qt::SizeHorCursor);
                break;
            case Body:
                setCursor(Qt::SizeAllCursor);
                break;
            default:
                setCursor(Qt::ArrowCursor);
                break;
        }
        return;
    }

    QPoint delta = event->pos() - m_dragStartPos;
    QRect newRect = m_dragStartRect;

    switch (m_activeHandle) {
        case Body:
            newRect.moveTopLeft(m_dragStartRect.topLeft() + delta);
            break;
        case TopLeft:
            newRect.setTopLeft(m_dragStartRect.topLeft() + delta);
            break;
        case TopCenter:
            newRect.setTop(m_dragStartRect.top() + delta.y());
            break;
        case TopRight:
            newRect.setTopRight(m_dragStartRect.topRight() + delta);
            break;
        case MidRight:
            newRect.setRight(m_dragStartRect.right() + delta.x());
            break;
        case BottomRight:
            newRect.setBottomRight(m_dragStartRect.bottomRight() + delta);
            break;
        case BottomCenter:
            newRect.setBottom(m_dragStartRect.bottom() + delta.y());
            break;
        case BottomLeft:
            newRect.setBottomLeft(m_dragStartRect.bottomLeft() + delta);
            break;
        case MidLeft:
            newRect.setLeft(m_dragStartRect.left() + delta.x());
            break;
        default:
            break;
    }

    // Minimum size enforcement
    if (newRect.width() >= 40 && newRect.height() >= 25) {
        bool sizeChanged = (m_boxRect.size() != newRect.size());
        m_boxRect = newRect;
        if (sizeChanged && (m_itemType == PdfTransformItemType::Image || m_itemType == PdfTransformItemType::Signature)) {
            m_wasResizing = true;
            updateCachedPixmap(true);
        }
        updateHandles();
        repositionToolbar();
        update();
    }
    event->accept();
}

void PdfTransformBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activeHandle != None) {
        m_activeHandle = None;
        if (m_wasResizing) {
            m_wasResizing = false;
            updateCachedPixmap(false);
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PdfTransformBox::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Undo)) {
        // Pressing Undo while a transform box is active cancels the uncommitted tool
        emit cancelled();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        // Redo is inactive while an uncommitted placement is in progress
        event->accept();
        return;
    }

    if (m_itemType == PdfTransformItemType::AreaSelection) {
        if (event->matches(QKeySequence::Cut)) {
            emit cutAreaRequested();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Copy)) {
            emit copyAreaRequested();
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_itemType == PdfTransformItemType::AreaSelection) {
            emit cutAreaRequested();
        } else {
            emit applied();
        }
        event->accept();
    } else if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PdfTransformBox::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}
