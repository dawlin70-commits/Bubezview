#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "app_launcher.h"

/**
 * @brief Modal dialog allowing the user to select an installed desktop application
 * or browse for a custom binary to open the current file with.
 */
class OpenWithDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the dialog for a specific target file.
     * @param filePath Absolute path of the file to open.
     * @param parent Optional parent widget.
     */
    explicit OpenWithDialog(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Returns the chosen desktop application.
     * @return Selected AppInfo, or an empty AppInfo if custom binary was chosen.
     */
    AppInfo selectedApp() const;

    /**
     * @brief Returns the path of a manually chosen custom executable.
     * @return Path to custom binary, or empty if a registered desktop app was selected.
     */
    QString customCommand() const;

private slots:
    void onFilterChanged(const QString &text);
    void onBrowseExecutable();
    void onAppItemDoubleClicked(QListWidgetItem *item);
    void onAccept();

private:
    void setupUi();
    void populateApps();

    QString m_filePath;
    QLineEdit *m_filterEdit;
    QListWidget *m_listWidget;
    QPushButton *m_btnBrowse;
    QPushButton *m_btnOpen;
    QPushButton *m_btnCancel;

    QList<AppInfo> m_associatedApps;
    QList<AppInfo> m_allApps;
    AppInfo m_selectedApp;
    QString m_customCommand;
};
