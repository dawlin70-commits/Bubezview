/**
 * @file fallback_notice_dialog.cpp
 * @brief Implementation of FallbackNoticeDialog.
 */

#include "fallback_notice_dialog.h"
#include "build_config.h"
#include "build_console_dialog.h"
#include "package_manager_helper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QIcon>
#include <QStyle>
#include <QApplication>

FallbackNoticeDialog::FallbackNoticeDialog(const QString &componentId,
                                           const QString &displayName,
                                           const QString &executablePath,
                                           QWidget *parent)
    : QDialog(parent)
    , m_componentId(componentId)
    , m_chkDontShowAgain(nullptr)
    , m_wantsCompilation(false)
{
    setWindowTitle(tr("Performance Optimization Notice"));
    setWindowIcon(QIcon(":/icons/info.svg"));
    setMinimumWidth(480);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header area with icon and informative text
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    auto *iconLabel = new QLabel(this);
    QIcon infoIcon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    iconLabel->setPixmap(infoIcon.pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    headerLayout->addWidget(iconLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(8);

    auto *titleLabel = new QLabel(this);
    titleLabel->setText(tr("<h3 style='margin:0;'>Performance Notice: %1</h3>").arg(displayName));
    textLayout->addWidget(titleLabel);

    auto *bodyLabel = new QLabel(this);
    bodyLabel->setWordWrap(true);
    bodyLabel->setText(tr(
        "<p>The <b>High-Performance</b> profile is enabled, but a custom source build for <b>%1</b> "
        "is not yet installed.</p>"
        "<p>BubezView will proceed using the standard system package:<br>"
        "<code>%2</code></p>"
        "<p style='color: #666; font-size: 11px;'>"
        "To achieve maximum speed with hardware optimizations (-O3, native CPU instructions), "
        "you can compile this component from source at any time.</p>"
    ).arg(displayName, executablePath));
    textLayout->addWidget(bodyLabel);

    headerLayout->addLayout(textLayout, 1);
    mainLayout->addLayout(headerLayout);

    // "Do not show again for this component" checkbox
    m_chkDontShowAgain = new QCheckBox(tr("Do not show this notice again for %1").arg(displayName), this);
    m_chkDontShowAgain->setChecked(false);
    mainLayout->addWidget(m_chkDontShowAgain);

    // Action buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    auto *btnCompile = new QPushButton(QIcon(":/icons/source_lightning.svg"), tr("Compile from Source Now"), this);
    btnCompile->setCursor(Qt::PointingHandCursor);
    connect(btnCompile, &QPushButton::clicked, this, &FallbackNoticeDialog::onCompileClicked);
    btnLayout->addWidget(btnCompile);

    auto *btnProceed = new QPushButton(tr("Use System Version"), this);
    btnProceed->setDefault(true);
    btnProceed->setCursor(Qt::PointingHandCursor);
    connect(btnProceed, &QPushButton::clicked, this, &FallbackNoticeDialog::onProceedClicked);
    btnLayout->addWidget(btnProceed);

    mainLayout->addLayout(btnLayout);
}

bool FallbackNoticeDialog::isNoticeSuppressed(const QString &componentId)
{
    if (componentId.isEmpty()) return false;
    QSettings settings;
    return settings.value(QString("Performance/suppressFallbackNotice_%1").arg(componentId.trimmed().toLower()), false).toBool();
}

void FallbackNoticeDialog::setNoticeSuppressed(const QString &componentId, bool suppress)
{
    if (componentId.isEmpty()) return;
    QSettings settings;
    settings.setValue(QString("Performance/suppressFallbackNotice_%1").arg(componentId.trimmed().toLower()), suppress);
}

bool FallbackNoticeDialog::checkAndPrompt(const QString &componentId,
                                          const QString &displayName,
                                          const QString &systemPath,
                                          QWidget *parent)
{
    QString id = componentId.trimmed().toLower();

    // 1. Only prompt if High-Performance profile is selected
    if (BuildConfig::instance().activeHardwareProfile() != BuildConfig::HardwareProfile::HighPerformance) {
        return true;
    }

    // 2. If a local source binary already exists, no fallback notice is needed
    if (BuildConfig::hasSourceBinary(id)) {
        return true;
    }

    // 3. If there is no system executable at all, this is a missing component (handled by regular install prompts)
    if (systemPath.isEmpty()) {
        return true;
    }

    // 4. Check if notice is suppressed by user for this specific component
    if (isNoticeSuppressed(id)) {
        return true;
    }

    // 5. Show modal prompt
    FallbackNoticeDialog dlg(id, displayName, systemPath, parent);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.wantsCompilation()) {
            BuildConsoleDialog console(parent);
            console.startBuildForComponent(id);
            console.exec();
            // After compilation finished, let the caller know to check binaries again or proceed
            return BuildConfig::hasSourceBinary(id);
        }
        return true;
    }

    return true;
}

void FallbackNoticeDialog::onProceedClicked()
{
    if (m_chkDontShowAgain && m_chkDontShowAgain->isChecked()) {
        setNoticeSuppressed(m_componentId, true);
    }
    m_wantsCompilation = false;
    accept();
}

void FallbackNoticeDialog::onCompileClicked()
{
    if (m_chkDontShowAgain && m_chkDontShowAgain->isChecked()) {
        setNoticeSuppressed(m_componentId, true);
    }
    m_wantsCompilation = true;
    accept();
}
