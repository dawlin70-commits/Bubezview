#include "pdf_image_extract_dialog.h"
#include "build_config.h"
#include "fallback_notice_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>

PdfImageExtractDialog::PdfImageExtractDialog(const QString &pdfPath, QWidget *parent)
    : QDialog(parent)
    , m_pdfPath(pdfPath)
    , m_headerLabel(nullptr)
    , m_statsLabel(nullptr)
    , m_listWidget(nullptr)
    , m_btnSelectAll(nullptr)
    , m_btnDeselectAll(nullptr)
    , m_btnCopyClipboard(nullptr)
    , m_btnSaveAs(nullptr)
    , m_btnSaveSelected(nullptr)
    , m_btnClose(nullptr)
{
    setWindowTitle(tr("Extract Images from PDF — BubezView"));
    resize(720, 560);
    setupUi();
}

PdfImageExtractDialog::~PdfImageExtractDialog()
{
    // QTemporaryDir automatically cleans up temporary files on destruction
}

void PdfImageExtractDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 1. Header Banner
    auto *headerLayout = new QHBoxLayout();
    m_headerLabel = new QLabel(this);
    m_headerLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    m_headerLabel->setText(tr("Embedded Images in PDF"));
    headerLayout->addWidget(m_headerLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. Central Thumbnail Grid
    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setIconSize(QSize(120, 120));
    m_listWidget->setGridSize(QSize(150, 165));
    m_listWidget->setResizeMode(QListView::Adjust);
    m_listWidget->setMovement(QListView::Static);
    m_listWidget->setSpacing(8);
    m_listWidget->setWordWrap(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setStyleSheet(
        "QListWidget { border: 1px solid palette(mid); border-radius: 6px; padding: 6px; }"
        "QListWidget::item { border-radius: 4px; padding: 4px; }"
        "QListWidget::item:hover { background: palette(alternate-base); }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
    );

    connect(m_listWidget, &QListWidget::itemChanged, this, &PdfImageExtractDialog::onItemChanged);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &PdfImageExtractDialog::onItemSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &PdfImageExtractDialog::onItemDoubleClicked);

    mainLayout->addWidget(m_listWidget, 1);

    // 3. Selection Toolbar (Select All / Deselect All + Stats)
    auto *selectionLayout = new QHBoxLayout();
    m_btnSelectAll = new QPushButton(tr("✓ Select All"), this);
    connect(m_btnSelectAll, &QPushButton::clicked, this, &PdfImageExtractDialog::onSelectAll);
    selectionLayout->addWidget(m_btnSelectAll);

    m_btnDeselectAll = new QPushButton(tr("✗ Deselect All"), this);
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &PdfImageExtractDialog::onDeselectAll);
    selectionLayout->addWidget(m_btnDeselectAll);

    selectionLayout->addSpacing(16);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet("color: palette(window-text); opacity: 0.8;");
    selectionLayout->addWidget(m_statsLabel);

    selectionLayout->addStretch();
    mainLayout->addLayout(selectionLayout);

    // 4. Bottom Action Bar
    auto *actionLayout = new QHBoxLayout();

    m_btnCopyClipboard = new QPushButton(tr("📋 Copy to Clipboard"), this);
    m_btnCopyClipboard->setToolTip(tr("Copy the currently selected image directly to the system clipboard"));
    m_btnCopyClipboard->setEnabled(false);
    connect(m_btnCopyClipboard, &QPushButton::clicked, this, &PdfImageExtractDialog::onCopyCurrentToClipboard);
    actionLayout->addWidget(m_btnCopyClipboard);

    m_btnSaveAs = new QPushButton(tr("💾 Save Image As..."), this);
    m_btnSaveAs->setToolTip(tr("Export only the currently selected image with a custom name"));
    m_btnSaveAs->setEnabled(false);
    connect(m_btnSaveAs, &QPushButton::clicked, this, &PdfImageExtractDialog::onSaveCurrentAs);
    actionLayout->addWidget(m_btnSaveAs);

    actionLayout->addStretch();

    m_btnSaveSelected = new QPushButton(tr("📤 Save Selected..."), this);
    m_btnSaveSelected->setToolTip(tr("Export all checked images to a destination directory"));
    m_btnSaveSelected->setDefault(true);
    connect(m_btnSaveSelected, &QPushButton::clicked, this, &PdfImageExtractDialog::onSaveSelected);
    actionLayout->addWidget(m_btnSaveSelected);

    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    actionLayout->addWidget(m_btnClose);

    mainLayout->addLayout(actionLayout);
}

int PdfImageExtractDialog::loadImages()
{
    m_images.clear();
    m_listWidget->clear();

    if (m_pdfPath.isEmpty() || !QFile::exists(m_pdfPath)) {
        return -1;
    }

    // Verify pdfimages command availability (prioritizes locally built binary)
    QString pdfimagesExe = BuildConfig::pdfimagesExecutable();
    if (pdfimagesExe.isEmpty()) {
        return -2; // Indicates poppler-utils package missing
    }

    FallbackNoticeDialog::checkAndPrompt("pdfimages", tr("Poppler Image Extractor (pdfimages)"), pdfimagesExe, this);
    pdfimagesExe = BuildConfig::pdfimagesExecutable();

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        return -1;
    }

    // Run pdfimages -png to extract raw images in lossless PNG format
    QString prefix = QDir(m_tempDir->path()).filePath("img");
    QStringList args;
    args << "-png" << m_pdfPath << prefix;

    int exitCode = QProcess::execute(pdfimagesExe, args);
    if (exitCode != 0) {
        // Fallback without -png in case native format export is preferred
        args.clear();
        args << m_pdfPath << prefix;
        exitCode = QProcess::execute(pdfimagesExe, args);
    }

    if (exitCode != 0) {
        return -1;
    }

    // Scan temporary directory for extracted images
    QDir dir(m_tempDir->path());
    QStringList filters;
    filters << "img*.*";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    int index = 0;
    for (const QFileInfo &fi : fileList) {
        QImageReader reader(fi.absoluteFilePath());
        QSize sz = reader.size();
        if (sz.isEmpty()) {
            continue;
        }

        QImage img = reader.read();
        if (img.isNull()) {
            continue;
        }

        PdfExtractedImageInfo info;
        info.filePath = fi.absoluteFilePath();
        info.index = index;
        info.width = img.width();
        info.height = img.height();
        info.sizeBytes = fi.size();
        info.format = fi.suffix().toUpper();
        info.thumbnail = img.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        m_images.append(info);

        auto *item = new QListWidgetItem();
        item->setIcon(QIcon(QPixmap::fromImage(info.thumbnail)));
        item->setText(tr("Image #%1\n%2×%3\n%4")
                          .arg(index + 1)
                          .arg(info.width)
                          .arg(info.height)
                          .arg(formatFileSize(info.sizeBytes)));
        item->setToolTip(tr("Image #%1\nFile: %2\nResolution: %3 × %4 px\nFormat: %5\nSize: %6")
                             .arg(index + 1)
                             .arg(fi.fileName())
                             .arg(info.width)
                             .arg(info.height)
                             .arg(info.format)
                             .arg(formatFileSize(info.sizeBytes)));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, index);

        m_listWidget->addItem(item);
        index++;
    }

    m_headerLabel->setText(tr("Embedded Images Found (%1)").arg(m_images.size()));
    updateSelectionStats();

    if (!m_images.isEmpty()) {
        m_listWidget->setCurrentRow(0);
    }

    return m_images.size();
}

void PdfImageExtractDialog::onSelectAll()
{
    m_listWidget->blockSignals(true);
    for (int i = 0; i < m_listWidget->count(); ++i) {
        m_listWidget->item(i)->setCheckState(Qt::Checked);
    }
    m_listWidget->blockSignals(false);
    updateSelectionStats();
}

void PdfImageExtractDialog::onDeselectAll()
{
    m_listWidget->blockSignals(true);
    for (int i = 0; i < m_listWidget->count(); ++i) {
        m_listWidget->item(i)->setCheckState(Qt::Unchecked);
    }
    m_listWidget->blockSignals(false);
    updateSelectionStats();
}

void PdfImageExtractDialog::onItemChanged(QListWidgetItem * /*item*/)
{
    updateSelectionStats();
}

void PdfImageExtractDialog::onItemSelectionChanged()
{
    bool hasSelection = (m_listWidget->currentItem() != nullptr);
    m_btnCopyClipboard->setEnabled(hasSelection);
    m_btnSaveAs->setEnabled(hasSelection);
}

void PdfImageExtractDialog::updateSelectionStats()
{
    int checkedCount = 0;
    qint64 totalBytes = 0;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            checkedCount++;
            int idx = item->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < m_images.size()) {
                totalBytes += m_images[idx].sizeBytes;
            }
        }
    }

    m_statsLabel->setText(tr("Selected: %1 of %2 (%3)")
                              .arg(checkedCount)
                              .arg(m_images.size())
                              .arg(formatFileSize(totalBytes)));

    m_btnSaveSelected->setEnabled(checkedCount > 0);
    m_btnSaveSelected->setText(tr("📤 Save Selected (%1)...").arg(checkedCount));
}

void PdfImageExtractDialog::onCopyCurrentToClipboard()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_images.size()) return;

    QImage fullImg(m_images[idx].filePath);
    if (!fullImg.isNull()) {
        QGuiApplication::clipboard()->setImage(fullImg);

        QString origText = m_btnCopyClipboard->text();
        m_btnCopyClipboard->setText(tr("✓ Copied!"));
        QTimer::singleShot(1500, this, [this, origText]() {
            if (m_btnCopyClipboard) m_btnCopyClipboard->setText(origText);
        });
    }
}

void PdfImageExtractDialog::onSaveCurrentAs()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_images.size()) return;

    const PdfExtractedImageInfo &info = m_images[idx];
    QFileInfo pdfFi(m_pdfPath);
    QString ext = QFileInfo(info.filePath).suffix().toLower();
    if (ext.isEmpty()) ext = "png";

    QString defaultName = pdfFi.dir().filePath(QString("%1_img%2.%3")
                                                   .arg(pdfFi.baseName())
                                                   .arg(idx + 1)
                                                   .arg(ext));

    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp);;All Files (*)");
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save Image As"), defaultName, filter);
    if (savePath.isEmpty()) return;

    bool ok = false;
    QString targetExt = QFileInfo(savePath).suffix().toLower();
    if (targetExt == ext) {
        if (QFile::exists(savePath)) QFile::remove(savePath);
        ok = QFile::copy(info.filePath, savePath);
    } else {
        QImage fullImg(info.filePath);
        ok = fullImg.save(savePath);
    }

    if (ok) {
        QMessageBox::information(this, tr("Image Saved"), tr("Image successfully saved to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, tr("Save Error"), tr("Failed to save image to:\n%1").arg(savePath));
    }
}

void PdfImageExtractDialog::onSaveSelected()
{
    QList<int> selectedIndices;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->checkState() == Qt::Checked) {
            int idx = m_listWidget->item(i)->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < m_images.size()) {
                selectedIndices.append(idx);
            }
        }
    }

    if (selectedIndices.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select at least one image to export."));
        return;
    }

    QFileInfo pdfFi(m_pdfPath);
    QString defaultDir = pdfFi.dir().filePath(pdfFi.baseName() + "_images");
    QString targetDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Destination Directory for Extracted Images"),
        defaultDir
    );

    if (targetDir.isEmpty()) return;

    QDir().mkpath(targetDir);
    int savedCount = 0;

    for (int idx : selectedIndices) {
        const PdfExtractedImageInfo &info = m_images[idx];
        QFileInfo srcFi(info.filePath);
        QString destName = QString("%1_img%2.%3")
                               .arg(pdfFi.baseName())
                               .arg(idx + 1)
                               .arg(srcFi.suffix().toLower());
        QString destPath = QDir(targetDir).filePath(destName);

        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }

        if (QFile::copy(info.filePath, destPath)) {
            savedCount++;
        }
    }

    if (savedCount > 0) {
        QMessageBox::information(
            this,
            tr("Export Complete"),
            tr("Successfully exported %1 image(s) to:\n%2").arg(savedCount).arg(targetDir)
        );
        accept();
    } else {
        QMessageBox::warning(
            this,
            tr("Export Failed"),
            tr("Could not write images to the selected directory.")
        );
    }
}

void PdfImageExtractDialog::onItemDoubleClicked(QListWidgetItem * /*item*/)
{
    onSaveCurrentAs();
}

QString PdfImageExtractDialog::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}
