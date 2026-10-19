#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <QPixmap>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QNetworkAccessManager>

/**
 * @struct CoverCandidate
 * @brief Represents an album cover candidate fetched from online metadata services.
 */
struct CoverCandidate {
    QString album;
    QString artist;
    QString year;
    QString thumbUrl;
    QString highResUrl;
    QPixmap thumbPixmap;
};

class CoverCardWidget;

/**
 * @class CoverSelectionDialog
 * @brief Modal dialog presenting a visual grid of album covers for user selection.
 *
 * Allows users to search for cover artwork, preview multiple candidates
 * from online music repositories (iTunes, Cover Art Archive), and pick
 * the exact desired cover in high resolution.
 */
class CoverSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit CoverSelectionDialog(const QString &initialQuery, QWidget *parent = nullptr);
    ~CoverSelectionDialog() override;

    /**
     * @brief Gets the path to the selected temporary cover image file on disk.
     * @return Path to downloaded JPG/PNG image, or empty if cancelled.
     */
    QString selectedCoverPath() const { return m_selectedCoverPath; }

private slots:
    void onSearchClicked();
    void onCardClicked(int index);
    void onCardDoubleClicked(int index);
    void onAcceptSelection();
    void onBrowseLocalClicked();

private:
    void setupUi();
    void applyTheme();
    void executeSearch(const QString &query);
    void fetchThumbnails();
    void populateGrid();
    bool downloadHighResCover(const QString &url, QString &outFilePath, QString &outError);

    QString m_initialQuery;
    QString m_selectedCoverPath;
    int m_selectedIndex;

    QLineEdit *m_txtSearchQuery;
    QPushButton *m_btnSearch;
    QLabel *m_lblStatus;
    QScrollArea *m_scrollArea;
    QWidget *m_gridContainer;
    QGridLayout *m_gridLayout;
    QPushButton *m_btnBrowseLocal;
    QPushButton *m_btnCancel;
    QPushButton *m_btnApply;

    QList<CoverCandidate> m_candidates;
    QList<CoverCardWidget*> m_cardWidgets;
    QNetworkAccessManager m_netMgr;
};
