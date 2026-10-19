#include "ocr_language_dialog.h"
#include "ocr_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>

OcrLanguageDialog::OcrLanguageDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage OCR Languages ⚙️"));
    resize(580, 480);
    setModal(true);

    setupUi();
    refreshLanguagesList();

    connect(&OcrManager::instance(), &OcrManager::downloadProgress,
            this, &OcrLanguageDialog::onDownloadProgress);
    connect(&OcrManager::instance(), &OcrManager::downloadFinished,
            this, &OcrLanguageDialog::onDownloadFinished);
    connect(&OcrManager::instance(), &OcrManager::languagesUpdated,
            this, &OcrLanguageDialog::refreshLanguagesList);
}

void OcrLanguageDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *infoLabel = new QLabel(
        tr("<b>Tesseract Language Models</b><br>"
           "Download language packages directly from GitHub tessdata_fast into your user folder.<br>"
           "<i>No administrator privileges (sudo/root) or package managers required.</i>"), this);
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *lblSearch = new QLabel(tr("🔍 Search:"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter by language name or code (e.g. Italian, spa, deu)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &OcrLanguageDialog::filterLanguages);
    searchLayout->addWidget(lblSearch);
    searchLayout->addWidget(m_searchEdit);
    mainLayout->addLayout(searchLayout);

    // Languages table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Code") << tr("Language") << tr("Status") << tr("Action"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 130);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_table, 1);

    // Custom ISO code row
    QHBoxLayout *customLayout = new QHBoxLayout();
    QLabel *lblCustom = new QLabel(tr("Custom Language Code:"), this);
    m_customCodeEdit = new QLineEdit(this);
    m_customCodeEdit->setPlaceholderText(tr("e.g. lat, kor, cat"));
    m_customCodeEdit->setMaximumWidth(100);
    m_btnCustomDownload = new QPushButton(tr("⬇️ Download Custom"), this);
    connect(m_btnCustomDownload, &QPushButton::clicked, this, &OcrLanguageDialog::handleCustomDownload);

    customLayout->addWidget(lblCustom);
    customLayout->addWidget(m_customCodeEdit);
    customLayout->addWidget(m_btnCustomDownload);
    customLayout->addStretch();
    mainLayout->addLayout(customLayout);

    // Bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(m_statusLabel);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);
}

void OcrLanguageDialog::refreshLanguagesList()
{
    m_table->setRowCount(0);
    m_activeBars.clear();
    m_activeButtons.clear();

    QList<OcrManager::LanguageInfo> catalog = OcrManager::instance().popularLanguagesCatalog();
    const QStringList installed = OcrManager::instance().installedLanguages();

    // Also include any installed language that might not be in the default popular catalog
    for (const QString &code : installed) {
        bool found = false;
        for (const auto &info : catalog) {
            if (info.code == code) {
                found = true;
                break;
            }
        }
        if (!found) {
            OcrManager::LanguageInfo extra = OcrManager::instance().languageInfo(code);
            extra.isInstalled = true;
            catalog.append(extra);
        }
    }

    int row = 0;
    for (const auto &info : catalog) {
        m_table->insertRow(row);

        // Code
        QTableWidgetItem *itemCode = new QTableWidgetItem(info.code);
        itemCode->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, itemCode);

        // Name
        QString dispName = QString("%1 (%2)").arg(info.englishName, info.nativeName);
        QTableWidgetItem *itemName = new QTableWidgetItem(dispName);
        m_table->setItem(row, 1, itemName);

        // Status
        QTableWidgetItem *itemStatus = new QTableWidgetItem();
        itemStatus->setTextAlignment(Qt::AlignCenter);
        if (info.isInstalled) {
            itemStatus->setText(tr("✅ Installed"));
            itemStatus->setForeground(QColor("#10b981"));
        } else {
            itemStatus->setText(tr("Not Installed"));
            itemStatus->setForeground(QColor("#9ca3af"));
        }
        m_table->setItem(row, 2, itemStatus);

        // Action widget
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *actLayout = new QHBoxLayout(actionWidget);
        actLayout->setContentsMargins(4, 2, 4, 2);

        if (info.isInstalled) {
            QPushButton *btnDone = new QPushButton(tr("Installed"), actionWidget);
            btnDone->setEnabled(false);
            actLayout->addWidget(btnDone);
        } else {
            QPushButton *btnDl = new QPushButton(tr("⬇️ Download"), actionWidget);
            QProgressBar *bar = new QProgressBar(actionWidget);
            bar->setRange(0, 100);
            bar->setValue(0);
            bar->setTextVisible(true);
            bar->setVisible(false);

            m_activeButtons[info.code] = btnDl;
            m_activeBars[info.code] = bar;

            QString langCode = info.code;
            connect(btnDl, &QPushButton::clicked, this, [this, langCode, btnDl, bar]() {
                handleDownloadClicked(langCode, btnDl, bar);
            });

            actLayout->addWidget(btnDl);
            actLayout->addWidget(bar);
        }

        m_table->setCellWidget(row, 3, actionWidget);
        row++;
    }

    filterLanguages(m_searchEdit->text());
}

void OcrLanguageDialog::filterLanguages(const QString &query)
{
    QString q = query.trimmed().toLower();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString code = m_table->item(r, 0)->text().toLower();
        QString name = m_table->item(r, 1)->text().toLower();
        bool match = q.isEmpty() || code.contains(q) || name.contains(q);
        m_table->setRowHidden(r, !match);
    }
}

void OcrLanguageDialog::handleDownloadClicked(const QString &langCode, QPushButton *btn, QProgressBar *bar)
{
    btn->setVisible(false);
    bar->setVisible(true);
    bar->setRange(0, 0); // Indeterminate initially
    m_statusLabel->setText(tr("Downloading %1...").arg(langCode));

    OcrManager::instance().downloadLanguage(langCode);
}

void OcrLanguageDialog::handleCustomDownload()
{
    QString code = m_customCodeEdit->text().trimmed().toLower();
    if (code.isEmpty()) return;

    if (OcrManager::instance().isLanguageInstalled(code)) {
        QMessageBox::information(this, tr("OCR Languages"), tr("Language '%1' is already installed.").arg(code));
        return;
    }

    m_btnCustomDownload->setEnabled(false);
    m_statusLabel->setText(tr("Downloading custom language '%1'...").arg(code));
    OcrManager::instance().downloadLanguage(code);
}

void OcrLanguageDialog::onDownloadProgress(const QString &langCode, qint64 rec, qint64 tot)
{
    if (m_activeBars.contains(langCode)) {
        QProgressBar *bar = m_activeBars[langCode];
        if (tot > 0) {
            bar->setRange(0, 100);
            int pct = static_cast<int>((rec * 100) / tot);
            bar->setValue(pct);
        }
    }
}

void OcrLanguageDialog::onDownloadFinished(const QString &langCode, bool success, const QString &err)
{
    m_btnCustomDownload->setEnabled(true);

    if (success) {
        m_statusLabel->setText(tr("Language '%1' downloaded successfully!").arg(langCode));
        refreshLanguagesList();
    } else {
        m_statusLabel->setText(tr("Download failed for '%1'.").arg(langCode));
        QMessageBox::warning(this, tr("Download Failed"),
                             tr("Could not download model for '%1':<br>%2").arg(langCode, err.toHtmlEscaped()));
        refreshLanguagesList();
    }
}
