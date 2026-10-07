/**
 * @file custom_source_dialog.h
 * @brief Modal dialog for configuring custom Git repository sources, tags, and build flags.
 */

#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QTextEdit;
class QPushButton;

/**
 * @struct CustomSourceConfig
 * @brief Holds user-defined Git source, tag, and build configuration.
 */
struct CustomSourceConfig {
    QString preset;         ///< "Official Stable", "Development Git", or "Custom URL"
    QString gitUrl;         ///< Git repository URL or mirror
    QString branchTag;      ///< Branch or git tag name
    QString buildFlags;     ///< Additional CMake or build arguments
};

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
    explicit CustomSourceDialog(const QString &componentId, const CustomSourceConfig &initialConfig, QWidget *parent = nullptr);

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

private:
    void setupUi();

    QString m_componentId;
    CustomSourceConfig m_config;

    QComboBox *m_comboPreset;
    QLineEdit *m_editGitUrl;
    QLineEdit *m_editBranchTag;
    QTextEdit *m_editBuildFlags;
    QPushButton *m_btnTestConnection;
    QPushButton *m_btnApply;
};
