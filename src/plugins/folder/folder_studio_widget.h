/**
 * @file folder_studio_widget.h
 * @brief Embedded Folder Studio UI widget implemented inside bubez_folder.so.
 *
 * Provides four universal, multi-format folder workflows:
 * - Duplicates: exact SHA-256 + document SimHash + audio/video + similar images + archive-vs-folder
 * - Recovery: PhotoRec / TestDisk magic-byte format detection, corruption check, junk filter & smart naming
 * - Tree Organizer: smart folder reorganization & flattening with Dry-Run preview & Undo rollback
 * - Batch Tools: token-based smart batch rename + empty-dir / broken-symlink / temp-junk cleaner
 */

#pragma once

#include "core/interfaces/i_folder_studio_plugin.h"
#include "folder_studio_engine.h"

#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QFrame>
#include <QHash>
#include <QMultiHash>
#include <QPixmap>
#include <QPointer>
#include <QVector>
#include <atomic>
#include <cstdint>
#include <memory>

class StudioFileCard;
class StudioFlowLayout;

/**
 * @class FolderStudioWidget
 * @brief Multi-format Folder Studio workspace hosted inside FolderViewer Edit Mode.
 */
class FolderStudioWidget : public IFolderStudioWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the Folder Studio UI for a given root directory.
     * @param folderPath Initial directory path.
     * @param parent Optional parent widget.
     */
    explicit FolderStudioWidget(const QString &folderPath, QWidget *parent = nullptr);
    ~FolderStudioWidget() override;

    /** @copydoc IFolderStudioWidget::setTargetFolder */
    void setTargetFolder(const QString &folderPath) override;

    /** @copydoc IFolderStudioWidget::setActiveTool */
    void setActiveTool(FolderStudioTool tool) override;

    /** @copydoc IFolderStudioWidget::updateTheme */
    void updateTheme() override;

    /** @copydoc IFolderStudioWidget::cancelActiveOperations */
    void cancelActiveOperations() override;

    /** @copydoc IFolderStudioWidget::setOnFolderModified */
    void setOnFolderModified(std::function<void()> cb) override;

    /** @copydoc IFolderStudioWidget::setOnPreviewFileRequested */
    void setOnPreviewFileRequested(std::function<void(const QString &, const QStringList &)> cb) override;

    /** @copydoc IFolderStudioWidget::notifyFileRemoved */
    void notifyFileRemoved(const QString &filePath) override;

private:
    void setupUi();
    QWidget *buildDuplicatesPage();
    QWidget *buildRecoveryPage();
    QWidget *buildOrganizerPage();
    QWidget *buildBatchPage();

    // Async workflow triggers
    void startDuplicatesScan();
    void populateDuplicatesGallery(const QList<DuplicateGroup> &groups);
    void clearDuplicatesGallery();
    void trashCheckedDuplicates();

    void startRecoveryScan();
    void populateRecoveryGallery(const QList<RecoveryItem> &items);
    void clearRecoveryGallery();
    void applyRecoveryRenames();
    void trashRecoveryJunkAndCorrupt();

    // Visual thumbnail card pipeline
    void requestThumbnailsForPaths(const QStringList &paths);
    void applyThumbnailToCards(const QString &filePath, const QPixmap &pixmap);

    void startOrganizePreview();
    void populateOrganizeTable(const QList<MovePlanItem> &plan);
    void applyOrganizePlan();

    void startBatchRenamePreview();
    void populateBatchRenameTable(const QList<MovePlanItem> &plan);
    void applyBatchRenamePlan();

    void startCleanupScan();
    void populateCleanupTable(const QList<CleanupItem> &items);
    void trashCheckedCleanupItems();

    void triggerUndoLastOperation();
    void refreshUndoButtons();
    void setBusy(bool busy, const QString &statusText = QString());
    QString relativeDisplayPath(const QString &fullPath) const;

    QString m_currentFolder;
    FolderStudioTool m_activeTool = FolderStudioTool::Duplicates;
    std::shared_ptr<std::atomic<bool>> m_aliveFlag;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    std::shared_ptr<std::atomic<uint64_t>> m_thumbGeneration;
    bool m_isBusy = false;

    std::function<void()> m_onFolderModified;
    std::function<void(const QString &, const QStringList &)> m_onPreviewFileRequested;

    // Shared thumbnail cache & active visual card registry
    QHash<QString, QPixmap> m_thumbCache;
    QMultiHash<QString, QPointer<StudioFileCard>> m_activeCards;

    // Shared top status / progress banner
    QFrame *m_statusBanner = nullptr;
    QLabel *m_lblFolderContext = nullptr;
    QLabel *m_lblStatus = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_btnCancelScan = nullptr;

    QStackedWidget *m_stack = nullptr;

    // Page 0: Duplicates (Grouped visual thumbnail cards)
    QCheckBox *m_chkDupExact = nullptr;
    QCheckBox *m_chkDupDocs = nullptr;
    QCheckBox *m_chkDupMedia = nullptr;
    QCheckBox *m_chkDupImages = nullptr;
    QCheckBox *m_chkDupArchive = nullptr;
    QCheckBox *m_chkDupRecursive = nullptr;
    QSlider *m_sliderImgSim = nullptr;
    QLabel *m_lblImgSimVal = nullptr;
    QPushButton *m_btnDupScan = nullptr;
    QPushButton *m_btnDupTrash = nullptr;
    QScrollArea *m_dupScrollArea = nullptr;
    QWidget *m_dupContainer = nullptr;
    QLabel *m_lblDupEmpty = nullptr;
    QList<DuplicateGroup> m_duplicateGroups;
    QVector<QPointer<StudioFileCard>> m_dupCards;

    // Page 1: Recovery (PhotoRec / TestDisk visual thumbnail cards)
    QCheckBox *m_chkRecRecursive = nullptr;
    QSpinBox *m_spinJunkKb = nullptr;
    QPushButton *m_btnRecScan = nullptr;
    QPushButton *m_btnRecFix = nullptr;
    QPushButton *m_btnRecTrash = nullptr;
    QPushButton *m_btnRecUndo = nullptr;
    QLabel *m_lblRecSummary = nullptr;
    QScrollArea *m_recScrollArea = nullptr;
    QWidget *m_recContainer = nullptr;
    StudioFlowLayout *m_recFlowLayout = nullptr;
    QLabel *m_lblRecEmpty = nullptr;
    QList<RecoveryItem> m_recoveryItems;
    QVector<QPointer<StudioFileCard>> m_recCards;

    // Page 2: Tree Organizer
    QComboBox *m_comboOrgMode = nullptr;
    QCheckBox *m_chkOrgRecursive = nullptr;
    QPushButton *m_btnOrgPreview = nullptr;
    QPushButton *m_btnOrgApply = nullptr;
    QPushButton *m_btnOrgUndo = nullptr;
    QLabel *m_lblOrgSummary = nullptr;
    QTableWidget *m_tableOrganize = nullptr;
    QList<MovePlanItem> m_organizePlan;

    // Page 3: Batch Tools (Rename + Cleanup)
    QTabWidget *m_batchTabs = nullptr;
    QLineEdit *m_editRenamePattern = nullptr;
    QLineEdit *m_editRenameFind = nullptr;
    QLineEdit *m_editRenameReplace = nullptr;
    QCheckBox *m_chkRenameRegex = nullptr;
    QCheckBox *m_chkRenameRecursive = nullptr;
    QPushButton *m_btnRenamePreview = nullptr;
    QPushButton *m_btnRenameApply = nullptr;
    QPushButton *m_btnRenameUndo = nullptr;
    QTableWidget *m_tableRename = nullptr;
    QList<MovePlanItem> m_renamePlan;

    QCheckBox *m_chkCleanRecursive = nullptr;
    QPushButton *m_btnCleanScan = nullptr;
    QPushButton *m_btnCleanTrash = nullptr;
    QTableWidget *m_tableCleanup = nullptr;
    QList<CleanupItem> m_cleanupItems;
};
