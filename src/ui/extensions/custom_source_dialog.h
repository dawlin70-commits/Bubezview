/**
 * @file custom_source_dialog.h
 * @brief Modal dialog for configuring custom Git repository sources, tags, and build flags.
 */

#pragma once

#include <QDialog>
#include <QString>
#include "core/system/build_config.h"

class QComboBox;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;

/**
 * @class CustomSourceDialog
 * @brief Interactive configuration dialog allowing users to define alternative source repositories.
 */
class CustomSourceDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs custom source dialog.
     * @param componentId Identifier of the component being configured (e.g. "poppler", "tesseract").
     * @param initialConfig Current source configuration.
     * @param parent Optional parent widget.
     */
    explicit CustomSourceDialog(const QString &componentId,
                                const CustomSourceConfig &initialConfig,
                                const QString &diagnosticCommand = QString(),
                                bool canCompile = true,
                                QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CustomSourceDialog() override = default;

    /**
     * @brief Returns the configured source parameters.
     */
    CustomSourceConfig config() const;

private slots:
    void onPresetChanged(int index);
    void testConnection();
    void resetToDefaults();
    void updateEffectivePreview();
    void runDiagnostic();

private:
    void setupUi();

    QString m_componentId;
    CustomSourceConfig m_config;
    QString m_diagnosticCommand;
    bool m_canCompile;

    class QTabWidget *m_tabs;
    QComboBox *m_comboPreset;
    QLineEdit *m_editGitUrl;
    QLineEdit *m_editBranchTag;
    QTextEdit *m_editBuildFlags;
    QLabel *m_lblPreview;
    QPushButton *m_btnTestConnection;
    QPushButton *m_btnReset;
    QPushButton *m_btnApply;

    // Diagnostic UI
    class QTextBrowser *m_txtDiagnosticOutput;
    QPushButton *m_btnRunDiagnostic;
};
