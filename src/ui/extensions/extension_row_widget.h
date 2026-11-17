/**
 * @file extension_row_widget.h
 * @brief Compact 2-column tile widget representing an extension or functional module.
 */

#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include "extension_feature_model.h"
#include "custom_source_dialog.h"

/**
 * @class ExtensionRowWidget
 * @brief Compact 2-column tile widget representing an extension/module.
 *        Features a 3-button direct control system:
 *        1. Install / Uninstall / Health indicator (🟢 / ⚪ / ⚠️)
 *        2. Switch between Distro Package and Native Source (📦 / ⚡)
 *        3. Advanced technical options and diagnostics menu (···)
 */
class ExtensionRowWidget : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief Constructs a compact tile for the specified module.
     * @param mod Extension metadata and feature set.
     * @param parent Optional parent widget.
     */
    explicit ExtensionRowWidget(const ExtensionModuleInfo &mod, QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ExtensionRowWidget() override = default;

    /**
     * @brief Returns current extension identifier.
     */
    QString extensionId() const { return m_mod.id; }

    /**
     * @brief Returns the category of this module.
     */
    QString category() const { return m_mod.category; }

    /**
     * @brief Refreshes status, candidate version, and action buttons.
     */
    void refreshState();

    /**
     * @brief Updates colors, badges, and fonts to match the active theme.
     */
    void updateThemeStyles();

signals:
    /**
     * @brief Emitted when the module installation state or build mode changes.
     */
    void statusChanged();

private slots:
    void handleInstallClicked();
    void handleSourceSwitchClicked();
    void runDiagnosticTest();
    void runRuntimeHealthCheck();
    void runBuildReadinessCheck();
    void showFeatureDetails();
    void openCustomSourceDialog();
    void openOptionsMenu();

private:
    void setupUi();
    void updateButtonStates();

    ExtensionModuleInfo m_mod;
    CustomSourceConfig m_sourceConfig;
    QString m_candidateVersion;
    bool m_updateAvailable = false;

    // UI Elements - Compact Tile
    QLabel *m_lblIcon = nullptr;
    QLabel *m_lblTitle = nullptr;
    QPushButton *m_btnInstall = nullptr;      ///< Button 1: Install / Uninstall / Health (🟢, ⚪, ⚠️)
    QPushButton *m_btnSourceSwitch = nullptr; ///< Button 2: Switch between Package (📦) <-> Source (⚡)
    QPushButton *m_btnOptions = nullptr;     ///< Button 3: Advanced Options menu (···)
};
