#include "unified_export_dialog.h"
#include "build_config.h"
#include "core/services/theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QIcon>

UnifiedExportDialog::UnifiedExportDialog(FileCategory category,
                                         const QString &sourcePath,
                                         int currentPage,
                                         int totalPages,
                                         QWidget *parent)
    : FramelessDialog(parent)
    , m_category(category)
    , m_sourcePath(sourcePath)
    , m_currentPage(currentPage)
    , m_totalPages(totalPages > 0 ? totalPages : 1)
{
    setWindowTitle(tr("Export Document"));
    resize(480, 260);
    setModal(true);

    setupUi();
    updateDefaultTargetPath();
}

void UnifiedExportDialog::setupUi()
{
    bool isDark = ThemeManager::isDarkTheme();
    QString textCol = isDark ? QStringLiteral("#f1f5f9") : QStringLiteral("#0f172a");
    QString inputBg = isDark ? QStringLiteral("rgba(15, 20, 32, 0.85)") : QStringLiteral("#ffffff");
    QString btnBg = isDark
        ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(38, 48, 70, 0.90), stop:1 rgba(26, 33, 50, 0.95))")
        : QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f1f5f9)");
    QString borderCol = isDark ? QStringLiteral("rgba(255, 255, 255, 0.14)") : QStringLiteral("rgba(15, 23, 42, 0.15)");

    setStyleSheet(QString(
        "QLabel, QRadioButton { color: %1; background: transparent; font-size: 12px; }"
        "QLineEdit { background: %2; color: %1; border: 1px solid %4; border-radius: 10px; padding: 6px 10px; }"
        "QComboBox { background: %3; color: %1; border: 1px solid %4; border-radius: 10px; padding: 5px 24px 5px 10px; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QPushButton { background: %3; color: %1; border: 1px solid %4; border-radius: 10px; padding: 7px 16px; font-weight: 600; }"
        "QPushButton:hover { border-color: #38bdf8; }"
    ).arg(textCol, inputBg, btnBg, borderCol));

    auto *mainLayout = contentLayout();
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(14);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(12);

    // 1. Format selector
    m_comboFormat = new QComboBox(this);
    if (m_category == FileCategory::Pdf) {
        m_comboFormat->addItem(tr("PDF Document (*.pdf)"), QStringLiteral("pdf"));
        m_comboFormat->addItem(tr("PNG Image (*.png)"), QStringLiteral("png"));
        m_comboFormat->addItem(tr("JPEG Image (*.jpg)"), QStringLiteral("jpg"));
        m_comboFormat->addItem(tr("WebP Image (*.webp)"), QStringLiteral("webp"));
        m_comboFormat->addItem(tr("Plain Text (*.txt)"), QStringLiteral("txt"));
    } else if (m_category == FileCategory::Image) {
        m_comboFormat->addItem(tr("PNG Image (*.png)"), QStringLiteral("png"));
        m_comboFormat->addItem(tr("JPEG Image (*.jpg)"), QStringLiteral("jpg"));
        m_comboFormat->addItem(tr("WebP Image (*.webp)"), QStringLiteral("webp"));
        m_comboFormat->addItem(tr("PDF Document (*.pdf)"), QStringLiteral("pdf"));
        m_comboFormat->addItem(tr("BMP Image (*.bmp)"), QStringLiteral("bmp"));
        m_comboFormat->addItem(tr("TIFF Image (*.tiff)"), QStringLiteral("tiff"));
    } else if (m_category == FileCategory::Text) {
        m_comboFormat->addItem(tr("Plain Text (*.txt)"), QStringLiteral("txt"));
        m_comboFormat->addItem(tr("PDF Document (*.pdf)"), QStringLiteral("pdf"));
        m_comboFormat->addItem(tr("Markdown Document (*.md)"), QStringLiteral("md"));
        m_comboFormat->addItem(tr("HTML Document (*.html)"), QStringLiteral("html"));
    } else {
        m_comboFormat->addItem(tr("PDF Document (*.pdf)"), QStringLiteral("pdf"));
        m_comboFormat->addItem(tr("Plain Text (*.txt)"), QStringLiteral("txt"));
    }
    connect(m_comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UnifiedExportDialog::onFormatChanged);
    formLayout->addRow(tr("Format:"), m_comboFormat);

    // 2. Page Scope (only for multi-page documents like PDF)
    m_scopeContainer = new QWidget(this);
    auto *scopeLayout = new QHBoxLayout(m_scopeContainer);
    scopeLayout->setContentsMargins(0, 0, 0, 0);
    scopeLayout->setSpacing(16);

    m_radioCurrentPage = new QRadioButton(tr("Current Page (Page %1)").arg(m_currentPage + 1), m_scopeContainer);
    m_radioAllPages = new QRadioButton(tr("All Pages (%1)").arg(m_totalPages), m_scopeContainer);

    m_scopeGroup = new QButtonGroup(this);
    m_scopeGroup->addButton(m_radioCurrentPage, 0);
    m_scopeGroup->addButton(m_radioAllPages, 1);
    m_radioCurrentPage->setChecked(true);

    connect(m_radioCurrentPage, &QRadioButton::toggled, this, &UnifiedExportDialog::onScopeChanged);

    scopeLayout->addWidget(m_radioCurrentPage);
    scopeLayout->addWidget(m_radioAllPages);
    scopeLayout->addStretch(1);

    if (m_category == FileCategory::Pdf && m_totalPages > 1) {
        formLayout->addRow(tr("Scope:"), m_scopeContainer);
    } else {
        m_scopeContainer->hide();
    }

    // 3. Destination File Path
    auto *pathContainer = new QWidget(this);
    auto *pathLayout = new QHBoxLayout(pathContainer);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(6);

    m_editPath = new QLineEdit(pathContainer);
    m_btnBrowse = new QPushButton(tr("Browse..."), pathContainer);
    m_btnBrowse->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowse, &QPushButton::clicked, this, &UnifiedExportDialog::onBrowseClicked);

    pathLayout->addWidget(m_editPath, 1);
    pathLayout->addWidget(m_btnBrowse, 0);
    formLayout->addRow(tr("Destination:"), pathContainer);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch(1);

    // 4. Action Buttons
    auto *btnRow = new QHBoxLayout();

    if (m_category == FileCategory::Pdf && BuildConfig::isPdfimagesAvailable()) {
        m_btnExtractImages = new QPushButton(QIcon::fromTheme("image-x-generic"), tr("Extract Embedded Images..."), this);
        m_btnExtractImages->setCursor(Qt::PointingHandCursor);
        m_btnExtractImages->setToolTip(tr("Open embedded images preview and selection dialog"));
        m_btnExtractImages->setStyleSheet(QStringLiteral(
            "QPushButton { padding: 7px 14px; border-radius: 10px; }"
        ));
        connect(m_btnExtractImages, &QPushButton::clicked, this, [this]() {
            done(ExtractImages);
        });
        btnRow->addWidget(m_btnExtractImages);
    }

    btnRow->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_btnCancel);

    m_btnExport = new QPushButton(tr("Export"), this);
    m_btnExport->setCursor(Qt::PointingHandCursor);
    m_btnExport->setDefault(true);
    m_btnExport->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(125, 211, 252, 0.55);"
        "  font-weight: bold;"
        "  padding: 7px 18px;"
        "  border-radius: 10px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6);"
        "}"
    ));
    connect(m_btnExport, &QPushButton::clicked, this, [this]() {
        if (m_editPath->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Missing Path"), tr("Please select a destination file path."));
            return;
        }
        accept();
    });
    btnRow->addWidget(m_btnExport);

    mainLayout->addLayout(btnRow);
}

QString UnifiedExportDialog::selectedFormat() const
{
    return m_comboFormat ? m_comboFormat->currentData().toString() : QStringLiteral("pdf");
}

bool UnifiedExportDialog::isAllPages() const
{
    if (!m_scopeContainer || !m_scopeContainer->isVisible()) return false;
    return m_radioAllPages && m_radioAllPages->isChecked();
}

QString UnifiedExportDialog::targetPath() const
{
    return m_editPath ? m_editPath->text().trimmed() : QString();
}

void UnifiedExportDialog::onFormatChanged()
{
    updateDefaultTargetPath();
}

void UnifiedExportDialog::onScopeChanged()
{
    updateDefaultTargetPath();
}

void UnifiedExportDialog::updateDefaultTargetPath()
{
    QFileInfo fi(m_sourcePath);
    QString baseDir = fi.dir().absolutePath();
    QString baseStem = fi.completeBaseName();
    QString ext = selectedFormat();

    QString fileName;
    if (m_category == FileCategory::Pdf && m_totalPages > 1) {
        if (isAllPages()) {
            if (ext == QStringLiteral("pdf")) {
                fileName = QStringLiteral("%1_copy.pdf").arg(baseStem);
            } else if (ext == QStringLiteral("txt")) {
                fileName = QStringLiteral("%1_full.txt").arg(baseStem);
            } else {
                fileName = QStringLiteral("%1_page_%02d.%2").arg(baseStem, ext);
            }
        } else {
            fileName = QStringLiteral("%1_p%2.%3").arg(baseStem).arg(m_currentPage + 1).arg(ext);
        }
    } else {
        fileName = QStringLiteral("%1_exported.%2").arg(baseStem, ext);
    }

    if (m_editPath) {
        m_editPath->setText(QDir(baseDir).filePath(fileName));
    }
}

void UnifiedExportDialog::onBrowseClicked()
{
    QString ext = selectedFormat();
    QString filter;
    if (ext == QStringLiteral("pdf")) filter = tr("PDF Document (*.pdf)");
    else if (ext == QStringLiteral("png")) filter = tr("PNG Image (*.png)");
    else if (ext == QStringLiteral("jpg")) filter = tr("JPEG Image (*.jpg *.jpeg)");
    else if (ext == QStringLiteral("webp")) filter = tr("WebP Image (*.webp)");
    else if (ext == QStringLiteral("txt")) filter = tr("Plain Text (*.txt)");
    else filter = tr("All Files (*)");

    QString chosen = QFileDialog::getSaveFileName(this, tr("Select Destination"), m_editPath->text(), filter);
    if (!chosen.isEmpty()) {
        if (!chosen.contains(QLatin1Char('.'))) {
            chosen += QStringLiteral(".") + ext;
        }
        m_editPath->setText(chosen);
    }
}
