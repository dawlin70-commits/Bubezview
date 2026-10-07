/**
 * @file build_options_dialog.h
 * @brief Interactive dialog for managing compilation profiles, CPU tuning, and engine modules.
 */

#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include "build_config.h"

/**
 * @class BuildOptionsDialog
 * @brief GUI manager allowing users to select CPU optimization, toggle optional modules,
 * and trigger incremental non-interactive recompilation directly from the UI.
 */
class BuildOptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit BuildOptionsDialog(QWidget *parent = nullptr);
    ~BuildOptionsDialog() override;

private slots:
    void onSaveClicked();
    void onRecompileClicked();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUi();
    void loadFromConfig();
    void saveToConfig();

    // CPU Profile
    QRadioButton *m_radioCpuNative;
    QRadioButton *m_radioCpuStandard;

    // Modules
    QCheckBox *m_chkStaticPdf;
    QWidget *m_pdfVersionContainer;
    class QComboBox *m_comboPdfVersion;
    QCheckBox *m_chkStaticArchive;
    QCheckBox *m_chkInternalOffice;

    // Status Badges
    QLabel *m_lblPdfStatus;
    QLabel *m_lblArchiveStatus;
    QLabel *m_lblOfficeStatus;

    // Build Execution
    QProgressBar *m_progressBar;
    QLabel *m_lblBuildStatus;
    QTextEdit *m_txtLog;
    QPushButton *m_btnSave;
    QPushButton *m_btnRecompile;
    QPushButton *m_btnClose;

    QProcess *m_process;
};
