#include "cover_selection_dialog.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QEventLoop>
#include <QStandardPaths>
#include <QDateTime>
#include <QTimer>
#include <QMouseEvent>
#include <QApplication>

/**
 * @class CoverCardWidget
 * @brief Clickable card widget displaying an individual album cover candidate.
 */
class CoverCardWidget : public QFrame {
public:
    CoverCardWidget(int index, const CoverCandidate &candidate, QWidget *parent = nullptr)
        : QFrame(parent)
        , m_index(index)
        , m_isSelected(false)
    {
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(170, 235);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(4);

        // Thumbnail image
        m_lblImage = new QLabel(this);
        m_lblImage->setFixedSize(154, 154);
        m_lblImage->setAlignment(Qt::AlignCenter);
        if (!candidate.thumbPixmap.isNull()) {
            m_lblImage->setPixmap(candidate.thumbPixmap.scaled(154, 154, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_lblImage->setText(QObject::tr("Loading..."));
        }
        layout->addWidget(m_lblImage, 0, Qt::AlignCenter);

        // Album Title
        m_lblAlbum = new QLabel(candidate.album, this);
        m_lblAlbum->setAlignment(Qt::AlignCenter);
        QFont fAlbum = m_lblAlbum->font();
        fAlbum.setBold(true);
        fAlbum.setPointSize(9);
        m_lblAlbum->setFont(fAlbum);
        m_lblAlbum->setWordWrap(true);
        m_lblAlbum->setMaximumHeight(34);
        layout->addWidget(m_lblAlbum);

        // Artist & Year
        QString subText = candidate.artist;
        if (!candidate.year.isEmpty()) {
            subText += QString(" (%1)").arg(candidate.year);
        }
        m_lblArtist = new QLabel(subText, this);
        m_lblArtist->setAlignment(Qt::AlignCenter);
        QFont fArtist = m_lblArtist->font();
        fArtist.setPointSize(8);
        m_lblArtist->setFont(fArtist);
        m_lblArtist->setStyleSheet("color: #9ca3af;");
        layout->addWidget(m_lblArtist);

        layout->addStretch();
        updateStyle();
    }

    void setSelected(bool sel) {
        m_isSelected = sel;
        updateStyle();
    }

    int index() const { return m_index; }

    std::function<void(int)> onClicked;
    std::function<void(int)> onDoubleClicked;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (onClicked) onClicked(m_index);
        }
        QFrame::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (onDoubleClicked) onDoubleClicked(m_index);
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    void updateStyle() {
        if (m_isSelected) {
            setStyleSheet(
                "CoverCardWidget {"
                "   background-color: #0369a1;"
                "   border: 2px solid #38bdf8;"
                "   border-radius: 8px;"
                "}"
                "QLabel { color: #ffffff; }"
            );
        } else {
            setStyleSheet(
                "CoverCardWidget {"
                "   background-color: #1e2226;"
                "   border: 1px solid #2e343c;"
                "   border-radius: 8px;"
                "}"
                "CoverCardWidget:hover {"
                "   background-color: #282d34;"
                "   border: 1px solid #60a5fa;"
                "}"
                "QLabel { color: #eff0f1; }"
            );
        }
    }

    int m_index;
    bool m_isSelected;
    QLabel *m_lblImage;
    QLabel *m_lblAlbum;
    QLabel *m_lblArtist;
};

CoverSelectionDialog::CoverSelectionDialog(const QString &initialQuery, QWidget *parent)
    : QDialog(parent)
    , m_initialQuery(initialQuery)
    , m_selectedIndex(-1)
{
    setWindowTitle(tr("Select Album Cover Art - BubezView"));
    setMinimumSize(680, 560);
    resize(740, 620);

    setupUi();
    applyTheme();

    if (!m_initialQuery.isEmpty()) {
        m_txtSearchQuery->setText(m_initialQuery);
        QTimer::singleShot(100, this, [this]() {
            executeSearch(m_initialQuery);
        });
    }
}

CoverSelectionDialog::~CoverSelectionDialog() = default;

void CoverSelectionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // 1. Top Search Bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(8);

    m_txtSearchQuery = new QLineEdit(this);
    m_txtSearchQuery->setPlaceholderText(tr("Enter artist and album name to search online..."));
    m_txtSearchQuery->setClearButtonEnabled(true);
    connect(m_txtSearchQuery, &QLineEdit::returnPressed, this, &CoverSelectionDialog::onSearchClicked);

    m_btnSearch = new QPushButton(tr("🔍 Search"), this);
    m_btnSearch->setCursor(Qt::PointingHandCursor);
    connect(m_btnSearch, &QPushButton::clicked, this, &CoverSelectionDialog::onSearchClicked);

    searchLayout->addWidget(m_txtSearchQuery, 1);
    searchLayout->addWidget(m_btnSearch);
    mainLayout->addLayout(searchLayout);

    // 2. Status Label
    m_lblStatus = new QLabel(tr("Search for album covers online or select a local image file."), this);
    m_lblStatus->setStyleSheet("color: #94a3b8; font-size: 12px;");
    mainLayout->addWidget(m_lblStatus);

    // 3. Center Scrollable Grid
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_gridContainer = new QWidget(m_scrollArea);
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(8, 8, 8, 8);
    m_gridLayout->setSpacing(14);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scrollArea->setWidget(m_gridContainer);

    mainLayout->addWidget(m_scrollArea, 1);

    // 4. Bottom Action Buttons
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(10);

    m_btnBrowseLocal = new QPushButton(tr("📁 Choose Local File..."), this);
    m_btnBrowseLocal->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowseLocal, &QPushButton::clicked, this, &CoverSelectionDialog::onBrowseLocalClicked);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnApply = new QPushButton(tr("Apply Selected Cover"), this);
    m_btnApply->setCursor(Qt::PointingHandCursor);
    m_btnApply->setEnabled(false);
    connect(m_btnApply, &QPushButton::clicked, this, &CoverSelectionDialog::onAcceptSelection);

    actionsLayout->addWidget(m_btnBrowseLocal);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_btnCancel);
    actionsLayout->addWidget(m_btnApply);

    mainLayout->addLayout(actionsLayout);
}

void CoverSelectionDialog::applyTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::DarkSlate) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #22262b;"
            "   color: #eff0f1;"
            "}"
            "QLineEdit {"
            "   background-color: #282c32;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 7px 10px;"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "   border-color: #61afef;"
            "}"
            "QPushButton {"
            "   background-color: #282c32;"
            "   color: #eff0f1;"
            "   border: 1px solid #353b44;"
            "   border-radius: 6px;"
            "   padding: 7px 14px;"
            "   font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #353b44;"
            "   color: #ffffff;"
            "}"
        );
        m_btnSearch->setStyleSheet("QPushButton { background-color: #075985; color: #ffffff; border: 1px solid #0284c7; font-weight: bold; border-radius: 6px; padding: 7px 16px; } QPushButton:hover { background-color: #0284c7; }");
        m_btnApply->setStyleSheet("QPushButton { background-color: #0284c7; color: #ffffff; border: 1px solid #0369a1; font-weight: bold; border-radius: 6px; padding: 7px 18px; } QPushButton:hover { background-color: #0369a1; } QPushButton:disabled { background-color: #282c32; color: #6b7280; border-color: #353b44; }");
    } else if (preset == ThemePreset::BreezeDark) {
        setStyleSheet(
            "QDialog {"
            "   background-color: #181b1d;"
            "   color: #eff0f1;"
            "}"
            "QLineEdit {"
            "   background-color: #22262a;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 7px 10px;"
            "   color: #eff0f1;"
            "   font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "   border-color: #3daee9;"
            "}"
            "QPushButton {"
            "   background-color: #22262a;"
            "   color: #eff0f1;"
            "   border: 1px solid #2a3037;"
            "   border-radius: 6px;"
            "   padding: 7px 14px;"
            "   font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2a3037;"
            "   color: #ffffff;"
            "}"
        );
        m_btnSearch->setStyleSheet("QPushButton { background-color: #1b4965; color: #ffffff; border: 1px solid #3daee9; font-weight: bold; border-radius: 6px; padding: 7px 16px; } QPushButton:hover { background-color: #2980b9; }");
        m_btnApply->setStyleSheet("QPushButton { background-color: #3daee9; color: #ffffff; border: 1px solid #2980b9; font-weight: bold; border-radius: 6px; padding: 7px 18px; } QPushButton:hover { background-color: #2980b9; } QPushButton:disabled { background-color: #22262a; color: #6b7280; border-color: #2a3037; }");
    }
}

void CoverSelectionDialog::onSearchClicked()
{
    QString q = m_txtSearchQuery->text().trimmed();
    if (q.isEmpty()) return;
    executeSearch(q);
}

void CoverSelectionDialog::executeSearch(const QString &query)
{
    m_candidates.clear();
    m_selectedIndex = -1;
    m_btnApply->setEnabled(false);

    // Clear existing cards
    for (CoverCardWidget *w : m_cardWidgets) {
        m_gridLayout->removeWidget(w);
        delete w;
    }
    m_cardWidgets.clear();

    m_lblStatus->setText(tr("Searching iTunes Store & Cover Art Archive for \"%1\"...").arg(query));
    qApp->processEvents();

    // 1. Query iTunes Search API
    QString itunesUrlStr = QString("https://itunes.apple.com/search?term=%1&entity=album&limit=12")
                               .arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    QUrl itunesUrl(itunesUrlStr);
    QNetworkRequest req(itunesUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(4000);

    QEventLoop loop;
    QNetworkReply *reply = m_netMgr.get(req);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonArray results = doc.object().value("results").toArray();
            for (const QJsonValue &v : results) {
                QJsonObject obj = v.toObject();
                QString artwork = obj.value("artworkUrl100").toString();
                if (artwork.isEmpty()) continue;

                CoverCandidate cand;
                cand.album = obj.value("collectionName").toString();
                cand.artist = obj.value("artistName").toString();
                QString relDate = obj.value("releaseDate").toString();
                if (relDate.length() >= 4) {
                    cand.year = relDate.left(4);
                }

                // Prepare crisp 200x200 thumbnail and 1000x1000 high-res URLs
                QString thumbUrl = artwork;
                thumbUrl.replace("100x100bb.jpg", "300x300bb.jpg");
                QString highResUrl = artwork;
                highResUrl.replace("100x100bb.jpg", "1000x1000bb.jpg");

                cand.thumbUrl = thumbUrl;
                cand.highResUrl = highResUrl;
                m_candidates.append(cand);
            }
        }
    }
    reply->deleteLater();

    // 2. If iTunes returned few results, query MusicBrainz
    if (m_candidates.size() < 4) {
        QString mbUrlStr = QString("https://musicbrainz.org/ws/2/release/?query=%1&fmt=json&limit=6")
                               .arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
        QUrl mbUrl(mbUrlStr);
        QNetworkRequest mbReq(mbUrl);
        mbReq.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/0.1.8 ( https://github.com/davide/bubezview )");
        mbReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        mbReq.setTransferTimeout(4000);

        QNetworkReply *mbReply = m_netMgr.get(mbReq);
        connect(mbReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (mbReply->error() == QNetworkReply::NoError) {
            QByteArray mbData = mbReply->readAll();
            QJsonDocument mbDoc = QJsonDocument::fromJson(mbData);
            if (mbDoc.isObject()) {
                QJsonArray releases = mbDoc.object().value("releases").toArray();
                for (const QJsonValue &rv : releases) {
                    QJsonObject rObj = rv.toObject();
                    QString mbid = rObj.value("id").toString();
                    if (mbid.isEmpty()) continue;

                    CoverCandidate mbCand;
                    mbCand.album = rObj.value("title").toString();
                    mbCand.artist = rObj.value("artist-credit").toArray().first().toObject().value("name").toString();
                    mbCand.year = rObj.value("date").toString().left(4);
                    mbCand.thumbUrl = QString("https://coverartarchive.org/release/%1/front-250").arg(mbid);
                    mbCand.highResUrl = QString("https://coverartarchive.org/release/%1/front-1200").arg(mbid);
                    m_candidates.append(mbCand);
                }
            }
        }
        mbReply->deleteLater();
    }

    if (m_candidates.isEmpty()) {
        m_lblStatus->setText(tr("No cover artwork found for \"%1\". Try refining your search query or choose a local file.").arg(query));
        return;
    }

    m_lblStatus->setText(tr("Downloading preview thumbnails (%1 found)...").arg(m_candidates.size()));
    qApp->processEvents();

    fetchThumbnails();
    populateGrid();
}

void CoverSelectionDialog::fetchThumbnails()
{
    QEventLoop loop;
    for (int i = 0; i < m_candidates.size(); ++i) {
        CoverCandidate &cand = m_candidates[i];
        QUrl url(cand.thumbUrl);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/0.1.8");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(3000);

        QNetworkReply *reply = m_netMgr.get(req);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imgBytes = reply->readAll();
            QPixmap pix;
            if (pix.loadFromData(imgBytes)) {
                cand.thumbPixmap = pix;
            }
        }
        reply->deleteLater();
    }

    // Filter out candidates whose thumbnails failed to load
    QList<CoverCandidate> validCandidates;
    for (const CoverCandidate &cand : m_candidates) {
        if (!cand.thumbPixmap.isNull()) {
            validCandidates.append(cand);
        }
    }
    m_candidates = validCandidates;
}

void CoverSelectionDialog::populateGrid()
{
    for (CoverCardWidget *w : m_cardWidgets) {
        m_gridLayout->removeWidget(w);
        delete w;
    }
    m_cardWidgets.clear();

    if (m_candidates.isEmpty()) {
        m_lblStatus->setText(tr("No image previews could be loaded for this query. Please try another search."));
        return;
    }

    m_lblStatus->setText(tr("Found %1 cover options. Click on a cover to select it, or double-click to apply:").arg(m_candidates.size()));

    const int columns = 3;
    for (int i = 0; i < m_candidates.size(); ++i) {
        CoverCardWidget *card = new CoverCardWidget(i, m_candidates[i], m_gridContainer);
        card->onClicked = [this](int idx) {
            onCardClicked(idx);
        };
        card->onDoubleClicked = [this](int idx) {
            onCardDoubleClicked(idx);
        };

        int row = i / columns;
        int col = i % columns;
        m_gridLayout->addWidget(card, row, col);
        m_cardWidgets.append(card);
    }

    // Auto-select first item
    if (!m_cardWidgets.isEmpty()) {
        onCardClicked(0);
    }
}

void CoverSelectionDialog::onCardClicked(int index)
{
    if (index < 0 || index >= m_candidates.size()) return;
    m_selectedIndex = index;

    for (int i = 0; i < m_cardWidgets.size(); ++i) {
        if (m_cardWidgets[i]) {
            m_cardWidgets[i]->setSelected(i == index);
        }
    }

    m_btnApply->setEnabled(true);
}

void CoverSelectionDialog::onCardDoubleClicked(int index)
{
    onCardClicked(index);
    onAcceptSelection();
}

void CoverSelectionDialog::onAcceptSelection()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
        return;
    }

    const CoverCandidate &selected = m_candidates[m_selectedIndex];
    m_lblStatus->setText(tr("Downloading high-resolution cover artwork..."));
    m_btnApply->setEnabled(false);
    qApp->processEvents();

    QString tempFile;
    QString err;
    bool ok = downloadHighResCover(selected.highResUrl, tempFile, err);

    // Fallback to thumbnail URL if high-res failed
    if (!ok && !selected.thumbUrl.isEmpty()) {
        ok = downloadHighResCover(selected.thumbUrl, tempFile, err);
    }

    if (ok && !tempFile.isEmpty()) {
        m_selectedCoverPath = tempFile;
        accept();
    } else {
        QMessageBox::warning(this, tr("Download Failed"), tr("Failed to download cover image: %1").arg(err));
        m_btnApply->setEnabled(true);
    }
}

bool CoverSelectionDialog::downloadHighResCover(const QString &urlStr, QString &outFilePath, QString &outError)
{
    QUrl url(urlStr);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "BubezView/0.1.8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(6000);

    QEventLoop loop;
    QNetworkReply *reply = m_netMgr.get(req);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        outError = reply->errorString();
        reply->deleteLater();
        return false;
    }

    QByteArray bytes = reply->readAll();
    reply->deleteLater();

    QImage img;
    if (!img.loadFromData(bytes)) {
        outError = tr("Invalid image format returned by server.");
        return false;
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString path = QString("%1/bubezview_cover_%2.jpg").arg(tempDir).arg(QDateTime::currentMSecsSinceEpoch());
    if (!img.save(path, "JPG", 95)) {
        outError = tr("Could not write temporary image file.");
        return false;
    }

    outFilePath = path;
    return true;
}

void CoverSelectionDialog::onBrowseLocalClicked()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select Local Cover Image"),
        QString(),
        tr("Images (*.jpg *.jpeg *.png *.webp *.bmp)")
    );

    if (!file.isEmpty()) {
        m_selectedCoverPath = file;
        accept();
    }
}
