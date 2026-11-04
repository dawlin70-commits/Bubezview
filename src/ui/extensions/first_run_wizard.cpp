/**
 * @file first_run_wizard.cpp
 * @brief Implementation of the guided FirstRunWizard dialog.
 */

#include "first_run_wizard.h"
#include "desktop_integrator.h"
#include "build_config.h"
#include "theme_manager.h"
#include "vector_icons.h"
#include "cpu_info.h"
#include "build_console_dialog.h"
#include "package_manager_helper.h"
#include "settings_dialog.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QKeyEvent>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QGridLayout>
#include <QSize>
#include <QPixmap>

#ifndef BUBEZ_OPT_LEVEL
#define BUBEZ_OPT_LEVEL "O2"
#endif

namespace {

/**
 * @class SpaceShortcutTestDialog
 * @brief Interactive modal dialog that captures key presses to verify Spacebar responsiveness.
 */
class SpaceShortcutTestDialog : public QDialog {
public:
    explicit SpaceShortcutTestDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_spaceDetected(false)
    {
        setWindowTitle(FirstRunWizard::tr("Test Spacebar Shortcut"));
        resize(440, 180);
        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(12);
        layout->setContentsMargins(20, 18, 20, 18);

        auto *lblTitle = new QLabel(FirstRunWizard::tr("<h3>🧪 Testing Spacebar Key</h3>"), this);
        lblTitle->setAlignment(Qt::AlignCenter);
        layout->addWidget(lblTitle);

        m_lblStatus = new QLabel(
            FirstRunWizard::tr("Please press the <b>Spacebar</b> on your keyboard now to verify key capture."),
            this
        );
        m_lblStatus->setAlignment(Qt::AlignCenter);
        m_lblStatus->setWordWrap(true);
        layout->addWidget(m_lblStatus);

        auto *hBox = new QHBoxLayout();
        auto *btnCancel = new QPushButton(FirstRunWizard::tr("Cancel / Having Issues"), this);
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
                m_spaceDetected = true;
                m_lblStatus->setText(FirstRunWizard::tr("<b style='color:#10b981;'>✅ Spacebar detected successfully!</b>"));
                QTimer::singleShot(350, this, &QDialog::accept);
                return true;
            }
        }
        return QDialog::eventFilter(watched, event);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            m_spaceDetected = true;
            m_lblStatus->setText(FirstRunWizard::tr("<b style='color:#10b981;'>✅ Spacebar detected successfully!</b>"));
            QTimer::singleShot(350, this, &QDialog::accept);
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    QLabel *m_lblStatus = nullptr;
    bool m_spaceDetected = false;
};

} // namespace

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("BubezView Setup Wizard"));
    setWizardStyle(QWizard::ModernStyle);

    // Never show back button on start page
    setOption(QWizard::NoBackButtonOnStartPage, true);

    // Spacious and responsive size for standard screens and high-DPI displays
    setMinimumSize(680, 500);
    int targetW = 760;
    int targetH = 545;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        targetW = qMin(targetW, qMax(680, avail.width() - 40));
        targetH = qMin(targetH, qMax(500, avail.height() - 60));
    }
    resize(targetW, targetH);

    // Register 3 pages (Page_Welcome -> Page_Spacebar -> Page_Modules)
    setPage(Page_Welcome, createWelcomePage());
    setPage(Page_Spacebar, createSpacebarPage());
    setPage(Page_Modules, createModulesPage());
    setStartId(Page_Welcome);

    // CustomButton1 for "Quick Start" on Page_Spacebar
    setOption(QWizard::HaveCustomButton1, true);
    setButtonText(QWizard::CustomButton1, tr("⚡ Quick Start (Recommended)"));
    setButtonText(QWizard::NextButton, tr("Next >"));
    setButtonText(QWizard::BackButton, tr("< Back"));
    setButtonText(QWizard::FinishButton, tr("Finish && Launch BubezView"));
    setButtonText(QWizard::CancelButton, tr("Cancel"));

    connect(this, &QWizard::customButtonClicked, this, [this](int which) {
        if (which == QWizard::CustomButton1) {
            accept();
        }
    });

    auto alignHeaderSubtitle = [this]() {
        for (QGridLayout *grid : findChildren<QGridLayout *>()) {
            if (grid->parentWidget() != this && grid->columnCount() >= 2 && grid->columnMinimumWidth(0) > 0) {
                grid->setColumnMinimumWidth(0, 0);
                grid->setVerticalSpacing(2);
                grid->setContentsMargins(14, 6, 14, 6);
            }
        }
    };

    connect(this, &QWizard::currentIdChanged, this, [this, alignHeaderSubtitle](int id) {
        alignHeaderSubtitle();

        // Ensure Back button is hidden on first page
        if (QAbstractButton *backBtn = button(QWizard::BackButton)) {
            backBtn->setVisible(id != Page_Welcome);
        }

        // Quick Start button is visible ONLY on Page_Spacebar
        if (QAbstractButton *quickBtn = button(QWizard::CustomButton1)) {
            quickBtn->setVisible(id == Page_Spacebar);
            if (id == Page_Spacebar) {
                quickBtn->setStyleSheet(
                    "QPushButton { background-color: #0284c7; color: #ffffff; font-weight: 700; "
                    "padding: 5px 14px; border-radius: 5px; border: 1px solid #0369a1; }"
                    "QPushButton:hover { background-color: #0369a1; }"
                );
            }
        }

        // Next button text adjusts per page
        if (id == Page_Welcome) {
            setButtonText(QWizard::NextButton, tr("Next >"));
        } else if (id == Page_Spacebar) {
            setButtonText(QWizard::NextButton, tr("Additional Options >"));
        }
    });
    QTimer::singleShot(0, this, alignHeaderSubtitle);
}

QWizardPage *FirstRunWizard::createWelcomePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Welcome to BubezView"));
    page->setSubTitle(tr("Ultra-fast native Linux file viewer and instant preview studio."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->setContentsMargins(6, 4, 6, 4);

    // =========================================================================
    // 1. Hero Welcome Card (Official BubezView Logo + Value Prop + Hardware Pills)
    // =========================================================================
    auto *heroFrame = new QFrame(page);
    heroFrame->setObjectName("wizardHeroFrame");
    heroFrame->setStyleSheet(
        "QFrame#wizardHeroFrame {"
        "  background-color: rgba(2, 132, 199, 0.08);"
        "  border: 1px solid rgba(56, 189, 248, 0.30);"
        "  border-radius: 10px;"
        "}"
    );
    auto *heroLayout = new QHBoxLayout(heroFrame);
    heroLayout->setContentsMargins(18, 14, 18, 14);
    heroLayout->setSpacing(18);

    auto *lblLogo = new QLabel(heroFrame);
    lblLogo->setAlignment(Qt::AlignCenter);
    QPixmap brandPix(":/icons/bubezview.png");
    if (!brandPix.isNull()) {
        lblLogo->setPixmap(brandPix.scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        lblLogo->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Document, QColor("#38bdf8")).pixmap(56, 56));
    }
    lblLogo->setFixedSize(72, 72);
    heroLayout->addWidget(lblLogo, 0, Qt::AlignVCenter);

    auto *heroRight = new QVBoxLayout();
    heroRight->setSpacing(8);

    auto *lblHeroTitle = new QLabel(
        tr("<span style='font-size: 15px; font-weight: 700;'>Ready for Instant File Previews</span><br>"
           "<span style='font-size: 12px; color: #64748b;'>"
           "Core viewers (PDF, Images, Text, Archives, Audio/Video) work out of the box.<br>"
           "Heavy studio tools are installed on demand with a single click only when you need them.</span>"),
        heroFrame
    );
    lblHeroTitle->setWordWrap(true);
    lblHeroTitle->setTextFormat(Qt::RichText);
    heroRight->addWidget(lblHeroTitle);

    // Hardware & Desktop Pill Badges Row
    QString deName = DesktopIntegrator::desktopName();
    QString cpuModel = CpuInfo::modelName();
    if (cpuModel.isEmpty()) {
        cpuModel = tr("Generic x86-64 Processor");
    }

    auto *pillsRow = new QHBoxLayout();
    pillsRow->setSpacing(8);
    pillsRow->setContentsMargins(0, 2, 0, 0);

    auto makeBadge = [heroFrame](const QString &text, const QString &accentHex) {
        auto *badge = new QLabel(text, heroFrame);
        badge->setTextFormat(Qt::RichText);
        badge->setStyleSheet(QString(
            "background-color: rgba(15, 23, 42, 0.35);"
            "border: 1px solid %1;"
            "border-radius: 9px;"
            "padding: 3px 10px;"
            "font-size: 11px; font-weight: 600;"
        ).arg(accentHex));
        return badge;
    };

    pillsRow->addWidget(makeBadge(QString("🎨 %1").arg(deName), "rgba(56, 189, 248, 0.45)"));
    pillsRow->addWidget(makeBadge(
        QString("⚡ %1 (%2T)").arg(cpuModel, QString::number(CpuInfo::logicalCores())),
        "rgba(16, 185, 129, 0.45)"
    ));
    pillsRow->addWidget(makeBadge(tr("🚀 C++17 / Qt6 Native"), "rgba(139, 92, 246, 0.45)"));
    pillsRow->addStretch(1);

    heroRight->addLayout(pillsRow);
    heroLayout->addLayout(heroRight, 1);
    layout->addWidget(heroFrame);

    // =========================================================================
    // 2. Visual Style & Live Theme Cards
    // =========================================================================
    auto *groupTheme = new QGroupBox(tr("Visual Style && Theme (Click for Live Preview)"), page);
    auto *themeLayout = new QVBoxLayout(groupTheme);
    themeLayout->setContentsMargins(14, 12, 14, 12);
    themeLayout->setSpacing(10);

    m_comboTheme = new QComboBox(groupTheme);
    m_comboTheme->addItem(tr("Dark Slate"), "dark_slate");
    m_comboTheme->addItem(tr("Breeze Dark"), "breeze_dark");
    m_comboTheme->addItem(tr("Modern Light"), "modern_light");
    m_comboTheme->addItem(tr("Breeze Light"), "breeze_light");
    m_comboTheme->setVisible(false);

    auto *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(8);

    m_themeCardGroup = new QButtonGroup(this);
    m_themeCardGroup->setExclusive(true);

    auto makeThemeCard = [groupTheme](const QString &title, const QString &subtitle) {
        auto *btn = new QPushButton(QString("%1\n%2").arg(title, subtitle), groupTheme);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(48);
        return btn;
    };

    m_btnThemeDarkSlate = makeThemeCard(tr("🌙 Dark Slate"), tr("Deep slate (#22262b)"));
    m_btnThemeBreezeDark = makeThemeCard(tr("💎 Breeze Dark"), tr("KDE dark (#181b1d)"));
    m_btnThemeModernLight = makeThemeCard(tr("☀️ Modern Light"), tr("Warm silver (#ececf0)"));
    m_btnThemeBreezeLight = makeThemeCard(tr("🌤️ Breeze Light"), tr("Clean light (#eff0f1)"));

    m_themeCardGroup->addButton(m_btnThemeDarkSlate, 0);
    m_themeCardGroup->addButton(m_btnThemeBreezeDark, 1);
    m_themeCardGroup->addButton(m_btnThemeModernLight, 2);
    m_themeCardGroup->addButton(m_btnThemeBreezeLight, 3);

    cardsRow->addWidget(m_btnThemeDarkSlate, 1);
    cardsRow->addWidget(m_btnThemeBreezeDark, 1);
    cardsRow->addWidget(m_btnThemeModernLight, 1);
    cardsRow->addWidget(m_btnThemeBreezeLight, 1);
    themeLayout->addLayout(cardsRow);

    connect(m_btnThemeDarkSlate, &QPushButton::clicked, this, [this]() { selectLiveTheme("dark_slate"); });
    connect(m_btnThemeBreezeDark, &QPushButton::clicked, this, [this]() { selectLiveTheme("breeze_dark"); });
    connect(m_btnThemeModernLight, &QPushButton::clicked, this, [this]() { selectLiveTheme("modern_light"); });
    connect(m_btnThemeBreezeLight, &QPushButton::clicked, this, [this]() { selectLiveTheme("breeze_light"); });

    m_chkFilmstrip = new QCheckBox(
        tr("Show bottom thumbnail filmstrip when browsing folders or multi-page PDFs"),
        groupTheme
    );
    m_chkFilmstrip->setCursor(Qt::PointingHandCursor);
    m_chkFilmstrip->setStyleSheet("font-size: 11px;");
    m_chkFilmstrip->setChecked(false);
    themeLayout->addWidget(m_chkFilmstrip);

    QString currentThemeCode = ThemeManager::presetToString(ThemeManager::currentPreset());
    selectLiveTheme(currentThemeCode);

    layout->addWidget(groupTheme);
    layout->addStretch(1);

    return page;
}

void FirstRunWizard::selectLiveTheme(const QString &themeCode)
{
    if (m_comboTheme) {
        int idx = m_comboTheme->findData(themeCode);
        if (idx >= 0) {
            m_comboTheme->setCurrentIndex(idx);
        }
    }

    auto styleCard = [](QPushButton *btn, bool active) {
        if (!btn) return;
        btn->setChecked(active);
        if (active) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(2, 132, 199, 0.16);"
                "  border: 2px solid #0284c7;"
                "  border-radius: 8px;"
                "  padding: 5px 8px;"
                "  font-weight: 700;"
                "  font-size: 11px;"
                "}"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(148, 163, 184, 0.08);"
                "  border: 1px solid rgba(148, 163, 184, 0.30);"
                "  border-radius: 8px;"
                "  padding: 5px 8px;"
                "  font-weight: 500;"
                "  font-size: 11px;"
                "}"
                "QPushButton:hover {"
                "  border: 1px solid rgba(56, 189, 248, 0.65);"
                "}"
            );
        }
    };

    styleCard(m_btnThemeDarkSlate, themeCode == QLatin1String("dark_slate"));
    styleCard(m_btnThemeBreezeDark, themeCode == QLatin1String("breeze_dark"));
    styleCard(m_btnThemeModernLight, themeCode == QLatin1String("modern_light"));
    styleCard(m_btnThemeBreezeLight, themeCode == QLatin1String("breeze_light"));

    ThemeManager::setPreset(ThemeManager::stringToPreset(themeCode));
    ThemeManager::applyTheme(qApp, qobject_cast<QMainWindow *>(parentWidget()));
}

QWizardPage *FirstRunWizard::createSpacebarPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Instant Preview Setup"));
    page->setSubTitle(tr("Press Space on any file in your file manager to open an immediate preview."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);
    layout->setContentsMargins(6, 4, 6, 4);

    const DesktopIntegrator::DesktopEnv currentDe = DesktopIntegrator::currentDesktop();
    const bool isKde = (currentDe == DesktopIntegrator::DesktopEnv::KDE);
    QString deName = DesktopIntegrator::desktopName();

    QString projectTrigger = QDir(QCoreApplication::applicationDirPath()).filePath("../trigger_bubez.sh");
    QString triggerPath = QDir::homePath() + "/.local/share/bubezview/trigger_bubez.sh";
    if (QFile::exists(projectTrigger)) {
        triggerPath = QFileInfo(projectTrigger).canonicalFilePath();
    }

    // =========================================================================
    // Dedicated Centerpiece Card: Only the Test Button
    // =========================================================================
    auto *groupSpace = new QGroupBox(tr("Spacebar Key Capture Test"), page);
    auto *spaceLayout = new QVBoxLayout(groupSpace);
    spaceLayout->setContentsMargins(18, 16, 18, 16);
    spaceLayout->setSpacing(14);

    auto *lblSpaceIntro = new QLabel(groupSpace);
    lblSpaceIntro->setTextFormat(Qt::RichText);
    lblSpaceIntro->setWordWrap(true);
    lblSpaceIntro->setStyleSheet("font-size: 12px; line-height: 1.4;");

    if (isKde) {
        lblSpaceIntro->setText(
            tr("In <b>KDE Plasma (Dolphin)</b>, pressing <b>Space</b> on any file opens its instant preview, "
               "and <b>Esc</b> closes it.<br>Click below to verify that Spacebar capture is working properly.")
        );
    } else {
        lblSpaceIntro->setText(
            tr("On <b>%1</b>, click below to verify Spacebar key capture in your session.")
                .arg(deName)
        );
    }
    spaceLayout->addWidget(lblSpaceIntro);

    // Single Test Button Row
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    m_btnTestShortcut = new QPushButton(tr("🧪 Test Spacebar Key"), groupSpace);
    m_btnTestShortcut->setCursor(Qt::PointingHandCursor);
    m_btnTestShortcut->setMinimumHeight(44);
    m_btnTestShortcut->setStyleSheet(
        "QPushButton {"
        "  background-color: #0284c7; color: #ffffff;"
        "  font-weight: 700; font-size: 13px;"
        "  padding: 8px 24px; border-radius: 6px; border: 1px solid #0369a1;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0369a1;"
        "}"
    );
    btnRow->addWidget(m_btnTestShortcut);
    btnRow->addStretch(1);
    spaceLayout->addLayout(btnRow);

    // Status / Result Feedback Label
    m_lblTestResult = new QLabel(
        tr("<span style='color: #64748b; font-size: 11px;'>Click the test button above and press the Spacebar on your keyboard.</span>"),
        groupSpace
    );
    m_lblTestResult->setTextFormat(Qt::RichText);
    m_lblTestResult->setWordWrap(true);
    spaceLayout->addWidget(m_lblTestResult);

    // Trigger Script Fallback Box: HIDDEN initially, shown if test fails or conflict reported
    m_triggerBox = new QWidget(groupSpace);
    auto *trigLayout = new QVBoxLayout(m_triggerBox);
    trigLayout->setContentsMargins(12, 10, 12, 10);
    trigLayout->setSpacing(8);
    m_triggerBox->setStyleSheet(
        "background-color: rgba(245, 158, 11, 0.08);"
        "border: 1px solid rgba(245, 158, 11, 0.35);"
        "border-radius: 8px;"
    );

    auto *lblTrigDesc = new QLabel(
        tr("<b>Trigger Fallback:</b> In your system keyboard shortcuts (KDE Plasma / GNOME / XFCE Shortcuts), "
           "bind your desired shortcut (e.g. <b>Ctrl + Space</b>) to this script:"),
        m_triggerBox
    );
    lblTrigDesc->setTextFormat(Qt::RichText);
    lblTrigDesc->setWordWrap(true);
    lblTrigDesc->setStyleSheet("border: none; background: transparent; font-size: 11px;");
    trigLayout->addWidget(lblTrigDesc);

    auto *trigPathRow = new QHBoxLayout();
    trigPathRow->setSpacing(8);

    m_lblTriggerHelp = new QLabel(
        QString("<code style='color:#0284c7;'>%1</code>").arg(triggerPath),
        m_triggerBox
    );
    m_lblTriggerHelp->setTextFormat(Qt::RichText);
    m_lblTriggerHelp->setWordWrap(true);
    m_lblTriggerHelp->setStyleSheet("border: none; background: transparent; font-size: 11px;");
    trigPathRow->addWidget(m_lblTriggerHelp, 1);

    m_btnCopyTrigger = new QPushButton(tr("📋 Copy Path"), m_triggerBox);
    m_btnCopyTrigger->setCursor(Qt::PointingHandCursor);
    m_btnCopyTrigger->setStyleSheet("font-size: 11px; font-weight: 600; padding: 4px 12px;");
    trigPathRow->addWidget(m_btnCopyTrigger);

    trigLayout->addLayout(trigPathRow);

    connect(m_btnCopyTrigger, &QPushButton::clicked, this, [this, triggerPath]() {
        if (QClipboard *cb = QGuiApplication::clipboard()) {
            cb->setText(triggerPath);
        }
        if (m_btnCopyTrigger) {
            m_btnCopyTrigger->setText(tr("✅ Copied!"));
            QTimer::singleShot(1800, this, [this]() {
                if (m_btnCopyTrigger) {
                    m_btnCopyTrigger->setText(tr("📋 Copy Path"));
                }
            });
        }
    });

    m_triggerBox->setVisible(false);
    spaceLayout->addWidget(m_triggerBox);

    connect(m_btnTestShortcut, &QPushButton::clicked, this, [this]() {
        SpaceShortcutTestDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && dlg.spaceDetected()) {
            m_lblTestResult->setText(
                tr("<b style='color:#10b981; font-size: 12px;'>✅ Spacebar verified and functioning properly!</b>")
            );
            m_useTriggerFallback = false;
            if (m_triggerBox) {
                m_triggerBox->setVisible(false);
            }
        } else {
            m_lblTestResult->setText(
                tr("<b style='color:#f59e0b; font-size: 12px;'>⚠️ Spacebar conflict or issue reported — copy the trigger script below to bind in system shortcuts:</b>")
            );
            m_useTriggerFallback = true;
            if (m_triggerBox) {
                m_triggerBox->setVisible(true);
            }
        }
    });

    layout->addWidget(groupSpace);

    // Friendly reminder note about Quick Start vs Additional Options
    auto *lblHelp = new QLabel(
        tr("💡 <b>Ready to go?</b> Click <b>⚡ Quick Start (Recommended)</b> below to launch immediately, "
           "or <b>Additional Options &gt;</b> to customize background modules and CPU tuning."),
        page
    );
    lblHelp->setTextFormat(Qt::RichText);
    lblHelp->setWordWrap(true);
    lblHelp->setStyleSheet(
        "color: #0284c7; font-size: 11px; padding: 8px 12px; "
        "background-color: rgba(2, 132, 199, 0.08); border: 1px solid rgba(2, 132, 199, 0.25); "
        "border-radius: 6px;"
    );
    layout->addWidget(lblHelp);
    layout->addStretch(1);

    return page;
}

QWizardPage *FirstRunWizard::createModulesPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Modules, Performance & Navigation"));
    page->setSubTitle(tr("Fine-tune background modules, CPU optimization, and window behavior."));

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(6, 4, 6, 4);
    pageLayout->setSpacing(8);

    // =========================================================================
    // 1. On-Demand Modules & Presets (Default: Minimal & On-Demand)
    // =========================================================================
    auto *groupModules = new QGroupBox(tr("1. Functional Modules && On-Demand Extensions"), page);
    auto *modulesLayout = new QVBoxLayout(groupModules);
    modulesLayout->setContentsMargins(12, 8, 12, 8);
    modulesLayout->setSpacing(6);

    auto *presetsGrid = new QHBoxLayout();
    presetsGrid->setSpacing(16);

    m_radioPresetMinimal = new QRadioButton(groupModules);
    m_radioPresetMinimal->setCursor(Qt::PointingHandCursor);
    m_radioPresetMinimal->setToolTip(tr("Core viewers active immediately. Heavy tools install on demand only when needed."));

    m_radioPresetFull = new QRadioButton(groupModules);
    m_radioPresetFull->setCursor(Qt::PointingHandCursor);
    m_radioPresetFull->setToolTip(tr("Pre-install all optional studio extensions right now."));

    m_radioPresetCustom = new QRadioButton(tr("⚙️ Custom Selection"), groupModules);
    m_radioPresetCustom->setCursor(Qt::PointingHandCursor);
    m_radioPresetCustom->setToolTip(tr("Select specific extension categories below to install now."));

    presetsGrid->addWidget(m_radioPresetMinimal);
    presetsGrid->addWidget(m_radioPresetFull);
    presetsGrid->addWidget(m_radioPresetCustom);
    presetsGrid->addStretch(1);
    modulesLayout->addLayout(presetsGrid);

    auto *sep = new QFrame(groupModules);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setStyleSheet("color: rgba(148, 163, 184, 0.25); margin: 2px 0;");
    modulesLayout->addWidget(sep);

    auto *gridCategories = new QGridLayout();
    gridCategories->setHorizontalSpacing(20);
    gridCategories->setVerticalSpacing(5);
    gridCategories->setContentsMargins(4, 2, 4, 2);

    auto makeCheckBox = [groupModules](VectorIcons::Type iconType, const QString &text, const QString &tooltip) {
        QString safeText = text;
        safeText.replace(QRegularExpression("(?<!&)&(?!&)"), "&&");
        auto *chk = new QCheckBox(safeText, groupModules);
        chk->setIcon(VectorIcons::createIcon(iconType));
        chk->setIconSize(QSize(17, 17));
        chk->setToolTip(tooltip);
        chk->setStyleSheet("font-weight: 600; font-size: 11px;");
        chk->setCursor(Qt::PointingHandCursor);
        return chk;
    };

    m_chkCatDocs = makeCheckBox(
        VectorIcons::Type::Pdf,
        tr("Documents & Office (LibreOffice, OCR, QPDF, Signatures)"),
        tr("Includes LibreOffice conversion, Tesseract OCR, QPDF repair, and PAdES digital signatures.")
    );
    m_chkCatArchives = makeCheckBox(
        VectorIcons::Type::Zip,
        tr("Archives & Disk Images (7-Zip, Multi-Core, ISO)"),
        tr("Includes 7-Zip multi-core compression, encrypted archives, and ISO/IMG disk tools.")
    );
    m_chkCatImages = makeCheckBox(
        VectorIcons::Type::Image,
        tr("Images & Graphics (Extended Codecs: WebP, TIFF, etc.)"),
        tr("Includes extended image decoders for WebP, TIFF, TGA, ICNS, PSD, and JPEG-XL.")
    );
    m_chkCatVideo = makeCheckBox(
        VectorIcons::Type::FolderVideos,
        tr("Video & Movies (MPV GPU Acceleration)"),
        tr("Includes high-performance libmpv video player with hardware GPU decoding.")
    );
    m_chkCatAudio = makeCheckBox(
        VectorIcons::Type::Media,
        tr("Music & Audio (10-Band Equalizer, TagLib Editor)"),
        tr("Includes FFTW3 real-time spectrum analyzer, 10-band equalizer, and TagLib metadata editor.")
    );

    gridCategories->addWidget(m_chkCatDocs, 0, 0);
    gridCategories->addWidget(m_chkCatVideo, 0, 1);
    gridCategories->addWidget(m_chkCatArchives, 1, 0);
    gridCategories->addWidget(m_chkCatAudio, 1, 1);
    gridCategories->addWidget(m_chkCatImages, 2, 0);

    m_btnConfigureExtensions = new QPushButton(tr("⚙️ Individual Extensions..."), groupModules);
    m_btnConfigureExtensions->setCursor(Qt::PointingHandCursor);
    m_btnConfigureExtensions->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    gridCategories->addWidget(m_btnConfigureExtensions, 2, 1, Qt::AlignRight);

    connect(m_btnConfigureExtensions, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extensions);
        dlg.exec();
        updateDynamicSummary();
    });

    modulesLayout->addLayout(gridCategories);

    m_lblDynamicSummary = new QLabel(groupModules);
    m_lblDynamicSummary->setWordWrap(true);
    m_lblDynamicSummary->setTextFormat(Qt::RichText);
    m_lblDynamicSummary->setStyleSheet(
        "font-size: 11px; padding: 6px 8px; "
        "background-color: rgba(2, 132, 199, 0.07); "
        "border: 1px solid rgba(2, 132, 199, 0.22); border-radius: 6px;"
    );
    modulesLayout->addWidget(m_lblDynamicSummary);

    pageLayout->addWidget(groupModules);

    // =========================================================================
    // 2. Processor & Build Profile (Compact Side-by-Side)
    // =========================================================================
    auto *groupCpu = new QGroupBox(tr("2. Hardware Performance Profile"), page);
    auto *cpuLayout = new QVBoxLayout(groupCpu);
    cpuLayout->setContentsMargins(12, 6, 12, 6);
    cpuLayout->setSpacing(4);

    auto *cpuRadiosRow = new QHBoxLayout();
    cpuRadiosRow->setSpacing(18);

    m_radioCpuBalanced = new QRadioButton(
        tr("📦 System Packages (Instant, zero compile wait)"),
        groupCpu
    );
    m_radioCpuBalanced->setCursor(Qt::PointingHandCursor);
    m_radioCpuBalanced->setStyleSheet("font-weight: 600; font-size: 11px;");

    m_radioCpuDesktop = new QRadioButton(
        tr("🚀 Native CPU Tuning (-O3 -march=native)"),
        groupCpu
    );
    m_radioCpuDesktop->setCursor(Qt::PointingHandCursor);
    m_radioCpuDesktop->setStyleSheet("font-weight: 600; font-size: 11px;");

    cpuRadiosRow->addWidget(m_radioCpuBalanced);
    cpuRadiosRow->addWidget(m_radioCpuDesktop);
    cpuRadiosRow->addStretch(1);
    cpuLayout->addLayout(cpuRadiosRow);

    m_lblCpuDetectedNotice = new QLabel(groupCpu);
    m_lblCpuDetectedNotice->setStyleSheet("color: #0284c7; font-size: 11px;");
    m_lblCpuDetectedNotice->setWordWrap(true);
    m_lblCpuDetectedNotice->setTextFormat(Qt::RichText);
    cpuLayout->addWidget(m_lblCpuDetectedNotice);

    auto updateCpuNotice = [this]() {
        if (!m_lblCpuDetectedNotice) return;
        const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
        if (m_radioCpuDesktop && m_radioCpuDesktop->isChecked()) {
            if (alreadyO3) {
                m_lblCpuDetectedNotice->setText(
                    tr("<i>✅ Current binary is already built with native -O3 optimizations (zero extra compile wait).</i>")
                );
            } else {
                m_lblCpuDetectedNotice->setText(
                    tr("<i>ℹ️ BubezView will compile with aggressive -O3 native tuning for your CPU upon finish.</i>")
                );
            }
        } else {
            m_lblCpuDetectedNotice->setText(
                tr("<i>✅ Uses distribution precompiled packages immediately with zero compilation time.</i>")
            );
        }
    };

    connect(m_radioCpuDesktop, &QRadioButton::toggled, this, [this, updateCpuNotice](bool) {
        updateModuleOptionTexts();
        updateCpuNotice();
    });
    connect(m_radioCpuBalanced, &QRadioButton::toggled, this, [this, updateCpuNotice](bool) {
        updateModuleOptionTexts();
        updateCpuNotice();
    });

    const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
    if (alreadyO3 && CpuInfo::recommendedProfile() == QLatin1String("native")) {
        m_radioCpuDesktop->setChecked(true);
    } else {
        m_radioCpuBalanced->setChecked(true);
    }
    updateCpuNotice();

    pageLayout->addWidget(groupCpu);

    // =========================================================================
    // 3. Navigation & Window Behavior (2x2 Grid)
    // =========================================================================
    auto *groupKeys = new QGroupBox(tr("3. Navigation && Window Memory"), page);
    auto *keysGrid = new QGridLayout(groupKeys);
    keysGrid->setContentsMargins(12, 6, 12, 6);
    keysGrid->setHorizontalSpacing(18);
    keysGrid->setVerticalSpacing(4);

    m_chkCloseOnEsc = new QCheckBox(tr("Press Escape (Esc) to close the viewer"), groupKeys);
    m_chkCloseOnEsc->setChecked(true);
    m_chkCloseOnEsc->setStyleSheet("font-size: 11px;");

    m_chkRememberGeometry = new QCheckBox(tr("Remember window size and position"), groupKeys);
    m_chkRememberGeometry->setChecked(true);
    m_chkRememberGeometry->setStyleSheet("font-size: 11px;");

    m_chkPreloadCache = new QCheckBox(tr("Preload adjacent files for instant browsing"), groupKeys);
    m_chkPreloadCache->setChecked(true);
    m_chkPreloadCache->setStyleSheet("font-size: 11px;");

    m_chkStandbyMode = new QCheckBox(tr("Release preview cache when idle (save RAM)"), groupKeys);
    m_chkStandbyMode->setChecked(false);
    m_chkStandbyMode->setStyleSheet("font-size: 11px;");

    keysGrid->addWidget(m_chkCloseOnEsc, 0, 0);
    keysGrid->addWidget(m_chkRememberGeometry, 0, 1);
    keysGrid->addWidget(m_chkPreloadCache, 1, 0);
    keysGrid->addWidget(m_chkStandbyMode, 1, 1);

    pageLayout->addWidget(groupKeys);
    pageLayout->addStretch(1);

    // Wire preset radio buttons
    connect(m_radioPresetFull, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(0);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetMinimal, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(1);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetCustom, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            updateModuleOptionTexts();
            updateDynamicSummary();
        }
    });

    auto onCheckChanged = [this](bool) {
        evaluatePresetFromCheckboxes();
        updateModuleOptionTexts();
    };
    connect(m_chkCatDocs, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatArchives, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatImages, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatVideo, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatAudio, &QCheckBox::toggled, this, onCheckChanged);

    // Default to Minimal & On-Demand on first run
    QSettings settings("Bubez", "BubezView");
    const QString savedPreset = settings.value("modules/preset", QStringLiteral("minimal")).toString();
    if (savedPreset == QLatin1String("full")) {
        m_radioPresetFull->setChecked(true);
        applyPreset(0);
    } else if (savedPreset == QLatin1String("custom")) {
        m_syncingModules = true;
        m_chkCatDocs->setChecked(settings.value("modules/category_documents_all", false).toBool());
        m_chkCatArchives->setChecked(settings.value("modules/category_archives_all", false).toBool());
        m_chkCatImages->setChecked(settings.value("modules/category_images_all", false).toBool());
        m_chkCatVideo->setChecked(settings.value("modules/category_video_all", false).toBool());
        m_chkCatAudio->setChecked(settings.value("modules/category_audio_all", false).toBool());
        m_syncingModules = false;
        evaluatePresetFromCheckboxes();
    } else {
        m_radioPresetMinimal->setChecked(true);
        applyPreset(1);
    }
    updateModuleOptionTexts();

    return page;
}

void FirstRunWizard::applyPreset(int presetIndex)
{
    m_syncingModules = true;
    switch (presetIndex) {
        case 0: // Complete Suite
            if (m_chkCatDocs) m_chkCatDocs->setChecked(true);
            if (m_chkCatImages) m_chkCatImages->setChecked(true);
            if (m_chkCatVideo) m_chkCatVideo->setChecked(true);
            if (m_chkCatAudio) m_chkCatAudio->setChecked(true);
            if (m_chkCatArchives) m_chkCatArchives->setChecked(true);
            break;

        case 1: // Minimal & On-Demand
            if (m_chkCatDocs) m_chkCatDocs->setChecked(false);
            if (m_chkCatImages) m_chkCatImages->setChecked(false);
            if (m_chkCatVideo) m_chkCatVideo->setChecked(false);
            if (m_chkCatAudio) m_chkCatAudio->setChecked(false);
            if (m_chkCatArchives) m_chkCatArchives->setChecked(false);
            break;

        default:
            break;
    }
    m_syncingModules = false;
    updateDynamicSummary();
}

void FirstRunWizard::evaluatePresetFromCheckboxes()
{
    if (m_syncingModules) return;

    bool allChecked = (m_chkCatDocs && m_chkCatDocs->isChecked()) &&
                      (m_chkCatImages && m_chkCatImages->isChecked()) &&
                      (m_chkCatVideo && m_chkCatVideo->isChecked()) &&
                      (m_chkCatAudio && m_chkCatAudio->isChecked()) &&
                      (m_chkCatArchives && m_chkCatArchives->isChecked());

    bool allUnchecked = (!m_chkCatDocs || !m_chkCatDocs->isChecked()) &&
                        (!m_chkCatImages || !m_chkCatImages->isChecked()) &&
                        (!m_chkCatVideo || !m_chkCatVideo->isChecked()) &&
                        (!m_chkCatAudio || !m_chkCatAudio->isChecked()) &&
                        (!m_chkCatArchives || !m_chkCatArchives->isChecked());

    m_syncingModules = true;
    if (allChecked) {
        if (m_radioPresetFull) m_radioPresetFull->setChecked(true);
    } else if (allUnchecked) {
        if (m_radioPresetMinimal) m_radioPresetMinimal->setChecked(true);
    } else {
        if (m_radioPresetCustom) m_radioPresetCustom->setChecked(true);
    }
    m_syncingModules = false;
    updateDynamicSummary();
}

void FirstRunWizard::updateDynamicSummary()
{
    if (!m_lblDynamicSummary) return;

    if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        m_lblDynamicSummary->setText(
            tr("<b>⚡ Minimal &amp; On-Demand (Active):</b> Core viewers (<b>PDF, Images, Text, Archives, Audio/Video</b>) "
               "are ready immediately. Extra studio tools (<i>PDF Repair, OCR, LibreOffice, Digital Signatures, 7-Zip, MPV GPU</i>) "
               "remain lightweight and ask for 1-click installation only when you use them.")
        );
    } else if (m_radioPresetFull && m_radioPresetFull->isChecked()) {
        m_lblDynamicSummary->setText(
            tr("<b>🚀 Complete Suite:</b> All optional studio extensions (<i>LibreOffice, Tesseract OCR, QPDF Repair, "
               "Digital Signatures, MPV GPU, 10-Band Equalizer, 7-Zip</i>) will be verified and installed now.")
        );
    } else {
        QStringList selectedCats;
        if (m_chkCatDocs && m_chkCatDocs->isChecked()) selectedCats << tr("Documents & Office");
        if (m_chkCatArchives && m_chkCatArchives->isChecked()) selectedCats << tr("Archives & ISO");
        if (m_chkCatVideo && m_chkCatVideo->isChecked()) selectedCats << tr("Video (MPV GPU)");
        if (m_chkCatAudio && m_chkCatAudio->isChecked()) selectedCats << tr("Audio (Equalizer & TagLib)");
        if (m_chkCatImages && m_chkCatImages->isChecked()) selectedCats << tr("Extended Image Codecs");

        m_lblDynamicSummary->setText(
            tr("<b>⚙️ Custom Selection:</b> Pre-installing extensions for: <b>%1</b>. "
               "All other tools remain available on demand.")
                .arg(selectedCats.join(", "))
        );
    }
}

void FirstRunWizard::updateModuleOptionTexts()
{
    if (!m_radioPresetFull || !m_radioPresetMinimal || !m_radioPresetCustom) return;

    m_radioPresetMinimal->setText(tr("⚡ Minimal && On-Demand (Recommended)"));
    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());
    if (isHighPerformance) {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (Host Compiled)"));
    } else {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (System Packages)"));
    }
    m_radioPresetCustom->setText(tr("⚙️ Custom Selection"));
}

void FirstRunWizard::accept()
{
    QSettings settings("Bubez", "BubezView");

    // 1. Mark first run as completed
    settings.setValue("first_run_completed", true);

    // 2. Hardware Performance Profile
    BuildConfig &cfg = BuildConfig::instance();
    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());
    if (isHighPerformance) {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::HighPerformance);
    } else {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::Balanced);
    }

    // 3. Execution of Compilation / Package Installation based on chosen categories
    bool isFull = m_radioPresetFull && m_radioPresetFull->isChecked();
    bool isMinimal = !m_radioPresetFull || (m_radioPresetMinimal && m_radioPresetMinimal->isChecked());

    bool enableDocsExt = isFull || (!isMinimal && m_chkCatDocs && m_chkCatDocs->isChecked());
    bool enableImagesExt = isFull || (!isMinimal && m_chkCatImages && m_chkCatImages->isChecked());
    bool enableVideoExt = isFull || (!isMinimal && m_chkCatVideo && m_chkCatVideo->isChecked());
    bool enableAudioExt = isFull || (!isMinimal && m_chkCatAudio && m_chkCatAudio->isChecked());
    bool enableArchivesExt = isFull || (!isMinimal && m_chkCatArchives && m_chkCatArchives->isChecked());

    // Core viewer engines are always enabled
    bool enablePdf = true;
    bool enableImage = true;
    bool enableVideo = true;
    bool enableAudio = true;
    bool enableArchive = true;

    // Granular extension flags mapped from category choices
    bool enableOffice = enableDocsExt;
    bool enableOcr = enableDocsExt;
    bool enableQpdf = enableDocsExt;
    bool enableOpenSsl = enableDocsExt;
    bool enableImageFormats = enableImagesExt;
    bool enableMpv = enableVideoExt;
    bool enableEqualizer = enableAudioExt;
    bool enableTagLib = enableAudioExt;
    bool enableSevenZip = enableArchivesExt;

    settings.setValue("modules/pdf_enabled", enablePdf);
    settings.setValue("modules/ocr_enabled", enableOcr);
    settings.setValue("modules/qpdf_enabled", enableQpdf);
    settings.setValue("modules/openssl_enabled", enableOpenSsl);
    settings.setValue("modules/office_enabled", enableOffice);
    settings.setValue("modules/image_enabled", enableImage);
    settings.setValue("modules/image_formats_enabled", enableImageFormats);
    settings.setValue("modules/video_enabled", enableVideo);
    settings.setValue("modules/mpv_enabled", enableMpv);
    settings.setValue("modules/audio_enabled", enableAudio);
    settings.setValue("modules/equalizer_enabled", enableEqualizer);
    settings.setValue("modules/taglib_enabled", enableTagLib);
    settings.setValue("modules/archive_enabled", enableArchive);
    settings.setValue("modules/sevenzip_enabled", enableSevenZip);
    settings.setValue("modules/media_enabled", true);

    settings.setValue("modules/category_documents_all", enableDocsExt);
    settings.setValue("modules/category_images_all", enableImagesExt);
    settings.setValue("modules/category_video_all", enableVideoExt);
    settings.setValue("modules/category_audio_all", enableAudioExt);
    settings.setValue("modules/category_archives_all", enableArchivesExt);
    settings.setValue("modules/preset", isMinimal ? "minimal" : (isFull ? "full" : "custom"));

    cfg.setEnableInternalOffice(enableOffice);
    settings.setValue("office_use_libreoffice", enableOffice);

    if (isHighPerformance) {
        const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
        if (!alreadyO3 && (enablePdf || enableArchive || enableMpv)) {
            BuildConsoleDialog console(this);
            console.setWindowTitle(tr("Compiling BubezView — High Performance Profile"));
            console.startBuild();
            console.exec();
        }

        QStringList missingExts;
        if (enableOcr && !PackageManagerHelper::isComponentInstalled("tesseract")) {
            missingExts << PackageManagerHelper::packageNameForComponent("tesseract");
        }
        if (enableQpdf && !PackageManagerHelper::isComponentInstalled("qpdf")) {
            missingExts << PackageManagerHelper::packageNameForComponent("qpdf");
        }
        if (enableSevenZip && !PackageManagerHelper::isComponentInstalled("7zip")) {
            missingExts << PackageManagerHelper::packageNameForComponent("7zip");
        }
        if (enableMpv && !PackageManagerHelper::isComponentInstalled("mpv")) {
            missingExts << PackageManagerHelper::packageNameForComponent("mpv");
        }
        if (!missingExts.isEmpty()) {
            PackageManagerHelper::runPrivilegedInstall(missingExts, this);
        }
    } else {
        QStringList componentsToCheck;
        if (enablePdf) componentsToCheck << "engine-poppler";
        if (enableArchive) componentsToCheck << "engine-libarchive";
        if (enableOcr) componentsToCheck << "tesseract";
        if (enableQpdf) componentsToCheck << "qpdf";
        if (enableSevenZip) componentsToCheck << "7zip";
        if (enableMpv) componentsToCheck << "mpv";

        QStringList missingPackages;
        for (const QString &cid : componentsToCheck) {
            if (!PackageManagerHelper::isComponentInstalled(cid)) {
                QString pkg = PackageManagerHelper::packageNameForComponent(cid);
                if (!pkg.isEmpty() && !missingPackages.contains(pkg)) {
                    missingPackages << pkg;
                }
            }
        }
        if (!missingPackages.isEmpty()) {
            PackageManagerHelper::runPrivilegedInstall(missingPackages, this);
        }
    }

    cfg.save();

    // 4. Primary Shortcut and System Integrations
    DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
    QString chosenShortcut = (m_useTriggerFallback || de != DesktopIntegrator::DesktopEnv::KDE) ? "Ctrl+Space" : "Space";
    settings.setValue("shortcut", chosenShortcut);

    if (de == DesktopIntegrator::DesktopEnv::KDE) {
        DesktopIntegrator::enableDolphin(chosenShortcut);
    } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
        DesktopIntegrator::enableGnomeNemo();
    }

    // 5. Appearance
    if (m_comboTheme) {
        QString themeCode = m_comboTheme->currentData().toString();
        ThemeManager::setPreset(ThemeManager::stringToPreset(themeCode));
    }
    if (m_chkFilmstrip) {
        settings.setValue("ui/filmstrip", m_chkFilmstrip->isChecked());
        settings.setValue("show_filmstrip", m_chkFilmstrip->isChecked());
    }

    // 6. Navigation, Behavior & Cache
    if (m_chkCloseOnEsc) {
        settings.setValue("quick_close_on_esc", m_chkCloseOnEsc->isChecked());
    }
    settings.setValue("quick_close_on_space", false);

    if (m_chkRememberGeometry) {
        settings.setValue("remember_geometry", m_chkRememberGeometry->isChecked());
    }
    if (m_chkPreloadCache) {
        settings.setValue("cache/preload_adjacent", m_chkPreloadCache->isChecked());
    }
    if (m_chkStandbyMode) {
        settings.setValue("cache/enable_standby", m_chkStandbyMode->isChecked());
    }

    settings.sync();
    QWizard::accept();
}
