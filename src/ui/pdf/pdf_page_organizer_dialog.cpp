/**
 * @file pdf_page_organizer_dialog.cpp
 * @brief Implementation of visual PDF page organizer and manipulator.
 */

#include "pdf_page_organizer_dialog.h"
#include "package_manager_helper.h"
#include "build_config.h"
#include "fallback_notice_dialog.h"
#include <poppler/qt6/poppler-qt6.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QApplication>
#include <QTransform>
#include <QPainter>

namespace {
    constexpr int ROLE_ORIGINAL_PAGE = Qt::UserRole + 1;
    constexpr int ROLE_ROTATION_DEG  = Qt::UserRole + 2;
    constexpr int ROLE_BASE_IMAGE    = Qt::UserRole + 3;
}

PdfPageOrganizerDialog::PdfPageOrganizerDialog(const QString &pdfPath, QWidget *parent)
    : QDialog(parent)
    , m_pdfPath(pdfPath)
    , m_lblHeader(nullptr)
    , m_lblStats(nullptr)
    , m_listWidget(nullptr)
    , m_btnRotateLeft(nullptr)
    , m_btnRotateRight(nullptr)
    , m_btnDelete(nullptr)
    , m_btnReverse(nullptr)
    , m_btnReset(nullptr)
    , m_btnSaveAs(nullptr)
    , m_btnSaveOverwrite(nullptr)
    , m_btnClose(nullptr)
    , m_progressBar(nullptr)
{
    setWindowTitle(tr("PDF Page Organizer & Reorderer"));
    resize(920, 640);
    setMinimumSize(680, 480);

    setupUi();
    loadThumbnails();
    updateStatsLabel();
}

PdfPageOrganizerDialog::~PdfPageOrganizerDialog() = default;

void PdfPageOrganizerDialog::setupUi()
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(10);
    mainLay->setContentsMargins(14, 14, 14, 14);

    // Top instruction & header
    m_lblHeader = new QLabel(
        tr("<b>PDF Page Manager</b> — Drag and drop thumbnails to reorder pages. "
           "Use the toolbar to rotate, remove, or reverse pages losslessly via QPDF."),
        this
    );
    m_lblHeader->setWordWrap(true);
    m_lblHeader->setStyleSheet("font-size: 12px; padding: 2px 0;");
    mainLay->addWidget(m_lblHeader);

    // Toolbar for actions
    auto *toolsLay = new QHBoxLayout();
    toolsLay->setSpacing(8);

    m_btnRotateLeft = new QPushButton(tr("↶ Rotate Left"), this);
    m_btnRotateLeft->setToolTip(tr("Rotate selected pages 90 degrees counter-clockwise"));
    m_btnRotateLeft->setCursor(Qt::PointingHandCursor);
    connect(m_btnRotateLeft, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onRotateLeft);

    m_btnRotateRight = new QPushButton(tr("↷ Rotate Right"), this);
    m_btnRotateRight->setToolTip(tr("Rotate selected pages 90 degrees clockwise"));
    m_btnRotateRight->setCursor(Qt::PointingHandCursor);
    connect(m_btnRotateRight, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onRotateRight);

    m_btnDelete = new QPushButton(tr("🗑️ Remove"), this);
    m_btnDelete->setToolTip(tr("Remove selected pages from the document"));
    m_btnDelete->setCursor(Qt::PointingHandCursor);
    connect(m_btnDelete, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onDeleteSelected);

    m_btnReverse = new QPushButton(tr("⇄ Reverse Order"), this);
    m_btnReverse->setToolTip(tr("Invert the overall page sequence"));
    m_btnReverse->setCursor(Qt::PointingHandCursor);
    connect(m_btnReverse, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onReverseOrder);

    m_btnReset = new QPushButton(tr("↺ Reset"), this);
    m_btnReset->setToolTip(tr("Reset all pages and rotations to their original state"));
    m_btnReset->setCursor(Qt::PointingHandCursor);
    connect(m_btnReset, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onResetOrder);

    toolsLay->addWidget(m_btnRotateLeft);
    toolsLay->addWidget(m_btnRotateRight);
    toolsLay->addWidget(m_btnDelete);
    toolsLay->addSpacing(12);
    toolsLay->addWidget(m_btnReverse);
    toolsLay->addWidget(m_btnReset);
    toolsLay->addStretch();

    mainLay->addLayout(toolsLay);

    // Grid list widget for pages
    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setIconSize(QSize(120, 160));
    m_listWidget->setGridSize(QSize(150, 205));
    m_listWidget->setSpacing(10);
    m_listWidget->setMovement(QListView::Snap);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setWrapping(true);
    m_listWidget->setWordWrap(true);

    // Native theme-friendly styling that uses Qt palette roles
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: palette(base);"
        "  color: palette(text);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
        "QListWidget::item {"
        "  background-color: palette(alternate-base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "  margin: 4px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 2px solid palette(highlight);"
        "}"
        "QListWidget::item:hover:!selected {"
        "  border: 1px solid palette(highlight);"
        "}"
    );

    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &PdfPageOrganizerDialog::onSelectionChanged);
    mainLay->addWidget(m_listWidget, 1);

    // Progress bar for long renders or processing
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    mainLay->addWidget(m_progressBar);

    // Bottom bar with stats & buttons
    auto *bottomLay = new QHBoxLayout();
    bottomLay->setSpacing(10);

    m_lblStats = new QLabel(this);
    m_lblStats->setStyleSheet("font-size: 11px; color: #64748b; font-weight: 500;");
    bottomLay->addWidget(m_lblStats, 1);

    m_btnClose = new QPushButton(tr("Cancel"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);

    m_btnSaveAs = new QPushButton(tr("Save As..."), this);
    m_btnSaveAs->setCursor(Qt::PointingHandCursor);
    connect(m_btnSaveAs, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onSaveAs);

    m_btnSaveOverwrite = new QPushButton(tr("Save (Overwrite)"), this);
    m_btnSaveOverwrite->setCursor(Qt::PointingHandCursor);
    m_btnSaveOverwrite->setStyleSheet(
        "QPushButton { font-weight: bold; background-color: #0284c7; color: white; "
        "padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnSaveOverwrite, &QPushButton::clicked, this, &PdfPageOrganizerDialog::onSaveOverwrite);

    bottomLay->addWidget(m_btnClose);
    bottomLay->addWidget(m_btnSaveAs);
    bottomLay->addWidget(m_btnSaveOverwrite);

    mainLay->addLayout(bottomLay);

    onSelectionChanged();
}

void PdfPageOrganizerDialog::loadThumbnails()
{
    m_listWidget->clear();

    if (m_pdfPath.isEmpty() || !QFile::exists(m_pdfPath)) {
        return;
    }

    m_popplerDoc = std::unique_ptr<Poppler::Document>(Poppler::Document::load(m_pdfPath));
    if (!m_popplerDoc || m_popplerDoc->isLocked()) {
        QMessageBox::critical(this, tr("Load Error"), tr("Failed to open PDF document or file is encrypted."));
        return;
    }

    m_popplerDoc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_popplerDoc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    int count = m_popplerDoc->numPages();
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, count);

    for (int i = 0; i < count; ++i) {
        m_progressBar->setValue(i + 1);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        std::unique_ptr<Poppler::Page> page(m_popplerDoc->page(i));
        if (!page) continue;

        // Render preview image at 72 DPI
        QImage img = page->renderToImage(72.0, 72.0);
        if (img.isNull()) continue;

        auto *item = new QListWidgetItem(m_listWidget);
        item->setData(ROLE_ORIGINAL_PAGE, i + 1);
        item->setData(ROLE_ROTATION_DEG, 0);
        item->setData(ROLE_BASE_IMAGE, QVariant::fromValue(img));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);

        updateItemVisuals(item);
    }

    m_progressBar->setVisible(false);
    updateStatsLabel();
}

void PdfPageOrganizerDialog::updateItemVisuals(QListWidgetItem *item)
{
    if (!item) return;

    int origPage = item->data(ROLE_ORIGINAL_PAGE).toInt();
    int rot = item->data(ROLE_ROTATION_DEG).toInt();
    QImage baseImg = item->data(ROLE_BASE_IMAGE).value<QImage>();

    QImage displayImg = baseImg;
    if (rot != 0) {
        QTransform t;
        t.rotate(rot);
        displayImg = baseImg.transformed(t, Qt::SmoothTransformation);
    }

    // Scale preview thumbnail smoothly
    QPixmap pix = QPixmap::fromImage(
        displayImg.scaled(120, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );

    item->setIcon(QIcon(pix));
    if (rot == 0) {
        item->setText(tr("Page %1").arg(origPage));
    } else {
        item->setText(tr("Page %1 (%2°)").arg(origPage).arg(rot));
    }
}

void PdfPageOrganizerDialog::updateStatsLabel()
{
    int total = m_listWidget->count();
    int selected = m_listWidget->selectedItems().count();

    m_lblStats->setText(
        tr("Total pages: %1 | Selected: %2").arg(total).arg(selected)
    );

    bool hasPages = total > 0;
    bool hasSelection = selected > 0;

    m_btnRotateLeft->setEnabled(hasSelection);
    m_btnRotateRight->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection && total > 1);
    m_btnReverse->setEnabled(total > 1);
    m_btnSaveAs->setEnabled(hasPages);
    m_btnSaveOverwrite->setEnabled(hasPages);
}

void PdfPageOrganizerDialog::onSelectionChanged()
{
    updateStatsLabel();
}

void PdfPageOrganizerDialog::onRotateLeft()
{
    const auto items = m_listWidget->selectedItems();
    for (auto *item : items) {
        int currentRot = item->data(ROLE_ROTATION_DEG).toInt();
        int newRot = (currentRot + 270) % 360;
        item->setData(ROLE_ROTATION_DEG, newRot);
        updateItemVisuals(item);
    }
}

void PdfPageOrganizerDialog::onRotateRight()
{
    const auto items = m_listWidget->selectedItems();
    for (auto *item : items) {
        int currentRot = item->data(ROLE_ROTATION_DEG).toInt();
        int newRot = (currentRot + 90) % 360;
        item->setData(ROLE_ROTATION_DEG, newRot);
        updateItemVisuals(item);
    }
}

void PdfPageOrganizerDialog::onDeleteSelected()
{
    const auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    if (items.count() >= m_listWidget->count()) {
        QMessageBox::warning(
            this,
            tr("Action Denied"),
            tr("A PDF document must contain at least one page. You cannot delete all pages.")
        );
        return;
    }

    for (auto *item : items) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
    }

    updateStatsLabel();
}

void PdfPageOrganizerDialog::onReverseOrder()
{
    int count = m_listWidget->count();
    if (count <= 1) return;

    QList<QListWidgetItem*> items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        items.append(m_listWidget->takeItem(0));
    }

    for (int i = items.count() - 1; i >= 0; --i) {
        m_listWidget->addItem(items.at(i));
    }

    updateStatsLabel();
}

void PdfPageOrganizerDialog::onResetOrder()
{
    QMessageBox::StandardButton ask = QMessageBox::question(
        this,
        tr("Reset Changes"),
        tr("Are you sure you want to revert all changes, reordering, and rotations?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (ask == QMessageBox::Yes) {
        loadThumbnails();
    }
}

void PdfPageOrganizerDialog::onSaveAs()
{
    QFileInfo fi(m_pdfPath);
    QString suggested = fi.dir().filePath(fi.completeBaseName() + "_Organized.pdf");

    QString destination = QFileDialog::getSaveFileName(
        this,
        tr("Save Organized PDF As"),
        suggested,
        tr("PDF Files (*.pdf)")
    );

    if (destination.isEmpty()) return;

    if (!destination.endsWith(".pdf", Qt::CaseInsensitive)) {
        destination += ".pdf";
    }

    const bool isSameAsSource = (QFileInfo(destination).absoluteFilePath() == fi.absoluteFilePath());
    const QString writeTarget = isSameAsSource
        ? fi.dir().filePath(QString(".bubez_tmp_%1.pdf").arg(QDateTime::currentMSecsSinceEpoch()))
        : destination;

    if (executeQpdfReorder(writeTarget)) {
        if (isSameAsSource) {
            m_popplerDoc.reset();
            QFile::remove(destination);
            if (!QFile::rename(writeTarget, destination)) {
                QMessageBox::critical(
                    this,
                    tr("File Replacement Error"),
                    tr("Failed to replace original file. New document is saved as:<br><b>%1</b>").arg(writeTarget.toHtmlEscaped())
                );
                return;
            }
        }
        m_outPath = destination;
        emit fileOrganized(destination);

        QMessageBox::information(
            this,
            tr("Save Succeeded"),
            tr("Organized PDF document saved successfully to:<br><b>%1</b>").arg(destination.toHtmlEscaped())
        );
        accept();
    } else if (isSameAsSource) {
        QFile::remove(writeTarget);
    }
}

void PdfPageOrganizerDialog::onSaveOverwrite()
{
    QMessageBox::StandardButton ask = QMessageBox::warning(
        this,
        tr("Confirm Overwrite"),
        tr("<p>Are you sure you want to overwrite the original file?</p>"
           "<p><b>%1</b></p>"
           "<p>This action will replace the original page order permanently.</p>").arg(m_pdfPath.toHtmlEscaped()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ask != QMessageBox::Yes) return;

    // Use a temporary file in the same directory to perform safe atomic replacement
    QFileInfo fi(m_pdfPath);
    QString tmpPath = fi.dir().filePath(QString(".bubez_tmp_%1.pdf").arg(QDateTime::currentMSecsSinceEpoch()));

    if (executeQpdfReorder(tmpPath)) {
        // Close our internal poppler doc handle before overwriting
        m_popplerDoc.reset();

        QFile::remove(m_pdfPath);
        if (QFile::rename(tmpPath, m_pdfPath)) {
            m_outPath = m_pdfPath;
            emit fileOrganized(m_pdfPath);

            QMessageBox::information(
                this,
                tr("Changes Saved"),
                tr("The original PDF has been overwritten with the new page configuration.")
            );
            accept();
        } else {
            QMessageBox::critical(
                this,
                tr("File Replacement Error"),
                tr("Failed to replace original file. New document is saved as:<br><b>%1</b>").arg(tmpPath.toHtmlEscaped())
            );
        }
    } else {
        QFile::remove(tmpPath);
    }
}

bool PdfPageOrganizerDialog::executeQpdfReorder(const QString &destinationPath)
{
    int count = m_listWidget->count();
    if (count == 0) {
        QMessageBox::warning(this, tr("Empty Document"), tr("Cannot save a PDF with no pages."));
        return false;
    }

    QString qpdfExe = BuildConfig::qpdfExecutable();
    if (qpdfExe.isEmpty()) {
        QMessageBox::StandardButton ask = QMessageBox::question(
            this,
            tr("QPDF Not Installed"),
            tr("Page reorganization requires the system utility <b>qpdf</b>.<br>Would you like to install it now?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (ask == QMessageBox::Yes) {
            if (PackageManagerHelper::installComponentInteractive("qpdf", this)) {
                qpdfExe = BuildConfig::qpdfExecutable();
            }
        }
        if (qpdfExe.isEmpty()) return false;
    } else {
        FallbackNoticeDialog::checkAndPrompt("qpdf", tr("QPDF Engine"), qpdfExe, this);
        qpdfExe = BuildConfig::qpdfExecutable();
    }

    // Build page range specification
    QStringList pageNumbers;
    pageNumbers.reserve(count);
    QStringList rotateArgs;

    for (int i = 0; i < count; ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        int origPage = item->data(ROLE_ORIGINAL_PAGE).toInt();
        int rot = item->data(ROLE_ROTATION_DEG).toInt();

        pageNumbers.append(QString::number(origPage));

        if (rot != 0) {
            // --rotate=+<rot>:<out_page_number>
            rotateArgs.append(QString("--rotate=+%1:%2").arg(rot).arg(i + 1));
        }
    }

    QString pagesSpec = pageNumbers.join(",");

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QProcess proc;
    QStringList args;
    args << m_pdfPath << "--pages" << "." << pagesSpec << "--";
    args.append(rotateArgs);
    args << destinationPath;

    proc.start(qpdfExe, args);
    proc.waitForFinished(-1);

    m_progressBar->setVisible(false);

    int exitCode = proc.exitCode();
    QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    if ((exitCode == 0 || exitCode == 3) && QFile::exists(destinationPath) && QFileInfo(destinationPath).size() > 0) {
        return true;
    }

    QMessageBox::critical(
        this,
        tr("QPDF Processing Error"),
        tr("<p>Failed to generate the organized PDF.</p>"
           "<p><i>Details:</i><br><code>%1</code></p>").arg(stdErr.isEmpty() ? tr("Unknown error") : stdErr.toHtmlEscaped())
    );
    return false;
}
