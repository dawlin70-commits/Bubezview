#include "mainwindow.h"
#include "dbus_previewer.h"
#include "theme_manager.h"
#include "first_run_wizard.h"
#include "plugin_manager.h"
#include "gpu_acceleration_manager.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QStyleFactory>
#include <QDBusConnection>
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

int main(int argc, char *argv[])
{
    // libmpv requires LC_NUMERIC to be set to "C" for proper decimal parsing
    std::setlocale(LC_NUMERIC, "C");

    // Force pure Qt internal dialogs to guarantee full isolation from KDE KIO workers
    // and prevent Plasma 6 Wayland selection/shortcut freeze in Dolphin.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);

    // Configure audio playback buffer latency on Linux/PipeWire/PulseAudio
    // to prevent underruns/dropouts during UI activity or intensive tasks.
    if (!qEnvironmentVariableIsSet("PULSE_LATENCY_MSEC")) {
        qputenv("PULSE_LATENCY_MSEC", "200");
    }

#ifndef BUBEZVIEW_VERSION
#define BUBEZVIEW_VERSION "0.1.0"
#endif

    // Setup high-performance OpenGL surface format (VSync, DoubleBuffering, 4x MSAA)
    GpuAccelerationManager::setupSurfaceFormat();

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

    // Initialize GPU acceleration detection and driver capabilities
    GpuAccelerationManager::instance().initialize();

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

    parser.addPositionalArgument("file", QCoreApplication::translate("main", "File or folder to preview (optional)"));

    parser.process(app);

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

    MainWindow window;
    window.setDaemonMode(parser.isSet(previewerOption));
    ThemeManager::applyTheme(&app, &window);
    DBusPreviewer dbusPreviewer(&window);

    // Register DBus services so spacebar in Nemo/Nautilus can invoke BubezView
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        sessionBus.registerService("org.nemo.Preview");
        sessionBus.registerService("org.gnome.NautilusPreviewer");
        sessionBus.registerObject("/org/nemo/Preview", &dbusPreviewer, QDBusConnection::ExportAllSlots);
        sessionBus.registerObject("/org/gnome/NautilusPreviewer", &dbusPreviewer, QDBusConnection::ExportAllSlots);
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
            ThemeManager::applyTheme(&app, &window);
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
