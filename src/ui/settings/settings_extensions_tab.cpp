/**
 * @file settings_extensions_tab.cpp
 * @brief Implementation of the extensions catalog settings tab.
 */

#include "settings_extensions_tab.h"
#include "extension_feature_model.h"
#include "extension_row_widget.h"
#include "diagnostic_report_dialog.h"
#include "theme_manager.h"
#include "build_config.h"
#include "plugin_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QProcess>
#include <QDateTime>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

SettingsExtensionsTab::SettingsExtensionsTab(QWidget *parent)
    : QWidget(parent)
    , m_activeCategory(QStringLiteral("Studio"))
{
    setupUi();
}

void SettingsExtensionsTab::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(14, 10, 14, 10);

    // =========================================================================
    // Header Row: Category Capsules Filter Bar + Diagnostic Health Check Button
    // =========================================================================
    QHBoxLayout *headerLay = new QHBoxLayout();
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(10);

    m_categoryCapsulesFrame = new QFrame(this);
    m_categoryCapsulesFrame->setObjectName(QStringLiteral("CategoryCapsulesFrame"));
    QHBoxLayout *capsulesLay = new QHBoxLayout(m_categoryCapsulesFrame);
    capsulesLay->setContentsMargins(3, 3, 3, 3);
    capsulesLay->setSpacing(2);

    auto pdfMods = ExtensionFeatureRegistry::modulesForCategory(QStringLiteral("PDF"));
    auto archMods = ExtensionFeatureRegistry::modulesForCategory(QStringLiteral("Archive"));
    auto mediaMods = ExtensionFeatureRegistry::modulesForCategory(QStringLiteral("Media"));
    auto officeMods = ExtensionFeatureRegistry::modulesForCategory(QStringLiteral("Office"));
    auto imageMods = ExtensionFeatureRegistry::modulesForCategory(QStringLiteral("Image"));

    struct CatDef {
        QString id;
        QString label;
    };
    QList<CatDef> categories = {
        {QStringLiteral("Studio"), tr("Studio (5)")},
        {QStringLiteral("PDF"), tr("Documents (%1)").arg(pdfMods.size())},
        {QStringLiteral("Archive"), tr("Archives (%1)").arg(archMods.size())},
        {QStringLiteral("Media"), tr("Audio && Video (%1)").arg(mediaMods.size())},
        {QStringLiteral("Office"), tr("Office (%1)").arg(officeMods.size())},
        {QStringLiteral("Image"), tr("Graphics (%1)").arg(imageMods.size())}
    };

    for (const auto &cat : categories) {
        QPushButton *btn = new QPushButton(cat.label, m_categoryCapsulesFrame);
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        const QString cid = cat.id;
        connect(btn, &QPushButton::clicked, this, [this, cid]() {
            onCategoryCapsuleClicked(cid);
        });
        capsulesLay->addWidget(btn);
        m_catButtons.insert(cat.id, btn);
    }

    headerLay->addWidget(m_categoryCapsulesFrame);
    headerLay->addStretch();

    m_btnGlobalDiag = new QPushButton(tr("🩺 Verify Components..."), this);
    m_btnGlobalDiag->setCursor(Qt::PointingHandCursor);
    m_btnGlobalDiag->setFixedHeight(30);
    m_btnGlobalDiag->setToolTip(tr("Checks all installed engines and verifies that external tools respond correctly."));
    connect(m_btnGlobalDiag, &QPushButton::clicked, this, &SettingsExtensionsTab::runGlobalDiagnostics);
    headerLay->addWidget(m_btnGlobalDiag);

    layout->addLayout(headerLay);

    // =========================================================================
    // Active Category 2-Column Grid (QGroupBox Liquid Glass Card)
    // =========================================================================
    m_grpActiveCategory = new QGroupBox(this);
    m_grpActiveCategory->setObjectName(QStringLiteral("CategoryCard"));

    m_gridLayout = new QGridLayout(m_grpActiveCategory);
    m_gridLayout->setContentsMargins(10, 10, 10, 8);
    m_gridLayout->setSpacing(8);

    layout->addWidget(m_grpActiveCategory);
    layout->addStretch(1);

    populateCategoryGrid(m_activeCategory);
    updateThemeStyles();
}

void SettingsExtensionsTab::onCategoryCapsuleClicked(const QString &catId)
{
    if (m_activeCategory == catId) return;
    populateCategoryGrid(catId);
}

void SettingsExtensionsTab::selectCategory(const QString &catId)
{
    onCategoryCapsuleClicked(catId);
}

void SettingsExtensionsTab::populateCategoryGrid(const QString &catId)
{
    m_activeCategory = catId;

    // Immediately remove and delete old tile widgets to prevent overlap
    const auto oldTiles = m_grpActiveCategory->findChildren<ExtensionRowWidget*>();
    for (ExtensionRowWidget *tile : oldTiles) {
        m_gridLayout->removeWidget(tile);
        delete tile;
    }
    QLayoutItem *child;
    while ((child = m_gridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // Set Card title based on category
    if (catId == QLatin1String("Studio")) {
        m_grpActiveCategory->setTitle(tr("Modular Edit Studio Plugins (.so) — Install or Uninstall Independently"));

        struct StudioDef {
            QString id;
            QString soFile;
            QString name;
            QString desc;
        };
        const QList<StudioDef> studios = {
            {QStringLiteral("image"),  QStringLiteral("bubez_image.so"),  tr("Image Studio"),  tr("Auto-enhance, light & color grading, presets, sharpen, blur, vignette.")},
            {QStringLiteral("folder"), QStringLiteral("bubez_folder.so"), tr("Folder Studio"), tr("Visual duplicates, PhotoRec recovery, tree organizer, batch rename & cleanup.")},
            {QStringLiteral("pdf"),    QStringLiteral("bubez_pdf.so"),    tr("PDF Studio"),    tr("Annotations, drawing, digital signatures, watermarks, page organizer & merge.")},
            {QStringLiteral("audio"),  QStringLiteral("bubez_audio.so"),  tr("Audio Studio"),  tr("MPV 64-bit audio, waveform cutter, filters, pitch, equalizer & tag editor.")},
            {QStringLiteral("video"),  QStringLiteral("bubez_video.so"),  tr("Video Studio"),  tr("Lossless trim/cut, frame snapshot, audio extraction, rotate, scale & speed.")}
        };

        auto findPluginFile = [](const QString &soFile) -> QString {
            const QString home = QDir::homePath();
            const QString appDir = QCoreApplication::applicationDirPath();
            const QString devRoot = BuildConfig::projectDevRoot();
            QStringList dirs = {
                home + QStringLiteral("/.local/share/bubezview/plugins"),
                home + QStringLiteral("/.local/lib/bubezview/plugins"),
                home + QStringLiteral("/.local/lib/x86_64-linux-gnu/bubezview/plugins"),
                home + QStringLiteral("/.local/lib64/bubezview/plugins"),
                appDir + QStringLiteral("/plugins")
            };
            if (!devRoot.isEmpty()) {
                dirs << (devRoot + QStringLiteral("/build_install/plugins"))
                     << (devRoot + QStringLiteral("/build_deb/plugins"));
            }
            for (const QString &d : dirs) {
                const QString p = d + QLatin1Char('/') + soFile;
                if (QFileInfo::exists(p)) return p;
            }
            return QString();
        };

        const bool isDark = ThemeManager::isDark(ThemeManager::effectivePreset(ThemeManager::currentPreset()));
        const QString tileBg = isDark ? QStringLiteral("rgba(255, 255, 255, 0.04)") : QStringLiteral("rgba(15, 23, 42, 0.035)");
        const QString tileBorder = isDark ? QStringLiteral("rgba(255, 255, 255, 0.10)") : QStringLiteral("rgba(15, 23, 42, 0.12)");
        const QString subCol = isDark ? QStringLiteral("#94a3b8") : QStringLiteral("#64748b");

        for (int i = 0; i < studios.size(); ++i) {
            const StudioDef &st = studios.at(i);
            const QString foundPath = findPluginFile(st.soFile);
            const bool installed = !foundPath.isEmpty() || PluginManager::instance().isStudioModuleLoaded(st.id);

            auto *card = new QFrame(m_grpActiveCategory);
            card->setStyleSheet(QStringLiteral(
                "QFrame { background-color: %1; border: 1px solid %2; border-radius: 10px; }").arg(tileBg, tileBorder));
            auto *cLay = new QVBoxLayout(card);
            cLay->setContentsMargins(12, 10, 12, 10);
            cLay->setSpacing(6);

            auto *topRow = new QHBoxLayout();
            topRow->setSpacing(6);
            auto *lblName = new QLabel(QStringLiteral("<b>%1</b> <span style='color:%2; font-size:11px;'>(%3)</span>")
                                           .arg(st.name, subCol, st.soFile), card);
            lblName->setStyleSheet(QStringLiteral("background: transparent; border: none; font-size: 12.5px;"));
            topRow->addWidget(lblName, 1);

            QString statusTxt = tr("Not installed");
            QString badgeCss = QStringLiteral(
                "background: rgba(148, 163, 184, 0.18); color: %1; border: none; border-radius: 6px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;")
                .arg(subCol);
            if (installed) {
                const qint64 kb = foundPath.isEmpty() ? 0 : qMax<qint64>(1, QFileInfo(foundPath).size() / 1024);
                statusTxt = kb > 0 ? tr("Active (%1 KB)").arg(kb) : tr("Active");
                badgeCss = QStringLiteral(
                    "background: rgba(34, 197, 94, 0.16); color: #22c55e; border: 1px solid rgba(34, 197, 94, 0.35); border-radius: 6px; padding: 2px 8px; font-size: 10.5px; font-weight: 700;");
            }
            auto *lblStatus = new QLabel(statusTxt, card);
            lblStatus->setStyleSheet(badgeCss);
            topRow->addWidget(lblStatus);
            cLay->addLayout(topRow);

            auto *btnRow = new QHBoxLayout();
            btnRow->setSpacing(6);
            btnRow->addStretch(1);
            const QString modId = st.id;
            const QString modName = st.name;
            const QString modSo = st.soFile;
            const QString modDesc = st.desc;

            // Compact Info button opening a popup with full module details instead of clipping text inside the tile
            auto *btnInfo = new QPushButton(QStringLiteral("i"), card);
            btnInfo->setCursor(Qt::PointingHandCursor);
            btnInfo->setFixedSize(26, 26);
            btnInfo->setToolTip(tr("Module information"));
            btnInfo->setStyleSheet(QStringLiteral(
                "QPushButton { background: rgba(148, 163, 184, 0.16); color: %1; border: 1px solid %2; border-radius: 13px; font-weight: 700; font-size: 12px; padding: 0px; }"
                "QPushButton:hover { background: rgba(56, 189, 248, 0.22); color: #38bdf8; border-color: rgba(56, 189, 248, 0.45); }")
                .arg(subCol, tileBorder));
            connect(btnInfo, &QPushButton::clicked, this, [this, modName, modSo, modDesc, installed, foundPath]() {
                const QString statusLine = installed
                    ? (foundPath.isEmpty()
                           ? tr("Installed and active")
                           : tr("Installed (%1 KB)\n%2").arg(qMax<qint64>(1, QFileInfo(foundPath).size() / 1024)).arg(foundPath))
                    : tr("Not installed");
                QMessageBox::information(
                    this,
                    modName,
                    tr("<b>%1</b> (<code>%2</code>)<br><br>%3<br><br><b>Status:</b> %4")
                        .arg(modName.toHtmlEscaped(), modSo.toHtmlEscaped(),
                             modDesc.toHtmlEscaped(), statusLine.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
            });
            btnRow->addWidget(btnInfo);

            if (installed) {
                auto *btnRemove = new QPushButton(tr("Uninstall"), card);
                btnRemove->setCursor(Qt::PointingHandCursor);
                btnRemove->setFixedHeight(26);
                btnRemove->setStyleSheet(QStringLiteral(
                    "QPushButton { background: rgba(239, 68, 68, 0.16); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.35); border-radius: 7px; padding: 0px 12px; font-weight: 600; font-size: 11px; }"
                    "QPushButton:hover { background: #ef4444; color: #ffffff; }"));
                connect(btnRemove, &QPushButton::clicked, this, [this, modId, modName, modSo]() {
                    const auto reply = QMessageBox::question(
                        this,
                        tr("Uninstall Studio Module"),
                        tr("Remove %1 (%2)?\nYou can re-activate it anytime by clicking Edit.")
                            .arg(modName, modSo),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (reply == QMessageBox::Yes) {
                        PluginManager::instance().uninstallStudioModule(modId);
                        populateCategoryGrid(QStringLiteral("Studio"));
                        emit pendingStateChanged();
                    }
                });
                btnRow->addWidget(btnRemove);
            } else {
                auto *btnActivate = new QPushButton(tr("Activate"), card);
                btnActivate->setCursor(Qt::PointingHandCursor);
                btnActivate->setFixedHeight(26);
                btnActivate->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #0284c7; color: #ffffff; border: none; border-radius: 7px; padding: 0px 12px; font-weight: 600; font-size: 11px; }"
                    "QPushButton:hover { background: #0369a1; }"));
                connect(btnActivate, &QPushButton::clicked, this, [this, modId]() {
                    if (PluginManager::instance().activateStudioModuleInteractive(modId, this)) {
                        populateCategoryGrid(QStringLiteral("Studio"));
                        emit pendingStateChanged();
                    }
                });
                btnRow->addWidget(btnActivate);
            }
            cLay->addLayout(btnRow);

            m_gridLayout->addWidget(card, i / 2, i % 2);
        }

        updateCapsuleSelection();
        updateThemeStyles();
        return;
    } else if (catId == QLatin1String("PDF")) {
        m_grpActiveCategory->setTitle(tr("Documents (PDF && Text)"));
    } else if (catId == QLatin1String("Archive")) {
        m_grpActiveCategory->setTitle(tr("Archives && Compression"));
    } else if (catId == QLatin1String("Media")) {
        m_grpActiveCategory->setTitle(tr("Audio && Video"));
    } else if (catId == QLatin1String("Office")) {
        m_grpActiveCategory->setTitle(tr("Office Documents"));
    } else if (catId == QLatin1String("Image")) {
        m_grpActiveCategory->setTitle(tr("Graphics && Extended Formats"));
    }

    // Retrieve modules for selected category
    QList<ExtensionModuleInfo> mods = ExtensionFeatureRegistry::modulesForCategory(catId);

    // Populate 2-column grid
    for (int i = 0; i < mods.size(); ++i) {
        ExtensionRowWidget *tile = new ExtensionRowWidget(mods[i], m_grpActiveCategory);
        connect(tile, &ExtensionRowWidget::statusChanged, this, [this]() {
            refreshAllRows();
            emit pendingStateChanged();
        });
        m_gridLayout->addWidget(tile, i / 2, i % 2);
    }

    updateCapsuleSelection();
    updateThemeStyles();
}

void SettingsExtensionsTab::updateCapsuleSelection()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    QString activeBg;
    QString activeBorder;
    QString activeText = QStringLiteral("#ffffff");

    if (active == ThemePreset::ZincGray) {
        activeBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        activeBorder = QStringLiteral("rgba(255, 255, 255, 0.25)");
    } else if (ThemeManager::isDarkTheme()) {
        activeBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        activeBorder = QStringLiteral("rgba(56, 189, 248, 0.45)");
    } else {
        activeBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        activeBorder = QStringLiteral("#0284c7");
    }

    for (auto it = m_catButtons.begin(); it != m_catButtons.end(); ++it) {
        QPushButton *btn = it.value();
        if (it.key() == m_activeCategory) {
            btn->setStyleSheet(QString(
                "QPushButton {"
                "    background: %1;"
                "    color: %2;"
                "    font-weight: bold;"
                "    font-size: 10.5px;"
                "    border: 1px solid %3;"
                "    border-radius: 11px;"
                "    padding: 0 9px;"
                "}"
            ).arg(activeBg, activeText, activeBorder));
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "    background: transparent;"
                "    color: #a1a1aa;"
                "    font-size: 10.5px;"
                "    font-weight: 500;"
                "    border: none;"
                "    border-radius: 11px;"
                "    padding: 0 7px;"
                "}"
                "QPushButton:hover {"
                "    background: rgba(255, 255, 255, 0.08);"
                "    color: #ffffff;"
                "}"
            );
        }
    }
}

void SettingsExtensionsTab::refreshAllRows()
{
    const auto rows = findChildren<ExtensionRowWidget*>();
    for (ExtensionRowWidget *row : rows) {
        row->refreshState();
    }
}

void SettingsExtensionsTab::runGlobalDiagnostics()
{
    const auto mods = ExtensionFeatureRegistry::allModules();

    int runtimeTotal = 0;
    int runtimeOk = 0;
    QStringList runtimeFailures;
    QStringList runtimeLog;

    QStringList uninstalledComponents;
    QStringList missingResources;

    QStringList buildReadyEngines;
    QStringList buildBlockedEngines;
    QStringList buildLog;

    // SEZIONE 1 & 2: Runtime scan and uninstalled/resource scan
    for (const auto &m : mods) {
        if (!m.isInstalled) {
            uninstalledComponents.append(m.displayName);
        } else {
            runtimeTotal++;
            QString cmd = m.diagnosticCommand.trimmed();
            if (cmd.isEmpty()) {
                runtimeOk++;
                runtimeLog.append(QString("• %1: OPERATIVO (verifica interna)").arg(m.displayName));
            } else {
                QProcess proc;
                proc.start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << cmd);
                if (proc.waitForFinished(2000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                    runtimeOk++;
                    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
                    runtimeLog.append(QString("• %1: OPERATIVO (%2)").arg(m.displayName, out.isEmpty() ? QStringLiteral("OK") : out));
                } else {
                    QString outErr = QString::fromUtf8(proc.readAllStandardOutput() + proc.readAllStandardError()).trimmed();
                    runtimeFailures.append(QString("%1 (codice %2)").arg(m.displayName).arg(proc.exitCode()));
                    runtimeLog.append(QString("• %1: ERRORE (codice %2) - %3").arg(m.displayName).arg(proc.exitCode()).arg(outErr));
                }
            }
        }

        // Check sub-dependencies across modules
        for (const auto &sub : m.subDependencies) {
            if (!sub.isInstalled) {
                missingResources.append(QString("%1 (%2)").arg(sub.name, m.displayName));
            }
        }

        // SEZIONE 3: Check build readiness for compilable engines
        if (m.canCompile) {
            QStringList missingList;
            QString inspectLog;
            bool ready = ExtensionFeatureRegistry::checkBuildReadiness(m, missingList, inspectLog);
            if (ready) {
                buildReadyEngines.append(m.displayName);
                buildLog.append(QString("• %1: PRONTO A COMPILARE (CMake, C++ e header dev presenti)").arg(m.displayName));
            } else {
                buildBlockedEngines.append(QString("%1 (mancano: %2)").arg(m.displayName, missingList.join(QStringLiteral(", "))));
                buildLog.append(QString("• %1: COMPILAZIONE BLOCCATA (mancano: %2)").arg(m.displayName, missingList.join(QStringLiteral(", "))));
            }
        }
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    QString fullReport = QString(
        "BUBEZVIEW — STATO COMPONENTI E SISTEMA\n"
        "Data: %1\n\n"
        "[MODULI RUNTIME] (%2/%3 Operativi)\n"
        "%4\n\n"
        "[COMPONENTI NON INSTALLATI & ACCESSORI]\n"
        "Non installati (%5):\n%6\n\n"
        "Accessori mancanti (%7):\n%8\n\n"
        "[AMBIENTE DI COMPILAZIONE]\n"
        "Pronti (%9): %10\n"
        "Header mancanti (%11): %12\n"
    ).arg(
        timestamp,
        QString::number(runtimeOk),
        QString::number(runtimeTotal),
        runtimeLog.join(QStringLiteral("\n")),
        QString::number(uninstalledComponents.count()),
        uninstalledComponents.isEmpty() ? QStringLiteral("  (tutti installati)") : QStringLiteral("  • ") + uninstalledComponents.join(QStringLiteral("\n  • ")),
        QString::number(missingResources.count()),
        missingResources.isEmpty() ? QStringLiteral("  (tutti presenti)") : QStringLiteral("  • ") + missingResources.join(QStringLiteral("\n  • ")),
        QString::number(buildReadyEngines.count()),
        buildReadyEngines.isEmpty() ? QStringLiteral("  (nessuno)") : buildReadyEngines.join(QStringLiteral(", ")),
        QString::number(buildBlockedEngines.count()),
        buildBlockedEngines.isEmpty() ? QStringLiteral("  (nessuno bloccato)") : buildBlockedEngines.join(QStringLiteral(", "))
    );

    DiagnosticReportDialog::showGlobalExtendedReport(
        this,
        runtimeOk,
        runtimeTotal,
        runtimeFailures,
        missingResources,
        buildReadyEngines,
        buildBlockedEngines,
        fullReport
    );
}

void SettingsExtensionsTab::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString textCol;
    QString btnDiagBg;
    QString btnDiagBorder;
    QString btnDiagText;
    QString frameBg;
    QString frameBorder;

    if (active == ThemePreset::ZincGray) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        textCol = QStringLiteral("#ffffff");

        btnDiagBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        btnDiagBorder = QStringLiteral("rgba(255, 255, 255, 0.20)");
        btnDiagText = QStringLiteral("#ffffff");

        frameBg = QStringLiteral("rgba(36, 37, 41, 0.85)");
        frameBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
    } else if (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.09)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#334155");
        textCol = QStringLiteral("#1e293b");

        btnDiagBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnDiagBorder = QStringLiteral("#0284c7");
        btnDiagText = QStringLiteral("#ffffff");

        frameBg = QStringLiteral("rgba(235, 239, 244, 0.85)");
        frameBorder = QStringLiteral("rgba(15, 23, 42, 0.10)");
    } else {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        textCol = QStringLiteral("#f8fafc");

        btnDiagBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1)");
        btnDiagBorder = QStringLiteral("rgba(56, 189, 248, 0.35)");
        btnDiagText = QStringLiteral("#ffffff");

        frameBg = QStringLiteral("rgba(15, 23, 42, 0.75)");
        frameBorder = QStringLiteral("rgba(56, 189, 248, 0.20)");
    }

    const QString boxStyle = QString(
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
    ).arg(textCol, cardBorder, cardBg, pillBg, pillColor, pillBorder);

    if (m_grpActiveCategory) {
        m_grpActiveCategory->setStyleSheet(boxStyle);
    }

    if (m_categoryCapsulesFrame) {
        m_categoryCapsulesFrame->setStyleSheet(QString(
            "QFrame#CategoryCapsulesFrame {"
            "    background: %1;"
            "    border: 1px solid %2;"
            "    border-radius: 14px;"
            "}"
        ).arg(frameBg, frameBorder));
    }

    if (m_btnGlobalDiag) {
        m_btnGlobalDiag->setStyleSheet(QString(
            "QPushButton {"
            "    background: %1;"
            "    color: %2;"
            "    border: 1px solid %3;"
            "    border-radius: 8px;"
            "    font-size: 11.5px;"
            "    font-weight: 600;"
            "    padding: 0 14px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #82838a;"
            "    border-color: rgba(255, 255, 255, 0.40);"
            "}"
        ).arg(btnDiagBg, btnDiagText, btnDiagBorder));
    }

    updateCapsuleSelection();

    // Refresh child tiles theme styling
    const auto rows = findChildren<ExtensionRowWidget*>();
    for (ExtensionRowWidget *row : rows) {
        row->updateThemeStyles();
    }
}
