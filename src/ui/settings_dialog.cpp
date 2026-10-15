/**
 * @file settings_dialog.cpp
 * @brief Implementation of the settings dialog coordinator.
 */

#include "settings_dialog.h"
#include "settings/settings_appearance_tab.h"
#include "settings/settings_integration_tab.h"
#include "settings/settings_performance_tab.h"
#include "settings/settings_extensions_tab.h"
#include "settings/settings_extra_tab.h"
#include "build_config.h"
#include "build_console_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent, TabIndex initialTab)
    : QDialog(parent)
{
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.load();
    m_initialPdfStatic = bcfg.staticPdf();
    m_initialArchiveStatic = bcfg.staticArchive();
    m_initialOfficeStatic = bcfg.staticOffice();
    m_initialEnableOffice = bcfg.enableInternalOffice();

    setupUi();
    setCurrentTab(initialTab);
    refreshStatuses();
    updatePendingBanner();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setCurrentTab(TabIndex tab)
{
    if (m_tabWidget) {
        m_tabWidget->setCurrentIndex(static_cast<int>(tab));
    }
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings — BubezView"));
    resize(820, 640);
    setMinimumSize(760, 560);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    m_tabWidget = new QTabWidget(this);

    m_appearanceTab = new SettingsAppearanceTab(this);
    connect(m_appearanceTab, &SettingsAppearanceTab::themeChanged, this, &SettingsDialog::themeChanged);
    m_tabWidget->addTab(m_appearanceTab, tr("Appearance"));

    m_integrationTab = new SettingsIntegrationTab(this);
    m_tabWidget->addTab(m_integrationTab, tr("Desktop Integration"));

    m_performanceTab = new SettingsPerformanceTab(this);
    connect(m_performanceTab, &SettingsPerformanceTab::pendingStateChanged, this, &SettingsDialog::updatePendingBanner);
    connect(m_performanceTab, &SettingsPerformanceTab::recompileRequested, this, [this](const QString &t) { startRecompilation(t); });
    connect(m_performanceTab, &SettingsPerformanceTab::openExtensionsTabRequested, this, [this]() { setCurrentTab(TabIndex::Extensions); });
    m_tabWidget->addTab(m_performanceTab, tr("Performance"));

    m_extensionsTab = new SettingsExtensionsTab(this);
    connect(m_extensionsTab, &SettingsExtensionsTab::pendingStateChanged, this, &SettingsDialog::updatePendingBanner);
    m_tabWidget->addTab(m_extensionsTab, tr("Extensions && Modules"));

    m_extraTab = new SettingsExtraTab(this);
    connect(m_extraTab, &SettingsExtraTab::themeChanged, this, &SettingsDialog::themeChanged);
    connect(m_extraTab, &SettingsExtraTab::setupWizardCompleted, this, &SettingsDialog::refreshStatuses);
    connect(m_extraTab, &SettingsExtraTab::recompileRequested, this, [this](const QString &t) { startRecompilation(t); });
    m_tabWidget->addTab(m_extraTab, tr("Diagnostics && Maintenance"));

    mainLayout->addWidget(m_tabWidget);

    // Pending recompile notification banner
    m_bannerPending = new QFrame(this);
    m_bannerPending->setStyleSheet(
        "QFrame {"
        "  background-color: rgba(245, 158, 11, 0.12);"
        "  border: 1px solid rgba(245, 158, 11, 0.35);"
        "  border-radius: 6px;"
        "}"
    );
    QHBoxLayout *layBanner = new QHBoxLayout(m_bannerPending);
    layBanner->setContentsMargins(12, 8, 12, 8);
    layBanner->setSpacing(10);

    m_lblBannerMsg = new QLabel(tr("⚠️ <b>Pending Changes:</b> Build options were modified. Recompilation is required to apply."), m_bannerPending);
    m_lblBannerMsg->setStyleSheet("color: #d97706; font-size: 11px;");
    m_lblBannerMsg->setWordWrap(true);
    layBanner->addWidget(m_lblBannerMsg, 1);

    m_btnBannerRevert = new QPushButton(tr("Revert"), m_bannerPending);
    m_btnBannerRevert->setCursor(Qt::PointingHandCursor);
    m_btnBannerRevert->setStyleSheet("QPushButton { padding: 4px 10px; border-radius: 4px; font-size: 11px; }");
    connect(m_btnBannerRevert, &QPushButton::clicked, this, &SettingsDialog::onRevertPendingClicked);
    layBanner->addWidget(m_btnBannerRevert);

    m_btnBannerRecompile = new QPushButton(tr("⚙️ Recompile && Apply Now"), m_bannerPending);
    m_btnBannerRecompile->setCursor(Qt::PointingHandCursor);
    m_btnBannerRecompile->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 5px 14px; border-radius: 4px; font-size: 11px; } "
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnBannerRecompile, &QPushButton::clicked, this, [this]() { startRecompilation(); });
    layBanner->addWidget(m_btnBannerRecompile);

    m_bannerPending->hide();
    mainLayout->addWidget(m_bannerPending);

    // Bottom separator
    QFrame *bottomSep = new QFrame(this);
    bottomSep->setFrameShape(QFrame::HLine);
    bottomSep->setStyleSheet("color: rgba(100, 116, 139, 0.2);");
    mainLayout->addWidget(bottomSep);

    // Bottom action bar
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setMinimumWidth(100);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);
}

void SettingsDialog::refreshStatuses()
{
    if (m_integrationTab) {
        m_integrationTab->refreshStatuses();
    }
}

bool SettingsDialog::hasPendingBuildChanges() const
{
    bool perfPending = (m_performanceTab && m_performanceTab->hasPendingChanges());

    BuildConfig &bcfg = BuildConfig::instance();
    bool pdfChanged = (bcfg.staticPdf() != m_initialPdfStatic);
    bool archChanged = (bcfg.staticArchive() != m_initialArchiveStatic);
    bool offStatChanged = (bcfg.staticOffice() != m_initialOfficeStatic);
    bool offChanged = (bcfg.enableInternalOffice() != m_initialEnableOffice);

    return perfPending || pdfChanged || archChanged || offStatChanged || offChanged;
}

void SettingsDialog::updatePendingBanner()
{
    if (!m_bannerPending) return;
    m_bannerPending->setVisible(hasPendingBuildChanges());
}

void SettingsDialog::onRevertPendingClicked()
{
    if (m_performanceTab) {
        m_performanceTab->revertPendingChanges();
    }

    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.setStaticPdf(m_initialPdfStatic);
    bcfg.setStaticArchive(m_initialArchiveStatic);
    bcfg.setStaticOffice(m_initialOfficeStatic);
    bcfg.setEnableInternalOffice(m_initialEnableOffice);
    bcfg.save();

    updatePendingBanner();
}

void SettingsDialog::startRecompilation()
{
    startRecompilation(QString());
}

void SettingsDialog::startRecompilation(const QString &target)
{
    // 1. Check optional dependency prompt for librevenge if enabled
    BuildConfig &cfg = BuildConfig::instance();
    if (cfg.enableInternalOffice() && !BuildConfig::isPkgInstalled("librevenge-0.0")) {
        QMessageBox::StandardButton res = QMessageBox::question(this, tr("Missing Dependency"),
            tr("The Internal Office Viewer requires 'librevenge', which is not currently detected on your system.<br><br>Would you like the installer to attempt installing it via your system package manager?"),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::No) {
            return;
        }
    }

    // 2. Persist updated compiler optimization level to disk (if not targeting a specific debug rebuild)
    if (m_performanceTab && target.isEmpty()) {
        cfg.setOptimizationLevel(m_performanceTab->optimizationLevel());
    }
    cfg.save();

    // 3. Launch the dedicated floating build console
    if (!m_buildConsoleDialog) {
        m_buildConsoleDialog = new BuildConsoleDialog(this);
        connect(m_buildConsoleDialog, &BuildConsoleDialog::buildSucceeded, this, [this]() {
            if (m_performanceTab) {
                m_performanceTab->onBuildSucceeded();
            }
            BuildConfig &bcfg = BuildConfig::instance();
            m_initialPdfStatic = bcfg.staticPdf();
            m_initialArchiveStatic = bcfg.staticArchive();
            m_initialOfficeStatic = bcfg.staticOffice();
            m_initialEnableOffice = bcfg.enableInternalOffice();
            updatePendingBanner();
        });
    }

    m_buildConsoleDialog->show();
    m_buildConsoleDialog->raise();
    m_buildConsoleDialog->activateWindow();
    if (!target.isEmpty()) {
        m_buildConsoleDialog->startBuildTarget(target);
    } else {
        m_buildConsoleDialog->startBuild();
    }
}
