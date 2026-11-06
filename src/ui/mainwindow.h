#pragma once

#include "base_skin.h"
#include "update_manager.h"
#include <QAction>
#include <QPushButton>
#include <QHBoxLayout>

/**
 * @brief Default desktop skin of BubezView.
 *
 * Implements a modern canvas-first GUI featuring:
 * - MinimalHeaderBar with integrated window controls, title/badges, file navigation, zoom popup, and action buttons.
 * - Dynamic contextual edit toolbars (Text, Stamp, Markup, Crop, Audio Studio).
 * - Interactive breadcrumb path navigation bar in the status bar.
 * - Global window-level keyboard shortcuts for fast operation.
 */
class MinimalHeaderBar;

class MainWindow : public BaseSkin {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief Toggles between Quick Look Preview mode and full Editing mode.
     */
    void setEditMode(bool editMode) override;
    MinimalHeaderBar *minimalHeaderBar() const { return m_minimalHeaderBar; }
    bool isFrameless() const { return m_isFrameless; }

    /**
     * @brief Enters full panoramic PDF page organizer mode in maximized view.
     */
    void enterPageOrganizerMode();

    /**
     * @brief Exits page organizer mode, applying pending changes and restoring pre-organizer geometry.
     * @param targetPageIndex Page index to navigate to in normal view (-1 for unchanged).
     */
    void exitPageOrganizerMode(int targetPageIndex = -1);

    /**
     * @brief Calculates which border edges intersect with the given global position.
     * @param globalPos Position in global screen coordinates.
     * @return Flags representing active resize edges (Qt::LeftEdge, Qt::RightEdge, etc.).
     */
    Qt::Edges calculateResizeEdges(const QPoint &globalPos) const;

    /**
     * @brief Updates application cursor to match active resize direction.
     * @param edges Flags indicating active resize borders.
     */
    void updateResizeCursor(Qt::Edges edges);

    /**
     * @brief Executes undo on the currently active viewer.
     */
    void undo();

    /**
     * @brief Executes redo on the currently active viewer.
     */
    void redo();

protected:
    /**
     * @brief Updates header bar title, badges, and contextual tools when a file is loaded.
     */
    void onFileLoaded(const QString &filePath, FileCategory category) override;
    void updateWindowTitle() override;
    void onViewerInitialized(FileCategory category) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void applyCurrentTheme() override;

private:
    void setupUi();
    void setupGlobalShortcuts();
    void updateStatusBar() override;

    MinimalHeaderBar *m_minimalHeaderBar;
    QWidget *m_breadcrumbWidget;
    QHBoxLayout *m_breadcrumbLayout;
    QLabel *m_fileIndexLabel;
    QLabel *m_fileInfoLabel;
    QLabel *m_statusLabel;

    void updateBreadcrumbs();

    void openPdfMergeDialog();

    QPushButton *m_btnUpdateNotification;
    QTimer *m_updateBlinkTimer;
    bool m_updateBlinkState;
    ReleaseInfo m_availableRelease;

    QFrame *m_updateBanner;
    QLabel *m_lblUpdateBannerText;
    QFrame *m_debugBanner = nullptr;

    void setupUpdateChecker();
    void showUpdateDialog(const struct ReleaseInfo &info);
    void setupUpdateBanner();
    void setupDebugBanner();
    void updateBlinkEffect();

    QString currentActiveTargetFilePath() const;

    void setupPdfViewerConnections();
    void setupArchiveViewerConnections();
    void setupMediaViewerConnections();
    void setupFolderViewerConnections();
    void revertCurrentEdits();
    void showSaveExportMenu(const QPoint &globalPos);

    void performManualResize(const QPoint &globalPos);
    void updateFramelessBorderStyle();

    QWidget *m_borderOverlay = nullptr;
    QSize m_lastMaskSize;
    bool m_isFrameless = true;
    bool m_wasMaximizedBeforeEdit = false;
    QRect m_preEditGeometry;
    bool m_wasMaximizedBeforeOrganizer = false;
    QRect m_preOrganizerGeometry;
    bool m_isBorderCursorSet = false;
    bool m_isManualResizing = false;
    Qt::Edges m_manualResizeEdges;
    QPoint m_manualResizeStartPos;
    QRect m_manualResizeStartGeom;
    static constexpr int RESIZE_BORDER_MARGIN = 6;
};
