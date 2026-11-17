/**
 * @file diagnostic_report_dialog.h
 * @brief Modal dialog presenting diagnostic test results, verification logs,
 * and command outputs with full text selection, mouse interaction, and a 1-click
 * clipboard copy button.
 */

#pragma once

#include "dialogs/frameless_dialog.h"
#include <QString>
#include <QStringList>

class QLabel;
class QTextBrowser;
class QPushButton;

class DiagnosticReportDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit DiagnosticReportDialog(QWidget *parent = nullptr);
    ~DiagnosticReportDialog() override = default;

    /**
     * @brief Configure and show a diagnostic report for an individual module or component.
     */
    static void showReport(QWidget *parent,
                           const QString &title,
                           const QString &componentName,
                           bool success,
                           const QString &command,
                           const QString &rawOutput,
                           int exitCode = 0);

    /**
     * @brief Configure and show a global health check report across all modules.
     */
    static void showGlobalReport(QWidget *parent,
                                 int passedCount,
                                 int totalCount,
                                 const QStringList &passedList,
                                 const QStringList &failedList,
                                 const QString &fullLog);

    /**
     * @brief Configure and show a runtime health check report (no compiler warnings).
     */
    static void showRuntimeReport(QWidget *parent,
                                  const QString &componentName,
                                  bool success,
                                  const QString &command,
                                  const QString &rawOutput,
                                  const QList<QPair<QString, bool>> &subResources = {},
                                  int exitCode = 0);

    /**
     * @brief Configure and show a build readiness report for compilable modules.
     */
    static void showBuildReadinessReport(QWidget *parent,
                                         const QString &componentName,
                                         bool ready,
                                         const QStringList &missingList,
                                         const QString &rawLog);

    /**
     * @brief Configure and show a 3-section global audit report across runtime, dependencies, and build readiness.
     */
    static void showGlobalExtendedReport(QWidget *parent,
                                         int runtimeOkCount,
                                         int runtimeTotalCount,
                                         const QStringList &runtimeFailures,
                                         const QStringList &missingResources,
                                         const QStringList &buildReadyEngines,
                                         const QStringList &buildBlockedEngines,
                                         const QString &fullPlainTextLog);

    void setReportData(const QString &title,
                       const QString &componentName,
                       bool success,
                       const QString &command,
                       const QString &rawOutput,
                       int exitCode = 0);

    void setRuntimeReportData(const QString &componentName,
                              bool success,
                              const QString &command,
                              const QString &rawOutput,
                              const QList<QPair<QString, bool>> &subResources,
                              int exitCode = 0);

    void setBuildReadinessReportData(const QString &componentName,
                                     bool ready,
                                     const QStringList &missingList,
                                     const QString &rawLog);

    void setGlobalExtendedReportData(int runtimeOkCount,
                                     int runtimeTotalCount,
                                     const QStringList &runtimeFailures,
                                     const QStringList &missingResources,
                                     const QStringList &buildReadyEngines,
                                     const QStringList &buildBlockedEngines,
                                     const QString &fullPlainTextLog);

    void setGlobalReportData(int passedCount,
                             int totalCount,
                             const QStringList &passedList,
                             const QStringList &failedList,
                             const QString &fullLog);

private slots:
    void copyReportToClipboard();

private:
    void setupUi();
    void updateThemeStyles();

    QLabel *m_lblIcon = nullptr;
    QLabel *m_lblTitle = nullptr;
    QLabel *m_lblSummary = nullptr;
    QLabel *m_lblCommandHdr = nullptr;
    QLabel *m_lblCommand = nullptr;
    QTextBrowser *m_txtLog = nullptr;
    QPushButton *m_btnCopy = nullptr;
    QPushButton *m_btnClose = nullptr;

    QString m_fullPlainTextReport;
};
