/**
 * @file update_dialog.h
 * @brief Interactive update dialog displaying changelog, build progress, and compilation log.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QProcess>
#include "update_manager.h"

/**
 * @brief Dialog that informs users about a new version and executes in-app updates.
 */
class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the UpdateDialog.
     * @param info Release metadata.
     * @param parent Optional parent widget.
     */
    explicit UpdateDialog(const ReleaseInfo &info, QWidget *parent = nullptr);
    ~UpdateDialog() override;

private slots:
    void startUpdate();
    void toggleDetails();
    void openBuildOptions();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void restartApplication();

private:
    void setupUi();

    ReleaseInfo m_info;
    QProcess *m_process;

    QLabel *m_lblTitle;
    QLabel *m_lblChangelog;
    QLabel *m_lblStatus;
    QProgressBar *m_progressBar;
    QPushButton *m_btnUpdate;
    QPushButton *m_btnCancel;
    QPushButton *m_btnDetails;
    QPushButton *m_btnConfig;
    QTextEdit *m_txtLog;
    bool m_showingDetails;
};
