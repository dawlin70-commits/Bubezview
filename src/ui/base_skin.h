#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QStringList>
#include <QFileInfo>
#include <QKeyEvent>
#include "plugin_manager.h"
#include "action_registry.h"

class ImageViewer;
class TextViewer;
class PdfViewer;
class ArchiveViewer;
class MediaViewer;
class OfficeViewer;
class FolderViewer;
class SearchBarWidget;
class FilmstripWidget;
class QPushButton;


/**
 * @brief Base controller window providing the core viewing, navigation, and transformation architecture.
 *
 * BaseSkin hosts the main central stacked widget with all viewers (Images, Text, PDF, Archives,
 * Media, Office, Folder), maintains sibling file lists for directory browsing, coordinates smooth
 * transitions, manages keyboard events, and dispatches actions to active sub-viewers.
 */
class BaseSkin : public QMainWindow {
    Q_OBJECT

public:
    enum class OpenResult {
        Success,           ///< File was recognized and loaded successfully by an internal viewer.
        DelegatedToPlugin, ///< Handled by a plugin module.
        Failed             ///< File could not be opened or does not exist.
    };

    explicit BaseSkin(QWidget *parent = nullptr);
    ~BaseSkin() override;

    /**
     * @brief Opens and renders the specified file or folder.
     * Detects MIME category, scans sibling files in the parent directory,
     * switches to the corresponding viewer page, and fires onFileLoaded().
     * @param filePath Absolute path of the file or directory.
     * @return OpenResult status.
     */
    virtual OpenResult openFile(const QString &filePath);

    int currentViewerIndex() const { return m_stackedWidget->currentIndex(); }
    QString currentFilePath() const { return m_currentFilePath; }
    QString currentShareFilePath() const;
    FolderViewer *folderViewer() const { return m_folderViewer; }
    OfficeViewer *officeViewer() const { return m_officeViewer; }
    ImageViewer *imageViewer() const { return m_imageViewer; }
    PdfViewer *pdfViewer() const { return m_pdfViewer; }
    MediaViewer *mediaViewer() const { return m_mediaViewer; }
    TextViewer *textViewer() const { return m_textViewer; }
    ArchiveViewer *archiveViewer() const { return m_archiveViewer; }
    SearchBarWidget *searchBar() const { return m_searchBar; }
    QStringList siblingFiles() const { return m_siblingFiles; }
    bool isArchivePreview() const { return !m_archiveOriginPath.isEmpty(); }
    QString archiveOriginPath() const { return m_archiveOriginPath; }
    QStringList archiveEntries() const { return m_archiveEntries; }
    int archiveCurrentEntryIndex() const { return m_archiveCurrentEntryIndex; }

    /**
     * @brief Formats current position in directory (e.g. "[3/25]").
     */
    QString fileIndexText() const;

    /**
     * @brief Formats file metadata summary for the status bar.
     */
    QString fileInfoText() const;

    FileCategory currentCategory() const { return m_currentCategory; }

    /**
     * @brief Refreshes window title, appending modified marker if active document has changes.
     */
    virtual void updateWindowTitle();

    /**
     * @brief Refreshes status bar widgets and metrics.
     */
    virtual void updateStatusBar() {}

public slots:
    // Navigation
    void nextFile();
    void prevFile();
    void toggleFullscreen();

    // Clipboard operations
    void cutToClipboard();
    void copyToClipboard();
    void pasteFromClipboard();
    void copyPathToClipboard();

    // External launches & File operations
    void openFileDialog();
    void openFolderDialog();
    void openWithDefault();
    void openWithOther();
    void saveCurrentFile();
    void saveAsCurrentFile();
    void exportCurrentFileAs();
    void deleteCurrentFile();
    void printCurrentFile();
    void showShareDialog();
    void showFileInfo();

    // Tools
    void searchInDocument();
    void showFindReplace();
    void gotoLineInDocument();
    void extractOcrCurrentFile();
    void openSettings();
    void openAppearanceSettings();
    virtual void applyCurrentTheme();

    // Transformations (Images & PDFs)
    void rotateCurrentRight();
    void rotateCurrentLeft();
    void flipCurrentHorizontal();
    void flipCurrentVertical();
    void togglePdfToc();
    void extractPdfImages(const QString &filePath = QString());
    void openPdfRepairDialog(const QString &filePath = QString());
    void openPdfPageOrganizer(const QString &filePath = QString());

    /**
     * @brief Displays the contextual popup menu at the designated global screen position.
     * Evaluates active viewer state or folder card selection to present relevant file actions.
     * @param globalPos Global screen coordinates.
     */
    void showContextMenuAt(const QPoint &globalPos);

    // Zoom
    void zoomInCurrent();
    void zoomOutCurrent();
    void resetZoomCurrent();
    void fitCurrent();
    void toggleZoomTool(bool active);
    bool isZoomToolActive() const;

    // View & Slideshow
    void toggleSlideshow();
    void nextImageFile();
    bool isSlideshowActive() const;
    void openCurrentFolderInGallery();
    void toggleFilmstrip();
    void setFilmstripVisible(bool visible);
    bool isFilmstripVisible() const;

    // Folder actions
    void renameCurrentFolderItem();
    void deleteCurrentFolderItem();
    void createFolderInCurrentFolder();

    /**
     * @brief Configures whether BubezView is running in previewer daemon mode.
     * @param daemon True if running as background DBus daemon listener.
     */
    void setDaemonMode(bool daemon);

    /**
     * @brief Checks if running in previewer daemon mode.
     * @return True if in daemon mode, false otherwise.
     */
    bool isDaemonMode() const;

    // Help & Updates
    void showAboutDialog();
    void showShortcutsDialog();
    void checkForUpdates();

    // Unsupported files inspection actions
    void openCurrentWithExternal();
    void showCurrentHexDump();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    bool promptAndSaveIfModified();
    void repositionSearchBar();

    /**
     * @brief Hook invoked whenever a new file is loaded.
     * Derived skins can override this to update toolbars and context menus.
     */
    virtual void onFileLoaded(const QString &filePath, FileCategory category, const QList<UIActionItem> &actions);

    void scanDirectory(const QString &currentFilePath);
    void startTransitionAnimation(bool forward);
    QString formatFileSize(qint64 bytes) const;

    QWidget *m_centralContainer;
    QStackedWidget *m_stackedWidget;
    SearchBarWidget *m_searchBar;
    FilmstripWidget *m_filmstrip;
    ImageViewer *m_imageViewer;
    TextViewer *m_textViewer;
    PdfViewer *m_pdfViewer;
    ArchiveViewer *m_archiveViewer;
    MediaViewer *m_mediaViewer;
    OfficeViewer *m_officeViewer;
    FolderViewer *m_folderViewer;
    QWidget *m_unsupportedViewer;
    QLabel *m_lblUnsupportedIcon;
    QLabel *m_lblUnsupportedName;
    QLabel *m_lblUnsupportedDetails;
    QPushButton *m_btnUnsupportedOpenWith;
    QPushButton *m_btnUnsupportedHexDump;

    bool loadUnsupportedFileInfo(const QString &filePath);

    bool m_filmstripWanted;

    QString m_currentFilePath;
    QString m_archiveOriginPath;
    QStringList m_archiveEntries;
    int m_archiveCurrentEntryIndex = -1;
    bool m_isLoadingArchivePreview = false;
    QStringList m_siblingFiles;
    int m_currentIndex;
    FileCategory m_currentCategory;

    void openArchiveFile(const QString &extractedFilePath, const QString &internalEntryPath);

    // Fast-Skip Transition System
    class QLabel *m_transitionOverlay;
    class QGraphicsOpacityEffect *m_overlayOpacity;
    class QParallelAnimationGroup *m_animGroup;
    qint64 m_lastSwitchTimeMs;

    class QTimer *m_slideshowTimer;
    class QTimer *m_backgroundTimer = nullptr;
    bool m_isDaemonMode = false;
};
