/**
 * @file pdf_image_dialog.cpp
 * @brief Implementation of dedicated image and logo insertion dialog for PDFs.
 */

#include "pdf_image_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QListWidget>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTransform>

PdfImageDialog::PdfImageDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("🖼️ Insert Image / Logo"));
    resize(520, 480);
    setupUi();
    refreshSavedList();
}

PdfImageDialog::~PdfImageDialog() = default;

QString PdfImageDialog::imagesDirPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/bubezview");
    }
    QString dirPath = base + QStringLiteral("/logos");
    QDir().mkpath(dirPath);
    return dirPath;
}

void PdfImageDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    m_tabs = new QTabWidget(this);
    setupImportTab();
    setupSavedTab();
    mainLayout->addWidget(m_tabs, 1);

    // Bottom action buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(tr("Insert Image ➔"), this);
    m_btnOk->setDefault(true);
    m_btnOk->setEnabled(false);
    m_btnOk->setStyleSheet(QStringLiteral("background-color: #0284C7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 6px;"));
    connect(m_btnOk, &QPushButton::clicked, this, &PdfImageDialog::accept);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

void PdfImageDialog::setupImportTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(10);

    m_lblPreview = new QLabel(tab);
    m_lblPreview->setMinimumHeight(180);
    m_lblPreview->setAlignment(Qt::AlignCenter);
    m_lblPreview->setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: 2px dashed #94A3B8; border-radius: 8px; color: #475569; font-weight: 500;"));
    m_lblPreview->setText(tr("No image loaded.<br>Click 'Browse Image...' below to select a picture or logo."));
    layout->addWidget(m_lblPreview, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    m_btnBrowse = new QPushButton(tr("📂 Browse Image..."), tab);
    m_btnBrowse->setStyleSheet(QStringLiteral("font-weight: 600; padding: 6px 12px;"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &PdfImageDialog::onBrowseClicked);
    btnRow->addWidget(m_btnBrowse);

    m_btnRotateLeft = new QPushButton(tr("↺ Rotate 90° L"), tab);
    m_btnRotateLeft->setEnabled(false);
    connect(m_btnRotateLeft, &QPushButton::clicked, this, &PdfImageDialog::onRotateLeftClicked);
    btnRow->addWidget(m_btnRotateLeft);

    m_btnRotateRight = new QPushButton(tr("↻ Rotate 90° R"), tab);
    m_btnRotateRight->setEnabled(false);
    connect(m_btnRotateRight, &QPushButton::clicked, this, &PdfImageDialog::onRotateRightClicked);
    btnRow->addWidget(m_btnRotateRight);

    btnRow->addStretch(1);
    layout->addLayout(btnRow);

    m_chkSaveFavorite = new QCheckBox(tr("⭐ Save to Favorites / Logo Gallery"), tab);
    m_chkSaveFavorite->setChecked(false);
    layout->addWidget(m_chkSaveFavorite);

    m_tabs->addTab(tab, tr("🖼️ Choose Image"));
}

void PdfImageDialog::setupSavedTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(10);

    m_listSaved = new QListWidget(tab);
    m_listSaved->setViewMode(QListView::IconMode);
    m_listSaved->setIconSize(QSize(120, 80));
    m_listSaved->setGridSize(QSize(140, 110));
    m_listSaved->setSpacing(10);
    m_listSaved->setStyleSheet(
        "QListWidget { background-color: #F8FAFC; border: 1px solid #CBD5E1; border-radius: 8px; }"
        "QListWidget::item { background-color: #FFFFFF; color: #1E293B; border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px; }"
        "QListWidget::item:selected { border: 2px solid #0284C7; background-color: #E0F2FE; color: #0369A1; }"
    );
    connect(m_listSaved, &QListWidget::itemSelectionChanged, this, &PdfImageDialog::onSavedItemSelectionChanged);
    layout->addWidget(m_listSaved, 1);

    QHBoxLayout *actLayout = new QHBoxLayout();
    m_btnDeleteSaved = new QPushButton(tr("🗑️ Delete Selected"), tab);
    m_btnDeleteSaved->setEnabled(false);
    connect(m_btnDeleteSaved, &QPushButton::clicked, this, &PdfImageDialog::onDeleteSavedClicked);
    actLayout->addWidget(m_btnDeleteSaved);
    actLayout->addStretch(1);
    layout->addLayout(actLayout);

    m_tabs->addTab(tab, tr("⭐ Favorite Logos"));
}

void PdfImageDialog::onBrowseClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select Image / Logo"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.svg);;All Files (*)")
    );

    if (filePath.isEmpty()) return;

    QImage loaded(filePath);
    if (loaded.isNull()) return;

    m_rawImage = loaded.convertToFormat(QImage::Format_ARGB32);
    m_rotationAngle = 0;
    m_btnRotateLeft->setEnabled(true);
    m_btnRotateRight->setEnabled(true);

    processImage();
}

void PdfImageDialog::onRotateLeftClicked()
{
    m_rotationAngle = (m_rotationAngle + 270) % 360;
    processImage();
}

void PdfImageDialog::onRotateRightClicked()
{
    m_rotationAngle = (m_rotationAngle + 90) % 360;
    processImage();
}

void PdfImageDialog::processImage()
{
    if (m_rawImage.isNull()) {
        m_processedImage = QImage();
        m_btnOk->setEnabled(false);
        return;
    }

    if (m_rotationAngle != 0) {
        QTransform tr;
        tr.rotate(m_rotationAngle);
        m_processedImage = m_rawImage.transformed(tr, Qt::SmoothTransformation);
    } else {
        m_processedImage = m_rawImage;
    }

    QSize targetSize = m_lblPreview->size() - QSize(20, 20);
    if (targetSize.width() < 100) targetSize.setWidth(300);
    if (targetSize.height() < 100) targetSize.setHeight(180);

    QPixmap previewPix = QPixmap::fromImage(
        m_processedImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    m_lblPreview->setPixmap(previewPix);
    m_btnOk->setEnabled(true);
}

void PdfImageDialog::refreshSavedList()
{
    m_listSaved->clear();
    QDir dir(imagesDirPath());

    // Migrate legacy saved_images folder into unified logos folder if present
    QDir legacyDir(dir.filePath(QStringLiteral("../saved_images")));
    if (legacyDir.exists()) {
        for (const QFileInfo &fi : legacyDir.entryInfoList(QStringList() << QStringLiteral("*.png") << QStringLiteral("*.jpg") << QStringLiteral("*.webp"), QDir::Files)) {
            QString target = dir.filePath(fi.fileName());
            if (!QFile::exists(target)) {
                QFile::rename(fi.absoluteFilePath(), target);
            }
        }
    }

    QStringList files = dir.entryList(QStringList() << QStringLiteral("*.png") << QStringLiteral("*.jpg") << QStringLiteral("*.webp"), QDir::Files, QDir::Time);

    for (const QString &file : files) {
        QString fullPath = dir.filePath(file);
        QImage img(fullPath);
        if (!img.isNull()) {
            QIcon icon(QPixmap::fromImage(img.scaled(120, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            QFileInfo fi(fullPath);
            auto item = new QListWidgetItem(icon, fi.completeBaseName(), m_listSaved);
            item->setData(Qt::UserRole, fullPath);
        }
    }

    m_btnDeleteSaved->setEnabled(!m_listSaved->selectedItems().isEmpty());
}

void PdfImageDialog::onSavedItemSelectionChanged()
{
    auto items = m_listSaved->selectedItems();
    m_btnDeleteSaved->setEnabled(!items.isEmpty());

    if (!items.isEmpty()) {
        QString fullPath = items.first()->data(Qt::UserRole).toString();
        QImage img(fullPath);
        if (!img.isNull()) {
            m_rawImage = img.convertToFormat(QImage::Format_ARGB32);
            m_rotationAngle = 0;
            m_btnRotateLeft->setEnabled(true);
            m_btnRotateRight->setEnabled(true);
            processImage();
        }
    }
}

void PdfImageDialog::onDeleteSavedClicked()
{
    auto items = m_listSaved->selectedItems();
    if (items.isEmpty()) return;

    QString fullPath = items.first()->data(Qt::UserRole).toString();
    QFile::remove(fullPath);
    refreshSavedList();

    if (m_listSaved->selectedItems().isEmpty()) {
        m_rawImage = QImage();
        m_processedImage = QImage();
        m_rotationAngle = 0;
        m_btnRotateLeft->setEnabled(false);
        m_btnRotateRight->setEnabled(false);
        m_lblPreview->clear();
        m_lblPreview->setText(tr("No image loaded.<br>Click 'Browse Image...' below to select a picture or logo."));
        m_btnOk->setEnabled(false);
    }
}

void PdfImageDialog::accept()
{
    if (m_tabs->currentIndex() == 0 && m_chkSaveFavorite->isChecked() && !m_processedImage.isNull()) {
        QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
        QString filename = QStringLiteral("logo_%1.png").arg(ts);
        QString savePath = imagesDirPath() + QStringLiteral("/") + filename;
        m_processedImage.save(savePath, "PNG");
    }
    QDialog::accept();
}

QImage PdfImageDialog::selectedImage() const
{
    return m_processedImage;
}
