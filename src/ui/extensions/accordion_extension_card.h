/**
 * @file accordion_extension_card.h
 * @brief Collapsible accordion widget representing an extension, its code features, diagnostics, and actions.
 */

#pragma once

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "extension_feature_model.h"

/**
 * @class AccordionExtensionCard
 * @brief Modern collapsible card displaying an extension's status, features enabled in code,
 *        sub-dependency alerts, live diagnostic checks, source compilation toggles, and installation buttons.
 */
class AccordionExtensionCard : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructs an accordion card for the given extension metadata.
     * @param mod Extension metadata and feature set.
     * @param parent Parent widget.
     */
    explicit AccordionExtensionCard(const ExtensionModuleInfo &mod, QWidget *parent = nullptr);

    /**
     * @brief Returns current extension identifier.
     */
    QString extensionId() const { return m_mod.id; }

    /**
     * @brief Returns true if card details are currently expanded.
     */
    bool isExpanded() const { return m_expanded; }

    /**
     * @brief Expands or collapses the card.
     * @param expand True to expand, false to collapse.
     */
    void setExpanded(bool expand);

    /**
     * @brief Refreshes UI state from the system.
     */
    void refreshState();

signals:
    /**
     * @brief Emitted when the extension is installed, uninstalled, or its build mode changes.
     */
    void statusChanged();

private slots:
    void toggleExpanded();
    void handleInstallOrUninstall();
    void runDiagnosticTest();

private:
    void updateHeaderStatusBadge();
    void updateActionButton();
    void updateCompileToggleStyles();

    ExtensionModuleInfo m_mod;
    bool m_expanded;

    // Header widgets
    QPushButton *m_btnToggle;
    QLabel *m_lblTitle;
    QLabel *m_lblSummary;
    QLabel *m_lblStatusBadge;

    // Details container widgets
    QFrame *m_detailsContainer;
    QVBoxLayout *m_featuresLayout;
    QList<QWidget *> m_featureRows;

    // Sub-dependencies container
    QVBoxLayout *m_subDepsLayout;

    // Footer widgets
    QLabel *m_lblModeTitle;
    QPushButton *m_btnSourceDynamic;
    QPushButton *m_btnSourceStatic;
    QPushButton *m_btnDiagnose;
    QLabel *m_lblDiagResult;
    QPushButton *m_btnAction;
};
