/**
 * @file settings_extra_tab.cpp
 * @brief Implementation of the general preferences, diagnostics, and setup wizard settings tab.
 */

#include "settings_extra_tab.h"
#include "theme_manager.h"
#include "first_run_wizard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QScrollArea>
#include <QFrame>
#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSysInfo>
#include <QScreen>

SettingsExtraTab::SettingsExtraTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsExtraTab::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget(scrollArea);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setSpacing(12);
    layout->setContentsMargins(14, 14, 14, 14);

    // 1. Language Selection Group
    QGroupBox *grpLang = new QGroupBox(tr("Language"), content);
    QHBoxLayout *layLang = new QHBoxLayout(grpLang);
    layLang->setContentsMargins(12, 10, 12, 12);

    QLabel *lblLang = new QLabel(tr("Interface language:"), grpLang);
    m_comboLanguage = new QComboBox(grpLang);
    m_comboLanguage->setCursor(Qt::PointingHandCursor);
    m_comboLanguage->addItem(tr("System Default (Automatic)"), "system");
    m_comboLanguage->addItem("Italiano", "it");
    m_comboLanguage->addItem("English", "en");
    m_comboLanguage->addItem("Español", "es");
    m_comboLanguage->addItem("Français", "fr");
    m_comboLanguage->addItem("Deutsch", "de");

    QSettings settings("Bubez", "BubezView");
    QString savedLang = settings.value("language", "system").toString();
    int langIdx = m_comboLanguage->findData(savedLang);
    if (langIdx >= 0) {
        m_comboLanguage->setCurrentIndex(langIdx);
    }
    connect(m_comboLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsExtraTab::onLanguageChanged);

    layLang->addWidget(lblLang);
    layLang->addWidget(m_comboLanguage, 1);
    layout->addWidget(grpLang);

    // 2. Office Engine Group
    QGroupBox *grpOffice = new QGroupBox(tr("Office Documents"), content);
    QVBoxLayout *layOffice = new QVBoxLayout(grpOffice);
    layOffice->setContentsMargins(12, 10, 12, 12);

    m_chkUseLibreOffice = new QCheckBox(tr("Use LibreOffice for preview (high-fidelity PDF)"), grpOffice);
    m_chkUseLibreOffice->setCursor(Qt::PointingHandCursor);
    m_chkUseLibreOffice->setChecked(ThemeManager::useLibreOffice());
    connect(m_chkUseLibreOffice, &QCheckBox::toggled, this, &SettingsExtraTab::onUseLibreOfficeToggled);
    layOffice->addWidget(m_chkUseLibreOffice);

    layout->addWidget(grpOffice);

    // 3. Interface & Viewer Components
    QGroupBox *grpInterface = new QGroupBox(tr("Interface & Viewer"), content);
    QVBoxLayout *layInterface = new QVBoxLayout(grpInterface);
    layInterface->setContentsMargins(12, 10, 12, 12);

    m_chkFilmstrip = new QCheckBox(tr("Show bottom thumbnail filmstrip by default"), grpInterface);
    m_chkFilmstrip->setCursor(Qt::PointingHandCursor);
    m_chkFilmstrip->setChecked(settings.value("ui/filmstrip", false).toBool());
    connect(m_chkFilmstrip, &QCheckBox::toggled, this, [](bool checked) {
        QSettings s;
        s.setValue("ui/filmstrip", checked);
        s.setValue("show_filmstrip", checked);
    });
    layInterface->addWidget(m_chkFilmstrip);
    layout->addWidget(grpInterface);

    // 4. Debugging & Diagnostics Group
    QGroupBox *grpDebug = new QGroupBox(tr("Debugging & Diagnostics"), content);
    QVBoxLayout *layDebug = new QVBoxLayout(grpDebug);
    layDebug->setContentsMargins(12, 10, 12, 12);
    layDebug->setSpacing(8);

    m_chkDebugMode = new QCheckBox(tr("Enable Debug Mode (verbose console and diagnostic output)"), grpDebug);
    m_chkDebugMode->setCursor(Qt::PointingHandCursor);
    m_chkDebugMode->setChecked(settings.value("debug_mode", false).toBool());
    connect(m_chkDebugMode, &QCheckBox::toggled, this, [](bool checked) {
        QSettings s;
        s.setValue("debug_mode", checked);
    });
    layDebug->addWidget(m_chkDebugMode);

    QHBoxLayout *layDiagBtn = new QHBoxLayout();
    QLabel *lblDiagDesc = new QLabel(tr("Generate and copy system configuration for bug reports:"), grpDebug);
    QPushButton *btnCopyDiag = new QPushButton(tr("Copy System Diagnostics"), grpDebug);
    btnCopyDiag->setCursor(Qt::PointingHandCursor);
    btnCopyDiag->setMinimumWidth(180);
    connect(btnCopyDiag, &QPushButton::clicked, this, &SettingsExtraTab::copyDiagnosticsToClipboard);

    layDiagBtn->addWidget(lblDiagDesc, 1);
    layDiagBtn->addWidget(btnCopyDiag);
    layDebug->addLayout(layDiagBtn);

    layout->addWidget(grpDebug);

    // 5. Guided Setup Wizard Group
    QGroupBox *grpWizard = new QGroupBox(tr("Setup Wizard"), content);
    QHBoxLayout *layWizard = new QHBoxLayout(grpWizard);
    layWizard->setContentsMargins(12, 10, 12, 12);

    QLabel *lblWizard = new QLabel(tr("Re-run the initial onboarding and integration assistant:"), grpWizard);
    QPushButton *btnRunWizard = new QPushButton(tr("Run Setup Wizard..."), grpWizard);
    btnRunWizard->setCursor(Qt::PointingHandCursor);
    btnRunWizard->setMinimumWidth(160);
    connect(btnRunWizard, &QPushButton::clicked, this, &SettingsExtraTab::launchSetupWizard);

    layWizard->addWidget(lblWizard, 1);
    layWizard->addWidget(btnRunWizard);
    layout->addWidget(grpWizard);

    layout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsExtraTab::onLanguageChanged(int index)
{
    if (!m_comboLanguage || index < 0) return;
    QString langCode = m_comboLanguage->itemData(index).toString();
    QSettings settings("Bubez", "BubezView");
    settings.setValue("language", langCode);
    QMessageBox::information(this, tr("Language"),
        tr("Language preference saved. Please restart BubezView to apply the new language."));
}

void SettingsExtraTab::onUseLibreOfficeToggled(bool checked)
{
    ThemeManager::setUseLibreOffice(checked);
}

void SettingsExtraTab::copyDiagnosticsToClipboard()
{
    QStringList report;
    report << "### BubezView System Diagnostics";
    QString appVer = QCoreApplication::applicationVersion();
    report << QString("- **Version**: %1").arg(appVer.isEmpty() ? QStringLiteral("0.1.0") : appVer);
    report << QString("- **OS**: %1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion());
    report << QString("- **Architecture**: %1 (%2)").arg(QSysInfo::currentCpuArchitecture(), QSysInfo::buildAbi());
    report << QString("- **Qt Version**: Compiled with %1, Running on %2").arg(QT_VERSION_STR, qVersion());

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        report << QString("- **Screen**: %1x%2 (DPI: %3, DevicePixelRatio: %4)")
                      .arg(screen->geometry().width())
                      .arg(screen->geometry().height())
                      .arg(screen->logicalDotsPerInch())
                      .arg(screen->devicePixelRatio());
    }

    auto checkCmd = [](const QString &bin) -> QString {
        QString path = QStandardPaths::findExecutable(bin);
        return path.isEmpty() ? "Not found" : QString("Available (%1)").arg(path);
    };

    report << "- **External Components**:\n"
           << QString("  - Poppler utils (pdfimages): %1\n").arg(checkCmd("pdfimages"))
           << QString("  - Poppler render (pdftoppm): %1\n").arg(checkCmd("pdftoppm"))
           << QString("  - PDF repair (qpdf): %1\n").arg(checkCmd("qpdf"))
           << QString("  - Office engine (libreoffice): %1\n").arg(checkCmd("libreoffice"))
           << QString("  - OCR engine (tesseract): %1\n").arg(checkCmd("tesseract"))
           << QString("  - Multimedia (ffmpeg): %1").arg(checkCmd("ffmpeg"));

    report << QString("- **Config Path**: %1").arg(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    report << QString("- **Debug Mode Active**: %1").arg(m_chkDebugMode && m_chkDebugMode->isChecked() ? "Yes" : "No");

    QString fullText = report.join("\n");
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(fullText);
    }

    QMessageBox::information(
        this,
        tr("Diagnostics Copied"),
        tr("System diagnostics have been copied to the clipboard.<br><br>You can now paste them directly into your bug report.")
    );
}

void SettingsExtraTab::launchSetupWizard()
{
    FirstRunWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted) {
        emit setupWizardCompleted();
        emit themeChanged();
    }
}
