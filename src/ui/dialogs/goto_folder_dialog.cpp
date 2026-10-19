#include "goto_folder_dialog.h"
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>

GotoFolderDialog::GotoFolderDialog(const QString &currentPath, QWidget *parent)
    : QDialog(parent)
    , m_currentPath(currentPath)
    , m_pathEdit(nullptr)
    , m_bookmarksList(nullptr)
    , m_btnBrowse(nullptr)
    , m_btnGo(nullptr)
    , m_btnCancel(nullptr)
{
    setWindowTitle(tr("Go to Folder — BubezView"));
    resize(480, 360);
    setupUi();
    populateBookmarks();
}

void GotoFolderDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    QLabel *lblTitle = new QLabel(tr("📁 Enter or select folder to open:"), this);
    lblTitle->setStyleSheet("font-weight: bold; color: #38bdf8; font-size: 13px;");
    mainLayout->addWidget(lblTitle);

    // Path input + browse
    QHBoxLayout *editLayout = new QHBoxLayout();
    editLayout->setSpacing(8);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("e.g. /home/user/Documents or ~/Downloads"));
    QFileInfo curFi(m_currentPath);
    if (curFi.isDir()) {
        m_pathEdit->setText(curFi.absoluteFilePath());
    } else {
        m_pathEdit->setText(curFi.dir().absolutePath());
    }
    m_pathEdit->setStyleSheet("background-color: #1e293b; color: #f8fafc; padding: 6px 10px; border: 1px solid #475569; border-radius: 6px;");
    editLayout->addWidget(m_pathEdit, 1);

    m_btnBrowse = new QPushButton(tr("Browse..."), this);
    m_btnBrowse->setStyleSheet("background-color: #334155; color: #f8fafc; padding: 6px 12px; border-radius: 6px; border: 1px solid #475569;");
    connect(m_btnBrowse, &QPushButton::clicked, this, &GotoFolderDialog::onBrowseClicked);
    editLayout->addWidget(m_btnBrowse);

    mainLayout->addLayout(editLayout);

    // Bookmarks list
    QLabel *lblQuick = new QLabel(tr("Favorite & recent locations:"), this);
    lblQuick->setStyleSheet("color: #94a3b8; font-size: 12px;");
    mainLayout->addWidget(lblQuick);

    m_bookmarksList = new QListWidget(this);
    m_bookmarksList->setStyleSheet(
        "QListWidget {"
        "   background-color: #1e293b;"
        "   color: #e2e8f0;"
        "   border: 1px solid #334155;"
        "   border-radius: 6px;"
        "   padding: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 7px 10px;"
        "   border-radius: 4px;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #334155;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "}"
    );
    connect(m_bookmarksList, &QListWidget::itemClicked, this, &GotoFolderDialog::onBookmarkClicked);
    connect(m_bookmarksList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        onBookmarkClicked(item);
        onAcceptClicked();
    });
    mainLayout->addWidget(m_bookmarksList, 1);

    // Bottom action buttons
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setStyleSheet("background-color: #334155; color: #f1f5f9; padding: 7px 14px; border-radius: 6px; border: none;");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionsLayout->addWidget(m_btnCancel);

    m_btnGo = new QPushButton(tr("➡️ Go"), this);
    m_btnGo->setStyleSheet("background-color: #0284c7; color: #ffffff; font-weight: bold; padding: 7px 18px; border-radius: 6px; border: none;");
    connect(m_btnGo, &QPushButton::clicked, this, &GotoFolderDialog::onAcceptClicked);
    actionsLayout->addWidget(m_btnGo);

    mainLayout->addLayout(actionsLayout);
}

void GotoFolderDialog::populateBookmarks()
{
    struct Bookmark {
        QString name;
        QString path;
        QString icon;
    };

    QList<Bookmark> bms = {
        {"🏠 " + tr("Home Folder"), QDir::homePath(), "user-home"},
        {"📄 " + tr("Documents"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "folder-documents"},
        {"⬇️ " + tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "folder-download"},
        {"🖼️ " + tr("Pictures"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "folder-pictures"},
        {"🎵 " + tr("Music"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "folder-music"},
        {"🎬 " + tr("Videos"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), "folder-videos"},
        {"🖥️ " + tr("Desktop"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "user-desktop"}
    };

    for (const auto &b : bms) {
        if (!b.path.isEmpty() && QDir(b.path).exists()) {
            QListWidgetItem *item = new QListWidgetItem(QString("%1  (%2)").arg(b.name, b.path), m_bookmarksList);
            item->setData(Qt::UserRole, b.path);
        }
    }
}

void GotoFolderDialog::onBookmarkClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
    }
}

void GotoFolderDialog::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder"), m_pathEdit->text());
    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void GotoFolderDialog::onAcceptClicked()
{
    QString target = m_pathEdit->text().trimmed();
    if (target.startsWith("~")) {
        target = QDir::homePath() + target.mid(1);
    }

    QDir d(target);
    if (!d.exists()) {
        QMessageBox::warning(this, tr("Invalid Path"), tr("The specified folder does not exist on the system."));
        return;
    }

    m_selectedFolder = d.canonicalPath();
    accept();
}
