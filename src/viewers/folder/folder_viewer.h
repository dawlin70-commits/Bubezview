#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QStyledItemDelegate>
#include <QThread>
#include <QPointer>
#include <atomic>

class QPlainTextEdit;

class ThumbnailWorker : public QThread
{
    Q_OBJECT
public:
    explicit ThumbnailWorker(const QStringList &filePaths, QObject *parent = nullptr);
    ~ThumbnailWorker() override;

    void cancelAndWait();

    /**
     * @brief Generates a crisp paper sheet preview with real micro-text for text/code files.
     * @param filePath Path to the text or code file.
     * @return QImage containing the rendered document sheet with folded corner and micro-text lines.
     */
    static QImage renderTextThumbnail(const QString &filePath);

signals:
    void thumbnailReady(const QString &filePath, const QImage &image);

protected:
    void run() override;

private:
    QStringList m_filePaths;
    std::atomic<bool> m_isCancelled;
};

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
     * @return True if directory exists and was loaded; false otherwise.
     */
    bool loadFolder(const QString &folderPath);

    /**
     * @brief Clears list items, cancels active thumbnail workers, and resets counters.
     */
    void clear();

    /**
     * @brief Updates colors and border styles to match active theme preset.
     */
    void updateTheme();

    int itemCount() const { return m_itemCount; }
    qint64 totalSize() const { return m_totalSize; }
    QString currentFolder() const { return m_currentFolder; }
    QListWidget *listWidget() const { return m_listWidget; }
    QPushButton *upButton() const { return m_btnUp; }

    /**
     * @brief Returns the absolute path of the currently selected card in the gallery.
     * @return File or folder path, or empty string if no selection.
     */
    QString selectedFilePath() const;

public slots:
    void goUpDirectory();
    void toggleInspector();

    /**
     * @brief Adds a custom folder to persistent bookmarks.
     * @param folderPath Absolute directory path.
     */
    void addCustomBookmark(const QString &folderPath);

    /**
     * @brief Removes a custom folder from persistent bookmarks.
     * @param folderPath Absolute directory path.
     */
    void removeCustomBookmark(const QString &folderPath);

    /**
     * @brief Returns the list of custom bookmark directory paths.
     * @return List of absolute folder paths.
     */
    QStringList customBookmarks() const;

signals:
    /**
     * @brief Emitted when a file card is double-clicked to open it in its primary viewer.
     * @param filePath Absolute path to the file.
     */
    void filePreviewRequested(const QString &filePath);

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

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onItemSelectionChanged();
    void onThumbnailReady(const QString &filePath, const QImage &image);

private:
    void setupUi();
    void populateBookmarks();
    void updateInspectorPane(const QString &filePath);
    QString formatSize(qint64 bytes) const;
    QIcon getFolderIcon(const QString &path) const;
    QIcon getFileIcon(const QString &path) const;

    QWidget *m_topBar;
    QPushButton *m_btnUp;
    QFrame *m_pillBadge;
    QLabel *m_pillIcon;
    QLabel *m_pillText;
    QPushButton *m_btnToggleInspector;

    // Layout Components
    QListWidget *m_bookmarksWidget;
    QListWidget *m_listWidget;
    
    // Right Column: Dolphin-Style Inspector & Preview
    QWidget *m_previewPane;
    QLabel *m_inspectorBadge;
    QLabel *m_previewImgLabel;
    QPlainTextEdit *m_previewTextEdit;
    QLabel *m_previewTitleLabel;
    QLabel *m_previewMetaLabel;

    QPointer<ThumbnailWorker> m_worker;

    QString m_currentFolder;
    int m_itemCount;
    qint64 m_totalSize;
};
