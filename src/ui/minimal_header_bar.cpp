#include "minimal_header_bar.h"
#include "vector_icons.h"
#include "pdf/pdf_history_widget.h"
#include <QWidgetAction>
#include <QPainter>
#include <QWindow>
#include <QApplication>
#include <QEvent>
#include <QButtonGroup>

/**
 * @brief Dynamic stacked widget that only considers the currently active page
 * for minimumSizeHint and sizeHint calculations, preventing hidden edit toolbars
 * from artificially locking the main window to a large minimum width in preview mode.
 */
class DynamicStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize minimumSizeHint() const override {
        // Always return a compact minimum size hint so neither preview nor edit
        // toolbars inflate the top-level window's minimum width on the WM.
        return QSize(40, 32);
    }

    QSize sizeHint() const override {
        if (currentWidget()) {
            return currentWidget()->sizeHint();
        }
        return QStackedWidget::sizeHint();
    }
};

MinimalHeaderBar::MinimalHeaderBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("MinimalHeaderBar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_uiScale = ThemeManager::uiScalePercent() / 100.0;
    setFixedHeight(qRound(54 * m_uiScale));
    setupUi();
    updateHeaderGeometry();
    applyThemePreset(ThemePreset::DarkSlate);
}

QSize MinimalHeaderBar::minimumSizeHint() const
{
    return QSize(320, height());
}

void MinimalHeaderBar::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_primaryBar = new QWidget(this);
    m_primaryBar->setFixedHeight(54);
    auto *mainLayout = new QHBoxLayout(m_primaryBar);
    mainLayout->setContentsMargins(16, 0, 16, 0);
    mainLayout->setSpacing(10);

    // =========================================================================
    // 1. LEFT: Window Controls + Navigation Island (Preview) / Edit Left Island (Edit)
    // =========================================================================
    m_windowControlsWidget = new QWidget(m_primaryBar);
    auto *ctrlsLayout = new QHBoxLayout(m_windowControlsWidget);
    ctrlsLayout->setContentsMargins(0, 0, 4, 0);
    ctrlsLayout->setSpacing(10);

    m_btnClose = new QToolButton(m_windowControlsWidget);
    m_btnClose->setFixedSize(36, 36);
    m_btnClose->setIconSize(QSize(18, 18));
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setToolTip(tr("Close (Esc / Space)"));
    connect(m_btnClose, &QToolButton::clicked, this, &MinimalHeaderBar::closeRequested);
    ctrlsLayout->addWidget(m_btnClose, 0, Qt::AlignVCenter);

    // Unified Rounded Glass Capsule Island for [< | > | ⊞ Gallery]
    m_navIslandFrame = new QFrame(m_windowControlsWidget);
    m_navIslandFrame->setObjectName("NavIslandFrame");
    m_navIslandFrame->setFixedHeight(38);
    auto *navIslandLayout = new QHBoxLayout(m_navIslandFrame);
    navIslandLayout->setContentsMargins(4, 3, 4, 3);
    navIslandLayout->setSpacing(2);

    m_btnPrev = new QToolButton(m_navIslandFrame);
    m_btnPrev->setFixedSize(34, 30);
    m_btnPrev->setIconSize(QSize(20, 20));
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    m_btnPrev->setToolTip(tr("Previous File (Left Arrow)"));
    m_btnPrev->setEnabled(false);
    connect(m_btnPrev, &QToolButton::clicked, this, &MinimalHeaderBar::prevRequested);
    navIslandLayout->addWidget(m_btnPrev, 0, Qt::AlignVCenter);

    m_btnNext = new QToolButton(m_navIslandFrame);
    m_btnNext->setFixedSize(34, 30);
    m_btnNext->setIconSize(QSize(20, 20));
    m_btnNext->setCursor(Qt::PointingHandCursor);
    m_btnNext->setToolTip(tr("Next File (Right Arrow)"));
    m_btnNext->setEnabled(false);
    connect(m_btnNext, &QToolButton::clicked, this, &MinimalHeaderBar::nextRequested);
    navIslandLayout->addWidget(m_btnNext, 0, Qt::AlignVCenter);

    m_sepNavIsland = new QFrame(m_navIslandFrame);
    m_sepNavIsland->setFrameShape(QFrame::VLine);
    m_sepNavIsland->setFrameShadow(QFrame::Plain);
    m_sepNavIsland->setFixedSize(1, 18);
    navIslandLayout->addWidget(m_sepNavIsland, 0, Qt::AlignVCenter);

    m_btnGallery = new QToolButton(m_navIslandFrame);
    m_btnGallery->setFixedSize(38, 30);
    m_btnGallery->setIconSize(QSize(21, 21));
    m_btnGallery->setCursor(Qt::PointingHandCursor);
    m_btnGallery->setToolTip(tr("Folder Gallery (Alt+Up)"));
    connect(m_btnGallery, &QToolButton::clicked, this, &MinimalHeaderBar::galleryRequested);
    navIslandLayout->addWidget(m_btnGallery, 0, Qt::AlignVCenter);

    ctrlsLayout->addWidget(m_navIslandFrame, 0, Qt::AlignVCenter);

    mainLayout->addWidget(m_windowControlsWidget, 0, Qt::AlignVCenter);

    // Install event filter to allow dragging from empty margins inside windowControlsWidget
    m_windowControlsWidget->installEventFilter(this);

    // Left Edit Mode Capsule Island: [✕ Discard | ↶ Undo | ⏱ Timeline]
    m_editLeftWidget = new QFrame(m_primaryBar);
    m_editLeftWidget->setObjectName("EditLeftIsland");
    m_editLeftWidget->setFixedHeight(38);
    auto *editLeftLayout = new QHBoxLayout(m_editLeftWidget);
    editLeftLayout->setContentsMargins(4, 3, 4, 3);
    editLeftLayout->setSpacing(2);

    m_btnDiscard = new QToolButton(m_editLeftWidget);
    m_btnDiscard->setFixedSize(38, 30);
    m_btnDiscard->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_btnDiscard->setIconSize(QSize(21, 21));
    m_btnDiscard->setCursor(Qt::PointingHandCursor);
    m_btnDiscard->setToolTip(tr("Discard changes (Esc)"));
    connect(m_btnDiscard, &QToolButton::clicked, this, &MinimalHeaderBar::discardEditRequested);
    editLeftLayout->addWidget(m_btnDiscard, 0, Qt::AlignVCenter);

    // Fast 1-click single-step undo button
    m_btnQuickUndo = new QToolButton(m_editLeftWidget);
    m_btnQuickUndo->setFixedSize(38, 30);
    m_btnQuickUndo->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_btnQuickUndo->setIconSize(QSize(21, 21));
    m_btnQuickUndo->setCursor(Qt::PointingHandCursor);
    m_btnQuickUndo->setToolTip(tr("Undo (Ctrl+Z)"));
    connect(m_btnQuickUndo, &QToolButton::clicked, this, &MinimalHeaderBar::undoRequested);
    editLeftLayout->addWidget(m_btnQuickUndo, 0, Qt::AlignVCenter);

    // Visual Timeline / Slides button
    m_btnHistory = new QToolButton(m_editLeftWidget);
    m_btnHistory->setFixedSize(38, 30);
    m_btnHistory->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_btnHistory->setIconSize(QSize(21, 21));
    m_btnHistory->setCursor(Qt::PointingHandCursor);
    m_btnHistory->setToolTip(tr("History / Timeline"));
    connect(m_btnHistory, &QToolButton::clicked, this, &MinimalHeaderBar::showHistoryDialog);
    editLeftLayout->addWidget(m_btnHistory, 0, Qt::AlignVCenter);

    m_historyWidget = new PdfHistoryWidget(this);
    connect(m_historyWidget, &PdfHistoryWidget::stepSelected, this, &MinimalHeaderBar::historyStepRestoreRequested);

    m_editLeftWidget->hide();
    mainLayout->addWidget(m_editLeftWidget, 0, Qt::AlignVCenter);

    // =========================================================================
    // 2. CENTER: Stacked Container (Preview Title/Badge <-> Contextual Edit Tools)
    // =========================================================================
    m_centerStack = new DynamicStackedWidget(m_primaryBar);
    m_centerStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_centerOpacityEffect = new QGraphicsOpacityEffect(m_centerStack);
    m_centerOpacityEffect->setOpacity(1.0);
    m_centerStack->setGraphicsEffect(m_centerOpacityEffect);

    // Page 0: Preview View (Active File Name & Sky-Blue Glowing Pill Index Badge)
    m_previewCenterWidget = new QWidget(m_centerStack);
    m_previewCenterWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_previewCenterWidget->setMinimumWidth(0);
    auto *previewCenterLay = new QHBoxLayout(m_previewCenterWidget);
    previewCenterLay->setContentsMargins(4, 0, 4, 0);
    previewCenterLay->setSpacing(10);

    m_lblTitle = new QLabel(tr("BubezView"), m_previewCenterWidget);
    m_lblTitle->setObjectName("HeaderTitleLabel");
    m_lblTitle->setStyleSheet("font-weight: 600; font-size: 13.5px;");
    m_lblTitle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_lblTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lblTitle->setMinimumWidth(0);
    previewCenterLay->addWidget(m_lblTitle, 0, Qt::AlignVCenter);

    m_lblBadge = new QLabel(m_previewCenterWidget);
    m_lblBadge->setObjectName("HeaderIndexBadge");
    m_lblBadge->setFixedHeight(24);
    m_lblBadge->setAlignment(Qt::AlignCenter);
    m_lblBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_lblBadge->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lblBadge->setMinimumWidth(0);
    m_lblBadge->setVisible(false);
    previewCenterLay->addWidget(m_lblBadge, 0, Qt::AlignVCenter);
    previewCenterLay->addStretch(1);

    m_centerStack->addWidget(m_previewCenterWidget);

    // Page 1: Contextual Edit Tools Container
    m_editToolContainer = new QWidget(m_centerStack);
    setupEditTools();
    m_centerStack->addWidget(m_editToolContainer);

    // Page 2: Dedicated Page Organizer Tools Container
    m_pageOrganizerToolContainer = new QWidget(m_centerStack);
    setupPageOrganizerTools();
    m_centerStack->addWidget(m_pageOrganizerToolContainer);

    mainLayout->addWidget(m_centerStack, 1, Qt::AlignVCenter);

    // =========================================================================
    // 3. RIGHT: Unified Right Actions Capsule Island + Standalone Primary Edit Pill
    // =========================================================================
    m_rightActionsIslandFrame = new QFrame(m_primaryBar);
    m_rightActionsIslandFrame->setObjectName("RightActionsIsland");
    m_rightActionsIslandFrame->setFixedHeight(38);
    auto *rightIslandLayout = new QHBoxLayout(m_rightActionsIslandFrame);
    rightIslandLayout->setContentsMargins(4, 3, 4, 3);
    rightIslandLayout->setSpacing(2);

    m_btnSearch = new QToolButton(m_rightActionsIslandFrame);
    m_btnSearch->setFixedSize(38, 30);
    m_btnSearch->setIconSize(QSize(21, 21));
    m_btnSearch->setCursor(Qt::PointingHandCursor);
    m_btnSearch->setVisible(false);
    connect(m_btnSearch, &QToolButton::clicked, this, &MinimalHeaderBar::searchRequested);
    rightIslandLayout->addWidget(m_btnSearch, 0, Qt::AlignVCenter);

    // Modern Zoom controller button with instant popup preset menu & mouse wheel zooming
    m_btnZoom = new QToolButton(m_rightActionsIslandFrame);
    m_btnZoom->setFixedSize(38, 30);
    m_btnZoom->setIconSize(QSize(21, 21));
    m_btnZoom->setCursor(Qt::PointingHandCursor);
    m_btnZoom->setToolTip(tr("Zoom: 100% (Click for options, scroll wheel to zoom)"));
    m_btnZoom->setPopupMode(QToolButton::InstantPopup);
    m_btnZoom->installEventFilter(this);

    m_menuZoom = new QMenu(this);
    m_actHand = m_menuZoom->addAction(tr("✋ Pan / Hand Tool (H)"), this, &MinimalHeaderBar::handToolToggleRequested);
    m_actHand->setCheckable(true);
    m_actHand->setChecked(true);
    m_menuZoom->addSeparator();
    m_actZoomIn = m_menuZoom->addAction(tr("➕ Zoom In (Ctrl++)"), this, &MinimalHeaderBar::zoomInRequested);
    m_actZoomOut = m_menuZoom->addAction(tr("➖ Zoom Out (Ctrl+-)"), this, &MinimalHeaderBar::zoomOutRequested);
    m_menuZoom->addSeparator();
    m_actZoomFitWidth = m_menuZoom->addAction(tr("↔ Fit to Width (Ctrl+9)"), this, [this]() {
        emit zoomFitWidthRequested();
        emit zoomFitRequested();
    });
    m_actZoomFitPage = m_menuZoom->addAction(tr("🔍 Fit to Window / Page"), this, &MinimalHeaderBar::zoomFitPageRequested);
    m_actZoomFit = m_actZoomFitWidth;
    m_actZoom100 = m_menuZoom->addAction(tr("1:1 Actual Size 100% (Ctrl+0)"), this, &MinimalHeaderBar::zoomResetRequested);
    m_menuZoom->addSeparator();

    const QList<int> zoomPresets = {50, 75, 100, 125, 150, 200, 300, 400};
    for (int p : zoomPresets) {
        QAction *act = m_menuZoom->addAction(QStringLiteral("%1%").arg(p), this, [this, p]() {
            emit zoomSetPercentRequested(p);
        });
        act->setCheckable(true);
        if (p == 100) act->setChecked(true);
        m_presetZoomActions.append(act);
    }
    m_btnZoom->setMenu(m_menuZoom);
    rightIslandLayout->addWidget(m_btnZoom, 0, Qt::AlignVCenter);

    // Quick single Rotate button for image view mode (90° Clockwise)
    m_btnRotate = new QToolButton(m_rightActionsIslandFrame);
    m_btnRotate->setFixedSize(38, 30);
    m_btnRotate->setIconSize(QSize(21, 21));
    m_btnRotate->setCursor(Qt::PointingHandCursor);
    m_btnRotate->setToolTip(tr("Rotate 90° Clockwise (R)"));
    m_btnRotate->setVisible(false);
    connect(m_btnRotate, &QToolButton::clicked, this, &MinimalHeaderBar::rotateRightRequested);
    rightIslandLayout->addWidget(m_btnRotate, 0, Qt::AlignVCenter);

    // Direct Trash button to quickly discard/move files to trash
    m_btnTrash = new QToolButton(m_rightActionsIslandFrame);
    m_btnTrash->setFixedSize(38, 30);
    m_btnTrash->setIconSize(QSize(21, 21));
    m_btnTrash->setCursor(Qt::PointingHandCursor);
    m_btnTrash->setToolTip(tr("Move to Trash (Delete)"));
    m_btnTrash->setVisible(false);
    connect(m_btnTrash, &QToolButton::clicked, this, &MinimalHeaderBar::trashRequested);
    rightIslandLayout->addWidget(m_btnTrash, 0, Qt::AlignVCenter);

    m_btnInspector = new QToolButton(m_rightActionsIslandFrame);
    m_btnInspector->setFixedSize(38, 30);
    m_btnInspector->setIconSize(QSize(21, 21));
    m_btnInspector->setCursor(Qt::PointingHandCursor);
    m_btnInspector->setToolTip(tr("File Properties & Info (Ctrl+I)"));
    connect(m_btnInspector, &QToolButton::clicked, this, &MinimalHeaderBar::inspectorRequested);
    rightIslandLayout->addWidget(m_btnInspector, 0, Qt::AlignVCenter);

    m_btnShare = new QToolButton(m_rightActionsIslandFrame);
    m_btnShare->setFixedSize(38, 30);
    m_btnShare->setIconSize(QSize(21, 21));
    m_btnShare->setCursor(Qt::PointingHandCursor);
    m_btnShare->setToolTip(tr("Share..."));
    connect(m_btnShare, &QToolButton::clicked, this, &MinimalHeaderBar::shareRequested);
    rightIslandLayout->addWidget(m_btnShare, 0, Qt::AlignVCenter);

    m_btnSettings = new QToolButton(m_rightActionsIslandFrame);
    m_btnSettings->setFixedSize(38, 30);
    m_btnSettings->setIconSize(QSize(21, 21));
    m_btnSettings->setCursor(Qt::PointingHandCursor);
    m_btnSettings->setToolTip(tr("Preferences (Ctrl+,)"));
    connect(m_btnSettings, &QToolButton::clicked, this, &MinimalHeaderBar::settingsRequested);
    rightIslandLayout->addWidget(m_btnSettings, 0, Qt::AlignVCenter);

    // Export / Save button inside Right Actions Island
    m_btnExport = new QToolButton(m_rightActionsIslandFrame);
    m_btnExport->setFixedSize(38, 30);
    m_btnExport->setIconSize(QSize(21, 21));
    m_btnExport->setCursor(Qt::PointingHandCursor);
    m_btnExport->setToolTip(tr("Export Document (Ctrl+E)"));
    connect(m_btnExport, &QToolButton::clicked, this, [this]() {
        if (m_editMode) {
            emit saveExportRequested(m_btnExport->mapToGlobal(QPoint(0, m_btnExport->height())));
        } else {
            emit exportRequested();
        }
    });
    rightIslandLayout->addWidget(m_btnExport, 0, Qt::AlignVCenter);

    mainLayout->addWidget(m_rightActionsIslandFrame, 0, Qt::AlignVCenter);

    // Standalone Primary Edit / Done Accent Pill (50x38, vibrant blue gradient)
    m_btnEdit = new QPushButton(m_primaryBar);
    m_btnEdit->setFixedSize(50, 38);
    m_btnEdit->setIconSize(QSize(22, 22));
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    m_btnEdit->setToolTip(tr("Edit Mode (Ctrl+T)"));
    connect(m_btnEdit, &QPushButton::clicked, this, [this]() {
        if (m_pageOrganizerMode) {
            emit pdfOrganizerBackRequested();
        } else {
            emit editToggled(!m_editMode);
        }
    });
    mainLayout->addWidget(m_btnEdit, 0, Qt::AlignVCenter);

    rootLayout->addWidget(m_primaryBar);

    // Row 2: Contextual Temporary Action Bar (floats over PDF as an overlay HUD without changing layout)
    m_contextualBar = new ContextualActionBar(this);
    connect(m_contextualBar, &ContextualActionBar::closeRequested, this, &MinimalHeaderBar::hideContextualBar);
    connect(m_contextualBar, &ContextualActionBar::textReplaceRequested, this, &MinimalHeaderBar::pdfTextReplaceRequested);
    connect(m_contextualBar, &ContextualActionBar::textPropertiesChanged, this, &MinimalHeaderBar::pdfTextPropertiesChanged);
    connect(m_contextualBar, &ContextualActionBar::textBgChanged, this, &MinimalHeaderBar::pdfTextBgChanged);
    connect(m_contextualBar, &ContextualActionBar::textDoneRequested, this, &MinimalHeaderBar::pdfTextDoneRequested);
    m_contextualBar->hide();
}

void MinimalHeaderBar::setTitle(const QString &title)
{
    m_rawTitle = title;
    updateDisplayTitle();
    if (m_btnTrash && !m_editMode && !m_pageOrganizerMode) {
        m_btnTrash->setVisible(!m_rawTitle.isEmpty());
    }
}

void MinimalHeaderBar::updateDisplayTitle()
{
    if (!m_lblTitle) return;
    QString displayTitle = m_rawTitle.isEmpty() ? tr("BubezView") : m_rawTitle;

    int leftWidth = m_windowControlsWidget ? m_windowControlsWidget->sizeHint().width() : 140;
    int rightWidth = 0;
    if (m_rightActionsIslandFrame && m_rightActionsIslandFrame->isVisible()) {
        rightWidth += m_rightActionsIslandFrame->sizeHint().width();
    }
    if (m_btnEdit && m_btnEdit->isVisible()) {
        rightWidth += m_btnEdit->width();
    }
    rightWidth = qMax(rightWidth + 56, 220);
    int availableWidth = width() - leftWidth - rightWidth;
    if (m_lblBadge && m_lblBadge->isVisible()) {
        availableWidth -= (m_lblBadge->sizeHint().width() + 14);
    }
    int maxTitleWidth = qBound(60, availableWidth, 650);

    QFontMetrics fm(m_lblTitle->font());
    QString elided = fm.elidedText(displayTitle, Qt::ElideMiddle, maxTitleWidth);
    m_lblTitle->setText(elided);
    m_lblTitle->setToolTip(m_rawTitle);
}

void MinimalHeaderBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayTitle();
    updateContextualBarPosition();
}

void MinimalHeaderBar::setOverlayContainer(QWidget *container)
{
    m_overlayContainer = container;
    if (m_contextualBar && container) {
        m_contextualBar->setParent(container);
        m_contextualBar->hide();
    }
}

void MinimalHeaderBar::updateContextualBarPosition()
{
    if (!m_contextualBar) return;
    QWidget *parentW = m_contextualBar->parentWidget();
    int parentWidth = parentW ? parentW->width() : width();
    int barW = qBound(380, m_contextualBar->sizeHint().width() + 40, parentWidth - 40);
    int barH = 38;
    int x = (parentWidth - barW) / 2;
    int y = 54;
    m_contextualBar->setGeometry(x, y, barW, barH);
    m_contextualBar->raise();
}

void MinimalHeaderBar::animateContextualBar(bool show)
{
    if (!m_contextualBar) return;
    updateContextualBarPosition();
    if (show) {
        m_contextualBar->show();
        m_contextualBar->raise();
        QRect endGeom = m_contextualBar->geometry();
        QRect startGeom = endGeom;
        startGeom.moveTop(endGeom.top() - 14);
        auto *anim = new QPropertyAnimation(m_contextualBar, "geometry", this);
        anim->setDuration(160);
        anim->setStartValue(startGeom);
        anim->setEndValue(endGeom);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        m_contextualBar->hide();
    }
}

void MinimalHeaderBar::setIndexBadge(const QString &badgeText)
{
    if (badgeText.isEmpty()) {
        m_lblBadge->setVisible(false);
        m_lblBadge->setText(QString());
        if (m_btnPrev) m_btnPrev->setEnabled(false);
        if (m_btnNext) m_btnNext->setEnabled(false);
    } else {
        m_lblBadge->setText(badgeText);
        m_lblBadge->setVisible(!m_editMode);
        if (m_btnPrev) {
            m_btnPrev->setEnabled(true);
            m_btnPrev->setVisible(!m_editMode);
        }
        if (m_btnNext) {
            m_btnNext->setEnabled(true);
            m_btnNext->setVisible(!m_editMode);
        }
    }
    if (m_sepNavIsland) {
        m_sepNavIsland->setVisible(!m_editMode && !badgeText.isEmpty());
    }
    updateDisplayTitle();
}

void MinimalHeaderBar::showContextualTextBar(const QString &text, const QFont &font,
                                            const QColor &color, PdfTextBgMode bgMode,
                                            const QColor &bgColor)
{
    if (!m_editMode) return;
    if (m_pdfHighlightActive) {
        m_pdfHighlightActive = false;
        emit pdfHighlightToolToggled(false);
    }
    if (m_btnPdfText && !m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(true);
    }
    if (m_contextualBar) {
        if (!text.isEmpty()) {
            m_contextualBar->setTextContent(text);
        }
        if (font.pointSize() > 0 || font.pointSizeF() > 0.0) {
            m_contextualBar->setTextFont(font);
        }
        if (color.isValid()) {
            m_contextualBar->setTextColor(color);
        }
        m_contextualBar->setTextBgMode(bgMode);
        m_contextualBar->setTextBgColor(bgColor);
        m_contextualBar->setMode(ContextBarMode::Text);
    }
    animateContextualBar(true);
}

void MinimalHeaderBar::showContextualHighlightBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    m_pdfHighlightActive = true;
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Highlight);
    }
    animateContextualBar(true);
}

void MinimalHeaderBar::showContextualShapesBar(int shapeType)
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_pdfHighlightActive) {
        m_pdfHighlightActive = false;
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setActiveShapeType(shapeType);
        m_contextualBar->setMode(ContextBarMode::Shapes);
    }
    animateContextualBar(true);
}

void MinimalHeaderBar::showContextualRedactBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_pdfHighlightActive) {
        m_pdfHighlightActive = false;
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Redact);
    }
    animateContextualBar(true);
}

void MinimalHeaderBar::showContextualStampBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_pdfHighlightActive) {
        m_pdfHighlightActive = false;
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Stamp);
    }
    animateContextualBar(true);
}

void MinimalHeaderBar::showContextualDrawingBar(int toolId)
{
    if (!m_editMode) return;
    showContextualShapesBar(toolId);
}

void MinimalHeaderBar::hideContextualBar()
{
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
    }
    emit pdfTextToolToggled(false);

    if (m_pdfHighlightActive) {
        m_pdfHighlightActive = false;
    }
    emit pdfHighlightToolToggled(false);

    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Hidden);
    }
    animateContextualBar(false);
}

void MinimalHeaderBar::setHandToolChecked(bool checked)
{
    if (m_actHand) {
        m_actHand->setChecked(checked);
    }
}

void MinimalHeaderBar::setUiScale(int scalePercent)
{
    m_uiScale = qBound(65, scalePercent, 200) / 100.0;
    updateHeaderGeometry();
    updateButtonStyles();
}

void MinimalHeaderBar::updateHeaderGeometry()
{
    const int barH = qRound(54 * m_uiScale);
    const int islandH = qRound(38 * m_uiScale);
    const int btnH = qRound(30 * m_uiScale);
    const int btnW38 = qRound(38 * m_uiScale);
    const int btnW34 = qRound(34 * m_uiScale);
    const int iconSz = qRound(20 * m_uiScale);
    const int closeSz = qRound(36 * m_uiScale);
    const int closeIconSz = qRound(18 * m_uiScale);

    if (m_primaryBar) m_primaryBar->setFixedHeight(barH);
    setFixedHeight(barH);

    if (m_btnClose) {
        m_btnClose->setFixedSize(closeSz, closeSz);
        m_btnClose->setIconSize(QSize(closeIconSz, closeIconSz));
    }

    if (m_navIslandFrame) m_navIslandFrame->setFixedHeight(islandH);
    if (m_editLeftWidget) m_editLeftWidget->setFixedHeight(islandH);
    if (m_rightActionsIslandFrame) m_rightActionsIslandFrame->setFixedHeight(islandH);
    if (m_editToolsIslandFrame) m_editToolsIslandFrame->setFixedHeight(islandH);
    if (m_orgToolsIslandFrame) m_orgToolsIslandFrame->setFixedHeight(islandH);

    const QSize sz(iconSz, iconSz);
    if (m_btnPrev) { m_btnPrev->setFixedSize(btnW34, btnH); m_btnPrev->setIconSize(sz); }
    if (m_btnNext) { m_btnNext->setFixedSize(btnW34, btnH); m_btnNext->setIconSize(sz); }
    if (m_btnGallery) { m_btnGallery->setFixedSize(btnW38, btnH); m_btnGallery->setIconSize(sz); }
    if (m_btnSearch) { m_btnSearch->setFixedSize(btnW38, btnH); m_btnSearch->setIconSize(sz); }
    if (m_btnZoom) { m_btnZoom->setFixedSize(btnW38, btnH); m_btnZoom->setIconSize(sz); }
    if (m_btnRotate) { m_btnRotate->setFixedSize(btnW38, btnH); m_btnRotate->setIconSize(sz); }
    if (m_btnTrash) { m_btnTrash->setFixedSize(btnW38, btnH); m_btnTrash->setIconSize(sz); }
    if (m_btnInspector) { m_btnInspector->setFixedSize(btnW38, btnH); m_btnInspector->setIconSize(sz); }
    if (m_btnShare) { m_btnShare->setFixedSize(btnW38, btnH); m_btnShare->setIconSize(sz); }
    if (m_btnSettings) { m_btnSettings->setFixedSize(btnW38, btnH); m_btnSettings->setIconSize(sz); }
    if (m_btnExport) { m_btnExport->setFixedSize(btnW38, btnH); m_btnExport->setIconSize(sz); }
    if (m_btnDiscard) { m_btnDiscard->setFixedSize(btnW38, btnH); m_btnDiscard->setIconSize(sz); }
    if (m_btnQuickUndo) { m_btnQuickUndo->setFixedSize(btnW38, btnH); m_btnQuickUndo->setIconSize(sz); }
    if (m_btnHistory) { m_btnHistory->setFixedSize(btnW38, btnH); m_btnHistory->setIconSize(sz); }

    if (m_btnEdit) {
        m_btnEdit->setFixedSize(qRound(50 * m_uiScale), islandH);
        m_btnEdit->setIconSize(QSize(qRound(22 * m_uiScale), qRound(22 * m_uiScale)));
    }

    // Scale contextual edit buttons inside EditToolsIsland
    if (m_btnImgRotateLeft) { m_btnImgRotateLeft->setFixedSize(btnW38, btnH); m_btnImgRotateLeft->setIconSize(sz); }
    if (m_btnImgRotateRight) { m_btnImgRotateRight->setFixedSize(btnW38, btnH); m_btnImgRotateRight->setIconSize(sz); }
    if (m_btnImgFlipH) { m_btnImgFlipH->setFixedSize(btnW38, btnH); m_btnImgFlipH->setIconSize(sz); }
    if (m_btnImgFlipV) { m_btnImgFlipV->setFixedSize(btnW38, btnH); m_btnImgFlipV->setIconSize(sz); }
    if (m_btnImgCrop) { m_btnImgCrop->setFixedSize(btnW38, btnH); m_btnImgCrop->setIconSize(sz); }
    if (m_btnImgResize) { m_btnImgResize->setFixedSize(btnW38, btnH); m_btnImgResize->setIconSize(sz); }
    if (m_btnImgInvert) { m_btnImgInvert->setFixedSize(btnW38, btnH); m_btnImgInvert->setIconSize(sz); }
    if (m_btnImgInspector) { m_btnImgInspector->setFixedSize(btnW38, btnH); m_btnImgInspector->setIconSize(sz); }
    if (m_btnOfficeEditLo) { m_btnOfficeEditLo->setFixedSize(btnW38, btnH); m_btnOfficeEditLo->setIconSize(sz); }
    if (m_btnOfficeSavePdf) { m_btnOfficeSavePdf->setFixedSize(btnW38, btnH); m_btnOfficeSavePdf->setIconSize(sz); }
    if (m_btnOfficeExtractImages) { m_btnOfficeExtractImages->setFixedSize(btnW38, btnH); m_btnOfficeExtractImages->setIconSize(sz); }
    if (m_btnPdfText) { m_btnPdfText->setFixedSize(qRound(40 * m_uiScale), btnH); m_btnPdfText->setIconSize(sz); }
    if (m_btnPdfShapes) { m_btnPdfShapes->setFixedSize(qRound(50 * m_uiScale), btnH); m_btnPdfShapes->setIconSize(sz); }
    if (m_btnPdfSign) { m_btnPdfSign->setFixedSize(qRound(50 * m_uiScale), btnH); m_btnPdfSign->setIconSize(sz); }
    if (m_btnPdfPages) { m_btnPdfPages->setFixedSize(qRound(40 * m_uiScale), btnH); m_btnPdfPages->setIconSize(sz); }
    if (m_btnTxtNumbers) { m_btnTxtNumbers->setFixedSize(btnW38, btnH); m_btnTxtNumbers->setIconSize(sz); }
    if (m_btnTxtSyntax) { m_btnTxtSyntax->setFixedSize(btnW38, btnH); m_btnTxtSyntax->setIconSize(sz); }
    if (m_btnTxtWrap) { m_btnTxtWrap->setFixedSize(btnW38, btnH); m_btnTxtWrap->setIconSize(sz); }

    updateGeometry();
    updateContextualBarPosition();
}

void MinimalHeaderBar::setEditMode(bool editMode)
{
    m_editMode = editMode;

    if (!editMode) {
        hideContextualBar();
        updateHeaderGeometry();

        if (m_windowControlsWidget) m_windowControlsWidget->setVisible(m_windowControlsVisible);
        if (m_navIslandFrame) m_navIslandFrame->show();
        if (m_btnPrev) m_btnPrev->setVisible(!m_rawTitle.isEmpty());
        if (m_btnNext) m_btnNext->setVisible(!m_rawTitle.isEmpty());
        if (m_sepNavIsland) m_sepNavIsland->setVisible(!m_rawTitle.isEmpty());
        if (m_btnGallery) m_btnGallery->show();
        if (m_editLeftWidget) m_editLeftWidget->hide();

        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) m_btnZoom->hide();
        if (m_btnZoom) m_btnZoom->setVisible(m_currentCategory == FileCategory::Image || m_currentCategory == FileCategory::Pdf || m_currentCategory == FileCategory::Text || m_currentCategory == FileCategory::Office);
        if (m_btnRotate) m_btnRotate->setVisible(m_currentCategory == FileCategory::Image);
        if (m_btnTrash) m_btnTrash->setVisible(!m_rawTitle.isEmpty());
        if (m_btnInspector) m_btnInspector->show();
        if (m_btnShare) m_btnShare->show();
        if (m_btnSettings) m_btnSettings->show();
        if (m_btnExport) m_btnExport->show();
        if (m_rightActionsIslandFrame) m_rightActionsIslandFrame->show();

        if (m_btnEdit) {
            m_btnEdit->setText(QString());
            m_btnEdit->setToolTip(tr("Edit Mode (Ctrl+T)"));
        }

        // Exiting edit mode: switch immediately to preview page 0 so its minimumSizeHint
        // does not block pre-edit window geometry restoration.
        if (m_centerFadeAnim) {
            m_centerFadeAnim->stop();
        }
        if (m_centerStack) {
            m_centerStack->setCurrentIndex(0);
            m_centerStack->updateGeometry();
        }
        if (m_centerOpacityEffect) {
            m_centerOpacityEffect->setOpacity(1.0);
        }
    } else {
        if (m_primaryBar) m_primaryBar->setFixedHeight(54);
        updateHeaderGeometry();

        if (m_windowControlsWidget) m_windowControlsWidget->hide();
        if (m_editLeftWidget) m_editLeftWidget->show();

        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) {
            m_btnZoom->setVisible(m_currentCategory == FileCategory::Image || m_currentCategory == FileCategory::Pdf || m_currentCategory == FileCategory::Text || m_currentCategory == FileCategory::Office);
        }
        if (m_btnRotate) m_btnRotate->hide();
        if (m_btnTrash) m_btnTrash->hide();
        if (m_btnInspector) m_btnInspector->hide();
        if (m_btnShare) m_btnShare->hide();
        if (m_btnSettings) m_btnSettings->hide();
        if (m_btnExport) m_btnExport->show();
        if (m_rightActionsIslandFrame) m_rightActionsIslandFrame->show();

        if (m_btnEdit) {
            m_btnEdit->setText(QString());
            m_btnEdit->setToolTip(tr("Done / Exit Edit Mode (Esc)"));
        }

        startCenterTransition(1);
    }
    updateContextualToolsVisibility();
    updateButtonStyles();
    if (m_centerStack) {
        m_centerStack->updateGeometry();
    }
    updateGeometry();
}

void MinimalHeaderBar::setPageOrganizerMode(bool active)
{
    m_pageOrganizerMode = active;

    if (active) {
        hideContextualBar();
        updateHeaderGeometry();

        if (m_windowControlsWidget) m_windowControlsWidget->setVisible(m_windowControlsVisible);
        if (m_navIslandFrame) m_navIslandFrame->hide();
        if (m_editLeftWidget) {
            m_editLeftWidget->show();
            if (m_btnDiscard) m_btnDiscard->hide();
            if (m_btnQuickUndo) m_btnQuickUndo->show();
            if (m_btnHistory) m_btnHistory->hide();
        }

        if (m_btnPrev) m_btnPrev->hide();
        if (m_btnNext) m_btnNext->hide();
        if (m_sepNavIsland) m_sepNavIsland->hide();
        if (m_btnGallery) m_btnGallery->hide();
        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) {
            m_btnZoom->setVisible(m_currentCategory == FileCategory::Image || m_currentCategory == FileCategory::Pdf || m_currentCategory == FileCategory::Text || m_currentCategory == FileCategory::Office);
        }
        if (m_btnRotate) m_btnRotate->hide();
        if (m_btnTrash) m_btnTrash->hide();
        if (m_btnInspector) m_btnInspector->hide();
        if (m_btnShare) m_btnShare->hide();
        if (m_btnSettings) m_btnSettings->hide();
        if (m_btnExport) m_btnExport->hide();

        if (m_btnEdit) {
            m_btnEdit->setText(QString());
            m_btnEdit->setToolTip(tr("Return to document viewing and reading (Esc)"));
        }

        if (m_centerFadeAnim) {
            m_centerFadeAnim->stop();
        }
        if (m_centerStack) {
            m_centerStack->setCurrentIndex(2);
            m_centerStack->updateGeometry();
        }
        if (m_centerOpacityEffect) {
            m_centerOpacityEffect->setOpacity(1.0);
        }
    } else {
        if (m_navIslandFrame) m_navIslandFrame->show();
        if (m_btnDiscard) m_btnDiscard->show();
        if (m_btnHistory) m_btnHistory->show();
        if (m_btnSettings) m_btnSettings->show();
        if (m_btnExport) m_btnExport->show();
        setEditMode(m_editMode);
    }

    if (m_centerStack) {
        m_centerStack->updateGeometry();
    }
    updateGeometry();
}

void MinimalHeaderBar::setCategory(FileCategory category)
{
    m_currentCategory = category;
    updateContextualToolsVisibility();
    if (m_centerStack) {
        m_centerStack->updateGeometry();
    }
    updateGeometry();
}

void MinimalHeaderBar::setTextEditStates(bool lineNumbers, bool syntax, bool wordWrap)
{
    if (m_btnTxtNumbers) {
        m_btnTxtNumbers->blockSignals(true);
        m_btnTxtNumbers->setChecked(lineNumbers);
        m_btnTxtNumbers->blockSignals(false);
    }
    if (m_btnTxtSyntax) {
        m_btnTxtSyntax->blockSignals(true);
        m_btnTxtSyntax->setChecked(syntax);
        m_btnTxtSyntax->blockSignals(false);
    }
    if (m_btnTxtWrap) {
        m_btnTxtWrap->blockSignals(true);
        m_btnTxtWrap->setChecked(wordWrap);
        m_btnTxtWrap->blockSignals(false);
    }
}

void MinimalHeaderBar::startCenterTransition(int targetIndex)
{
    if (!m_centerStack || m_centerStack->currentIndex() == targetIndex) return;

    if (QGuiApplication::platformName() == "offscreen" || !isVisible()) {
        m_centerStack->setCurrentIndex(targetIndex);
        m_centerStack->updateGeometry();
        updateGeometry();
        return;
    }

    if (!m_centerFadeAnim) {
        m_centerFadeAnim = new QPropertyAnimation(m_centerOpacityEffect, "opacity", this);
        m_centerFadeAnim->setDuration(120);
    }
    m_centerFadeAnim->stop();
    m_centerFadeAnim->setStartValue(1.0);
    m_centerFadeAnim->setEndValue(0.0);

    auto *conn = new QMetaObject::Connection();
    *conn = connect(m_centerFadeAnim, &QPropertyAnimation::finished, this, [this, targetIndex, conn]() {
        disconnect(*conn);
        delete conn;
        m_centerStack->setCurrentIndex(targetIndex);
        m_centerStack->updateGeometry();
        updateGeometry();
        m_centerFadeAnim->setStartValue(0.0);
        m_centerFadeAnim->setEndValue(1.0);
        m_centerFadeAnim->start();
    });
    m_centerFadeAnim->start();
}

void MinimalHeaderBar::setupEditTools()
{
    auto *outerLay = new QHBoxLayout(m_editToolContainer);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // Left stretch to dynamically center the Edit Tools Capsule Island over document content
    outerLay->addStretch(1);

    m_editToolsIslandFrame = new QFrame(m_editToolContainer);
    m_editToolsIslandFrame->setObjectName("EditToolsIsland");
    m_editToolsIslandFrame->setFixedHeight(38);
    auto *lay = new QHBoxLayout(m_editToolsIslandFrame);
    lay->setContentsMargins(5, 3, 5, 3);
    lay->setSpacing(4);

    auto makeToolBtn = [this, lay](QToolButton *&btn, const QString &tip) {
        btn = new QToolButton(m_editToolsIslandFrame);
        btn->setFixedSize(38, 30);
        btn->setIconSize(QSize(21, 21));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    auto makePillBtn = [this, lay](QPushButton *&btn, const QString &text, const QString &tip) {
        btn = new QPushButton(m_editToolsIslandFrame);
        btn->setFixedHeight(30);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(text);
        btn->setToolTip(tip);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    auto makeSeparator = [this, lay](QFrame *&sep) {
        sep = new QFrame(m_editToolsIslandFrame);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setFixedWidth(1);
        sep->setFixedHeight(18);
        lay->addWidget(sep, 0, Qt::AlignVCenter);
    };

    // --- Image Tools ---
    // Cluster 1: Geometric transformations
    makeToolBtn(m_btnImgRotateLeft, tr("Rotate 90° CCW (Ctrl+Shift+R)"));
    connect(m_btnImgRotateLeft, &QToolButton::clicked, this, &MinimalHeaderBar::rotateLeftRequested);

    makeToolBtn(m_btnImgRotateRight, tr("Rotate 90° CW (Ctrl+R)"));
    connect(m_btnImgRotateRight, &QToolButton::clicked, this, &MinimalHeaderBar::rotateRightRequested);

    makeToolBtn(m_btnImgFlipH, tr("Flip Horizontal"));
    connect(m_btnImgFlipH, &QToolButton::clicked, this, &MinimalHeaderBar::flipHRequested);

    makeToolBtn(m_btnImgFlipV, tr("Flip Vertical"));
    connect(m_btnImgFlipV, &QToolButton::clicked, this, &MinimalHeaderBar::flipVRequested);

    makeSeparator(m_sepImg1);

    // Cluster 2: Crop & Adjustments
    makeToolBtn(m_btnImgCrop, tr("Crop Selection (C)"));
    connect(m_btnImgCrop, &QToolButton::clicked, this, &MinimalHeaderBar::cropRequested);

    makeSeparator(m_sepImg2);

    makeToolBtn(m_btnImgResize, tr("Resize Image (Ctrl+Shift+I)"));
    connect(m_btnImgResize, &QToolButton::clicked, this, &MinimalHeaderBar::imageResizeRequested);

    makeToolBtn(m_btnImgInvert, tr("Invert Colors"));
    connect(m_btnImgInvert, &QToolButton::clicked, this, &MinimalHeaderBar::imageInvertRequested);

    makeToolBtn(m_btnImgInspector, tr("Image Inspector / EXIF (I)"));
    connect(m_btnImgInspector, &QToolButton::clicked, this, &MinimalHeaderBar::imageInspectorRequested);

    // --- Office Tools ---
    makeToolBtn(m_btnOfficeEditLo, tr("Open in LibreOffice"));
    connect(m_btnOfficeEditLo, &QToolButton::clicked, this, &MinimalHeaderBar::officeEditLoRequested);

    makeToolBtn(m_btnOfficeSavePdf, tr("Export Document as PDF"));
    connect(m_btnOfficeSavePdf, &QToolButton::clicked, this, &MinimalHeaderBar::officeSavePdfRequested);

    makeToolBtn(m_btnOfficeExtractImages, tr("Extract Embedded Images"));
    connect(m_btnOfficeExtractImages, &QToolButton::clicked, this, &MinimalHeaderBar::officeExtractImagesRequested);

    makeSeparator(m_sepOffice1);

    // --- PDF Tools: 4 Clean Central Pills inside EditToolsIsland ---
    // Pill 1: Text
    m_btnPdfText = new QPushButton(m_editToolsIslandFrame);
    m_btnPdfText->setFixedSize(40, 30);
    m_btnPdfText->setIconSize(QSize(21, 21));
    m_btnPdfText->setCheckable(true);
    m_btnPdfText->setCursor(Qt::PointingHandCursor);
    m_btnPdfText->setToolTip(tr("Add text (T)"));
    lay->addWidget(m_btnPdfText, 0, Qt::AlignVCenter);

    connect(m_btnPdfText, &QPushButton::toggled, this, [this](bool checked) {
        bool isLight = ThemeManager::isLight(m_currentPreset);
        QColor textIconColor = checked ? QColor("#ffffff") : (isLight ? QColor("#0f172a") : QColor("#f1f5f9"));
        m_btnPdfText->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorText, textIconColor, 21));
        if (checked) {
            if (m_pdfHighlightActive) {
                m_pdfHighlightActive = false;
                emit pdfHighlightToolToggled(false);
            }
            showContextualTextBar();
            emit pdfTextToolToggled(true);
        } else {
            if (m_contextualBar && m_contextualBar->mode() == ContextBarMode::Text) {
                hideContextualBar();
            }
            emit pdfTextToolToggled(false);
        }
    });

    // Pill 2: Insert ▾ (Geometric shapes, images, highlighter, and rubber stamps)
    m_btnPdfShapes = new QToolButton(m_editToolsIslandFrame);
    m_btnPdfShapes->setFixedSize(50, 30);
    m_btnPdfShapes->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnPdfShapes->setIconSize(QSize(21, 21));
    m_btnPdfShapes->setCursor(Qt::PointingHandCursor);
    m_btnPdfShapes->setText(QStringLiteral("▾"));
    m_btnPdfShapes->setToolTip(tr("Insert shapes and images"));
    m_btnPdfShapes->setPopupMode(QToolButton::InstantPopup);

    m_menuShapes = new QMenu(this);
    m_menuShapes->addAction(tr("📐 Geometric Shapes"), this, [this]() {
        showContextualShapesBar(0);
        emit pdfShapesToolRequested(0);
    });
    m_menuShapes->addAction(tr("🖼️ Insert Image..."), this, &MinimalHeaderBar::pdfInsertImageRequested);
    m_menuShapes->addAction(tr("🖍️ Highlight"), this, [this]() {
        if (m_btnPdfText && m_btnPdfText->isChecked()) {
            QSignalBlocker blocker(m_btnPdfText);
            m_btnPdfText->setChecked(false);
            emit pdfTextToolToggled(false);
        }
        showContextualHighlightBar();
        emit pdfHighlightToolToggled(true);
    });
    m_menuShapes->addAction(tr("💮 Rubber Stamp"), this, [this]() {
        showContextualStampBar();
        emit pdfStampToolRequested();
    });

    m_btnPdfShapes->setMenu(m_menuShapes);
    lay->addWidget(m_btnPdfShapes, 0, Qt::AlignVCenter);

    // Pill 4: Signatures
    m_btnPdfSign = new QToolButton(m_editToolsIslandFrame);
    m_btnPdfSign->setFixedSize(50, 30);
    m_btnPdfSign->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnPdfSign->setIconSize(QSize(21, 21));
    m_btnPdfSign->setCursor(Qt::PointingHandCursor);
    m_btnPdfSign->setText(QStringLiteral("▾"));
    m_btnPdfSign->setToolTip(tr("Sign document"));
    m_btnPdfSign->setPopupMode(QToolButton::InstantPopup);

    m_menuSignature = new QMenu(this);
    m_menuSignature->addAction(tr("✒️ Manage & Place Signatures..."), this, &MinimalHeaderBar::pdfSignatureRequested);
    m_menuSignature->addSeparator();
    m_menuSignature->addAction(tr("🔐 Cryptographic Signature (PAdES)..."), this, &MinimalHeaderBar::pdfCryptoSignatureRequested);
    m_menuSignature->addAction(tr("🔍 Find Signature Fields in Document..."), this, &MinimalHeaderBar::pdfScanSignatureFieldsRequested);

    m_btnPdfSign->setMenu(m_menuSignature);
    lay->addWidget(m_btnPdfSign, 0, Qt::AlignVCenter);

    // Pill 5: Direct Visual Page Organizer
    m_btnPdfPages = new QToolButton(m_editToolsIslandFrame);
    m_btnPdfPages->setFixedSize(40, 30);
    m_btnPdfPages->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_btnPdfPages->setIconSize(QSize(21, 21));
    m_btnPdfPages->setCursor(Qt::PointingHandCursor);
    if (m_btnPdfPages) {
        m_btnPdfPages->setToolTip(tr("Manage pages"));
        connect(m_btnPdfPages, &QToolButton::clicked, this, &MinimalHeaderBar::pdfPagesRequested);
        lay->addWidget(m_btnPdfPages, 0, Qt::AlignVCenter);
    }

    // --- Archive Tools ---
    makePillBtn(m_btnArcExtract, tr("Extract All"), tr("Extract all files from archive"));
    connect(m_btnArcExtract, &QPushButton::clicked, this, &MinimalHeaderBar::archiveExtractRequested);

    makePillBtn(m_btnArcAdd, tr("Add Files..."), tr("Add files or folders to archive"));
    connect(m_btnArcAdd, &QPushButton::clicked, this, &MinimalHeaderBar::archiveAddRequested);

    makePillBtn(m_btnArcRemove, tr("Remove"), tr("Remove selected items from archive"));
    connect(m_btnArcRemove, &QPushButton::clicked, this, &MinimalHeaderBar::archiveRemoveRequested);

    makeSeparator(m_sepArc1);

    makePillBtn(m_btnArcTest, tr("Test"), tr("Test archive integrity"));
    connect(m_btnArcTest, &QPushButton::clicked, this, &MinimalHeaderBar::archiveTestRequested);

    // --- Text Tools ---
    makeToolBtn(m_btnTxtNumbers, tr("Toggle Line Numbers"));
    m_btnTxtNumbers->setCheckable(true);
    connect(m_btnTxtNumbers, &QToolButton::toggled, this, &MinimalHeaderBar::textLineNumbersToggled);

    makeToolBtn(m_btnTxtSyntax, tr("Toggle Syntax Highlighting"));
    m_btnTxtSyntax->setCheckable(true);
    connect(m_btnTxtSyntax, &QToolButton::toggled, this, &MinimalHeaderBar::textSyntaxToggled);

    makeToolBtn(m_btnTxtWrap, tr("Toggle Word Wrap"));
    m_btnTxtWrap->setCheckable(true);
    connect(m_btnTxtWrap, &QToolButton::toggled, this, &MinimalHeaderBar::textWordWrapToggled);

    // --- Audio Tools (1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags) ---
    m_audToolsGroup = new QButtonGroup(this);
    m_audToolsGroup->setExclusive(true);

    makePillBtn(m_btnAudStudio, tr("Trim"), tr("Audio Trimming && Fade-In/Out"));
    m_btnAudStudio->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudStudio, 1);
    connect(m_btnAudStudio, &QPushButton::clicked, this, &MinimalHeaderBar::audioStudioRequested);

    makePillBtn(m_btnAudRestore, tr("Filters"), tr("Noise Reduction, FFT Spectrum && Dynamics"));
    m_btnAudRestore->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudRestore, 2);
    connect(m_btnAudRestore, &QPushButton::clicked, this, &MinimalHeaderBar::audioRestoreRequested);

    makePillBtn(m_btnAudPitch, tr("Pitch && Ch."), tr("Channels, Pitch Shift && Time Stretch"));
    m_btnAudPitch->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudPitch, 3);
    connect(m_btnAudPitch, &QPushButton::clicked, this, &MinimalHeaderBar::audioPitchRequested);

    makePillBtn(m_btnAudEq, tr("Equalizer"), tr("10-Band Track Equalizer"));
    m_btnAudEq->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudEq, 4);
    connect(m_btnAudEq, &QPushButton::clicked, this, &MinimalHeaderBar::audioEqualizerRequested);

    makePillBtn(m_btnAudTags, tr("Tags"), tr("Edit Audio Metadata && Album Cover"));
    m_btnAudTags->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudTags, 5);
    connect(m_btnAudTags, &QPushButton::clicked, this, &MinimalHeaderBar::audioTagsRequested);

    makePillBtn(m_btnAudSummary, tr("Summary"), tr("Review Active Changes, Safety Check && Save"));
    m_btnAudSummary->setCheckable(true);
    m_audToolsGroup->addButton(m_btnAudSummary, 6);
    connect(m_btnAudSummary, &QPushButton::clicked, this, &MinimalHeaderBar::audioSummaryRequested);

    m_btnAudStudio->setChecked(true);

    // --- Folder Studio Tools (0=Duplicates, 1=Recovery, 2=Organize, 3=Batch) ---
    m_fldToolsGroup = new QButtonGroup(this);
    m_fldToolsGroup->setExclusive(true);

    makePillBtn(m_btnFldDuplicates, tr("Duplicates"), tr("Find exact and multi-format equivalent files"));
    m_btnFldDuplicates->setCheckable(true);
    m_fldToolsGroup->addButton(m_btnFldDuplicates, 0);
    connect(m_btnFldDuplicates, &QPushButton::clicked, this, [this]() { emit folderStudioToolRequested(0); });

    makePillBtn(m_btnFldRecovery, tr("Recovery"), tr("PhotoRec / TestDisk signature repair, integrity check && smart naming"));
    m_btnFldRecovery->setCheckable(true);
    m_fldToolsGroup->addButton(m_btnFldRecovery, 1);
    connect(m_btnFldRecovery, &QPushButton::clicked, this, [this]() { emit folderStudioToolRequested(1); });

    makePillBtn(m_btnFldOrganize, tr("Organize"), tr("Smart folder tree organization && flattening with Dry-Run preview"));
    m_btnFldOrganize->setCheckable(true);
    m_fldToolsGroup->addButton(m_btnFldOrganize, 2);
    connect(m_btnFldOrganize, &QPushButton::clicked, this, [this]() { emit folderStudioToolRequested(2); });

    makePillBtn(m_btnFldBatch, tr("Batch"), tr("Token-based batch rename && empty folder / junk cleaner"));
    m_btnFldBatch->setCheckable(true);
    m_fldToolsGroup->addButton(m_btnFldBatch, 3);
    connect(m_btnFldBatch, &QPushButton::clicked, this, [this]() { emit folderStudioToolRequested(3); });

    m_btnFldDuplicates->setChecked(true);

    outerLay->addWidget(m_editToolsIslandFrame, 0, Qt::AlignVCenter);
    // Right stretch to balance the centering
    outerLay->addStretch(1);
    updateContextualToolsVisibility();
}

void MinimalHeaderBar::setupPageOrganizerTools()
{
    auto *outerLay = new QHBoxLayout(m_pageOrganizerToolContainer);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    outerLay->addStretch(1);

    m_orgToolsIslandFrame = new QFrame(m_pageOrganizerToolContainer);
    m_orgToolsIslandFrame->setObjectName("OrgToolsIsland");
    m_orgToolsIslandFrame->setFixedHeight(38);
    auto *lay = new QHBoxLayout(m_orgToolsIslandFrame);
    lay->setContentsMargins(5, 3, 5, 3);
    lay->setSpacing(4);

    auto makePill = [this, lay](QPushButton *&btn, const QString &text, const QString &tip, auto slot) {
        btn = new QPushButton(m_orgToolsIslandFrame);
        btn->setFixedHeight(30);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(text);
        btn->setToolTip(tip);
        btn->setMinimumWidth(btn->fontMetrics().horizontalAdvance(text) + 38);
        connect(btn, &QPushButton::clicked, this, slot);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    makePill(m_btnOrgRotate, tr("Rotate"), tr("Rotate selected pages 90° clockwise"), &MinimalHeaderBar::pdfOrganizerRotateRequested);
    makePill(m_btnOrgDelete, tr("Delete"), tr("Remove selected pages from document (Del)"), &MinimalHeaderBar::pdfOrganizerDeleteRequested);

    makePill(m_btnOrgInsertBlank, tr("Blank"), tr("Insert an empty blank page after selected item"), &MinimalHeaderBar::pdfOrganizerInsertBlankRequested);
    makePill(m_btnOrgAddPdf, tr("Add PDF..."), tr("Append or insert pages from another PDF file"), &MinimalHeaderBar::pdfOrganizerAddPdfRequested);

    makePill(m_btnOrgReverse, tr("Reverse"), tr("Invert the overall page sequence"), &MinimalHeaderBar::pdfOrganizerReverseRequested);
    makePill(m_btnOrgReset, tr("Reset"), tr("Revert all pages and rotations to their original state"), &MinimalHeaderBar::pdfOrganizerResetRequested);

    makePill(m_btnOrgRepair, tr("Repair PDF"), tr("Repair damaged PDF structure, corrupted streams or rebuild cross-reference tables"), &MinimalHeaderBar::pdfRepairRequested);

    outerLay->addWidget(m_orgToolsIslandFrame, 0, Qt::AlignVCenter);
    outerLay->addStretch(1);
}

void MinimalHeaderBar::updateContextualToolsVisibility()
{
    const bool isImg = (m_currentCategory == FileCategory::Image);
    const bool isPdf = (m_currentCategory == FileCategory::Pdf);
    const bool isArc = (m_currentCategory == FileCategory::Archive);
    const bool isTxt = (m_currentCategory == FileCategory::Text);
    const bool isAud = (m_currentCategory == FileCategory::Media) && m_audioToolsAvailable;
    const bool isOff = (m_currentCategory == FileCategory::Office);
    const bool isFld = (m_currentCategory == FileCategory::Folder);

    if (m_btnImgRotateLeft) m_btnImgRotateLeft->setVisible(isImg);
    if (m_btnImgRotateRight) m_btnImgRotateRight->setVisible(isImg);
    if (m_btnImgFlipH) m_btnImgFlipH->setVisible(isImg);
    if (m_btnImgFlipV) m_btnImgFlipV->setVisible(isImg);
    if (m_btnImgCrop) m_btnImgCrop->setVisible(isImg);
    if (m_btnImgResize) m_btnImgResize->setVisible(isImg);
    if (m_btnImgInvert) m_btnImgInvert->setVisible(isImg);
    if (m_btnImgInspector) m_btnImgInspector->setVisible(isImg);

    if (m_btnOfficeEditLo) m_btnOfficeEditLo->setVisible(isOff);
    if (m_btnOfficeSavePdf) m_btnOfficeSavePdf->setVisible(isOff);
    if (m_btnOfficeExtractImages) m_btnOfficeExtractImages->setVisible(isOff);

    if (m_btnPdfText) m_btnPdfText->setVisible(isPdf);
    if (m_btnPdfShapes) m_btnPdfShapes->setVisible(isPdf);
    if (m_btnPdfSign) m_btnPdfSign->setVisible(isPdf);
    if (m_btnPdfPages) m_btnPdfPages->setVisible(isPdf);
    if (m_btnExport && !m_pageOrganizerMode) m_btnExport->show();

    if (m_sepImg1) m_sepImg1->setVisible(isImg);
    if (m_sepImg2) m_sepImg2->setVisible(isImg);
    if (m_sepOffice1) m_sepOffice1->setVisible(isOff);
    if (m_sepArc1) m_sepArc1->setVisible(isArc);

    if (m_btnArcExtract) m_btnArcExtract->setVisible(isArc);
    if (m_btnArcAdd) m_btnArcAdd->setVisible(isArc);
    if (m_btnArcRemove) m_btnArcRemove->setVisible(isArc);
    if (m_btnArcTest) m_btnArcTest->setVisible(isArc);

    if (m_btnTxtNumbers) m_btnTxtNumbers->setVisible(isTxt);
    if (m_btnTxtSyntax) m_btnTxtSyntax->setVisible(isTxt);
    if (m_btnTxtWrap) m_btnTxtWrap->setVisible(isTxt);

    if (m_btnAudStudio) m_btnAudStudio->setVisible(isAud);
    if (m_btnAudRestore) m_btnAudRestore->setVisible(isAud);
    if (m_btnAudPitch) m_btnAudPitch->setVisible(isAud);
    if (m_btnAudEq) m_btnAudEq->setVisible(isAud);
    if (m_btnAudTags) m_btnAudTags->setVisible(isAud);
    if (m_btnAudSummary) m_btnAudSummary->setVisible(isAud);

    if (m_btnFldDuplicates) m_btnFldDuplicates->setVisible(isFld);
    if (m_btnFldRecovery) m_btnFldRecovery->setVisible(isFld);
    if (m_btnFldOrganize) m_btnFldOrganize->setVisible(isFld);
    if (m_btnFldBatch) m_btnFldBatch->setVisible(isFld);

    if (m_editToolsIslandFrame) {
        m_editToolsIslandFrame->setVisible(isImg || isPdf || isArc || isTxt || isAud || isOff || isFld);
    }

    // In Folder Edit Mode, hide file-save/export island and document-undo buttons since Studio tools apply directly with Undo
    if (m_editMode && !m_pageOrganizerMode) {
        if (isFld) {
            if (m_btnExport) m_btnExport->hide();
            if (m_rightActionsIslandFrame) m_rightActionsIslandFrame->hide();
            if (m_btnQuickUndo) m_btnQuickUndo->hide();
            if (m_btnHistory) m_btnHistory->hide();
        } else {
            if (m_btnQuickUndo) m_btnQuickUndo->show();
            if (m_btnHistory) m_btnHistory->show();
        }
    }

    if (m_btnZoom && !m_pageOrganizerMode) {
        m_btnZoom->setVisible(isImg || isPdf || isTxt || (m_currentCategory == FileCategory::Office));
    }
    if (m_btnRotate && !m_editMode && !m_pageOrganizerMode) {
        m_btnRotate->setVisible(isImg);
    }
    if (m_btnTrash && !m_editMode && !m_pageOrganizerMode) {
        m_btnTrash->setVisible(!m_rawTitle.isEmpty());
    }
}

void MinimalHeaderBar::setWindowControlsVisible(bool visible)
{
    m_windowControlsVisible = visible;
    if (m_btnClose) {
        m_btnClose->setVisible(visible);
    }
}

void MinimalHeaderBar::setSearchAvailable(bool available)
{
    if (m_btnSearch) {
        m_btnSearch->setVisible(available);
    }
}

void MinimalHeaderBar::applyThemePreset(ThemePreset preset)
{
    m_currentPreset = ThemeManager::effectivePreset(preset);
    updateButtonStyles();
    update();
}

void MinimalHeaderBar::updateButtonStyles()
{
    const bool isLight = (m_currentPreset == ThemePreset::ModernLight || m_currentPreset == ThemePreset::BreezeLight);
    const bool isZinc = (m_currentPreset == ThemePreset::ZincGray);
    const QColor iconColor = isLight ? QColor("#0f172a") : QColor("#f1f5f9");
    const int islandRadius = qBound(6, qRound(14 * m_uiScale), 20);

    // 1. Liquid Glass Capsule Island Frame Style (scaled dynamically with uiScale)
    QString islandFrameStyle;
    if (isLight) {
        islandFrameStyle = QString(
            "QFrame#NavIslandFrame, QFrame#EditLeftIsland, QFrame#EditToolsIsland, QFrame#OrgToolsIsland, QFrame#RightActionsIsland {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.85), stop:1 rgba(232, 236, 242, 0.75));"
            "   border: 1px solid rgba(15, 23, 42, 0.12);"
            "   border-radius: %1px;"
            "}"
        ).arg(islandRadius);
    } else if (isZinc) {
        islandFrameStyle = QString(
            "QFrame#NavIslandFrame, QFrame#EditLeftIsland, QFrame#EditToolsIsland, QFrame#OrgToolsIsland, QFrame#RightActionsIsland {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e);"
            "   border: 1px solid rgba(255, 255, 255, 0.20);"
            "   border-radius: %1px;"
            "}"
        ).arg(islandRadius);
    } else {
        islandFrameStyle = QString(
            "QFrame#NavIslandFrame, QFrame#EditLeftIsland, QFrame#EditToolsIsland, QFrame#OrgToolsIsland, QFrame#RightActionsIsland {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.085), stop:1 rgba(255, 255, 255, 0.04));"
            "   border: 1px solid rgba(255, 255, 255, 0.11);"
            "   border-radius: %1px;"
            "}"
        ).arg(islandRadius);
    }

    if (m_navIslandFrame) m_navIslandFrame->setStyleSheet(islandFrameStyle);
    if (m_editLeftWidget) m_editLeftWidget->setStyleSheet(islandFrameStyle);
    if (m_editToolsIslandFrame) m_editToolsIslandFrame->setStyleSheet(islandFrameStyle);
    if (m_orgToolsIslandFrame) m_orgToolsIslandFrame->setStyleSheet(islandFrameStyle);
    if (m_rightActionsIslandFrame) m_rightActionsIslandFrame->setStyleSheet(islandFrameStyle);

    // 2. Title & Glowing Sky-Blue Index Pill Badge [12/25]
    QString titleColor = isLight ? "#0f172a" : "#f8fafc";
    QString badgeBg = isLight ? "rgba(2, 132, 199, 0.12)" : "rgba(56, 189, 248, 0.16)";
    QString badgeBorder = isLight ? "rgba(2, 132, 199, 0.32)" : "rgba(56, 189, 248, 0.38)";
    QString badgeColor = isLight ? "#0284c7" : "#7dd3fc";

    m_lblTitle->setStyleSheet(QString("color: %1; font-weight: 600; font-size: 13.5px; padding-left: 2px;").arg(titleColor));
    m_lblBadge->setStyleSheet(QString(
        "background-color: %1; color: %2; border: 1px solid %3; border-radius: 12px; padding: 0px 10px; font-size: 11px; font-weight: 700;"
    ).arg(badgeBg, badgeColor, badgeBorder));

    // 3. Borderless Inner Island Buttons (smooth 10px radius inside the 14px capsule islands)
    QString innerHoverBg = isLight ? "rgba(15, 23, 42, 0.08)" : "rgba(255, 255, 255, 0.12)";
    QString innerPressedBg = isLight ? "rgba(15, 23, 42, 0.14)" : "rgba(255, 255, 255, 0.18)";
    QString textColor = isLight ? "#0f172a" : "#f1f5f9";

    QString islandInnerBtnStyle = QString(
        "QToolButton, QPushButton {"
        "   background: transparent;"
        "   color: %1;"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 0px;"
        "   font-size: 11.5px;"
        "   font-weight: 600;"
        "}"
        "QToolButton:hover, QPushButton:hover {"
        "   background-color: %2;"
        "}"
        "QToolButton:pressed, QPushButton:pressed {"
        "   background-color: %3;"
        "}"
        "QToolButton:checked, QPushButton:checked {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff;"
        "}"
        "QToolButton:disabled, QPushButton:disabled {"
        "   opacity: 0.30;"
        "}"
        "QToolButton::menu-indicator {"
        "   image: none;"
        "   width: 0px;"
        "}"
    ).arg(textColor, innerHoverBg, innerPressedBg);

    IconStyle iconStyle = ThemeManager::iconStyle();

    auto getIconColor = [&](VectorIcons::UiIcon iconType) -> QColor {
        switch (iconStyle) {
        case IconStyle::Yellow:
            return QColor("#fbbf24"); // Giallo ambra
        case IconStyle::Red:
            return QColor("#f87171"); // Rosso corallo
        case IconStyle::Green:
            return QColor("#34d399"); // Verde smeraldo
        case IconStyle::Blue:
            return QColor("#38bdf8"); // Azzurro cielo
        case IconStyle::ColorAccents: {
            switch (iconType) {
            case VectorIcons::UiIcon::Close:
            case VectorIcons::UiIcon::Trash:
            case VectorIcons::UiIcon::EditorDiscard:
                return QColor("#f87171"); // Red / Coral
            case VectorIcons::UiIcon::ChevronLeft:
            case VectorIcons::UiIcon::ChevronRight:
            case VectorIcons::UiIcon::EditorText:
            case VectorIcons::UiIcon::EditorUndo:
                return QColor("#38bdf8"); // Sky blue
            case VectorIcons::UiIcon::Grid:
            case VectorIcons::UiIcon::EditorPages:
                return QColor("#a78bfa"); // Purple
            case VectorIcons::UiIcon::Search:
                return QColor("#22d3ee"); // Cyan
            case VectorIcons::UiIcon::Zoom:
                return QColor("#06b6d4"); // Teal
            case VectorIcons::UiIcon::RotateRight:
            case VectorIcons::UiIcon::RotateLeft:
                return QColor("#818cf8"); // Indigo
            case VectorIcons::UiIcon::Info:
                return QColor("#c084fc"); // Violet
            case VectorIcons::UiIcon::Share:
            case VectorIcons::UiIcon::EditorSign:
                return QColor("#34d399"); // Emerald / Mint
            case VectorIcons::UiIcon::Settings:
                return isLight ? QColor("#64748b") : QColor("#94a3b8");
            case VectorIcons::UiIcon::EditorExport:
            case VectorIcons::UiIcon::EditorSave:
                return QColor("#10b981"); // Green
            case VectorIcons::UiIcon::EditorInsert:
                return QColor("#fb923c"); // Orange
            case VectorIcons::UiIcon::EditorTimeline:
            case VectorIcons::UiIcon::Calendar:
                return QColor("#fbbf24"); // Amber
            default:
                return iconColor;
            }
        }
        case IconStyle::Linear:
        default:
            return iconColor;
        }
    };

    // 4. Standalone Circular Glass Close Button (scaled dynamically to maintain perfect circle)
    const int closeSz = qRound(36 * m_uiScale);
    const int closeRadius = closeSz / 2;
    const int closeIconSz = qRound(18 * m_uiScale);
    const int iconSz = qRound(20 * m_uiScale);

    QString closeBtnBg;
    QString closeBtnBorder;
    if (isLight) {
        closeBtnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.90), stop:1 rgba(241, 245, 249, 0.80))");
        closeBtnBorder = QStringLiteral("rgba(15, 23, 42, 0.12)");
    } else if (isZinc) {
        closeBtnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        closeBtnBorder = QStringLiteral("rgba(255, 255, 255, 0.20)");
    } else {
        closeBtnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.10), stop:1 rgba(255, 255, 255, 0.04))");
        closeBtnBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
    }

    QString closeBtnStyle = QString(
        "QToolButton {"
        "   background: %1;"
        "   border: 1px solid %2;"
        "   border-radius: %3px;"
        "}"
        "QToolButton:hover {"
        "   background-color: rgba(239, 68, 68, 0.22);"
        "   border-color: rgba(239, 68, 68, 0.55);"
        "}"
        "QToolButton:pressed {"
        "   background-color: rgba(239, 68, 68, 0.35);"
        "}"
    ).arg(closeBtnBg, closeBtnBorder, QString::number(closeRadius));

    if (m_btnClose) {
        m_btnClose->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Close, getIconColor(VectorIcons::UiIcon::Close), closeIconSz));
        m_btnClose->setStyleSheet(closeBtnStyle);
    }

    // 5. Left Navigation Island Buttons (< | > | Gallery)
    if (m_btnPrev) {
        m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronLeft, getIconColor(VectorIcons::UiIcon::ChevronLeft), iconSz));
        m_btnPrev->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnNext) {
        m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronRight, getIconColor(VectorIcons::UiIcon::ChevronRight), iconSz));
        m_btnNext->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnGallery) {
        m_btnGallery->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, getIconColor(VectorIcons::UiIcon::Grid), iconSz));
        m_btnGallery->setStyleSheet(islandInnerBtnStyle);
    }

    // 6. Right Actions Island Buttons (Search, Zoom, Rotate, Trash, Info, Share, Settings, Export/Save)
    if (m_btnSearch) {
        m_btnSearch->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Search, getIconColor(VectorIcons::UiIcon::Search), iconSz));
        m_btnSearch->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnZoom) {
        m_btnZoom->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Zoom, getIconColor(VectorIcons::UiIcon::Zoom), iconSz));
        m_btnZoom->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnRotate) {
        m_btnRotate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, getIconColor(VectorIcons::UiIcon::RotateRight), iconSz));
        m_btnRotate->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnTrash) {
        m_btnTrash->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Trash, getIconColor(VectorIcons::UiIcon::Trash), iconSz));
        m_btnTrash->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnInspector) {
        m_btnInspector->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Info, getIconColor(VectorIcons::UiIcon::Info), iconSz));
        m_btnInspector->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnShare) {
        m_btnShare->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Share, getIconColor(VectorIcons::UiIcon::Share), iconSz));
        m_btnShare->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnSettings) {
        m_btnSettings->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Settings, getIconColor(VectorIcons::UiIcon::Settings), iconSz));
        m_btnSettings->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnExport) {
        m_btnExport->setStyleSheet(islandInnerBtnStyle);
        if (m_editMode) {
            m_btnExport->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorSave, getIconColor(VectorIcons::UiIcon::EditorSave), iconSz));
            m_btnExport->setToolTip(tr("Save document (Ctrl+S)"));
        } else {
            m_btnExport->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorExport, getIconColor(VectorIcons::UiIcon::EditorExport), iconSz));
            m_btnExport->setToolTip(tr("Export document (Ctrl+E)"));
        }
    }

    // 7. Contextual Edit Tools inside EditToolsIsland (borderless 10px radius with padding for text pills)
    QString contextualPillStyle = QString(
        "QPushButton, QToolButton {"
        "   background: transparent;"
        "   color: %1;"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 0px 12px 0px 10px;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover, QToolButton:hover {"
        "   background-color: %2;"
        "}"
        "QPushButton:pressed, QToolButton:pressed {"
        "   background-color: %3;"
        "}"
        "QPushButton:checked, QToolButton:checked {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff;"
        "}"
    ).arg(textColor, innerHoverBg, innerPressedBg);

    // Image tools
    if (m_btnImgRotateLeft) {
        m_btnImgRotateLeft->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, iconColor, 21));
        m_btnImgRotateLeft->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgRotateRight) {
        m_btnImgRotateRight->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, iconColor, 21));
        m_btnImgRotateRight->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgFlipH) {
        m_btnImgFlipH->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::FlipH, iconColor, 21));
        m_btnImgFlipH->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgFlipV) {
        m_btnImgFlipV->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::FlipV, iconColor, 21));
        m_btnImgFlipV->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgCrop) {
        m_btnImgCrop->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Crop, iconColor, 21));
        m_btnImgCrop->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgResize) {
        m_btnImgResize->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shapes, iconColor, 21));
        m_btnImgResize->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgInvert) {
        m_btnImgInvert->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, iconColor, 21));
        m_btnImgInvert->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnImgInspector) {
        m_btnImgInspector->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Info, iconColor, 21));
        m_btnImgInspector->setStyleSheet(islandInnerBtnStyle);
    }

    // Office tools
    if (m_btnOfficeEditLo) {
        m_btnOfficeEditLo->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ExternalLink, iconColor, 21));
        m_btnOfficeEditLo->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnOfficeSavePdf) {
        m_btnOfficeSavePdf->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorSave, iconColor, 21));
        m_btnOfficeSavePdf->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnOfficeExtractImages) {
        m_btnOfficeExtractImages->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Extract, iconColor, 21));
        m_btnOfficeExtractImages->setStyleSheet(islandInnerBtnStyle);
    }

    // PDF Clean 4-Pill cluster inside EditToolsIsland
    if (m_btnPdfText) {
        QColor textIconColor = m_btnPdfText->isChecked() ? QColor("#ffffff") : getIconColor(VectorIcons::UiIcon::EditorText);
        m_btnPdfText->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorText, textIconColor, 21));
        m_btnPdfText->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnPdfShapes) {
        m_btnPdfShapes->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorInsert, getIconColor(VectorIcons::UiIcon::EditorInsert), 21));
        m_btnPdfShapes->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnPdfSign) {
        m_btnPdfSign->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorSign, getIconColor(VectorIcons::UiIcon::EditorSign), 21));
        m_btnPdfSign->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnPdfPages) {
        m_btnPdfPages->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorPages, getIconColor(VectorIcons::UiIcon::EditorPages), 21));
        m_btnPdfPages->setStyleSheet(islandInnerBtnStyle);
    }

    // Page Organizer pills inside OrgToolsIsland
    if (m_btnOrgRotate) {
        m_btnOrgRotate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, iconColor, 15));
        m_btnOrgRotate->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgDelete) {
        m_btnOrgDelete->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Trash, isLight ? QColor("#dc2626") : QColor("#f87171"), 15));
        m_btnOrgDelete->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgInsertBlank) {
        m_btnOrgInsertBlank->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Plus, iconColor, 15));
        m_btnOrgInsertBlank->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgAddPdf) {
        m_btnOrgAddPdf->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::File, iconColor, 15));
        m_btnOrgAddPdf->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgReverse) {
        m_btnOrgReverse->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, iconColor, 15));
        m_btnOrgReverse->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgReset) {
        m_btnOrgReset->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, iconColor, 15));
        m_btnOrgReset->setStyleSheet(contextualPillStyle);
    }
    if (m_btnOrgRepair) {
        m_btnOrgRepair->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repair, iconColor, 15));
        m_btnOrgRepair->setStyleSheet(contextualPillStyle);
    }

    // 8. Left Edit Island Buttons: [✕ Discard | ↶ Undo | ⏱ Timeline]
    QString discardBtnStyle = QString(
        "QToolButton {"
        "   background: transparent;"
        "   color: %1;"
        "   border: none;"
        "   border-radius: 10px;"
        "   padding: 0px;"
        "}"
        "QToolButton:hover {"
        "   background-color: rgba(239, 68, 68, 0.22);"
        "   color: #ef4444;"
        "}"
    ).arg(textColor);

    if (m_btnDiscard) {
        m_btnDiscard->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorDiscard, getIconColor(VectorIcons::UiIcon::EditorDiscard), 21));
        m_btnDiscard->setStyleSheet(discardBtnStyle);
    }
    if (m_btnQuickUndo) {
        m_btnQuickUndo->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorUndo, getIconColor(VectorIcons::UiIcon::EditorUndo), 21));
        m_btnQuickUndo->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnHistory) {
        m_btnHistory->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::EditorTimeline, getIconColor(VectorIcons::UiIcon::EditorTimeline), 21));
        m_btnHistory->setStyleSheet(islandInnerBtnStyle);
    }

    if (m_contextualBar) {
        m_contextualBar->applyThemePreset(m_currentPreset);
    }

    // Archive tools
    if (m_btnArcExtract) {
        m_btnArcExtract->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Extract, isLight ? QColor("#0284c7") : QColor("#38bdf8"), 18));
        m_btnArcExtract->setStyleSheet(contextualPillStyle);
    }
    if (m_btnArcAdd) {
        m_btnArcAdd->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Plus, iconColor, 18));
        m_btnArcAdd->setStyleSheet(contextualPillStyle);
    }
    if (m_btnArcRemove) {
        m_btnArcRemove->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Trash, isLight ? QColor("#dc2626") : QColor("#f87171"), 18));
        m_btnArcRemove->setStyleSheet(contextualPillStyle);
    }
    if (m_btnArcTest) {
        m_btnArcTest->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Test, isLight ? QColor("#16a34a") : QColor("#4ade80"), 18));
        m_btnArcTest->setStyleSheet(contextualPillStyle);
    }

    // Text tools
    if (m_btnTxtNumbers) {
        m_btnTxtNumbers->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Numbers, iconColor, 21));
        m_btnTxtNumbers->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnTxtSyntax) {
        m_btnTxtSyntax->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, iconColor, 21));
        m_btnTxtSyntax->setStyleSheet(islandInnerBtnStyle);
    }
    if (m_btnTxtWrap) {
        m_btnTxtWrap->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Wrap, iconColor, 21));
        m_btnTxtWrap->setStyleSheet(islandInnerBtnStyle);
    }

    // Audio tools
    if (m_btnAudStudio) {
        m_btnAudStudio->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Scissors, iconColor, 18));
        m_btnAudStudio->setStyleSheet(contextualPillStyle);
    }
    if (m_btnAudRestore) {
        m_btnAudRestore->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Signal, iconColor, 18));
        m_btnAudRestore->setStyleSheet(contextualPillStyle);
    }
    if (m_btnAudPitch) {
        m_btnAudPitch->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, iconColor, 18));
        m_btnAudPitch->setStyleSheet(contextualPillStyle);
    }
    if (m_btnAudEq) {
        m_btnAudEq->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Equalizer, iconColor, 18));
        m_btnAudEq->setStyleSheet(contextualPillStyle);
    }
    if (m_btnAudTags) {
        m_btnAudTags->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Tag, iconColor, 18));
        m_btnAudTags->setStyleSheet(contextualPillStyle);
    }
    if (m_btnAudSummary) {
        m_btnAudSummary->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Test, iconColor, 18));
        m_btnAudSummary->setStyleSheet(contextualPillStyle);
    }

    // Folder Studio tools
    if (m_btnFldDuplicates) {
        m_btnFldDuplicates->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Search, iconColor, 17));
        m_btnFldDuplicates->setStyleSheet(contextualPillStyle);
    }
    if (m_btnFldRecovery) {
        m_btnFldRecovery->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Repair, iconColor, 17));
        m_btnFldRecovery->setStyleSheet(contextualPillStyle);
    }
    if (m_btnFldOrganize) {
        m_btnFldOrganize->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, iconColor, 17));
        m_btnFldOrganize->setStyleSheet(contextualPillStyle);
    }
    if (m_btnFldBatch) {
        m_btnFldBatch->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Tag, iconColor, 17));
        m_btnFldBatch->setStyleSheet(contextualPillStyle);
    }

    // Dividers inside capsule islands
    QString sepStyle = QString("background-color: %1; border: none;").arg(
        isLight ? "rgba(15, 23, 42, 0.14)" : "rgba(255, 255, 255, 0.14)"
    );
    if (m_sepNavIsland) m_sepNavIsland->setStyleSheet(sepStyle);
    if (m_sepImg1) m_sepImg1->setStyleSheet(sepStyle);
    if (m_sepImg2) m_sepImg2->setStyleSheet(sepStyle);
    if (m_sepOffice1) m_sepOffice1->setStyleSheet(sepStyle);
    if (m_sepArc1) m_sepArc1->setStyleSheet(sepStyle);

    // 9. Standalone Primary Edit / Done Accent Pill (scaled dynamically, vibrant #38bdf8 -> #2563eb gradient)
    if (m_btnEdit) {
        const int editIconSz = qRound(22 * m_uiScale);
        m_btnEdit->setText(QString());

        QString primaryEditPillStyle = QString(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
            "   color: #ffffff;"
            "   border: 1px solid rgba(255, 255, 255, 0.35);"
            "   border-radius: %1px;"
            "   padding: 0px;"
            "}"
            "QPushButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6);"
            "   border-color: rgba(255, 255, 255, 0.55);"
            "}"
            "QPushButton:pressed {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0284c7, stop:1 #1d4ed8);"
            "}"
        ).arg(islandRadius);

        if (m_pageOrganizerMode || m_editMode) {
            m_btnEdit->setToolTip(tr("Done / Exit Edit Mode (Esc)"));
            m_btnEdit->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Check, QColor("#ffffff"), editIconSz));
        } else {
            m_btnEdit->setToolTip(tr("Edit Mode (Ctrl+T)"));
            m_btnEdit->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Edit, QColor("#ffffff"), editIconSz));
        }
        m_btnEdit->setStyleSheet(primaryEditPillStyle);
    }

    updateHeaderGeometry();
}

void MinimalHeaderBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bgGrad(0, 0, width(), height());
    switch (m_currentPreset) {
    case ThemePreset::ModernLight:
    case ThemePreset::BreezeLight:
        bgGrad.setColorAt(0.0, QColor("#edf0f4"));
        bgGrad.setColorAt(0.5, QColor("#e2e6eb"));
        bgGrad.setColorAt(1.0, QColor("#d6dbe2"));
        break;
    case ThemePreset::BreezeDark:
        bgGrad.setColorAt(0.0, QColor("#1c212e"));
        bgGrad.setColorAt(0.5, QColor("#151924"));
        bgGrad.setColorAt(1.0, QColor("#10131c"));
        break;
    case ThemePreset::ZincGray:
        bgGrad.setColorAt(0.0, QColor("#3e3f44"));
        bgGrad.setColorAt(0.5, QColor("#343539"));
        bgGrad.setColorAt(1.0, QColor("#2b2c30"));
        break;
    case ThemePreset::DarkSlate:
    default:
        bgGrad.setColorAt(0.0, QColor("#1e2538"));
        bgGrad.setColorAt(0.5, QColor("#171d2c"));
        bgGrad.setColorAt(1.0, QColor("#131825"));
        break;
    }

    // Fill Liquid Glass header background seamlessly (round top corners when frameless and not maximized)
    const bool isFramelessNormal = (window() && !window()->isMaximized() && !window()->isFullScreen());
    if (isFramelessNormal) {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        path.addRoundedRect(rect(), 18, 18);
        QRect bottomHalf(0, height() / 2, width(), height() - height() / 2);
        path.addRect(bottomHalf);
        p.fillPath(path, bgGrad);
    } else {
        p.fillRect(rect(), bgGrad);
    }
}

void MinimalHeaderBar::setZoomFactor(double factor)
{
    m_currentZoomFactor = factor;
    int percent = qRound(factor * 100.0);
    if (m_btnZoom) {
        m_btnZoom->setToolTip(tr("Zoom: %1% (Click for options, scroll wheel to zoom)").arg(percent));
    }
    for (auto *act : m_presetZoomActions) {
        bool ok = false;
        int actPercent = act->text().remove(QLatin1Char('%')).trimmed().toInt(&ok);
        if (ok) {
            act->setChecked(std::abs(actPercent - percent) <= 5);
        }
    }
}

bool MinimalHeaderBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_btnZoom && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent*>(event);
        if (we->angleDelta().y() > 0) {
            emit zoomInRequested();
        } else if (we->angleDelta().y() < 0) {
            emit zoomOutRequested();
        }
        return true;
    }

    if (watched == m_windowControlsWidget && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // If clicked on margin/padding area rather than control buttons, initiate window move
            QWidget *child = m_windowControlsWidget->childAt(me->pos());
            if (!child || child == m_windowControlsWidget) {
                mousePressEvent(me);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MinimalHeaderBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Native Wayland (xdg_toplevel.move) and X11 NetWM system move
        if (window() && window()->windowHandle()) {
            if (window()->windowHandle()->startSystemMove()) {
                event->accept();
                return;
            }
        }
        // Fallback for environments where startSystemMove() is unavailable
        m_dragStartPosition = event->globalPosition().toPoint();
        if (window()) {
            m_windowStartPosition = window()->pos();
        }
        m_isDragging = true;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MinimalHeaderBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        if (window()) {
            QPoint delta = event->globalPosition().toPoint() - m_dragStartPosition;
            window()->move(m_windowStartPosition + delta);
            event->accept();
            return;
        }
    }
    QWidget::mouseMoveEvent(event);
}

void MinimalHeaderBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void MinimalHeaderBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeRestoreRequested();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void MinimalHeaderBar::setActiveAudioWorkspace(int index)
{
    if (!m_audToolsGroup) return;
    if (auto *btn = m_audToolsGroup->button(index)) {
        btn->setChecked(true);
    } else if (index == 0) {
        m_audToolsGroup->setExclusive(false);
        if (auto *checked = m_audToolsGroup->checkedButton()) {
            checked->setChecked(false);
        }
        m_audToolsGroup->setExclusive(true);
    }
}

int MinimalHeaderBar::activeAudioWorkspace() const
{
    if (!m_audToolsGroup) return 0;
    int id = m_audToolsGroup->checkedId();
    return id >= 1 ? id : 0;
}

void MinimalHeaderBar::setAudioWorkspaceToolsAvailable(bool available)
{
    if (m_audioToolsAvailable == available) return;
    m_audioToolsAvailable = available;
    updateContextualToolsVisibility();
}

void MinimalHeaderBar::setActiveFolderStudioTool(int index)
{
    if (!m_fldToolsGroup) return;
    if (auto *btn = m_fldToolsGroup->button(index)) {
        btn->setChecked(true);
    }
}

void MinimalHeaderBar::setAudioWorkspaceModified(int index, bool modified)
{
    QString dot = modified ? QString::fromUtf8(" •") : QString();
    switch (index) {
    case 1:
        if (m_btnAudStudio) m_btnAudStudio->setText(tr("Trim") + dot);
        break;
    case 2:
        if (m_btnAudRestore) m_btnAudRestore->setText(tr("Filters") + dot);
        break;
    case 3:
        if (m_btnAudPitch) m_btnAudPitch->setText(tr("Pitch && Ch.") + dot);
        break;
    case 4:
        if (m_btnAudEq) m_btnAudEq->setText(tr("Equalizer") + dot);
        break;
    case 5:
        if (m_btnAudTags) m_btnAudTags->setText(tr("Tags") + dot);
        break;
    default:
        break;
    }
}

void MinimalHeaderBar::setHistorySteps(const QList<PdfHistoryStep> &steps)
{
    if (m_historyWidget) {
        m_historyWidget->setHistorySteps(steps);
    }
}

void MinimalHeaderBar::showHistoryDialog()
{
    emit showHistoryTimelineRequested();
    if (m_historyWidget) {
        m_historyWidget->updateTheme();
        if (window()) {
            int dlgW = qMin(1100, qMax(800, window()->width() - 80));
            m_historyWidget->setFixedWidth(dlgW);
            m_historyWidget->move(window()->geometry().center().x() - dlgW / 2,
                                  window()->geometry().center().y() - m_historyWidget->height() / 2);
        }
        m_historyWidget->exec();
    }
}
