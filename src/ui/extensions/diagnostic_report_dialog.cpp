/**
 * @file diagnostic_report_dialog.cpp
 * @brief Implementation of the diagnostic test report dialog with clipboard copy.
 */

#include "diagnostic_report_dialog.h"
#include "core/services/theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QGuiApplication>
#include <QClipboard>
#include <QDateTime>
#include <QTimer>
#include <QFont>

DiagnosticReportDialog::DiagnosticReportDialog(QWidget *parent)
    : FramelessDialog(parent)
{
    setupUi();
    updateThemeStyles();
}

void DiagnosticReportDialog::setupUi()
{
    setWindowTitle(tr("Diagnostic Report"));
    resize(640, 460);
    setMinimumSize(520, 360);

    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Header layout
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_lblIcon = new QLabel(this);
    m_lblIcon->setStyleSheet(QStringLiteral("font-size: 26px;"));
    m_lblIcon->setFixedWidth(36);
    m_lblIcon->setAlignment(Qt::AlignCenter | Qt::AlignTop);
    headerLayout->addWidget(m_lblIcon);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    m_lblTitle = new QLabel(this);
    m_lblTitle->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_lblTitle->setWordWrap(true);
    titleLayout->addWidget(m_lblTitle);

    m_lblSummary = new QLabel(this);
    m_lblSummary->setStyleSheet(QStringLiteral("font-size: 12px;"));
    m_lblSummary->setWordWrap(true);
    m_lblSummary->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
    titleLayout->addWidget(m_lblSummary);

    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Command section
    m_lblCommandHdr = new QLabel(tr("Executed Command:"), this);
    m_lblCommandHdr->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold;"));
    mainLayout->addWidget(m_lblCommandHdr);

    m_lblCommand = new QLabel(this);
    m_lblCommand->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_lblCommand->setWordWrap(true);
    mainLayout->addWidget(m_lblCommand);

    // Log / Output section
    QLabel *lblOutputHdr = new QLabel(tr("Diagnostic Output & Execution Log:"), this);
    lblOutputHdr->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold;"));
    mainLayout->addWidget(lblOutputHdr);

    m_txtLog = new QTextBrowser(this);
    m_txtLog->setReadOnly(true);
    m_txtLog->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
    m_txtLog->setOpenExternalLinks(true);
    mainLayout->addWidget(m_txtLog, 1);

    // Bottom action row
    QHBoxLayout *botLayout = new QHBoxLayout();
    botLayout->setSpacing(10);

    m_btnCopy = new QPushButton(tr("📋 Copy Report"), this);
    m_btnCopy->setCursor(Qt::PointingHandCursor);
    m_btnCopy->setToolTip(tr("Copy the full diagnostic report to the system clipboard"));
    connect(m_btnCopy, &QPushButton::clicked, this, &DiagnosticReportDialog::copyReportToClipboard);
    botLayout->addWidget(m_btnCopy);

    botLayout->addStretch();

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    botLayout->addWidget(m_btnClose);

    mainLayout->addLayout(botLayout);
}

void DiagnosticReportDialog::updateThemeStyles()
{
    FramelessDialog::updateThemeStyles();

    const bool dark = ThemeManager::isDarkTheme();
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    if (active == ThemePreset::ZincGray) {
        m_lblTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #f4f4f5;");
        m_lblSummary->setStyleSheet("font-size: 12px; color: #a1a1aa;");
        m_lblCommandHdr->setStyleSheet("font-size: 11px; font-weight: bold; color: #d4d4d8;");
        m_lblCommand->setStyleSheet(
            "background-color: rgba(0, 0, 0, 0.35);"
            "color: #38bdf8;"
            "border: 1px solid rgba(255, 255, 255, 0.12);"
            "border-radius: 6px;"
            "padding: 6px 10px;"
            "font-family: monospace;"
            "font-size: 11px;"
        );
        m_txtLog->setStyleSheet(
            "QTextBrowser {"
            "    background-color: #18181b;"
            "    color: #38bdf8;"
            "    border: 1px solid rgba(255, 255, 255, 0.15);"
            "    border-radius: 8px;"
            "    padding: 8px;"
            "    font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
            "    font-size: 11px;"
            "}"
        );
        m_btnCopy->setStyleSheet(
            "QPushButton {"
            "    background-color: #0284c7;"
            "    color: #ffffff;"
            "    font-weight: bold;"
            "    border: 1px solid #38bdf8;"
            "    border-radius: 6px;"
            "    padding: 6px 16px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: #0369a1; }"
        );
        m_btnClose->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(255, 255, 255, 0.08);"
            "    color: #f4f4f5;"
            "    border: 1px solid rgba(255, 255, 255, 0.18);"
            "    border-radius: 6px;"
            "    padding: 6px 18px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.16); }"
        );
    } else if (dark) {
        m_lblTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #f8fafc;");
        m_lblSummary->setStyleSheet("font-size: 12px; color: #94a3b8;");
        m_lblCommandHdr->setStyleSheet("font-size: 11px; font-weight: bold; color: #cbd5e1;");
        m_lblCommand->setStyleSheet(
            "background-color: rgba(15, 23, 42, 0.65);"
            "color: #38bdf8;"
            "border: 1px solid rgba(56, 189, 248, 0.25);"
            "border-radius: 6px;"
            "padding: 6px 10px;"
            "font-family: monospace;"
            "font-size: 11px;"
        );
        m_txtLog->setStyleSheet(
            "QTextBrowser {"
            "    background-color: #0f172a;"
            "    color: #38bdf8;"
            "    border: 1px solid rgba(56, 189, 248, 0.30);"
            "    border-radius: 8px;"
            "    padding: 8px;"
            "    font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
            "    font-size: 11px;"
            "}"
        );
        m_btnCopy->setStyleSheet(
            "QPushButton {"
            "    background-color: #0284c7;"
            "    color: #ffffff;"
            "    font-weight: bold;"
            "    border: 1px solid #38bdf8;"
            "    border-radius: 6px;"
            "    padding: 6px 16px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: #0369a1; }"
        );
        m_btnClose->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(255, 255, 255, 0.08);"
            "    color: #f8fafc;"
            "    border: 1px solid rgba(255, 255, 255, 0.18);"
            "    border-radius: 6px;"
            "    padding: 6px 18px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.16); }"
        );
    } else {
        m_lblTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #0f172a;");
        m_lblSummary->setStyleSheet("font-size: 12px; color: #475569;");
        m_lblCommandHdr->setStyleSheet("font-size: 11px; font-weight: bold; color: #334155;");
        m_lblCommand->setStyleSheet(
            "background-color: #f1f5f9;"
            "color: #0369a1;"
            "border: 1px solid #cbd5e1;"
            "border-radius: 6px;"
            "padding: 6px 10px;"
            "font-family: monospace;"
            "font-size: 11px;"
        );
        m_txtLog->setStyleSheet(
            "QTextBrowser {"
            "    background-color: #f8fafc;"
            "    color: #0f172a;"
            "    border: 1px solid #cbd5e1;"
            "    border-radius: 8px;"
            "    padding: 8px;"
            "    font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
            "    font-size: 11px;"
            "}"
        );
        m_btnCopy->setStyleSheet(
            "QPushButton {"
            "    background-color: #0284c7;"
            "    color: #ffffff;"
            "    font-weight: bold;"
            "    border: 1px solid #0284c7;"
            "    border-radius: 6px;"
            "    padding: 6px 16px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: #0369a1; }"
        );
        m_btnClose->setStyleSheet(
            "QPushButton {"
            "    background-color: rgba(0, 0, 0, 0.05);"
            "    color: #334155;"
            "    border: 1px solid rgba(0, 0, 0, 0.15);"
            "    border-radius: 6px;"
            "    padding: 6px 18px;"
            "    font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: rgba(0, 0, 0, 0.10); }"
        );
    }
}

void DiagnosticReportDialog::setReportData(const QString &title,
                                           const QString &componentName,
                                           bool success,
                                           const QString &command,
                                           const QString &rawOutput,
                                           int exitCode)
{
    setWindowTitle(title.isEmpty() ? tr("Diagnostic Report - %1").arg(componentName) : title);

    if (success) {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #10b981;'><b>●</b></p>"));
        m_lblTitle->setText(tr("Diagnostic Passed — %1").arg(componentName));
        m_lblSummary->setText(tr("Component is installed, operational, and verified successfully with exit code 0."));
    } else {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #f59e0b;'><b>▲</b></p>"));
        m_lblTitle->setText(tr("Diagnostic Issues Detected — %1").arg(componentName));
        m_lblSummary->setText(tr("Component verification encountered an issue (Exit Code %1).").arg(exitCode));
    }

    if (!command.trimmed().isEmpty()) {
        m_lblCommandHdr->setVisible(true);
        m_lblCommand->setVisible(true);
        m_lblCommand->setText(command);
    } else {
        m_lblCommandHdr->setVisible(false);
        m_lblCommand->setVisible(false);
    }

    m_txtLog->setPlainText(rawOutput);

    // Format plaintext report for 1-click clipboard copy
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    m_fullPlainTextReport = QString(
        "=== BubezView Diagnostic Report ===\n"
        "Date: %1\n"
        "Component: %2\n"
        "Status: %3 (Exit Code: %4)\n"
        "Command: %5\n\n"
        "--- Output / Execution Log ---\n"
        "%6\n"
        "====================================\n"
    ).arg(
        timestamp,
        componentName,
        success ? QStringLiteral("PASSED") : QStringLiteral("FAILED"),
        QString::number(exitCode),
        command.isEmpty() ? QStringLiteral("(none)") : command,
        rawOutput
    );
}

void DiagnosticReportDialog::setGlobalReportData(int passedCount,
                                                 int totalCount,
                                                 const QStringList &passedList,
                                                 const QStringList &failedList,
                                                 const QString &fullLog)
{
    setWindowTitle(tr("Global System Diagnostic Report"));

    bool allOk = failedList.isEmpty();
    if (allOk) {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #10b981;'><b>●</b></p>"));
        m_lblTitle->setText(tr("Component Verification — All Operational"));
        m_lblSummary->setText(tr("All %1 installed functional engines and extensions responded properly with zero errors.").arg(totalCount));
    } else {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #f59e0b;'><b>▲</b></p>"));
        m_lblTitle->setText(tr("Component Verification — Issues Detected"));
        m_lblSummary->setText(tr("%1 of %2 components are healthy, but %3 issues were encountered.")
                              .arg(passedCount).arg(totalCount).arg(failedList.count()));
    }

    m_lblCommandHdr->setVisible(false);
    m_lblCommand->setVisible(false);

    m_txtLog->setPlainText(fullLog);

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    m_fullPlainTextReport = tr(
        "BUBEZVIEW — COMPONENT VERIFICATION\n"
        "Date: %1 | Status: %2 (%3/%4 Operational)\n\n"
        "%5\n"
    ).arg(
        timestamp,
        allOk ? tr("OPERATIONAL") : tr("ISSUES DETECTED"),
        QString::number(passedCount),
        QString::number(totalCount),
        fullLog
    );
}

void DiagnosticReportDialog::copyReportToClipboard()
{
    QGuiApplication::clipboard()->setText(m_fullPlainTextReport);

    m_btnCopy->setText(tr("✓ Copied to Clipboard!"));
    m_btnCopy->setStyleSheet(
        "QPushButton {"
        "    background-color: #059669;"
        "    color: #ffffff;"
        "    font-weight: bold;"
        "    border: 1px solid #10b981;"
        "    border-radius: 6px;"
        "    padding: 6px 16px;"
        "    font-size: 12px;"
        "}"
    );

    QTimer::singleShot(2500, this, [this]() {
        if (!m_btnCopy) return;
        m_btnCopy->setText(tr("📋 Copy Report"));
        updateThemeStyles();
    });
}

void DiagnosticReportDialog::setRuntimeReportData(const QString &componentName,
                                                  bool success,
                                                  const QString &command,
                                                  const QString &rawOutput,
                                                  const QList<QPair<QString, bool>> &subResources,
                                                  int exitCode)
{
    setWindowTitle(tr("Health Check — %1").arg(componentName));

    if (success) {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #10b981;'><b>●</b></p>"));
        m_lblTitle->setText(tr("Runtime Operational — %1").arg(componentName));
        m_lblSummary->setText(tr("Component is installed, active, and verified. BubezView loads and renders files without errors."));
    } else {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #ef4444;'><b>▲</b></p>"));
        m_lblTitle->setText(tr("Runtime Issue Detected — %1").arg(componentName));
        m_lblSummary->setText(tr("Component verification encountered an execution failure (Exit code: %1).").arg(exitCode));
    }

    if (!command.trimmed().isEmpty()) {
        m_lblCommandHdr->setVisible(true);
        m_lblCommand->setVisible(true);
        m_lblCommand->setText(command);
    } else {
        m_lblCommandHdr->setVisible(false);
        m_lblCommand->setVisible(false);
    }

    QString subSection;
    if (!subResources.isEmpty()) {
        subSection += "\n\n--- Auxiliary Resources & Optional Extensions ---\n";
        for (const auto &pair : subResources) {
            subSection += QString("• %1: %2\n").arg(pair.first, pair.second ? tr("Installed") : tr("Not Installed (Optional)"));
        }
    }

    m_txtLog->setPlainText(rawOutput + subSection);

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    m_fullPlainTextReport = tr(
        "BUBEZVIEW — COMPONENT STATUS: %2\n"
        "Date: %1 | Status: %3 (Exit Code: %4)\n"
        "Command: %5\n\n"
        "%6%7\n"
    ).arg(
        timestamp,
        componentName,
        success ? tr("OPERATIONAL") : tr("ERROR"),
        QString::number(exitCode),
        command.isEmpty() ? tr("(internal)") : command,
        rawOutput,
        subSection
    );
}

void DiagnosticReportDialog::setBuildReadinessReportData(const QString &componentName,
                                                         bool ready,
                                                         const QStringList &missingList,
                                                         const QString &rawLog)
{
    setWindowTitle(tr("Build Prerequisites — %1").arg(componentName));

    if (ready) {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #10b981;'><b>●</b></p>"));
        m_lblTitle->setText(tr("Ready for Source Compilation — %1").arg(componentName));
        m_lblSummary->setText(tr("All build tools (CMake, C++ compiler) and development headers are present. You can build this engine from source with native CPU optimization."));
    } else {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #f59e0b;'><b>▲</b></p>"));
        m_lblTitle->setText(tr("Build Prerequisites Missing — %1").arg(componentName));
        m_lblSummary->setText(tr("Cannot compile from source yet: missing development packages (%1).\nPlease install the missing packages before attempting compilation.")
                              .arg(missingList.join(QStringLiteral(", "))));
    }

    m_lblCommandHdr->setVisible(false);
    m_lblCommand->setVisible(false);

    m_txtLog->setPlainText(rawLog);

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    m_fullPlainTextReport = tr(
        "BUBEZVIEW — BUILD PREREQUISITES: %2\n"
        "Date: %1 | Readiness: %3\n"
        "Missing: %4\n\n"
        "%5\n"
    ).arg(
        timestamp,
        componentName,
        ready ? tr("READY TO COMPILE") : tr("MISSING DEPENDENCIES"),
        missingList.isEmpty() ? tr("(none)") : missingList.join(QStringLiteral(", ")),
        rawLog
    );
}

void DiagnosticReportDialog::setGlobalExtendedReportData(int runtimeOkCount,
                                                         int runtimeTotalCount,
                                                         const QStringList &runtimeFailures,
                                                         const QStringList &missingResources,
                                                         const QStringList &buildReadyEngines,
                                                         const QStringList &buildBlockedEngines,
                                                         const QString &fullPlainTextLog)
{
    setWindowTitle(tr("Global Diagnostic System Audit"));

    bool allOk = runtimeFailures.isEmpty();
    if (allOk) {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #10b981;'><b>●</b></p>"));
        m_lblTitle->setText(tr("Global System Audit — All Active Engines Healthy"));
    } else {
        m_lblIcon->setText(QStringLiteral("<p style='font-size: 22px; color: #f59e0b;'><b>▲</b></p>"));
        m_lblTitle->setText(tr("Global System Audit — Notice"));
    }

    QString summary = tr("• 📦 Runtime: %1 of %2 installed modules operational.\n"
                         "• 🧩 Optional Resources: %3 missing accessories identified.\n"
                         "• ⚡ Build Environment: %4 compilable engines ready, %5 missing dev headers.")
                      .arg(runtimeOkCount).arg(runtimeTotalCount)
                      .arg(missingResources.count())
                      .arg(buildReadyEngines.count()).arg(buildBlockedEngines.count());
    m_lblSummary->setText(summary);

    m_lblCommandHdr->setVisible(false);
    m_lblCommand->setVisible(false);

    m_txtLog->setPlainText(fullPlainTextLog);
    m_fullPlainTextReport = fullPlainTextLog;
}

void DiagnosticReportDialog::showRuntimeReport(QWidget *parent,
                                              const QString &componentName,
                                              bool success,
                                              const QString &command,
                                              const QString &rawOutput,
                                              const QList<QPair<QString, bool>> &subResources,
                                              int exitCode)
{
    DiagnosticReportDialog dlg(parent);
    dlg.setRuntimeReportData(componentName, success, command, rawOutput, subResources, exitCode);
    dlg.exec();
}

void DiagnosticReportDialog::showBuildReadinessReport(QWidget *parent,
                                                     const QString &componentName,
                                                     bool ready,
                                                     const QStringList &missingList,
                                                     const QString &rawLog)
{
    DiagnosticReportDialog dlg(parent);
    dlg.setBuildReadinessReportData(componentName, ready, missingList, rawLog);
    dlg.exec();
}

void DiagnosticReportDialog::showGlobalExtendedReport(QWidget *parent,
                                                     int runtimeOkCount,
                                                     int runtimeTotalCount,
                                                     const QStringList &runtimeFailures,
                                                     const QStringList &missingResources,
                                                     const QStringList &buildReadyEngines,
                                                     const QStringList &buildBlockedEngines,
                                                     const QString &fullPlainTextLog)
{
    DiagnosticReportDialog dlg(parent);
    dlg.setGlobalExtendedReportData(runtimeOkCount, runtimeTotalCount, runtimeFailures, missingResources, buildReadyEngines, buildBlockedEngines, fullPlainTextLog);
    dlg.exec();
}

void DiagnosticReportDialog::showReport(QWidget *parent,
                                        const QString &title,
                                        const QString &componentName,
                                        bool success,
                                        const QString &command,
                                        const QString &rawOutput,
                                        int exitCode)
{
    DiagnosticReportDialog dlg(parent);
    dlg.setReportData(title, componentName, success, command, rawOutput, exitCode);
    dlg.exec();
}

void DiagnosticReportDialog::showGlobalReport(QWidget *parent,
                                              int passedCount,
                                              int totalCount,
                                              const QStringList &passedList,
                                              const QStringList &failedList,
                                              const QString &fullLog)
{
    DiagnosticReportDialog dlg(parent);
    dlg.setGlobalReportData(passedCount, totalCount, passedList, failedList, fullLog);
    dlg.exec();
}
