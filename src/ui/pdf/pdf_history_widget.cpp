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
    bool isLight = (ThemeManager::currentPreset() == ThemePreset::ModernLight);

    QString bgColor;
    QString borderColor;
    int borderWidth = m_isCurrent ? 2 : 1;

    if (m_isCurrent) {
        bgColor = isLight ? QStringLiteral("rgba(0, 122, 255, 0.12)") : QStringLiteral("rgba(10, 132, 255, 0.22)");
        borderColor = isLight ? QStringLiteral("#007AFF") : QStringLiteral("#0A84FF");
    } else if (hovered) {
        bgColor = isLight ? QStringLiteral("rgba(0, 122, 255, 0.08)") : QStringLiteral("rgba(10, 132, 255, 0.14)");
        borderColor = isLight ? QStringLiteral("rgba(0, 122, 255, 0.60)") : QStringLiteral("rgba(10, 132, 255, 0.70)");
    } else {
        bgColor = isLight ? QStringLiteral("rgba(0, 0, 0, 0.03)") : QStringLiteral("rgba(255, 255, 255, 0.04)");
        borderColor = isLight ? QStringLiteral("rgba(0, 0, 0, 0.12)") : QStringLiteral("rgba(255, 255, 255, 0.12)");
    }

    setStyleSheet(QString(
        "QFrame#PdfHistoryCard {"
        "   background-color: %1;"
        "   border: %2px solid %3;"
        "   border-radius: %4px;"
        "}"
    ).arg(bgColor, QString::number(borderWidth), borderColor, QString::number(m_isCurrent ? 12 : 8)));
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
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setMinimumWidth(880);
    setFixedHeight(390);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(10);

    // 1. Header Bar: Title, Subtitle and Close button
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(4, 0, 4, 0);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);

    m_lblHeader = new QLabel(tr("🎞️ Visual History & Document Timeline"), this);
    QFont hf = m_lblHeader->font();
    hf.setPointSize(12);
    hf.setBold(true);
    m_lblHeader->setFont(hf);
    titleCol->addWidget(m_lblHeader);

    m_lblSubtitle = new QLabel(tr("Click any snapshot to jump to that document state"), this);
    m_lblSubtitle->setStyleSheet(QStringLiteral("font-size: 11px; opacity: 0.7;"));
    titleCol->addWidget(m_lblSubtitle);

    headerLayout->addLayout(titleCol, 1);

    m_btnClose = new QToolButton(this);
    m_btnClose->setText(QStringLiteral("✕"));
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setToolTip(tr("Close history timeline (Esc)"));
    connect(m_btnClose, &QToolButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(m_btnClose, 0, Qt::AlignTop);

    mainLayout->addLayout(headerLayout);

    // 2. Horizontal Scroll Area for Snapshot Cards
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFixedHeight(315);

    m_cardContainer = new QWidget(m_scrollArea);
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
    bool isLight = (ThemeManager::currentPreset() == ThemePreset::ModernLight);
    QString bg = isLight ? QStringLiteral("#FFFFFF") : QStringLiteral("#18181B");
    QString textCol = isLight ? QStringLiteral("#111827") : QStringLiteral("#F9FAFB");
    QString borderCol = isLight ? QStringLiteral("rgba(0, 0, 0, 0.15)") : QStringLiteral("rgba(255, 255, 255, 0.15)");

    setStyleSheet(QString(
        "QDialog {"
        "   background-color: %1;"
        "   border: 1.5px solid %2;"
        "   border-radius: 12px;"
        "}"
        "QLabel {"
        "   color: %3;"
        "}"
        "QToolButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 14px;"
        "   color: %3;"
        "   font-weight: bold;"
        "   font-size: 13px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %4;"
        "}"
        "QScrollBar:horizontal {"
        "   height: 8px;"
        "   background: transparent;"
        "   margin: 0px 4px 0px 4px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: %5;"
        "   min-width: 24px;"
        "   border-radius: 4px;"
        "}"
    ).arg(bg, borderCol, textCol,
          isLight ? QStringLiteral("rgba(0, 0, 0, 0.08)") : QStringLiteral("rgba(255, 255, 255, 0.12)"),
          isLight ? QStringLiteral("rgba(0, 0, 0, 0.25)") : QStringLiteral("rgba(255, 255, 255, 0.25)")));
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
        m_lblHeader->setText(tr("🎞️ Visual History & Document Timeline"));
        return;
    }

    m_lblHeader->setText(tr("🎞️ Visual History & Document Timeline (%1 steps)").arg(steps.size()));

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
