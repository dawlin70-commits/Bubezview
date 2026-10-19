/**
 * @file goto_folder_dialog.h
 * @brief Quick folder navigation dialog with common shortcuts and bookmarks.
 */

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>

/**
 * @class GotoFolderDialog
 * @brief Modal navigation dialog allowing users to enter a directory path or pick standard system locations.
 *
 * Provides shortcuts to Home, Documents, Downloads, Pictures, Videos, Desktop, Root, and custom directories.
 */
class GotoFolderDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs a GotoFolderDialog.
     * @param currentPath Initial directory path.
     * @param parent Optional parent widget.
     */
    explicit GotoFolderDialog(const QString &currentPath, QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~GotoFolderDialog() override = default;

    /**
     * @brief Retrieves the directory path confirmed by the user.
     * @return Path string to navigate to.
     */
    QString selectedFolder() const { return m_selectedFolder; }

private slots:
    /**
     * @brief Handles selection of a bookmarked location from the list.
     * @param item The clicked bookmark item.
     */
    void onBookmarkClicked(QListWidgetItem *item);

    /**
     * @brief Opens the native file dialog to choose a folder.
     */
    void onBrowseClicked();

    /**
     * @brief Validates and accepts the folder path.
     */
    void onAcceptClicked();

private:
    /**
     * @brief Initializes widgets, layout, and style.
     */
    void setupUi();

    /**
     * @brief Populates standard system directories into the bookmark list.
     */
    void populateBookmarks();

    QString m_currentPath;
    QString m_selectedFolder;
    QLineEdit *m_pathEdit;
    QListWidget *m_bookmarksList;
    QPushButton *m_btnBrowse;
    QPushButton *m_btnGo;
    QPushButton *m_btnCancel;
};

