/**
 * @file settings_extra_tab.cpp
 * @brief Implementation of the diagnostics, multi-level debug modes, and maintenance tab.
 */

#include "settings_extra_tab.h"
#include "first_run_wizard.h"
#include "plugin_manager.h"
#include "build_config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
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
#include <QFile>
#include <QDir>

#ifndef BUBEZ_OPT_LEVEL
#define BUBEZ_OPT_LEVEL "O2"
#endif

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
    layout->setSpacing(14);
    layout->setContentsMargins(14, 14, 14, 14);

    // 1. Debugging & Diagnostics Mode Group
    QGroupBox *grpDebug = new QGroupBox(tr("Debugging & Diagnostics"), content);
    QVBoxLayout *layDebug = new QVBoxLayout(grpDebug);
    layDebug->setContentsMargins(12, 10, 12, 12);
    layDebug->setSpacing(10);

    QLabel *lblDebugIntro = new QLabel(
        tr("Manage diagnostic tracing, session logs, and developer compilation levels. In case of issues, you can inspect real-time logs or copy a complete diagnostic report."),
        grpDebug
    );
    lblDebugIntro->setWordWrap(true);
    lblDebugIntro->setStyleSheet("color: #64748b; font-size: 11px;");
    layDebug->addWidget(lblDebugIntro);

    QHBoxLayout *rowCombo = new QHBoxLayout();
    QLabel *lblMode = new QLabel(tr("<b>Diagnostic Mode:</b>"), grpDebug);
    m_comboDebugLevel = new QComboBox(grpDebug);
    m_comboDebugLevel->setCursor(Qt::PointingHandCursor);
    m_comboDebugLevel->addItem(tr("Disabled — Standard User Mode (Optimal performance)"), 0);
    m_comboDebugLevel->addItem(tr("Level 1: Lightweight Log (No recompile, session log & CLI output)"), 1);
    m_comboDebugLevel->addItem(tr("Level 2: Medium Debug (-O1 with symbols, fast incremental build)"), 2);
    m_comboDebugLevel->addItem(tr("Level 3: Deep Debug (-O0 full symbols & frame pointers)"), 3);

    QSettings settings("Bubez", "BubezView");
    bool isDebug = settings.value("debug_mode", false).toBool();
    QString optLevel = BuildConfig::instance().optimizationLevel();
    int initialLevel = 0;
    if (isDebug) {
        if (optLevel == "O0") initialLevel = 3;
        else if (optLevel == "O1") initialLevel = 2;
        else initialLevel = 1;
    } else {
        initialLevel = 0;
    }
    m_previousDebugLevel = initialLevel;
    m_comboDebugLevel->setCurrentIndex(initialLevel);

    connect(m_comboDebugLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsExtraTab::onDebugLevelChanged);

    rowCombo->addWidget(lblMode);
    rowCombo->addWidget(m_comboDebugLevel, 1);
    layDebug->addLayout(rowCombo);

    m_lblDebugStatus = new QLabel(grpDebug);
    m_lblDebugStatus->setWordWrap(true);
    m_lblDebugStatus->setStyleSheet("QLabel { background-color: rgba(100, 116, 139, 0.08); border: 1px solid rgba(100, 116, 139, 0.2); border-radius: 6px; padding: 8px 12px; font-size: 11px; }");
    layDebug->addWidget(m_lblDebugStatus);
    updateDebugStatusLabel();

    QLabel *lblTermHint = new QLabel(
        tr("💡 <b>Live Terminal Debugging:</b> To trace crashes, console errors, or real-time events live without modifying compiler flags, run in terminal: <code>bubezview --debug</code>"),
        grpDebug
    );
    lblTermHint->setWordWrap(true);
    lblTermHint->setStyleSheet("QLabel { background-color: rgba(2, 132, 199, 0.08); border: 1px solid rgba(2, 132, 199, 0.25); border-radius: 6px; padding: 8px 12px; font-size: 11px; }");
    layDebug->addWidget(lblTermHint);

    QHBoxLayout *layReport = new QHBoxLayout();
    QLabel *lblReportDesc = new QLabel(tr("Copy full system specifications, active compiler flags, soft dependencies, and session debug logs:"), grpDebug);
    lblReportDesc->setWordWrap(true);
    lblReportDesc->setStyleSheet("color: #475569; font-size: 11px;");

    QPushButton *btnCopyReport = new QPushButton(tr("📋 Copy Full Diagnostics Report"), grpDebug);
    btnCopyReport->setCursor(Qt::PointingHandCursor);
    btnCopyReport->setMinimumWidth(220);
    btnCopyReport->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }");
    connect(btnCopyReport, &QPushButton::clicked, this, &SettingsExtraTab::copyFullReportToClipboard);

    layReport->addWidget(lblReportDesc, 1);
    layReport->addWidget(btnCopyReport);
    layDebug->addLayout(layReport);

    layout->addWidget(grpDebug);

    // 2. Guided Setup & Maintenance Group
    QGroupBox *grpWizard = new QGroupBox(tr("Setup & Maintenance"), content);
    QHBoxLayout *layWizard = new QHBoxLayout(grpWizard);
    layWizard->setContentsMargins(12, 10, 12, 12);

    QLabel *lblWizard = new QLabel(tr("Re-run the initial onboarding assistant to reconfigure desktop integrations, shortcut triggers, and default settings:"), grpWizard);
    lblWizard->setWordWrap(true);
    lblWizard->setStyleSheet("color: #475569; font-size: 11px;");

    QPushButton *btnRunWizard = new QPushButton(tr("Run Setup Wizard..."), grpWizard);
    btnRunWizard->setCursor(Qt::PointingHandCursor);
    btnRunWizard->setMinimumWidth(180);
    connect(btnRunWizard, &QPushButton::clicked, this, &SettingsExtraTab::launchSetupWizard);

    layWizard->addWidget(lblWizard, 1);
    layWizard->addWidget(btnRunWizard);
    layout->addWidget(grpWizard);

    layout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsExtraTab::updateDebugStatusLabel()
{
    if (!m_lblDebugStatus || !m_comboDebugLevel) return;

    int idx = m_comboDebugLevel->currentIndex();
    QString compiledOpt = QStringLiteral(BUBEZ_OPT_LEVEL);
    QString configuredOpt = BuildConfig::instance().optimizationLevel();

    switch (idx) {
    case 0:
        m_lblDebugStatus->setText(QString(tr("🟢 <b>Current Status: Standard User Mode</b><br>"
                                             "• Configured Optimization: <b>-%1</b> (Compiled binary: <b>-%2</b>)<br>"
                                             "• Verbose Logging: <b>Disabled</b><br>"
                                             "• Peak runtime performance and battery efficiency enabled.")).arg(configuredOpt, compiledOpt));
        break;
    case 1:
        m_lblDebugStatus->setText(QString(tr("🟡 <b>Current Status: Level 1 — Lightweight Log</b><br>"
                                             "• Configured Optimization: <b>-%1</b> (No recompilation required)<br>"
                                             "• Verbose Logging: <b>Active</b><br>"
                                             "• Session logs streamed live to terminal and <code>~/.cache/Bubez/BubezView/bubezview.log</code>.")).arg(compiledOpt));
        break;
    case 2:
        m_lblDebugStatus->setText(QString(tr("🟠 <b>Current Status: Level 2 — Medium Debug (-O1)</b><br>"
                                             "• Optimization: <b>-O1</b> with debug symbols (<code>-g</code>)<br>"
                                             "• Fast incremental recompile; suitable for general bug tracking.")));
        break;
    case 3:
        m_lblDebugStatus->setText(QString(tr("🔴 <b>Current Status: Level 3 — Deep Debug (-O0)</b><br>"
                                             "• Optimization: <b>-O0</b> (Unoptimized with full debug symbols and frame pointers)<br>"
                                             "• Maximum debuggability for GDB, Valgrind, and backtraces.")));
        break;
    default:
        break;
    }
}

void SettingsExtraTab::onDebugLevelChanged(int index)
{
    if (m_isUpdatingUi || index == m_previousDebugLevel) return;

    QSettings settings("Bubez", "BubezView");

    if (index == 0) {
        // Switching to Disabled / User Mode
        if (m_previousDebugLevel == 2 || m_previousDebugLevel == 3 ||
            BuildConfig::instance().optimizationLevel() == "O1" || BuildConfig::instance().optimizationLevel() == "O0") {
            QString restoredOpt = settings.value("backup_user_opt_level", "O2").toString();
            if (restoredOpt != "O2" && restoredOpt != "O3") {
                restoredOpt = "O2";
            }

            QMessageBox::StandardButton res = QMessageBox::question(
                this,
                tr("Restore User Mode"),
                tr("Restoring User Mode will recompile BubezView with standard optimization (<b>-%1</b>).<br><br>"
                   "<b>Target:</b> Main executable only (<code>bubezview</code>). Modular plugins will not be touched.<br><br>"
                   "Start recompilation now?").arg(restoredOpt),
                QMessageBox::Yes | QMessageBox::No
            );

            if (res == QMessageBox::Yes) {
                settings.setValue("debug_mode", false);
                settings.sync();
                BuildConfig::instance().setOptimizationLevel(restoredOpt);
                BuildConfig::instance().save();
                m_previousDebugLevel = 0;
                updateDebugStatusLabel();
                emit recompileRequested(QStringLiteral("bubezview"));
            } else {
                m_isUpdatingUi = true;
                m_comboDebugLevel->setCurrentIndex(m_previousDebugLevel);
                m_isUpdatingUi = false;
                return;
            }
        } else {
            // Level 1 -> Level 0: Just disable logging
            settings.setValue("debug_mode", false);
            settings.sync();
            m_previousDebugLevel = 0;
            updateDebugStatusLabel();
        }
    } else if (index == 1) {
        // Level 1: Lightweight Log
        if (m_previousDebugLevel == 2 || m_previousDebugLevel == 3) {
            QString restoredOpt = settings.value("backup_user_opt_level", "O2").toString();
            if (restoredOpt != "O2" && restoredOpt != "O3") {
                restoredOpt = "O2";
            }

            QMessageBox::StandardButton res = QMessageBox::question(
                this,
                tr("Switch to Level 1"),
                tr("Level 1 uses standard optimization with verbose logging enabled.<br><br>"
                   "Would you like to restore standard optimization (<b>-%1</b>) for 'bubezview'?").arg(restoredOpt),
                QMessageBox::Yes | QMessageBox::No
            );

            if (res == QMessageBox::Yes) {
                settings.setValue("debug_mode", true);
                BuildConfig::instance().setOptimizationLevel(restoredOpt);
                BuildConfig::instance().save();
                m_previousDebugLevel = 1;
                updateDebugStatusLabel();
                emit recompileRequested(QStringLiteral("bubezview"));
            } else {
                settings.setValue("debug_mode", true);
                m_previousDebugLevel = 1;
                updateDebugStatusLabel();
            }
        } else {
            settings.setValue("debug_mode", true);
            m_previousDebugLevel = 1;
            updateDebugStatusLabel();
        }
    } else if (index == 2) {
        // Level 2: Medium Debug (-O1)
        QString curOpt = BuildConfig::instance().optimizationLevel();
        if (curOpt != "O1" && curOpt != "O0") {
            settings.setValue("backup_user_opt_level", curOpt);
        }

        QMessageBox::StandardButton res = QMessageBox::question(
            this,
            tr("Switch to Level 2 (Medium Debug)"),
            tr("Level 2 requires recompiling BubezView with <b>-O1 -g</b>.<br><br>"
               "<b>Target:</b> Main executable only (<code>bubezview</code>). Modular plugins will not be touched.<br><br>"
               "Start recompilation now?"),
            QMessageBox::Yes | QMessageBox::No
        );

        if (res == QMessageBox::Yes) {
            settings.setValue("debug_mode", true);
            BuildConfig::instance().setOptimizationLevel("O1");
            BuildConfig::instance().save();
            m_previousDebugLevel = 2;
            updateDebugStatusLabel();
            emit recompileRequested(QStringLiteral("bubezview"));
        } else {
            m_isUpdatingUi = true;
            m_comboDebugLevel->setCurrentIndex(m_previousDebugLevel);
            m_isUpdatingUi = false;
            return;
        }
    } else if (index == 3) {
        // Level 3: Deep Debug (-O0)
        QString curOpt = BuildConfig::instance().optimizationLevel();
        if (curOpt != "O1" && curOpt != "O0") {
            settings.setValue("backup_user_opt_level", curOpt);
        }

        QMessageBox::StandardButton res = QMessageBox::question(
            this,
            tr("Switch to Level 3 (Deep Debug)"),
            tr("Level 3 requires recompiling BubezView with <b>-O0 -g -fno-omit-frame-pointer</b>.<br><br>"
               "<b>Target:</b> Main executable only (<code>bubezview</code>). Modular plugins will not be touched.<br><br>"
               "Start recompilation now?"),
            QMessageBox::Yes | QMessageBox::No
        );

        if (res == QMessageBox::Yes) {
            settings.setValue("debug_mode", true);
            BuildConfig::instance().setOptimizationLevel("O0");
            BuildConfig::instance().save();
            m_previousDebugLevel = 3;
            updateDebugStatusLabel();
            emit recompileRequested(QStringLiteral("bubezview"));
        } else {
            m_isUpdatingUi = true;
            m_comboDebugLevel->setCurrentIndex(m_previousDebugLevel);
            m_isUpdatingUi = false;
            return;
        }
    }
}

void SettingsExtraTab::copyFullReportToClipboard()
{
    QStringList report;
    report << "### BubezView System & Diagnostics Report";
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

    // 1. Compiler and Optimization Settings
    report << QString("- **Compiled Optimization Level**: -%1").arg(BUBEZ_OPT_LEVEL);
    report << QString("- **Configured Optimization Level**: -%1").arg(BuildConfig::instance().optimizationLevel());
    report << QString("- **CPU Native Profile**: %1").arg(BuildConfig::instance().isNativeCpu() ? "Yes (-march=native)" : "No");

    QSettings settings("Bubez", "BubezView");
    bool debugActive = settings.value("debug_mode", false).toBool();
    QString debugDesc;
    if (debugActive) {
        int lvl = m_comboDebugLevel ? m_comboDebugLevel->currentIndex() : 1;
        debugDesc = QString("Yes (Level: %1)").arg(lvl);
    } else {
        debugDesc = "No (Disabled — Standard User Mode)";
    }
    report << QString("- **Debug Mode Active**: %1").arg(debugDesc);

    // 2. Modular Engine Plugins
    report << "\n#### Modular Engine Plugins";
    const auto plugins = PluginManager::instance().discoveredPlugins();
    if (plugins.isEmpty()) {
        report << "- (No dynamic plugins discovered; relying on compiled-in builtins)";
    } else {
        for (const auto &p : plugins) {
            QString status = p.isLoaded
                ? QString("Loaded (%1)").arg(p.filePath)
                : QString("Not loaded");
            report << QString("- **%1** (`%2`): %3").arg(p.displayName, p.id, status);
        }
    }

    // 3. Optional External Tools (Soft Dependencies)
    auto checkCmd = [](const QString &bin) -> QString {
        QString path = QStandardPaths::findExecutable(bin);
        return path.isEmpty() ? QString("Not installed (optional)") : QString("Available (%1)").arg(path);
    };

    report << "\n#### Optional External Tools (Soft Dependencies)";
    report << QString("- Office converter (libreoffice): %1").arg(checkCmd("libreoffice"));
    report << QString("- OCR engine (tesseract): %1").arg(checkCmd("tesseract"));
    report << QString("- PDF repair (qpdf): %1").arg(checkCmd("qpdf"));
    report << QString("- PDF image extractor (pdfimages): %1").arg(checkCmd("pdfimages"));
    report << QString("- Multimedia fallback (ffmpeg): %1").arg(checkCmd("ffmpeg"));

    report << QString("\n- **Config Path**: %1").arg(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    report << QString("- **Cache Path**: %1").arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

    // 4. Session Debug Log
    report << "\n#### Session Execution Log";
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (logDir.isEmpty()) {
        logDir = QDir::homePath() + "/.cache/BubezView";
    }
    const QString logFilePath = logDir + "/bubezview.log";

    QFile file(logFilePath);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString logContent = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        if (!logContent.isEmpty()) {
            report << "```log";
            report << logContent;
            report << "```";
        } else {
            report << "*(Log file exists but contains no recorded events)*";
        }
    } else {
        report << QString("*(No active debug log found at `%1`)*").arg(logFilePath);
    }

    QString fullText = report.join("\n");
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(fullText);
    }

    QMessageBox::information(
        this,
        tr("Diagnostics Report Copied"),
        tr("A complete diagnostics report containing system specifications, compiler settings, plugin statuses, and session logs has been copied to your clipboard.<br><br>"
           "You can now paste it directly into your bug report or forum discussion.")
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
