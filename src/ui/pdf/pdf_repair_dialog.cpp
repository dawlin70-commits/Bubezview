/**
 * @file pdf_repair_dialog.cpp
 * @brief Implementation of PDF recovery dialog using QPDF.
 */

#include "pdf_repair_dialog.h"
#include "package_manager_helper.h"
#include "build_config.h"
#include "fallback_notice_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QApplication>

PdfRepairDialog::PdfRepairDialog(const QString &initialFilePath, QWidget *parent)
    : QDialog(parent)
    , m_inputPath(initialFilePath)
    , m_editInput(nullptr)
    , m_btnBrowseInput(nullptr)
    , m_editOutput(nullptr)
    , m_btnBrowseOutput(nullptr)
    , m_lblStatus(nullptr)
    , m_progressBar(nullptr)
    , m_btnRepair(nullptr)
    , m_btnClose(nullptr)
{
    setWindowTitle(tr("Repair Corrupted PDF"));
    resize(580, 280);
    setupUi();

    if (!m_inputPath.isEmpty()) {
        m_editInput->setText(m_inputPath);
        QFileInfo fi(m_inputPath);
        QString defOut = fi.dir().filePath(fi.completeBaseName() + "_Repaired.pdf");
        m_editOutput->setText(defOut);
    }
}

void PdfRepairDialog::setupUi()
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(12);
    mainLay->setContentsMargins(18, 18, 18, 18);

    // Header
    auto *lblHeader = new QLabel(
        tr("<b>Restore & Repair PDF Document</b><br>"
           "<span style='color: #64748b; font-size: 11px;'>"
           "Reconstructs damaged cross-reference tables (xref), page dictionaries, and streams "
           "using the QPDF engine.</span>"),
        this
    );
    lblHeader->setWordWrap(true);
    mainLay->addWidget(lblHeader);

    // Input file row
    auto *inputLay = new QHBoxLayout();
    auto *lblIn = new QLabel(tr("Corrupted file:"), this);
    lblIn->setFixedWidth(110);
    m_editInput = new QLineEdit(this);
    m_editInput->setPlaceholderText(tr("Select the damaged PDF file..."));
    m_btnBrowseInput = new QPushButton(tr("Browse..."), this);
    m_btnBrowseInput->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowseInput, &QPushButton::clicked, this, &PdfRepairDialog::onBrowseInput);
    inputLay->addWidget(lblIn);
    inputLay->addWidget(m_editInput, 1);
    inputLay->addWidget(m_btnBrowseInput);
    mainLay->addLayout(inputLay);

    // Output file row
    auto *outputLay = new QHBoxLayout();
    auto *lblOut = new QLabel(tr("Save repaired to:"), this);
    lblOut->setFixedWidth(110);
    m_editOutput = new QLineEdit(this);
    m_editOutput->setPlaceholderText(tr("Target path for the recovered PDF file..."));
    m_btnBrowseOutput = new QPushButton(tr("Browse..."), this);
    m_btnBrowseOutput->setCursor(Qt::PointingHandCursor);
    connect(m_btnBrowseOutput, &QPushButton::clicked, this, &PdfRepairDialog::onBrowseOutput);
    outputLay->addWidget(lblOut);
    outputLay->addWidget(m_editOutput, 1);
    outputLay->addWidget(m_btnBrowseOutput);
    mainLay->addLayout(outputLay);

    // Status label
    m_lblStatus = new QLabel(this);
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet("font-size: 11px; color: #64748b; padding: 4px;");
    mainLay->addWidget(m_lblStatus);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    mainLay->addWidget(m_progressBar);

    mainLay->addStretch();

    // Buttons
    auto *btnLay = new QHBoxLayout();
    btnLay->addStretch();
    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);

    m_btnRepair = new QPushButton(tr("🛠️ Start Repair"), this);
    m_btnRepair->setCursor(Qt::PointingHandCursor);
    m_btnRepair->setStyleSheet(
        "QPushButton { font-weight: bold; background-color: #0284c7; color: white; "
        "padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnRepair, &QPushButton::clicked, this, &PdfRepairDialog::onStartRepair);

    btnLay->addWidget(m_btnClose);
    btnLay->addWidget(m_btnRepair);
    mainLay->addLayout(btnLay);
}

void PdfRepairDialog::onBrowseInput()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select PDF to Repair"),
        m_editInput->text().isEmpty() ? QDir::homePath() : QFileInfo(m_editInput->text()).absolutePath(),
        tr("PDF Files (*.pdf);;All Files (*.*)")
    );
    if (!file.isEmpty()) {
        m_editInput->setText(file);
        QFileInfo fi(file);
        m_editOutput->setText(fi.dir().filePath(fi.completeBaseName() + "_Repaired.pdf"));
    }
}

void PdfRepairDialog::onBrowseOutput()
{
    QString file = QFileDialog::getSaveFileName(
        this,
        tr("Save Repaired PDF"),
        m_editOutput->text().isEmpty() ? QDir::homePath() : m_editOutput->text(),
        tr("PDF Files (*.pdf)")
    );
    if (!file.isEmpty()) {
        if (!file.endsWith(".pdf", Qt::CaseInsensitive)) {
            file += ".pdf";
        }
        m_editOutput->setText(file);
    }
}

void PdfRepairDialog::onStartRepair()
{
    QString inPath = m_editInput->text().trimmed();
    QString outPath = m_editOutput->text().trimmed();

    if (inPath.isEmpty() || !QFile::exists(inPath)) {
        QMessageBox::warning(this, tr("Missing Input File"), tr("Please select an existing PDF file to repair."));
        return;
    }

    if (outPath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Destination"), tr("Please specify a target destination path for the repaired PDF."));
        return;
    }

    if (QFileInfo(inPath).absoluteFilePath() == QFileInfo(outPath).absoluteFilePath()) {
        QMessageBox::warning(
            this,
            tr("Invalid Target Path"),
            tr("The target file must be different from the source file to prevent data loss in case of corruption.")
        );
        return;
    }

    QString qpdfExe = BuildConfig::qpdfExecutable();
    if (qpdfExe.isEmpty()) {
        QMessageBox::StandardButton ask = QMessageBox::question(
            this,
            tr("QPDF Not Installed"),
            tr("The PDF recovery module requires the system utility <b>qpdf</b>.<br>Would you like to install it now?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (ask == QMessageBox::Yes) {
            if (PackageManagerHelper::installComponentInteractive("qpdf", this)) {
                qpdfExe = BuildConfig::qpdfExecutable();
            }
        }
        if (qpdfExe.isEmpty()) return;
    } else {
        FallbackNoticeDialog::checkAndPrompt("qpdf", tr("QPDF Engine"), qpdfExe, this);
        qpdfExe = BuildConfig::qpdfExecutable();
    }

    m_btnRepair->setEnabled(false);
    m_btnBrowseInput->setEnabled(false);
    m_btnBrowseOutput->setEnabled(false);
    m_progressBar->setVisible(true);
    m_lblStatus->setText(tr("Reconstructing xref tables and repairing streams..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QProcess proc;
    QStringList args;
    args << "--warning-exit-0" << inPath << outPath;
    proc.start(qpdfExe, args);
    proc.waitForFinished(-1);

    m_progressBar->setVisible(false);
    m_btnRepair->setEnabled(true);
    m_btnBrowseInput->setEnabled(true);
    m_btnBrowseOutput->setEnabled(true);

    int exitCode = proc.exitCode();
    QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    if ((exitCode == 0 || exitCode == 3) && QFile::exists(outPath) && QFileInfo(outPath).size() > 0) {
        m_repairedPath = outPath;
        m_lblStatus->setStyleSheet("font-size: 11px; color: #16a34a; font-weight: bold; padding: 4px;");
        m_lblStatus->setText(tr("🟢 PDF repaired successfully!"));
        emit fileRepaired(outPath);

        QMessageBox::StandardButton openNow = QMessageBox::question(
            this,
            tr("Repair Succeeded"),
            tr("<p>The PDF file has been successfully repaired and saved to:</p>"
               "<p><b>%1</b></p>"
               "<p>Would you like to open it now in BubezView?</p>").arg(outPath.toHtmlEscaped()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        if (openNow == QMessageBox::Yes) {
            accept();
        }
    } else {
        m_lblStatus->setStyleSheet("font-size: 11px; color: #dc2626; font-weight: bold; padding: 4px;");
        m_lblStatus->setText(tr("❌ Repair failed or document unrecoverable."));
        QMessageBox::critical(
            this,
            tr("Repair Failed"),
            tr("<p>QPDF was unable to reconstruct the structure of this document.</p>"
               "<p><i>Diagnostic detail:</i><br><code>%1</code></p>").arg(stdErr.isEmpty() ? tr("File severely truncated or unreadable.") : stdErr.toHtmlEscaped())
        );
    }
}
