/**
 * @file extension_row_widget.h
 * @brief Self-contained two-level compact row widget representing an extension/module.
 */

#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include "extension_feature_model.h"
#include "custom_source_dialog.h"

/**
 * @class ExtensionRowWidget
 * @brief Two-level compact row widget designed to prevent horizontal text overflow while
 *        providing immediate one-click install/uninstall/upgrade actions, binary vs source toggles,
 *        verification integrity checks, and C++ feature discovery popovers.
 */
class ExtensionRowWidget : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief Constructs a two-level row for the specified module.
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

signals:
    /**
     * @brief Emitted when the module installation state or build mode changes.
     */
    void statusChanged();

private slots:
    void handleActionClicked();
    void runDiagnosticTest();
    void showFeatureDetails();
    void openCustomSourceDialog();

private:
    void setupUi();
    void updateActionButtons();
    void updateSourceToggleStyles();

    ExtensionModuleInfo m_mod;
    CustomSourceConfig m_sourceConfig;
    QString m_candidateVersion;
    bool m_updateAvailable;

    // UI Elements - Level 1 (Full-width header)
    QLabel *m_lblTitle;
    QPushButton *m_btnInfo;

    // UI Elements - Level 2 (Sub-info left, controls right)
    QLabel *m_lblSubInfo;
    QPushButton *m_btnSourceBinary;
    QPushButton *m_btnSourceBuild;
    QPushButton *m_btnEditSource;
    QPushButton *m_btnVerify;
    QPushButton *m_btnAction;
};
