/**
 * @file settings_extra_tab.cpp
 * @brief Implementation of the diagnostics, multi-level debug modes, and maintenance tab.
 */

#include "settings_extra_tab.h"
#include "first_run_wizard.h"
#include "build_console_dialog.h"
#include "plugin_manager.h"
#include "build_config.h"
#include "theme_manager.h"
#include "desktop_integrator.h"
#include "preload_cache_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QListView>
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
#include <QDirIterator>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QProcess>

#ifndef BUBEZ_OPT_LEVEL
#define BUBEZ_OPT_LEVEL "O2"
#endif

static qint64 calculateDirectorySize(const QString &dirPath)
{
    qint64 total = 0;
    QDir dir(dirPath);
    if (!dir.exists()) return 0;

    QDirIterator it(dirPath, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

static QString formatBytesHuman(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    } else {
        return QString("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    }
}

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
    layout->setSpacing(10);
    layout->setContentsMargins(14, 8, 14, 8);

    // =========================================================================
    // Card 1: Maintenance & Recovery (Manutenzione & Ripristino)
    // =========================================================================
    m_grpMaintenance = new QGroupBox(tr("Maintenance && Recovery"), content);
    QVBoxLayout *layMaint = new QVBoxLayout(m_grpMaintenance);
    layMaint->setContentsMargins(14, 12, 14, 12);
    layMaint->setSpacing(10);

    m_lblMaintSub = new QLabel(
        tr("Rerun initial setup wizard, repair desktop associations and quick-look shortcuts, clear thumbnail cache, or uninstall."),
        m_grpMaintenance
    );
    m_lblMaintSub->setObjectName(QStringLiteral("RowSubtitle"));
    m_lblMaintSub->setWordWrap(true);
    layMaint->addWidget(m_lblMaintSub);

    QHBoxLayout *layMaintBtns = new QHBoxLayout();
    layMaintBtns->setContentsMargins(0, 0, 0, 0);
    layMaintBtns->setSpacing(10);

    m_btnRunWizard = new QPushButton(tr("🚀 Setup Wizard..."), m_grpMaintenance);
    m_btnRunWizard->setCursor(Qt::PointingHandCursor);
    m_btnRunWizard->setMinimumHeight(36);
    m_btnRunWizard->setToolTip(tr("Rerun the initial configuration wizard to customize appearance, skins, and presets"));
    connect(m_btnRunWizard, &QPushButton::clicked, this, &SettingsExtraTab::launchSetupWizard);

    m_btnReinstall = new QPushButton(tr("🔄 Repair Integrations"), m_grpMaintenance);
    m_btnReinstall->setCursor(Qt::PointingHandCursor);
    m_btnReinstall->setMinimumHeight(36);
    m_btnReinstall->setToolTip(tr("Restore desktop menu launcher, file manager right-click menus, and spacebar quick-look"));
    connect(m_btnReinstall, &QPushButton::clicked, this, &SettingsExtraTab::reinstallIntegrations);

    m_btnClearCache = new QPushButton(tr("🧹 Clear Cache"), m_grpMaintenance);
    m_btnClearCache->setCursor(Qt::PointingHandCursor);
    m_btnClearCache->setMinimumHeight(36);
    m_btnClearCache->setToolTip(tr("Clear thumbnail caches and temporary session files from ~/.cache/Bubez"));
    connect(m_btnClearCache, &QPushButton::clicked, this, &SettingsExtraTab::clearThumbnailCache);

    m_btnUninstall = new QPushButton(tr("🗑️ Uninstall..."), m_grpMaintenance);
    m_btnUninstall->setCursor(Qt::PointingHandCursor);
    m_btnUninstall->setMinimumHeight(36);
    m_btnUninstall->setToolTip(tr("Cleanly uninstall BubezView and unregister desktop components"));
    connect(m_btnUninstall, &QPushButton::clicked, this, &SettingsExtraTab::uninstallApplication);

    layMaintBtns->addWidget(m_btnRunWizard);
    layMaintBtns->addWidget(m_btnReinstall);
    layMaintBtns->addWidget(m_btnClearCache);
    layMaintBtns->addWidget(m_btnUninstall);
    layMaint->addLayout(layMaintBtns);

    layout->addWidget(m_grpMaintenance);

    // =========================================================================
    // Card 2: Diagnostics & Bug Reports (Diagnostica & Segnalazione Bug)
    // =========================================================================
    m_grpDebug = new QGroupBox(tr("Diagnostics && Bug Reports"), content);
    QVBoxLayout *layDebug = new QVBoxLayout(m_grpDebug);
    layDebug->setContentsMargins(14, 12, 14, 12);
    layDebug->setSpacing(10);

    m_lblDebugSub = new QLabel(
        tr("Configure logging verbosity or copy technical specifications for bug reporting."),
        m_grpDebug
    );
    m_lblDebugSub->setObjectName(QStringLiteral("RowSubtitle"));
    m_lblDebugSub->setWordWrap(true);
    layDebug->addWidget(m_lblDebugSub);

    QHBoxLayout *rowCombo = new QHBoxLayout();
    rowCombo->setContentsMargins(0, 0, 0, 0);
    rowCombo->setSpacing(10);
    QLabel *lblMode = new QLabel(tr("Diagnostic Mode:"), m_grpDebug);
    lblMode->setObjectName(QStringLiteral("RowTitle"));

    m_comboDebugLevel = new QComboBox(m_grpDebug);
    m_comboDebugLevel->setView(new QListView(m_comboDebugLevel));
    m_comboDebugLevel->setCursor(Qt::PointingHandCursor);
    m_comboDebugLevel->addItem(tr("Disabled — Standard User Mode (Optimal performance)"), 0);
    m_comboDebugLevel->addItem(tr("Level 1: Lightweight Log (Session logs to ~/.cache and CLI)"), 1);
    m_comboDebugLevel->addItem(tr("Level 2: Medium Debug (-O1 with debug symbols)"), 2);
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

    m_lblDebugStatus = new QLabel(m_grpDebug);
    m_lblDebugStatus->setWordWrap(true);
    layDebug->addWidget(m_lblDebugStatus);
    updateDebugStatusLabel();

    QHBoxLayout *layBottomAction = new QHBoxLayout();
    layBottomAction->setContentsMargins(0, 2, 0, 0);
    layBottomAction->setSpacing(12);

    m_lblTermHint = new QLabel(
        tr("💡 Terminal trace: <code>bubezview --debug</code>"),
        m_grpDebug
    );
    m_lblTermHint->setTextFormat(Qt::RichText);
    layBottomAction->addWidget(m_lblTermHint, 1, Qt::AlignVCenter);

    m_btnCopyReport = new QPushButton(tr("📋 Copy System Report"), m_grpDebug);
    m_btnCopyReport->setCursor(Qt::PointingHandCursor);
    m_btnCopyReport->setMinimumHeight(32);
    connect(m_btnCopyReport, &QPushButton::clicked, this, &SettingsExtraTab::copyFullReportToClipboard);
    layBottomAction->addWidget(m_btnCopyReport, 0, Qt::AlignVCenter);

    layDebug->addLayout(layBottomAction);

    layout->addWidget(m_grpDebug);
    layout->addStretch(1);

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    updateThemeStyles();
}

void SettingsExtraTab::updateDebugStatusLabel()
{
    if (!m_lblDebugStatus || !m_comboDebugLevel) return;

    int idx = m_comboDebugLevel->currentIndex();
    QString compiledOpt = QStringLiteral(BUBEZ_OPT_LEVEL);
    QString configuredOpt = BuildConfig::instance().optimizationLevel();

    switch (idx) {
    case 0:
        m_lblDebugStatus->setText(QString(tr("● <b>Standard User Mode:</b> Optimization -%1 (binary: -%2) • Logging disabled • Peak performance"))
                                  .arg(configuredOpt, compiledOpt));
        break;
    case 1:
        m_lblDebugStatus->setText(QString(tr("● <b>Level 1 (Lightweight Log):</b> Verbose logging active to terminal and <code>~/.cache/Bubez/BubezView/bubezview.log</code>")));
        break;
    case 2:
        m_lblDebugStatus->setText(QString(tr("● <b>Level 2 (Medium Debug):</b> Recompiled with -O1 -g for incremental bug tracing")));
        break;
    case 3:
        m_lblDebugStatus->setText(QString(tr("● <b>Level 3 (Deep Debug):</b> -O0 unoptimized with full symbols & frame pointers for GDB/Valgrind")));
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
    report << QString("- MuPDF engine (mutool): %1").arg(checkCmd("mutool"));
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

    QStringList candidateLogPaths = {
        logDir + "/bubezview.log",
        QDir::homePath() + "/.cache/Bubez/BubezView/bubezview.log",
        QDir::homePath() + "/.cache/BubezView/bubezview.log"
    };
    candidateLogPaths.removeDuplicates();

    QString logFilePath;
    for (const QString &candidate : candidateLogPaths) {
        if (QFile::exists(candidate)) {
            logFilePath = candidate;
            break;
        }
    }
    if (logFilePath.isEmpty()) {
        logFilePath = candidateLogPaths.first();
    }

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
        const QStringList pendingBuild = wizard.pendingBuildComponents();
        if (!pendingBuild.isEmpty()) {
            BuildConsoleDialog console(this);
            console.startBuildComponents(pendingBuild);
            console.exec();
        }
        emit setupWizardCompleted();
        emit themeChanged();
    }
}

void SettingsExtraTab::reinstallIntegrations()
{
    const auto de = DesktopIntegrator::currentDesktop();
    const QString deName = DesktopIntegrator::desktopName();

    // 1. Install desktop app launcher (.desktop in ~/.local/share/applications)
    DesktopIntegrator::installAppMenu();

    // 2. Install context menu actions (Nautilus, Nemo, Dolphin)
    DesktopIntegrator::enableContextMenu();

    // 3. Install desktop-specific QuickLook integration (Spacebar trigger)
    QString chosenShortcut = QStringLiteral("Space");
    if (de == DesktopIntegrator::DesktopEnv::KDE) {
        chosenShortcut = DesktopIntegrator::currentDolphinShortcut();
        DesktopIntegrator::enableDolphin(chosenShortcut);
    } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
        DesktopIntegrator::enableGnomeNemo();
    } else {
        DesktopIntegrator::enableDolphin(chosenShortcut);
        DesktopIntegrator::enableGnomeNemo();
    }

    // 4. Update system desktop database (kbuildsycoca is already handled inside enableDolphin)
    const QString home = QDir::homePath();
    QProcess::execute(QStringLiteral("update-desktop-database"), QStringList() << (home + QStringLiteral("/.local/share/applications")));

    const QString msg = tr("Desktop integrations for <b>%1</b> have been restored successfully!<br><br>"
                           "• <b>Application Menu:</b> Registered <code>bubezview.desktop</code> in <code>~/.local/share/applications</code><br>"
                           "• <b>Context Menu:</b> File manager actions active (Dolphin / Nautilus / Nemo)<br>"
                           "• <b>QuickLook Preview:</b> Shortcut <b>%2</b> reconfigured<br>"
                           "• <b>System Cache:</b> Desktop database and sycoca updated.")
                           .arg(deName, chosenShortcut);

    QMessageBox::information(this, tr("Integrations Restored"), msg);
}

void SettingsExtraTab::clearThumbnailCache()
{
    // Clear in-memory preload image cache
    PreloadCacheManager::instance().clear();

    const QString home = QDir::homePath();
    QStringList cachePaths = {
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
        home + QStringLiteral("/.cache/Bubez/BubezView"),
        home + QStringLiteral("/.cache/BubezView"),
        home + QStringLiteral("/.cache/Bubez")
    };
    cachePaths.removeDuplicates();

    qint64 totalFreed = 0;
    int filesRemoved = 0;

    for (const QString &path : cachePaths) {
        if (path.isEmpty() || !QDir(path).exists()) continue;

        totalFreed += calculateDirectorySize(path);

        QDir dir(path);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &fi : entries) {
            if (fi.isDir()) {
                QDir subDir(fi.absoluteFilePath());
                subDir.removeRecursively();
            } else {
                QFile::remove(fi.absoluteFilePath());
                filesRemoved++;
            }
        }
    }

    const QString msg = tr("BubezView cache has been cleared successfully!<br><br>"
                           "• <b>Disk space freed:</b> %1<br>"
                           "• <b>Temporary files removed:</b> %2<br>"
                           "• <b>In-memory cache:</b> cleared and released")
                           .arg(formatBytesHuman(totalFreed), QString::number(filesRemoved));

    QMessageBox::information(this, tr("Cache Cleared"), msg);
}

void SettingsExtraTab::uninstallApplication()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Uninstall BubezView"));
    dlg.setMinimumWidth(460);

    QVBoxLayout *dlgLay = new QVBoxLayout(&dlg);
    dlgLay->setSpacing(12);
    dlgLay->setContentsMargins(18, 18, 18, 18);

    QLabel *lblTitle = new QLabel(tr("<h3>Uninstall BubezView</h3>"), &dlg);
    dlgLay->addWidget(lblTitle);

    QLabel *lblDesc = new QLabel(
        tr("You are about to remove <b>BubezView</b> from your user account.<br><br>"
           "The following user-level components will be removed:<br>"
           "• User executables (<code>~/.local/bin/bubezview*</code>)<br>"
           "• Extensions and modules (<code>~/.local/lib/bubezview/</code>)<br>"
           "• Application menu entry (<code>bubezview.desktop</code>)<br>"
           "• File manager context menu actions (Dolphin, Nautilus, Nemo)"),
        &dlg
    );
    lblDesc->setWordWrap(true);
    dlgLay->addWidget(lblDesc);

    QCheckBox *chkCache = new QCheckBox(tr("Clear and remove thumbnail cache (~/.cache/Bubez)"), &dlg);
    chkCache->setChecked(true);
    dlgLay->addWidget(chkCache);

    QCheckBox *chkConfig = new QCheckBox(tr("Remove personal settings, history, and layout (~/.config/Bubez)"), &dlg);
    chkConfig->setChecked(false);
    dlgLay->addWidget(chkConfig);

    QLabel *lblWarn = new QLabel(tr("<span style='color: #ef4444; font-weight: 500;'>⚠️ The application will close once uninstallation completes.</span>"), &dlg);
    lblWarn->setWordWrap(true);
    dlgLay->addWidget(lblWarn);

    QDialogButtonBox *btnBox = new QDialogButtonBox(&dlg);
    QPushButton *btnCancel = btnBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    btnCancel->setCursor(Qt::PointingHandCursor);
    QPushButton *btnConfirm = btnBox->addButton(tr("Uninstall Now"), QDialogButtonBox::AcceptRole);
    btnConfirm->setCursor(Qt::PointingHandCursor);
    btnConfirm->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #dc2626, stop:1 #b91c1c);"
                       "  color: white;"
                       "  font-weight: bold;"
                       "  padding: 6px 14px;"
                       "  border: 1px solid rgba(239, 68, 68, 0.4);"
                       "  border-radius: 6px;"
                       "}"
                       "QPushButton:hover {"
                       "  background: #ef4444;"
                       "}")
    );

    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLay->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString home = QDir::homePath();

    // 1. Remove Desktop Integrations
    DesktopIntegrator::uninstallAppMenu();
    DesktopIntegrator::disableDolphin();
    DesktopIntegrator::disableGnomeNemo();
    DesktopIntegrator::disableContextMenu();

    QFile::remove(home + QStringLiteral("/.local/share/applications/bubezview.desktop"));
    QFile::remove(home + QStringLiteral("/.local/share/kio/servicemenus/bubezview.desktop"));
    QFile::remove(home + QStringLiteral("/.local/share/kservices5/ServiceMenus/bubezview.desktop"));
    QFile::remove(home + QStringLiteral("/.local/share/nautilus/scripts/Preview with BubezView"));
    QFile::remove(home + QStringLiteral("/.local/share/nautilus/scripts/Anteprima con BubezView"));
    QFile::remove(home + QStringLiteral("/.local/share/nemo/actions/bubezview.nemo_action"));

    QDir shareDir(home + QStringLiteral("/.local/share/bubezview"));
    if (shareDir.exists()) shareDir.removeRecursively();
    QDir shareDirBubez(home + QStringLiteral("/.local/share/BubezView"));
    if (shareDirBubez.exists()) shareDirBubez.removeRecursively();

    // 2. Remove Binaries
    QFile::remove(home + QStringLiteral("/.local/bin/bubezview"));
    QFile::remove(home + QStringLiteral("/.local/bin/bubezview-dev"));

    // 3. Remove Modular Plugins
    QDir libDir(home + QStringLiteral("/.local/lib/bubezview"));
    if (libDir.exists()) {
        libDir.removeRecursively();
    }

    // 4. Optionally Remove Cache
    if (chkCache->isChecked()) {
        QDir c1(home + QStringLiteral("/.cache/Bubez"));
        if (c1.exists()) c1.removeRecursively();
        QDir c2(home + QStringLiteral("/.cache/BubezView"));
        if (c2.exists()) c2.removeRecursively();
    }

    // 5. Optionally Remove Config
    if (chkConfig->isChecked()) {
        QDir cfg1(home + QStringLiteral("/.config/Bubez"));
        if (cfg1.exists()) cfg1.removeRecursively();
        QDir cfg2(home + QStringLiteral("/.config/BubezView"));
        if (cfg2.exists()) cfg2.removeRecursively();
    }

    // 6. Update Desktop Database (kbuildsycoca is already handled inside disableDolphin)
    QProcess::execute(QStringLiteral("update-desktop-database"), QStringList() << (home + QStringLiteral("/.local/share/applications")));

    // 7. Inform user and exit application
    QMessageBox::information(
        nullptr,
        tr("Uninstall Completed"),
        tr("<b>BubezView</b> has been successfully removed from your system.<br><br>"
           "Thank you for using BubezView! The application will now close.")
    );

    QCoreApplication::quit();
}

void SettingsExtraTab::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool dark = ThemeManager::isDarkTheme();

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString textCol;
    QString comboBg;
    QString comboBorder;
    QString comboPopupBg;
    QString comboSelectionBg;
    QString comboSelectionText;
    QString hoverBorder;
    QString btnBg;
    QString btnHover;

    if (active == ThemePreset::ZincGray) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        textCol = QStringLiteral("#ffffff");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        comboBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        comboPopupBg = QStringLiteral("#2a2b2e");
        comboSelectionBg = QStringLiteral("#585960");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("rgba(255, 255, 255, 0.40)");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        btnHover = QStringLiteral("#63646c");
    } else if (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.09)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#334155");
        textCol = QStringLiteral("#1e293b");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0)");
        comboBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        comboPopupBg = QStringLiteral("#f8fafc");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#0284c7");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnHover = QStringLiteral("#0ea5e9");
    } else {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        textCol = QStringLiteral("#f8fafc");

        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        comboBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        comboPopupBg = QStringLiteral("#151c28");
        comboSelectionBg = QStringLiteral("#0284c7");
        comboSelectionText = QStringLiteral("#ffffff");
        hoverBorder = QStringLiteral("#38bdf8");

        btnBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnHover = QStringLiteral("#0ea5e9");
    }

    const QString style = QString(
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 14px;"
        "  background: %3;"
        "  margin-top: 14px;"
        "  padding: 16px 14px 12px 14px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 14px;"
        "  top: 1px;"
        "  padding: 2px 12px;"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  background: transparent;"
        "}"
        "QPushButton {"
        "  background: %13;"
        "  color: #ffffff;"
        "  border: 1px solid %6;"
        "  border-radius: 10px;"
        "  padding: 6px 14px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: %14;"
        "  border-color: %8;"
        "}"
        "QComboBox {"
        "  background: %7;"
        "  color: %1;"
        "  border: 1px solid %6;"
        "  border-radius: 10px;"
        "  padding: 6px 30px 6px 12px;"
        "  font-size: 12px;"
        "  min-height: 26px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %8;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 24px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(%9);"
        "  width: 12px;"
        "  height: 12px;"
        "  margin-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %10;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 10px;"
        "  selection-background-color: %11;"
        "  selection-color: %12;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  min-height: 26px;"
        "  padding: 5px 12px;"
        "  border-radius: 6px;"
        "  color: %1;"
        "  background: transparent;"
        "}"
        "QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover {"
        "  background-color: %11;"
        "  color: %12;"
        "}"
    ).arg(textCol, cardBorder, cardBg, pillBg, pillColor, pillBorder, comboBg, hoverBorder,
          dark ? QStringLiteral(":/icons/chevron_down_light.svg") : QStringLiteral(":/icons/chevron_down_dark.svg"),
          comboPopupBg, comboSelectionBg, comboSelectionText, btnBg, btnHover);

    setStyleSheet(style);

    const QString popupViewStyle = QString(
        "QListView {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QListView::item {"
        "  min-height: 26px;"
        "  padding: 5px 12px;"
        "  border-radius: 6px;"
        "  color: %2;"
        "  background: transparent;"
        "}"
        "QListView::item:hover, QListView::item:selected {"
        "  background-color: %4;"
        "  color: %5;"
        "}"
    ).arg(comboPopupBg, textCol, cardBorder, comboSelectionBg, comboSelectionText);

    if (m_comboDebugLevel && m_comboDebugLevel->view()) {
        m_comboDebugLevel->view()->setStyleSheet(popupViewStyle);
    }

    if (m_btnUninstall) {
        m_btnUninstall->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #dc2626, stop:1 #b91c1c);"
                           "  color: #ffffff;"
                           "  border: 1px solid rgba(239, 68, 68, 0.4);"
                           "  border-radius: 8px;"
                           "  padding: 6px 14px;"
                           "  font-weight: 600;"
                           "  font-size: 11px;"
                           "}"
                           "QPushButton:hover {"
                           "  background: #ef4444;"
                           "  border-color: #fca5a5;"
                           "}"
                           "QPushButton:pressed {"
                           "  background: #991b1b;"
                           "}")
        );
    }

    if (m_btnCopyReport) {
        m_btnCopyReport->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background-color: #0284c7;"
                           "  color: #ffffff;"
                           "  border: 1px solid #38bdf8;"
                           "  border-radius: 8px;"
                           "  padding: 6px 14px;"
                           "  font-weight: 600;"
                           "  font-size: 11px;"
                           "}"
                           "QPushButton:hover {"
                           "  background-color: #0369a1;"
                           "  border-color: #7dd3fc;"
                           "}")
        );
    }

    if (m_lblDebugStatus) {
        if (dark) {
            m_lblDebugStatus->setStyleSheet(
                QStringLiteral("QLabel {"
                               "  background-color: rgba(255, 255, 255, 0.05);"
                               "  border: 1px solid rgba(255, 255, 255, 0.10);"
                               "  border-radius: 8px;"
                               "  padding: 6px 10px;"
                               "  font-size: 11px;"
                               "  color: #94a3b8;"
                               "}")
            );
        } else {
            m_lblDebugStatus->setStyleSheet(
                QStringLiteral("QLabel {"
                               "  background-color: rgba(15, 23, 42, 0.04);"
                               "  border: 1px solid rgba(15, 23, 42, 0.08);"
                               "  border-radius: 8px;"
                               "  padding: 6px 10px;"
                               "  font-size: 11px;"
                               "  color: #475569;"
                               "}")
            );
        }
    }

    const QString subColor = (active == ThemePreset::ZincGray) ? QStringLiteral("#a1a1aa") :
                             (dark ? QStringLiteral("#94a3b8") : QStringLiteral("#64748b"));
    if (m_lblMaintSub) m_lblMaintSub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(subColor));
    if (m_lblDebugSub) m_lblDebugSub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(subColor));
    if (m_lblTermHint) m_lblTermHint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(subColor));
}
