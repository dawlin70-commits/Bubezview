#include "open_with_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMessageBox>

OpenWithDialog::OpenWithDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_filterEdit(nullptr)
    , m_listWidget(nullptr)
    , m_btnBrowse(nullptr)
    , m_btnOpen(nullptr)
    , m_btnCancel(nullptr)
{
    setupUi();
    populateApps();
}

void OpenWithDialog::setupUi()
{
    setWindowTitle(tr("Open With... — BubezView"));
    resize(460, 480);

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(16, 16, 16, 16);
    mainLay->setSpacing(10);

    QFileInfo fi(m_filePath);
    QMimeDatabase db;
    QString mime = db.mimeTypeForFile(m_filePath).comment();
    if (mime.isEmpty()) mime = db.mimeTypeForFile(m_filePath).name();

    QLabel *lblHeader = new QLabel(
        QString("<b>%1</b><br><small style='color: #888;'>%2 • %3</small>")
            .arg(fi.fileName(), mime, fi.dir().path()),
        this
    );
    lblHeader->setWordWrap(true);
    mainLay->addWidget(lblHeader);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Search installed applications..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &OpenWithDialog::onFilterChanged);
    mainLay->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    m_listWidget->setIconSize(QSize(28, 28));
    m_listWidget->setStyleSheet(
        "QListWidget::item {"
        "   padding: 6px 8px;"
        "   border-radius: 4px;"
        "}"
    );
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &OpenWithDialog::onAppItemDoubleClicked);
    mainLay->addWidget(m_listWidget, 1);

    QHBoxLayout *btnLay = new QHBoxLayout();
    m_btnBrowse = new QPushButton(tr("Choose Executable..."), this);
    m_btnBrowse->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowse, &QPushButton::clicked, this, &OpenWithDialog::onBrowseExecutable);
    btnLay->addWidget(m_btnBrowse);

    btnLay->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLay->addWidget(m_btnCancel);

    m_btnOpen = new QPushButton(tr("Open"), this);
    m_btnOpen->setCursor(Qt::PointingHandCursor);
    m_btnOpen->setDefault(true);
    connect(m_btnOpen, &QPushButton::clicked, this, &OpenWithDialog::onAccept);
    btnLay->addWidget(m_btnOpen);

    mainLay->addLayout(btnLay);
}

void OpenWithDialog::populateApps()
{
    m_listWidget->clear();
    m_associatedApps = AppLauncher::getAssociatedApps(m_filePath);

    if (!m_associatedApps.isEmpty()) {
        auto *header = new QListWidgetItem(tr("Recommended Applications:"), m_listWidget);
        header->setFlags(Qt::NoItemFlags);
        QFont f = header->font();
        f.setBold(true);
        header->setFont(f);
        header->setForeground(palette().color(QPalette::PlaceholderText));

        for (int i = 0; i < m_associatedApps.size(); ++i) {
            const AppInfo &app = m_associatedApps[i];
            auto *item = new QListWidgetItem(app.icon(), app.name, m_listWidget);
            if (!app.genericName.isEmpty() && app.genericName != app.name) {
                item->setText(QString("%1 (%2)").arg(app.name, app.genericName));
            }
            item->setToolTip(app.comment.isEmpty() ? app.execCmd : app.comment);
            item->setData(Qt::UserRole, i); // Index in m_associatedApps
            item->setData(Qt::UserRole + 1, false); // isAllApps = false
        }
    }

    m_allApps = AppLauncher::getAllApps();
    auto *headerOther = new QListWidgetItem(tr("Other Applications:"), m_listWidget);
    headerOther->setFlags(Qt::NoItemFlags);
    QFont f2 = headerOther->font();
    f2.setBold(true);
    headerOther->setFont(f2);
    headerOther->setForeground(palette().color(QPalette::PlaceholderText));

    // Exclude ones already in recommended
    QSet<QString> assocIds;
    for (const AppInfo &a : m_associatedApps) assocIds.insert(a.id);

    for (int i = 0; i < m_allApps.size(); ++i) {
        const AppInfo &app = m_allApps[i];
        if (assocIds.contains(app.id)) continue;

        auto *item = new QListWidgetItem(app.icon(), app.name, m_listWidget);
        if (!app.genericName.isEmpty() && app.genericName != app.name) {
            item->setText(QString("%1 (%2)").arg(app.name, app.genericName));
        }
        item->setToolTip(app.comment.isEmpty() ? app.execCmd : app.comment);
        item->setData(Qt::UserRole, i); // Index in m_allApps
        item->setData(Qt::UserRole + 1, true); // isAllApps = true
    }

    // Select first selectable item
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->flags() & Qt::ItemIsEnabled) {
            m_listWidget->setCurrentRow(i);
            break;
        }
    }
}

void OpenWithDialog::onFilterChanged(const QString &text)
{
    QString q = text.trimmed();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!(item->flags() & Qt::ItemIsEnabled)) {
            item->setHidden(!q.isEmpty());
            continue;
        }
        bool match = item->text().contains(q, Qt::CaseInsensitive) ||
                     item->toolTip().contains(q, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void OpenWithDialog::onBrowseExecutable()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Custom Executable"), "/usr/bin");
    if (!path.isEmpty()) {
        m_customCommand = path;
        accept();
    }
}

void OpenWithDialog::onAppItemDoubleClicked(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    onAccept();
}

void OpenWithDialog::onAccept()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur || !(cur->flags() & Qt::ItemIsEnabled)) {
        if (!m_customCommand.isEmpty()) {
            accept();
        }
        return;
    }

    int idx = cur->data(Qt::UserRole).toInt();
    bool isAll = cur->data(Qt::UserRole + 1).toBool();

    if (isAll && idx >= 0 && idx < m_allApps.size()) {
        m_selectedApp = m_allApps[idx];
    } else if (!isAll && idx >= 0 && idx < m_associatedApps.size()) {
        m_selectedApp = m_associatedApps[idx];
    }

    accept();
}

AppInfo OpenWithDialog::selectedApp() const
{
    return m_selectedApp;
}

QString OpenWithDialog::customCommand() const
{
    return m_customCommand;
}
