#pragma once

#include "base_skin.h"
#include "update_manager.h"
#include <QToolBar>
#include <QAction>
#include <QPushButton>
#include <QMap>
#include <QHBoxLayout>

/**
 * @brief Default desktop skin of BubezView.
 *
 * Implements a full native desktop GUI featuring:
 * - Traditional menu bar with File, Edit, View, Tools, Settings, and Help menus.
 * - Dynamic context-aware top toolbar driven by ActionRegistry.
 * - Interactive breadcrumb path navigation bar in the status bar.
 * - Dynamic Open With application integration.
 */
#include "configure_toolbar_dialog.h"
#include "flippable_toolbar.h"

class MainWindow : public BaseSkin {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    /**
     * @brief Rebuilds toolbar buttons dynamically when a new file or format category is loaded.
     */
    void onFileLoaded(const QString &filePath, FileCategory category, const QList<UIActionItem> &actions) override;

public slots:
    void applyCurrentTheme() override;
    void configureToolbars();
    void rebuildToolbar();

public:
    QStringList defaultToolbarActions() const;

private:
    void setupUi();
    void updateStatusBar() override;
    void setupToolbarActionsMap();

    QToolBar *m_toolbar;
    FlippableToolbar *m_flippableToolbar;
    QWidget *m_breadcrumbWidget;
    QHBoxLayout *m_breadcrumbLayout;
    QLabel *m_fileIndexLabel;
    QLabel *m_fileInfoLabel;
    QLabel *m_statusLabel;

    void updateBreadcrumbs();

    QMap<QString, QAction*> m_toolbarActions;
    QList<QAction*> m_dynamicActions;

    class ShareManager *m_shareManager;
    QMenu *m_menuShare;
    QMenu *m_menuOpenWith;
    QMenu *m_menuFile;
    QMenu *m_menuEdit;
    QMenu *m_menuView;
    QMenu *m_menuTools;
    QMenu *m_menuSettings;
    QMenu *m_menuHelp;

    QAction *m_actSave;
    QAction *m_actSaveAs;
    QAction *m_actExportAs;
    QAction *m_actTrashFile;
    QAction *m_actPrint;
    QAction *m_actUndo;
    QAction *m_actRedo;
    QAction *m_actCut;
    QAction *m_actCopy;
    QAction *m_actPaste;
    QAction *m_actRotateRight;
    QAction *m_actRotateLeft;
    QAction *m_actFlipH;
    QAction *m_actFlipV;
    QAction *m_actFolderRename;
    QAction *m_actFolderTrash;
    QAction *m_actFolderNew;
    QAction *m_actToggleTextEdit;
    QAction *m_actLineNumbers;
    QAction *m_actSyntaxHighlight;
    QAction *m_actWordWrap;
    QAction *m_actShowWhitespace;
    QAction *m_actGotoLine;
    QAction *m_actFindReplace;
    QAction *m_actDuplicateLine;
    QAction *m_actDeleteLine;
    QAction *m_actToggleComment;
    QAction *m_actOcr;
    QAction *m_actSearch;
    QAction *m_actPdfToc;
    QAction *m_actExtractImages;
    QAction *m_actToolsExportAs;
    QAction *m_actPrev;
    QAction *m_actNext;
    QAction *m_actFolderView;
    QAction *m_actZoomIn;
    QAction *m_actZoomOut;
    QAction *m_actZoomReset;
    QAction *m_actFit;
    QAction *m_actZoomTool;
    QAction *m_actSlideshow;
    QAction *m_actFilmstrip;
    QAction *m_actPdfToolsHub;
    QAction *m_actPdfHighlighter;
    QAction *m_actPdfTypewriter;
    QAction *m_actPdfSignature;
    QAction *m_actPdfInsertImage;
    QAction *m_actPdfInsertBlankPage;
    QAction *m_actPdfWatermark;
    QAction *m_actPdfShape;
    QAction *m_actPdfSnapshot;
    QAction *m_actPdfSelectArea;
    QAction *m_actPdfRedact;
    QMenu   *m_menuPdfMarkup;
    QMenu   *m_menuPdfDocOps;
    QAction *m_actAudioTrim;
    QAction *m_actAudioTags;
    QAction *m_actAudioExtract;
    QAction *m_actMergePdf;
    QAction *m_actRepairPdf;
    QAction *m_actOrganizePdf;

    QAction *m_actOpenFile;
    QAction *m_actOpenFolder;
    QAction *m_actOpenWith;
    QAction *m_actCopyPath;
    QAction *m_actFullscreen;
    QAction *m_actInspector;
    QAction *m_actPdfPrevPage;
    QAction *m_actPdfNextPage;
    QAction *m_actConfigureToolbars;
    QAction *m_actToggleToolbar;
    QAction *m_actSettings;
    QAction *m_actCheckUpdates;
    QAction *m_actShortcuts;
    QAction *m_actAbout;

    QMap<QString, ToolbarActionInfo> m_toolbarActionInfoMap;
    QMap<QString, QAction*> m_idToActionMap;
    QStringList m_configuredToolbarActionIds;

    // View toggles: discovery mode & unsupported files
    QAction *m_actShowUninstalledFeatures;
    QAction *m_actShowUnsupportedFiles;

    // Discovery placeholder actions
    QAction *m_actOfficeDiscovery;
    QAction *m_actQpdfDiscovery;
    QAction *m_actPdfimagesDiscovery;

    void openPdfMergeDialog();
    void updateDiscoveryActions();

    QPushButton *m_btnUpdateNotification;
    QTimer *m_updateBlinkTimer;
    bool m_updateBlinkState;
    ReleaseInfo m_availableRelease;

    QFrame *m_updateBanner;
    QLabel *m_lblUpdateBannerText;

    void setupUpdateChecker();
    void showUpdateDialog(const struct ReleaseInfo &info);
    void setupUpdateBanner();
    void updateBlinkEffect();

    void populateOpenWithMenu();
    void updateContextualMenuStates();
    QString currentActiveTargetFilePath() const;
};
