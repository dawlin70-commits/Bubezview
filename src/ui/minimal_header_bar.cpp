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
    setFixedHeight(46);
    setupUi();
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
    m_primaryBar->setFixedHeight(46);
    auto *mainLayout = new QHBoxLayout(m_primaryBar);
    mainLayout->setContentsMargins(14, 0, 14, 0);
    mainLayout->setSpacing(8);

    // =========================================================================
    // 1. LEFT: Window Controls (Preview) / Discard & History (Edit)
    // =========================================================================
    m_windowControlsWidget = new QWidget(m_primaryBar);
    auto *ctrlsLayout = new QHBoxLayout(m_windowControlsWidget);
    ctrlsLayout->setContentsMargins(0, 0, 6, 0);
    ctrlsLayout->setSpacing(6);

    m_btnClose = new QToolButton(m_windowControlsWidget);
    m_btnClose->setFixedSize(32, 32);
    m_btnClose->setIconSize(QSize(18, 18));
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setToolTip(tr("Close (Esc / Space)"));
    connect(m_btnClose, &QToolButton::clicked, this, &MinimalHeaderBar::closeRequested);
    ctrlsLayout->addWidget(m_btnClose, 0, Qt::AlignVCenter);

    ctrlsLayout->addSpacing(4);

    // Segmented Prev/Next navigation pair (1px internal gap for unified capsule look)
    auto *navPairLayout = new QHBoxLayout();
    navPairLayout->setContentsMargins(0, 0, 0, 0);
    navPairLayout->setSpacing(1);

    m_btnPrev = new QToolButton(m_windowControlsWidget);
    m_btnPrev->setFixedSize(34, 32);
    m_btnPrev->setIconSize(QSize(20, 20));
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    m_btnPrev->setToolTip(tr("Previous File (Left Arrow)"));
    m_btnPrev->setEnabled(false);
    connect(m_btnPrev, &QToolButton::clicked, this, &MinimalHeaderBar::prevRequested);
    navPairLayout->addWidget(m_btnPrev);

    m_btnNext = new QToolButton(m_windowControlsWidget);
    m_btnNext->setFixedSize(34, 32);
    m_btnNext->setIconSize(QSize(20, 20));
    m_btnNext->setCursor(Qt::PointingHandCursor);
    m_btnNext->setToolTip(tr("Next File (Right Arrow)"));
    m_btnNext->setEnabled(false);
    connect(m_btnNext, &QToolButton::clicked, this, &MinimalHeaderBar::nextRequested);
    navPairLayout->addWidget(m_btnNext);

    ctrlsLayout->addLayout(navPairLayout);
    ctrlsLayout->addSpacing(2);

    m_btnGallery = new QToolButton(m_windowControlsWidget);
    m_btnGallery->setFixedSize(34, 32);
    m_btnGallery->setIconSize(QSize(20, 20));
    m_btnGallery->setCursor(Qt::PointingHandCursor);
    m_btnGallery->setToolTip(tr("Folder Gallery (Alt+Up)"));
    connect(m_btnGallery, &QToolButton::clicked, this, &MinimalHeaderBar::galleryRequested);
    ctrlsLayout->addWidget(m_btnGallery, 0, Qt::AlignVCenter);

    mainLayout->addWidget(m_windowControlsWidget, 0, Qt::AlignVCenter);

    // Install hover event filter to show/hide glyph inside close button
    m_windowControlsWidget->installEventFilter(this);
    updateTrafficLightIcons(false);

    // Left Edit Mode capsule: [✕ Discard] + [↶ History]
    m_editLeftWidget = new QWidget(m_primaryBar);
    auto *editLeftLayout = new QHBoxLayout(m_editLeftWidget);
    editLeftLayout->setContentsMargins(0, 0, 6, 0);
    editLeftLayout->setSpacing(6);

    m_btnDiscard = new QToolButton(m_editLeftWidget);
    m_btnDiscard->setFixedHeight(34);
    m_btnDiscard->setCursor(Qt::PointingHandCursor);
    m_btnDiscard->setText(tr("✕ Discard"));
    m_btnDiscard->setToolTip(tr("Discard all changes and exit edit mode (Esc)"));
    connect(m_btnDiscard, &QToolButton::clicked, this, &MinimalHeaderBar::discardEditRequested);
    editLeftLayout->addWidget(m_btnDiscard, 0, Qt::AlignVCenter);

    // Fast 1-click single-step undo button
    m_btnQuickUndo = new QToolButton(m_editLeftWidget);
    m_btnQuickUndo->setFixedHeight(34);
    m_btnQuickUndo->setCursor(Qt::PointingHandCursor);
    m_btnQuickUndo->setText(tr("↶ Undo"));
    m_btnQuickUndo->setToolTip(tr("Undo last change (Ctrl+Z)"));
    connect(m_btnQuickUndo, &QToolButton::clicked, this, &MinimalHeaderBar::undoRequested);
    editLeftLayout->addWidget(m_btnQuickUndo, 0, Qt::AlignVCenter);

    // Visual Timeline / Slides button
    m_btnHistory = new QToolButton(m_editLeftWidget);
    m_btnHistory->setFixedHeight(34);
    m_btnHistory->setCursor(Qt::PointingHandCursor);
    m_btnHistory->setText(tr("🎞️ Timeline"));
    m_btnHistory->setToolTip(tr("View document history timeline & snapshots"));
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

    // Page 0: Preview View (Active File Name & Compact Pill Index Badge)
    m_previewCenterWidget = new QWidget(m_centerStack);
    m_previewCenterWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_previewCenterWidget->setMinimumWidth(0);
    auto *previewCenterLay = new QHBoxLayout(m_previewCenterWidget);
    previewCenterLay->setContentsMargins(0, 0, 0, 0);
    previewCenterLay->setSpacing(8);

    m_lblTitle = new QLabel(tr("BubezView"), m_previewCenterWidget);
    m_lblTitle->setObjectName("HeaderTitleLabel");
    m_lblTitle->setStyleSheet("font-weight: 600; font-size: 13px;");
    m_lblTitle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_lblTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lblTitle->setMinimumWidth(0);
    previewCenterLay->addWidget(m_lblTitle, 0, Qt::AlignVCenter);

    m_lblBadge = new QLabel(m_previewCenterWidget);
    m_lblBadge->setObjectName("HeaderIndexBadge");
    m_lblBadge->setFixedHeight(22);
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
    // 3. RIGHT: Actions (Search, Info, Share, Settings gear, Primary Edit Pill)
    // =========================================================================
    m_btnSearch = new QToolButton(m_primaryBar);
    m_btnSearch->setVisible(false);
    connect(m_btnSearch, &QToolButton::clicked, this, &MinimalHeaderBar::searchRequested);

    // Modern Zoom controller button with instant popup preset menu & mouse wheel zooming
    m_btnZoom = new QToolButton(m_primaryBar);
    m_btnZoom->setFixedSize(34, 32);
    m_btnZoom->setIconSize(QSize(20, 20));
    m_btnZoom->setCursor(Qt::PointingHandCursor);
    m_btnZoom->setToolTip(tr("Zoom: 100% (Click for options, scroll wheel to zoom)"));
    m_btnZoom->setPopupMode(QToolButton::InstantPopup);
    m_btnZoom->installEventFilter(this);

    m_menuZoom = new QMenu(this);
    m_actZoomIn = m_menuZoom->addAction(tr("➕ Zoom In (Ctrl++)"), this, &MinimalHeaderBar::zoomInRequested);
    m_actZoomOut = m_menuZoom->addAction(tr("➖ Zoom Out (Ctrl+-)"), this, &MinimalHeaderBar::zoomOutRequested);
    m_menuZoom->addSeparator();
    m_actZoomFit = m_menuZoom->addAction(tr("🔍 Fit to Window / Width (Ctrl+9)"), this, &MinimalHeaderBar::zoomFitRequested);
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
    mainLayout->addWidget(m_btnZoom, 0, Qt::AlignVCenter);

    // Quick single Rotate button for image view mode (90° Clockwise)
    m_btnRotate = new QToolButton(m_primaryBar);
    m_btnRotate->setFixedSize(34, 32);
    m_btnRotate->setIconSize(QSize(20, 20));
    m_btnRotate->setCursor(Qt::PointingHandCursor);
    m_btnRotate->setToolTip(tr("Rotate 90° Clockwise (R)"));
    m_btnRotate->setVisible(false);
    connect(m_btnRotate, &QToolButton::clicked, this, &MinimalHeaderBar::rotateRightRequested);
    mainLayout->addWidget(m_btnRotate, 0, Qt::AlignVCenter);

    m_btnInspector = new QToolButton(m_primaryBar);
    m_btnInspector->setFixedSize(34, 32);
    m_btnInspector->setIconSize(QSize(20, 20));
    m_btnInspector->setCursor(Qt::PointingHandCursor);
    m_btnInspector->setToolTip(tr("File Properties & Info (Ctrl+I)"));
    connect(m_btnInspector, &QToolButton::clicked, this, &MinimalHeaderBar::inspectorRequested);
    mainLayout->addWidget(m_btnInspector, 0, Qt::AlignVCenter);

    m_btnShare = new QToolButton(m_primaryBar);
    m_btnShare->setFixedSize(34, 32);
    m_btnShare->setIconSize(QSize(20, 20));
    m_btnShare->setCursor(Qt::PointingHandCursor);
    m_btnShare->setToolTip(tr("Share..."));
    connect(m_btnShare, &QToolButton::clicked, this, &MinimalHeaderBar::shareRequested);
    mainLayout->addWidget(m_btnShare, 0, Qt::AlignVCenter);

    m_btnSettings = new QToolButton(m_primaryBar);
    m_btnSettings->setFixedSize(34, 32);
    m_btnSettings->setIconSize(QSize(20, 20));
    m_btnSettings->setCursor(Qt::PointingHandCursor);
    m_btnSettings->setToolTip(tr("Preferences (Ctrl+,)"));
    connect(m_btnSettings, &QToolButton::clicked, this, &MinimalHeaderBar::settingsRequested);
    mainLayout->addWidget(m_btnSettings, 0, Qt::AlignVCenter);

    mainLayout->addSpacing(4);

    // Save & Export button with unified menu
    m_btnExport = new QToolButton(m_primaryBar);
    m_btnExport->setFixedHeight(34);
    m_btnExport->setCursor(Qt::PointingHandCursor);
    m_btnExport->setText(tr("💾 Save & Export ▾"));
    m_btnExport->setToolTip(tr("Save, Export, OCR, and Sharing options"));
    m_btnExport->setPopupMode(QToolButton::InstantPopup);

    m_menuExport = new QMenu(this);
    QAction *actSave = m_menuExport->addAction(tr("💾 Save"), this, [this]() {
        emit saveExportRequested(mapToGlobal(m_btnExport->pos()));
    });
    actSave->setShortcut(QKeySequence::Save);

    QAction *actSaveAs = m_menuExport->addAction(tr("📄 Save As..."), this, &MinimalHeaderBar::saveAsRequested);
    actSaveAs->setShortcut(QKeySequence::SaveAs);

    m_menuExport->addSeparator();
    m_menuExport->addAction(tr("📑 Save Current Page as PDF..."), this, &MinimalHeaderBar::pdfSavePageAsPdfRequested);
    m_menuExport->addAction(tr("🖼️ Save Current Page as Image..."), this, &MinimalHeaderBar::pdfSavePageAsImageRequested);
    m_menuExport->addAction(tr("🔍 Recognize Text (OCR)..."), this, &MinimalHeaderBar::pdfOcrRequested);
    m_menuExport->addAction(tr("📦 Extract All Images (pdfimages)..."), this, &MinimalHeaderBar::pdfExtractImagesRequested);
    m_actPagesRepair = m_menuExport->addAction(tr("🔧 Repair Corrupted PDF..."), this, &MinimalHeaderBar::pdfRepairRequested);

    m_menuExport->addSeparator();
    m_menuExport->addAction(tr("🔗 Share File..."), this, &MinimalHeaderBar::shareRequested);

    m_btnExport->setMenu(m_menuExport);
    m_btnExport->hide();
    mainLayout->addWidget(m_btnExport, 0, Qt::AlignVCenter);

    mainLayout->addSpacing(4);

    // Primary Edit / Done button
    m_btnEdit = new QPushButton(tr("Edit"), m_primaryBar);
    m_btnEdit->setFixedHeight(32);
    m_btnEdit->setMinimumWidth(74);
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    m_btnEdit->setToolTip(tr("Toggle Edit Mode"));
    connect(m_btnEdit, &QPushButton::clicked, this, [this]() {
        if (m_pageOrganizerMode) {
            emit pdfOrganizerBackRequested();
        } else {
            emit editToggled(!m_editMode);
        }
    });
    mainLayout->addWidget(m_btnEdit, 0, Qt::AlignVCenter);

    rootLayout->addWidget(m_primaryBar);

    // Row 2: Contextual Temporary Action Bar (36px when open, 0px when closed)
    m_contextualBar = new ContextualActionBar(this);
    connect(m_contextualBar, &ContextualActionBar::closeRequested, this, &MinimalHeaderBar::hideContextualBar);
    connect(m_contextualBar, &ContextualActionBar::textReplaceRequested, this, &MinimalHeaderBar::pdfTextReplaceRequested);
    connect(m_contextualBar, &ContextualActionBar::textPropertiesChanged, this, &MinimalHeaderBar::pdfTextPropertiesChanged);
    connect(m_contextualBar, &ContextualActionBar::textBgChanged, this, &MinimalHeaderBar::pdfTextBgChanged);
    connect(m_contextualBar, &ContextualActionBar::textDoneRequested, this, &MinimalHeaderBar::pdfTextDoneRequested);
    rootLayout->addWidget(m_contextualBar);
}

void MinimalHeaderBar::setTitle(const QString &title)
{
    m_rawTitle = title;
    updateDisplayTitle();
}

void MinimalHeaderBar::updateDisplayTitle()
{
    if (!m_lblTitle) return;
    QString displayTitle = m_rawTitle.isEmpty() ? tr("BubezView") : m_rawTitle;

    int leftWidth = m_windowControlsWidget ? m_windowControlsWidget->sizeHint().width() : 110;
    int rightWidth = 200; // estimated width of right action buttons
    int availableWidth = width() - leftWidth - rightWidth;
    if (m_lblBadge && m_lblBadge->isVisible()) {
        availableWidth -= (m_lblBadge->sizeHint().width() + 12);
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
    updateDisplayTitle();
}

void MinimalHeaderBar::showContextualTextBar(const QString &text, const QFont &font,
                                            const QColor &color, PdfTextBgMode bgMode,
                                            const QColor &bgColor)
{
    if (!m_editMode) return;
    if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(false);
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
        if (font.pointSize() > 0) {
            m_contextualBar->setTextFont(font);
        }
        if (color.isValid()) {
            m_contextualBar->setTextColor(color);
        }
        m_contextualBar->setTextBgMode(bgMode);
        m_contextualBar->setTextBgColor(bgColor);
        m_contextualBar->setMode(ContextBarMode::Text);
    }
    updateHeaderGeometry();
}

void MinimalHeaderBar::showContextualHighlightBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_btnPdfHighlight && !m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(true);
    }
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Highlight);
    }
    updateHeaderGeometry();
}

void MinimalHeaderBar::showContextualShapesBar(int shapeType)
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(false);
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setActiveShapeType(shapeType);
        m_contextualBar->setMode(ContextBarMode::Shapes);
    }
    updateHeaderGeometry();
}

void MinimalHeaderBar::showContextualRedactBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(false);
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Redact);
    }
    updateHeaderGeometry();
}

void MinimalHeaderBar::showContextualStampBar()
{
    if (!m_editMode) return;
    if (m_btnPdfText && m_btnPdfText->isChecked()) {
        QSignalBlocker blocker(m_btnPdfText);
        m_btnPdfText->setChecked(false);
        emit pdfTextToolToggled(false);
    }
    if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(false);
        emit pdfHighlightToolToggled(false);
    }
    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Stamp);
    }
    updateHeaderGeometry();
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

    if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
        QSignalBlocker blocker(m_btnPdfHighlight);
        m_btnPdfHighlight->setChecked(false);
    }
    emit pdfHighlightToolToggled(false);

    if (m_contextualBar) {
        m_contextualBar->setMode(ContextBarMode::Hidden);
    }
    updateHeaderGeometry();
}

void MinimalHeaderBar::updateHeaderGeometry()
{
    if (m_editMode) {
        bool showContext = m_contextualBar && m_contextualBar->mode() != ContextBarMode::Hidden;
        if (m_primaryBar) m_primaryBar->setFixedHeight(46);
        setFixedHeight(showContext ? 82 : 46);
    } else {
        if (m_primaryBar) m_primaryBar->setFixedHeight(46);
        setFixedHeight(46);
    }
    updateGeometry();
}

void MinimalHeaderBar::setEditMode(bool editMode)
{
    m_editMode = editMode;

    if (!editMode) {
        hideContextualBar();
        if (m_primaryBar) m_primaryBar->setFixedHeight(46);
        setFixedHeight(46);

        if (m_windowControlsWidget) m_windowControlsWidget->setVisible(m_windowControlsVisible);
        if (m_btnPrev) m_btnPrev->setVisible(!m_rawTitle.isEmpty());
        if (m_btnNext) m_btnNext->setVisible(!m_rawTitle.isEmpty());
        if (m_btnGallery) m_btnGallery->show();
        if (m_editLeftWidget) m_editLeftWidget->hide();

        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) m_btnZoom->hide();
        if (m_btnZoom) m_btnZoom->setVisible(m_currentCategory == FileCategory::Image || m_currentCategory == FileCategory::Pdf || m_currentCategory == FileCategory::Text || m_currentCategory == FileCategory::Office);
        if (m_btnRotate) m_btnRotate->setVisible(m_currentCategory == FileCategory::Image);
        if (m_btnInspector) m_btnInspector->show();
        if (m_btnShare) m_btnShare->show();
        if (m_btnSettings) m_btnSettings->show();
        if (m_btnExport) m_btnExport->hide();

        if (m_btnEdit) {
            m_btnEdit->setText(tr("Edit"));
            m_btnEdit->setFixedHeight(32);
            m_btnEdit->setToolTip(tr("Toggle Edit Mode"));
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
        if (m_primaryBar) m_primaryBar->setFixedHeight(46);
        updateHeaderGeometry();

        if (m_windowControlsWidget) m_windowControlsWidget->hide();
        if (m_editLeftWidget) m_editLeftWidget->show();

        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) m_btnZoom->hide();
        if (m_btnRotate) m_btnRotate->hide();
        if (m_btnInspector) m_btnInspector->hide();
        if (m_btnShare) m_btnShare->hide();
        if (m_btnSettings) m_btnSettings->hide();
        if (m_btnExport) m_btnExport->show();

        if (m_btnEdit) {
            m_btnEdit->setText(tr("✓ Confirm"));
            m_btnEdit->setFixedHeight(34);
            m_btnEdit->setToolTip(tr("Confirm changes and return to preview mode (Esc)"));
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
        if (m_primaryBar) m_primaryBar->setFixedHeight(46);
        setFixedHeight(46);

        if (m_windowControlsWidget) m_windowControlsWidget->setVisible(m_windowControlsVisible);
        if (m_editLeftWidget) {
            m_editLeftWidget->show();
            if (m_btnDiscard) m_btnDiscard->hide();
            if (m_btnQuickUndo) m_btnQuickUndo->show();
            if (m_btnHistory) m_btnHistory->hide();
        }

        if (m_btnPrev) m_btnPrev->hide();
        if (m_btnNext) m_btnNext->hide();
        if (m_btnGallery) m_btnGallery->hide();
        if (m_btnSearch) m_btnSearch->hide();
        if (m_btnZoom) m_btnZoom->hide();
        if (m_btnRotate) m_btnRotate->hide();
        if (m_btnInspector) m_btnInspector->hide();
        if (m_btnShare) m_btnShare->hide();
        if (m_btnSettings) m_btnSettings->hide();
        if (m_btnExport) m_btnExport->hide();

        if (m_btnEdit) {
            m_btnEdit->setText(tr("✓ Done"));
            m_btnEdit->setFixedHeight(32);
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
        if (m_btnDiscard) m_btnDiscard->show();
        if (m_btnHistory) m_btnHistory->show();
        if (m_btnSettings) m_btnSettings->show();
        if (m_btnExport) m_btnExport->setVisible(m_editMode);
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
    auto *lay = new QHBoxLayout(m_editToolContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    // Left stretch to dynamically center tool cluster over document content
    lay->addStretch(1);

    auto makeToolBtn = [this, lay](QToolButton *&btn, const QString &tip) {
        btn = new QToolButton(m_editToolContainer);
        btn->setFixedSize(36, 32);
        btn->setIconSize(QSize(21, 21));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    auto makePillBtn = [this, lay](QPushButton *&btn, const QString &text, const QString &tip) {
        btn = new QPushButton(m_editToolContainer);
        btn->setFixedHeight(32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(text);
        btn->setToolTip(tip);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    auto makeSeparator = [this, lay](QFrame *&sep) {
        sep = new QFrame(m_editToolContainer);
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

    // Cluster 2: Crop & Photo Studio
    makeToolBtn(m_btnImgCrop, tr("Crop Selection (C)"));
    connect(m_btnImgCrop, &QToolButton::clicked, this, &MinimalHeaderBar::cropRequested);

    makePillBtn(m_btnImgStudio, tr("Studio"), tr("Photo Studio & Adjustments (Ctrl+B)"));
    connect(m_btnImgStudio, &QPushButton::clicked, this, &MinimalHeaderBar::photoStudioRequested);

    // --- PDF Tools: 5 Clean Central Pills ---
    // Pill 1: Text
    m_btnPdfText = new QPushButton(m_editToolContainer);
    m_btnPdfText->setFixedHeight(34);
    m_btnPdfText->setCheckable(true);
    m_btnPdfText->setCursor(Qt::PointingHandCursor);
    m_btnPdfText->setText(tr("✍️ Text"));
    m_btnPdfText->setToolTip(tr("Write text directly on PDF (T)"));
    lay->addWidget(m_btnPdfText, 0, Qt::AlignVCenter);

    connect(m_btnPdfText, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            if (m_btnPdfHighlight && m_btnPdfHighlight->isChecked()) {
                QSignalBlocker blocker(m_btnPdfHighlight);
                m_btnPdfHighlight->setChecked(false);
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

    // Pill 2: Highlight (Kept as invisible object for state tracking)
    m_btnPdfHighlight = new QPushButton(this);
    m_btnPdfHighlight->setVisible(false);
    m_btnPdfHighlight->hide();

    // Pill 3: Insert ▾ (Geometric shapes, images, highlighter, and rubber stamps)
    m_btnPdfShapes = new QToolButton(m_editToolContainer);
    m_btnPdfShapes->setFixedHeight(34);
    m_btnPdfShapes->setCursor(Qt::PointingHandCursor);
    m_btnPdfShapes->setText(tr("➕ Insert ▾"));
    m_btnPdfShapes->setToolTip(tr("Geometric shapes, images, highlighter, and rubber stamps"));
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
    m_btnPdfSign = new QToolButton(m_editToolContainer);
    m_btnPdfSign->setFixedHeight(34);
    m_btnPdfSign->setCursor(Qt::PointingHandCursor);
    m_btnPdfSign->setText(tr("✒️ Signature ▾"));
    m_btnPdfSign->setToolTip(tr("Handwritten, image, and cryptographic signatures"));
    m_btnPdfSign->setPopupMode(QToolButton::InstantPopup);

    m_menuSignature = new QMenu(this);
    m_menuSignature->addAction(tr("✒️ Manage & Place Signatures..."), this, &MinimalHeaderBar::pdfSignatureRequested);
    m_menuSignature->addSeparator();
    m_menuSignature->addAction(tr("🔐 Cryptographic Signature (PAdES)..."), this, &MinimalHeaderBar::pdfCryptoSignatureRequested);
    m_menuSignature->addAction(tr("🔍 Find Signature Fields in Document..."), this, &MinimalHeaderBar::pdfScanSignatureFieldsRequested);

    m_btnPdfSign->setMenu(m_menuSignature);
    lay->addWidget(m_btnPdfSign, 0, Qt::AlignVCenter);

    // Pill 5: Direct Visual Page Organizer
    m_btnPdfPages = new QToolButton(m_editToolContainer);
    m_btnPdfPages->setFixedHeight(34);
    m_btnPdfPages->setCursor(Qt::PointingHandCursor);
    m_btnPdfPages->setText(tr("📑 Organize Pages"));
    m_btnPdfPages->setToolTip(tr("Visual drag-and-drop page manager: reorder, rotate, delete, insert blank pages, and merge PDFs"));
    connect(m_btnPdfPages, &QToolButton::clicked, this, &MinimalHeaderBar::pdfPagesRequested);
    lay->addWidget(m_btnPdfPages, 0, Qt::AlignVCenter);

    // Dummy legacy pointers initialized and hidden for backwards compatibility
    m_btnPdfStudio = new QPushButton(m_editToolContainer); m_btnPdfStudio->hide();
    m_btnPdfSelectArea = new QToolButton(m_editToolContainer); m_btnPdfSelectArea->hide();
    m_btnPdfDraw = new QToolButton(m_editToolContainer); m_btnPdfDraw->hide();
    m_btnPdfType = new QToolButton(m_editToolContainer); m_btnPdfType->hide();
    m_btnPdfShape = new QToolButton(m_editToolContainer); m_btnPdfShape->hide();
    m_btnPdfStamp = new QToolButton(m_editToolContainer); m_btnPdfStamp->hide();
    m_btnPdfRedact = new QToolButton(m_editToolContainer); m_btnPdfRedact->hide();
    m_btnPdfRepair = new QToolButton(m_editToolContainer); m_btnPdfRepair->hide();

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

    // Right stretch to balance the centering
    lay->addStretch(1);
    updateContextualToolsVisibility();
}

void MinimalHeaderBar::setupPageOrganizerTools()
{
    auto *lay = new QHBoxLayout(m_pageOrganizerToolContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    lay->addStretch(1);

    auto makePill = [this, lay](QPushButton *&btn, const QString &text, const QString &tip, auto slot) {
        btn = new QPushButton(m_pageOrganizerToolContainer);
        btn->setFixedHeight(32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(text);
        btn->setToolTip(tip);
        btn->setMinimumWidth(btn->fontMetrics().horizontalAdvance(text) + 40);
        connect(btn, &QPushButton::clicked, this, slot);
        lay->addWidget(btn, 0, Qt::AlignVCenter);
    };

    makePill(m_btnOrgRotate, tr("Rotate"), tr("Rotate selected pages 90° clockwise"), &MinimalHeaderBar::pdfOrganizerRotateRequested);
    makePill(m_btnOrgDelete, tr("Delete"), tr("Remove selected pages from document (Del)"), &MinimalHeaderBar::pdfOrganizerDeleteRequested);

    makePill(m_btnOrgInsertBlank, tr("Blank"), tr("Insert an empty blank page after selected item"), &MinimalHeaderBar::pdfOrganizerInsertBlankRequested);
    makePill(m_btnOrgAddPdf, tr("Add PDF..."), tr("Append or insert pages from another PDF file"), &MinimalHeaderBar::pdfOrganizerAddPdfRequested);

    makePill(m_btnOrgReverse, tr("Reverse"), tr("Invert the overall page sequence"), &MinimalHeaderBar::pdfOrganizerReverseRequested);
    makePill(m_btnOrgReset, tr("Reset"), tr("Revert all pages and rotations to their original state"), &MinimalHeaderBar::pdfOrganizerResetRequested);

    lay->addStretch(1);
}

void MinimalHeaderBar::updateContextualToolsVisibility()
{
    const bool isImg = (m_currentCategory == FileCategory::Image);
    const bool isPdf = (m_currentCategory == FileCategory::Pdf);
    const bool isArc = (m_currentCategory == FileCategory::Archive);
    const bool isTxt = (m_currentCategory == FileCategory::Text);
    const bool isAud = (m_currentCategory == FileCategory::Media) && m_audioToolsAvailable;

    if (m_btnImgRotateLeft) m_btnImgRotateLeft->setVisible(isImg);
    if (m_btnImgRotateRight) m_btnImgRotateRight->setVisible(isImg);
    if (m_btnImgFlipH) m_btnImgFlipH->setVisible(isImg);
    if (m_btnImgFlipV) m_btnImgFlipV->setVisible(isImg);
    if (m_btnImgCrop) m_btnImgCrop->setVisible(isImg);
    if (m_btnImgStudio) m_btnImgStudio->setVisible(isImg);

    if (m_btnPdfText) m_btnPdfText->setVisible(isPdf);
    if (m_btnPdfHighlight) m_btnPdfHighlight->setVisible(false);
    if (m_btnPdfShapes) m_btnPdfShapes->setVisible(isPdf);
    if (m_btnPdfSign) m_btnPdfSign->setVisible(isPdf);
    if (m_btnPdfPages) m_btnPdfPages->setVisible(isPdf);
    if (m_actPagesRepair) m_actPagesRepair->setVisible(isPdf);

    if (m_btnPdfStudio) m_btnPdfStudio->setVisible(false);
    if (m_btnPdfSelectArea) m_btnPdfSelectArea->setVisible(false);
    if (m_btnPdfDraw) m_btnPdfDraw->setVisible(false);
    if (m_btnPdfType) m_btnPdfType->setVisible(false);
    if (m_btnPdfShape) m_btnPdfShape->setVisible(false);
    if (m_btnPdfStamp) m_btnPdfStamp->setVisible(false);
    if (m_btnPdfRedact) m_btnPdfRedact->setVisible(false);
    if (m_btnExport) m_btnExport->setVisible(m_editMode);

    if (m_sepPdf1) m_sepPdf1->setVisible(false);
    if (m_sepPdf2) m_sepPdf2->setVisible(false);
    if (m_sepImg1) m_sepImg1->setVisible(isImg);
    if (m_sepArc1) m_sepArc1->setVisible(isArc);
    if (m_sepExport) m_sepExport->setVisible(false);

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

    if (m_btnZoom && !m_editMode && !m_pageOrganizerMode) {
        m_btnZoom->setVisible(isImg || isPdf || isTxt || (m_currentCategory == FileCategory::Office));
    }
    if (m_btnRotate && !m_editMode && !m_pageOrganizerMode) {
        m_btnRotate->setVisible(isImg);
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
    m_currentPreset = preset;
    updateButtonStyles();
    update();
}

void MinimalHeaderBar::updateButtonStyles()
{
    const bool isLight = (m_currentPreset == ThemePreset::ModernLight || m_currentPreset == ThemePreset::BreezeLight);
    const QColor iconColor = isLight ? QColor("#1d1d1f") : QColor("#e2e8f0");

    // Icons for Share, Settings, Prev & Next, Gallery, Zoom, Inspector
    m_btnShare->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Share, iconColor, 19));
    m_btnSettings->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Settings, iconColor, 19));
    if (m_btnPrev) m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronLeft, iconColor, 18));
    if (m_btnNext) m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronRight, iconColor, 18));
    if (m_btnGallery) m_btnGallery->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, iconColor, 18));
    if (m_btnZoom) m_btnZoom->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Zoom, iconColor, 20));
    if (m_btnInspector) m_btnInspector->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Info, iconColor, 19));

    // Colors and compact pill styling based on theme and edit state
    QString titleColor = isLight ? "#1d1d1f" : "#f1f5f9";
    QString badgeBg = isLight ? "rgba(0, 0, 0, 0.06)" : "rgba(255, 255, 255, 0.08)";
    QString badgeColor = isLight ? "#52525b" : "#94a3b8";
    QString toolBtnHover = isLight ? "rgba(0, 0, 0, 0.07)" : "rgba(255, 255, 255, 0.10)";
    QString toolBtnPressed = isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.16)";

    m_lblTitle->setStyleSheet(QString("color: %1; font-weight: 600; font-size: 13px; padding-left: 2px;").arg(titleColor));
    m_lblBadge->setStyleSheet(QString(
        "background-color: %1; color: %2; border-radius: 11px; padding: 0px 9px; font-size: 11px; font-weight: 600;"
    ).arg(badgeBg, badgeColor));

    // Refined macOS squircle hover style for top-bar action buttons
    QString toolBtnStyle = QString(
        "QToolButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 8px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %1;"
        "}"
        "QToolButton:pressed {"
        "   background-color: %2;"
        "}"
        "QToolButton:disabled {"
        "   opacity: 0.30;"
        "}"
        "QToolButton::menu-indicator {"
        "   image: none;"
        "   width: 0px;"
        "}"
    ).arg(toolBtnHover, toolBtnPressed);
    if (m_btnClose) {
        m_btnClose->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Close, iconColor, 18));
        m_btnClose->setStyleSheet(toolBtnStyle);
    }
    if (m_btnZoom) {
        m_btnZoom->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Zoom, iconColor, 20));
        m_btnZoom->setStyleSheet(toolBtnStyle);
    }
    if (m_btnRotate) {
        m_btnRotate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, iconColor, 20));
        m_btnRotate->setStyleSheet(toolBtnStyle);
    }
    if (m_btnInspector) {
        m_btnInspector->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Info, iconColor, 20));
        m_btnInspector->setStyleSheet(toolBtnStyle);
    }
    if (m_btnShare) {
        m_btnShare->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Share, iconColor, 20));
        m_btnShare->setStyleSheet(toolBtnStyle);
    }
    if (m_btnSettings) {
        m_btnSettings->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Settings, iconColor, 20));
        m_btnSettings->setStyleSheet(toolBtnStyle);
    }
    if (m_btnDiscard) {
        m_btnDiscard->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, iconColor, 20));
        m_btnDiscard->setStyleSheet(toolBtnStyle);
    }

    // Segmented capsule for Prev / Next & soft pill for Gallery (borderless, organic curvature)
    QString navBtnBg = isLight ? "rgba(0, 0, 0, 0.05)" : "rgba(255, 255, 255, 0.07)";
    QString navBtnHover = isLight ? "rgba(0, 0, 0, 0.10)" : "rgba(255, 255, 255, 0.14)";
    QString navBtnPressed = isLight ? "rgba(0, 0, 0, 0.15)" : "rgba(255, 255, 255, 0.20)";
    QString navBtnDisabledBg = isLight ? "rgba(0, 0, 0, 0.025)" : "rgba(255, 255, 255, 0.03)";

    QString prevBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   border: none;"
        "   border-top-left-radius: 8px;"
        "   border-bottom-left-radius: 8px;"
        "   border-top-right-radius: 2px;"
        "   border-bottom-right-radius: 2px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %2;"
        "}"
        "QToolButton:pressed {"
        "   background-color: %3;"
        "}"
        "QToolButton:disabled {"
        "   background-color: %4;"
        "   opacity: 0.25;"
        "}"
    ).arg(navBtnBg, navBtnHover, navBtnPressed, navBtnDisabledBg);

    QString nextBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   border: none;"
        "   border-top-left-radius: 2px;"
        "   border-bottom-left-radius: 2px;"
        "   border-top-right-radius: 8px;"
        "   border-bottom-right-radius: 8px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %2;"
        "}"
        "QToolButton:pressed {"
        "   background-color: %3;"
        "}"
        "QToolButton:disabled {"
        "   background-color: %4;"
        "   opacity: 0.25;"
        "}"
    ).arg(navBtnBg, navBtnHover, navBtnPressed, navBtnDisabledBg);

    QString galleryBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   border: none;"
        "   border-radius: 8px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %2;"
        "}"
        "QToolButton:pressed {"
        "   background-color: %3;"
        "}"
        "QToolButton:disabled {"
        "   background-color: %4;"
        "   opacity: 0.25;"
        "}"
    ).arg(navBtnBg, navBtnHover, navBtnPressed, navBtnDisabledBg);

    if (m_btnPrev) {
        m_btnPrev->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronLeft, iconColor, 20));
        m_btnPrev->setStyleSheet(prevBtnStyle);
    }
    if (m_btnNext) {
        m_btnNext->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::ChevronRight, iconColor, 20));
        m_btnNext->setStyleSheet(nextBtnStyle);
    }
    if (m_btnGallery) {
        m_btnGallery->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Grid, iconColor, 20));
        m_btnGallery->setStyleSheet(galleryBtnStyle);
    }

    // Apply styles to contextual edit tool buttons (unified 32px height, 8px radius)
    QString contextualPillStyle = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   padding: 0px 12px 0px 10px;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %6;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "#1e293b" : "#f1f5f9",
        isLight ? "rgba(0, 0, 0, 0.06)" : "rgba(255, 255, 255, 0.08)",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.16)",
        navBtnPressed
    );

    QString checkableToolStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 8px;"
        "   padding: 0px 12px 0px 10px;"
        "   color: %3;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "}"
        "QToolButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QToolButton:checked {"
        "   background-color: %6;"
        "   border-color: %7;"
        "   color: #ffffff;"
        "   font-weight: 600;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "rgba(0, 0, 0, 0.06)" : "rgba(255, 255, 255, 0.08)",
        isLight ? "#1e293b" : "#f1f5f9",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.16)",
        isLight ? "#0284c7" : "#0284c7",
        isLight ? "#0369a1" : "#38bdf8"
    );

    QString editToolBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 8px;"
        "}"
        "QToolButton:hover {"
        "   background-color: %3;"
        "   border-color: %4;"
        "}"
        "QToolButton:pressed {"
        "   background-color: %5;"
        "}"
        "QToolButton:checked {"
        "   background-color: %6;"
        "   border-color: %7;"
        "}"
        "QToolButton:disabled {"
        "   opacity: 0.30;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "rgba(0, 0, 0, 0.06)" : "rgba(255, 255, 255, 0.08)",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.16)",
        navBtnPressed,
        isLight ? "#0284c7" : "#0284c7",
        isLight ? "#0369a1" : "#38bdf8"
    );

    // Image tools
    if (m_btnImgRotateLeft) {
        m_btnImgRotateLeft->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, iconColor, 21));
        m_btnImgRotateLeft->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnImgRotateRight) {
        m_btnImgRotateRight->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, iconColor, 21));
        m_btnImgRotateRight->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnImgFlipH) {
        m_btnImgFlipH->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::FlipH, iconColor, 21));
        m_btnImgFlipH->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnImgFlipV) {
        m_btnImgFlipV->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::FlipV, iconColor, 21));
        m_btnImgFlipV->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnImgCrop) {
        m_btnImgCrop->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Crop, iconColor, 21));
        m_btnImgCrop->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnImgStudio) {
        m_btnImgStudio->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, isLight ? QColor("#0284c7") : QColor("#38bdf8"), 20));
        m_btnImgStudio->setStyleSheet(contextualPillStyle);
    }

    // New PDF Clean 4-Pill cluster styles (height 34px, 8px radius)
    QString pdfPillBtnStyle = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   padding: 0px 14px;"
        "   font-size: 12.5px;"
        "   font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QPushButton:checked {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   border-color: #0284c7;"
        "   font-weight: 600;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "#1e293b" : "#f1f5f9",
        isLight ? "rgba(0, 0, 0, 0.08)" : "rgba(255, 255, 255, 0.10)",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.16)" : "rgba(255, 255, 255, 0.20)"
    );

    QString pdfPillToolBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   padding: 0px 12px;"
        "   font-size: 12.5px;"
        "   font-weight: 500;"
        "}"
        "QToolButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QToolButton::menu-indicator {"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: right center;"
        "   right: 4px;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "#1e293b" : "#f1f5f9",
        isLight ? "rgba(0, 0, 0, 0.08)" : "rgba(255, 255, 255, 0.10)",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.16)" : "rgba(255, 255, 255, 0.20)"
    );

    if (m_btnPdfText) {
        m_btnPdfText->setStyleSheet(pdfPillBtnStyle);
    }
    if (m_btnPdfHighlight) {
        m_btnPdfHighlight->setStyleSheet(pdfPillBtnStyle);
    }
    if (m_btnPdfShapes) {
        m_btnPdfShapes->setStyleSheet(pdfPillToolBtnStyle);
    }
    if (m_btnPdfSign) {
        m_btnPdfSign->setStyleSheet(pdfPillToolBtnStyle);
    }
    if (m_btnPdfPages) {
        m_btnPdfPages->setStyleSheet(pdfPillToolBtnStyle);
    }
    QString orgPillStyle = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   padding: 0px 10px;"
        "   font-size: 12px;"
        "   font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %6;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "#1e293b" : "#f1f5f9",
        isLight ? "rgba(0, 0, 0, 0.08)" : "rgba(255, 255, 255, 0.10)",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.16)" : "rgba(255, 255, 255, 0.20)",
        navBtnPressed
    );

    if (m_btnOrgRotate) {
        m_btnOrgRotate->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateRight, iconColor, 15));
        m_btnOrgRotate->setStyleSheet(orgPillStyle);
    }
    if (m_btnOrgDelete) {
        m_btnOrgDelete->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Trash, isLight ? QColor("#dc2626") : QColor("#f87171"), 15));
        m_btnOrgDelete->setStyleSheet(orgPillStyle);
    }
    if (m_btnOrgInsertBlank) {
        m_btnOrgInsertBlank->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Plus, iconColor, 15));
        m_btnOrgInsertBlank->setStyleSheet(orgPillStyle);
    }
    if (m_btnOrgAddPdf) {
        m_btnOrgAddPdf->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::File, iconColor, 15));
        m_btnOrgAddPdf->setStyleSheet(orgPillStyle);
    }
    if (m_btnOrgReverse) {
        m_btnOrgReverse->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Shuffle, iconColor, 15));
        m_btnOrgReverse->setStyleSheet(orgPillStyle);
    }
    if (m_btnOrgReset) {
        m_btnOrgReset->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::RotateLeft, iconColor, 15));
        m_btnOrgReset->setStyleSheet(orgPillStyle);
    }

    // Left Edit cluster: [✕ Discard] + [↶ History]
    QString discardBtnStyle = QString(
        "QToolButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   padding: 0px 12px;"
        "   font-size: 12px;"
        "   font-weight: 500;"
        "}"
        "QToolButton:hover {"
        "   background-color: rgba(239, 68, 68, 0.18);"
        "   color: #ef4444;"
        "   border-color: #ef4444;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "#1e293b" : "#f1f5f9",
        isLight ? "rgba(0, 0, 0, 0.08)" : "rgba(255, 255, 255, 0.10)"
    );
    if (m_btnDiscard) {
        m_btnDiscard->setStyleSheet(discardBtnStyle);
    }
    if (m_btnQuickUndo) {
        m_btnQuickUndo->setStyleSheet(pdfPillToolBtnStyle);
    }
    if (m_btnHistory) {
        m_btnHistory->setStyleSheet(pdfPillToolBtnStyle);
    }
    if (m_btnExport) {
        m_btnExport->setStyleSheet(pdfPillToolBtnStyle);
    }

    if (m_contextualBar) {
        m_contextualBar->applyThemePreset(m_currentPreset);
    }

    // Legacy PDF tools compatibility

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
        m_btnTxtNumbers->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnTxtSyntax) {
        m_btnTxtSyntax->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Palette, iconColor, 21));
        m_btnTxtSyntax->setStyleSheet(editToolBtnStyle);
    }
    if (m_btnTxtWrap) {
        m_btnTxtWrap->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Wrap, iconColor, 21));
        m_btnTxtWrap->setStyleSheet(editToolBtnStyle);
    }

    // Audio tools
    QString audPillStyle = QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 8px;"
        "   padding: 0px 11px 0px 9px;"
        "   color: %3;"
        "   font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "}"
        "QPushButton:checked {"
        "   background-color: #0284c7;"
        "   border-color: #0369a1;"
        "   color: #ffffff;"
        "   font-weight: 600;"
        "}"
    ).arg(
        navBtnBg,
        isLight ? "rgba(0, 0, 0, 0.06)" : "rgba(255, 255, 255, 0.08)",
        isLight ? "#1e293b" : "#f1f5f9",
        navBtnHover,
        isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.16)"
    );

    if (m_btnAudStudio) {
        m_btnAudStudio->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Scissors, iconColor, 18));
        m_btnAudStudio->setStyleSheet(audPillStyle);
    }
    if (m_btnAudRestore) {
        m_btnAudRestore->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Signal, iconColor, 18));
        m_btnAudRestore->setStyleSheet(audPillStyle);
    }
    if (m_btnAudPitch) {
        m_btnAudPitch->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Volume, iconColor, 18));
        m_btnAudPitch->setStyleSheet(audPillStyle);
    }
    if (m_btnAudEq) {
        m_btnAudEq->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Equalizer, iconColor, 18));
        m_btnAudEq->setStyleSheet(audPillStyle);
    }
    if (m_btnAudTags) {
        m_btnAudTags->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Tag, iconColor, 18));
        m_btnAudTags->setStyleSheet(audPillStyle);
    }
    if (m_btnAudSummary) {
        m_btnAudSummary->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Test, iconColor, 18));
        m_btnAudSummary->setStyleSheet(audPillStyle);
    }

    // Dividers between tool clusters
    QString sepStyle = QString("background-color: %1; border: none;").arg(
        isLight ? "rgba(0, 0, 0, 0.12)" : "rgba(255, 255, 255, 0.12)"
    );
    if (m_sepPdf1) m_sepPdf1->setStyleSheet(sepStyle);
    if (m_sepPdf2) m_sepPdf2->setStyleSheet(sepStyle);
    if (m_sepImg1) m_sepImg1->setStyleSheet(sepStyle);
    if (m_sepArc1) m_sepArc1->setStyleSheet(sepStyle);
    if (m_sepExport) m_sepExport->setStyleSheet(sepStyle);

    // Primary Edit / Done Capsule Pill Button (32px height -> 16px radius)
    if (m_btnEdit) {
        QString primaryBg = isLight ? "#007aff" : "#2563eb";
        QString primaryHover = isLight ? "#0062cc" : "#3b82f6";
        QString primaryPressed = isLight ? "#004999" : "#1d4ed8";
        QString primaryStyle = QString(
            "QPushButton {"
            "   background-color: %1;"
            "   color: #ffffff;"
            "   font-weight: 600;"
            "   font-size: 12.5px;"
            "   border: none;"
            "   border-radius: 16px;"
            "   padding: 0px 16px 0px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: %2;"
            "}"
            "QPushButton:pressed {"
            "   background-color: %3;"
            "}"
        ).arg(primaryBg, primaryHover, primaryPressed);

        if (m_pageOrganizerMode) {
            m_btnEdit->setText(tr("✓ Done"));
            m_btnEdit->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Check, Qt::white, 16));
        } else if (!m_editMode) {
            m_btnEdit->setText(tr("Edit"));
            m_btnEdit->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Edit, Qt::white, 16));
        } else {
            m_btnEdit->setText(tr("✓ Confirm"));
            m_btnEdit->setIcon(VectorIcons::createUiIcon(VectorIcons::UiIcon::Check, Qt::white, 16));
        }
        m_btnEdit->setStyleSheet(primaryStyle);
    }

    updateTrafficLightIcons(m_trafficLightsHovered);
}

void MinimalHeaderBar::updateTrafficLightIcons(bool hovered)
{
    m_trafficLightsHovered = hovered;
}

void MinimalHeaderBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    QColor bg;
    QColor borderBottom;

    switch (m_currentPreset) {
    case ThemePreset::ModernLight:
        bg = QColor("#f5f5f7");
        borderBottom = QColor(0, 0, 0, 16);
        break;
    case ThemePreset::BreezeLight:
        bg = QColor("#eff0f1");
        borderBottom = QColor(0, 0, 0, 16);
        break;
    case ThemePreset::BreezeDark:
        bg = QColor("#1f2226");
        borderBottom = QColor(255, 255, 255, 13);
        break;
    case ThemePreset::DarkSlate:
    default:
        bg = QColor("#1a1d23");
        borderBottom = QColor(255, 255, 255, 13);
        break;
    }

    // Fill header bar background
    p.fillRect(rect(), bg);

    // Draw ultra-subtle hairline divider at bottom (indented 14px from edges for a softer feel)
    p.setPen(borderBottom);
    p.drawLine(14, height() - 1, width() - 14, height() - 1);
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

    if (watched == m_windowControlsWidget) {
        if (event->type() == QEvent::Enter) {
            updateTrafficLightIcons(true);
        } else if (event->type() == QEvent::Leave) {
            updateTrafficLightIcons(false);
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // If clicked on margin/padding area rather than traffic light buttons, initiate window move
                QWidget *child = m_windowControlsWidget->childAt(me->pos());
                if (!child || child == m_windowControlsWidget) {
                    mousePressEvent(me);
                    return true;
                }
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
