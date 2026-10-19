#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QPixmap>
#include <QStringList>
#include <QList>
#include "core/audio/audio_processor.h"

/**
 * @struct AudioTrackItem
 * @brief Represents an individual audio track item in the batch tag editor.
 */
struct AudioTrackItem {
    QString filePath;
    AudioMetadata metadata;
    AudioMetadata originalMetadata;
    QPixmap originalCover;
    bool originalHadCover = false;
};

/**
 * @class AudioTagDialog
 * @brief Batch Tag & Metadata editor with album cover picker and spacious track manager.
 *
 * Employs a modern side-by-side layout: top actions toolbar, left panel with common album
 * metadata and cover art, and a full-height spacious tracklist table on the right.
 */
class AudioTagDialog : public QDialog {
    Q_OBJECT

public:
    explicit AudioTagDialog(const QString &currentFilePath,
                            const QStringList &siblingFiles = QStringList(),
                            QWidget *parent = nullptr);
    ~AudioTagDialog() override;

private slots:
    void onChangeCoverClicked();
    void onRemoveCoverClicked();
    void onDownloadCoverClicked();
    void onRevertClicked();
    void onAutoNumberClicked();
    void onAutoTitleFromFilenamesClicked();
    void onSearchOnlineTagsClicked();
    void onRenameFilesOnDiskClicked();
    void onTitleCaseClicked();
    void onSaveClicked();
    void onTableRowSelected(int row);

private:
    void setupUi();
    void applyTheme();
    void loadTracks();
    void updateCoverPreview();
    void syncUiFromTrack(int index);
    QString toTitleCase(const QString &input);

    QString m_initialFilePath;
    QStringList m_candidateFiles;
    QList<AudioTrackItem> m_tracks;
    int m_currentTrackIdx;

    // Initial snapshot of common fields
    QString m_initialArtist;
    QString m_initialAlbum;
    QString m_initialYear;
    QString m_initialGenre;
    QString m_initialComment;

    // Top Action Toolbar Buttons
    QPushButton *m_btnAutoTitle;        ///< Level 1: Auto-title from filename
    QPushButton *m_btnSearchTags;       ///< Level 2: Search tags online via MusicBrainz
    QPushButton *m_btnRenameFiles;      ///< Level 3: Rename files on disk (01 - Title)
    QPushButton *m_btnAutoNumber;       ///< Auto-number tracks 01..N
    QPushButton *m_btnTitleCase;        ///< Format titles in Title Case

    // Common Global Fields (Left Panel)
    QLineEdit *m_txtArtist;
    QLineEdit *m_txtAlbum;
    QLineEdit *m_txtYear;
    QLineEdit *m_txtGenre;
    QLineEdit *m_txtComment;

    // Apply to all flags
    QCheckBox *m_chkApplyArtist;
    QCheckBox *m_chkApplyAlbum;
    QCheckBox *m_chkApplyYear;
    QCheckBox *m_chkApplyGenre;
    QCheckBox *m_chkApplyCover;

    // Cover Art (Left Panel)
    QLabel *m_lblCoverImage;
    QPushButton *m_btnDownloadCover;
    QPushButton *m_btnChangeCover;
    QPushButton *m_btnRemoveCover;
    QString m_newCoverPath;
    bool m_removeCover;

    // Tracks Table (Right Panel, Full Height)
    QTableWidget *m_tableTracks;

    // Bottom Action Buttons
    QPushButton *m_btnRevert;
    QPushButton *m_btnSave;
    QPushButton *m_btnCancel;
};
