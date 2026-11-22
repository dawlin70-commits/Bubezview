#include "mainwindow.h"
#include "dbus_previewer.h"
#include "theme_manager.h"
#include "first_run_wizard.h"
#include "build_console_dialog.h"
#include "build_config.h"
#include "plugin_manager.h"
#include "settings_dialog.h"
#include "settings_extensions_tab.h"
#include "compiler_wizard_dialog.h"
#include "diagnostic_report_dialog.h"
#include "settings_performance_tab.h"
#include "gpu_acceleration_manager.h"
#include "share_dialog.h"
#include "image_resize_dialog.h"
#include "goto_folder_dialog.h"
#include "pdf_repair_dialog.h"
#include "ocr_dialog.h"
#include <QApplication>
#include <QComboBox>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QStyleFactory>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QUrl>
#include <QThread>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QSettings>
#include <QDebug>
#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include <memory>
#include <clocale>
#include <sys/stat.h>
#include <unistd.h>

static std::unique_ptr<QFile> s_debugLogFile;
static std::unique_ptr<QTextStream> s_debugLogStream;

/**
 * @brief Custom Qt message handler redirecting logs to console and bubezview.log.
 */
static void bubezLogMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    const QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const char *typeStr = "INFO";
    switch (type) {
    case QtDebugMsg:    typeStr = "DEBUG"; break;
    case QtInfoMsg:     typeStr = "INFO"; break;
    case QtWarningMsg:  typeStr = "WARN"; break;
    case QtCriticalMsg: typeStr = "CRIT"; break;
    case QtFatalMsg:    typeStr = "FATAL"; break;
    }

    const QString formattedMsg = QString("[%1] [%2] %3\n").arg(timeStr, QString::fromLatin1(typeStr), msg);

    // 1. Output live to stderr
    std::cerr << formattedMsg.toUtf8().constData();
    std::cerr.flush();

    // 2. Output to log file if open with 2 MB safety cap
    if (s_debugLogFile && s_debugLogFile->isOpen() && s_debugLogStream) {
        if (s_debugLogFile->size() < 2 * 1024 * 1024) {
            *s_debugLogStream << formattedMsg;
            s_debugLogStream->flush();
        }
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

#ifndef BUBEZVIEW_VERSION
#define BUBEZVIEW_VERSION "0.1.0"
#endif

int main(int argc, char *argv[])
{
    // Fast-path command-line options that do not require GUI or X11/Wayland display initialization
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v" || arg == "-version") {
            std::cout << "BubezView " << BUBEZVIEW_VERSION << "\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h" || arg == "-help") {
            std::cout << "BubezView " << BUBEZVIEW_VERSION << " — Universal fast file viewer\n"
                      << "Usage: bubezview [options] [file]\n\n"
                      << "Options:\n"
                      << "  -h, --help       Displays help on commandline options.\n"
                      << "  -v, --version    Displays version information.\n"
                      << "  -d, --debug      Enable verbose debug output.\n";
            return 0;
        }
    }

    // libmpv requires LC_NUMERIC to be set to "C" for proper decimal parsing
    std::setlocale(LC_NUMERIC, "C");

    // Enable FreeDesktop Portal integration for 100% native system file pickers (KDE Dolphin / GNOME)
    if (!qEnvironmentVariableIsSet("QT_USE_PORTAL")) {
        qputenv("QT_USE_PORTAL", "1");
    }

    // Configure audio playback buffer latency on Linux/PipeWire/PulseAudio
    // to prevent underruns/dropouts during UI activity or intensive tasks.
    if (!qEnvironmentVariableIsSet("PULSE_LATENCY_MSEC")) {
        qputenv("PULSE_LATENCY_MSEC", "200");
    }

    // Setup high-performance OpenGL surface format (VSync, DoubleBuffering, 4x MSAA)
    GpuAccelerationManager::setupSurfaceFormat();

    // Isolate configuration when running development builds so testing preferences (e.g. themes)
    // do not overwrite the system-installed BubezView user configuration (~/.config/Bubez/BubezView.conf).
    if (!qEnvironmentVariableIsSet("BUBEZVIEW_NO_DEV_ISOLATION")) {
        char exePath[4096] = {0};
        ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (count > 0) {
            std::string path(exePath, count);
            if (path.find("/build_deb/") != std::string::npos || path.find("/Progetti/BubezView/") != std::string::npos) {
                const char *currentXdg = getenv("XDG_CONFIG_HOME");
                if (!currentXdg || strlen(currentXdg) == 0) {
                    const char *home = getenv("HOME");
                    if (home) {
                        std::string devConfigDir = std::string(home) + "/.config/bubezview-dev";
                        mkdir(devConfigDir.c_str(), 0755);
                        mkdir((devConfigDir + "/Bubez").c_str(), 0755);
                        std::string mainConf = std::string(home) + "/.config/Bubez/BubezView.conf";
                        std::string devConf = devConfigDir + "/Bubez/BubezView.conf";
                        if (access(devConf.c_str(), F_OK) != 0 && access(mainConf.c_str(), F_OK) == 0) {
                            QFile::copy(QString::fromStdString(mainConf), QString::fromStdString(devConf));
                        }
                        setenv("XDG_CONFIG_HOME", devConfigDir.c_str(), 1);
                    }
                }
            }
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("BubezView");
    app.setApplicationVersion(BUBEZVIEW_VERSION);
    app.setOrganizationName("Bubez");
    app.setDesktopFileName("bubezview");

    // Self-heal configuration file permissions if a root-owned container process replaced it
    {
        QSettings checkSettings("Bubez", "BubezView");
        const QString cfgPath = checkSettings.fileName();
        QFileInfo cfgInfo(cfgPath);
        if (cfgInfo.exists() && !cfgInfo.isWritable()) {
            QFile oldFile(cfgPath);
            if (oldFile.open(QIODevice::ReadOnly)) {
                const QByteArray rawData = oldFile.readAll();
                oldFile.close();
                if (QFile::remove(cfgPath)) {
                    QFile newFile(cfgPath);
                    if (newFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        newFile.write(rawData);
                        newFile.close();
                    }
                }
            }
        }
    }

    // GPU acceleration detection is now initialized lazily on first access to keep cold startup fast.

    // Ensure Qt discovers image and multimedia plugins in user-local directories (~/.local)
    // especially critical on immutable distributions (SteamOS, Fedora Silverblue, Bazzite)
    const QString userHome = QDir::homePath();
    const QStringList extraPluginDirs = {
        userHome + "/.local/lib/qt6/plugins",
        userHome + "/.local/lib/x86_64-linux-gnu/qt6/plugins",
        userHome + "/.local/lib64/qt6/plugins",
        userHome + "/.local/plugins",
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/bubezview/plugins"
    };
    for (const QString &pluginDir : extraPluginDirs) {
        if (QDir(pluginDir).exists()) {
            QCoreApplication::addLibraryPath(pluginDir);
        }
    }

    // Ensure Distrobox/container environments can locate host system icons (e.g. Breeze)
    QStringList iconSearchPaths = QIcon::themeSearchPaths();
    if (QDir("/run/host/usr/share/icons").exists()) {
        iconSearchPaths.prepend("/run/host/usr/share/icons");
    }
    if (QDir("/run/host/usr/local/share/icons").exists()) {
        iconSearchPaths.prepend("/run/host/usr/local/share/icons");
    }
    QIcon::setThemeSearchPaths(iconSearchPaths);

    if (QIcon::themeName().isEmpty() || QIcon::themeName() == "hicolor") {
        if (QDir("/run/host/usr/share/icons/breeze").exists() || QDir("/usr/share/icons/breeze").exists()) {
            QIcon::setThemeName("breeze");
        }
    }

    // Set fallback window icon from embedded Qt resource or system icon theme
    QIcon appIcon = QIcon::fromTheme("bubezview", QIcon(":/icons/bubezview.png"));
    app.setWindowIcon(appIcon);

    // Initialize Localization (i18n)
    QSettings settings("Bubez", "BubezView");
    QString userLang = settings.value("language", "system").toString();
    QString localeName = (userLang == "system") ? QLocale::system().name() : userLang;
    QString langPrefix = localeName.left(2).toLower();

    QTranslator appTranslator;
    // 1. Try embedded Qt resources first
    if (appTranslator.load(QString(":/translations/bubezview_%1.qm").arg(langPrefix))) {
        app.installTranslator(&appTranslator);
    } else if (appTranslator.load(QString(":/translations/bubezview_%1.qm").arg(localeName))) {
        app.installTranslator(&appTranslator);
    } else {
        // 2. Fallback to filesystem search paths
        QStringList searchPaths = {
            QDir::homePath() + "/.local/share/BubezView/assets/translations",
            "/usr/share/bubezview/translations",
            QCoreApplication::applicationDirPath() + "/translations"
        };
        for (const QString &path : searchPaths) {
            if (appTranslator.load(QString("bubezview_%1").arg(langPrefix), path) ||
                appTranslator.load(QString("bubezview_%1").arg(localeName), path)) {
                app.installTranslator(&appTranslator);
                break;
            }
        }
    }

    // Load standard Qt system translations for common dialog buttons (e.g. Open, Cancel)
    QTranslator qtTranslator;
    if (qtTranslator.load(QString("qtbase_%1").arg(langPrefix), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Initialize application theme based on user settings (default: 100% Native)
    ThemeManager::applyTheme(&app, nullptr);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "BubezView: Instant universal native file viewer in C++"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption previewerOption(
        QStringList() << "previewer",
        QCoreApplication::translate("main", "Run as DBus previewer daemon for Nemo / Nautilus (org.nemo.Preview).")
    );
    parser.addOption(previewerOption);

    QCommandLineOption screenshotOption(
        "screenshot",
        QCoreApplication::translate("main", "Capture a preview screenshot and save it to the specified file."),
        "output_path"
    );
    parser.addOption(screenshotOption);

    QCommandLineOption debugOption(
        QStringList() << "d" << "debug",
        QCoreApplication::translate("main", "Enable verbose debug and diagnostic console output.")
    );
    parser.addOption(debugOption);

    QCommandLineOption wizardOption(
        QStringList() << "setup" << "wizard",
        QCoreApplication::translate("main", "Force open the initial setup wizard.")
    );
    parser.addOption(wizardOption);

    QCommandLineOption settingsSnapshotsOption(
        "settings-snapshots",
        QCoreApplication::translate("main", "Generate snapshots of all settings tabs and exit."),
        "directory"
    );
    parser.addOption(settingsSnapshotsOption);

    parser.addPositionalArgument("file", QCoreApplication::translate("main", "File or folder to preview (optional)"));

    parser.process(app);

    // Fast-path single-instance D-Bus handoff:
    // If a BubezView instance is already running (e.g. in RAM standby), forward the target file
    // over D-Bus and exit immediately (< 5 ms) so Dolphin/KIO launcher jobs complete cleanly.
    const QStringList fastPathArgs = parser.positionalArguments();
    if (!fastPathArgs.isEmpty() &&
        !parser.isSet(previewerOption) &&
        !parser.isSet(screenshotOption) &&
        !parser.isSet(wizardOption) &&
        !parser.isSet(settingsSnapshotsOption)) {
        QDBusConnection existingBus = QDBusConnection::sessionBus();
        if (existingBus.isConnected() && existingBus.interface() &&
            existingBus.interface()->isServiceRegistered(QStringLiteral("org.bubez.BubezView"))) {
            QDBusInterface iface(QStringLiteral("org.bubez.BubezView"),
                                 QStringLiteral("/org/bubez/BubezView"),
                                 QStringLiteral("org.nemo.Preview"),
                                 existingBus);
            if (iface.isValid()) {
                const QString absPath = QFileInfo(fastPathArgs.first()).absoluteFilePath();
                const QString fileUri = QUrl::fromLocalFile(absPath).toString();
                const QString activationToken = qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
                QDBusReply<void> reply = iface.call(QStringLiteral("ShowFileWithToken"), fileUri, activationToken);
                if (!reply.isValid()) {
                    // Fallback for older running standby instances exposing only ShowFile
                    reply = iface.call(QStringLiteral("ShowFile"), fileUri, 0, false);
                }
                if (reply.isValid()) {
                    return 0;
                }
            }
        }
    }

    // Configure logging verbosity if debug mode was requested via CLI or saved settings
    const bool debugMode = parser.isSet(debugOption) || settings.value("debug_mode", false).toBool();
    if (debugMode) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\nqt.*.debug=false"));

        // Setup log file in cache directory: ~/.cache/BubezView/bubezview.log
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (logDir.isEmpty()) {
            logDir = QDir::homePath() + "/.cache/BubezView";
        }
        QDir().mkpath(logDir);
        const QString logFilePath = logDir + "/bubezview.log";

        s_debugLogFile = std::make_unique<QFile>(logFilePath);
        if (s_debugLogFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            s_debugLogStream = std::make_unique<QTextStream>(s_debugLogFile.get());
        }

        qInstallMessageHandler(bubezLogMessageHandler);
        qDebug() << "[BubezView] Debug mode active. Live log streaming to console and:" << logFilePath;
    }

    // Discover and load modular plugins (.so)
    PluginManager::instance().discoverPlugins();

    if (parser.isSet(settingsSnapshotsOption)) {
        QString outDir = parser.value(settingsSnapshotsOption);
        QDir().mkpath(outDir);

        static QTranslator itTranslator;
        if (itTranslator.load(QStringLiteral(":/translations/bubezview_it.qm")) ||
            itTranslator.load(QStringLiteral("/home/davide/Progetti/BubezView/build_deb/translations/bubezview_it.qm"))) {
            app.installTranslator(&itTranslator);
        }

        auto captureTabsForPreset = [&](ThemePreset preset, const QString &suffix) {
            ThemeManager::setPreset(preset);
            ThemeManager::applyTheme(&app, nullptr);

            for (int i = 0; i < 5; ++i) {
                SettingsDialog dlg(nullptr, static_cast<SettingsDialog::TabIndex>(i));
                dlg.show();
                for (int w = 0; w < 10; ++w) {
                    app.processEvents();
                    QThread::msleep(20);
                }
                QPixmap pix = dlg.grab();
                pix.save(QString("%1/settings_tab_%2_%3.png").arg(outDir, QString::number(i), suffix));

                if (i == 3 && suffix == "zinc" && dlg.extensionsTab()) {
                    QStringList otherCats = {QStringLiteral("Archive"), QStringLiteral("Media"), QStringLiteral("Office"), QStringLiteral("Image")};
                    QStringList otherFiles = {QStringLiteral("archivi"), QStringLiteral("media"), QStringLiteral("ufficio"), QStringLiteral("grafica")};
                    for (int c = 0; c < otherCats.size(); ++c) {
                        dlg.extensionsTab()->selectCategory(otherCats[c]);
                        for (int w = 0; w < 10; ++w) {
                            app.processEvents();
                            QThread::msleep(20);
                        }
                        QPixmap cpix = dlg.grab();
                        cpix.save(QString("%1/settings_tab_3_categoria_%2.png").arg(outDir, otherFiles[c]));
                    }
                    dlg.extensionsTab()->selectCategory(QStringLiteral("PDF"));
                }
            }
        };

        captureTabsForPreset(ThemePreset::ZincGray, "zinc");
        captureTabsForPreset(ThemePreset::DarkSlate, "dark");
        captureTabsForPreset(ThemePreset::ModernLight, "light");

        // Capture Compiler Wizard dialog showing cleaned-up 8 flags
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            CompilerWizardDialog wizard;
            wizard.resize(840, 680);
            wizard.applyPreset(3);
            wizard.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixW = wizard.grab();
            pixW.save(QString("%1/compiler_wizard_dialog.png").arg(outDir));
        }

        // Capture Settings Tab 2 with System Packages selected (Wizard disabled)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            SettingsDialog dlg(nullptr, SettingsDialog::TabIndex::Performance);
            auto *perfTab = dlg.findChild<SettingsPerformanceTab*>();
            if (perfTab) {
                auto *combo = perfTab->findChild<QComboBox*>();
                if (combo) {
                    combo->setCurrentIndex(0); // Pacchetti di sistema (Nessuna compilazione)
                }
            }
            dlg.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixSys = dlg.grab();
            pixSys.save(QString("%1/settings_tab_2_system_packages.png").arg(outDir));
        }

        // Capture Settings Tab 3 with Category 1 (Documents / PDF) selected
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            SettingsDialog dlg(nullptr, SettingsDialog::TabIndex::Extensions);
            auto *catTabs = dlg.findChild<QTabWidget*>("ExtensionCategoryTabs");
            if (catTabs && catTabs->count() > 1) {
                catTabs->setCurrentIndex(1); // Documenti (PDF)
            }
            dlg.show();
            for (int w = 0; w < 12; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixDocs = dlg.grab();
            pixDocs.save(QString("%1/settings_tab_3_category_documents.png").arg(outDir));
        }

        // Capture Diagnostic Report Dialog - Component Health Check (Runtime Mode, No Dev Warnings)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            DiagnosticReportDialog diagDlg;
            QList<QPair<QString, bool>> subRes = {
                {QString::fromUtf8("Supporto Font Asiatici (poppler-data)"), true},
                {QString::fromUtf8("Estrattore Immagini (pdfimages)"), true}
            };
            diagDlg.setRuntimeReportData(
                QString::fromUtf8("Visualizzatore PDF"),
                true,
                QStringLiteral("pkg-config --modversion poppler-qt6 2>/dev/null || echo 'poppler-qt6 present'"),
                QStringLiteral("22.12.0\nPoppler Qt6 runtime engine active and operational.\nAll vector and text rendering hooks verified."),
                subRes,
                0
            );
            diagDlg.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixDiagSingle = diagDlg.grab();
            pixDiagSingle.save(QString("%1/diagnostic_dialog_single_zinc.png").arg(outDir));
        }

        // Capture Diagnostic Report Dialog - Build Prerequisites Check
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            DiagnosticReportDialog diagBuild;
            QStringList missing = {QStringLiteral("libpoppler-qt6-dev")};
            QString buildLog = QString::fromUtf8(
                "=== Visualizzatore PDF: Build Prerequisites Inspection ===\n"
                "✓ Build System: CMake detected.\n"
                "✓ Compiler Toolchain: C++ compiler (g++/clang++) detected.\n"
                "✓ Package Locator: pkg-config detected.\n"
                "❌ C/C++ Headers: 'poppler-qt6' development headers NOT found.\n"
                "   -> Install via package manager: apt install libpoppler-qt6-dev\n"
                "✓ Source Management: git detected."
            );
            diagBuild.setBuildReadinessReportData(
                QString::fromUtf8("Visualizzatore PDF"),
                false,
                missing,
                buildLog
            );
            diagBuild.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixDiagBuild = diagBuild.grab();
            pixDiagBuild.save(QString("%1/diagnostic_dialog_build_prerequisites.png").arg(outDir));
        }

        // Capture Diagnostic Report Dialog - Global 3-Section System Audit (ZincGray)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            DiagnosticReportDialog diagGlobal;
            QStringList failures;
            QStringList missingResources = {QString::fromUtf8("Dizionario Italiano OCR (tesseract-ocr-ita)")};
            QStringList readyEngines = {QString::fromUtf8("Visualizzatore Archivi (LibArchive)")};
            QStringList blockedEngines = {
                QString::fromUtf8("Visualizzatore PDF (mancano: libpoppler-qt6-dev)"),
                QString::fromUtf8("Media Player (mancano: libmpv-dev)")
            };
            QString fullReport = QString::fromUtf8(
                "BUBEZVIEW — STATO COMPONENTI E SISTEMA\n"
                "Data: 2026-11-14 21:05\n\n"
                "[MODULI RUNTIME] (12/12 Operativi)\n"
                "• Visualizzatore PDF: OPERATIVO (22.12.0)\n"
                "• Estrazione Immagini: OPERATIVO (pdfimages 22.12.0)\n"
                "• Supporto Font Asiatici: OPERATIVO (/usr/share/poppler/cMap)\n"
                "• Linearizzazione & Ottimizzazione: OPERATIVO (qpdf 11.3.0)\n"
                "• Riconoscimento Testo OCR: OPERATIVO (tesseract 5.3.0)\n"
                "• Firma Digitale & Crittografia: OPERATIVO (OpenSSL 3.0.15)\n"
                "• Gestione Archivi: OPERATIVO (libarchive 3.7.4)\n"
                "• Compressione Zstandard: OPERATIVO (zstd v1.5.4)\n"
                "• Compressione XZ: OPERATIVO (xz 5.4.1)\n"
                "• Elaboratore Audio DSP: OPERATIVO (FFTW3 DSP Engine)\n"
                "• Metadati Audio: OPERATIVO (TagLib Engine)\n"
                "• Interfaccia Grafica Nativa: OPERATIVO (Qt6 GUI)\n\n"
                "[COMPONENTI NON INSTALLATI & ACCESSORI]\n"
                "Non installati (2):\n"
                "  • 7-Zip (7zz)\n"
                "  • MuPDF\n\n"
                "Accessori mancanti (1):\n"
                "  • Dizionario Italiano OCR (tesseract-ocr-ita)\n\n"
                "[SEZIONE 3] IDONEITÀ AMBIENTE DI COMPILAZIONE (TUTTI I MOTORI)\n"
                "Motori pronti a compilare (1):\n"
                "  • Visualizzatore Archivi (LibArchive)\n\n"
                "Motori con dipendenze di build mancanti (2):\n"
                "  • Visualizzatore PDF (mancano: libpoppler-qt6-dev)\n"
                "  • Media Player (mancano: libmpv-dev)\n\n"
                "================================================================================\n"
            );
            diagGlobal.setGlobalExtendedReportData(12, 12, failures, missingResources, readyEngines, blockedEngines, fullReport);
            diagGlobal.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixDiagGlobal = diagGlobal.grab();
            pixDiagGlobal.save(QString("%1/diagnostic_dialog_global_zinc.png").arg(outDir));
        }

        // Capture Diagnostic Report Dialog - Copied to Clipboard visual feedback
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            DiagnosticReportDialog diagCopied;
            diagCopied.setReportData(
                QString::fromUtf8("Resoconto diagnostico - Visualizzatore PDF"),
                QString::fromUtf8("Visualizzatore PDF"),
                true,
                QStringLiteral("pkg-config --modversion poppler-qt6 2>/dev/null || echo 'poppler-qt6 present'"),
                QStringLiteral("22.12.0"),
                0
            );
            // Simulate copy click
            auto *btnCopy = diagCopied.findChild<QPushButton*>();
            if (btnCopy) {
                btnCopy->click();
            }
            diagCopied.show();
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            QPixmap pixCopied = diagCopied.grab();
            pixCopied.save(QString("%1/diagnostic_dialog_copied_feedback.png").arg(outDir));
        }

        // Capture FirstRunWizard 4 Pages (ZincGray Theme)
        {
            qputenv("XDG_CURRENT_DESKTOP", "KDE");
            ThemeManager::setPreset(ThemePreset::ZincGray);
            FirstRunWizard wizard;
            wizard.resize(750, 510);
            wizard.show();
            QStringList pageNames = {
                QStringLiteral("wizard_page_0_welcome.png"),
                QStringLiteral("wizard_page_1_spacebar.png"),
                QStringLiteral("wizard_page_2_performance.png"),
                QStringLiteral("wizard_page_3_summary.png")
            };
            for (int p = 0; p < pageNames.size(); ++p) {
                for (int w = 0; w < 10; ++w) {
                    app.processEvents();
                    QThread::msleep(20);
                }
                QPixmap pixW = wizard.grab();
                pixW.save(QString("%1/%2").arg(outDir, pageNames[p]));
                if (p < pageNames.size() - 1) {
                    wizard.next();
                }
            }
            wizard.close();

            // Capture Page 0 in Modern Light theme
            {
                ThemeManager::setPreset(ThemePreset::ModernLight);
                FirstRunWizard wizardLight;
                wizardLight.resize(750, 510);
                wizardLight.show();
                for (int w = 0; w < 10; ++w) {
                    app.processEvents();
                    QThread::msleep(20);
                }
                wizardLight.grab().save(QString("%1/wizard_page_0_welcome_light.png").arg(outDir));
                wizardLight.close();
            }

            // Also capture Page 1 in unsupported desktop environment to show the guide/fallback
            qputenv("XDG_CURRENT_DESKTOP", "XFCE");
            FirstRunWizard wizardFallback;
            wizardFallback.resize(750, 510);
            wizardFallback.show();
            wizardFallback.next(); // Go to Page 1
            for (int w = 0; w < 10; ++w) {
                app.processEvents();
                QThread::msleep(20);
            }
            wizardFallback.grab().save(QString("%1/wizard_page_1_spacebar_fallback.png").arg(outDir));
            wizardFallback.close();
        }

        // Capture Share Dialog (Zinc Theme)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            ShareDialog shareDlg(QStringLiteral("/tmp/sample_photo.jpg"));
            shareDlg.show();
            for (int w = 0; w < 10; ++w) { app.processEvents(); QThread::msleep(20); }
            QPixmap pix = shareDlg.grab();
            pix.save(QString("%1/dialog_share_zinc.png").arg(outDir));
            shareDlg.close();
        }

        // Capture Image Resize Dialog (Zinc Theme)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            ImageResizeDialog resizeDlg(3840, 2160);
            resizeDlg.show();
            for (int w = 0; w < 10; ++w) { app.processEvents(); QThread::msleep(20); }
            QPixmap pix = resizeDlg.grab();
            pix.save(QString("%1/dialog_resize_zinc.png").arg(outDir));
            resizeDlg.close();
        }

        // Capture Goto Folder Dialog (Zinc Theme)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            GotoFolderDialog gotoDlg(QStringLiteral("/home/davide/Documents"));
            gotoDlg.show();
            for (int w = 0; w < 10; ++w) { app.processEvents(); QThread::msleep(20); }
            QPixmap pix = gotoDlg.grab();
            pix.save(QString("%1/dialog_goto_folder_zinc.png").arg(outDir));
            gotoDlg.close();
        }

        // Capture PDF Repair Dialog (Zinc Theme)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            PdfRepairDialog repairDlg(QStringLiteral("/home/user/Documents/Damaged_Guide.pdf"));
            repairDlg.show();
            for (int w = 0; w < 10; ++w) { app.processEvents(); QThread::msleep(20); }
            QPixmap pix = repairDlg.grab();
            pix.save(QString("%1/dialog_pdf_repair_zinc.png").arg(outDir));
            repairDlg.close();
        }

        // Capture OCR Dialog (Zinc Theme)
        {
            ThemeManager::setPreset(ThemePreset::ZincGray);
            OcrDialog ocrDlg(QString::fromUtf8("Sample Recognized Document Text\nBubezView OCR Engine"),
                             QStringLiteral("Document.pdf"),
                             QStringLiteral("/home/user/Document.pdf"),
                             true, 0, 1, nullptr);
            ocrDlg.show();
            for (int w = 0; w < 10; ++w) { app.processEvents(); QThread::msleep(20); }
            QPixmap pix = ocrDlg.grab();
            pix.save(QString("%1/dialog_ocr_zinc.png").arg(outDir));
            ocrDlg.close();
        }

        // Restore default
        ThemeManager::setPreset(ThemePreset::DarkSlate);
        return 0;
    }

    MainWindow window;
    window.setDaemonMode(parser.isSet(previewerOption));
    DBusPreviewer dbusPreviewer(&window);

    // Register DBus services so spacebar in Dolphin/Nemo/Nautilus can invoke BubezView instantaneously
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        sessionBus.registerService("org.bubez.BubezView");
        sessionBus.registerService("org.nemo.Preview");
        sessionBus.registerService("org.gnome.NautilusPreviewer");
        sessionBus.registerObject("/org/bubez/BubezView", &dbusPreviewer, QDBusConnection::ExportAllSlots);
        sessionBus.registerObject("/org/nemo/Preview", &dbusPreviewer, QDBusConnection::ExportAllSlots);
        sessionBus.registerObject("/org/gnome/NautilusPreviewer", &dbusPreviewer,
                                  QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllSlots);
    }

    if (parser.isSet(previewerOption)) {
        // Run as DBus daemon listener waiting for ShowFile calls
        app.setQuitOnLastWindowClosed(false);
        return app.exec();
    }

    // Check if first-run setup wizard should be displayed
    bool firstRunCompleted = settings.value("first_run_completed", false).toBool();
    if ((!firstRunCompleted || parser.isSet(wizardOption)) && !parser.isSet(screenshotOption)) {
        FirstRunWizard wizard(&window);
        if (wizard.exec() == QDialog::Accepted) {
            window.applyCurrentTheme();

            const QStringList pendingBuild = wizard.pendingBuildComponents();
            if (!pendingBuild.isEmpty()) {
                BuildConsoleDialog console(&window);
                console.startBuildComponents(pendingBuild);
                console.exec();
            }
        }
    }

    const QStringList args = parser.positionalArguments();

    if (!args.isEmpty()) {
        QString targetFile = args.first();
        MainWindow::OpenResult result = window.openFile(targetFile);
        if (result == MainWindow::OpenResult::DelegatedToPlugin) {
            // Handled by plugin (e.g. Python module); exit lightweight C++ app cleanly
            return 0;
        }
        window.show();

        if (parser.isSet(screenshotOption)) {
            // Allow asynchronous workers and rendering to complete
            for (int i = 0; i < 20; ++i) {
                app.processEvents();
                QThread::msleep(25);
            }
            QPixmap pix = window.grab();
            pix.save(parser.value(screenshotOption));
            return 0;
        }
    } else {
        // Open file picker if launched without arguments
        QString selected = QFileDialog::getOpenFileName(
            &window,
            QObject::tr("Select a file to open"),
            QDir::homePath()
        );
        if (!selected.isEmpty()) {
            MainWindow::OpenResult result = window.openFile(selected);
            if (result == MainWindow::OpenResult::DelegatedToPlugin) {
                return 0;
            }
            window.show();
        } else {
            return 0;
        }
    }

    return app.exec();
}
