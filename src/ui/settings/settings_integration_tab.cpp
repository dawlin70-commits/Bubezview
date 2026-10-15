/**
 * @file settings_integration_tab.cpp
 * @brief Implementation of the desktop integration settings tab.
 */

#include "settings_integration_tab.h"
#include "desktop_integrator.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QFrame>

SettingsIntegrationTab::SettingsIntegrationTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshStatuses();
}

void SettingsIntegrationTab::setupUi()
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
    layout->setContentsMargins(14, 14, 14, 14);

    QLabel *header = new QLabel(QString(tr("Detected desktop environment: <b>%1</b>")).arg(DesktopIntegrator::desktopName()), content);
    header->setStyleSheet("color: #888; margin-bottom: 2px;");
    layout->addWidget(header);

    // 1. Dolphin Group
    QGroupBox *grpDolphin = new QGroupBox(tr("Dolphin (KDE Plasma)"), content);
    QVBoxLayout *layDolphin = new QVBoxLayout(grpDolphin);
    layDolphin->setSpacing(8);
    layDolphin->setContentsMargins(12, 10, 12, 12);

    QHBoxLayout *rowDolphinStatus = new QHBoxLayout();
    m_lblDolphinStatus = new QLabel(grpDolphin);
    m_btnDolphinToggle = new QPushButton(grpDolphin);
    m_btnDolphinToggle->setCursor(Qt::PointingHandCursor);
    m_btnDolphinToggle->setMinimumWidth(160);
    connect(m_btnDolphinToggle, &QPushButton::clicked, this, &SettingsIntegrationTab::toggleDolphin);
    rowDolphinStatus->addWidget(m_lblDolphinStatus, 1);
    rowDolphinStatus->addWidget(m_btnDolphinToggle);
    layDolphin->addLayout(rowDolphinStatus);

    QHBoxLayout *rowDolphinKey = new QHBoxLayout();
    QLabel *lblKey = new QLabel(tr("Trigger shortcut in Dolphin:"), grpDolphin);
    m_comboDolphinShortcut = new QComboBox(grpDolphin);
    m_comboDolphinShortcut->setCursor(Qt::PointingHandCursor);
    m_comboDolphinShortcut->addItem(tr("Space (Mac-style Quick Look)"), "Space");
    m_comboDolphinShortcut->addItem(tr("Ctrl + Space (Safe non-conflicting)"), "Ctrl+Space");

    QString curKey = DesktopIntegrator::currentDolphinShortcut();
    int keyIdx = m_comboDolphinShortcut->findData(curKey);
    if (keyIdx >= 0) {
        m_comboDolphinShortcut->setCurrentIndex(keyIdx);
    }
    rowDolphinKey->addWidget(lblKey);
    rowDolphinKey->addWidget(m_comboDolphinShortcut, 1);
    layDolphin->addLayout(rowDolphinKey);

    layout->addWidget(grpDolphin);

    // 2. GNOME Nautilus & Cinnamon Nemo Group
    QGroupBox *grpGnome = new QGroupBox(tr("GNOME Nautilus / Nemo"), content);
    QHBoxLayout *rowGnome = new QHBoxLayout(grpGnome);
    rowGnome->setContentsMargins(12, 10, 12, 12);
    m_lblGnomeStatus = new QLabel(grpGnome);
    m_btnGnomeToggle = new QPushButton(grpGnome);
    m_btnGnomeToggle->setCursor(Qt::PointingHandCursor);
    m_btnGnomeToggle->setMinimumWidth(160);
    connect(m_btnGnomeToggle, &QPushButton::clicked, this, &SettingsIntegrationTab::toggleGnomeNemo);
    rowGnome->addWidget(m_lblGnomeStatus, 1);
    rowGnome->addWidget(m_btnGnomeToggle);
    layout->addWidget(grpGnome);

    // 3. Context Menu (Right Click) Group
    QGroupBox *grpContext = new QGroupBox(tr("Context Menu (Right Click)"), content);
    QHBoxLayout *rowContext = new QHBoxLayout(grpContext);
    rowContext->setContentsMargins(12, 10, 12, 12);
    m_lblContextStatus = new QLabel(grpContext);
    m_btnContextToggle = new QPushButton(grpContext);
    m_btnContextToggle->setCursor(Qt::PointingHandCursor);
    m_btnContextToggle->setMinimumWidth(160);
    connect(m_btnContextToggle, &QPushButton::clicked, this, &SettingsIntegrationTab::toggleContextMenu);
    rowContext->addWidget(m_lblContextStatus, 1);
    rowContext->addWidget(m_btnContextToggle);
    layout->addWidget(grpContext);

    // 4. App Launcher Menu (.desktop)
    QGroupBox *grpAppMenu = new QGroupBox(tr("System Application Menu"), content);
    QHBoxLayout *rowAppMenu = new QHBoxLayout(grpAppMenu);
    rowAppMenu->setContentsMargins(12, 10, 12, 12);
    m_lblAppMenuStatus = new QLabel(grpAppMenu);
    m_btnAppMenuToggle = new QPushButton(grpAppMenu);
    m_btnAppMenuToggle->setCursor(Qt::PointingHandCursor);
    m_btnAppMenuToggle->setMinimumWidth(160);
    connect(m_btnAppMenuToggle, &QPushButton::clicked, this, &SettingsIntegrationTab::toggleAppMenu);
    rowAppMenu->addWidget(m_lblAppMenuStatus, 1);
    rowAppMenu->addWidget(m_btnAppMenuToggle);
    layout->addWidget(grpAppMenu);

    // 5. Office Engine Integration Group
    QGroupBox *grpOffice = new QGroupBox(tr("Office Documents Integration"), content);
    QVBoxLayout *layOffice = new QVBoxLayout(grpOffice);
    layOffice->setContentsMargins(12, 10, 12, 12);

    m_chkUseLibreOffice = new QCheckBox(tr("Use LibreOffice for preview (high-fidelity PDF conversion)"), grpOffice);
    m_chkUseLibreOffice->setCursor(Qt::PointingHandCursor);
    m_chkUseLibreOffice->setChecked(ThemeManager::useLibreOffice());
    connect(m_chkUseLibreOffice, &QCheckBox::toggled, this, &SettingsIntegrationTab::onUseLibreOfficeToggled);
    layOffice->addWidget(m_chkUseLibreOffice);

    layout->addWidget(grpOffice);

    layout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsIntegrationTab::onUseLibreOfficeToggled(bool checked)
{
    ThemeManager::setUseLibreOffice(checked);
}

void SettingsIntegrationTab::refreshStatuses()
{
    // Dolphin
    bool dolOn = DesktopIntegrator::isDolphinEnabled();
    QString shortcutName = DesktopIntegrator::currentDolphinShortcut();
    if (dolOn) {
        m_lblDolphinStatus->setText(QString(tr("🟢 <b>Enabled</b> (%1 opens BubezView in Dolphin)")).arg(shortcutName));
        m_btnDolphinToggle->setText(tr("Disable Dolphin Shortcut"));
    } else {
        m_lblDolphinStatus->setText(tr("⚪ <b>Disabled</b>"));
        m_btnDolphinToggle->setText(tr("✅ Enable Dolphin Shortcut"));
    }

    if (m_comboDolphinShortcut) {
        int idx = m_comboDolphinShortcut->findData(shortcutName);
        if (idx >= 0) {
            m_comboDolphinShortcut->setCurrentIndex(idx);
        }
    }

    // GNOME / Nemo
    bool gnOn = DesktopIntegrator::isGnomeNemoEnabled();
    m_lblGnomeStatus->setText(gnOn ? tr("🟢 <b>Enabled</b> (DBus and Sushi forward ready)") : tr("⚪ <b>Disabled</b>"));
    m_btnGnomeToggle->setText(gnOn ? tr("Disable for GNOME") : tr("✅ Enable for GNOME/Mint"));

    // Context Menu
    bool ctxOn = DesktopIntegrator::isContextMenuEnabled();
    m_lblContextStatus->setText(ctxOn ? tr("🟢 <b>Present</b> in file manager right-click menu") : tr("⚪ <b>Not configured</b>"));
    m_btnContextToggle->setText(ctxOn ? tr("Remove from Right Click") : tr("✅ Add to Right Click"));

    // App Menu
    bool appOn = DesktopIntegrator::isAppMenuInstalled();
    m_lblAppMenuStatus->setText(appOn ? tr("🟢 <b>Installed</b> in system menu and 'Open With...'") : tr("⚪ <b>Not present</b>"));
    m_btnAppMenuToggle->setText(appOn ? tr("Remove from Menu") : tr("✅ Add to App Menu"));
}

void SettingsIntegrationTab::toggleDolphin()
{
    if (DesktopIntegrator::isDolphinEnabled()) {
        DesktopIntegrator::disableDolphin();
        QMessageBox::information(this, tr("Dolphin"), tr("Dolphin shortcut for BubezView removed."));
    } else {
        QString chosenKey = "Space";
        if (m_comboDolphinShortcut) {
            chosenKey = m_comboDolphinShortcut->currentData().toString();
        }
        DesktopIntegrator::enableDolphin(chosenKey);
        QMessageBox::information(this, tr("Dolphin"), QString(tr("Dolphin shortcut '%1' enabled successfully!")).arg(chosenKey));
    }
    refreshStatuses();
}

void SettingsIntegrationTab::toggleGnomeNemo()
{
    if (DesktopIntegrator::isGnomeNemoEnabled()) {
        DesktopIntegrator::disableGnomeNemo();
        QMessageBox::information(this, tr("GNOME / Nemo"), tr("GNOME/Nemo integration removed."));
    } else {
        DesktopIntegrator::enableGnomeNemo();
        QMessageBox::information(
            this,
            tr("GNOME / Nemo"),
            tr("Integration for GNOME (Nautilus) and Linux Mint (Nemo) enabled successfully!\n\n"
               "✓ DBus services registered for BubezView.\n"
               "✓ Sushi and Nemo-Preview compatibility ready.")
        );
    }
    refreshStatuses();
}

void SettingsIntegrationTab::toggleContextMenu()
{
    if (DesktopIntegrator::isContextMenuEnabled()) {
        DesktopIntegrator::disableContextMenu();
        QMessageBox::information(this, tr("Right Click"), tr("Context menu items removed."));
    } else {
        DesktopIntegrator::enableContextMenu();
        QMessageBox::information(this, tr("Right Click"), tr("'Preview with BubezView' items added to right-click menu successfully!"));
    }
    refreshStatuses();
}

void SettingsIntegrationTab::toggleAppMenu()
{
    if (DesktopIntegrator::isAppMenuInstalled()) {
        DesktopIntegrator::uninstallAppMenu();
        QMessageBox::information(this, tr("Application Menu"), tr("Icon removed from application menu."));
    } else {
        DesktopIntegrator::installAppMenu();
        QMessageBox::information(this, tr("Application Menu"), tr("BubezView successfully added to application menu and 'Open With...' menu!"));
    }
    refreshStatuses();
}
