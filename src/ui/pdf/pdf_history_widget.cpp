/**
 * @file pdf_history_widget.cpp
 * @brief Implementation of PdfHistoryWidget and PdfHistoryCard.
 */

#include "pdf_history_widget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include "core/services/theme_manager.h"

PdfHistoryCard::PdfHistoryCard(const PdfHistoryStep &step, QWidget *parent)
    : QFrame(parent)
    , m_stepIndex(step.index)
    , m_isCurrent(step.isCurrent)
{
    // The active/selected slide is noticeably larger so it stands out distinctly
    if (m_isCurrent) {
        setFixedSize(220, 275);
    } else {
        setFixedSize(160, 230);
    }

    setCursor(Qt::PointingHandCursor);
    setObjectName(QStringLiteral("PdfHistoryCard"));

    updateStyle(false);

    auto *cardLayout = new QVBoxLayout(this);
    cardLayout->setContentsMargins(m_isCurrent ? 10 : 6, m_isCurrent ? 8 : 6, m_isCurrent ? 10 : 6, m_isCurrent ? 8 : 6);
    cardLayout->setSpacing(m_isCurrent ? 6 : 4);

    // 1. Badge or step number header
    m_lblBadge = new QLabel(this);
    m_lblBadge->setAlignment(Qt::AlignCenter);
    m_lblBadge->setFixedHeight(m_isCurrent ? 18 : 14);
    if (m_isCurrent) {
        m_lblBadge->setText(tr("● Active Selection"));
        m_lblBadge->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #0A84FF;"));
    } else {
        m_lblBadge->setText(tr("Step %1").arg(step.index + 1));
        bool isLight = (ThemeManager::currentPreset() == ThemePreset::ModernLight);
        m_lblBadge->setStyleSheet(isLight
            ? QStringLiteral("font-size: 10px; color: rgba(0, 0, 0, 0.55); font-weight: 500;")
            : QStringLiteral("font-size: 10px; color: rgba(255, 255, 255, 0.55); font-weight: 500;"));
    }
    cardLayout->addWidget(m_lblBadge);

    // 2. Generous Thumbnail Preview
    m_lblThumbnail = new QLabel(this);
    int thumbW = m_isCurrent ? 200 : 148;
    int thumbH = m_isCurrent ? 185 : 146;
    m_lblThumbnail->setFixedSize(thumbW, thumbH);
    m_lblThumbnail->setAlignment(Qt::AlignCenter);

    bool isLight = (ThemeManager::currentPreset() == ThemePreset::ModernLight);
    m_lblThumbnail->setStyleSheet(QString(
        "background-color: %1;"
        "border: 1px solid %2;"
        "border-radius: 6px;"
    ).arg(isLight ? QStringLiteral("#FFFFFF") : QStringLiteral("#18181B"),
          isLight ? QStringLiteral("rgba(0, 0, 0, 0.12)") : QStringLiteral("rgba(255, 255, 255, 0.12)")));

    if (!step.thumbnail.isNull()) {
        m_lblThumbnail->setPixmap(step.thumbnail.scaled(thumbW - 4, thumbH - 4, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_lblThumbnail->setText(tr("📄"));
        m_lblThumbnail->setStyleSheet(m_lblThumbnail->styleSheet() + QStringLiteral("font-size: 32px;"));
    }
    cardLayout->addWidget(m_lblThumbnail, 0, Qt::AlignCenter);

    // 3. Action Description
    m_lblTitle = new QLabel(step.description, this);
    m_lblTitle->setAlignment(Qt::AlignCenter);
    m_lblTitle->setFixedHeight(m_isCurrent ? 22 : 18);
    QFont f = m_lblTitle->font();
    f.setPointSize(m_isCurrent ? 10 : 9);
    f.setBold(m_isCurrent);
    m_lblTitle->setFont(f);
    m_lblTitle->setToolTip(step.description);
    m_lblTitle->setStyleSheet(isLight
        ? (m_isCurrent ? QStringLiteral("color: #0A84FF; font-weight: bold;") : QStringLiteral("color: #1F2937;"))
        : (m_isCurrent ? QStringLiteral("color: #38BDF8; font-weight: bold;") : QStringLiteral("color: #F3F4F6;")));
    cardLayout->addWidget(m_lblTitle);

    // 4. Timestamp
    m_lblTime = new QLabel(step.timestamp.toString(QStringLiteral("hh:mm:ss")), this);
    m_lblTime->setAlignment(Qt::AlignCenter);
    m_lblTime->setFixedHeight(14);
    m_lblTime->setStyleSheet(isLight
        ? QStringLiteral("font-size: 9px; color: rgba(0, 0, 0, 0.45);")
        : QStringLiteral("font-size: 9px; color: rgba(255, 255, 255, 0.45);"));
    cardLayout->addWidget(m_lblTime);
}

void PdfHistoryCard::updateStyle(bool hovered)
{
    bool isLight = !ThemeManager::isDarkTheme();

    QString bgColor;
    QString borderColor;
    int borderWidth = m_isCurrent ? 2 : 1;

    if (m_isCurrent) {
        bgColor = isLight
            ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 189, 248, 0.22), stop:1 rgba(37, 99, 235, 0.14))")
            : QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 189, 248, 0.28), stop:1 rgba(37, 99, 235, 0.18))");
        borderColor = isLight ? QStringLiteral("#0284c7") : QStringLiteral("#38bdf8");
    } else if (hovered) {
        bgColor = isLight ? QStringLiteral("rgba(56, 189, 248, 0.12)") : QStringLiteral("rgba(56, 189, 248, 0.18)");
        borderColor = isLight ? QStringLiteral("rgba(2, 132, 199, 0.65)") : QStringLiteral("rgba(56, 189, 248, 0.70)");
    } else {
        bgColor = isLight ? QStringLiteral("rgba(255, 255, 255, 0.72)") : QStringLiteral("rgba(30, 41, 59, 0.65)");
        borderColor = isLight ? QStringLiteral("rgba(15, 23, 42, 0.10)") : QStringLiteral("rgba(255, 255, 255, 0.12)");
    }

    setStyleSheet(QString(
        "QFrame#PdfHistoryCard {"
        "   background: %1;"
        "   border: %2px solid %3;"
        "   border-radius: %4px;"
        "}"
    ).arg(bgColor, QString::number(borderWidth), borderColor, QString::number(m_isCurrent ? 14 : 12)));
}

void PdfHistoryCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_stepIndex);
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void PdfHistoryCard::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    updateStyle(true);
}

void PdfHistoryCard::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    updateStyle(false);
}

// =============================================================================
// PdfHistoryWidget implementation
// =============================================================================

PdfHistoryWidget::PdfHistoryWidget(QWidget *parent)
    : FramelessDialog(parent)
{
    setWindowTitle(tr("🎞️ Visual History & Document Timeline"));
    setMinimumWidth(880);
    setFixedHeight(410);

    auto *mainLayout = contentLayout();
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    m_lblSubtitle = new QLabel(tr("Click any snapshot to jump to that document state"), this);
    m_lblSubtitle->setStyleSheet(QStringLiteral("font-size: 11px; opacity: 0.7; margin-bottom: 2px;"));
    mainLayout->addWidget(m_lblSubtitle);

    // 2. Horizontal Scroll Area for Snapshot Cards
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFixedHeight(315);

    m_cardContainer = new QWidget(m_scrollArea);
    m_cardContainer->setObjectName(QStringLiteral("PdfHistoryCardContainer"));
    m_cardLayout = new QHBoxLayout(m_cardContainer);
    m_cardLayout->setContentsMargins(6, 6, 6, 6);
    m_cardLayout->setSpacing(14);
    m_cardLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_lblEmpty = new QLabel(tr("No modification history available yet."), m_cardContainer);
    m_lblEmpty->setAlignment(Qt::AlignCenter);
    m_cardLayout->addWidget(m_lblEmpty);

    m_scrollArea->setWidget(m_cardContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    updateTheme();
}

void PdfHistoryWidget::updateTheme()
{
    bool isLight = !ThemeManager::isDarkTheme();
    QString bg = isLight
        ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f8fafc, stop:0.5 #f1f5f9, stop:1 #e2e8f0)")
        : QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e2538, stop:0.5 #151a28, stop:1 #0f1420)");
    QString textCol = isLight ? QStringLiteral("#0f172a") : QStringLiteral("#f8fafc");
    QString borderCol = isLight ? QStringLiteral("rgba(15, 23, 42, 0.14)") : QStringLiteral("rgba(255, 255, 255, 0.16)");

    setStyleSheet(QString(
        "QScrollArea, QWidget#qt_scrollarea_viewport, QWidget#PdfHistoryCardContainer {"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QLabel {"
        "   color: %2;"
        "   background: transparent;"
        "}"
        "QToolButton {"
        "   background: rgba(148, 163, 184, 0.14);"
        "   border: 1px solid %1;"
        "   border-radius: 14px;"
        "   color: %2;"
        "   font-weight: bold;"
        "   font-size: 13px;"
        "}"
        "QToolButton:hover {"
        "   background-color: rgba(239, 68, 68, 0.25);"
        "   border-color: rgba(248, 113, 113, 0.55);"
        "}"
        "QScrollBar:horizontal {"
        "   height: 8px;"
        "   background: transparent;"
        "   margin: 0px 4px 0px 4px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: %3;"
        "   min-width: 24px;"
        "   border-radius: 4px;"
        "}"
    ).arg(borderCol, textCol,
          isLight ? QStringLiteral("rgba(100, 116, 139, 0.42)") : QStringLiteral("rgba(148, 163, 184, 0.38)")));
}

void PdfHistoryWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void PdfHistoryWidget::setHistorySteps(const QList<PdfHistoryStep> &steps)
{
    // Clear previous cards immediately so no ghost cards overlap
    QLayoutItem *item;
    while ((item = m_cardLayout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->setParent(nullptr);
            delete w;
        }
        delete item;
    }

    if (steps.isEmpty()) {
        m_lblEmpty = new QLabel(tr("No modification history available yet."), m_cardContainer);
        m_lblEmpty->setAlignment(Qt::AlignCenter);
        m_cardLayout->addWidget(m_lblEmpty);
        setWindowTitle(tr("🎞️ Visual History & Document Timeline"));
        return;
    }

    setWindowTitle(tr("🎞️ Visual History & Document Timeline (%1 steps)").arg(steps.size()));

    for (const auto &step : steps) {
        auto *card = new PdfHistoryCard(step, m_cardContainer);
        connect(card, &PdfHistoryCard::clicked, this, [this](int idx) {
            emit stepSelected(idx);
            accept();
        });
        m_cardLayout->addWidget(card);
    }
    m_cardLayout->addStretch(1);
}
