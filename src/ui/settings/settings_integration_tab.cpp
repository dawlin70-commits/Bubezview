/**
 * @file settings_integration_tab.cpp
 * @brief Implementation of the desktop integration settings tab.
 */

#include "settings_integration_tab.h"
#include "settings_appearance_tab.h"
#include "desktop_integrator.h"
#include "theme_manager.h"
#include "dialogs/frameless_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QFrame>
#include <QDialog>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QTimer>
#include <QSettings>
#include <QSignalBlocker>

namespace {

/**
 * @class SpaceShortcutTestDialog
 * @brief Interactive modal dialog that captures key presses to verify Spacebar responsiveness.
 */
class SpaceShortcutTestDialog : public FramelessDialog {
public:
    explicit SpaceShortcutTestDialog(QWidget *parent = nullptr)
        : FramelessDialog(parent)
        , m_spaceDetected(false)
    {
        setWindowTitle(SettingsIntegrationTab::tr("Spacebar Key Test"));
        resize(420, 160);
        setModal(true);

        auto *layout = contentLayout();
        layout->setSpacing(12);
        layout->setContentsMargins(4, 4, 4, 4);

        auto *lblTitle = new QLabel(SettingsIntegrationTab::tr("<h3>🧪 Testing Spacebar Key</h3>"), this);
        lblTitle->setAlignment(Qt::AlignCenter);
        layout->addWidget(lblTitle);

        m_lblStatus = new QLabel(
            SettingsIntegrationTab::tr("Press the <b>Spacebar</b> on your keyboard now to verify key capture."),
            this
        );
        m_lblStatus->setAlignment(Qt::AlignCenter);
        m_lblStatus->setWordWrap(true);
        layout->addWidget(m_lblStatus);

        auto *hBox = new QHBoxLayout();
        auto *btnCancel = new QPushButton(SettingsIntegrationTab::tr("Close"), this);
        btnCancel->setObjectName(QStringLiteral("IntegrationActionBtn"));
        btnCancel->setFocusPolicy(Qt::NoFocus);
        btnCancel->installEventFilter(this);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        hBox->addStretch();
        hBox->addWidget(btnCancel);
        layout->addLayout(hBox);

        setFocusPolicy(Qt::StrongFocus);
    }

    bool spaceDetected() const { return m_spaceDetected; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Space) {
                onSpacePressed();
                return true;
            }
        }
        return FramelessDialog::eventFilter(watched, event);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            onSpacePressed();
            return;
        }
        FramelessDialog::keyPressEvent(event);
    }

private:
    void onSpacePressed() {
        m_spaceDetected = true;
        m_lblStatus->setText(SettingsIntegrationTab::tr("<b style='color:#10b981; font-size: 13px;'>✅ Spacebar detected successfully!</b>"));
        QTimer::singleShot(600, this, &QDialog::accept);
    }

    QLabel *m_lblStatus = nullptr;
    bool m_spaceDetected = false;
};

/**
 * @class UnsupportedSystemGuideDialog
 * @brief Guided setup dialog providing instructions for configuring global shortcuts on unsupported desktops.
 */
class UnsupportedSystemGuideDialog : public FramelessDialog {
public:
    explicit UnsupportedSystemGuideDialog(QWidget *parent = nullptr)
        : FramelessDialog(parent)
    {
        setWindowTitle(SettingsIntegrationTab::tr("Shortcut Setup Guide (Unsupported Systems)"));
        resize(540, 390);
        setModal(true);

        auto *layout = contentLayout();
        layout->setSpacing(12);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setContentsMargins(20, 18, 20, 18);

        auto *lblHeader = new QLabel(SettingsIntegrationTab::tr("<h3>📋 Shortcut Setup for Unsupported Desktops</h3>"), this);
        layout->addWidget(lblHeader);

        auto *lblDesc = new QLabel(
            SettingsIntegrationTab::tr("On window managers or desktops without direct file manager plugins (e.g. XFCE, i3, Sway, LXQt), "
                                      "you can configure a global system shortcut (e.g. <b>Ctrl+Space</b> or <b>Super+Space</b>) "
                                      "to launch BubezView."),
            this
        );
        lblDesc->setWordWrap(true);
        layout->addWidget(lblDesc);

        auto *grpCmd = new QGroupBox(SettingsIntegrationTab::tr("Command to execute"), this);
        auto *cmdLayout = new QHBoxLayout(grpCmd);
        cmdLayout->setContentsMargins(10, 8, 10, 8);
        cmdLayout->setSpacing(8);

        QString cmdText = QCoreApplication::applicationFilePath();
        if (cmdText.isEmpty()) {
            cmdText = QStringLiteral("bubezview");
        }

        QLineEdit *editCmd = new QLineEdit(cmdText, grpCmd);
        editCmd->setReadOnly(true);
        editCmd->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; padding: 4px;"));
        cmdLayout->addWidget(editCmd, 1);

        QPushButton *btnCopy = new QPushButton(SettingsIntegrationTab::tr("Copy"), grpCmd);
        btnCopy->setObjectName(QStringLiteral("IntegrationActionBtn"));
        connect(btnCopy, &QPushButton::clicked, this, [btnCopy, cmdText]() {
            if (auto *cb = QGuiApplication::clipboard()) {
                cb->setText(cmdText);
            }
            btnCopy->setText(SettingsIntegrationTab::tr("Copied!"));
            QTimer::singleShot(1500, btnCopy, [btnCopy]() {
                btnCopy->setText(SettingsIntegrationTab::tr("Copy"));
            });
        });
        cmdLayout->addWidget(btnCopy);
        layout->addWidget(grpCmd);

        auto *lblSteps = new QLabel(
            SettingsIntegrationTab::tr("<b>Desktop-specific setup steps:</b><br>"
                                      "• <b>XFCE:</b> Settings → Keyboard → Application Shortcuts → Add → enter command → press <i>Ctrl+Space</i>.<br>"
                                      "• <b>GNOME:</b> Settings → Keyboard → View and Customize Shortcuts → Custom Shortcuts → Add.<br>"
                                      "• <b>KDE Plasma:</b> System Settings → Shortcuts → Custom Shortcuts → Edit → New Global Shortcut.<br>"
                                      "• <b>i3 / Sway:</b> Add <code>bindsym $mod+space exec %1</code> to your config file.")
                                      .arg(cmdText),
            this
        );
        lblSteps->setWordWrap(true);
        lblSteps->setTextFormat(Qt::RichText);
        layout->addWidget(lblSteps);

        layout->addStretch();

        auto *btnBox = new QHBoxLayout();
        btnBox->addStretch();
        auto *btnClose = new QPushButton(SettingsIntegrationTab::tr("Close"), this);
        btnClose->setObjectName(QStringLiteral("IntegrationActionBtn"));
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        btnBox->addWidget(btnClose);
        layout->addLayout(btnBox);
    }
};

} // namespace

static QString primaryFileManagerName()
{
    switch (DesktopIntegrator::currentDesktop()) {
    case DesktopIntegrator::DesktopEnv::KDE:
        return QStringLiteral("Dolphin");
    case DesktopIntegrator::DesktopEnv::GNOME:
        return QStringLiteral("Nautilus");
    case DesktopIntegrator::DesktopEnv::Cinnamon:
        return QStringLiteral("Nemo");
    case DesktopIntegrator::DesktopEnv::XFCE:
        return QStringLiteral("Thunar");
    default:
        return QStringLiteral("File Manager");
    }
}

static QFrame *createDivider(QWidget *parent)
{
    QFrame *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("RowDivider"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    return line;
}

SettingsIntegrationTab::SettingsIntegrationTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshStatuses();
    updateThemeStyles();
}

QWidget *SettingsIntegrationTab::createSettingRow(
    const QString &title,
    QWidget *control,
    QWidget *parent,
    QWidget *extraWidget,
    const QString &subtitle)
{
    QWidget *rowWidget = new QWidget(parent);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(10);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    QLabel *lblTitle = new QLabel(title, rowWidget);
    lblTitle->setObjectName(QStringLiteral("RowTitle"));
    textLayout->addWidget(lblTitle);

    if (!subtitle.trimmed().isEmpty()) {
        QLabel *lblSub = new QLabel(subtitle, rowWidget);
        lblSub->setObjectName(QStringLiteral("RowSubtitle"));
        lblSub->setWordWrap(true);
        textLayout->addWidget(lblSub);
    }

    rowLayout->addLayout(textLayout, 1);

    if (extraWidget) {
        rowLayout->addWidget(extraWidget, 0, Qt::AlignVCenter);
    }
    if (control) {
        rowLayout->addWidget(control, 0, Qt::AlignVCenter);
    }

    return rowWidget;
}

void SettingsIntegrationTab::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget(scrollArea);
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(14, 4, 14, 6);
    contentLayout->setSpacing(8);

    m_lblHeader = new QLabel(QString(tr("Detected desktop environment: <b>%1</b>")).arg(DesktopIntegrator::desktopName()), content);
    m_lblHeader->setObjectName(QStringLiteral("IntegrationHeaderLabel"));
    contentLayout->addWidget(m_lblHeader);

    const QString fmName = primaryFileManagerName();

    // 1. File Manager & Spacebar Integration Card
    m_grpQuickLook = new QGroupBox(tr("File Manager && Quick Look"), content);
    QVBoxLayout *layQuickLook = new QVBoxLayout(m_grpQuickLook);
    layQuickLook->setContentsMargins(12, 10, 12, 8);
    layQuickLook->setSpacing(4);

    m_switchSpace = new CapsuleToggleSwitch(m_grpQuickLook);
    connect(m_switchSpace, &QAbstractButton::toggled, this, &SettingsIntegrationTab::onSpacePreviewToggled);

    m_btnTestSpace = new QPushButton(tr("Test Spacebar"), m_grpQuickLook);
    m_btnTestSpace->setObjectName(QStringLiteral("IntegrationActionBtn"));
    m_btnTestSpace->setCursor(Qt::PointingHandCursor);
    connect(m_btnTestSpace, &QPushButton::clicked, this, &SettingsIntegrationTab::onTestSpaceClicked);

    m_switchFallback = new CapsuleToggleSwitch(m_grpQuickLook);
    connect(m_switchFallback, &QAbstractButton::toggled, this, &SettingsIntegrationTab::onFallbackShortcutToggled);

    m_btnGuide = new QPushButton(tr("Setup Guide..."), m_grpQuickLook);
    m_btnGuide->setObjectName(QStringLiteral("IntegrationActionBtn"));
    m_btnGuide->setCursor(Qt::PointingHandCursor);
    connect(m_btnGuide, &QPushButton::clicked, this, &SettingsIntegrationTab::onUnsupportedGuideClicked);

    QString row1Title;
    if (DesktopIntegrator::currentDesktop() == DesktopIntegrator::DesktopEnv::Other) {
        row1Title = tr("Spacebar preview in File Manager");
    } else {
        row1Title = tr("Spacebar preview in %1").arg(fmName);
    }

    QWidget *row1 = createSettingRow(
        row1Title,
        m_switchSpace,
        m_grpQuickLook,
        m_btnTestSpace
    );
    QWidget *row2 = createSettingRow(
        tr("Ctrl+Space shortcut (for unsupported systems)"),
        m_switchFallback,
        m_grpQuickLook,
        m_btnGuide
    );

    layQuickLook->addWidget(row1);
    layQuickLook->addWidget(createDivider(m_grpQuickLook));
    layQuickLook->addWidget(row2);
    contentLayout->addWidget(m_grpQuickLook);

    // 2. System Integration (Context Menu & Launcher) Card
    m_grpSystem = new QGroupBox(tr("System Integration"), content);
    QVBoxLayout *laySystem = new QVBoxLayout(m_grpSystem);
    laySystem->setContentsMargins(12, 10, 12, 8);
    laySystem->setSpacing(4);

    m_switchContextMenu = new CapsuleToggleSwitch(m_grpSystem);
    connect(m_switchContextMenu, &QAbstractButton::toggled, this, &SettingsIntegrationTab::onContextMenuToggled);

    m_switchAppMenu = new CapsuleToggleSwitch(m_grpSystem);
    connect(m_switchAppMenu, &QAbstractButton::toggled, this, &SettingsIntegrationTab::onAppMenuToggled);

    QWidget *rowCtx = createSettingRow(
        tr("Mouse right-click context menu"),
        m_switchContextMenu,
        m_grpSystem
    );
    QWidget *rowApp = createSettingRow(
        tr("Application launcher menu"),
        m_switchAppMenu,
        m_grpSystem
    );

    laySystem->addWidget(rowCtx);
    laySystem->addWidget(createDivider(m_grpSystem));
    laySystem->addWidget(rowApp);
    contentLayout->addWidget(m_grpSystem);

    // 3. Office Documents Card
    m_grpOffice = new QGroupBox(tr("Office Documents"), content);
    QVBoxLayout *layOffice = new QVBoxLayout(m_grpOffice);
    layOffice->setContentsMargins(12, 10, 12, 8);
    layOffice->setSpacing(4);

    m_switchLibreOffice = new CapsuleToggleSwitch(m_grpOffice);
    connect(m_switchLibreOffice, &QAbstractButton::toggled, this, &SettingsIntegrationTab::onUseLibreOfficeToggled);

    QWidget *rowOffice = createSettingRow(
        tr("High-fidelity preview (requires LibreOffice)"),
        m_switchLibreOffice,
        m_grpOffice
    );

    layOffice->addWidget(rowOffice);
    contentLayout->addWidget(m_grpOffice);

    contentLayout->addStretch();
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void SettingsIntegrationTab::refreshStatuses()
{
    const QSignalBlocker b1(m_switchSpace);
    const QSignalBlocker b2(m_switchFallback);
    const QSignalBlocker b3(m_switchContextMenu);
    const QSignalBlocker b4(m_switchAppMenu);
    const QSignalBlocker b5(m_switchLibreOffice);

    if (m_lblHeader) {
        m_lblHeader->setText(QString(tr("Detected desktop environment: <b>%1</b>")).arg(DesktopIntegrator::desktopName()));
    }

    const auto de = DesktopIntegrator::currentDesktop();
    bool spaceEnabled = false;
    bool fallbackActive = false;

    QSettings settings("Bubez", "BubezView");
    fallbackActive = settings.value(QStringLiteral("Integration/FallbackShortcut"), false).toBool();

    if (de == DesktopIntegrator::DesktopEnv::KDE) {
        spaceEnabled = DesktopIntegrator::isDolphinEnabled();
        if (DesktopIntegrator::currentDolphinShortcut() == QLatin1String("Ctrl+Space")) {
            fallbackActive = true;
        }
    } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
        spaceEnabled = DesktopIntegrator::isGnomeNemoEnabled();
    } else {
        spaceEnabled = DesktopIntegrator::isDolphinEnabled() || DesktopIntegrator::isGnomeNemoEnabled();
        if (DesktopIntegrator::currentDolphinShortcut() == QLatin1String("Ctrl+Space")) {
            fallbackActive = true;
        }
    }

    if (m_switchSpace) {
        m_switchSpace->setCheckedAnimated(spaceEnabled);
    }
    if (m_switchFallback) {
        m_switchFallback->setCheckedAnimated(fallbackActive);
        m_switchFallback->setEnabled(true);
    }

    if (m_switchContextMenu) {
        m_switchContextMenu->setCheckedAnimated(DesktopIntegrator::isContextMenuEnabled());
    }
    if (m_switchAppMenu) {
        m_switchAppMenu->setCheckedAnimated(DesktopIntegrator::isAppMenuInstalled());
    }

    if (m_switchLibreOffice) {
        m_switchLibreOffice->setCheckedAnimated(ThemeManager::useLibreOffice());
    }
}

void SettingsIntegrationTab::onSpacePreviewToggled(bool checked)
{
    const auto de = DesktopIntegrator::currentDesktop();
    const QString shortcut = (m_switchFallback && m_switchFallback->isChecked())
        ? QStringLiteral("Ctrl+Space")
        : QStringLiteral("Space");

    if (checked) {
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            DesktopIntegrator::enableDolphin(shortcut);
        } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
            DesktopIntegrator::enableGnomeNemo();
        } else {
            DesktopIntegrator::enableDolphin(shortcut);
            DesktopIntegrator::enableGnomeNemo();
        }
    } else {
        if (m_switchFallback && m_switchFallback->isChecked() &&
            (de == DesktopIntegrator::DesktopEnv::KDE || de == DesktopIntegrator::DesktopEnv::Other)) {
            DesktopIntegrator::enableDolphin(QStringLiteral("Ctrl+Space"));
        } else {
            if (de == DesktopIntegrator::DesktopEnv::KDE) {
                DesktopIntegrator::disableDolphin();
            } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
                DesktopIntegrator::disableGnomeNemo();
            } else {
                DesktopIntegrator::disableDolphin();
                DesktopIntegrator::disableGnomeNemo();
            }
        }
    }
}

void SettingsIntegrationTab::onFallbackShortcutToggled(bool checked)
{
    const auto de = DesktopIntegrator::currentDesktop();
    const QString shortcut = checked ? QStringLiteral("Ctrl+Space") : QStringLiteral("Space");

    if (de == DesktopIntegrator::DesktopEnv::KDE || de == DesktopIntegrator::DesktopEnv::Other) {
        if (m_switchSpace && m_switchSpace->isChecked()) {
            DesktopIntegrator::enableDolphin(shortcut);
        } else if (checked) {
            DesktopIntegrator::enableDolphin(QStringLiteral("Ctrl+Space"));
        } else {
            DesktopIntegrator::disableDolphin();
        }
    }

    QSettings settings("Bubez", "BubezView");
    settings.setValue(QStringLiteral("Integration/FallbackShortcut"), checked);
}

void SettingsIntegrationTab::onContextMenuToggled(bool checked)
{
    if (checked) {
        DesktopIntegrator::enableContextMenu();
    } else {
        DesktopIntegrator::disableContextMenu();
    }
}

void SettingsIntegrationTab::onAppMenuToggled(bool checked)
{
    if (checked) {
        DesktopIntegrator::installAppMenu();
    } else {
        DesktopIntegrator::uninstallAppMenu();
    }
}

void SettingsIntegrationTab::onUseLibreOfficeToggled(bool checked)
{
    ThemeManager::setUseLibreOffice(checked);
}

void SettingsIntegrationTab::onTestSpaceClicked()
{
    SpaceShortcutTestDialog dlg(this);
    dlg.exec();
}

void SettingsIntegrationTab::onUnsupportedGuideClicked()
{
    UnsupportedSystemGuideDialog dlg(this);
    dlg.exec();
}

void SettingsIntegrationTab::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString titleColor;
    QString subColor;
    QString dividerColor;
    QString btnHoverBg;
    QString btnHoverBorder;
    QString btnPressedBg;

    if (active == ThemePreset::ZincGray) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        titleColor = QStringLiteral("#ffffff");
        subColor = QStringLiteral("rgba(255, 255, 255, 0.70)");
        dividerColor = QStringLiteral("rgba(255, 255, 255, 0.09)");
        btnHoverBg = QStringLiteral("rgba(255, 255, 255, 0.15)");
        btnHoverBorder = QStringLiteral("rgba(255, 255, 255, 0.35)");
        btnPressedBg = QStringLiteral("rgba(255, 255, 255, 0.08)");
    } else if (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.70), stop:0.5 rgba(246, 248, 251, 0.65), stop:1 rgba(238, 241, 246, 0.60))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.09)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#334155");
        titleColor = QStringLiteral("#1e293b");
        subColor = QStringLiteral("#64748b");
        dividerColor = QStringLiteral("rgba(15, 23, 42, 0.08)");
        btnHoverBg = QStringLiteral("#f1f5f9");
        btnHoverBorder = QStringLiteral("#0284c7");
        btnPressedBg = QStringLiteral("#e2e8f0");
    } else {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        titleColor = QStringLiteral("#f8fafc");
        subColor = QStringLiteral("rgba(226, 232, 240, 0.72)");
        dividerColor = QStringLiteral("rgba(255, 255, 255, 0.08)");
        btnHoverBg = QStringLiteral("rgba(56, 189, 248, 0.18)");
        btnHoverBorder = QStringLiteral("rgba(56, 189, 248, 0.50)");
        btnPressedBg = QStringLiteral("rgba(56, 189, 248, 0.10)");
    }

    const QString style = QString(
        "QLabel#IntegrationHeaderLabel {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  margin-bottom: 0px;"
        "}"
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "  background: %3;"
        "  margin-top: 10px;"
        "  padding: 10px 12px 8px 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 12px;"
        "  top: 1px;"
        "  padding: 1px 10px;"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel#RowTitle {"
        "  color: %1;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  background: transparent;"
        "}"
        "QLabel#RowSubtitle {"
        "  color: %7;"
        "  font-size: 11px;"
        "  font-weight: 400;"
        "  background: transparent;"
        "}"
        "QFrame#RowDivider {"
        "  background-color: %8;"
        "  max-height: 1px;"
        "  min-height: 1px;"
        "  border: none;"
        "  margin: 2px 0px;"
        "}"
        "QPushButton#IntegrationActionBtn {"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  padding: 3px 10px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#IntegrationActionBtn:hover {"
        "  background: %9;"
        "  border-color: %10;"
        "}"
        "QPushButton#IntegrationActionBtn:pressed {"
        "  background: %11;"
        "}"
    ).arg(titleColor, cardBorder, cardBg, pillBg, pillColor, pillBorder, subColor, dividerColor,
          btnHoverBg, btnHoverBorder, btnPressedBg);

    setStyleSheet(style);

    if (m_switchSpace) m_switchSpace->update();
    if (m_switchFallback) m_switchFallback->update();
    if (m_switchContextMenu) m_switchContextMenu->update();
    if (m_switchAppMenu) m_switchAppMenu->update();
    if (m_switchLibreOffice) m_switchLibreOffice->update();
}
