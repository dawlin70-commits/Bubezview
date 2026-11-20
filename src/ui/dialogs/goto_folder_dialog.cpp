#include "goto_folder_dialog.h"
#include "core/services/theme_manager.h"
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>

GotoFolderDialog::GotoFolderDialog(const QString &currentPath, QWidget *parent)
    : FramelessDialog(parent)
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
    const bool isDark = ThemeManager::isDarkTheme();
    const QString titleCol = isDark ? QStringLiteral("#38bdf8") : QStringLiteral("#0284c7");
    const QString subCol = isDark ? QStringLiteral("#94a3b8") : QStringLiteral("#475569");
    const QString inputBg = isDark ? QStringLiteral("#1e293b") : QStringLiteral("#ffffff");
    const QString inputFg = isDark ? QStringLiteral("#f8fafc") : QStringLiteral("#0f172a");
    const QString borderCol = isDark ? QStringLiteral("#475569") : QStringLiteral("#cbd5e1");
    const QString listBorder = isDark ? QStringLiteral("#334155") : QStringLiteral("#cbd5e1");
    const QString listHover = isDark ? QStringLiteral("#334155") : QStringLiteral("#e2e8f0");
    const QString btnBg = isDark ? QStringLiteral("#334155") : QStringLiteral("#e2e8f0");
    const QString btnFg = isDark ? QStringLiteral("#f1f5f9") : QStringLiteral("#1e293b");

    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(12);

    QLabel *lblTitle = new QLabel(tr("📁 Enter or select folder to open:"), this);
    lblTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: %1; font-size: 13px;").arg(titleCol));
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
    m_pathEdit->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; padding: 6px 10px; border: 1px solid %3; border-radius: 6px;")
        .arg(inputBg, inputFg, borderCol));
    editLayout->addWidget(m_pathEdit, 1);

    m_btnBrowse = new QPushButton(tr("Browse..."), this);
    m_btnBrowse->setCursor(Qt::PointingHandCursor);
    m_btnBrowse->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; padding: 6px 12px; border-radius: 6px; border: 1px solid %3;")
        .arg(btnBg, btnFg, borderCol));
    connect(m_btnBrowse, &QPushButton::clicked, this, &GotoFolderDialog::onBrowseClicked);
    editLayout->addWidget(m_btnBrowse);

    mainLayout->addLayout(editLayout);

    // Bookmarks list
    QLabel *lblQuick = new QLabel(tr("Favorite & recent locations:"), this);
    lblQuick->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subCol));
    mainLayout->addWidget(lblQuick);

    m_bookmarksList = new QListWidget(this);
    m_bookmarksList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 6px;"
        "   padding: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 7px 10px;"
        "   border-radius: 4px;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: %4;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "}").arg(inputBg, inputFg, listBorder, listHover));
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
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; padding: 7px 14px; border-radius: 6px; border: none;")
        .arg(btnBg, btnFg));
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionsLayout->addWidget(m_btnCancel);

    m_btnGo = new QPushButton(tr("➡️ Go"), this);
    m_btnGo->setCursor(Qt::PointingHandCursor);
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
    };

    QList<Bookmark> bms = {
        {"🏠 " + tr("Home Folder"), QDir::homePath()},
        {"📄 " + tr("Documents"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)},
        {"⬇️ " + tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)},
        {"🖼️ " + tr("Pictures"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
        {"🎵 " + tr("Music"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation)},
        {"🎬 " + tr("Videos"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)},
        {"🖥️ " + tr("Desktop"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)}
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
