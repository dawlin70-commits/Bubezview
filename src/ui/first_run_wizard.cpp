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
#include "compiler_wizard_dialog.h"
#include "build_options_dialog.h"
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
#include <QScrollArea>
#include <QFormLayout>
#include <QStyle>
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDialog>
#include <QKeyEvent>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QGridLayout>

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

    // Compact minimum size to guarantee bottom navigation buttons remain visible on 800x600 / 768p VMs
    setMinimumSize(580, 380);
    int targetW = 680;
    int targetH = 445;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        targetW = qMin(targetW, qMax(580, avail.width() - 40));
        targetH = qMin(targetH, qMax(380, avail.height() - 60));
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
    page->setSubTitle(tr("Select which document readers, media players, and preview engines you want to enable."));

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(4, 2, 4, 2);
    pageLayout->setSpacing(6);

    // 1. Compact Module Setup Presets (2-column grid + dynamic description label)
    auto *groupPresets = new QGroupBox(tr("Module Setup Presets"), page);
    auto *presetsLayout = new QVBoxLayout(groupPresets);
    presetsLayout->setContentsMargins(10, 6, 10, 6);
    presetsLayout->setSpacing(4);

    auto *presetsGrid = new QGridLayout();
    presetsGrid->setHorizontalSpacing(12);
    presetsGrid->setVerticalSpacing(2);

    m_radioPresetFull = new QRadioButton(groupPresets);
    m_radioPresetDocs = new QRadioButton(groupPresets);
    m_radioPresetMedia = new QRadioButton(groupPresets);
    m_radioPresetMinimal = new QRadioButton(groupPresets);
    m_radioPresetCustom = new QRadioButton(tr("⚙️ Custom Configuration"), groupPresets);

    presetsGrid->addWidget(m_radioPresetFull, 0, 0);
    presetsGrid->addWidget(m_radioPresetDocs, 0, 1);
    presetsGrid->addWidget(m_radioPresetMedia, 1, 0);
    presetsGrid->addWidget(m_radioPresetMinimal, 1, 1);
    presetsGrid->addWidget(m_radioPresetCustom, 2, 0, 1, 2);

    presetsLayout->addLayout(presetsGrid);

    m_lblPresetDesc = new QLabel(groupPresets);
    m_lblPresetDesc->setWordWrap(true);
    m_lblPresetDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-top: 2px;");
    presetsLayout->addWidget(m_lblPresetDesc);

    pageLayout->addWidget(groupPresets);

    // 2. Scrollable Granular Components & Capabilities
    auto *groupComponents = new QGroupBox(tr("Active Components && Capabilities"), page);
    auto *groupCompOuterLayout = new QVBoxLayout(groupComponents);
    groupCompOuterLayout->setContentsMargins(4, 4, 4, 4);

    auto *scrollArea = new QScrollArea(groupComponents);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *container = new QWidget(scrollArea);
    auto *compLayout = new QVBoxLayout(container);
    compLayout->setContentsMargins(6, 2, 6, 2);
    compLayout->setSpacing(4);

    auto addCategoryHeader = [compLayout, container](const QString &headerTitle) {
        auto *lblCat = new QLabel(headerTitle, container);
        lblCat->setStyleSheet(
            "font-size: 11px; font-weight: bold; color: #0284c7; "
            "margin-top: 4px; margin-bottom: 1px; text-transform: uppercase; letter-spacing: 0.5px;"
        );
        compLayout->addWidget(lblCat);
    };

    auto addModuleRow = [compLayout, container](
        QCheckBox *&chk, QLabel *&badge,
        const QString &title, const QString &desc
    ) {
        auto *rowWidget = new QWidget(container);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(6);

        auto *colText = new QVBoxLayout();
        colText->setSpacing(0);

        // Escape single '&' into '&&' for QCheckBox so Qt does not hide it as an Alt accelerator
        QString safeTitle = title;
        safeTitle.replace(QRegularExpression("(?<!&)&(?!&)"), "&&");

        chk = new QCheckBox(safeTitle, rowWidget);
        chk->setStyleSheet("font-weight: 600; font-size: 12px;");
        colText->addWidget(chk);

        auto *lblDesc = new QLabel(desc, rowWidget);
        lblDesc->setStyleSheet("color: #64748b; font-size: 10px; margin-left: 20px;");
        lblDesc->setWordWrap(true);
        colText->addWidget(lblDesc);

        badge = new QLabel(rowWidget);
        badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        badge->setTextFormat(Qt::RichText);

        rowLayout->addLayout(colText, 1);
        rowLayout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);

        compLayout->addWidget(rowWidget);
    };

    auto addSubModuleRow = [compLayout, container](
        QCheckBox *&chk, QLabel *&badge,
        const QString &title, const QString &desc
    ) {
        auto *rowWidget = new QWidget(container);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(20, 1, 0, 1);
        rowLayout->setSpacing(6);

        auto *colText = new QVBoxLayout();
        colText->setSpacing(0);

        QString safeTitle = title;
        safeTitle.replace(QRegularExpression("(?<!&)&(?!&)"), "&&");

        chk = new QCheckBox(safeTitle, rowWidget);
        // Inherit native theme text color so it is clearly legible on both Light and Dark themes
        chk->setStyleSheet("font-weight: 500; font-size: 11px;");
        colText->addWidget(chk);

        auto *lblDesc = new QLabel(desc, rowWidget);
        lblDesc->setStyleSheet("color: #64748b; font-size: 10px; margin-left: 20px;");
        lblDesc->setWordWrap(true);
        colText->addWidget(lblDesc);

        badge = new QLabel(rowWidget);
        badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        badge->setTextFormat(Qt::RichText);

        rowLayout->addLayout(colText, 1);
        rowLayout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);

        compLayout->addWidget(rowWidget);
    };

    // Category 1: Documents & Office
    addCategoryHeader(tr("📄 Documents & Office"));
    addModuleRow(
        m_chkModulePdf, m_badgePdf,
        tr("PDF Documents & Digital Signatures"),
        tr("Vector rendering pipeline (Poppler) and cryptographic signature validation (OpenSSL).")
    );
    addSubModuleRow(
        m_chkSubOcr, m_badgeOcr,
        tr("Optical Character Recognition (Tesseract OCR)"),
        tr("Extract selectable UTF-8 text from scanned pages and visual screen snippets.")
    );
    addSubModuleRow(
        m_chkSubQpdf, m_badgeQpdf,
        tr("PDF Repair & Advanced Tools (QPDF / pdfimages)"),
        tr("Fix corrupted PDF streams, linearize for instant loading, and extract embedded lossless images.")
    );
    addModuleRow(
        m_chkModuleOffice, m_badgeOffice,
        tr("Office Document Preview"),
        tr("Preview Microsoft Office (.docx, .xlsx, .pptx) and OpenDocument files via LibreOffice.")
    );

    // Category 2: Multimedia & Audio
    addCategoryHeader(tr("🎬 Multimedia & Audio"));
    addModuleRow(
        m_chkModuleMedia, m_badgeMedia,
        tr("Multimedia Video & Audio Player (MPV)"),
        tr("Hardware GPU video decoding (VA-API/NVDEC) and high-fidelity bit-perfect audio streaming.")
    );
    addSubModuleRow(
        m_chkSubEqualizer, m_badgeEqualizer,
        tr("10-Band Equalizer & Visualizer (FFTW3)"),
        tr("Real-time stereo spectrum visualizer and studio sound equalization.")
    );

    // Category 3: Compressed Archives
    addCategoryHeader(tr("📦 Compressed Archives"));
    addModuleRow(
        m_chkModuleArchive, m_badgeArchive,
        tr("Archive Manager (LibArchive)"),
        tr("Universal archive browsing, cataloging, and direct extraction for ZIP, TAR, 7Z, and ISO.")
    );
    addSubModuleRow(
        m_chkSubSevenZip, m_badgeSevenZip,
        tr("Multi-Core 7-Zip & zstd Compression"),
        tr("Parallel multi-threaded compression across all CPU cores for ZIP, 7Z, and modern zstd archives.")
    );

    scrollArea->setWidget(container);
    groupCompOuterLayout->addWidget(scrollArea);
    pageLayout->addWidget(groupComponents, 1);

    // 3. Fixed Bottom Bar outside ScrollArea so "Advanced Settings..." is always visible
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(2, 0, 2, 0);
    m_lblModulesHint = new QLabel(
        tr("ℹ️ Settings can be customized or recompiled at any time from Settings → Extensions."),
        page
    );
    m_lblModulesHint->setStyleSheet("color: #64748b; font-size: 11px;");
    m_lblModulesHint->setWordWrap(true);
    bottomRow->addWidget(m_lblModulesHint, 1);

    m_btnConfigureExtensions = new QPushButton(tr("⚙️ Advanced Settings..."), page);
    m_btnConfigureExtensions->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnConfigureExtensions);
    pageLayout->addLayout(bottomRow);

    connect(m_btnConfigureExtensions, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this, SettingsDialog::TabIndex::Extensions);
        dlg.exec();
        updateModuleStatusBadges();
    });

    // Wire preset radio buttons to applyPreset
    connect(m_radioPresetFull, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(0);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetDocs, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(1);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetMedia, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(2);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetMinimal, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            applyPreset(3);
            updateModuleOptionTexts();
        }
    });
    connect(m_radioPresetCustom, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            updateModuleOptionTexts();
        }
    });

    // Sub-feature enabling linked to parent checkbox
    connect(m_chkModulePdf, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubOcr) m_chkSubOcr->setEnabled(checked);
        if (m_chkSubQpdf) m_chkSubQpdf->setEnabled(checked);
    });
    connect(m_chkModuleMedia, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubEqualizer) m_chkSubEqualizer->setEnabled(checked);
    });
    connect(m_chkModuleArchive, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_chkSubSevenZip) m_chkSubSevenZip->setEnabled(checked);
    });

    // Wire checkboxes to evaluatePresetFromCheckboxes
    auto onCheckChanged = [this](bool) {
        evaluatePresetFromCheckboxes();
        updateModuleOptionTexts();
    };
    connect(m_chkModulePdf, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubOcr, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubQpdf, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleOffice, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleMedia, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubEqualizer, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkModuleArchive, &QCheckBox::toggled, this, onCheckChanged);
    connect(m_chkSubSevenZip, &QCheckBox::toggled, this, onCheckChanged);

    // Initial state: populate labels, badges, and activate Complete Suite by default
    m_radioPresetFull->setChecked(true);
    applyPreset(0);
    updateModuleOptionTexts();
    updateModuleStatusBadges();

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
        case 0: // Complete Suite
            if (m_chkModulePdf) m_chkModulePdf->setChecked(true);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(true);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(true);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(true);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(true);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(true);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 1: // Documents & Office
            if (m_chkModulePdf) m_chkModulePdf->setChecked(true);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(true);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(true);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(true);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(false);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(false);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 2: // Media & Entertainment
            if (m_chkModulePdf) m_chkModulePdf->setChecked(false);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(false);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(false);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(false);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(true);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(true);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(true);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(true);
            break;
        case 3: // Minimal Core (Zero Extra Dependencies: only core image, text, code & folder viewers)
            if (m_chkModulePdf) m_chkModulePdf->setChecked(false);
            if (m_chkSubOcr) m_chkSubOcr->setChecked(false);
            if (m_chkSubQpdf) m_chkSubQpdf->setChecked(false);
            if (m_chkModuleOffice) m_chkModuleOffice->setChecked(false);
            if (m_chkModuleMedia) m_chkModuleMedia->setChecked(false);
            if (m_chkSubEqualizer) m_chkSubEqualizer->setChecked(false);
            if (m_chkModuleArchive) m_chkModuleArchive->setChecked(false);
            if (m_chkSubSevenZip) m_chkSubSevenZip->setChecked(false);
            break;
        default:
            break;
    }
    m_syncingModules = false;
}

void FirstRunWizard::evaluatePresetFromCheckboxes()
{
    if (m_syncingModules) return;

    bool pdf = m_chkModulePdf && m_chkModulePdf->isChecked();
    bool ocr = m_chkSubOcr && m_chkSubOcr->isChecked();
    bool qpdf = m_chkSubQpdf && m_chkSubQpdf->isChecked();
    bool off = m_chkModuleOffice && m_chkModuleOffice->isChecked();
    bool med = m_chkModuleMedia && m_chkModuleMedia->isChecked();
    bool eq = m_chkSubEqualizer && m_chkSubEqualizer->isChecked();
    bool arc = m_chkModuleArchive && m_chkModuleArchive->isChecked();
    bool sz = m_chkSubSevenZip && m_chkSubSevenZip->isChecked();

    m_syncingModules = true;
    if (pdf && ocr && qpdf && off && med && eq && arc && sz) {
        if (m_radioPresetFull) m_radioPresetFull->setChecked(true);
    } else if (pdf && ocr && qpdf && off && !med && !eq && arc && sz) {
        if (m_radioPresetDocs) m_radioPresetDocs->setChecked(true);
    } else if (!pdf && !ocr && !qpdf && !off && med && eq && arc && sz) {
        if (m_radioPresetMedia) m_radioPresetMedia->setChecked(true);
    } else if (!pdf && !ocr && !qpdf && !off && !med && !eq && !arc && !sz) {
        if (m_radioPresetMinimal) m_radioPresetMinimal->setChecked(true);
    } else {
        if (m_radioPresetCustom) m_radioPresetCustom->setChecked(true);
    }
    m_syncingModules = false;
}

void FirstRunWizard::updateModuleStatusBadges()
{
    if (m_badgePdf) {
        bool ready = BuildConfig::isPkgInstalled("poppler-qt6") || !BuildConfig::pdfimagesExecutable().isEmpty() || BuildConfig::instance().staticPdf();
        m_badgePdf->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                  : tr("<span style='color: #f59e0b;'>○ Available</span>"));
    }
    if (m_badgeOcr) {
        bool ready = BuildConfig::isTesseractAvailable();
        m_badgeOcr->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                  : tr("<span style='color: #64748b;'>○ Optional</span>"));
    }
    if (m_badgeQpdf) {
        bool ready = BuildConfig::isQpdfAvailable();
        m_badgeQpdf->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                   : tr("<span style='color: #64748b;'>○ Optional</span>"));
    }
    if (m_badgeOffice) {
        bool ready = !QStandardPaths::findExecutable("libreoffice").isEmpty();
        m_badgeOffice->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                     : tr("<span style='color: #64748b;'>○ Optional: libreoffice</span>"));
    }
    if (m_badgeMedia) {
        bool ready = BuildConfig::isPkgInstalled("mpv") || !QStandardPaths::findExecutable("mpv").isEmpty();
        m_badgeMedia->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                    : tr("<span style='color: #64748b;'>○ Available</span>"));
    }
    if (m_badgeEqualizer) {
        m_badgeEqualizer->setText(tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>"));
    }
    if (m_badgeArchive) {
        bool ready = BuildConfig::isPkgInstalled("libarchive");
        m_badgeArchive->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                      : tr("<span style='color: #64748b;'>○ Available</span>"));
    }
    if (m_badgeSevenZip) {
        bool ready = BuildConfig::isSevenZipAvailable();
        m_badgeSevenZip->setText(ready ? tr("<span style='color: #10b981; font-weight: 600;'>● Ready</span>")
                                       : tr("<span style='color: #64748b;'>○ Optional: 7zip</span>"));
    }
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
        modulesPresetText = tr("Complete Suite (All modules enabled)");
    } else if (m_radioPresetDocs && m_radioPresetDocs->isChecked()) {
        modulesPresetText = tr("Documents & Office");
    } else if (m_radioPresetMedia && m_radioPresetMedia->isChecked()) {
        modulesPresetText = tr("Media & Entertainment");
    } else if (m_radioPresetMinimal && m_radioPresetMinimal->isChecked()) {
        modulesPresetText = tr("Minimal Core");
    }

    QStringList activeModulesList;
    if (m_chkModulePdf && m_chkModulePdf->isChecked()) {
        QString pdfStr = tr("PDF");
        QStringList pdfExtras;
        if (m_chkSubOcr && m_chkSubOcr->isChecked()) pdfExtras << "OCR";
        if (m_chkSubQpdf && m_chkSubQpdf->isChecked()) pdfExtras << "QPDF";
        if (!pdfExtras.isEmpty()) {
            pdfStr += QString(" (%1)").arg(pdfExtras.join("+"));
        }
        activeModulesList << pdfStr;
    }
    if (m_chkModuleOffice && m_chkModuleOffice->isChecked()) activeModulesList << tr("Office Preview");
    if (m_chkModuleMedia && m_chkModuleMedia->isChecked()) {
        QString medStr = tr("Media");
        if (m_chkSubEqualizer && m_chkSubEqualizer->isChecked()) medStr += tr(" (+Equalizer)");
        activeModulesList << medStr;
    }
    if (m_chkModuleArchive && m_chkModuleArchive->isChecked()) {
        QString arcStr = tr("Archives");
        if (m_chkSubSevenZip && m_chkSubSevenZip->isChecked()) arcStr += tr(" (+7-Zip)");
        activeModulesList << arcStr;
    }
    QString activeModulesStr = activeModulesList.isEmpty() ? tr("None (Core Viewers Only)") : activeModulesList.join(", ");

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
    rows += makeRow(tr("Modules Preset"), modulesPresetText);
    rows += makeRow(tr("Enabled Modules"), activeModulesStr);
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
    if (!m_radioPresetFull || !m_radioPresetDocs || !m_radioPresetMedia || !m_radioPresetMinimal) return;

    bool isHighPerformance = (m_radioCpuDesktop && m_radioCpuDesktop->isChecked());

    // Keep radio button labels concise on a single line (double '&&' so Qt renders literal '&')
    if (isHighPerformance) {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (Host Compiled)"));
        m_radioPresetDocs->setText(tr("📄 Documents && Office (Host Compiled)"));
    } else {
        m_radioPresetFull->setText(tr("🚀 Complete Suite (System Packages)"));
        m_radioPresetDocs->setText(tr("📄 Documents && Office (System Packages)"));
    }
    m_radioPresetMedia->setText(tr("🎬 Media && Entertainment"));
    m_radioPresetMinimal->setText(tr("🪶 Minimal Core (Zero Extra Deps)"));

    if (!m_lblPresetDesc) return;

    if (m_radioPresetFull->isChecked()) {
        m_lblPresetDesc->setText(isHighPerformance
            ? tr("All features compiled from source with aggressive CPU optimizations (-O3, -march=native).")
            : tr("All features using distribution precompiled packages with zero compile wait time."));
    } else if (m_radioPresetDocs->isChecked()) {
        m_lblPresetDesc->setText(isHighPerformance
            ? tr("Optimized PDF vector rendering, LibreOffice preview, OCR, and digital signatures.")
            : tr("PDF viewing, LibreOffice integration, OCR, and cryptographic signatures."));
    } else if (m_radioPresetMedia->isChecked()) {
        m_lblPresetDesc->setText(
            tr("Hardware GPU video playback, 10-band audio equalizer, and archive management.")
        );
    } else if (m_radioPresetMinimal->isChecked()) {
        m_lblPresetDesc->setText(
            tr("Fast native image, code, and folder preview with instant cold startup and zero extra dependencies.")
        );
    } else {
        m_lblPresetDesc->setText(
            tr("Choose each component and engine manually using the checkboxes below.")
        );
    }
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

    // 3. Execution of Compilation / Package Installation based on chosen modules
    bool enablePdf = m_chkModulePdf && m_chkModulePdf->isChecked();
    bool enableOcr = m_chkSubOcr && m_chkSubOcr->isChecked();
    bool enableQpdf = m_chkSubQpdf && m_chkSubQpdf->isChecked();
    bool enableOffice = m_chkModuleOffice && m_chkModuleOffice->isChecked();
    bool enableMedia = m_chkModuleMedia && m_chkModuleMedia->isChecked();
    bool enableEqualizer = m_chkSubEqualizer && m_chkSubEqualizer->isChecked();
    bool enableArchive = m_chkModuleArchive && m_chkModuleArchive->isChecked();
    bool enableSevenZip = m_chkSubSevenZip && m_chkSubSevenZip->isChecked();

    settings.setValue("modules/pdf_enabled", enablePdf);
    settings.setValue("modules/ocr_enabled", enableOcr);
    settings.setValue("modules/qpdf_enabled", enableQpdf);
    settings.setValue("modules/office_enabled", enableOffice);
    settings.setValue("modules/media_enabled", enableMedia);
    settings.setValue("modules/equalizer_enabled", enableEqualizer);
    settings.setValue("modules/archive_enabled", enableArchive);
    settings.setValue("modules/sevenzip_enabled", enableSevenZip);

    cfg.setEnableInternalOffice(enableOffice);
    settings.setValue("office_use_libreoffice", enableOffice);

    if (isHighPerformance) {
        // High Performance Profile: only trigger recompile if current binary is not already built with -O3
        const bool alreadyO3 = (QStringLiteral(BUBEZ_OPT_LEVEL) == QLatin1String("O3"));
        if (!alreadyO3 && (enablePdf || enableArchive || enableMedia)) {
            BuildConsoleDialog console(this);
            console.setWindowTitle(tr("Compiling BubezView — High Performance Profile"));
            console.startBuild();
            console.exec();
        }

        // For advanced tools, verify optional system tools (QPDF, Tesseract OCR, 7-Zip)
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
        if (enableMedia) componentsToCheck << "mpv";

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
