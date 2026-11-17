#pragma once

#include "frameless_dialog.h"
#include <QString>
#include "plugin_manager.h"

class QComboBox;
class QRadioButton;
class QLineEdit;
class QPushButton;
class QButtonGroup;
class QWidget;

/**
 * @brief Unified export dialog supporting format conversion (PDF, Image, Text)
 * and page scope selection (Current Page vs All Pages).
 */
class UnifiedExportDialog : public FramelessDialog {
    Q_OBJECT

public:
    enum DialogResult {
        Rejected = QDialog::Rejected,
        Accepted = QDialog::Accepted,
        ExtractImages = 2
    };

    explicit UnifiedExportDialog(FileCategory category,
                                const QString &sourcePath,
                                int currentPage,
                                int totalPages,
                                QWidget *parent = nullptr);

    QString selectedFormat() const;
    bool isAllPages() const;
    QString targetPath() const;

private slots:
    void onFormatChanged();
    void onScopeChanged();
    void onBrowseClicked();

private:
    void setupUi();
    void updateDefaultTargetPath();

    FileCategory m_category;
    QString m_sourcePath;
    int m_currentPage;
    int m_totalPages;

    QComboBox *m_comboFormat = nullptr;
    QWidget *m_scopeContainer = nullptr;
    QRadioButton *m_radioCurrentPage = nullptr;
    QRadioButton *m_radioAllPages = nullptr;
    QButtonGroup *m_scopeGroup = nullptr;
    QLineEdit *m_editPath = nullptr;
    QPushButton *m_btnBrowse = nullptr;
    QPushButton *m_btnExtractImages = nullptr;
    QPushButton *m_btnExport = nullptr;
    QPushButton *m_btnCancel = nullptr;
};
