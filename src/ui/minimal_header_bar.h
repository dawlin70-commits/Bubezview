#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QFrame>
#include <QMenu>
#include "theme_manager.h"
#include "core/services/plugin_manager.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "contextual_action_bar.h"

class PdfHistoryWidget;

/**
 * @brief Lightweight, macOS Quick Look-inspired minimal header bar.
 *
 * Provides a streamlined top bar featuring:
 * - Window traffic-light controls (Close, Minimize, Maximize) with hover symbols.
 * - Active file name title and file index badge (e.g. "[3/28]").
 * - Native Wayland/X11 dragging via windowHandle()->startSystemMove().
 * - Header double-click to toggle maximize / restore.
 * - Action buttons: Share, Preferences/Settings, and prominent primary "Edit / Done" mode toggle.
 */
class MinimalHeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit MinimalHeaderBar(QWidget *parent = nullptr);
    ~MinimalHeaderBar() override = default;

    /**
     * @brief Returns a compact minimum size hint so that edit toolbars never
     *        force the parent window to expand its minimum width.
     */
    QSize minimumSizeHint() const override;

    /**
     * @brief Updates the title displayed on the header bar.
     * @param title Title text or file name.
     */
    void setTitle(const QString &title);

    /**
     * @brief Updates the file index badge text (e.g. "[3/28]").
     * @param badgeText Formatted index string, or empty to hide the badge.
     */
    void setIndexBadge(const QString &badgeText);

    /**
     * @brief Toggles between Preview Mode and Edit Mode.
     * @param editMode True for Edit Mode ("Done"), False for Preview Mode ("Edit").
     */
    void setEditMode(bool editMode);

    /**
     * @brief Checks if Edit Mode is currently active.
     */
    bool isEditMode() const { return m_editMode; }

    /**
     * @brief Toggles between normal viewing/editing and dedicated Page Organizer Mode.
     */
    void setPageOrganizerMode(bool active);
    bool isPageOrganizerMode() const { return m_pageOrganizerMode; }

    /**
     * @brief Configures visibility of client-side window traffic light controls.
     * @param visible True when running in frameless CSD mode, false when system titlebar is present.
     */
    void setWindowControlsVisible(bool visible);

    /**
     * @brief Checks if traffic light controls are enabled.
     */
    bool areWindowControlsVisible() const { return m_windowControlsVisible; }

    /**
     * @brief Re-styles header widgets and buttons to match the selected theme preset.
     * @param preset Active theme preset.
     */
    void applyThemePreset(ThemePreset preset);

    QToolButton *shareButton() const { return m_btnShare; }
    QToolButton *settingsButton() const { return m_btnSettings; }
    QToolButton *prevButton() const { return m_btnPrev; }
    QToolButton *nextButton() const { return m_btnNext; }
    QToolButton *galleryButton() const { return m_btnGallery; }
    QToolButton *searchButton() const { return m_btnSearch; }
    QToolButton *zoomButton() const { return m_btnZoom; }
    QAction *zoomInAction() const { return m_actZoomIn; }
    QAction *zoomOutAction() const { return m_actZoomOut; }
    QAction *zoomFitAction() const { return m_actZoomFit; }
    QAction *zoomResetAction() const { return m_actZoom100; }
    double zoomFactor() const { return m_currentZoomFactor; }
    void setZoomFactor(double factor);
    QToolButton *inspectorButton() const { return m_btnInspector; }
    QPushButton *editButton() const { return m_btnEdit; }

    ContextualActionBar *contextualBar() const { return m_contextualBar; }
    void showContextualTextBar(const QString &text = QString(), const QFont &font = QFont(),
                               const QColor &color = QColor(), PdfTextBgMode bgMode = PdfTextBgMode::Auto,
                               const QColor &bgColor = Qt::white);
    void showContextualHighlightBar();
    void showContextualShapesBar(int shapeType = 0);
    void showContextualRedactBar();
    void showContextualStampBar();
    void showContextualDrawingBar(int toolId = 0);
    void hideContextualBar();
    void updateHeaderGeometry();

    QToolButton *discardButton() const { return m_btnDiscard; }
    QToolButton *quickUndoButton() const { return m_btnQuickUndo; }
    QToolButton *historyButton() const { return m_btnHistory; }
    QToolButton *historyTimelineButton() const { return m_btnHistory; }
    QMenu *historyMenu() const { return m_menuHistory; }
    PdfHistoryWidget *historyWidget() const { return m_historyWidget; }
    void setHistorySteps(const QList<PdfHistoryStep> &steps);
    void showHistoryDialog();
    QToolButton *rotateButton() const { return m_btnRotate; }
    QToolButton *saveExportButton() const { return m_btnExport; }
    QPushButton *pdfTextButton() const { return m_btnPdfText; }
    QPushButton *pdfHighlightButton() const { return m_btnPdfHighlight; }
    QToolButton *pdfShapesButton() const { return m_btnPdfShapes; }
    QToolButton *pdfSignButton() const { return m_btnPdfSign; }
    QToolButton *pdfPagesButton() const { return m_btnPdfPages; }

    /**
     * @brief Configures whether in-document search is available (e.g. enabled for Text/PDF).
     * @param available True if search button should be enabled, false otherwise.
     */
    void setSearchAvailable(bool available);

    /**
     * @brief Sets the active file category to configure contextual edit tools.
     * @param category Active category (Image, Pdf, Archive, Text, Media, etc.)
     */
    void setCategory(FileCategory category);

    /**
     * @brief Returns current file category.
     */
    FileCategory category() const { return m_currentCategory; }

    /**
     * @brief Sets check states for text toggle buttons.
     */
    void setTextEditStates(bool lineNumbers, bool syntax, bool wordWrap);

    /**
     * @brief Updates the active check state in the audio tool capsule bar.
     * @param index Active audio workspace index (1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags, 6=Summary).
     */
    void setActiveAudioWorkspace(int index);

    /**
     * @brief Returns current active audio workspace index (1..6) or -1 if not set.
     */
    int activeAudioWorkspace() const;

    /**
     * @brief Displays or clears the modification dot indicator (•) on an audio workspace button.
     * @param index Workspace index (1..5).
     * @param modified True if changes are active on that workspace.
     */
    void setAudioWorkspaceModified(int index, bool modified);

    /**
     * @brief Declares whether the current media file supports the audio workspaces.
     *
     * The Player/Equalizer/Tags/Trim/Restore capsules are shown only for audio-only
     * files; video files share the `Media` category but have no workspaces.
     * @param available True if the loaded media file is audio-only.
     */
    void setAudioWorkspaceToolsAvailable(bool available);

signals:
    void prevRequested();
    void nextRequested();
    void galleryRequested();
    void searchRequested();
    void inspectorRequested();
    void editToggled(bool editMode);
    void discardEditRequested();
    void shareRequested();
    void saveExportRequested(const QPoint &globalPos);
    void saveAsRequested();
    void settingsRequested();
    void closeRequested();
    void minimizeRequested();
    void maximizeRestoreRequested();

    // Contextual Action Signals
    // Image
    void rotateLeftRequested();
    void rotateRightRequested();
    void flipHRequested();
    void flipVRequested();
    void cropRequested();

    // PDF
    void pdfStudioRequested();
    void pdfHighlightRequested();
    void pdfSelectAreaRequested();
    void pdfDrawRequested();
    void pdfTypewriterRequested();
    void pdfSignatureRequested();
    void pdfShapeRequested();
    void pdfStampRequested();
    void pdfRedactRequested();
    void pdfPagesRequested();
    void pdfOrganizerRotateRequested();
    void pdfOrganizerDeleteRequested();
    void pdfOrganizerInsertBlankRequested();
    void pdfOrganizerAddPdfRequested();
    void pdfOrganizerReverseRequested();
    void pdfOrganizerResetRequested();
    void pdfOrganizerBackRequested();
    void pdfRepairRequested();
    void pdfInsertImageRequested();
    void pdfCryptoSignatureRequested();
    void pdfScanSignatureFieldsRequested();
    void pdfInsertBlankPageRequested();
    void pdfDeletePageRequested();
    void pdfMergeRequested();
    void pdfTextToolToggled(bool active);
    void pdfHighlightToolToggled(bool active);
    void pdfShapesToolRequested(int shapeType);
    void pdfRedactToolRequested();
    void pdfStampToolRequested();
    void pdfDrawingToolRequested(int toolId);
    void pdfSavePageAsPdfRequested();
    void pdfSavePageAsImageRequested();
    void pdfOcrRequested();
    void pdfExtractImagesRequested();

    // Zoom Controls
    void zoomInRequested();
    void zoomOutRequested();
    void zoomFitRequested();
    void zoomResetRequested();
    void zoomSetPercentRequested(int percent);

    void undoRequested();
    void redoRequested();
    void historyStepRestoreRequested(int index);
    void showHistoryTimelineRequested();
    void pdfTextPropertiesChanged(const QFont &font, const QColor &color, bool whiteout);
    void pdfTextBgChanged(PdfTextBgMode mode, const QColor &color);
    void pdfTextDoneRequested();
    void pdfTextReplaceRequested(const QString &text, const QFont &font, const QColor &color, bool whiteout);

    // Archive
    void archiveExtractRequested();
    void archiveAddRequested();
    void archiveRemoveRequested();
    void archiveTestRequested();

    // Text
    void textLineNumbersToggled(bool checked);
    void textSyntaxToggled(bool checked);
    void textWordWrapToggled(bool checked);
    void textFindRequested();

    // Audio
    void audioStudioRequested();
    void audioRestoreRequested();
    void audioPitchRequested();
    void audioEqualizerRequested();
    void audioTagsRequested();
    void audioSummaryRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupEditTools();
    void setupPageOrganizerTools();
    void updateButtonStyles();
    void updateTrafficLightIcons(bool hovered);
    void updateContextualToolsVisibility();
    void updateDisplayTitle();
    void startCenterTransition(int targetIndex);

    QString m_rawTitle;
    bool m_editMode = false;
    bool m_pageOrganizerMode = false;
    bool m_windowControlsVisible = true;
    bool m_trafficLightsHovered = false;
    bool m_isDragging = false;
    QPoint m_dragStartPosition;
    QPoint m_windowStartPosition;
    ThemePreset m_currentPreset = ThemePreset::DarkSlate;
    FileCategory m_currentCategory = FileCategory::Unknown;
    bool m_audioToolsAvailable = false;

    QWidget *m_primaryBar = nullptr;
    ContextualActionBar *m_contextualBar = nullptr;
    QWidget *m_windowControlsWidget = nullptr;
    QWidget *m_editLeftWidget = nullptr;
    QToolButton *m_btnDiscard = nullptr;
    QToolButton *m_btnQuickUndo = nullptr;
    QToolButton *m_btnHistory = nullptr;
    QToolButton *m_btnClose = nullptr;

    QStackedWidget *m_centerStack = nullptr;
    QWidget *m_previewCenterWidget = nullptr;
    QWidget *m_editToolContainer = nullptr;
    QGraphicsOpacityEffect *m_centerOpacityEffect = nullptr;
    QPropertyAnimation *m_centerFadeAnim = nullptr;

    QLabel *m_lblTitle = nullptr;
    QLabel *m_lblBadge = nullptr;
    QToolButton *m_btnPrev = nullptr;
    QToolButton *m_btnNext = nullptr;
    QToolButton *m_btnGallery = nullptr;

    // Image Tool Buttons
    QToolButton *m_btnImgRotateLeft = nullptr;
    QToolButton *m_btnImgRotateRight = nullptr;
    QToolButton *m_btnImgFlipH = nullptr;
    QToolButton *m_btnImgFlipV = nullptr;
    QToolButton *m_btnImgCrop = nullptr;

    // PDF Clean 5-Pill Editing Cluster
    QPushButton *m_btnPdfText = nullptr;
    QPushButton *m_btnPdfHighlight = nullptr;
    QToolButton *m_btnPdfShapes = nullptr;
    QToolButton *m_btnPdfSign = nullptr;
    QToolButton *m_btnPdfPages = nullptr;

    // PDF Menus
    QMenu *m_menuHistory = nullptr;
    PdfHistoryWidget *m_historyWidget = nullptr;
    QMenu *m_menuShapes = nullptr;
    QMenu *m_menuSignature = nullptr;
    QMenu *m_menuPages = nullptr;
    QMenu *m_menuExport = nullptr;
    QAction *m_actPagesRepair = nullptr;

    // PDF Legacy Tool Buttons (preserved for compatibility)
    QPushButton *m_btnPdfStudio = nullptr;
    QToolButton *m_btnPdfSelectArea = nullptr;
    QToolButton *m_btnPdfDraw = nullptr;
    QToolButton *m_btnPdfType = nullptr;
    QToolButton *m_btnPdfShape = nullptr;
    QToolButton *m_btnPdfStamp = nullptr;
    QToolButton *m_btnPdfRedact = nullptr;
    QToolButton *m_btnPdfRepair = nullptr;

    // Page Organizer Bar Controls (unified single header bar)
    QWidget *m_pageOrganizerToolContainer = nullptr;
    QPushButton *m_btnOrgRotate = nullptr;
    QPushButton *m_btnOrgDelete = nullptr;
    QPushButton *m_btnOrgInsertBlank = nullptr;
    QPushButton *m_btnOrgAddPdf = nullptr;
    QPushButton *m_btnOrgReverse = nullptr;
    QPushButton *m_btnOrgReset = nullptr;

    // Archive Tool Buttons
    QPushButton *m_btnArcExtract = nullptr;
    QPushButton *m_btnArcAdd = nullptr;
    QPushButton *m_btnArcRemove = nullptr;
    QPushButton *m_btnArcTest = nullptr;

    // Text Tool Buttons
    QToolButton *m_btnTxtNumbers = nullptr;
    QToolButton *m_btnTxtSyntax = nullptr;
    QToolButton *m_btnTxtWrap = nullptr;

    // Audio Tool Buttons (1=Trim, 2=Filters, 3=Pitch & Ch., 4=Equalizer, 5=Tags, 6=Summary)
    QPushButton *m_btnAudStudio = nullptr;
    QPushButton *m_btnAudRestore = nullptr;
    QPushButton *m_btnAudPitch = nullptr;
    QPushButton *m_btnAudEq = nullptr;
    QPushButton *m_btnAudTags = nullptr;
    QPushButton *m_btnAudSummary = nullptr;
    class QButtonGroup *m_audToolsGroup = nullptr;

    QToolButton *m_btnSearch = nullptr;
    QToolButton *m_btnZoom = nullptr;
    QToolButton *m_btnRotate = nullptr;
    QMenu *m_menuZoom = nullptr;
    QAction *m_actZoomIn = nullptr;
    QAction *m_actZoomOut = nullptr;
    QAction *m_actZoomFit = nullptr;
    QAction *m_actZoom100 = nullptr;
    QList<QAction*> m_presetZoomActions;
    double m_currentZoomFactor = 1.0;
    QToolButton *m_btnInspector = nullptr;
    QToolButton *m_btnShare = nullptr;
    QToolButton *m_btnExport = nullptr;
    QToolButton *m_btnSettings = nullptr;
    QPushButton *m_btnEdit = nullptr;

    // Separators between functional tool clusters
    QFrame *m_sepPdf1 = nullptr;
    QFrame *m_sepPdf2 = nullptr;
    QFrame *m_sepImg1 = nullptr;
    QFrame *m_sepExport = nullptr;
    QFrame *m_sepArc1 = nullptr;
};
