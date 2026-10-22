/**
 * @file first_run_wizard.cpp
 * @brief Implementation of the FirstRunWizard dialog.
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
#include <QDialog>
#include <QKeyEvent>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QGridLayout>
#include <QSize>

#ifndef BUBEZ_OPT_LEVEL
#define BUBEZ_OPT_LEVEL "O2"
#endif

namespace {

/**
 * @class SpaceShortcutTestDialog
 * @brief Interactive modal dialog that captures key presses to verify spacebar responsiveness.
 */
class SpaceShortcutTestDialog : public QDialog {
public:
    explicit SpaceShortcutTestDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_spaceDetected(false)
    {
        setWindowTitle(FirstRunWizard::tr("Test Spacebar Shortcut"));
        resize(420, 170);
        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(12);

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

    // Spacious and responsive size for standard screens and high-DPI displays
    setMinimumSize(660, 480);
    int targetW = 750;
    int targetH = 530;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        targetW = qMin(targetW, qMax(660, avail.width() - 40));
        targetH = qMin(targetH, qMax(480, avail.height() - 60));
    }
    resize(targetW, targetH);

    // Register pages
    setPage(Page_Welcome, createWelcomePage());
    setPage(Page_CpuOptimization, createCpuOptimizationPage());
    setPage(Page_Modules, createModulesPage());
    setPage(Page_Appearance, createAppearancePage());
    setPage(Page_Behavior, createBehaviorPage());
    setPage(Page_Summary, createSummaryPage());

    setStartId(Page_Welcome);

    // Custom button labels (double ampersand to prevent mnemonic eating '&')
    setButtonText(QWizard::NextButton, tr("Next >"));
    setButtonText(QWizard::BackButton, tr("< Back"));
    setButtonText(QWizard::FinishButton, tr("Finish && Launch BubezView"));
    setButtonText(QWizard::CancelButton, tr("Cancel"));

    auto alignHeaderSubtitle = [this]() {
        // Remove QWizard::ModernStyle's default 18px left indent on the header subtitle
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
        if (id == Page_Summary) {
            updateSummaryText();
        }
    });
    QTimer::singleShot(0, this, alignHeaderSubtitle);
}

QWizardPage *FirstRunWizard::createWelcomePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Welcome to BubezView"));
    page->setSubTitle(tr("High-performance native file viewer and instant preview tool for Linux."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(6, 4, 6, 4);

    // 1. Compact Horizontal Intro Banner (Icon + Description side by side)
    auto *groupIntro = new QGroupBox(page);
    auto *introLayout = new QHBoxLayout(groupIntro);
    introLayout->setContentsMargins(12, 10, 12, 10);
    introLayout->setSpacing(14);

    auto *lblIcon = new QLabel(groupIntro);
    lblIcon->setAlignment(Qt::AlignCenter);
    lblIcon->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Document, QColor("#3b82f6")).pixmap(48, 48));
    lblIcon->setFixedSize(52, 52);
    introLayout->addWidget(lblIcon);

    auto *lblDesc = new QLabel(
        tr("This wizard will help you configure your preferred processor optimizations, "
           "document modules, visual appearance, and navigation behavior.<br><br>"
           "You can complete this setup in under a minute, and change any preference later "
           "from the Settings dialog or directly from application menus."),
        groupIntro
    );
    lblDesc->setWordWrap(true);
    lblDesc->setTextFormat(Qt::RichText);
    lblDesc->setStyleSheet("font-size: 12px;");
    introLayout->addWidget(lblDesc, 1);

    layout->addWidget(groupIntro);

    // 2. Detected Desktop & Hardware Environment Card
    auto *groupEnv = new QGroupBox(tr("Detected Environment"), page);
    auto *envLayout = new QVBoxLayout(groupEnv);
    envLayout->setContentsMargins(12, 8, 12, 8);
    envLayout->setSpacing(4);

    QString deName = DesktopIntegrator::desktopName();
    QString cpuModel = CpuInfo::modelName();
    if (cpuModel.isEmpty()) {
        cpuModel = tr("Generic x86-64 Processor");
    }

    auto *lblDe = new QLabel(
        tr("<table width='100%' cellspacing='2' cellpadding='1'>"
           "<tr><td width='34%'><b>Desktop Environment:</b></td><td>%1</td></tr>"
           "<tr><td><b>Detected Processor:</b></td><td>%2 (%3 threads)</td></tr>"
           "<tr><td><b>Core Architecture:</b></td><td>Native C++17 / Qt6 (Hardware-Accelerated)</td></tr>"
           "</table>")
        .arg(deName, cpuModel, QString::number(CpuInfo::logicalCores())),
        groupEnv
    );
    lblDe->setTextFormat(Qt::RichText);
    lblDe->setStyleSheet("font-size: 12px;");
    envLayout->addWidget(lblDe);

    layout->addWidget(groupEnv);

    // 3. Setup Steps Overview Grid (fills remaining space cleanly without dead void)
    auto *groupSteps = new QGroupBox(tr("Quick Setup Overview"), page);
    auto *stepsGrid = new QGridLayout(groupSteps);
    stepsGrid->setContentsMargins(12, 8, 12, 8);
    stepsGrid->setHorizontalSpacing(16);
    stepsGrid->setVerticalSpacing(6);

    auto makeStepItem = [groupSteps](const QString &text) {
        auto *lbl = new QLabel(text, groupSteps);
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setStyleSheet("font-size: 11px;");
        return lbl;
    };

    stepsGrid->addWidget(makeStepItem(tr("<b>1. CPU Profile:</b> Native host tuning or system packages")), 0, 0);
    stepsGrid->addWidget(makeStepItem(tr("<b>2. Modules:</b> PDF, Office, Media, Archives &amp; OCR")), 0, 1);
    stepsGrid->addWidget(makeStepItem(tr("<b>3. Appearance:</b> System theme &amp; thumbnail bar")), 1, 0);
    stepsGrid->addWidget(makeStepItem(tr("<b>4. Shortcuts:</b> Instant Spacebar preview in file manager")), 1, 1);

    layout->addWidget(groupSteps);
    layout->addStretch(1);

    return page;
}

QWizardPage *FirstRunWizard::createCpuOptimizationPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Processor & Hardware Optimization"));
    page->setSubTitle(tr("Select the performance profile tailored for your computer."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(6, 4, 6, 4);

    // Host CPU Card
    auto *groupCpu = new QGroupBox(tr("Detected Host Processor"), page);
    auto *cpuLayout = new QVBoxLayout(groupCpu);
    cpuLayout->setContentsMargins(12, 8, 12, 8);
    QString cpuModel = CpuInfo::modelName();
    if (cpuModel.isEmpty()) {
        cpuModel = tr("Generic x86-64 Processor");
    }
    QString tier = CpuInfo::microarchitectureTier();
    auto *lblCpu = new QLabel(
        tr("<b>Model:</b> %1<br><b>Architecture tier:</b> x86-64-%2 (%3 logical cores)")
            .arg(cpuModel, tier, QString::number(CpuInfo::logicalCores())),
        groupCpu
    );
    lblCpu->setTextFormat(Qt::RichText);
    lblCpu->setStyleSheet("font-size: 12px;");
    cpuLayout->addWidget(lblCpu);
    layout->addWidget(groupCpu);

    // Profile Selection (Separate short RadioButton title from wrapped QLabel description to prevent truncation)
    auto *groupProfiles = new QGroupBox(tr("Performance Profile"), page);
    auto *profilesLayout = new QVBoxLayout(groupProfiles);
    profilesLayout->setContentsMargins(12, 8, 12, 8);
    profilesLayout->setSpacing(6);

    m_radioCpuDesktop = new QRadioButton(
        tr("🚀 High Performance (Everything Compiled from Source)"),
        groupProfiles
    );
    m_radioCpuDesktop->setStyleSheet("font-weight: 600; font-size: 13px;");
    profilesLayout->addWidget(m_radioCpuDesktop);

    auto *lblDesktopDesc = new QLabel(
        tr("Maximum speed. All engines compiled with aggressive host optimizations (-O3, -march=native, LTO)."),
        groupProfiles
    );
    lblDesktopDesc->setWordWrap(true);
    lblDesktopDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 22px; margin-bottom: 4px;");
    profilesLayout->addWidget(lblDesktopDesc);

    m_radioCpuBalanced = new QRadioButton(
        tr("📦 Balanced (System Packaged Dependencies)"),
        groupProfiles
    );
    m_radioCpuBalanced->setStyleSheet("font-weight: 600; font-size: 13px;");
    profilesLayout->addWidget(m_radioCpuBalanced);

    auto *lblBalancedDesc = new QLabel(
        tr("Daily use. Uses precompiled system libraries from your distribution with zero compile wait times."),
        groupProfiles
    );
    lblBalancedDesc->setWordWrap(true);
    lblBalancedDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 22px; margin-bottom: 4px;");
    profilesLayout->addWidget(lblBalancedDesc);

    m_lblCpuDetectedNotice = new QLabel(groupProfiles);
    m_lblCpuDetectedNotice->setStyleSheet("color: #0284c7; font-size: 11px; margin-top: 2px;");
    m_lblCpuDetectedNotice->setWordWrap(true);
    m_lblCpuDetectedNotice->setTextFormat(Qt::RichText);
    profilesLayout->addWidget(m_lblCpuDetectedNotice);

    auto updateCpuNotice = [this]() {
        if (!m_lblCpuDetectedNotice) return;
        const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
        if (m_radioCpuDesktop && m_radioCpuDesktop->isChecked()) {
            if (alreadyO3) {
                m_lblCpuDetectedNotice->setText(
                    tr("<i>✅ Current binary is already built with native -O3 optimizations (no extra compile wait).</i>")
                );
            } else {
                m_lblCpuDetectedNotice->setText(
                    tr("<i>ℹ️ Optimal profile for your CPU. BubezView will compile with -O3 native tuning upon finish.</i>")
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

    // Intelligent auto-detection of optimal profile
    QString rec = CpuInfo::recommendedProfile();
    if (rec == QLatin1String("native")) {
        m_radioCpuDesktop->setChecked(true);
    } else {
        m_radioCpuBalanced->setChecked(true);
    }
    updateCpuNotice();

    layout->addWidget(groupProfiles);
    layout->addStretch(1);

    return page;
}

QWizardPage *FirstRunWizard::createModulesPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Engines & Functional Modules"));
    page->setSubTitle(tr("Choose which document readers, media players, and preview suites to activate."));

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(10, 6, 10, 6);
    pageLayout->setSpacing(10);

    // 1. Single Unified Box: Presets & Module Selection
    auto *groupModules = new QGroupBox(tr("Module Setup && Presets"), page);
    auto *modulesLayout = new QVBoxLayout(groupModules);
    modulesLayout->setContentsMargins(14, 8, 14, 8);
    modulesLayout->setSpacing(6);

    // Top: Presets (Complete Suite, Minimal Core, Custom)
    auto *presetsGrid = new QHBoxLayout();
    presetsGrid->setSpacing(20);

    m_radioPresetFull = new QRadioButton(groupModules);
    m_radioPresetFull->setCursor(Qt::PointingHandCursor);
    m_radioPresetFull->setToolTip(tr("Enable all viewer engines and all present and future extension packages."));

    m_radioPresetMinimal = new QRadioButton(groupModules);
    m_radioPresetMinimal->setCursor(Qt::PointingHandCursor);
    m_radioPresetMinimal->setToolTip(tr("Enable only the lightweight core engines with zero heavy background packages."));

    m_radioPresetCustom = new QRadioButton(tr("Custom Selection"), groupModules);
    m_radioPresetCustom->setCursor(Qt::PointingHandCursor);
    m_radioPresetCustom->setToolTip(tr("Choose specific categories below to receive full extension suites."));

    presetsGrid->addWidget(m_radioPresetFull);
    presetsGrid->addWidget(m_radioPresetMinimal);
    presetsGrid->addWidget(m_radioPresetCustom);
    presetsGrid->addStretch(1);

    modulesLayout->addLayout(presetsGrid);

    // Subtle horizontal divider line
    auto *sep = new QFrame(groupModules);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setStyleSheet("color: rgba(148, 163, 184, 0.25); margin: 2px 0;");
    modulesLayout->addWidget(sep);

    // Category Checkboxes arranged in a clean, spacious 2-column grid with native VectorIcons
    auto *gridCategories = new QGridLayout();
    gridCategories->setHorizontalSpacing(24);
    gridCategories->setVerticalSpacing(6);
    gridCategories->setContentsMargins(4, 2, 4, 2);

    auto makeCheckBox = [groupModules](VectorIcons::Type iconType, const QString &text, const QString &tooltip) {
        QString safeText = text;
        safeText.replace(QRegularExpression("(?<!&)&(?!&)"), "&&");
        auto *chk = new QCheckBox(safeText, groupModules);
        chk->setIcon(VectorIcons::createIcon(iconType));
        chk->setIconSize(QSize(18, 18));
        chk->setToolTip(tooltip);
        chk->setStyleSheet("font-weight: 600; font-size: 11px;");
        chk->setCursor(Qt::PointingHandCursor);
        return chk;
    };

    m_chkCatDocs = makeCheckBox(
        VectorIcons::Type::Pdf,
        tr("Documents && Office (LibreOffice, OCR, QPDF, Signatures)"),
        tr("Includes LibreOffice document conversion, Tesseract OCR text extraction, QPDF structural repair, and PAdES digital signatures.")
    );
    m_chkCatArchives = makeCheckBox(
        VectorIcons::Type::Zip,
        tr("Archives && Disk Images (7-Zip, Multi-Core, ISO)"),
        tr("Includes 7-Zip multi-core compression, encrypted archive creation, and bootable ISO/IMG disk image tools.")
    );
    m_chkCatImages = makeCheckBox(
        VectorIcons::Type::Image,
        tr("Images && Graphics (Extended Codecs: WebP, TIFF, etc.)"),
        tr("Includes extended Qt6 image decoders for WebP, TIFF, TGA, Apple ICNS, PSD, and JPEG-XL formats.")
    );
    m_chkCatVideo = makeCheckBox(
        VectorIcons::Type::FolderVideos,
        tr("Video && Movies (MPV GPU Acceleration)"),
        tr("Includes high-performance libmpv video player with hardware GPU decoding and subtitle track controls. When unchecked, videos play via native OS codecs.")
    );
    m_chkCatAudio = makeCheckBox(
        VectorIcons::Type::Media,
        tr("Music && Audio (10-Band Equalizer, TagLib Editor)"),
        tr("Includes FFTW3 real-time spectrum analyzer, 10-band parametric equalizer, and TagLib ID3/FLAC metadata editor.")
    );

    gridCategories->addWidget(m_chkCatDocs, 0, 0);
    gridCategories->addWidget(m_chkCatVideo, 0, 1);

    gridCategories->addWidget(m_chkCatArchives, 1, 0);
    gridCategories->addWidget(m_chkCatAudio, 1, 1);

    gridCategories->addWidget(m_chkCatImages, 2, 0);

    modulesLayout->addLayout(gridCategories);
    pageLayout->addWidget(groupModules);

    // 2. Large Spacious Dynamic Live Configuration Summary Card
    auto *groupSummaryCard = new QGroupBox(tr("Live Configuration Summary"), page);
    auto *summaryCardLayout = new QVBoxLayout(groupSummaryCard);
    summaryCardLayout->setContentsMargins(14, 8, 14, 8);
    summaryCardLayout->setSpacing(6);

    m_lblDynamicSummary = new QLabel(groupSummaryCard);
    m_lblDynamicSummary->setWordWrap(true);
    m_lblDynamicSummary->setTextFormat(Qt::RichText);
    m_lblDynamicSummary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_lblDynamicSummary->setStyleSheet("font-size: 11px;");
    summaryCardLayout->addWidget(m_lblDynamicSummary, 1);

    auto *lblTip = new QLabel(
        tr("💡 <b>Tip:</b> You can also enable, disable, or install individual extensions at any time via menu "
           "<b>View → Show Uninstalled Features</b>, or in <b>Settings → Extensions</b>."),
        groupSummaryCard
    );
    lblTip->setWordWrap(true);
    lblTip->setTextFormat(Qt::RichText);
    lblTip->setStyleSheet(
        "color: #0284c7; font-size: 11px; padding: 5px 8px; "
        "background-color: rgba(2, 132, 199, 0.08); border: 1px solid rgba(2, 132, 199, 0.25); "
        "border-radius: 5px;"
    );
    summaryCardLayout->addWidget(lblTip);

    pageLayout->addWidget(groupSummaryCard, 1);

    // 3. Bottom Row with Advanced Settings
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(2, 0, 2, 0);
    bottomRow->addStretch(1);

    m_btnConfigureExtensions = new QPushButton(tr("⚙️ Advanced Settings (Individual Extensions)..."), page);
    m_btnConfigureExtensions->setCursor(Qt::PointingHandCursor);
    m_btnConfigureExtensions->setToolTip(tr("Open granular extension settings to inspect or configure individual packages."));
    bottomRow->addWidget(m_btnConfigureExtensions);
    pageLayout->addLayout(bottomRow);

    connect(m_btnConfigureExtensions, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extensions);
        dlg.exec();
        updateDynamicSummary();
    });

    // Wire preset radio buttons to applyPreset
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

    // Wire category checkboxes to evaluatePresetFromCheckboxes
    auto onCheckChanged = [this](bool) {
        evaluatePresetFromCheckboxes();
        updateModuleOptionTexts();
    };
    connect(m_chkCatDocs, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatArchives, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatImages, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatVideo, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkCatAudio, &QCheckBox::toggled, this, onCheckChanged);

    // Restore previously saved state if wizard is re-run, or default to Complete Suite on first run
    QSettings settings("Bubez", "BubezView");
    const QString savedPreset = settings.value("modules/preset", QStringLiteral("full")).toString();
    if (savedPreset == QLatin1String("minimal")) {
        m_radioPresetMinimal->setChecked(true);
        applyPreset(1);
    } else if (savedPreset == QLatin1String("custom")) {
        m_syncingModules = true;
        m_chkCatDocs->setChecked(settings.value("modules/category_documents_all", true).toBool());
        m_chkCatArchives->setChecked(settings.value("modules/category_archives_all", true).toBool());
        m_chkCatImages->setChecked(settings.value("modules/category_images_all", true).toBool());
        m_chkCatVideo->setChecked(settings.value("modules/category_video_all", true).toBool());
        m_chkCatAudio->setChecked(settings.value("modules/category_audio_all", true).toBool());
        m_syncingModules = false;
        evaluatePresetFromCheckboxes();
    } else {
        m_radioPresetFull->setChecked(true);
        applyPreset(0);
    }
    updateModuleOptionTexts();

    return page;
}

QWizardPage *FirstRunWizard::createAppearancePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Appearance & Theme"));
    page->setSubTitle(tr("Customize the visual style and UI components of BubezView."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(6, 4, 6, 4);

    auto *groupTheme = new QGroupBox(tr("Application Theme"), page);
    auto *themeLayout = new QVBoxLayout(groupTheme);
    themeLayout->setContentsMargins(12, 8, 12, 8);
    themeLayout->setSpacing(6);

    m_comboTheme = new QComboBox(groupTheme);
    m_comboTheme->addItem(tr("Native System Theme (Matches desktop light/dark mode)"), "native");
    m_comboTheme->addItem(tr("Dark Slate (Deep modern slate theme)"), "dark_slate");
    m_comboTheme->addItem(tr("Breeze Dark (KDE Breeze-inspired dark styling)"), "breeze_dark");
    themeLayout->addWidget(m_comboTheme);

    auto *lblThemeInfo = new QLabel(groupTheme);
    lblThemeInfo->setWordWrap(true);
    lblThemeInfo->setTextFormat(Qt::RichText);
    lblThemeInfo->setStyleSheet("color: #64748b; font-size: 11px; margin-top: 2px;");

    auto updateThemeDescription = [this, lblThemeInfo]() {
        QString code = m_comboTheme ? m_comboTheme->currentData().toString() : "native";
        if (code == "dark_slate") {
            lblThemeInfo->setText(
                tr("<b>Dark Slate:</b> High-contrast dark slate palette (<code>#0f172a</code>) ideal for low-light environments, media viewing, and code inspection.")
            );
        } else if (code == "breeze_dark") {
            lblThemeInfo->setText(
                tr("<b>Breeze Dark:</b> Charcoal KDE Breeze dark palette (<code>#232629</code>) with crisp cyan accents and soft borders.")
            );
        } else {
            lblThemeInfo->setText(
                tr("<b>Native System Theme:</b> Automatically inherits your desktop environment's active Qt color scheme, fonts, and window styling.")
            );
        }
    };
    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [updateThemeDescription](int) {
        updateThemeDescription();
    });
    updateThemeDescription();
    themeLayout->addWidget(lblThemeInfo);

    layout->addWidget(groupTheme);

    auto *groupComponents = new QGroupBox(tr("Viewer Components"), page);
    auto *compLayout = new QVBoxLayout(groupComponents);
    compLayout->setContentsMargins(12, 8, 12, 8);
    compLayout->setSpacing(4);

    m_chkFilmstrip = new QCheckBox(
        tr("Show bottom thumbnail filmstrip when viewing folders or multi-page documents"),
        groupComponents
    );
    m_chkFilmstrip->setStyleSheet("font-weight: 600; font-size: 12px;");
    m_chkFilmstrip->setChecked(false);
    compLayout->addWidget(m_chkFilmstrip);

    auto *lblFilmstripDesc = new QLabel(
        tr("Displays a quick-jump thumbnail strip along the bottom edge. You can also toggle it on demand while viewing any file."),
        groupComponents
    );
    lblFilmstripDesc->setWordWrap(true);
    lblFilmstripDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-left: 22px;");
    compLayout->addWidget(lblFilmstripDesc);

    layout->addWidget(groupComponents);
    layout->addStretch(1);
    return page;
}

QWizardPage *FirstRunWizard::createBehaviorPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Navigation, Shortcuts & Behavior"));
    page->setSubTitle(tr("Configure shortcuts, dismissal keys, and navigation caching for daily use."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(8);
    layout->setContentsMargins(6, 4, 6, 4);

    // 1. Primary Shortcut & Test
    auto *groupShortcut = new QGroupBox(tr("Primary Preview Shortcut"), page);
    auto *scLayout = new QVBoxLayout(groupShortcut);
    scLayout->setContentsMargins(12, 6, 12, 6);
    scLayout->setSpacing(4);

    auto *lblShortcutInfo = new QLabel(
        tr("<b>Primary Shortcut:</b> Press <b>Space</b> on any file in your file manager for instant preview."),
        groupShortcut
    );
    lblShortcutInfo->setTextFormat(Qt::RichText);
    lblShortcutInfo->setWordWrap(true);
    lblShortcutInfo->setStyleSheet("font-size: 12px;");
    scLayout->addWidget(lblShortcutInfo);

    auto *hBoxTest = new QHBoxLayout();
    hBoxTest->setSpacing(10);
    m_btnTestShortcut = new QPushButton(tr("🧪 Test Shortcut..."), groupShortcut);
    m_btnTestShortcut->setCursor(Qt::PointingHandCursor);
    hBoxTest->addWidget(m_btnTestShortcut);

    m_lblTestResult = new QLabel(groupShortcut);
    m_lblTestResult->setTextFormat(Qt::RichText);
    m_lblTestResult->setWordWrap(true);
    hBoxTest->addWidget(m_lblTestResult, 1);
    scLayout->addLayout(hBoxTest);

    m_chkUseCtrlSpace = new QCheckBox(
        tr("Use Ctrl + Space fallback (connects to trigger_bubez.sh if Space is reserved by Dolphin)"),
        groupShortcut
    );
    m_chkUseCtrlSpace->setVisible(false);
    scLayout->addWidget(m_chkUseCtrlSpace);

    QString projectTrigger = QDir(QCoreApplication::applicationDirPath()).filePath("../trigger_bubez.sh");
    QString triggerPath = QDir::homePath() + "/.local/share/bubezview/trigger_bubez.sh";
    if (QFile::exists(projectTrigger)) {
        triggerPath = QFileInfo(projectTrigger).canonicalFilePath();
    }

    m_lblTriggerHelp = new QLabel(groupShortcut);
    m_lblTriggerHelp->setTextFormat(Qt::RichText);
    m_lblTriggerHelp->setWordWrap(true);
    m_lblTriggerHelp->setText(
        tr("<b>Important note for trigger fallback:</b> In your desktop system settings (KDE Plasma / GNOME Shortcuts), "
           "you will need to bind <b>Ctrl + Space</b> to the trigger script located at:<br>"
           "<code style='color:#0284c7;'>%1</code>")
        .arg(triggerPath)
    );
    m_lblTriggerHelp->setStyleSheet("background-color: rgba(56, 189, 248, 0.08); border: 1px solid rgba(56, 189, 248, 0.25); border-radius: 6px; padding: 6px; font-size: 11px;");
    m_lblTriggerHelp->setVisible(false);
    scLayout->addWidget(m_lblTriggerHelp);

    auto updateTriggerVisibility = [this](bool visible) {
        if (m_chkUseCtrlSpace) m_chkUseCtrlSpace->setVisible(visible);
        if (m_lblTriggerHelp) m_lblTriggerHelp->setVisible(visible && m_chkUseCtrlSpace->isChecked());
    };

    connect(m_chkUseCtrlSpace, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_lblTriggerHelp) m_lblTriggerHelp->setVisible(checked);
    });

    connect(m_btnTestShortcut, &QPushButton::clicked, this, [this, updateTriggerVisibility]() {
        SpaceShortcutTestDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && dlg.spaceDetected()) {
            m_lblTestResult->setText(tr("<b style='color:#10b981;'>✅ Spacebar verified and functioning properly!</b>"));
            m_chkUseCtrlSpace->setChecked(false);
            updateTriggerVisibility(false);
        } else {
            m_lblTestResult->setText(tr("<b style='color:#f59e0b;'>⚠️ Space key not detected or conflict reported. Fallback available:</b>"));
            m_chkUseCtrlSpace->setChecked(true);
            updateTriggerVisibility(true);
        }
    });

    layout->addWidget(groupShortcut);

    // 2. Quick Dismissal & Navigation (double ampersand so '&' is displayed)
    auto *groupKeys = new QGroupBox(tr("Navigation && Window Memory"), page);
    auto *keysLayout = new QVBoxLayout(groupKeys);
    keysLayout->setContentsMargins(12, 6, 12, 6);
    keysLayout->setSpacing(4);

    m_chkCloseOnEsc = new QCheckBox(
        tr("Instant dismissal: Press Escape (Esc) to close the viewer"),
        groupKeys
    );
    m_chkCloseOnEsc->setChecked(true);
    keysLayout->addWidget(m_chkCloseOnEsc);

    m_chkRememberGeometry = new QCheckBox(
        tr("Remember window size and position across preview sessions"),
        groupKeys
    );
    m_chkRememberGeometry->setChecked(true);
    keysLayout->addWidget(m_chkRememberGeometry);

    m_chkPreloadCache = new QCheckBox(
        tr("Preload adjacent files in background for instant browsing"),
        groupKeys
    );
    m_chkPreloadCache->setChecked(true);
    keysLayout->addWidget(m_chkPreloadCache);

    m_chkStandbyMode = new QCheckBox(
        tr("Memory standby: Release preview memory cache when idle to save RAM"),
        groupKeys
    );
    m_chkStandbyMode->setChecked(false);
    keysLayout->addWidget(m_chkStandbyMode);

    layout->addWidget(groupKeys);

    // 3. Shortcuts & Launchers
    auto *groupLaunchers = new QGroupBox(tr("Desktop Integration"), page);
    auto *launchersLayout = new QVBoxLayout(groupLaunchers);
    launchersLayout->setContentsMargins(12, 6, 12, 6);

    m_chkDesktopShortcut = new QCheckBox(
        tr("Place a quick launcher on the Desktop (~/Desktop or ~/Scrivania)"),
        groupLaunchers
    );
    m_chkDesktopShortcut->setChecked(false);
    launchersLayout->addWidget(m_chkDesktopShortcut);

    layout->addWidget(groupLaunchers);
    layout->addStretch(1);

    return page;
}

QWizardPage *FirstRunWizard::createSummaryPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle(tr("Setup Complete!"));
    page->setSubTitle(tr("Review your configuration before launching BubezView."));

    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(6, 4, 6, 4);

    // Wrap summary inside a proper QGroupBox card to prevent translucent background bleed-through
    auto *groupSummary = new QGroupBox(tr("Summary of Your Choices:"), page);
    auto *summaryLayout = new QVBoxLayout(groupSummary);
    summaryLayout->setContentsMargins(12, 8, 12, 8);

    m_lblSummaryDetails = new QLabel(groupSummary);
    m_lblSummaryDetails->setTextFormat(Qt::RichText);
    m_lblSummaryDetails->setWordWrap(true);
    m_lblSummaryDetails->setStyleSheet("font-size: 12px;");

    summaryLayout->addWidget(m_lblSummaryDetails);
    layout->addWidget(groupSummary);
    layout->addStretch(1);

    return page;
}

void FirstRunWizard::applyPreset(int presetIndex)
{
    m_syncingModules = true;
    switch (presetIndex) {
        case 0: // Complete Suite (All categories with full extension suites)
            if (m_chkCatDocs) m_chkCatDocs->setChecked(true);
            if (m_chkCatImages) m_chkCatImages->setChecked(true);
            if (m_chkCatVideo) m_chkCatVideo->setChecked(true);
            if (m_chkCatAudio) m_chkCatAudio->setChecked(true);
            if (m_chkCatArchives) m_chkCatArchives->setChecked(true);
            break;

        case 1: // Minimal Core (Only base lightweight visualizers, zero heavy packages)
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

    QString profileTitle;
    QString extText;
    QString futureText;

    if (m_radioPresetFull && m_radioPresetFull->isChecked()) {
        profileTitle = tr("<span style='color: #0284c7; font-weight: bold;'>🚀 Complete Suite</span>");
        extText = tr("<b>All extension packages active:</b> LibreOffice document converter, Tesseract OCR, QPDF repair, Digital signatures (PAdES), MPV GPU acceleration, 10-Band Equalizer, TagLib editor, Extended codecs (WebP, TIFF, etc.), 7-Zip Multi-Core.");
        futureText = tr("All newly added extensions in every category will be <b>automatically included and activated</b>.");
    } else if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        profileTitle = tr("<span style='color: #10b981; font-weight: bold;'>⚡ Minimal Core</span>");
        extText = tr("<b>None (Zero heavy background packages):</b> Pure lightweight visualizers using native system decoders (No MPV, No LibreOffice, No OCR, No 7-Zip).");
        futureText = tr("Only base lightweight viewer engines will be updated. No heavy extension packages will ever be installed without your consent.");
    } else {
        profileTitle = tr("<span style='color: #8b5cf6; font-weight: bold;'>⚙️ Custom Selection</span>");

        QStringList selectedCats;
        if (m_chkCatDocs && m_chkCatDocs->isChecked()) selectedCats << tr("Documents & Office (LibreOffice, OCR, QPDF, Signatures)");
        if (m_chkCatArchives && m_chkCatArchives->isChecked()) selectedCats << tr("Archives & ISO (7-Zip, Multi-Core, ISO Tools)");
        if (m_chkCatVideo && m_chkCatVideo->isChecked()) selectedCats << tr("Video & Movies (MPV GPU Acceleration)");
        if (m_chkCatAudio && m_chkCatAudio->isChecked()) selectedCats << tr("Music & Audio (Equalizer, TagLib)");
        if (m_chkCatImages && m_chkCatImages->isChecked()) selectedCats << tr("Images & Graphics (Extended Codecs)");

        if (selectedCats.isEmpty()) {
            extText = tr("No category extensions selected. All categories will run in lightweight Minimal Core mode.");
            futureText = tr("No heavy extension packages will be added.");
        } else {
            extText = tr("Full extensions active <b>only for selected categories:</b><br>• %1<br>Remaining categories run in lightweight Minimal Core mode.").arg(selectedCats.join("<br>• "));
            futureText = tr("Newly added extensions in the future will be <b>automatically added only to your selected categories</b>.");
        }
    }

    QString html = QString(
        "<table width='100%' cellpadding='3' cellspacing='0' style='font-size: 11px;'>"
        "  <tr><td width='22%' valign='top' style='color: #64748b; font-weight: bold;'>%1:</td>"
        "      <td valign='top'>%2</td></tr>"
        "  <tr><td valign='top' style='color: #64748b; font-weight: bold;'>%3:</td>"
        "      <td valign='top'><span style='color: #10b981; font-weight: 600;'>● Always Active:</span> %4</td></tr>"
        "  <tr><td valign='top' style='color: #64748b; font-weight: bold;'>%5:</td>"
        "      <td valign='top'>%6</td></tr>"
        "  <tr><td valign='top' style='color: #64748b; font-weight: bold;'>%7:</td>"
        "      <td valign='top'>%8</td></tr>"
        "</table>"
    ).arg(
        tr("Active Profile"),
        profileTitle,
        tr("Core Engines"),
        tr("PDF, Images, Video (System), Audio (System), Archives & ISO"),
        tr("Extension Suites"),
        extText,
        tr("Future Updates"),
        futureText
    );

    m_lblDynamicSummary->setText(html);
}

void FirstRunWizard::updateSummaryText()
{
    if (!m_lblSummaryDetails) return;

    QString shortcutText = (m_chkUseCtrlSpace && m_chkUseCtrlSpace->isChecked())
        ? tr("Ctrl + Space (trigger)")
        : tr("Space");
    QString cpuText = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked())
        ? tr("🚀 High Performance (Compiled from Source)")
        : tr("📦 Balanced (System Packages)");

    QString modulesPresetText = tr("Custom Configuration");
    if (m_radioPresetFull && m_radioPresetFull->isChecked()) {
        modulesPresetText = tr("Complete Suite (All viewers & extensions active)");
    } else if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        modulesPresetText = tr("Minimal Core (Zero heavy background packages)");
    }

    QStringList activeCategories;
    if (m_radioPresetFull && m_radioPresetFull->isChecked()) {
        activeCategories << tr("All Categories (Full Extensions)");
    } else if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        activeCategories << tr("None (Essential Core Viewers Only)");
    } else {
        if (m_chkCatDocs && m_chkCatDocs->isChecked()) activeCategories << tr("Documents & Office");
        if (m_chkCatImages && m_chkCatImages->isChecked()) activeCategories << tr("Images & Graphics");
        if (m_chkCatVideo && m_chkCatVideo->isChecked()) activeCategories << tr("Video & Movies");
        if (m_chkCatAudio && m_chkCatAudio->isChecked()) activeCategories << tr("Music & Audio");
        if (m_chkCatArchives && m_chkCatArchives->isChecked()) activeCategories << tr("Archives & Disk Images");
        if (activeCategories.isEmpty()) activeCategories << tr("None (Essential Core Viewers Only)");
    }

    QString activeCategoriesStr = activeCategories.join(", ");
    QString viewersStr = tr("PDF, Images, Video, Audio, Archives & ISO");

    QString themeText = m_comboTheme ? m_comboTheme->currentText() : tr("Native");
    QString filmstripText = (m_chkFilmstrip && m_chkFilmstrip->isChecked()) ? tr("Enabled") : tr("Disabled");
    QString escText = (m_chkCloseOnEsc && m_chkCloseOnEsc->isChecked()) ? tr("Escape (Esc)") : tr("Disabled");
    QString geomText = (m_chkRememberGeometry && m_chkRememberGeometry->isChecked()) ? tr("Enabled") : tr("Disabled");
    QString preloadText = (m_chkPreloadCache && m_chkPreloadCache->isChecked()) ? tr("Enabled") : tr("Disabled");
    if (m_chkStandbyMode && m_chkStandbyMode->isChecked()) {
        preloadText += QString(" (%1)").arg(tr("RAM Standby Active"));
    }
    QString desktopIconText = (m_chkDesktopShortcut && m_chkDesktopShortcut->isChecked()) ? tr("Enabled") : tr("Disabled");

    auto makeRow = [](const QString &label, const QString &value) {
        return QString("<tr><td width='38%' style='padding: 3px 6px 3px 0;'><b>%1:</b></td>"
                       "<td style='padding: 3px 0;'>%2</td></tr>").arg(label, value);
    };

    QString rows;
    rows += makeRow(tr("Primary Shortcut"), shortcutText);
    rows += makeRow(tr("Hardware Performance Profile"), cpuText);
    rows += makeRow(tr("Configuration Preset"), modulesPresetText);
    rows += makeRow(tr("Core Viewers"), viewersStr);
    rows += makeRow(tr("Full Extension Suites"), activeCategoriesStr);
    rows += makeRow(tr("Theme"), themeText);
    rows += makeRow(tr("Thumbnail Filmstrip"), filmstripText);
    rows += makeRow(tr("Quick Dismissal Key"), escText);
    rows += makeRow(tr("Remember Window Geometry"), geomText);
    rows += makeRow(tr("Preload Cache"), preloadText);
    rows += makeRow(tr("Desktop Quick Launcher"), desktopIconText);

    QString summaryHtml = QString(
        "<table width='100%' cellspacing='0' cellpadding='0'>%1</table>"
        "<p style='margin-top: 10px; color: #0284c7;'><b>%2</b></p>"
    ).arg(
        rows,
        tr("Click 'Finish & Launch BubezView' to apply these settings and begin!")
    );

    m_lblSummaryDetails->setText(summaryHtml);
}

void FirstRunWizard::updateModuleOptionTexts()
{
    if (!m_radioPresetFull || !m_radioPresetMinimal || !m_radioPresetCustom) return;

    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());

    if (isHighPerformance) {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (Host Compiled)"));
    } else {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (System Packages)"));
    }
    m_radioPresetMinimal->setText(tr("⚡ Minimal Core (Zero Extra Deps)"));
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
    bool isMinimal = m_radioPresetMinimal && m_radioPresetMinimal->isChecked();

    bool enableDocsExt = isFull || (!isMinimal && m_chkCatDocs && m_chkCatDocs->isChecked());
    bool enableImagesExt = isFull || (!isMinimal && m_chkCatImages && m_chkCatImages->isChecked());
    bool enableVideoExt = isFull || (!isMinimal && m_chkCatVideo && m_chkCatVideo->isChecked());
    bool enableAudioExt = isFull || (!isMinimal && m_chkCatAudio && m_chkCatAudio->isChecked());
    bool enableArchivesExt = isFull || (!isMinimal && m_chkCatArchives && m_chkCatArchives->isChecked());

    // Core viewer engines are always available
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

    // Save category-level selection for future extension auto-expansion
    settings.setValue("modules/category_documents_all", enableDocsExt);
    settings.setValue("modules/category_images_all", enableImagesExt);
    settings.setValue("modules/category_video_all", enableVideoExt);
    settings.setValue("modules/category_audio_all", enableAudioExt);
    settings.setValue("modules/category_archives_all", enableArchivesExt);
    settings.setValue("modules/preset", isMinimal ? "minimal" : (isFull ? "full" : "custom"));

    cfg.setEnableInternalOffice(enableOffice);
    settings.setValue("office_use_libreoffice", enableOffice);

    if (isHighPerformance) {
        // High Performance Profile: only trigger recompile if current binary is not already built with -O3
        const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
        if (!alreadyO3 && (enablePdf || enableArchive || enableMpv)) {
            BuildConsoleDialog console(this);
            console.setWindowTitle(tr("Compiling BubezView — High Performance Profile"));
            console.startBuild();
            console.exec();
        }

        // For advanced tools, verify optional system tools (QPDF, Tesseract OCR, 7-Zip, MPV)
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
        // Balanced Profile: check and install missing system packages without compiling
        QStringList componentsToCheck;
        if (enablePdf) componentsToCheck << "engine-poppler";
        if (enableArchive) componentsToCheck << "engine-libarchive";
        if (enableOcr) componentsToCheck << "tesseract";
        if (enableQpdf) componentsToCheck << "qpdf";
        if (enableSevenZip) componentsToCheck << "7zip";
        // Only verify/install MPV package if user specifically enabled MPV GPU acceleration
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
    bool useCtrlSpace = (m_chkUseCtrlSpace && m_chkUseCtrlSpace->isChecked());
    QString chosenShortcut = useCtrlSpace ? "Ctrl+Space" : "Space";
    settings.setValue("shortcut", chosenShortcut);

    // Context menus and global shortcuts are automatically registered in the background
    DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
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
    // Space is reserved for preview/navigation, not closing
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

    // 7. Desktop Shortcut (optional, disabled by default)
    if (m_chkDesktopShortcut && m_chkDesktopShortcut->isChecked()) {
        QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopDir.isEmpty() || !QDir(desktopDir).exists()) {
            if (QDir(QDir::homePath() + "/Scrivania").exists()) {
                desktopDir = QDir::homePath() + "/Scrivania";
            } else {
                desktopDir = QDir::homePath() + "/Desktop";
            }
        }

        if (QDir(desktopDir).exists()) {
            QString desktopFilePath = desktopDir + "/BubezView.desktop";
            QFile file(desktopFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QString projectTrigger = QDir(QCoreApplication::applicationDirPath()).filePath("../trigger_bubez.sh");
                QString triggerScript = QDir::homePath() + "/.local/share/bubezview/trigger_bubez.sh";
                if (QFile::exists(projectTrigger)) {
                    triggerScript = QFileInfo(projectTrigger).canonicalFilePath();
                } else if (QFile::exists(QDir::homePath() + "/.local/share/bubezview/bubezview")) {
                    triggerScript = QDir::homePath() + "/.local/share/bubezview/bubezview";
                } else {
                    triggerScript = QCoreApplication::applicationFilePath();
                }

                QString iconPath = "bubezview";
                QString projectIcon = QDir(QCoreApplication::applicationDirPath()).filePath("../assets/bubezview_icon.png");
                if (QFile::exists(projectIcon)) {
                    iconPath = QFileInfo(projectIcon).canonicalFilePath();
                }
                QString content = QString(
                    "[Desktop Entry]\n"
                    "Version=1.0\n"
                    "Type=Application\n"
                    "Name=BubezView\n"
                    "GenericName=File Viewer\n"
                    "Comment=Universal fast file viewer\n"
                    "Exec=%1 %F\n"
                    "Icon=%2\n"
                    "Terminal=false\n"
                    "Categories=Utility;Viewer;\n"
                    "StartupNotify=true\n"
                    "MimeType=all/allfiles;inode/directory;\n"
                ).arg(triggerScript, iconPath);
                file.write(content.toUtf8());
                file.close();
                file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                    QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                    QFileDevice::ReadOther | QFileDevice::ExeOther);
            }
        }
    }

    settings.sync();
    QWizard::accept();
}
