/**
 * @file custom_source_dialog.cpp
 * @brief Implementation of custom Git repository sources and build flags configuration dialog.
 */

#include "custom_source_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QMessageBox>

CustomSourceDialog::CustomSourceDialog(const QString &componentId, const CustomSourceConfig &initialConfig, QWidget *parent)
    : QDialog(parent)
    , m_componentId(componentId)
    , m_config(initialConfig)
{
    setWindowTitle(tr("Configure Source Repository & Build Flags"));
    resize(520, 360);
    setupUi();
}

void CustomSourceDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    QLabel *lblHeader = new QLabel(
        QString("<b>%1: %2</b><br>"
                "<span style='color: #64748b; font-size: 11px;'>%3</span>")
            .arg(tr("Source Build Configuration"), m_componentId)
            .arg(tr("Select an official repository preset or specify a custom Git repository and branch/tag.")),
        this
    );
    lblHeader->setWordWrap(true);
    mainLayout->addWidget(lblHeader);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 1. Preset Selector
    m_comboPreset = new QComboBox(this);
    m_comboPreset->addItem(tr("Official Stable"), "stable");
    m_comboPreset->addItem(tr("Development Git (main/master)"), "git");
    m_comboPreset->addItem(tr("Custom Git URL"), "custom");

    int presetIdx = 0;
    if (m_config.preset == "git") presetIdx = 1;
    else if (m_config.preset == "custom") presetIdx = 2;
    m_comboPreset->setCurrentIndex(presetIdx);

    formLayout->addRow(tr("Preset:"), m_comboPreset);

    // 2. Git URL
    m_editGitUrl = new QLineEdit(this);
    m_editGitUrl->setPlaceholderText("https://github.com/...");
    m_editGitUrl->setText(m_config.gitUrl);
    formLayout->addRow(tr("Git Repository:"), m_editGitUrl);

    // 3. Branch / Tag
    m_editBranchTag = new QLineEdit(this);
    m_editBranchTag->setPlaceholderText("master / main / v1.2.3");
    m_editBranchTag->setText(m_config.branchTag);
    formLayout->addRow(tr("Branch / Tag:"), m_editBranchTag);

    // 4. Extra Build Flags
    m_editBuildFlags = new QTextEdit(this);
    m_editBuildFlags->setMaximumHeight(70);
    m_editBuildFlags->setPlaceholderText("-DCMAKE_BUILD_TYPE=Release -DENABLE_EXTRA=ON");
    m_editBuildFlags->setText(m_config.buildFlags);
    formLayout->addRow(tr("Build Flags:"), m_editBuildFlags);

    mainLayout->addLayout(formLayout);

    // Connect preset changes
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CustomSourceDialog::onPresetChanged);

    // Button row
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnTestConnection = new QPushButton(tr("Test Connection"), this);
    m_btnTestConnection->setCursor(Qt::PointingHandCursor);
    connect(m_btnTestConnection, &QPushButton::clicked, this, &CustomSourceDialog::testConnection);
    btnLayout->addWidget(m_btnTestConnection);

    btnLayout->addStretch();

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    m_btnApply = new QPushButton(tr("Apply Source"), this);
    m_btnApply->setCursor(Qt::PointingHandCursor);
    m_btnApply->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 5px 14px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }");
    connect(m_btnApply, &QPushButton::clicked, this, [this]() {
        m_config.preset = m_comboPreset->currentData().toString();
        m_config.gitUrl = m_editGitUrl->text().trimmed();
        m_config.branchTag = m_editBranchTag->text().trimmed();
        m_config.buildFlags = m_editBuildFlags->toPlainText().trimmed();
        accept();
    });
    btnLayout->addWidget(m_btnApply);

    mainLayout->addLayout(btnLayout);

    onPresetChanged(presetIdx);
}

void CustomSourceDialog::onPresetChanged(int index)
{
    QString mode = m_comboPreset->itemData(index).toString();
    if (mode == "stable") {
        if (m_componentId == "poppler" || m_componentId == "pdfimages") {
            m_editGitUrl->setText("https://gitlab.freedesktop.org/poppler/poppler.git");
            m_editBranchTag->setText("poppler-24.08.0");
        } else if (m_componentId == "tesseract") {
            m_editGitUrl->setText("https://github.com/tesseract-ocr/tesseract.git");
            m_editBranchTag->setText("5.4.1");
        } else if (m_componentId == "libarchive" || m_componentId == "zstd" || m_componentId == "xz") {
            m_editGitUrl->setText("https://github.com/libarchive/libarchive.git");
            m_editBranchTag->setText("v3.7.4");
        } else {
            m_editGitUrl->setText("https://github.com/tesseract-ocr/tesseract.git");
            m_editBranchTag->setText("main");
        }
        m_editGitUrl->setEnabled(false);
    } else if (mode == "git") {
        if (m_componentId == "poppler" || m_componentId == "pdfimages") {
            m_editGitUrl->setText("https://gitlab.freedesktop.org/poppler/poppler.git");
        } else if (m_componentId == "tesseract") {
            m_editGitUrl->setText("https://github.com/tesseract-ocr/tesseract.git");
        } else {
            m_editGitUrl->setText("https://github.com/libarchive/libarchive.git");
        }
        m_editBranchTag->setText("master");
        m_editGitUrl->setEnabled(false);
    } else {
        m_editGitUrl->setEnabled(true);
    }
}

void CustomSourceDialog::testConnection()
{
    QString url = m_editGitUrl->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("Test Connection"), tr("Please specify a valid Git repository URL."));
        return;
    }

    m_btnTestConnection->setEnabled(false);
    m_btnTestConnection->setText(tr("Testing..."));

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus status) {
        m_btnTestConnection->setEnabled(true);
        m_btnTestConnection->setText(tr("Test Connection"));

        if (status == QProcess::NormalExit && exitCode == 0) {
            QMessageBox::information(this, tr("Connection Successful"),
                tr("Successfully contacted the Git remote repository!"));
        } else {
            QMessageBox::warning(this, tr("Connection Failed"),
                tr("Failed to contact Git remote repository.<br>Please check the network connection and repository URL."));
        }
        proc->deleteLater();
    });

    proc->start("git", QStringList() << "ls-remote" << "--heads" << url);
}

CustomSourceConfig CustomSourceDialog::config() const
{
    return m_config;
}
