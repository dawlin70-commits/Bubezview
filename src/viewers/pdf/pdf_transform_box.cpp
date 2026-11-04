/**
 * @file pdf_transform_box.cpp
 * @brief Implementation of interactive 8-handle transformation overlay for PDF items.
 */

#include "pdf_transform_box.h"
#include "pdf_canvas_tools.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QColorDialog>
#include <QLabel>
#include <QTextEdit>
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
        "QToolTip { background-color: #1E293B; color: #F8FAFC; border: 1px solid #38BDF8; border-radius: 4px; padding: 4px 6px; font-size: 11px; }"
        "QPushButton { background: transparent; border: none; font-size: 11px; font-weight: 700; padding: 4px 8px; color: #F1F5F9; }"
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
    tbLayout->setSpacing(3);

    m_toolbar->installEventFilter(this);
    QLabel *lblMoveGrip = new QLabel(QStringLiteral("⠿"), m_toolbar);
    lblMoveGrip->setObjectName(QStringLiteral("GripMove"));
    lblMoveGrip->setCursor(Qt::SizeAllCursor);
    lblMoveGrip->setToolTip(tr("Drag to move"));
    lblMoveGrip->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 13px; font-weight: bold; padding: 0 4px;"));
    tbLayout->addWidget(lblMoveGrip);

    m_btnEdit = new QPushButton(tr("✏️ Edit"), m_toolbar);
    m_btnEdit->setObjectName(QStringLiteral("BtnEdit"));
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    connect(m_btnEdit, &QPushButton::clicked, this, &PdfTransformBox::editRequested);

    m_btnRedactBlack = new QPushButton(m_toolbar);
    m_btnRedactBlack->setObjectName(QStringLiteral("BtnRedactBlack"));
    m_btnRedactBlack->setFixedSize(18, 18);
    m_btnRedactBlack->setCursor(Qt::PointingHandCursor);
    m_btnRedactBlack->setToolTip(tr("Black Redaction"));
    m_btnRedactBlack->setVisible(false);
    connect(m_btnRedactBlack, &QPushButton::clicked, this, [this]() { setRedactionColor(Qt::black); });

    m_btnRedactWhite = new QPushButton(m_toolbar);
    m_btnRedactWhite->setObjectName(QStringLiteral("BtnRedactWhite"));
    m_btnRedactWhite->setFixedSize(18, 18);
    m_btnRedactWhite->setCursor(Qt::PointingHandCursor);
    m_btnRedactWhite->setToolTip(tr("White Redaction"));
    m_btnRedactWhite->setVisible(false);
    connect(m_btnRedactWhite, &QPushButton::clicked, this, [this]() { setRedactionColor(Qt::white); });

    m_btnRedactGray = new QPushButton(m_toolbar);
    m_btnRedactGray->setObjectName(QStringLiteral("BtnRedactGray"));
    m_btnRedactGray->setFixedSize(18, 18);
    m_btnRedactGray->setCursor(Qt::PointingHandCursor);
    m_btnRedactGray->setToolTip(tr("Gray Redaction"));
    m_btnRedactGray->setVisible(false);
    connect(m_btnRedactGray, &QPushButton::clicked, this, [this]() { setRedactionColor(QColor(128, 128, 128)); });

    m_btnRedactCustom = new QPushButton(QStringLiteral("🎨"), m_toolbar);
    m_btnRedactCustom->setObjectName(QStringLiteral("BtnRedactCustom"));
    m_btnRedactCustom->setFixedSize(20, 20);
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

    // Inline canvas text editor
    m_inlineTextEdit = new QTextEdit(this);
    m_inlineTextEdit->setFrameShape(QFrame::NoFrame);
    m_inlineTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inlineTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inlineTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_inlineTextEdit->setAcceptRichText(false);
    m_inlineTextEdit->document()->setDocumentMargin(2);
    m_inlineTextEdit->installEventFilter(this);
    m_inlineTextEdit->hide();

    connect(m_inlineTextEdit, &QTextEdit::textChanged, this, [this]() {
        m_text = m_inlineTextEdit->toPlainText();
        m_isTextModified = true;
        if (!m_text.contains('\n')) {
            int textW = QFontMetrics(screenFont()).horizontalAdvance(m_text);
            if (textW + 36 > m_boxRect.width()) {
                m_boxRect.setWidth(textW + 36);
            }
        }
        adjustHeightToFitText();
    });

    updateRedactionColorButtons();
    updateHandles();
}

PdfTransformBox::~PdfTransformBox() = default;

QString PdfTransformBox::text() const
{
    if (m_inlineTextEdit && m_inlineTextEdit->isVisible() && m_itemType == PdfTransformItemType::Text) {
        return m_inlineTextEdit->toPlainText();
    }
    return m_text;
}

void PdfTransformBox::setPlacementMode(bool placementOnly, const QPoint &centerOnPoint,
                                      int suggestedWidth, const QPoint &topLeftPos)
{
    m_isPlacementMode = placementOnly;
    if (placementOnly && m_itemType == PdfTransformItemType::Text) {
        if (m_inlineTextEdit) {
            m_inlineTextEdit->hide();
        }
        if (m_btnEdit) {
            m_btnEdit->setText(tr("✏️ Edit"));
            m_btnEdit->setVisible(true);
        }

        // Fit box to the actual rendered text with a comfortable margin buffer
        QFontMetrics fm(screenFont());
        QRect textBound = fm.boundingRect(QRect(0, 0, 2400, 2400), m_textAlignment | Qt::TextWordWrap, m_text);

        // Measure maximum line advance
        int maxLineW = 0;
        const QStringList lines = m_text.split('\n');
        for (const QString &line : lines) {
            maxLineW = std::max(maxLineW, fm.horizontalAdvance(line));
        }

        // Provide a comfortable margin buffer so font metric variations
        // between Qt and Poppler font engines never cause premature word wrapping.
        int naturalW = std::max({100, maxLineW + 28, textBound.width() + 16});
        int w = std::max(naturalW, suggestedWidth);
        int h = std::max({20, textBound.height() + 8, static_cast<int>(lines.size() * fm.lineSpacing() + 6)});

        if (!topLeftPos.isNull()) {
            m_boxRect = QRect(topLeftPos.x(), topLeftPos.y(), w, h);
        } else {
            QPoint center = centerOnPoint.isNull() ? m_boxRect.center() : centerOnPoint;
            m_boxRect = QRect(center.x() - w / 2, center.y() - h / 2, w, h);
        }

        if (width() > m_boxRect.width()) {
            if (m_boxRect.left() < 6) m_boxRect.moveLeft(6);
            if (m_boxRect.right() > width() - 6) m_boxRect.moveRight(width() - 6);
        }
        if (height() > m_boxRect.height()) {
            if (m_boxRect.top() < 6) m_boxRect.moveTop(6);
            if (m_boxRect.bottom() > height() - 6) m_boxRect.moveBottom(height() - 6);
        }

        updateHandles();
        repositionToolbar();
        setFocus();
        update();
        emit geometryChanged(m_boxRect);
    } else if (!placementOnly && m_itemType == PdfTransformItemType::Text) {
        if (m_inlineTextEdit) {
            m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
            m_inlineTextEdit->show();
            m_inlineTextEdit->setFocus();
        }
        if (m_btnEdit) {
            m_btnEdit->setVisible(false);
        }
        update();
    }
}

void PdfTransformBox::setHasCollision(bool collision)
{
    if (m_hasCollision == collision) return;
    m_hasCollision = collision;
    if (m_btnApply) {
        if (collision) {
            m_btnApply->setStyleSheet(QStringLiteral(
                "QPushButton#BtnApply { background-color: #991B1B; color: #FCA5A5; border: 1px solid #EF4444; }"));
            m_btnApply->setToolTip(tr("⚠️ Cannot apply: box borders overlap existing text on the page"));
        } else {
            m_btnApply->setStyleSheet(QString());
            m_btnApply->setToolTip(QString());
        }
    }
    update();
}

void PdfTransformBox::setTextBgMode(PdfTextBgMode mode)
{
    m_textBgMode = mode;
    m_whiteout = (mode != PdfTextBgMode::Transparent);
    updateInlineTextEditStyle();
    update();
}

void PdfTransformBox::setTextBgColor(const QColor &color)
{
    m_textBgColor = color;
    updateInlineTextEditStyle();
    update();
}

void PdfTransformBox::updateInlineTextEditStyle()
{
    if (!m_inlineTextEdit) return;
    QSignalBlocker blocker(m_inlineTextEdit);
    QSignalBlocker docBlocker(m_inlineTextEdit->document());
    m_inlineTextEdit->setFont(screenFont());
    m_inlineTextEdit->setAlignment(m_textAlignment);

    QString bg;
    if (m_textBgMode == PdfTextBgMode::Transparent) {
        bg = QStringLiteral("transparent");
    } else {
        bg = m_textBgColor.isValid() ? m_textBgColor.name(QColor::HexRgb) : QStringLiteral("#ffffff");
    }
    QString colorHex = m_color.isValid() ? m_color.name(QColor::HexRgb) : QStringLiteral("#000000");

    m_inlineTextEdit->setStyleSheet(QString(
        "QTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px dashed rgba(56, 189, 248, 0.7);"
        "  padding: 0px;"
        "  selection-background-color: #38bdf8;"
        "  selection-color: #ffffff;"
        "}"
    ).arg(bg, colorHex));
}

void PdfTransformBox::adjustHeightToFitText()
{
    if (m_itemType != PdfTransformItemType::Text || !m_inlineTextEdit) return;

    QFontMetrics fm(screenFont());
    int lineSpacing = fm.lineSpacing();

    int innerW = std::max(60, m_boxRect.width() - 8);
    m_inlineTextEdit->document()->setTextWidth(innerW);
    int docH = static_cast<int>(std::ceil(m_inlineTextEdit->document()->size().height()));

    int targetH;
    if (!m_text.contains('\n') && docH <= lineSpacing * 1.5) {
        // Pristine single line
        targetH = std::max({lineSpacing + 6, 22, m_minBoxHeight});
    } else {
        // Multi-line content or wrapped column paragraph
        int minH = static_cast<int>(std::ceil(lineSpacing * 1.2)) + 12;
        targetH = std::max({minH, docH + 8, m_minBoxHeight});
    }

    if (m_boxRect.height() != targetH) {
        m_boxRect.setHeight(targetH);
        m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
        updateHandles();
        repositionToolbar();
        update();
    } else {
        m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
    }
}

bool PdfTransformBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_toolbar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_activeHandle = Body;
                m_dragStartPos = mapFromGlobal(me->globalPosition().toPoint());
                m_dragStartRect = m_boxRect;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_activeHandle == Body) {
            auto *me = static_cast<QMouseEvent *>(event);
            QPoint posInThis = mapFromGlobal(me->globalPosition().toPoint());
            QPoint delta = posInThis - m_dragStartPos;
            QRect newRect = m_dragStartRect;
            newRect.moveTopLeft(m_dragStartRect.topLeft() + delta);
            if (newRect.width() >= 24 && newRect.height() >= 16) {
                m_boxRect = newRect;
                if (m_itemType == PdfTransformItemType::Text && m_inlineTextEdit) {
                    m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
                }
                updateHandles();
                repositionToolbar();
                update();
                emit geometryChanged(m_boxRect);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_activeHandle == Body) {
            m_activeHandle = None;
            return true;
        }
    }

    if (watched == m_inlineTextEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            emit cancelled();
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            (ke->modifiers() & Qt::ControlModifier)) {
            emit applied();
            return true;
        }
        // Pixel-perfect nudge with arrow keys if Alt or Ctrl is held
        if (ke->modifiers() & (Qt::AltModifier | Qt::ControlModifier)) {
            int step = (ke->modifiers() & Qt::ShiftModifier) ? 5 : 1;
            int dx = 0;
            int dy = 0;
            if (ke->key() == Qt::Key_Left) dx = -step;
            else if (ke->key() == Qt::Key_Right) dx = step;
            else if (ke->key() == Qt::Key_Up) dy = -step;
            else if (ke->key() == Qt::Key_Down) dy = step;

            if (dx != 0 || dy != 0) {
                m_boxRect.translate(dx, dy);
                m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
                updateHandles();
                repositionToolbar();
                update();
                emit geometryChanged(m_boxRect);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PdfTransformBox::setBoxRect(const QRect &rect)
{
    m_boxRect = rect;
    if (m_inlineTextEdit) {
        m_inlineTextEdit->setGeometry(m_boxRect);
    }
    updateHandles();
    repositionToolbar();
    update();
    emit geometryChanged(m_boxRect);
}

void PdfTransformBox::updateTextContent(const QString &text)
{
    m_text = text;
    if (m_inlineTextEdit && m_inlineTextEdit->toPlainText() != text) {
        QSignalBlocker blocker(m_inlineTextEdit);
        m_inlineTextEdit->setPlainText(text);
    }
    if (!m_text.contains('\n')) {
        int textW = QFontMetrics(screenFont()).horizontalAdvance(m_text);
        if (textW + 36 > m_boxRect.width()) {
            m_boxRect.setWidth(textW + 36);
        }
    }
    adjustHeightToFitText();
    update();
}

void PdfTransformBox::setScaleFactor(double scaleFactor)
{
    if (scaleFactor <= 0.01 || qFuzzyCompare(m_scaleFactor, scaleFactor)) return;
    m_scaleFactor = scaleFactor;
    updateInlineTextEditStyle();
    if (m_itemType == PdfTransformItemType::Text) {
        if (m_isPlacementMode) {
            setPlacementMode(true, m_boxRect.center(), m_boxRect.width());
        } else {
            adjustHeightToFitText();
        }
    }
    update();
}

QFont PdfTransformBox::screenFont() const
{
    QFont sf = m_font;
    qreal pt = sf.pointSizeF();
    if (pt <= 0.0) pt = sf.pointSize();
    if (pt <= 0.0) {
        int px = sf.pixelSize();
        pt = (px > 0) ? px : 12.0;
    }

    double scale = (m_scaleFactor > 0.01) ? m_scaleFactor : 1.0;
    int px = std::max(6, qRound(pt * scale));
    sf.setPixelSize(px);
    return sf;
}

void PdfTransformBox::updateTextFont(const QFont &font)
{
    m_font = font;
    updateInlineTextEditStyle();
    if (m_isPlacementMode) {
        setPlacementMode(true, m_boxRect.center(), m_boxRect.width());
        emit geometryChanged(m_boxRect);
    } else {
        adjustHeightToFitText();
    }
}

void PdfTransformBox::updateTextColor(const QColor &color)
{
    m_color = color;
    updateInlineTextEditStyle();
    update();
}

void PdfTransformBox::updateTextWhiteout(bool whiteout)
{
    m_whiteout = whiteout;
    updateInlineTextEditStyle();
    update();
}

void PdfTransformBox::updateTextAlignment(Qt::Alignment alignment)
{
    m_textAlignment = alignment;
    updateInlineTextEditStyle();
    update();
}

void PdfTransformBox::showText(const QString &text, const QFont &font, const QColor &textColor,
                              PdfTextBgMode bgMode, const QColor &bgColor,
                              const QRect &initialRect, const QPoint &initialPos)
{
    m_itemType = PdfTransformItemType::Text;
    m_text = text;
    m_font = font;
    m_color = textColor;
    m_textBgMode = bgMode;
    m_textBgColor = bgColor.isValid() ? bgColor : Qt::white;
    m_whiteout = (bgMode != PdfTextBgMode::Transparent);
    m_isTextModified = false;
    m_isPlacementMode = false;
    setHasCollision(false);

    QFontMetrics fm(screenFont());
    int textW = fm.horizontalAdvance(text);
    int lineH = fm.lineSpacing();

    if (initialRect.isValid() && initialRect.width() > 0 && initialRect.height() > 0) {
        // Position directly over the existing text with generous width margin to prevent premature wrap
        int targetW = std::max(initialRect.width() + 36, textW + 40);
        int targetH = std::max(initialRect.height() + 4, lineH + 6);
        m_minBoxHeight = targetH;
        m_boxRect = QRect(initialRect.left() - 4, initialRect.top() - 2, targetW, targetH);
    } else {
        m_minBoxHeight = 0;
        // Target a natural readable column width: ~60% of available page width, bounded [200, 520]
        int minW = 200;
        int maxAllowedW = std::max(minW, width() - 40);
        int targetW = qBound(minW, qRound(width() * 0.60), std::min(520, maxAllowedW));

        int posX = initialPos.x() - 20;
        int posY = initialPos.y() - 15;
        m_boxRect = QRect(posX, posY, targetW, 50);
    }

    if (width() > m_boxRect.width()) {
        if (m_boxRect.left() < 10) m_boxRect.moveLeft(10);
        if (m_boxRect.right() > width() - 10) m_boxRect.moveRight(width() - 10);
    }
    if (height() > m_boxRect.height()) {
        if (m_boxRect.top() < 10) m_boxRect.moveTop(10);
        if (m_boxRect.bottom() > height() - 10) m_boxRect.moveBottom(height() - 10);
    }

    if (m_inlineTextEdit) {
        {
            QSignalBlocker blocker(m_inlineTextEdit);
            QSignalBlocker docBlocker(m_inlineTextEdit->document());
            m_inlineTextEdit->setPlainText(m_text);
        }
        updateInlineTextEditStyle();
        m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
        m_inlineTextEdit->show();
    }

    adjustHeightToFitText();

    if (m_btnEdit) m_btnEdit->setVisible(false);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) {
        m_btnApplyAndContinue->setText(tr("🔄 Stamp Again"));
        m_btnApplyAndContinue->setToolTip(tr("Apply current item and place another copy ready on canvas"));
        m_btnApplyAndContinue->setVisible(true);
    }
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateHandles();
    repositionToolbar();
    show();
    raise();

    if (m_inlineTextEdit) {
        m_inlineTextEdit->setFocus();
        m_inlineTextEdit->selectAll();
    } else {
        setFocus();
    }
    update();
}

void PdfTransformBox::showText(const QString &text, const QFont &font, const QColor &textColor,
                              bool whiteout, const QPoint &initialPos)
{
    showText(text, font, textColor, whiteout ? PdfTextBgMode::Auto : PdfTextBgMode::Transparent,
             Qt::white, QRect(), initialPos);
}

void PdfTransformBox::showShape(int shapeType, int strokeWidth, const QColor &strokeColor,
                               const QColor &fillColor, const QRect &initialRect,
                               const QPoint &localStart, const QPoint &localEnd)
{
    m_itemType = PdfTransformItemType::Shape;
    m_shapeType = shapeType;
    m_shapeStrokeWidth = std::max(1, strokeWidth);
    m_shapeStrokeColor = strokeColor.isValid() ? strokeColor : QColor(QStringLiteral("#ef4444"));
    m_shapeFillColor = fillColor;
    m_shapeLocalStart = localStart;
    m_shapeLocalEnd = localEnd;
    m_whiteout = false;

    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }

    QRect r = initialRect;
    if (r.width() < 30) r.setWidth(30);
    if (r.height() < 30) r.setHeight(30);
    m_boxRect = r;

    if (width() > m_boxRect.width()) {
        if (m_boxRect.left() < 10) m_boxRect.moveLeft(10);
        if (m_boxRect.right() > width() - 10) m_boxRect.moveRight(width() - 10);
    }
    if (height() > m_boxRect.height()) {
        if (m_boxRect.top() < 10) m_boxRect.moveTop(10);
        if (m_boxRect.bottom() > height() - 10) m_boxRect.moveBottom(height() - 10);
    }

    if (m_btnEdit) m_btnEdit->setVisible(false);
    if (m_btnRedactBlack) m_btnRedactBlack->setVisible(false);
    if (m_btnRedactWhite) m_btnRedactWhite->setVisible(false);
    if (m_btnRedactGray) m_btnRedactGray->setVisible(false);
    if (m_btnRedactCustom) m_btnRedactCustom->setVisible(false);
    if (m_btnApply) m_btnApply->setVisible(true);
    if (m_btnApplyAndContinue) {
        m_btnApplyAndContinue->setText(tr("🔄 Draw Another"));
        m_btnApplyAndContinue->setToolTip(tr("Commit shape and keep tool active for another"));
        m_btnApplyAndContinue->setVisible(true);
    }
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);

    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::updateShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    m_shapeType = shapeType;
    m_shapeStrokeWidth = std::max(1, strokeWidth);
    m_shapeStrokeColor = strokeColor;
    m_shapeFillColor = fillColor;
    update();
}

void PdfTransformBox::setShapeType(int shapeType)
{
    m_shapeType = shapeType;
    update();
}

void PdfTransformBox::setShapeStrokeWidth(int width)
{
    m_shapeStrokeWidth = std::max(1, width);
    update();
}

void PdfTransformBox::setShapeStrokeColor(const QColor &color)
{
    m_shapeStrokeColor = color;
    update();
}

void PdfTransformBox::setShapeFillColor(const QColor &color)
{
    m_shapeFillColor = color;
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
    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }
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
    if (m_btnApplyAndContinue) {
        m_btnApplyAndContinue->setText(tr("🔄 Stamp Again"));
        m_btnApplyAndContinue->setToolTip(tr("Apply current item and place another copy ready on canvas"));
        m_btnApplyAndContinue->setVisible(true);
    }
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
    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }
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
    if (m_btnApplyAndContinue) {
        m_btnApplyAndContinue->setText(tr("🔄 Stamp Again"));
        m_btnApplyAndContinue->setToolTip(tr("Apply current item and place another copy ready on canvas"));
        m_btnApplyAndContinue->setVisible(true);
    }
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
    updateRedactionColorButtons();
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
        case 4: { // Solid Rectangle
            painter.drawRect(stampRect);
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
    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }
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
    if (m_btnApplyAndContinue) {
        m_btnApplyAndContinue->setText(tr("🔄 Apply & Continue"));
        m_btnApplyAndContinue->setToolTip(tr("Apply current redaction and keep tool active for another area"));
        m_btnApplyAndContinue->setVisible(true);
    }
    if (m_btnCutArea) m_btnCutArea->setVisible(false);
    if (m_btnCopyArea) m_btnCopyArea->setVisible(false);
    updateRedactionColorButtons();
    updateHandles();
    repositionToolbar();
    show();
    raise();
    setFocus();
    update();
}

void PdfTransformBox::showAreaSelection(const QRect &initialRect)
{
    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }
    m_itemType = PdfTransformItemType::AreaSelection;
    m_whiteout = false;
    m_isPlacementMode = false;
    setHasCollision(false);
    m_boxRect = initialRect.normalized();
    if (m_boxRect.width() < 10) m_boxRect.setWidth(60);
    if (m_boxRect.height() < 10) m_boxRect.setHeight(40);

    if (m_btnEdit) {
        m_btnEdit->setText(tr("✏️ Replace Text"));
        m_btnEdit->setVisible(true);
    }
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
    if (m_inlineTextEdit) {
        m_inlineTextEdit->hide();
    }
    m_isPlacementMode = false;
    setHasCollision(false);
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

void PdfTransformBox::updateRedactionColorButtons()
{
    if (!m_btnRedactBlack || !m_btnRedactWhite || !m_btnRedactGray || !m_btnRedactCustom) {
        return;
    }

    bool isBlack = (m_redactionColor == Qt::black);
    bool isWhite = (m_redactionColor == Qt::white);
    bool isGray = (m_redactionColor == QColor(128, 128, 128));
    bool isCustom = (!isBlack && !isWhite && !isGray);

    m_btnRedactBlack->setStyleSheet(isBlack
        ? QStringLiteral("QPushButton#BtnRedactBlack { background-color: #000000; border: 2px solid #38BDF8; border-radius: 9px; padding: 0px; }")
        : QStringLiteral("QPushButton#BtnRedactBlack { background-color: #000000; border: 1px solid #64748B; border-radius: 9px; padding: 0px; } QPushButton#BtnRedactBlack:hover { border: 1.5px solid #94A3B8; }"));

    m_btnRedactWhite->setStyleSheet(isWhite
        ? QStringLiteral("QPushButton#BtnRedactWhite { background-color: #FFFFFF; border: 2px solid #38BDF8; border-radius: 9px; padding: 0px; }")
        : QStringLiteral("QPushButton#BtnRedactWhite { background-color: #FFFFFF; border: 1px solid #64748B; border-radius: 9px; padding: 0px; } QPushButton#BtnRedactWhite:hover { border: 1.5px solid #94A3B8; }"));

    m_btnRedactGray->setStyleSheet(isGray
        ? QStringLiteral("QPushButton#BtnRedactGray { background-color: #808080; border: 2px solid #38BDF8; border-radius: 9px; padding: 0px; }")
        : QStringLiteral("QPushButton#BtnRedactGray { background-color: #808080; border: 1px solid #64748B; border-radius: 9px; padding: 0px; } QPushButton#BtnRedactGray:hover { border: 1.5px solid #94A3B8; }"));

    if (isCustom) {
        QString textColor = (m_redactionColor.lightness() < 128) ? QStringLiteral("#FFFFFF") : QStringLiteral("#000000");
        m_btnRedactCustom->setStyleSheet(QString(
            "QPushButton#BtnRedactCustom { background-color: %1; color: %2; border: 2px solid #38BDF8; border-radius: 10px; font-size: 11px; padding: 0px; }")
            .arg(m_redactionColor.name(), textColor));
    } else {
        m_btnRedactCustom->setStyleSheet(QStringLiteral(
            "QPushButton#BtnRedactCustom { background-color: #1E293B; color: #F1F5F9; border: 1px solid #64748B; border-radius: 10px; font-size: 11px; padding: 0px; } QPushButton#BtnRedactCustom:hover { border: 1.5px solid #94A3B8; }"));
    }
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

    // 3. Vertical placement: prefer placing above m_boxRect with comfortable clearance
    int ty = m_boxRect.top() - th - 10;

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
    if (dragHandleRect().contains(pos) || m_boxRect.adjusted(-6, -6, 6, 6).contains(pos)) {
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
        if (m_textBgMode != PdfTextBgMode::Transparent) {
            if (m_textBgMode == PdfTextBgMode::Feathered) {
                QImage patch = PdfCanvasTools::createFeatheredPatch(m_boxRect.size(), m_textBgColor);
                painter.drawImage(m_boxRect.topLeft(), patch);
            } else {
                painter.fillRect(m_boxRect, m_textBgColor);
            }
        }
        if (!m_inlineTextEdit || !m_inlineTextEdit->isVisible()) {
            painter.setFont(screenFont());
            painter.setPen(m_color);
            painter.drawText(m_boxRect.adjusted(3, 2, -3, -2),
                             m_textAlignment | Qt::TextWordWrap,
                             m_text);
        }
    } else if (m_itemType == PdfTransformItemType::Signature || m_itemType == PdfTransformItemType::Image) {
        if (!m_cachedScaledPixmap.isNull()) {
            painter.drawPixmap(m_boxRect.topLeft(), m_cachedScaledPixmap);
        } else if (!m_image.isNull()) {
            painter.drawImage(m_boxRect, m_image);
        }
    } else if (m_itemType == PdfTransformItemType::Watermark) {
        drawWatermarkStamp(painter, m_boxRect, m_text, m_angle, m_opacity, m_color, m_watermarkStyle);
    } else if (m_itemType == PdfTransformItemType::Shape) {
        painter.save();
        painter.translate(m_boxRect.topLeft());
        int halfW = std::max(1, m_shapeStrokeWidth / 2);
        QRect localR(halfW, halfW, std::max(2, m_boxRect.width() - m_shapeStrokeWidth), std::max(2, m_boxRect.height() - m_shapeStrokeWidth));

        QPen pen(m_shapeStrokeColor, m_shapeStrokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        if (m_shapeFillColor.isValid() && m_shapeFillColor.alpha() > 0) {
            painter.setBrush(m_shapeFillColor);
        } else {
            painter.setBrush(Qt::NoBrush);
        }

        if (m_shapeType == 0) { // Rectangle
            painter.drawRect(localR);
        } else if (m_shapeType == 1) { // Ellipse / Circle
            painter.drawEllipse(localR);
        } else if (m_shapeType == 2) { // Arrow
            QPoint p1 = (m_shapeLocalStart != QPoint() && m_shapeLocalStart != QPoint(-1, -1))
                            ? m_shapeLocalStart
                            : QPoint(halfW, halfW);
            QPoint p2 = (m_shapeLocalEnd != QPoint() && m_shapeLocalEnd != QPoint(-1, -1))
                            ? m_shapeLocalEnd
                            : QPoint(localR.right(), localR.bottom());
            painter.drawLine(p1, p2);

            double angle = std::atan2(static_cast<double>(p2.y() - p1.y()),
                                      static_cast<double>(p2.x() - p1.x()));
            double arrowSize = std::max(10.0, static_cast<double>(m_shapeStrokeWidth) * 3.0);
            QPointF head1 = QPointF(p2) - QPointF(arrowSize * std::cos(angle - M_PI / 6.0),
                                                  arrowSize * std::sin(angle - M_PI / 6.0));
            QPointF head2 = QPointF(p2) - QPointF(arrowSize * std::cos(angle + M_PI / 6.0),
                                                  arrowSize * std::sin(angle + M_PI / 6.0));
            QPolygonF arrowHead;
            arrowHead << QPointF(p2) << head1 << head2;
            painter.setBrush(m_shapeStrokeColor);
            painter.drawPolygon(arrowHead);
        } else if (m_shapeType == 3) { // Straight Line
            QPoint p1 = (m_shapeLocalStart != QPoint() && m_shapeLocalStart != QPoint(-1, -1))
                            ? m_shapeLocalStart
                            : QPoint(halfW, halfW);
            QPoint p2 = (m_shapeLocalEnd != QPoint() && m_shapeLocalEnd != QPoint(-1, -1))
                            ? m_shapeLocalEnd
                            : QPoint(localR.right(), localR.bottom());
            painter.drawLine(p1, p2);
        } else if (m_shapeType == 4) { // Rounded Rectangle
            painter.drawRoundedRect(localR, 8, 8);
        }
        painter.restore();
    } else if (m_itemType == PdfTransformItemType::Redaction) {
        painter.fillRect(m_boxRect, m_redactionColor);
        if (m_boxRect.width() >= 90 && m_boxRect.height() >= 24) {
            if (m_redactionColor.lightness() < 128) {
                painter.setPen(QColor(248, 113, 113));
            } else {
                painter.setPen(QColor(185, 28, 28));
            }
            painter.setFont(QFont(QStringLiteral("Sans Serif"), 8, QFont::Bold));
            painter.drawText(m_boxRect, Qt::AlignCenter, tr("[REDACTED]"));
        }
    } else if (m_itemType == PdfTransformItemType::AreaSelection) {
        painter.fillRect(m_boxRect, QColor(14, 165, 233, 20));
    }

    // 2. Render Selection Outline (Red SolidLine if colliding with existing text, Cyan DashLine otherwise)
    if (m_hasCollision) {
        painter.fillRect(m_boxRect, QColor(239, 68, 68, 35));
        QPen borderPen(QColor(239, 68, 68), 2.5, Qt::SolidLine);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_boxRect);
    } else {
        QPen borderPen(QColor(56, 189, 248), 1.5, Qt::DashLine);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_boxRect);
    }

    // 3. Render 8 Control Handles (high-contrast: white fill with cyan or red border)
    const QColor handleBorderColor = m_hasCollision ? QColor(239, 68, 68) : QColor(56, 189, 248);
    painter.setPen(QPen(handleBorderColor, 1.5));
    painter.setBrush(Qt::white);
    for (int i = 0; i < 8; ++i) {
        painter.drawRect(m_handles[i]);
    }

    // 4. Render Collision Warning Pill below the box when overlapping neighboring text
    if (m_hasCollision) {
        const QString warnText = tr("⚠️ Overlap with existing text");
        QFont warnFont = font();
        warnFont.setPointSize(9);
        warnFont.setBold(true);
        painter.setFont(warnFont);
        QFontMetrics fm(warnFont);
        int pillW = fm.horizontalAdvance(warnText) + 14;
        int pillH = fm.height() + 6;
        int pillX = qBound(4, m_boxRect.left(), std::max(4, width() - pillW - 4));
        int pillY = m_boxRect.bottom() + 6;
        if (pillY + pillH > height() - 4) {
            pillY = std::max(4, m_boxRect.top() - pillH - 6);
        }
        QRect pillRect(pillX, pillY, pillW, pillH);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(220, 38, 38, 235));
        painter.drawRoundedRect(pillRect, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(pillRect, Qt::AlignCenter, warnText);
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

void PdfTransformBox::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_itemType == PdfTransformItemType::Text && m_isPlacementMode) {
        if (m_boxRect.contains(event->pos())) {
            emit editRequested();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
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
    if (newRect.width() >= 24 && newRect.height() >= 16) {
        bool sizeChanged = (m_boxRect.size() != newRect.size());
        m_boxRect = newRect;

        // Synchronize inline text editor geometry whenever box moves OR resizes
        if (m_itemType == PdfTransformItemType::Text && m_inlineTextEdit) {
            m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
        }

        if (sizeChanged) {
            if (m_itemType == PdfTransformItemType::Image || m_itemType == PdfTransformItemType::Signature) {
                m_wasResizing = true;
                updateCachedPixmap(true);
            } else if (m_itemType == PdfTransformItemType::Text && !m_isPlacementMode) {
                adjustHeightToFitText();
            } else if (m_itemType == PdfTransformItemType::Shape) {
                if (m_dragStartRect.width() > 0 && m_dragStartRect.height() > 0) {
                    double sx = static_cast<double>(newRect.width()) / m_dragStartRect.width();
                    double sy = static_cast<double>(newRect.height()) / m_dragStartRect.height();
                    if (m_shapeLocalStart != QPoint() && m_shapeLocalStart != QPoint(-1, -1)) {
                        m_shapeLocalStart = QPoint(qRound(m_shapeLocalStart.x() * sx), qRound(m_shapeLocalStart.y() * sy));
                    }
                    if (m_shapeLocalEnd != QPoint() && m_shapeLocalEnd != QPoint(-1, -1)) {
                        m_shapeLocalEnd = QPoint(qRound(m_shapeLocalEnd.x() * sx), qRound(m_shapeLocalEnd.y() * sy));
                    }
                }
            }
        }
        updateHandles();
        repositionToolbar();
        emit geometryChanged(m_boxRect);
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
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
               event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        int step = (event->modifiers() & Qt::ShiftModifier) ? 5 : 1;
        int dx = 0;
        int dy = 0;
        if (event->key() == Qt::Key_Left) dx = -step;
        else if (event->key() == Qt::Key_Right) dx = step;
        else if (event->key() == Qt::Key_Up) dy = -step;
        else if (event->key() == Qt::Key_Down) dy = step;

        if (dx != 0 || dy != 0) {
            m_boxRect.translate(dx, dy);
            if (m_itemType == PdfTransformItemType::Text && m_inlineTextEdit) {
                m_inlineTextEdit->setGeometry(m_boxRect.adjusted(3, 2, -3, -2));
            }
            updateHandles();
            repositionToolbar();
            emit geometryChanged(m_boxRect);
            update();
            event->accept();
            return;
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PdfTransformBox::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}
