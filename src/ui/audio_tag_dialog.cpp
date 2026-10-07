#include "audio_tag_dialog.h"
#include "cover_selection_dialog.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QEventLoop>

AudioTagDialog::AudioTagDialog(const QString &currentFilePath,
                               const QStringList &siblingFiles,
                               QWidget *parent)
    : QDialog(parent)
    , m_initialFilePath(currentFilePath)
    , m_currentTrackIdx(0)
    , m_newCoverPath(QString())
    , m_removeCover(false)
{
    setWindowTitle(tr("Audio Tag & Album Editor - BubezView"));
    setMinimumSize(1020, 640);
    resize(1060, 680);

    // Filter audio sibling files
    const QStringList audioExts = {"mp3", "flac", "wav", "ogg", "m4a", "aac", "wma", "opus", "aiff", "ape", "wv"};
    for (const QString &path : siblingFiles) {
        QString ext = QFileInfo(path).suffix().toLower();
        if (audioExts.contains(ext)) {
            m_candidateFiles.append(path);
        }
    }
    if (!m_candidateFiles.contains(m_initialFilePath) && QFileInfo::exists(m_initialFilePath)) {
        m_candidateFiles.prepend(m_initialFilePath);
    }

    setupUi();
    loadTracks();
}

AudioTagDialog::~AudioTagDialog() = default;

void AudioTagDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // 1. Top Action Toolbar: All batch action buttons in a clear, accessible row
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(8);

    m_btnAutoTitle = new QPushButton(tr("🏷️ Auto-Title from Filename"), this);
    m_btnAutoTitle->setCursor(Qt::PointingHandCursor);
    m_btnAutoTitle->setToolTip(tr("Clean track titles from filenames by stripping track numbers (e.g. '01 - Song' -> 'Song')"));
    connect(m_btnAutoTitle, &QPushButton::clicked, this, &AudioTagDialog::onAutoTitleFromFilenamesClicked);

    m_btnSearchTags = new QPushButton(tr("🔍 Search Tags Online"), this);
    m_btnSearchTags->setCursor(Qt::PointingHandCursor);
    m_btnSearchTags->setToolTip(tr("Search MusicBrainz for album metadata and automatically fill all track titles"));
    connect(m_btnSearchTags, &QPushButton::clicked, this, &AudioTagDialog::onSearchOnlineTagsClicked);

    m_btnRenameFiles = new QPushButton(tr("📝 Rename Files on Disk (01 - Title)"), this);
    m_btnRenameFiles->setCursor(Qt::PointingHandCursor);
    m_btnRenameFiles->setToolTip(tr("Rename files on disk formatted with two-digit sequence (01 - Title.ext)"));
    connect(m_btnRenameFiles, &QPushButton::clicked, this, &AudioTagDialog::onRenameFilesOnDiskClicked);

    m_btnAutoNumber = new QPushButton(tr("🔢 Auto-Number (01...N)"), this);
    m_btnAutoNumber->setCursor(Qt::PointingHandCursor);
    m_btnAutoNumber->setToolTip(tr("Number all tracks sequentially with two digits"));
    connect(m_btnAutoNumber, &QPushButton::clicked, this, &AudioTagDialog::onAutoNumberClicked);

    m_btnTitleCase = new QPushButton(tr("Aa Title Case"), this);
    m_btnTitleCase->setCursor(Qt::PointingHandCursor);
    m_btnTitleCase->setToolTip(tr("Capitalize words in track titles"));
    connect(m_btnTitleCase, &QPushButton::clicked, this, &AudioTagDialog::onTitleCaseClicked);

    topBarLayout->addWidget(m_btnAutoTitle);
    topBarLayout->addWidget(m_btnSearchTags);
    topBarLayout->addWidget(m_btnRenameFiles);
    topBarLayout->addSpacing(16);
    topBarLayout->addWidget(m_btnAutoNumber);
    topBarLayout->addWidget(m_btnTitleCase);
    topBarLayout->addStretch();
    mainLayout->addLayout(topBarLayout);

    // 2. Middle Body: Side-by-Side Layout
    // Left: Album Common Tags & Cover Art (~320px)
    // Right: Full-Height Spacious Tracklist Table (occupies remaining space)
    QHBoxLayout *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(16);

    // --- LEFT COLUMN ---
    QVBoxLayout *leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(12);

    // Cover Art Card
    QGroupBox *grpCover = new QGroupBox(tr("Album Cover"), this);
    grpCover->setFixedWidth(380);
    QVBoxLayout *coverLayout = new QVBoxLayout(grpCover);
    coverLayout->setSpacing(8);
    coverLayout->setContentsMargins(10, 14, 10, 10);

    m_lblCoverImage = new QLabel(grpCover);
    m_lblCoverImage->setFixedSize(150, 150);
    m_lblCoverImage->setAlignment(Qt::AlignCenter);

    QHBoxLayout *coverBtns = new QHBoxLayout();
    coverBtns->setSpacing(6);

    m_btnDownloadCover = new QPushButton(tr("🌐 Download"), grpCover);
    m_btnDownloadCover->setCursor(Qt::PointingHandCursor);
    m_btnDownloadCover->setToolTip(tr("Search and choose cover art from online candidates"));
    connect(m_btnDownloadCover, &QPushButton::clicked, this, &AudioTagDialog::onDownloadCoverClicked);

    m_btnChangeCover = new QPushButton(tr("📁 Browse"), grpCover);
    m_btnChangeCover->setCursor(Qt::PointingHandCursor);
    m_btnChangeCover->setToolTip(tr("Select a cover image from your local drive"));
    connect(m_btnChangeCover, &QPushButton::clicked, this, &AudioTagDialog::onChangeCoverClicked);

    m_btnRemoveCover = new QPushButton(tr("❌"), grpCover);
    m_btnRemoveCover->setCursor(Qt::PointingHandCursor);
    m_btnRemoveCover->setFixedWidth(32);
    m_btnRemoveCover->setToolTip(tr("Remove cover"));
    connect(m_btnRemoveCover, &QPushButton::clicked, this, &AudioTagDialog::onRemoveCoverClicked);

    coverBtns->addWidget(m_btnDownloadCover);
    coverBtns->addWidget(m_btnChangeCover);
    coverBtns->addWidget(m_btnRemoveCover);

    m_chkApplyCover = new QCheckBox(tr("Apply cover to all tracks"), grpCover);
    m_chkApplyCover->setChecked(true);

    coverLayout->addWidget(m_lblCoverImage, 0, Qt::AlignCenter);
    coverLayout->addLayout(coverBtns);
    coverLayout->addWidget(m_chkApplyCover);
    leftColumn->addWidget(grpCover);

    // Common Metadata Fields
    QGroupBox *grpCommon = new QGroupBox(tr("Album Metadata"), this);
    grpCommon->setFixedWidth(380);
    QGridLayout *formGrid = new QGridLayout(grpCommon);
    formGrid->setHorizontalSpacing(8);
    formGrid->setVerticalSpacing(8);
    formGrid->setContentsMargins(10, 14, 10, 10);

    // Row 0: Artist
    m_txtArtist = new QLineEdit(grpCommon);
    m_chkApplyArtist = new QCheckBox(tr("All"), grpCommon);
    m_chkApplyArtist->setChecked(true);
    m_chkApplyArtist->setToolTip(tr("Apply Artist to all tracks"));
    formGrid->addWidget(new QLabel(tr("Artist:"), grpCommon), 0, 0);
    formGrid->addWidget(m_txtArtist, 0, 1);
    formGrid->addWidget(m_chkApplyArtist, 0, 2);

    // Row 1: Album
    m_txtAlbum = new QLineEdit(grpCommon);
    m_chkApplyAlbum = new QCheckBox(tr("All"), grpCommon);
    m_chkApplyAlbum->setChecked(true);
    m_chkApplyAlbum->setToolTip(tr("Apply Album name to all tracks"));
    formGrid->addWidget(new QLabel(tr("Album:"), grpCommon), 1, 0);
    formGrid->addWidget(m_txtAlbum, 1, 1);
    formGrid->addWidget(m_chkApplyAlbum, 1, 2);

    // Row 2: Year
    m_txtYear = new QLineEdit(grpCommon);
    m_chkApplyYear = new QCheckBox(tr("All"), grpCommon);
    m_chkApplyYear->setChecked(true);
    m_chkApplyYear->setToolTip(tr("Apply Year to all tracks"));
    formGrid->addWidget(new QLabel(tr("Year:"), grpCommon), 2, 0);
    formGrid->addWidget(m_txtYear, 2, 1);
    formGrid->addWidget(m_chkApplyYear, 2, 2);

    // Row 3: Genre
    m_txtGenre = new QLineEdit(grpCommon);
    m_chkApplyGenre = new QCheckBox(tr("All"), grpCommon);
    m_chkApplyGenre->setChecked(true);
    m_chkApplyGenre->setToolTip(tr("Apply Genre to all tracks"));
    formGrid->addWidget(new QLabel(tr("Genre:"), grpCommon), 3, 0);
    formGrid->addWidget(m_txtGenre, 3, 1);
    formGrid->addWidget(m_chkApplyGenre, 3, 2);

    // Row 4: Comment
    m_txtComment = new QLineEdit(grpCommon);
    formGrid->addWidget(new QLabel(tr("Comment:"), grpCommon), 4, 0);
    formGrid->addWidget(m_txtComment, 4, 1, 1, 2);

    leftColumn->addWidget(grpCommon);
    leftColumn->addStretch();
    bodyLayout->addLayout(leftColumn, 0);

    // --- RIGHT COLUMN: Tracks List (Spacious, Full Height) ---
    QGroupBox *grpTracks = new QGroupBox(tr("Album Tracks List"), this);
    QVBoxLayout *tracksLayout = new QVBoxLayout(grpTracks);
    tracksLayout->setContentsMargins(10, 14, 10, 10);

    m_tableTracks = new QTableWidget(grpTracks);
    m_tableTracks->setColumnCount(4);
    m_tableTracks->setHorizontalHeaderLabels({tr("#"), tr("Track Title"), tr("File Name"), tr("Duration")});
    m_tableTracks->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableTracks->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableTracks->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableTracks->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableTracks->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableTracks->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableTracks->setAlternatingRowColors(true);
    connect(m_tableTracks, &QTableWidget::cellClicked, this, &AudioTagDialog::onTableRowSelected);

    tracksLayout->addWidget(m_tableTracks);
    bodyLayout->addWidget(grpTracks, 1);

    mainLayout->addLayout(bodyLayout, 1);

    // 3. Bottom Action Buttons: Revert (Left), Cancel & Save (Right)
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    m_btnRevert = new QPushButton(tr("🔄 Revert All Edits"), this);
    m_btnRevert->setCursor(Qt::PointingHandCursor);
    m_btnRevert->setToolTip(tr("Discard all changes and restore original tags and cover art"));
    connect(m_btnRevert, &QPushButton::clicked, this, &AudioTagDialog::onRevertClicked);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnSave = new QPushButton(tr("Save All Changes"), this);
    m_btnSave->setCursor(Qt::PointingHandCursor);
    connect(m_btnSave, &QPushButton::clicked, this, &AudioTagDialog::onSaveClicked);

    actionsLayout->addWidget(m_btnRevert);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_btnCancel);
    actionsLayout->addWidget(m_btnSave);
    mainLayout->addLayout(actionsLayout);

    applyTheme();
}

void AudioTagDialog::loadTracks()
{
    m_tracks.clear();
    m_tableTracks->setRowCount(0);

    int row = 0;
    int initialRowToSelect = 0;

    for (const QString &path : m_candidateFiles) {
        AudioMetadata meta = AudioProcessor::readMetadata(path);
        QPixmap cov = AudioProcessor::extractCoverArt(path, 150);

        AudioTrackItem item;
        item.filePath = path;
        item.metadata = meta;
        item.originalMetadata = meta;
        item.originalCover = cov;
        item.originalHadCover = !cov.isNull();
        m_tracks.append(item);

        m_tableTracks->insertRow(row);

        // Column 0: Track number (formatted to two digits if numeric)
        QString numStr = meta.trackNumber.trimmed();
        if (numStr.contains('/')) {
            numStr = numStr.split('/').first().trimmed();
        }
        if (numStr.isEmpty()) {
            numStr = QString("%1").arg(row + 1, 2, 10, QChar('0'));
        } else {
            bool isInt = false;
            int n = numStr.toInt(&isInt);
            if (isInt) {
                numStr = QString("%1").arg(n, 2, 10, QChar('0'));
            }
        }
        QTableWidgetItem *numItem = new QTableWidgetItem(numStr);
        numItem->setTextAlignment(Qt::AlignCenter);
        m_tableTracks->setItem(row, 0, numItem);

        // Column 1: Title (editable)
        QString displayTitle = meta.title.isEmpty() ? QFileInfo(path).completeBaseName() : meta.title;
        QTableWidgetItem *titleItem = new QTableWidgetItem(displayTitle);
        m_tableTracks->setItem(row, 1, titleItem);

        // Column 2: Filename (read only)
        QTableWidgetItem *fileItem = new QTableWidgetItem(QFileInfo(path).fileName());
        fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsEditable);
        fileItem->setForeground(QColor("#a1a1aa"));
        m_tableTracks->setItem(row, 2, fileItem);

        // Column 3: Duration (read only)
        qint64 sec = meta.durationMs / 1000;
        QString durText = QString("%1:%2").arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
        QTableWidgetItem *durItem = new QTableWidgetItem(durText);
        durItem->setFlags(durItem->flags() & ~Qt::ItemIsEditable);
        durItem->setTextAlignment(Qt::AlignCenter);
        m_tableTracks->setItem(row, 3, durItem);

        if (path == m_initialFilePath) {
            initialRowToSelect = row;
        }
        row++;
    }

    if (!m_tracks.isEmpty()) {
        m_tableTracks->selectRow(initialRowToSelect);
        syncUiFromTrack(initialRowToSelect);

        // Snapshot initial common fields
        m_initialArtist = m_txtArtist->text();
        m_initialAlbum = m_txtAlbum->text();
        m_initialYear = m_txtYear->text();
        m_initialGenre = m_txtGenre->text();
        m_initialComment = m_txtComment->text();
    }
}

void AudioTagDialog::syncUiFromTrack(int index)
{
    if (index < 0 || index >= m_tracks.size()) return;
    m_currentTrackIdx = index;
    const AudioMetadata &meta = m_tracks[index].metadata;

    if (m_txtArtist->text().isEmpty()) m_txtArtist->setText(meta.artist);
    if (m_txtAlbum->text().isEmpty()) m_txtAlbum->setText(meta.album);
    if (m_txtYear->text().isEmpty()) m_txtYear->setText(meta.year);
    if (m_txtGenre->text().isEmpty()) m_txtGenre->setText(meta.genre);
    if (m_txtComment->text().isEmpty()) m_txtComment->setText(meta.comment);

    updateCoverPreview();
}

void AudioTagDialog::updateCoverPreview()
{
    if (m_removeCover) {
        m_lblCoverImage->setText(tr("No Cover\n(Removed)"));
        m_lblCoverImage->setPixmap(QPixmap());
        return;
    }

    if (!m_newCoverPath.isEmpty()) {
        QPixmap pix(m_newCoverPath);
        if (!pix.isNull()) {
            m_lblCoverImage->setPixmap(pix.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_lblCoverImage->setText(QString());
            return;
        }
    }

    if (m_currentTrackIdx >= 0 && m_currentTrackIdx < m_tracks.size()) {
        const AudioTrackItem &item = m_tracks[m_currentTrackIdx];
        if (!item.originalCover.isNull()) {
            m_lblCoverImage->setPixmap(item.originalCover.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_lblCoverImage->setText(QString());
            return;
        }
    }

    m_lblCoverImage->setText(tr("🎵\nNo Cover Art"));
    m_lblCoverImage->setPixmap(QPixmap());
}

void AudioTagDialog::onTableRowSelected(int row)
{
    if (row >= 0 && row < m_tracks.size()) {
        m_currentTrackIdx = row;
        updateCoverPreview();
    }
}

void AudioTagDialog::onChangeCoverClicked()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Select Cover Image"), QString(), tr("Images (*.jpg *.jpeg *.png *.webp)"));
    if (!file.isEmpty()) {
        m_newCoverPath = file;
        m_removeCover = false;
        updateCoverPreview();
    }
}

void AudioTagDialog::onRemoveCoverClicked()
{
    m_newCoverPath.clear();
    m_removeCover = true;
    updateCoverPreview();
}

void AudioTagDialog::onDownloadCoverClicked()
{
    QString artist = m_txtArtist ? m_txtArtist->text().trimmed() : QString();
    QString album = m_txtAlbum ? m_txtAlbum->text().trimmed() : QString();

    if (artist.isEmpty() && m_currentTrackIdx >= 0 && m_currentTrackIdx < m_tracks.size()) {
        artist = m_tracks[m_currentTrackIdx].metadata.artist;
    }
    if (album.isEmpty() && m_currentTrackIdx >= 0 && m_currentTrackIdx < m_tracks.size()) {
        album = m_tracks[m_currentTrackIdx].metadata.album;
    }

    QString query = QString("%1 %2").arg(artist, album).trimmed();
    if (query.isEmpty() && !m_tracks.isEmpty()) {
        query = QFileInfo(m_tracks[0].filePath).dir().dirName();
    }

    CoverSelectionDialog dlg(query, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString coverPath = dlg.selectedCoverPath();
        if (!coverPath.isEmpty() && QFileInfo::exists(coverPath)) {
            m_newCoverPath = coverPath;
            m_removeCover = false;
            updateCoverPreview();
        }
    }
}

void AudioTagDialog::onRevertClicked()
{
    auto reply = QMessageBox::question(
        this,
        tr("Revert All Edits"),
        tr("Are you sure you want to discard all changes and restore original tags and cover art?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Restore common fields
    m_txtArtist->setText(m_initialArtist);
    m_txtAlbum->setText(m_initialAlbum);
    m_txtYear->setText(m_initialYear);
    m_txtGenre->setText(m_initialGenre);
    m_txtComment->setText(m_initialComment);

    // Restore table rows
    for (int i = 0; i < m_tracks.size(); ++i) {
        AudioTrackItem &item = m_tracks[i];
        item.metadata = item.originalMetadata;

        QTableWidgetItem *numItem = m_tableTracks->item(i, 0);
        QTableWidgetItem *titleItem = m_tableTracks->item(i, 1);
        QTableWidgetItem *fileItem = m_tableTracks->item(i, 2);

        QString numStr = item.originalMetadata.trackNumber.trimmed();
        if (numStr.contains('/')) {
            numStr = numStr.split('/').first().trimmed();
        }
        if (numStr.isEmpty()) {
            numStr = QString("%1").arg(i + 1, 2, 10, QChar('0'));
        } else {
            bool isInt = false;
            int n = numStr.toInt(&isInt);
            if (isInt) {
                numStr = QString("%1").arg(n, 2, 10, QChar('0'));
            }
        }
        if (numItem) numItem->setText(numStr);
        if (titleItem) titleItem->setText(item.originalMetadata.title.isEmpty() ? QFileInfo(item.filePath).completeBaseName() : item.originalMetadata.title);
        if (fileItem) fileItem->setText(QFileInfo(item.filePath).fileName());
    }

    m_newCoverPath.clear();
    m_removeCover = false;
    updateCoverPreview();

    QMessageBox::information(this, tr("Revert Complete"), tr("All tags, track titles, and cover art have been restored to their original state."));
}

void AudioTagDialog::onAutoNumberClicked()
{
    for (int i = 0; i < m_tableTracks->rowCount(); ++i) {
        QTableWidgetItem *item = m_tableTracks->item(i, 0);
        if (item) {
            item->setText(QString("%1").arg(i + 1, 2, 10, QChar('0')));
        }
    }
}

void AudioTagDialog::onAutoTitleFromFilenamesClicked()
{
    // Cleans up "01 - Title.mp3", "01. Title.flac", "[01] Title" -> "Title"
    static const QRegularExpression reLeading(R"(^(?:\[?\d+\]?[\s\.\-_]+)+)");
    static const QRegularExpression reTrailing(R"(\s*\[[^\]]+\]\s*$)");

    for (int i = 0; i < m_tableTracks->rowCount(); ++i) {
        QTableWidgetItem *fileItem = m_tableTracks->item(i, 2);
        QTableWidgetItem *titleItem = m_tableTracks->item(i, 1);
        if (fileItem && titleItem) {
            QString base = QFileInfo(fileItem->text()).completeBaseName();
            base.remove(reLeading);
            base.remove(reTrailing);
            titleItem->setText(base.trimmed());
        }
    }
}

void AudioTagDialog::onSearchOnlineTagsClicked()
{
    if (m_tracks.isEmpty()) return;

    QString defaultQuery;
    QString currentArtist = m_txtArtist ? m_txtArtist->text().trimmed() : QString();
    QString currentAlbum = m_txtAlbum ? m_txtAlbum->text().trimmed() : QString();

    if (!currentArtist.isEmpty() && !currentAlbum.isEmpty()) {
        defaultQuery = QString("%1 %2").arg(currentArtist, currentAlbum);
    } else {
        defaultQuery = QFileInfo(m_tracks[0].filePath).dir().dirName();
    }

    bool ok = false;
    QString query = QInputDialog::getText(
        this,
        tr("Search Tags Online"),
        tr("Enter Artist and Album to search on MusicBrainz:"),
        QLineEdit::Normal,
        defaultQuery,
        &ok
    );

    if (!ok || query.trimmed().isEmpty()) {
        return;
    }
    query = query.trimmed();

    QProgressDialog progress(tr("Searching album tracks on MusicBrainz..."), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    qApp->processEvents();

    QNetworkAccessManager netMgr;
    QString mbSearchUrl = QString("https://musicbrainz.org/ws/2/release/?query=%1&fmt=json&limit=5")
                              .arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    QUrl targetUrl(mbSearchUrl);
    QNetworkRequest req(targetUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/0.1.8 ( https://github.com/davide/bubezview )");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QEventLoop loop;
    QNetworkReply *reply = netMgr.get(req);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        progress.close();
        QMessageBox::warning(this, tr("Search Error"), tr("Network error querying MusicBrainz: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        progress.close();
        QMessageBox::warning(this, tr("Search Error"), tr("Invalid data received from MusicBrainz."));
        return;
    }

    QJsonArray releases = doc.object().value("releases").toArray();
    if (releases.isEmpty()) {
        progress.close();
        QMessageBox::information(this, tr("No Results"), tr("No albums found on MusicBrainz matching \"%1\".").arg(query));
        return;
    }

    QString mbid = releases.first().toObject().value("id").toString();
    if (mbid.isEmpty()) {
        progress.close();
        return;
    }

    // Now fetch complete release details with tracklist: inc=recordings+artist-credits
    progress.setLabelText(tr("Downloading tracklist details..."));
    qApp->processEvents();

    QString detailsUrlStr = QString("https://musicbrainz.org/ws/2/release/%1?inc=recordings+artist-credits&fmt=json").arg(mbid);
    QUrl detailsUrl(detailsUrlStr);
    QNetworkRequest detailsReq(detailsUrl);
    detailsReq.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/0.1.8 ( https://github.com/davide/bubezview )");
    detailsReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *detailsReply = netMgr.get(detailsReq);
    connect(detailsReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (detailsReply->error() != QNetworkReply::NoError) {
        progress.close();
        QMessageBox::warning(this, tr("Details Error"), tr("Error fetching release details: %1").arg(detailsReply->errorString()));
        detailsReply->deleteLater();
        return;
    }

    QByteArray detailsData = detailsReply->readAll();
    detailsReply->deleteLater();
    progress.close();

    QJsonDocument detailsDoc = QJsonDocument::fromJson(detailsData);
    if (!detailsDoc.isObject()) return;

    QJsonObject relObj = detailsDoc.object();
    QString foundTitle = relObj.value("title").toString();
    QString foundYear = relObj.value("date").toString().left(4);
    QString foundArtist;
    QJsonArray artistCredits = relObj.value("artist-credit").toArray();
    if (!artistCredits.isEmpty()) {
        foundArtist = artistCredits.first().toObject().value("name").toString();
    }

    if (!foundArtist.isEmpty()) m_txtArtist->setText(foundArtist);
    if (!foundTitle.isEmpty()) m_txtAlbum->setText(foundTitle);
    if (!foundYear.isEmpty()) m_txtYear->setText(foundYear);

    // Extract tracklist
    QJsonArray media = relObj.value("media").toArray();
    int tracksApplied = 0;
    if (!media.isEmpty()) {
        QJsonArray mbTracks = media.first().toObject().value("tracks").toArray();
        for (int i = 0; i < m_tableTracks->rowCount() && i < mbTracks.size(); ++i) {
            QJsonObject trackObj = mbTracks[i].toObject();
            QString tTitle = trackObj.value("title").toString();
            if (!tTitle.isEmpty()) {
                QTableWidgetItem *titleItem = m_tableTracks->item(i, 1);
                if (titleItem) {
                    titleItem->setText(tTitle);
                    tracksApplied++;
                }
            }
            QTableWidgetItem *numItem = m_tableTracks->item(i, 0);
            if (numItem) {
                numItem->setText(QString("%1").arg(i + 1, 2, 10, QChar('0')));
            }
        }
    }

    QString successMsg = tr("Applied information from MusicBrainz!\n• Artist: %1\n• Album: %2\n• Year: %3\n• Track titles populated: %4 of %5\n\nWould you like to download the album cover art as well?")
                             .arg(foundArtist, foundTitle, foundYear)
                             .arg(tracksApplied)
                             .arg(m_tableTracks->rowCount());

    auto replyAsk = QMessageBox::question(this, tr("Tags Applied"), successMsg, QMessageBox::Yes | QMessageBox::No);
    if (replyAsk == QMessageBox::Yes) {
        onDownloadCoverClicked();
    }
}

void AudioTagDialog::onRenameFilesOnDiskClicked()
{
    if (m_tracks.isEmpty()) return;

    struct RenamePlan {
        int index;
        QString oldPath;
        QString newFileName;
    };
    QList<RenamePlan> plan;
    QString previewText;

    for (int i = 0; i < m_tracks.size(); ++i) {
        QTableWidgetItem *numItem = m_tableTracks->item(i, 0);
        QTableWidgetItem *titleItem = m_tableTracks->item(i, 1);
        QTableWidgetItem *fileItem = m_tableTracks->item(i, 2);

        int trackNum = i + 1;
        if (numItem) {
            bool isInt = false;
            int n = numItem->text().trimmed().toInt(&isInt);
            if (isInt && n > 0) trackNum = n;
        }

        QString title = titleItem ? titleItem->text().trimmed() : QString();
        if (title.isEmpty()) title = QString("Track %1").arg(trackNum);

        QString ext = QFileInfo(m_tracks[i].filePath).suffix();
        QString newName = QString("%1 - %2.%3")
                              .arg(trackNum, 2, 10, QChar('0'))
                              .arg(title, ext);

        static const QRegularExpression invalidChars(R"([<>:"/\\|?*])");
        newName.remove(invalidChars);

        RenamePlan p;
        p.index = i;
        p.oldPath = m_tracks[i].filePath;
        p.newFileName = newName;
        plan.append(p);

        QString oldName = fileItem ? fileItem->text() : QFileInfo(p.oldPath).fileName();
        if (i < 10) {
            previewText += QString("• %1  ➔  %2\n").arg(oldName, newName);
        }
    }

    if (m_tracks.size() > 10) {
        previewText += tr("... and %1 more tracks.\n").arg(m_tracks.size() - 10);
    }

    QString prompt = tr("Are you sure you want to rename %1 audio files on disk with two-digit sequence (01 - Title.ext)?\n\nPreview:\n%2")
                         .arg(m_tracks.size())
                         .arg(previewText);

    auto reply = QMessageBox::question(this, tr("Confirm Disk File Renaming"), prompt, QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int successCount = 0;
    QStringList errors;

    for (const RenamePlan &p : plan) {
        QString outNewPath;
        QString err;
        bool ok = AudioProcessor::renameAudioFile(p.oldPath, p.newFileName, outNewPath, err);
        if (ok) {
            successCount++;
            m_tracks[p.index].filePath = outNewPath;
            QTableWidgetItem *fileItem = m_tableTracks->item(p.index, 2);
            if (fileItem) fileItem->setText(p.newFileName);
        } else {
            errors.append(QString("%1: %2").arg(p.newFileName, err));
        }
    }

    if (errors.isEmpty()) {
        QMessageBox::information(this, tr("Renaming Succeeded"), tr("Successfully renamed %1 of %2 files on disk!").arg(successCount).arg(m_tracks.size()));
    } else {
        QMessageBox::warning(this, tr("Renaming Notice"), tr("Renamed %1 of %2 files.\n\nErrors encountered:\n%3").arg(successCount).arg(m_tracks.size()).arg(errors.join("\n")));
    }
}

QString AudioTagDialog::toTitleCase(const QString &input)
{
    if (input.isEmpty()) return input;

    static const QStringList minorWords = {
        "a", "an", "the", "and", "but", "or", "for", "nor", "on", "at",
        "to", "from", "by", "in", "of", "with", "de", "del", "della", "di",
        "da", "e", "ed", "il", "lo", "la", "i", "gli", "le", "un", "una", "uno"
    };

    QStringList words = input.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i) {
        QString w = words[i].toLower();
        if (i == 0 || !minorWords.contains(w)) {
            if (!w.isEmpty()) {
                w[0] = w[0].toUpper();
            }
        }
        words[i] = w;
    }
    return words.join(" ");
}

void AudioTagDialog::onTitleCaseClicked()
{
    for (int i = 0; i < m_tableTracks->rowCount(); ++i) {
        QTableWidgetItem *item = m_tableTracks->item(i, 1);
        if (item) {
            item->setText(toTitleCase(item->text()));
        }
    }
}

void AudioTagDialog::onSaveClicked()
{
    m_btnSave->setEnabled(false);
    qApp->processEvents();

    QString commonArtist = m_txtArtist->text().trimmed();
    QString commonAlbum = m_txtAlbum->text().trimmed();
    QString commonYear = m_txtYear->text().trimmed();
    QString commonGenre = m_txtGenre->text().trimmed();
    QString commonComment = m_txtComment->text().trimmed();

    bool applyArtist = m_chkApplyArtist->isChecked();
    bool applyAlbum = m_chkApplyAlbum->isChecked();
    bool applyYear = m_chkApplyYear->isChecked();
    bool applyGenre = m_chkApplyGenre->isChecked();
    bool applyCover = m_chkApplyCover->isChecked();

    int successCount = 0;

    for (int i = 0; i < m_tracks.size(); ++i) {
        AudioTrackItem &track = m_tracks[i];
        AudioMetadata updated = track.metadata;

        // Individual Title & Track # from table
        QTableWidgetItem *numItem = m_tableTracks->item(i, 0);
        QTableWidgetItem *titleItem = m_tableTracks->item(i, 1);
        if (numItem) updated.trackNumber = numItem->text().trimmed();
        if (titleItem) updated.title = titleItem->text().trimmed();

        // Common Fields
        if (applyArtist && !commonArtist.isEmpty()) updated.artist = commonArtist;
        if (applyAlbum && !commonAlbum.isEmpty()) updated.album = commonAlbum;
        if (applyYear && !commonYear.isEmpty()) {
            updated.year = commonYear;
            updated.date = commonYear;
        }
        if (applyGenre && !commonGenre.isEmpty()) updated.genre = commonGenre;
        if (!commonComment.isEmpty()) updated.comment = commonComment;

        QString coverToApply;
        bool stripCover = false;
        if (applyCover || i == m_currentTrackIdx) {
            coverToApply = m_newCoverPath;
            stripCover = m_removeCover;
        }

        bool ok = AudioProcessor::writeMetadata(track.filePath, updated, coverToApply, stripCover);
        if (ok) {
            successCount++;
        }
    }

    m_btnSave->setEnabled(true);

    if (successCount > 0) {
        QMessageBox::information(this, tr("Success"), tr("%1 of %2 audio files updated successfully!").arg(successCount).arg(m_tracks.size()));
        accept();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to update audio tags. Please ensure files are not write-protected."));
    }
}

void AudioTagDialog::applyTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::DarkSlate) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #22262b;"
            "   color: #eff0f1;"
            "   font-family: sans-serif;"
            "}"
            "QLabel {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QLineEdit {"
            "   background-color: #282c32;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "   selection-background-color: #0284c7;"
            "}"
            "QLineEdit:focus {"
            "   border-color: #61afef;"
            "}"
            "QGroupBox {"
            "   border: 1px solid #2a2f35;"
            "   border-radius: 8px;"
            "   margin-top: 10px;"
            "   padding-top: 14px;"
            "   padding-bottom: 10px;"
            "   padding-left: 10px;"
            "   padding-right: 10px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "   color: #eff0f1;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 10px;"
            "   padding: 0 4px;"
            "   color: #61afef;"
            "}"
            "QTableWidget {"
            "   background-color: #1e2226;"
            "   alternate-background-color: #24292f;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a2f35;"
            "   border-radius: 6px;"
            "   gridline-color: #2a2f35;"
            "   selection-background-color: #383c42;"
            "   selection-color: #ffffff;"
            "   font-size: 13px;"
            "}"
            "QHeaderView::section {"
            "   background-color: #282c32;"
            "   color: #c5cbd3;"
            "   padding: 6px;"
            "   border: 1px solid #2a2f35;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "}"
            "QCheckBox {"
            "   color: #c5cbd3;"
            "   font-size: 12px;"
            "   spacing: 4px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 15px;"
            "   height: 15px;"
            "   background-color: #282c32;"
            "   border: 1px solid #353b44;"
            "   border-radius: 4px;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0284c7;"
            "   border-color: #61afef;"
            "}"
            "QPushButton {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 6px 12px;"
            "   font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #353b44;"
            "   color: #ffffff;"
            "}"
        );
        if (m_lblCoverImage) {
            m_lblCoverImage->setStyleSheet("background-color: #1e2226; border: 1px solid #2a2f35; border-radius: 8px; color: #959ca7;");
        }
        if (m_btnDownloadCover) {
            m_btnDownloadCover->setStyleSheet("QPushButton { background-color: #282c32; color: #61afef; border: 1px solid #353b44; border-radius: 6px; padding: 5px 10px; font-weight: bold; } QPushButton:hover { background-color: #353b44; color: #ffffff; }");
        }
        if (m_btnChangeCover) {
            m_btnChangeCover->setStyleSheet("QPushButton { background-color: #282c32; color: #eff0f1; border: 1px solid #353b44; border-radius: 6px; padding: 5px 10px; } QPushButton:hover { background-color: #353b44; color: #61afef; }");
        }
        if (m_btnRemoveCover) {
            m_btnRemoveCover->setStyleSheet("QPushButton { background-color: #282c32; color: #ef4444; border: 1px solid #353b44; border-radius: 6px; padding: 5px; } QPushButton:hover { background-color: #353b44; }");
        }
        if (m_btnAutoTitle) {
            m_btnAutoTitle->setStyleSheet("QPushButton { background-color: #27272a; color: #e4e4e7; border: 1px solid #3f3f46; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #3f3f46; color: #38bdf8; }");
        }
        if (m_btnSearchTags) {
            m_btnSearchTags->setStyleSheet("QPushButton { background-color: #075985; color: #ffffff; border: 1px solid #0284c7; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #0284c7; }");
        }
        if (m_btnRenameFiles) {
            m_btnRenameFiles->setStyleSheet("QPushButton { background-color: #1e3a5f; color: #ffffff; border: 1px solid #3b82f6; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #2563eb; }");
        }
        if (m_btnRevert) {
            m_btnRevert->setStyleSheet("QPushButton { background-color: #282c32; color: #f59e0b; border: 1px solid #d97706; border-radius: 6px; padding: 7px 16px; font-weight: bold; } QPushButton:hover { background-color: #78350f; color: #ffffff; }");
        }
        if (m_btnSave) {
            m_btnSave->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; border: 1px solid #0369a1; border-radius: 6px; padding: 7px 20px; font-size: 13px; } QPushButton:hover { background-color: #0369a1; }");
        }
        if (m_btnCancel) {
            m_btnCancel->setStyleSheet("QPushButton { background-color: #282c32; color: #eff0f1; border: 1px solid #353b44; border-radius: 6px; padding: 7px 16px; font-size: 13px; } QPushButton:hover { background-color: #353b44; }");
        }
    } else if (preset == ThemePreset::BreezeDark) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #181b1d;"
            "   color: #eff0f1;"
            "   font-family: sans-serif;"
            "}"
            "QLabel {"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QLineEdit {"
            "   background-color: #22262a;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "   selection-background-color: #3daee9;"
            "}"
            "QLineEdit:focus {"
            "   border-color: #3daee9;"
            "}"
            "QGroupBox {"
            "   border: 1px solid #23272b;"
            "   border-radius: 8px;"
            "   margin-top: 10px;"
            "   padding-top: 14px;"
            "   padding-bottom: 10px;"
            "   padding-left: 10px;"
            "   padding-right: 10px;"
            "   font-weight: bold;"
            "   font-size: 13px;"
            "   color: #eff0f1;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top left;"
            "   left: 10px;"
            "   padding: 0 4px;"
            "   color: #3daee9;"
            "}"
            "QTableWidget {"
            "   background-color: #181b1d;"
            "   alternate-background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #23272b;"
            "   border-radius: 6px;"
            "   gridline-color: #23272b;"
            "   selection-background-color: #2a3037;"
            "   selection-color: #ffffff;"
            "   font-size: 13px;"
            "}"
            "QHeaderView::section {"
            "   background-color: #22262a;"
            "   color: #cfd2d5;"
            "   padding: 6px;"
            "   border: 1px solid #23272b;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "}"
            "QCheckBox {"
            "   color: #cfd2d5;"
            "   font-size: 12px;"
            "   spacing: 4px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 15px;"
            "   height: 15px;"
            "   background-color: #22262a;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 4px;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #3daee9;"
            "   border-color: #3daee9;"
            "}"
            "QPushButton {"
            "   background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 6px 12px;"
            "   font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2a3037;"
            "   color: #ffffff;"
            "}"
        );
        if (m_lblCoverImage) {
            m_lblCoverImage->setStyleSheet("background-color: #181b1d; border: 1px solid #23272b; border-radius: 8px; color: #a1a1aa;");
        }
        if (m_btnDownloadCover) {
            m_btnDownloadCover->setStyleSheet("QPushButton { background-color: #22262a; color: #3daee9; border: 1px solid #2a3037; border-radius: 6px; padding: 5px 10px; font-weight: bold; } QPushButton:hover { background-color: #2a3037; color: #ffffff; }");
        }
        if (m_btnChangeCover) {
            m_btnChangeCover->setStyleSheet("QPushButton { background-color: #22262a; color: #eff0f1; border: 1px solid #2a3037; border-radius: 6px; padding: 5px 10px; } QPushButton:hover { background-color: #2a3037; color: #3daee9; }");
        }
        if (m_btnRemoveCover) {
            m_btnRemoveCover->setStyleSheet("QPushButton { background-color: #22262a; color: #ef4444; border: 1px solid #2a3037; border-radius: 6px; padding: 5px; } QPushButton:hover { background-color: #2a3037; }");
        }
        if (m_btnAutoTitle) {
            m_btnAutoTitle->setStyleSheet("QPushButton { background-color: #22262a; color: #eff0f1; border: 1px solid #2a3037; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #2a3037; color: #3daee9; }");
        }
        if (m_btnSearchTags) {
            m_btnSearchTags->setStyleSheet("QPushButton { background-color: #1b4965; color: #ffffff; border: 1px solid #3daee9; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #2980b9; }");
        }
        if (m_btnRenameFiles) {
            m_btnRenameFiles->setStyleSheet("QPushButton { background-color: #1e3a5f; color: #ffffff; border: 1px solid #3daee9; border-radius: 6px; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background-color: #2980b9; }");
        }
        if (m_btnRevert) {
            m_btnRevert->setStyleSheet("QPushButton { background-color: #22262a; color: #f59e0b; border: 1px solid #d97706; border-radius: 6px; padding: 7px 16px; font-weight: bold; } QPushButton:hover { background-color: #78350f; color: #ffffff; }");
        }
        if (m_btnSave) {
            m_btnSave->setStyleSheet("QPushButton { background-color: #3daee9; color: white; font-weight: bold; border: 1px solid #2980b9; border-radius: 6px; padding: 7px 20px; font-size: 13px; } QPushButton:hover { background-color: #2980b9; }");
        }
        if (m_btnCancel) {
            m_btnCancel->setStyleSheet("QPushButton { background-color: #22262a; color: #eff0f1; border: 1px solid #2a3037; border-radius: 6px; padding: 7px 16px; font-size: 13px; } QPushButton:hover { background-color: #2a3037; }");
        }
    } else {
        setStyleSheet("QGroupBox { font-weight: bold; }");
    }
}
