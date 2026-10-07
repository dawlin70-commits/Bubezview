/**
 * @file build_console_dialog.h
 * @brief Dedicated floating console dialog for real-time build and recompilation monitoring.
 */

#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QTextEdit;
class QPushButton;
class QProcess;

/**
 * @class BuildConsoleDialog
 * @brief Dedicated, resizable floating console showing live compilation progress, terminal logs, and controls.
 */
class BuildConsoleDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the build console dialog.
     * @param parent Optional parent widget.
     */
    explicit BuildConsoleDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor. Ensures running build process is safely handled.
     */
    ~BuildConsoleDialog() override;

    /**
     * @brief Launches the in-place compilation process.
     * @return True if process started successfully, false otherwise.
     */
    bool startBuild();

    /**
     * @brief Returns whether a build process is currently active.
     * @return True if compiling, false if idle or finished.
     */
    bool isBuilding() const;

signals:
    /**
     * @brief Emitted when recompilation completes successfully.
     */
    void buildSucceeded();

    /**
     * @brief Emitted when recompilation fails.
     * @param exitCode The process exit code.
     */
    void buildFailed(int exitCode);

protected:
    /**
     * @brief Intercepts window close event to confirm abortion if a build is active.
     * @param event Close event parameters.
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
     * @brief Reads standard output/error from compiler process and appends to log console.
     */
    void onReadyReadStandardOutput();

    /**
     * @brief Handles termination of the build process.
     * @param exitCode Return code of the build script.
     * @param exitStatus Exit status enum.
     */
    void onProcessFinished(int exitCode, int exitStatus);

    /**
     * @brief Aborts active compilation upon user request.
     */
    void cancelBuild();

    /**
     * @brief Restarts BubezView with the newly compiled binary.
     */
    void restartApplication();

private:
    /**
     * @brief Initializes dialog geometry and UI layout.
     */
    void setupUi();

    QLabel *m_lblTitle;
    QLabel *m_lblStatus;
    QProgressBar *m_progressBar;
    QTextEdit *m_txtLog;
    QPushButton *m_btnCancel;
    QPushButton *m_btnRestart;
    QPushButton *m_btnClose;

    QProcess *m_process;
    bool m_succeeded;
};
