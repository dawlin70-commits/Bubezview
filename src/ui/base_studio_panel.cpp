/**
 * @file base_studio_panel.cpp
 * @brief Implementation of the reusable base studio panel shell.
 */

#include "base_studio_panel.h"
#include <QPainter>
#include <QStyleOption>
#include <QWheelEvent>

BaseStudioPanel::BaseStudioPanel(QWidget *parent)
    : QWidget(parent)
    , m_currentHeaderIconAccent(QColor("#38bdf8"))
{
    setObjectName("BaseStudioPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(280);
    setMaximumWidth(600);

    setupBaseUi();
    updateTheme();
}

void BaseStudioPanel::setupBaseUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 10, 12, 12);
    rootLayout->setSpacing(10);

    // =========================================================================
    // 1. HEADER: Icon, Title, Compare, Reset, Close
    // =========================================================================
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_headerIconLabel = new QLabel(this);
    m_headerIconLabel->setPixmap(VectorIcons::renderUiPixmap(m_currentHeaderIcon, m_currentHeaderIconAccent, 18, devicePixelRatioF()));
    headerLayout->addWidget(m_headerIconLabel, 0, Qt::AlignVCenter);

    m_lblTitle = new QLabel(tr("Studio"), this);
    m_lblTitle->setObjectName("lblStudioTitle");
    headerLayout->addWidget(m_lblTitle, 1, Qt::AlignVCenter);

    // Hold-to-compare button
    m_btnCompare = new QToolButton(this);
    m_btnCompare->setFixedSize(28, 28);
    m_btnCompare->setIconSize(QSize(18, 18));
    m_btnCompare->setCursor(Qt::PointingHandCursor);
    m_btnCompare->setToolTip(tr("Hold to compare with original"));
    connect(m_btnCompare, &QToolButton::pressed, this, [this]() { emit compareToggled(true); });
    connect(m_btnCompare, &QToolButton::released, this, [this]() { emit compareToggled(false); });
    headerLayout->addWidget(m_btnCompare, 0, Qt::AlignVCenter);

    // Reset button
    m_btnReset = new QToolButton(this);
    m_btnReset->setFixedSize(28, 28);
    m_btnReset->setIconSize(QSize(18, 18));
    m_btnReset->setCursor(Qt::PointingHandCursor);
    m_btnReset->setToolTip(tr("Reset all adjustments"));
    connect(m_btnReset, &QToolButton::clicked, this, &BaseStudioPanel::resetRequested);
    headerLayout->addWidget(m_btnReset, 0, Qt::AlignVCenter);

    // Close button
    m_btnClose = new QToolButton(this);
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setIconSize(QSize(18, 18));
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setToolTip(tr("Close Studio"));
    connect(m_btnClose, &QToolButton::clicked, this, &BaseStudioPanel::closeRequested);
    headerLayout->addWidget(m_btnClose, 0, Qt::AlignVCenter);

    rootLayout->addLayout(headerLayout);

    // =========================================================================
    // 2. SEGMENTED CAPSULE TAB BAR
    // =========================================================================
    m_tabBarWidget = new QWidget(this);
    m_tabBarWidget->setObjectName("StudioTabBar");
    m_tabBarWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_tabBarWidget->setVisible(false); // Hidden until >1 tab is added

    m_tabLayout = new QHBoxLayout(m_tabBarWidget);
    m_tabLayout->setContentsMargins(2, 2, 2, 2);
    m_tabLayout->setSpacing(2);

    rootLayout->addWidget(m_tabBarWidget, 0);

    // =========================================================================
    // 3. STACKED CONTENT WIDGET
    // =========================================================================
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setAttribute(Qt::WA_StyledBackground, true);
    rootLayout->addWidget(m_stackedWidget, 1);

    // =========================================================================
    // 4. FOOTER: Cancel & Apply
    // =========================================================================
    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 4, 0, 0);
    footerLayout->setSpacing(8);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setFixedHeight(32);
    m_btnCancel->setToolTip(tr("Discard studio adjustments"));
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
        emit cancelRequested();
        emit closeRequested();
    });
    footerLayout->addWidget(m_btnCancel, 1);

    m_btnApply = new QPushButton(tr("Apply Changes"), this);
    m_btnApply->setObjectName("btnStudioApply");
    m_btnApply->setCursor(Qt::PointingHandCursor);
    m_btnApply->setFixedHeight(32);
    m_btnApply->setToolTip(tr("Apply adjustments permanently"));
    connect(m_btnApply, &QPushButton::clicked, this, &BaseStudioPanel::applyRequested);
    footerLayout->addWidget(m_btnApply, 2);

    rootLayout->addLayout(footerLayout);
}

void BaseStudioPanel::setHeaderTitle(const QString &title)
{
    if (m_lblTitle) {
        m_lblTitle->setText(title);
    }
}

void BaseStudioPanel::setHeaderIcon(VectorIcons::UiIcon icon, const QColor &accentColor)
{
    m_currentHeaderIcon = icon;
    if (accentColor.isValid()) {
        m_currentHeaderIconAccent = accentColor;
    }
    if (m_headerIconLabel) {
        m_headerIconLabel->setPixmap(VectorIcons::renderUiPixmap(m_currentHeaderIcon, m_currentHeaderIconAccent, 18, devicePixelRatioF()));
    }
}

int BaseStudioPanel::addTabPage(const QString &title, QWidget *pageWidget)
{
    const int index = m_stackedWidget->addWidget(pageWidget);

    auto *btn = new QPushButton(title, m_tabBarWidget);
    btn->setCheckable(true);
    btn->setChecked(index == 0);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(26);
    connect(btn, &QPushButton::clicked, this, [this, index]() {
        onTabButtonClicked(index);
    });

    m_tabLayout->addWidget(btn, 1);
    m_tabButtons.append(btn);

    // Show tab bar if we have multiple tabs
    m_tabBarWidget->setVisible(m_tabButtons.size() > 1);

    return index;
}

void BaseStudioPanel::setCurrentTab(int index)
{
    if (index >= 0 && index < m_stackedWidget->count()) {
        onTabButtonClicked(index);
    }
}

int BaseStudioPanel::currentTabIndex() const
{
    return m_stackedWidget ? m_stackedWidget->currentIndex() : -1;
}

void BaseStudioPanel::onTabButtonClicked(int index)
{
    for (int i = 0; i < m_tabButtons.size(); ++i) {
        m_tabButtons[i]->setChecked(i == index);
    }
    m_stackedWidget->setCurrentIndex(index);
    emit tabChanged(index);
}

void BaseStudioPanel::setCompareButtonVisible(bool visible)
{
    if (m_btnCompare) {
        m_btnCompare->setVisible(visible);
    }
}

void BaseStudioPanel::setResetButtonVisible(bool visible)
{
    if (m_btnReset) {
        m_btnReset->setVisible(visible);
    }
}

void BaseStudioPanel::setApplyButtonText(const QString &text)
{
    if (m_btnApply) {
        m_btnApply->setText(text);
    }
}

void BaseStudioPanel::setApplyButtonEnabled(bool enabled)
{
    if (m_btnApply) {
        m_btnApply->setEnabled(enabled);
    }
}

QScrollArea *BaseStudioPanel::createStandardScrollArea(QWidget *content, QWidget *parent)
{
    auto *scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scrollArea->viewport()) {
        scrollArea->viewport()->setAutoFillBackground(false);
    }
    if (content) {
        scrollArea->setWidget(content);
    }
    return scrollArea;
}

void BaseStudioPanel::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void BaseStudioPanel::wheelEvent(QWheelEvent *event)
{
    // Always consume wheel events over the studio panel to prevent
    // unintentional scrolling or page flips in the underlying document/image viewer
    event->accept();
}

void BaseStudioPanel::customizeTheme(QString &, bool)
{
    // Default implementation does nothing; subclasses can override
}

void BaseStudioPanel::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();
    const bool isDark = ThemeManager::isDark(preset);

    // 1. Theme-adaptive palette applied to panel and all child widgets
    QPalette pal = palette();
    QColor windowBg = isDark ? QColor("#1e222a") : QColor("#f8fafc");
    QColor textColor = isDark ? QColor("#f1f5f9") : QColor("#0f172a");
    QColor baseBg = isDark ? QColor("#181b20") : QColor("#ffffff");
    QColor btnBg = isDark ? QColor("#282c34") : QColor("#e2e8f0");
    pal.setColor(QPalette::Window, windowBg);
    pal.setColor(QPalette::Base, baseBg);
    pal.setColor(QPalette::WindowText, textColor);
    pal.setColor(QPalette::Text, textColor);
    pal.setColor(QPalette::Button, btnBg);
    pal.setColor(QPalette::ButtonText, textColor);
    setPalette(pal);

    // 2. High-contrast theme-adaptive header tool button icons
    QColor headerIconColor = isDark ? QColor("#f1f5f9") : QColor("#334155");

    if (m_btnCompare) {
        m_btnCompare->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Eye, headerIconColor, 16));
    }
    if (m_btnReset) {
        m_btnReset->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, headerIconColor, 16));
    }
    if (m_btnClose) {
        m_btnClose->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Close, headerIconColor, 16));
    }
    if (m_headerIconLabel) {
        m_headerIconLabel->setPixmap(VectorIcons::renderUiPixmap(m_currentHeaderIcon, m_currentHeaderIconAccent, 18, devicePixelRatioF()));
    }

    // 3. Dynamic color tokens
    const QString bgHex = isDark ? "#1e222a" : "#f8fafc";
    const QString borderHex = isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.08)";
    const QString textPrimary = isDark ? "#f1f5f9" : "#0f172a";
    const QString textSecondary = isDark ? "#cbd5e1" : "#334155";
    const QString textMuted = isDark ? "#94a3b8" : "#64748b";

    const QString tabBarBg = isDark ? "rgba(255, 255, 255, 0.06)" : "rgba(0, 0, 0, 0.05)";
    const QString tabInactiveText = isDark ? "#94a3b8" : "#64748b";
    const QString tabActiveBg = "#0284c7";

    const QString controlBg = isDark ? "rgba(255, 255, 255, 0.05)" : "rgba(0, 0, 0, 0.03)";
    const QString controlBorder = isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.12)";
    const QString controlHoverBorder = isDark ? "#38bdf8" : "#0284c7";

    const QString btnDefaultBg = isDark ? "rgba(255, 255, 255, 0.06)" : "rgba(0, 0, 0, 0.04)";
    const QString btnDefaultHoverBg = isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.08)";
    const QString menuBg = isDark ? "#22262b" : "#ffffff";

    // 4. Robust stylesheet composition
    QString qss;
    qss.reserve(4096);

    // Root panel background & border (supports both BaseStudioPanel and derived classes)
    qss += "#BaseStudioPanel, #PdfStudioPanel { background-color: " + bgHex + "; border-left: 1px solid " + borderHex + "; }\n";

    // Stacked widget, scroll areas, and scroll viewports MUST be transparent
    qss += "QStackedWidget, "
           "QScrollArea, "
           "QScrollArea > QWidget, "
           "QScrollArea > QWidget > QWidget { "
           "   background-color: transparent; "
           "   background: transparent; "
           "   border: none; "
           "}\n";

    // Panel title
    qss += "#lblStudioTitle { font-weight: 700; font-size: 13px; color: " + textPrimary + "; }\n";

    // Segmented tab bar
    qss += "#StudioTabBar { background-color: " + tabBarBg + "; border-radius: 8px; padding: 2px; }\n";
    qss += "#StudioTabBar QPushButton { background-color: transparent; color: " + tabInactiveText + "; border: none; border-radius: 6px; font-size: 11px; font-weight: 600; padding: 3px 2px; }\n";
    qss += "#StudioTabBar QPushButton:checked { background-color: " + tabActiveBg + "; color: #ffffff; }\n";
    qss += "#StudioTabBar QPushButton:hover:!checked { background-color: " + btnDefaultHoverBg + "; color: " + textPrimary + "; }\n";

    // Labels
    qss += "QLabel { color: " + textPrimary + "; }\n";
    qss += "QLabel#sectionHeader { font-size: 10px; font-weight: 700; color: " + textMuted + "; letter-spacing: 0.5px; margin-top: 6px; }\n";
    qss += "QLabel#sliderTitle { font-size: 11px; font-weight: 500; color: " + textSecondary + "; }\n";
    qss += "QLabel#sliderVal { font-size: 11px; font-weight: 600; color: " + textMuted + "; }\n";
    qss += "QLabel#hintLabel { font-size: 10px; color: " + textMuted + "; }\n";

    // Checkboxes
    qss += "QCheckBox { color: " + textSecondary + "; font-size: 11px; font-weight: 500; spacing: 6px; }\n";
    qss += "QCheckBox:hover { color: " + textPrimary + "; }\n";
    qss += "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; border: 1px solid " + controlBorder + "; background-color: " + controlBg + "; }\n";
    qss += "QCheckBox::indicator:hover { border-color: " + controlHoverBorder + "; }\n";
    qss += "QCheckBox::indicator:checked { background-color: #0284c7; border-color: #0284c7; }\n";
    qss += "QCheckBox#chkAutoEnable { font-weight: 600; font-size: 12px; color: #0284c7; }\n";

    // Group Boxes
    qss += "QGroupBox { font-size: 11px; font-weight: 600; color: " + textMuted + "; border: 1px solid " + controlBorder + "; border-radius: 6px; margin-top: 10px; padding-top: 10px; }\n";
    qss += "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0 4px; }\n";

    // Combo Box
    qss += "QComboBox { background-color: " + controlBg + "; border: 1px solid " + controlBorder + "; border-radius: 6px; padding: 3px 8px; color: " + textPrimary + "; font-size: 11px; font-weight: 500; }\n";
    qss += "QComboBox:hover { border-color: " + controlHoverBorder + "; }\n";
    qss += "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 18px; border-left: none; }\n";
    qss += "QComboBox QAbstractItemView { background-color: " + menuBg + "; color: " + textPrimary + "; border: 1px solid " + controlBorder + "; selection-background-color: #0284c7; selection-color: #ffffff; }\n";

    // General Tool Buttons
    qss += "QToolButton { background-color: " + btnDefaultBg + "; border: 1px solid " + controlBorder + "; border-radius: 6px; padding: 3px 6px; color: " + textPrimary + "; font-size: 11px; font-weight: 500; }\n";
    qss += "QToolButton:hover { background-color: " + btnDefaultHoverBg + "; border-color: " + controlHoverBorder + "; }\n";
    qss += "QToolButton:checked { background-color: #0284c7; border-color: #0284c7; color: #ffffff; }\n";

    // General Push Buttons (Cancel, Preset buttons)
    qss += "QPushButton { background-color: " + btnDefaultBg + "; border: 1px solid " + controlBorder + "; border-radius: 6px; color: " + textPrimary + "; font-size: 11px; font-weight: 500; padding: 4px 8px; }\n";
    qss += "QPushButton:hover { background-color: " + btnDefaultHoverBg + "; border-color: " + controlHoverBorder + "; }\n";
    qss += "QPushButton:checked { background-color: #0284c7; color: #ffffff; border: 1px solid #0284c7; font-weight: 600; }\n";

    // Primary Apply Button
    qss += "QPushButton#btnStudioApply { background-color: #0284c7; color: #ffffff; font-weight: 700; border: none; }\n";
    qss += "QPushButton#btnStudioApply:hover { background-color: #0369a1; }\n";

    // Let derived panels add specialized rules or icon updates
    customizeTheme(qss, isDark);

    // Scrollbars
    qss += ThemeManager::scrollBarStyleSheet(preset);

    setStyleSheet(qss);
}
