#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QStyledItemDelegate>
#include <QThread>
#include <QPointer>
#include <QHash>
#include <QMap>
#include <QSet>
#include <atomic>

class QLineEdit;
class QComboBox;
class QStackedWidget;
class QTimer;
class IFolderStudioWidget;

/**
 * @brief Background worker thread for asynchronous folder thumbnail generation.
 *
 * Extracts and downscales raster images, renders first-page PDF previews via Poppler,
 * and paints micro-text document sheets without blocking the GUI thread.
 */
class ThumbnailWorker : public QThread
{
    Q_OBJECT
public:
    explicit ThumbnailWorker(const QStringList &filePaths, QObject *parent = nullptr);
    ~ThumbnailWorker() override;

    /**
     * @brief Signals the worker loop to abort and waits for thread completion.
     */
    void cancelAndWait();

    /**
     * @brief Generates a crisp paper sheet preview with real micro-text for text/code files.
     * @param filePath Path to the text or code file.
     * @return QImage containing the rendered document sheet with folded corner and micro-text lines.
     */
    static QImage renderTextThumbnail(const QString &filePath);

    /**
     * @brief Synchronously generates a thumbnail image for a single supported file.
     * @param filePath Absolute path to the source file.
     * @return Rendered thumbnail QImage, or null image if unsupported or unreadable.
     */
    static QImage renderFileThumbnail(const QString &filePath);

signals:
    /**
     * @brief Emitted when a thumbnail has been generated for a file.
     * @param filePath Absolute path to the source file.
     * @param image Generated thumbnail image.
     */
    void thumbnailReady(const QString &filePath, const QImage &image);

protected:
    void run() override;

private:
    QStringList m_filePaths;
    std::atomic<bool> m_isCancelled;
};

/**
 * @brief Custom item delegate that renders folder gallery items as modern elevated cards.
 */
class FolderCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit FolderCardDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

/**
 * @brief High-speed folder gallery viewer displaying visual thumbnail cards.
 *
 * Implements asynchronous thumbnail rendering on a background QThread,
 * custom card-style delegate painting, up-directory navigation, and double-click file preview.
 */
class FolderViewer : public QWidget
{
    Q_OBJECT

public:
    explicit FolderViewer(QWidget *parent = nullptr);
    ~FolderViewer() override;

    /**
     * @brief Scans and populates the folder gallery for a directory path.
     * Starts background asynchronous thumbnail extraction.
     * @param folderPath Absolute filesystem directory path.
     * @param forceReload If false and folderPath is already loaded, preserves current items and scroll.
     * @return True if directory exists and was loaded; false otherwise.
     */
    bool loadFolder(const QString &folderPath, bool forceReload = false);

    /**
     * @brief Selects an item by file path without forcing unnecessary scrolling if already visible.
     * @param filePath Absolute path of the file.
     * @param ensureVisible If true and item is currently outside visible area, scrolls gently to it.
     */
    void selectItemByPath(const QString &filePath, bool ensureVisible = false);

    /**
     * @brief Clears list items, cancels active thumbnail workers, and resets counters.
     */
    void clear();

    /**
     * @brief Updates colors and border styles to match active theme preset.
     */
    void updateTheme();

    /** @brief Returns the total number of visible items (folders + files) in the current directory. */
    int itemCount() const { return m_itemCount; }

    /** @brief Returns the combined byte size of all files in the current directory. */
    qint64 totalSize() const { return m_totalSize; }

    /** @brief Returns the canonical path of the currently loaded directory. */
    QString currentFolder() const { return m_currentFolder; }

    /** @brief Returns the central card grid list widget. */
    QListWidget *listWidget() const { return m_listWidget; }

    /** @brief Returns the parent directory navigation button. */
    QPushButton *upButton() const { return m_btnUp; }

    /** @brief Returns whether subfolders are included in the gallery alongside files. */
    bool showFolders() const { return m_showFolders; }

    /** @brief Sets whether subfolders are displayed in the gallery. */
    void setShowFolders(bool show);

    /**
     * @brief Returns the visual card rectangle of an item in coordinates of FolderViewer.
     * Used for morphing zoom transitions from/to the gallery.
     * @param filePath Absolute path of the file.
     * @return Rectangle in local coordinates, or a sensible center fallback.
     */
    QRect cardRectForPath(const QString &filePath) const;

    /**
     * @brief Returns the exact thumbnail/icon rectangle inside the card in coordinates of FolderViewer.
     * @param filePath Absolute path of the file.
     * @return Exact thumbnail/icon rectangle in local coordinates.
     */
    QRect thumbnailRectForPath(const QString &filePath) const;

    /**
     * @brief Ensures a card item has its thumbnail generated synchronously before visual transitions.
     * @param filePath Absolute path of the file.
     */
    void ensureThumbnailForPath(const QString &filePath);

    /**
     * @brief Temporarily suppresses rendering of a specific card during morphing zoom transition.
     * @param filePath Absolute path of the item to hide, or empty string to show all.
     * @param hidden True to hide, false to restore.
     */
    void setCardHiddenForTransition(const QString &filePath, bool hidden);

    /**
     * @brief Returns the file path of the card currently hidden for transition, if any.
     */
    QString cardHiddenForTransition() const { return m_cardHiddenForTransition; }

    /**
     * @brief Returns the absolute path of the currently selected card in the gallery.
     * @return File or folder path, or empty string if no selection.
     */
    QString selectedFilePath() const;

    /**
     * @brief Toggles Folder Studio Edit Mode (loads bubez_folder.so lazily on demand).
     * @param editMode True to display Folder Studio; false to return to the gallery grid.
     */
    void setEditMode(bool editMode);

    /**
     * @brief Returns whether Folder Studio Edit Mode is currently active.
     */
    bool isEditMode() const { return m_isEditMode; }

    /**
     * @brief Switches the active workspace tool inside Folder Studio Edit Mode.
     * @param toolIndex Tool index (0=Duplicates, 1=Recovery, 2=Organize, 3=Batch).
     */
    void setStudioTool(int toolIndex);

    /**
     * @brief Returns the active Folder Studio tool index (0=Duplicates, 1=Recovery, 2=Organize, 3=Batch).
     */
    int activeStudioTool() const { return m_activeStudioTool; }

    /**
     * @brief Returns the ordered list of currently visible (filtered) non-directory file paths in the gallery.
     * @return List of absolute file paths currently visible in the folder grid.
     */
    QStringList visibleFilePaths() const;

    /**
     * @brief Notifies the embedded Folder Studio widget that a file was removed externally (e.g. during preview).
     * @param filePath Absolute path of the removed file.
     */
    void notifyStudioFileRemoved(const QString &filePath);

    /**
     * @brief Synchronously detaches and destroys the Folder Studio widget (safe before plugin unload).
     */
    void detachStudioWidget();

public slots:
    /** @brief Navigates to the parent directory of the current folder if one exists. */
    void goUpDirectory();

    /**
     * @brief Updates thumbnail icon for a single file card in the gallery.
     * @param filePath Absolute path of the file.
     * @param image Updated thumbnail image.
     */
    void updateThumbnail(const QString &filePath, const QImage &image);

signals:
    /**
     * @brief Emitted when a file card is double-clicked to open it in its primary viewer.
     * @param filePath Absolute path to the file.
     * @param contextFiles Optional ordered list of scoped files for Prev/Next navigation
     *                     (e.g. duplicate group items or currently filtered gallery files).
     */
    void filePreviewRequested(const QString &filePath, const QStringList &contextFiles = QStringList());

    /**
     * @brief Emitted when a folder has finished scanning items.
     * @param folderPath Absolute path of the folder.
     * @param count Number of child items found.
     * @param totalSize Combined size of all files in bytes.
     */
    void folderLoaded(const QString &folderPath, int count, qint64 totalSize);

    /**
     * @brief Emitted when a folder item card selection has changed.
     * @param selectedFilePath Absolute path of the newly selected item, or empty string.
     */
    void selectionChanged(const QString &selectedFilePath);

    /**
     * @brief Emitted when a context menu is requested on the folder card grid.
     * @param globalPos Global screen coordinates where right-click occurred.
     * @param selectedFilePath Absolute path of the right-clicked item, or empty string.
     */
    void itemContextMenuRequested(const QPoint &globalPos, const QString &selectedFilePath);

    /**
     * @brief Emitted when the user clicks the Trash button in the folder top bar.
     */
    void trashSelectedRequested();

    /**
     * @brief Emitted when the user presses I to open the unified Properties Inspector.
     */
    void propertiesRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onItemSelectionChanged();
    void onThumbnailReady(const QString &filePath, const QImage &image);
    void onFilterTextChanged(const QString &text);
    void applyCombinedFilter();
    void triggerAsyncDeepFilter();

private:
    void setupUi();
    void ensureStudioWidget();
    QString formatSize(qint64 bytes) const;
    QIcon getFolderIcon(const QString &path) const;
    QIcon getFileIcon(const QString &path) const;

    QStackedWidget *m_viewStack = nullptr;
    QWidget *m_galleryPage = nullptr;
    QWidget *m_studioContainer = nullptr;
    IFolderStudioWidget *m_studioWidget = nullptr;
    QLabel *m_lblStudioFallback = nullptr;

    QWidget *m_topBar;
    QPushButton *m_btnUp;
    QPushButton *m_btnShowFolders;
    QPushButton *m_btnTrash;
    QFrame *m_pillBadge;
    QLabel *m_pillIcon;
    QLabel *m_pillText;
    QComboBox *m_comboCategoryFilter = nullptr;
    QLineEdit *m_filterEdit;
    QPushButton *m_btnDeepFilter = nullptr;
    QTimer *m_deepFilterTimer = nullptr;

    // Main Card Grid
    QListWidget *m_listWidget;

    QPointer<ThumbnailWorker> m_worker;
    QHash<QString, QListWidgetItem*> m_pathToItem;
    QMap<QString, int> m_folderScrollPositions;
    QSet<QString> m_deepMatchedPaths;
    bool m_hasDeepResults = false;

    QString m_currentFolder;
    QString m_cardHiddenForTransition;
    quint64 m_loadGeneration = 0;
    std::atomic<quint64> m_deepFilterGeneration{0};
    bool m_showFolders = false;
    bool m_isEditMode = false;
    int m_activeStudioTool = 0;
    int m_itemCount;
    qint64 m_totalSize;
};
