/**
 * @file build_options_dialog.cpp
 * @brief Implementation of interactive compilation and module configuration dialog.
 */

#include "build_options_dialog.h"
#include "update_manager.h"
#include "package_manager_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QMessageBox>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

BuildOptionsDialog::BuildOptionsDialog(QWidget *parent)
    : QDialog(parent)
    , m_radioCpuNative(nullptr)
    , m_radioCpuStandard(nullptr)
    , m_chkStaticPdf(nullptr)
    , m_pdfVersionContainer(nullptr)
    , m_comboPdfVersion(nullptr)
    , m_chkStaticArchive(nullptr)
    , m_chkInternalOffice(nullptr)
    , m_lblPdfStatus(nullptr)
    , m_lblArchiveStatus(nullptr)
    , m_lblOfficeStatus(nullptr)
    , m_progressBar(nullptr)
    , m_lblBuildStatus(nullptr)
    , m_txtLog(nullptr)
    , m_btnSave(nullptr)
    , m_btnRecompile(nullptr)
    , m_btnClose(nullptr)
    , m_process(nullptr)
{
    setWindowTitle(tr("Compilation Profiles & Module Manager"));
    setMinimumWidth(580);
    setupUi();
    loadFromConfig();
}

BuildOptionsDialog::~BuildOptionsDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BuildOptionsDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    QLabel *lblHeader = new QLabel(this);
    lblHeader->setText(QString("<h3>%1</h3><p>%2</p>")
        .arg(tr("🛠️ BubezView Engine & Compilation Manager"))
        .arg(tr("Customize processor instruction tuning, engine bundling, and optional functional modules.")));
    mainLayout->addWidget(lblHeader);

    // 1. CPU Optimization Group
    QGroupBox *grpCpu = new QGroupBox(tr("Processor (CPU) Instruction Tuning"), this);
    QVBoxLayout *cpuLayout = new QVBoxLayout(grpCpu);
    cpuLayout->setSpacing(8);

    m_radioCpuNative = new QRadioButton(tr("🚀 Native Microarchitecture (-march=native -O3) [Maximum Local Speed]"), grpCpu);
    QLabel *lblNativeDesc = new QLabel(tr("Compiles optimized specifically for your local CPU (AVX2, Zen, FMA). Delivers maximum speed and aggressive vectorization for complex files."), grpCpu);
    lblNativeDesc->setStyleSheet("color: #94a3b8; font-size: 11px; margin-left: 24px;");

    m_radioCpuStandard = new QRadioButton(tr("🛡️ Standard Universal Baseline (-march=x86-64 -O2) [Universal PC Compatibility]"), grpCpu);
    QLabel *lblStandardDesc = new QLabel(tr("Compiles baseline 64-bit binaries compatible with any x86-64 computer. Keeps binary compact and UI snappy."), grpCpu);
    lblStandardDesc->setStyleSheet("color: #94a3b8; font-size: 11px; margin-left: 24px;");

    cpuLayout->addWidget(m_radioCpuNative);
    cpuLayout->addWidget(lblNativeDesc);
    cpuLayout->addWidget(m_radioCpuStandard);
    cpuLayout->addWidget(lblStandardDesc);
    mainLayout->addWidget(grpCpu);

    // 2. Modules & Engines Group
    QGroupBox *grpModules = new QGroupBox(tr("Engines & Optional Modules"), this);
    QVBoxLayout *modLayout = new QVBoxLayout(grpModules);
    modLayout->setSpacing(10);

    // PDF Module
    QHBoxLayout *pdfRow = new QHBoxLayout();
    m_chkStaticPdf = new QCheckBox(tr("Embed PDF Engine statically (Self-contained, no external poppler)"), grpModules);
    bool popplerFound = BuildConfig::isPkgInstalled("poppler-qt6");
    m_lblPdfStatus = new QLabel(popplerFound ? tr("🟢 System library found") : tr("🟡 Missing system pkg"), grpModules);
    m_lblPdfStatus->setStyleSheet(popplerFound ? "color: #4ade80; font-size: 11px;" : "color: #facc15; font-size: 11px;");
    pdfRow->addWidget(m_chkStaticPdf, 1);
    pdfRow->addWidget(m_lblPdfStatus);
    modLayout->addLayout(pdfRow);

    // PDF Source Version Selector (Auto-discovery with curated fallback)
    m_pdfVersionContainer = new QWidget(grpModules);
    QHBoxLayout *pdfVerLayout = new QHBoxLayout(m_pdfVersionContainer);
    pdfVerLayout->setContentsMargins(24, 0, 0, 0);
    pdfVerLayout->setSpacing(8);

    QLabel *lblVerPrompt = new QLabel(tr("Poppler Source Release:"), m_pdfVersionContainer);
    lblVerPrompt->setStyleSheet("color: #94a3b8; font-size: 11px;");
    m_comboPdfVersion = new QComboBox(m_pdfVersionContainer);
    m_comboPdfVersion->setMinimumWidth(280);

    QString detectedVer = PackageManagerHelper::queryPackageCandidateVersion("poppler-qt6");
    if (!detectedVer.isEmpty()) {
        m_comboPdfVersion->addItem(tr("Auto-detected System (%1) [Recommended]").arg(detectedVer), detectedVer);
    }
    for (const QString &v : PackageManagerHelper::knownStableVersions("poppler-qt6")) {
        if (v != detectedVer) {
            m_comboPdfVersion->addItem(tr("Stable Release %1").arg(v), v);
        }
    }

    pdfVerLayout->addWidget(lblVerPrompt);
    pdfVerLayout->addWidget(m_comboPdfVersion);
    pdfVerLayout->addStretch();
    modLayout->addWidget(m_pdfVersionContainer);

    connect(m_chkStaticPdf, &QCheckBox::toggled, m_pdfVersionContainer, &QWidget::setVisible);
    m_pdfVersionContainer->setVisible(false);

    // Archive Module
    QHBoxLayout *archRow = new QHBoxLayout();
    m_chkStaticArchive = new QCheckBox(tr("Embed Archive Engine statically (Self-contained, no external libarchive)"), grpModules);
    bool archiveFound = BuildConfig::isPkgInstalled("libarchive");
    m_lblArchiveStatus = new QLabel(archiveFound ? tr("🟢 System library found") : tr("🟡 Missing system pkg"), grpModules);
    m_lblArchiveStatus->setStyleSheet(archiveFound ? "color: #4ade80; font-size: 11px;" : "color: #facc15; font-size: 11px;");
    archRow->addWidget(m_chkStaticArchive, 1);
    archRow->addWidget(m_lblArchiveStatus);
    modLayout->addLayout(archRow);

    // Internal Office Module (Optional)
    QHBoxLayout *officeRow = new QHBoxLayout();
    m_chkInternalOffice = new QCheckBox(tr("Enable Internal Office Document Viewer (Instant preview with librevenge)"), grpModules);
    bool officeFound = BuildConfig::isPkgInstalled("librevenge-0.0");
    m_lblOfficeStatus = new QLabel(officeFound ? tr("🟢 librevenge ready") : tr("🟡 librevenge missing"), grpModules);
    m_lblOfficeStatus->setStyleSheet(officeFound ? "color: #4ade80; font-size: 11px;" : "color: #facc15; font-size: 11px;");
    officeRow->addWidget(m_chkInternalOffice, 1);
    officeRow->addWidget(m_lblOfficeStatus);
    modLayout->addLayout(officeRow);

    QLabel *lblOfficeNote = new QLabel(tr("Note: If disabled, BubezView seamlessly converts documents using external LibreOffice without needing internal parsers."), grpModules);
    lblOfficeNote->setStyleSheet("color: #94a3b8; font-size: 11px; margin-left: 24px;");
    modLayout->addWidget(lblOfficeNote);

    mainLayout->addWidget(grpModules);

    // Progress and log widget
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    m_lblBuildStatus = new QLabel(this);
    m_lblBuildStatus->setStyleSheet("color: #38bdf8; font-size: 11px;");
    m_lblBuildStatus->hide();
    mainLayout->addWidget(m_lblBuildStatus);

    m_txtLog = new QTextEdit(this);
    m_txtLog->setReadOnly(true);
    m_txtLog->setFixedHeight(120);
    m_txtLog->setStyleSheet("font-family: monospace; font-size: 10px; background-color: #111; color: #a3e635; border-radius: 4px;");
    m_txtLog->hide();
    mainLayout->addWidget(m_txtLog);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnClose = new QPushButton(tr("Cancel"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnClose);

    btnLayout->addStretch();

    m_btnSave = new QPushButton(tr("💾 Save Configuration"), this);
    m_btnSave->setCursor(Qt::PointingHandCursor);
    connect(m_btnSave, &QPushButton::clicked, this, &BuildOptionsDialog::onSaveClicked);
    btnLayout->addWidget(m_btnSave);

    m_btnRecompile = new QPushButton(tr("🚀 Recompile & Apply Now"), this);
    m_btnRecompile->setCursor(Qt::PointingHandCursor);
    m_btnRecompile->setDefault(true);
    m_btnRecompile->setStyleSheet("background-color: #0284c7; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    connect(m_btnRecompile, &QPushButton::clicked, this, &BuildOptionsDialog::onRecompileClicked);
    btnLayout->addWidget(m_btnRecompile);

    mainLayout->addLayout(btnLayout);
}

void BuildOptionsDialog::loadFromConfig()
{
    BuildConfig &cfg = BuildConfig::instance();
    cfg.load();

    if (cfg.isNativeCpu()) {
        m_radioCpuNative->setChecked(true);
    } else {
        m_radioCpuStandard->setChecked(true);
    }

    m_chkStaticPdf->setChecked(cfg.staticPdf());
    QString savedVer = cfg.popplerVersion();
    if (!savedVer.isEmpty()) {
        int idx = m_comboPdfVersion->findData(savedVer);
        if (idx >= 0) {
            m_comboPdfVersion->setCurrentIndex(idx);
        }
    }
    m_pdfVersionContainer->setVisible(m_chkStaticPdf->isChecked());
    m_chkStaticArchive->setChecked(cfg.staticArchive());
    m_chkInternalOffice->setChecked(cfg.enableInternalOffice());
}

void BuildOptionsDialog::saveToConfig()
{
    BuildConfig &cfg = BuildConfig::instance();
    cfg.setCpuProfile(m_radioCpuNative->isChecked() ? "native" : "standard");
    cfg.setStaticPdf(m_chkStaticPdf->isChecked());
    QString chosenVer = m_comboPdfVersion->currentData().toString();
    if (chosenVer.isEmpty()) {
        chosenVer = m_comboPdfVersion->currentText();
    }
    cfg.setPopplerVersion(chosenVer);
    cfg.setStaticArchive(m_chkStaticArchive->isChecked());
    cfg.setEnableInternalOffice(m_chkInternalOffice->isChecked());
    cfg.save();
}

void BuildOptionsDialog::onSaveClicked()
{
    saveToConfig();
    QMessageBox::information(this, tr("Configuration Saved"),
        tr("Your compilation settings have been saved to disk.<br>They will be used automatically in future updates."));
    accept();
}

void BuildOptionsDialog::onRecompileClicked()
{
    saveToConfig();

    QString scriptPath = UpdateManager::instance().findInstallerScript();
    if (scriptPath.isEmpty()) {
        QMessageBox::warning(this, tr("Installer Not Found"),
            tr("Could not locate local compilation script (Installa).<br>Configuration saved, but automatic recompilation cannot start."));
        return;
    }

    // Check if internal office was requested but librevenge is missing on system
    if (m_chkInternalOffice->isChecked() && !BuildConfig::isPkgInstalled("librevenge-0.0")) {
        QMessageBox::StandardButton res = QMessageBox::question(this, tr("Missing Library"),
            tr("The Internal Office Viewer requires 'librevenge', which is not currently detected on your system.<br><br>Would you like the installer to attempt installing it via your system package manager?"),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::No) {
            return;
        }
    }

    // Long compilation warning for engine modules
    QMessageBox::StandardButton warn = QMessageBox::question(
        this,
        tr("Long Compilation Warning"),
        tr("<p><b>Notice: Compiling engine modules from source may take several minutes.</b></p>"
           "<p>Modules like Poppler PDF or OCR engines involve extensive C++ templates and large source trees.</p>"
           "<p>Are you sure you want to proceed with compilation?</p>"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    if (warn != QMessageBox::Yes) {
        return;
    }

    m_btnSave->setEnabled(false);
    m_btnRecompile->setEnabled(false);
    m_btnClose->setEnabled(false);

    m_progressBar->show();
    m_progressBar->setRange(0, 0); // Busy spinner
    m_lblBuildStatus->show();
    m_lblBuildStatus->setText(tr("⚙️ Configuring and compiling BubezView with updated options..."));
    m_txtLog->show();
    m_txtLog->clear();
    adjustSize();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildOptionsDialog::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildOptionsDialog::onProcessFinished);

    QStringList args;
    args << "--cli" << "--update";
    m_process->start(scriptPath, args);
}

void BuildOptionsDialog::onProcessOutput()
{
    if (!m_process) return;
    QString out = QString::fromUtf8(m_process->readAllStandardOutput());
    m_txtLog->append(out);

    if (out.contains("Compilazione") || out.contains("Building")) {
        m_lblBuildStatus->setText(tr("⚙️ Compiling updated modules in parallel..."));
    } else if (out.contains("CMake")) {
        m_lblBuildStatus->setText(tr("🔧 Generating CMake build environment..."));
    }
}

void BuildOptionsDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_progressBar->setRange(0, 100);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_progressBar->setValue(100);
        m_lblBuildStatus->setText(tr("✅ Compilation completed successfully! Changes will take effect the next time you start BubezView."));
        m_btnClose->setEnabled(true);
        m_btnClose->setText(tr("Close"));

        QMessageBox::StandardButton cleanQ = QMessageBox::question(
            this,
            tr("Clean Up Temporary Build Files?"),
            tr("<p><b>Compilation completed successfully!</b></p>"
               "<p>Would you like to clean up temporary intermediate object files and source build caches to save disk space?</p>"
               "<p><i>(CMake, compilers, and installed binaries will remain intact.)</i></p>"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );
        if (cleanQ == QMessageBox::Yes) {
            QProcess::startDetached("cmake", QStringList() << "--build" << "build_deb" << "--target" << "clean");
            m_lblBuildStatus->setText(tr("✅ Compilation completed and temporary files cleaned up!"));
        }
    } else {
        m_progressBar->setValue(0);
        m_lblBuildStatus->setText(tr("❌ Compilation failed. Check compiler log details."));
        m_btnClose->setEnabled(true);
        m_btnClose->setText(tr("Close"));
    }
}
