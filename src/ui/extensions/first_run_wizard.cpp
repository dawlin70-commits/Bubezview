/**
 * @file first_run_wizard.cpp
 * @brief Implementation of the guided FirstRunWizard dialog.
 */

#include "first_run_wizard.h"
#include "desktop_integrator.h"
#include "build_config.h"
#include "theme_manager.h"
#include "cpu_info.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QListView>

namespace {

/**
 * @class UnsupportedSystemGuideDialog
 * @brief Guided setup dialog providing instructions for configuring global shortcuts on unsupported desktops.
 */
class UnsupportedSystemGuideDialog : public FramelessDialog {
public:
    explicit UnsupportedSystemGuideDialog(QWidget *parent = nullptr)
        : FramelessDialog(parent)
    {
        setDialogTitle(FirstRunWizard::tr("Guida Scorciatoia Desktop"));
        resize(520, 360);
        setModal(true);

        auto *layout = contentLayout();
        layout->setSpacing(12);
        layout->setContentsMargins(16, 14, 16, 14);

        auto *lblHeader = new QLabel(FirstRunWizard::tr("<h3>📋 Configurazione Scorciatoia di Sistema</h3>"), this);
        layout->addWidget(lblHeader);

        auto *lblDesc = new QLabel(
            FirstRunWizard::tr("Nei desktop o window manager senza supporto nativo al tasto Spazio nel file manager (es. XFCE, i3, Sway), "
                               "puoi assegnare una scorciatoia globale (ad es. <b>Ctrl+Spazio</b>) per avviare BubezView con il comando sottostante."),
            this
        );
        lblDesc->setWordWrap(true);
        layout->addWidget(lblDesc);

        auto *grpCmd = new QGroupBox(FirstRunWizard::tr("Comando da associare"), this);
        auto *cmdLayout = new QHBoxLayout(grpCmd);
        cmdLayout->setContentsMargins(10, 8, 10, 8);
        cmdLayout->setSpacing(8);

        QString cmdText = QCoreApplication::applicationFilePath();
        if (cmdText.isEmpty()) {
            cmdText = QStringLiteral("bubezview");
        }

        auto *editCmd = new QLineEdit(cmdText, grpCmd);
        editCmd->setReadOnly(true);
        editCmd->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; padding: 4px;"));
        cmdLayout->addWidget(editCmd, 1);

        auto *btnCopy = new QPushButton(FirstRunWizard::tr("📋 Copia"), grpCmd);
        btnCopy->setCursor(Qt::PointingHandCursor);
        connect(btnCopy, &QPushButton::clicked, this, [btnCopy, cmdText]() {
            if (QClipboard *cb = QGuiApplication::clipboard()) {
                cb->setText(cmdText);
            }
            btnCopy->setText(FirstRunWizard::tr("✅ Copiato!"));
            QTimer::singleShot(1500, btnCopy, [btnCopy]() {
                btnCopy->setText(FirstRunWizard::tr("📋 Copia"));
            });
        });
        cmdLayout->addWidget(btnCopy);
        layout->addWidget(grpCmd);

        auto *lblHint = new QLabel(
            FirstRunWizard::tr("1. Apri le <b>Impostazioni Tastiera</b> del tuo ambiente desktop.<br>"
                               "2. Aggiungi una scorciatoia personalizzata con il comando copiato sopra.<br>"
                               "3. Premi la combinazione scelta (es. <b>Ctrl+Spazio</b>) per visualizzare i file."),
            this
        );
        lblHint->setTextFormat(Qt::RichText);
        layout->addWidget(lblHint);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *btnClose = new QPushButton(FirstRunWizard::tr("Chiudi"), this);
        btnClose->setCursor(Qt::PointingHandCursor);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(btnClose);
        layout->addLayout(btnRow);
    }
};

} // namespace

/**
 * @class TopBarPreviewWidget
 * @brief Compact interactive visual widget rendering the sleek BubezView top bar
 *        live for the 3 themes (Zinc Gray, Dark Slate, Modern Light).
 */
class TopBarPreviewWidget : public QWidget {
public:
    explicit TopBarPreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(40);
        setTheme(QStringLiteral("zinc_gray"));
    }

    void setTheme(const QString &themeCode)
    {
        if (themeCode == QLatin1String("dark_slate")) {
            m_barBgTop = QColor("#1e2538");
            m_barBgBottom = QColor("#131825");
            m_barBorder = QColor(56, 189, 248, 80);
            m_textColor = QColor("#f1f5f9");
            m_btnBg = QColor(255, 255, 255, 20);
            m_btnBorder = QColor(255, 255, 255, 35);
        } else if (themeCode == QLatin1String("modern_light")) {
            m_barBgTop = QColor("#ffffff");
            m_barBgBottom = QColor("#e2e6ec");
            m_barBorder = QColor(0, 0, 0, 40);
            m_textColor = QColor("#1e293b");
            m_btnBg = QColor(0, 0, 0, 16);
            m_btnBorder = QColor(0, 0, 0, 32);
        } else { // ZincGray default
            m_barBgTop = QColor("#44454b");
            m_barBgBottom = QColor("#2e2f33");
            m_barBorder = QColor(255, 255, 255, 45);
            m_textColor = QColor("#f3f4f6");
            m_btnBg = QColor(255, 255, 255, 24);
            m_btnBorder = QColor(255, 255, 255, 45);
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRect r = rect();
        const int radius = 9;

        // Top Bar (Header)
        QRect barRect = r.adjusted(1, 1, -1, -1);
        QLinearGradient barGrad(barRect.topLeft(), barRect.bottomLeft());
        barGrad.setColorAt(0.0, m_barBgTop);
        barGrad.setColorAt(1.0, m_barBgBottom);

        QPainterPath barPath;
        barPath.addRoundedRect(barRect, radius, radius);
        p.fillPath(barPath, barGrad);
        p.setPen(QPen(m_barBorder, 1.0));
        p.drawPath(barPath);

        // Traffic lights (macOS dots on left)
        int dotY = barRect.center().y();
        int dotX = barRect.left() + 14;
        const int dotSize = 10;
        const int dotSpacing = 6;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ef4444"));
        p.drawEllipse(dotX, dotY - dotSize / 2, dotSize, dotSize);
        dotX += dotSize + dotSpacing;

        p.setBrush(QColor("#f59e0b"));
        p.drawEllipse(dotX, dotY - dotSize / 2, dotSize, dotSize);
        dotX += dotSize + dotSpacing;

        p.setBrush(QColor("#10b981"));
        p.drawEllipse(dotX, dotY - dotSize / 2, dotSize, dotSize);

        // Center Title & Document Icon
        QFont titleFont = font();
        titleFont.setPixelSize(11);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.setPen(m_textColor);

        const QString titleText = QStringLiteral("📄 anteprima_documento.pdf  [1/1]");
        QRect textRect(barRect.left() + 70, barRect.top(), barRect.width() - 230, barRect.height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, titleText);

        // Action Buttons Mockup on Right
        int btnW = 66;
        int btnH = 22;
        int btnY = barRect.center().y() - btnH / 2;
        int rightX = barRect.right() - 10;

        auto drawPillBtn = [&](const QString &label, bool primary) {
            rightX -= btnW;
            QRect btnR(rightX, btnY, btnW, btnH);
            QColor bg = primary ? QColor("#0284c7") : m_btnBg;
            QColor border = primary ? QColor("#0369a1") : m_btnBorder;
            QColor txt = primary ? Qt::white : m_textColor;

            p.setBrush(bg);
            p.setPen(QPen(border, 1.0));
            p.drawRoundedRect(btnR, 5, 5);

            QFont btnFont = font();
            btnFont.setPixelSize(10);
            btnFont.setBold(primary);
            p.setFont(btnFont);
            p.setPen(txt);
            p.drawText(btnR, Qt::AlignCenter, label);
            rightX -= 6;
        };

        drawPillBtn(FirstRunWizard::tr("Modifica"), true);
        drawPillBtn(FirstRunWizard::tr("Condividi"), false);
    }

private:
    QColor m_barBgTop;
    QColor m_barBgBottom;
    QColor m_barBorder;
    QColor m_textColor;
    QColor m_btnBg;
    QColor m_btnBorder;
};

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : FramelessDialog(parent)
{
    // Capture initial theme and desktop integration state so reject() can roll back cleanly
    m_initialPreset = ThemeManager::currentPreset();
    const DesktopIntegrator::DesktopEnv initDe = DesktopIntegrator::currentDesktop();
    if (initDe == DesktopIntegrator::DesktopEnv::KDE) {
        m_initialDesktopIntegration = DesktopIntegrator::isDolphinEnabled();
    } else if (initDe == DesktopIntegrator::DesktopEnv::GNOME ||
               initDe == DesktopIntegrator::DesktopEnv::Cinnamon) {
        m_initialDesktopIntegration = DesktopIntegrator::isGnomeNemoEnabled();
    }

    setDialogTitle(tr("Configurazione Guidata BubezView"));
    setResizable(false);

    int targetW = 750;
    int targetH = 510;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        targetW = qMin(targetW, qMax(720, avail.width() - 40));
        targetH = qMin(targetH, qMax(490, avail.height() - 60));
    }
    setFixedSize(targetW, targetH);
    unsetCursor();
    while (QGuiApplication::overrideCursor()) {
        QGuiApplication::restoreOverrideCursor();
    }

    auto *mainLayout = contentLayout();
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    // 4 Sequential Pages
    m_pageStack = new QStackedWidget(this);
    m_pageStack->addWidget(createWelcomePage());      // Page_Welcome = 0
    m_pageStack->addWidget(createSpacebarPage());     // Page_Spacebar = 1
    m_pageStack->addWidget(createPerformancePage());  // Page_Performance = 2
    m_pageStack->addWidget(createSummaryPage());      // Page_Summary = 3
    mainLayout->addWidget(m_pageStack, 1);

    // Bottom Navigation Bar
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(4, 4, 4, 2);
    bottomLayout->setSpacing(10);

    m_btnCancel = new QPushButton(tr("Annulla"), this);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_btnCancel);

    bottomLayout->addStretch();

    m_btnBack = new QPushButton(tr("< Indietro"), this);
    m_btnBack->setCursor(Qt::PointingHandCursor);
    m_btnBack->setVisible(false);
    connect(m_btnBack, &QPushButton::clicked, this, &FirstRunWizard::back);
    bottomLayout->addWidget(m_btnBack);

    m_btnNext = new QPushButton(tr("Avanti >"), this);
    m_btnNext->setCursor(Qt::PointingHandCursor);
    connect(m_btnNext, &QPushButton::clicked, this, &FirstRunWizard::next);
    bottomLayout->addWidget(m_btnNext);

    mainLayout->addLayout(bottomLayout);

    goToPage(Page_Welcome);
    updateThemeStyles();
}

void FirstRunWizard::next()
{
    const int cur = currentId();
    if (cur < Page_Summary) {
        goToPage(cur + 1);
    } else {
        accept();
    }
}

void FirstRunWizard::back()
{
    const int cur = currentId();
    if (cur > Page_Welcome) {
        goToPage(cur - 1);
    }
}

void FirstRunWizard::goToPage(int id)
{
    if (id < 0 || id >= m_pageStack->count()) return;
    m_pageStack->setCurrentIndex(id);

    if (m_btnBack) {
        m_btnBack->setVisible(id != Page_Welcome);
    }

    if (id == Page_Spacebar) {
        runSpacebarAutoTest();
    } else if (id == Page_Summary) {
        refreshSummaryPage();
    }

    if (m_btnNext) {
        if (id != Page_Summary) {
            m_btnNext->setText(tr("Next >"));
        }
        updateNextButtonStyle();
    }
}

int FirstRunWizard::currentId() const
{
    return m_pageStack ? m_pageStack->currentIndex() : 0;
}

QWidget *FirstRunWizard::createWelcomePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(14);
    layout->setContentsMargins(14, 12, 14, 12);

    // Header
    auto *headerBox = new QWidget(page);
    auto *headerLay = new QVBoxLayout(headerBox);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(2);
    auto *lblTitle = new QLabel(tr("Benvenuto in BubezView"), headerBox);
    lblTitle->setStyleSheet("font-size: 16px; font-weight: 700; color: #38bdf8; background: transparent;");
    auto *lblSub = new QLabel(tr("Visualizzatore nativo e anteprima file istantanea per Linux."), headerBox);
    lblSub->setStyleSheet("font-size: 11px; opacity: 0.85; background: transparent;");
    headerLay->addWidget(lblTitle);
    headerLay->addWidget(lblSub);
    layout->addWidget(headerBox);

    // Language Selector Card (Pre-selected with system language)
    auto *grpLang = new QGroupBox(tr("Lingua dell'interfaccia"), page);
    auto *layLang = new QHBoxLayout(grpLang);
    layLang->setContentsMargins(14, 10, 14, 10);
    layLang->setSpacing(12);

    auto *lblLangPrompt = new QLabel(grpLang);
    lblLangPrompt->setStyleSheet("font-size: 11px; font-weight: 600;");
    layLang->addWidget(lblLangPrompt);

    m_comboLanguage = new QComboBox(grpLang);
    m_comboLanguage->setView(new QListView(m_comboLanguage));
    m_comboLanguage->setCursor(Qt::PointingHandCursor);
    m_comboLanguage->addItem(QStringLiteral("Italiano"), QStringLiteral("it"));
    m_comboLanguage->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_comboLanguage->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_comboLanguage->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    m_comboLanguage->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));

    // Detect system language; fallback to English ("en") if unsupported
    QString sysLocale = QLocale::system().name().toLower();
    QString defaultCode = QStringLiteral("en");
    bool matchedSystem = false;
    QString detectedLangName;
    if (sysLocale.startsWith(QLatin1String("it"))) { defaultCode = QStringLiteral("it"); matchedSystem = true; detectedLangName = QStringLiteral("Italiano"); }
    else if (sysLocale.startsWith(QLatin1String("fr"))) { defaultCode = QStringLiteral("fr"); matchedSystem = true; detectedLangName = QStringLiteral("Français"); }
    else if (sysLocale.startsWith(QLatin1String("es"))) { defaultCode = QStringLiteral("es"); matchedSystem = true; detectedLangName = QStringLiteral("Español"); }
    else if (sysLocale.startsWith(QLatin1String("de"))) { defaultCode = QStringLiteral("de"); matchedSystem = true; detectedLangName = QStringLiteral("Deutsch"); }
    else if (sysLocale.startsWith(QLatin1String("en"))) { defaultCode = QStringLiteral("en"); matchedSystem = true; detectedLangName = QStringLiteral("English"); }

    if (matchedSystem) {
        lblLangPrompt->setText(tr("Lingua di sistema rilevata (%1):").arg(detectedLangName));
    } else {
        lblLangPrompt->setText(tr("Lingua predefinita (Inglese — sistema non supportato):"));
    }

    // Prefer previously saved language if re-running wizard from Settings; otherwise auto-detect
    QSettings savedSettings("Bubez", "BubezView");
    const QString savedLang = savedSettings.value("language").toString();
    const QString selectedLangCode = (!savedLang.isEmpty() && m_comboLanguage->findData(savedLang) >= 0)
        ? savedLang
        : defaultCode;

    int langIdx = m_comboLanguage->findData(selectedLangCode);
    if (langIdx >= 0) {
        m_comboLanguage->setCurrentIndex(langIdx);
    }
    layLang->addWidget(m_comboLanguage, 1);
    layout->addWidget(grpLang);

    // Live Theme Visual Preview Box & 3 Theme Cards
    auto *grpTheme = new QGroupBox(tr("Aspetto && Tema Grafico"), page);
    auto *layTheme = new QVBoxLayout(grpTheme);
    layTheme->setContentsMargins(14, 12, 14, 12);
    layTheme->setSpacing(12);

    // Compact live preview mockup of the Bubez bar
    m_barPreview = new TopBarPreviewWidget(grpTheme);
    layTheme->addWidget(m_barPreview);

    // 3 Theme selection buttons
    auto *themeRow = new QHBoxLayout();
    themeRow->setSpacing(10);

    m_themeGroup = new QButtonGroup(this);
    m_themeGroup->setExclusive(true);

    auto makeThemeBtn = [grpTheme](const QString &title, const QString &sub) {
        auto *btn = new QPushButton(QString("%1\n%2").arg(title, sub), grpTheme);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(46);
        return btn;
    };

    m_btnThemeZinc = makeThemeBtn(QStringLiteral("⬛ Zinc Gray"), tr("Studio scuro (#1e1f22)"));
    m_btnThemeDarkSlate = makeThemeBtn(QStringLiteral("🟦 Dark Slate"), tr("Ardesia scuro (#22262b)"));
    m_btnThemeModernLight = makeThemeBtn(QStringLiteral("⬜ Modern Light"), tr("Titanio chiaro (#e2e5ea)"));

    m_themeGroup->addButton(m_btnThemeZinc, 0);
    m_themeGroup->addButton(m_btnThemeDarkSlate, 1);
    m_themeGroup->addButton(m_btnThemeModernLight, 2);

    themeRow->addWidget(m_btnThemeZinc, 1);
    themeRow->addWidget(m_btnThemeDarkSlate, 1);
    themeRow->addWidget(m_btnThemeModernLight, 1);
    layTheme->addLayout(themeRow);

    connect(m_btnThemeZinc, &QPushButton::clicked, this, [this]() { selectTheme("zinc_gray"); });
    connect(m_btnThemeDarkSlate, &QPushButton::clicked, this, [this]() { selectTheme("dark_slate"); });
    connect(m_btnThemeModernLight, &QPushButton::clicked, this, [this]() { selectTheme("modern_light"); });

    QString currentThemeCode = ThemeManager::presetToString(ThemeManager::currentPreset());
    if (currentThemeCode != QLatin1String("dark_slate") && currentThemeCode != QLatin1String("modern_light")) {
        currentThemeCode = QStringLiteral("zinc_gray");
    }
    selectTheme(currentThemeCode);

    layout->addWidget(grpTheme);
    layout->addStretch(1);

    return page;
}

void FirstRunWizard::styleThemeButton(QPushButton *btn, bool active)
{
    if (!btn) return;
    btn->setChecked(active);

    const ThemePreset activePreset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = (activePreset == ThemePreset::ModernLight || activePreset == ThemePreset::BreezeLight);

    if (active) {
        if (btn == m_btnThemeModernLight) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #ffffff;"
                "  border: 2px solid #0284c7;"
                "  border-radius: 8px;"
                "  padding: 6px 4px;"
                "  font-weight: 700;"
                "  font-size: 11px;"
                "  color: #0f172a;"
                "}"
            );
        } else if (btn == m_btnThemeDarkSlate) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #1e2538;"
                "  border: 2px solid #38bdf8;"
                "  border-radius: 8px;"
                "  padding: 6px 4px;"
                "  font-weight: 700;"
                "  font-size: 11px;"
                "  color: #38bdf8;"
                "}"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #2e2f33;"
                "  border: 2px solid #0284c7;"
                "  border-radius: 8px;"
                "  padding: 6px 4px;"
                "  font-weight: 700;"
                "  font-size: 11px;"
                "  color: #38bdf8;"
                "}"
            );
        }
    } else {
        btn->setStyleSheet(
            QString(
                "QPushButton {"
                "  background-color: %1;"
                "  border: 1px solid %2;"
                "  border-radius: 8px;"
                "  padding: 6px 4px;"
                "  font-weight: 500;"
                "  font-size: 11px;"
                "  color: %3;"
                "}"
                "QPushButton:hover {"
                "  border: 1px solid %4;"
                "}"
            ).arg(
                isLight ? "#f8fafc" : "rgba(255, 255, 255, 0.07)",
                isLight ? "rgba(0, 0, 0, 0.16)" : "rgba(255, 255, 255, 0.18)",
                isLight ? "#1e293b" : "#f3f4f6",
                isLight ? "#0284c7" : "rgba(56, 189, 248, 0.65)"
            )
        );
    }
}

void FirstRunWizard::selectTheme(const QString &themeCode)
{
    ThemeManager::setPreset(ThemeManager::stringToPreset(themeCode));
    ThemeManager::applyTheme(qApp, qobject_cast<QMainWindow *>(parentWidget()));

    if (m_barPreview) {
        m_barPreview->setTheme(themeCode);
    }

    styleThemeButton(m_btnThemeZinc, themeCode == QLatin1String("zinc_gray"));
    styleThemeButton(m_btnThemeDarkSlate, themeCode == QLatin1String("dark_slate"));
    styleThemeButton(m_btnThemeModernLight, themeCode == QLatin1String("modern_light"));

    updateThemeStyles();
}

QWidget *FirstRunWizard::createSpacebarPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->setContentsMargins(4, 2, 4, 4);

    // Header
    auto *headerBox = new QWidget(page);
    auto *headerLay = new QVBoxLayout(headerBox);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(2);
    auto *lblTitle = new QLabel(tr("Integrazione Desktop & Tasto Spazio"), headerBox);
    lblTitle->setStyleSheet("font-size: 16px; font-weight: 700; color: #38bdf8; background: transparent;");
    auto *lblSub = new QLabel(tr("Premi la barra Spaziatrice su qualsiasi file per aprire l'anteprima istantanea."), headerBox);
    lblSub->setStyleSheet("font-size: 11px; opacity: 0.85; background: transparent;");
    headerLay->addWidget(lblTitle);
    headerLay->addWidget(lblSub);
    layout->addWidget(headerBox);

    // Automatic Test Card
    auto *grpTest = new QGroupBox(tr("Stato Integrazione Tasto Spazio"), page);
    auto *layTest = new QVBoxLayout(grpTest);
    layTest->setContentsMargins(14, 12, 14, 12);
    layTest->setSpacing(10);

    m_lblTestStatus = new QLabel(grpTest);
    m_lblTestStatus->setTextFormat(Qt::RichText);
    m_lblTestStatus->setWordWrap(true);
    layTest->addWidget(m_lblTestStatus);

    // Fallback explanation card (shown if test does not pass / unsupported desktop)
    m_cardFallback = new QWidget(grpTest);
    auto *layFallback = new QVBoxLayout(m_cardFallback);
    layFallback->setContentsMargins(12, 10, 12, 10);
    layFallback->setSpacing(8);
    m_cardFallback->setStyleSheet(
        "background-color: rgba(245, 158, 11, 0.08);"
        "border: 1px solid rgba(245, 158, 11, 0.35);"
        "border-radius: 8px;"
    );

    auto *lblFallTitle = new QLabel(
        tr("<b>Configurazione Scorciatoia di Sistema:</b> Assegna una scorciatoia globale (es. <b>Ctrl + Spazio</b>) "
           "nelle impostazioni tastiera del tuo desktop a questo comando:"),
        m_cardFallback
    );
    lblFallTitle->setTextFormat(Qt::RichText);
    lblFallTitle->setWordWrap(true);
    lblFallTitle->setStyleSheet("border: none; background: transparent; font-size: 11px;");
    layFallback->addWidget(lblFallTitle);

    auto *cmdRow = new QHBoxLayout();
    cmdRow->setSpacing(8);
    QString cmdPath = QCoreApplication::applicationFilePath();
    if (cmdPath.isEmpty()) cmdPath = QStringLiteral("bubezview");

    auto *lblCmd = new QLabel(QString("<code style='color:#0284c7;'>%1</code>").arg(cmdPath), m_cardFallback);
    lblCmd->setTextFormat(Qt::RichText);
    lblCmd->setStyleSheet("border: none; background: transparent; font-size: 11px;");
    cmdRow->addWidget(lblCmd, 1);

    m_btnCopyCommand = new QPushButton(tr("📋 Copia comando"), m_cardFallback);
    m_btnCopyCommand->setCursor(Qt::PointingHandCursor);
    m_btnCopyCommand->setStyleSheet("font-size: 11px; font-weight: 600; padding: 4px 10px;");
    connect(m_btnCopyCommand, &QPushButton::clicked, this, [this, cmdPath]() {
        if (QClipboard *cb = QGuiApplication::clipboard()) {
            cb->setText(cmdPath);
        }
        m_btnCopyCommand->setText(tr("✅ Copiato!"));
        QTimer::singleShot(1500, this, [this]() {
            if (m_btnCopyCommand) m_btnCopyCommand->setText(tr("📋 Copia comando"));
        });
    });
    cmdRow->addWidget(m_btnCopyCommand);

    m_btnGuide = new QPushButton(tr("Guida..."), m_cardFallback);
    m_btnGuide->setCursor(Qt::PointingHandCursor);
    m_btnGuide->setStyleSheet("font-size: 11px; font-weight: 600; padding: 4px 10px;");
    connect(m_btnGuide, &QPushButton::clicked, this, [this]() {
        UnsupportedSystemGuideDialog dlg(this);
        dlg.exec();
    });
    cmdRow->addWidget(m_btnGuide);

    layFallback->addLayout(cmdRow);
    layTest->addWidget(m_cardFallback);

    layout->addWidget(grpTest);

    // Desktop Integration Checkbox (Selected by default)
    auto *grpInteg = new QGroupBox(tr("Opzioni di Integrazione"), page);
    auto *layInteg = new QVBoxLayout(grpInteg);
    layInteg->setContentsMargins(14, 10, 14, 10);
    layInteg->setSpacing(6);

    m_chkDesktopIntegration = new QCheckBox(
        tr("Abilita integrazione desktop (menu contestuale del mouse e associazione file manager)"),
        grpInteg
    );
    QSettings savedInteg("Bubez", "BubezView");
    m_chkDesktopIntegration->setChecked(savedInteg.value("integration/desktop", true).toBool());
    m_chkDesktopIntegration->setCursor(Qt::PointingHandCursor);
    m_chkDesktopIntegration->setStyleSheet("font-weight: 600; font-size: 11px;");
    layInteg->addWidget(m_chkDesktopIntegration);

    layout->addWidget(grpInteg);
    layout->addStretch(1);

    return page;
}

void FirstRunWizard::runSpacebarAutoTest()
{
    const DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
    const QString deName = DesktopIntegrator::desktopName();

    const bool isSupported = (de == DesktopIntegrator::DesktopEnv::KDE ||
                              de == DesktopIntegrator::DesktopEnv::GNOME ||
                              de == DesktopIntegrator::DesktopEnv::Cinnamon);

    if (isSupported) {
        // Automatically enable and test the file manager integration
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            if (!m_initialDesktopIntegration) m_spacebarTestModifiedSystem = true;
            DesktopIntegrator::enableDolphin("Space");
        } else {
            if (!m_initialDesktopIntegration) m_spacebarTestModifiedSystem = true;
            DesktopIntegrator::enableGnomeNemo();
        }

        bool verified = false;
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            verified = DesktopIntegrator::isDolphinEnabled();
        } else {
            verified = DesktopIntegrator::isGnomeNemoEnabled();
        }

        if (verified) {
            if (m_lblTestStatus) {
                m_lblTestStatus->setText(
                    tr("<div style='background-color: rgba(16, 185, 129, 0.12); border: 1px solid rgba(16, 185, 129, 0.40); border-radius: 8px; padding: 14px 16px;'>"
                       "<div style='color: #10b981; font-weight: 700; font-size: 13px; margin-bottom: 4px;'>"
                       "✅ Integrazione attiva e tasto Spazio assegnato con successo per %1"
                       "</div>"
                       "<div style='font-size: 11px; line-height: 1.5;'>"
                       "L'assegnazione è stata eseguita e verificata in automatico. Premi la <b>Barra Spaziatrice</b> su qualsiasi file nel file manager per aprire l'anteprima istantanea, ed <b>Esc</b> per chiuderla."
                       "</div>"
                       "</div>").arg(deName)
                );
            }
            if (m_cardFallback) {
                m_cardFallback->setVisible(false);
            }
        } else {
            if (m_lblTestStatus) {
                m_lblTestStatus->setText(
                    tr("<div style='background-color: rgba(245, 158, 11, 0.12); border: 1px solid rgba(245, 158, 11, 0.40); border-radius: 8px; padding: 14px 16px;'>"
                       "<div style='color: #f59e0b; font-weight: 700; font-size: 13px; margin-bottom: 4px;'>"
                       "⚠️ Ambiente %1: assegnazione diretta non disponibile"
                       "</div>"
                       "<div style='font-size: 11px; line-height: 1.5;'>"
                       "Per abilitare l'anteprima, configura una scorciatoia globale (ad es. <b>Ctrl+Spazio</b>) associata al comando sottostante."
                       "</div>"
                       "</div>").arg(deName)
                );
            }
            if (m_cardFallback) {
                m_cardFallback->setVisible(true);
            }
        }
    } else {
        if (m_lblTestStatus) {
            m_lblTestStatus->setText(
                tr("<div style='background-color: rgba(245, 158, 11, 0.12); border: 1px solid rgba(245, 158, 11, 0.40); border-radius: 8px; padding: 14px 16px;'>"
                   "<div style='color: #f59e0b; font-weight: 700; font-size: 13px; margin-bottom: 4px;'>"
                   "⚠️ Ambiente rilevato: %1 (tasto Spazio diretto non nativo)"
                   "</div>"
                   "<div style='font-size: 11px; line-height: 1.5;'>"
                   "Il tuo ambiente desktop richiede una scorciatoia di sistema globale per avviare l'anteprima."
                   "</div>"
                   "</div>").arg(deName)
            );
        }
        if (m_cardFallback) {
            m_cardFallback->setVisible(true);
        }
    }
}

QWidget *FirstRunWizard::createPerformancePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->setContentsMargins(4, 2, 4, 4);

    // Header
    auto *headerBox = new QWidget(page);
    auto *headerLay = new QVBoxLayout(headerBox);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(2);
    auto *lblTitle = new QLabel(tr("Prestazioni & Compilazione"), headerBox);
    lblTitle->setStyleSheet("font-size: 16px; font-weight: 700; color: #38bdf8; background: transparent;");
    auto *lblSub = new QLabel(tr("Livello di ottimizzazione per la tua CPU e tempo di riavvio rapido."), headerBox);
    lblSub->setStyleSheet("font-size: 11px; opacity: 0.85; background: transparent;");
    headerLay->addWidget(lblTitle);
    headerLay->addWidget(lblSub);
    layout->addWidget(headerBox);

    // CPU Info Badge
    QString cpuModel = CpuInfo::modelName();
    if (cpuModel.isEmpty()) cpuModel = tr("Processore x86-64");
    QString tier = CpuInfo::microarchitectureTier().toUpper();

    auto *lblCpuPill = new QLabel(
        QString("⚡ <b>%1</b> (%2 thread)  •  ISA: <b>x86-64-%3</b>")
            .arg(cpuModel, QString::number(CpuInfo::logicalCores()), tier.isEmpty() ? QStringLiteral("v3") : tier),
        page
    );
    lblCpuPill->setTextFormat(Qt::RichText);
    lblCpuPill->setStyleSheet(
        "background-color: rgba(2, 132, 199, 0.08); border: 1px solid rgba(56, 189, 248, 0.35); "
        "border-radius: 8px; padding: 6px 12px; font-size: 11px;"
    );
    layout->addWidget(lblCpuPill);

    // 3 Direct Compilation Strategy Cards (clean, instant, no confusing slider)
    auto *grpTier = new QGroupBox(tr("Strategia di Compilazione / Esecuzione"), page);
    auto *layTier = new QVBoxLayout(grpTier);
    layTier->setContentsMargins(14, 12, 14, 12);
    layTier->setSpacing(10);

    auto *tierRow = new QHBoxLayout();
    tierRow->setSpacing(10);

    m_tierGroup = new QButtonGroup(this);
    m_tierGroup->setExclusive(true);

    // Determine hardware-recommended tier from CpuInfo (VM/Laptop/Desktop + core count + ISA)
    const QString recProfile = CpuInfo::recommendedProfile();
    if (recProfile == QLatin1String("standard")) {
        m_recommendedTier = 0;
    } else if (recProfile == QLatin1String("native")) {
        m_recommendedTier = 2;
    } else {
        m_recommendedTier = 1;
    }

    auto makeTierBtn = [grpTier](const QString &title, const QString &sub, bool isRecommended) {
        const QString displayTitle = isRecommended ? (title + QStringLiteral(" ★")) : title;
        auto *btn = new QPushButton(QString("%1\n%2").arg(displayTitle, sub), grpTier);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(46);
        return btn;
    };

    m_btnTierSystem = makeTierBtn(tr("Pacchetti di Sistema"), tr("Richiede sudo"), m_recommendedTier == 0);
    m_btnTierMissing = makeTierBtn(tr("Compila Mancanti"), tr("Zero password (Home)"), m_recommendedTier == 1);
    m_btnTierMax = makeTierBtn(tr("Massime Prestazioni"), tr("Solo per PC potenti"), m_recommendedTier == 2);

    m_tierGroup->addButton(m_btnTierSystem, 0);
    m_tierGroup->addButton(m_btnTierMissing, 1);
    m_tierGroup->addButton(m_btnTierMax, 2);

    tierRow->addWidget(m_btnTierSystem, 1);
    tierRow->addWidget(m_btnTierMissing, 1);
    tierRow->addWidget(m_btnTierMax, 1);
    layTier->addLayout(tierRow);

    m_lblTierDesc = new QLabel(grpTier);
    m_lblTierDesc->setTextFormat(Qt::RichText);
    m_lblTierDesc->setWordWrap(true);
    m_lblTierDesc->setStyleSheet("font-size: 13px; font-weight: 600; margin-top: 8px; border: none; background: transparent;");
    layTier->addWidget(m_lblTierDesc);

    connect(m_btnTierSystem, &QPushButton::clicked, this, [this]() { selectCompilationTier(0); });
    connect(m_btnTierMissing, &QPushButton::clicked, this, [this]() { selectCompilationTier(1); });
    connect(m_btnTierMax, &QPushButton::clicked, this, [this]() { selectCompilationTier(2); });

    // Load previously saved tier if re-running wizard from Settings; otherwise use hardware recommendation
    QSettings savedPerf("Bubez", "BubezView");
    int initialTier = m_recommendedTier;
    if (savedPerf.contains("build_tier")) {
        initialTier = qBound(0, savedPerf.value("build_tier", m_recommendedTier).toInt(), 2);
    }
    selectCompilationTier(initialTier);

    layout->addWidget(grpTier);

    // Fast Restart Standby Card with compact right-aligned combo
    auto *grpStandby = new QGroupBox(tr("Riavvio Rapido (Standby in Memoria RAM)"), page);
    auto *layStandby = new QHBoxLayout(grpStandby);
    layStandby->setContentsMargins(14, 10, 14, 10);
    layStandby->setSpacing(12);

    auto *lblStandbyPrompt = new QLabel(tr("Tempo di permanenza in memoria:"), grpStandby);
    lblStandbyPrompt->setStyleSheet("font-size: 11px; font-weight: 600;");
    layStandby->addWidget(lblStandbyPrompt);
    layStandby->addStretch();

    m_comboStandby = new QComboBox(grpStandby);
    m_comboStandby->setView(new QListView(m_comboStandby));
    m_comboStandby->setCursor(Qt::PointingHandCursor);
    m_comboStandby->setFixedWidth(220);
    m_comboStandby->addItem(tr("Disattivato"), 0);
    m_comboStandby->addItem(tr("10 minuti"), 10);
    m_comboStandby->addItem(tr("30 minuti (Consigliato)"), 30);
    m_comboStandby->addItem(tr("1 ora"), 60);
    m_comboStandby->addItem(tr("Sempre attivo"), -1);

    // Pre-select saved standby duration if available, otherwise 30 minutes
    const int savedStandby = savedPerf.value("background_keep_alive_minutes", 30).toInt();
    int idxStandby = m_comboStandby->findData(savedStandby);
    if (idxStandby < 0) {
        idxStandby = m_comboStandby->findData(30);
    }
    if (idxStandby >= 0) {
        m_comboStandby->setCurrentIndex(idxStandby);
    }
    layStandby->addWidget(m_comboStandby);
    layout->addWidget(grpStandby);

    layout->addStretch(1);
    return page;
}

void FirstRunWizard::styleTierButton(QPushButton *btn, bool active)
{
    if (!btn) return;
    btn->setChecked(active);

    const ThemePreset activePreset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = (activePreset == ThemePreset::ModernLight || activePreset == ThemePreset::BreezeLight);

    if (active) {
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(2, 132, 199, 0.22);"
            "  border: 2px solid #0284c7;"
            "  border-radius: 8px;"
            "  padding: 6px 4px;"
            "  font-weight: 700;"
            "  font-size: 11px;"
            + QString("  color: %1;").arg(isLight ? "#0369a1" : "#38bdf8") +
            "}"
        );
    } else {
        btn->setStyleSheet(
            QString(
                "QPushButton {"
                "  background-color: %1;"
                "  border: 1px solid %2;"
                "  border-radius: 8px;"
                "  padding: 6px 4px;"
                "  font-weight: 500;"
                "  font-size: 11px;"
                "  color: %3;"
                "}"
                "QPushButton:hover {"
                "  border: 1px solid %4;"
                "}"
            ).arg(
                isLight ? "rgba(0, 0, 0, 0.05)" : "rgba(255, 255, 255, 0.07)",
                isLight ? "rgba(0, 0, 0, 0.15)" : "rgba(255, 255, 255, 0.18)",
                isLight ? "#1e293b" : "#f3f4f6",
                isLight ? "#0284c7" : "rgba(56, 189, 248, 0.65)"
            )
        );
    }
}

void FirstRunWizard::selectCompilationTier(int tier)
{
    m_selectedTier = tier;
    styleTierButton(m_btnTierSystem, tier == 0);
    styleTierButton(m_btnTierMissing, tier == 1);
    styleTierButton(m_btnTierMax, tier == 2);

    if (m_lblTierDesc) {
        if (tier == 0) {
            m_lblTierDesc->setText(tr("🔒 Richiede permessi di amministratore (sudo) per installare i pacchetti di sistema."));
        } else if (tier == 1) {
            m_lblTierDesc->setText(tr("✨ Non tocca il sistema e non chiede password (tutto nella tua Home)."));
        } else {
            m_lblTierDesc->setText(tr("⚡ Compilazione nativa con istruzioni CPU avanzate — solo per PC potenti."));
        }
    }
}

QStringList FirstRunWizard::pendingBuildComponents() const
{
    QStringList pendingBuild;
    const QString home = QDir::homePath();

    if (m_selectedTier == 1) {
        // MissingOnly: build core viewing engines that are absent both system-wide and in user-space
        const QStringList candidates = {
            QStringLiteral("poppler-data"),
            QStringLiteral("zstd"),
            QStringLiteral("xz"),
            QStringLiteral("7zip"),
            QStringLiteral("mpv")
        };
        for (const QString &comp : candidates) {
            if (!BuildConfig::hasSystemBinary(comp) && !BuildConfig::hasSourceBinary(comp)) {
                pendingBuild.append(comp);
            }
        }
    } else if (m_selectedTier == 2) {
        // AllStable (Max Performance): ensure locally compiled user-space engines are present
        const QStringList candidates = {
            QStringLiteral("poppler-data"),
            QStringLiteral("zstd"),
            QStringLiteral("xz"),
            QStringLiteral("7zip"),
            QStringLiteral("poppler"),
            QStringLiteral("libarchive")
        };
        for (const QString &comp : candidates) {
            if (comp == QLatin1String("poppler")) {
                if (!QFile::exists(home + QStringLiteral("/.local/bin/pdfimages"))) {
                    pendingBuild.append(comp);
                }
            } else if (comp == QLatin1String("libarchive")) {
                if (!QFile::exists(home + QStringLiteral("/.local/lib/libarchive.so")) &&
                    !QFile::exists(home + QStringLiteral("/.local/lib64/libarchive.so"))) {
                    pendingBuild.append(comp);
                }
            } else if (!BuildConfig::hasSourceBinary(comp)) {
                pendingBuild.append(comp);
            }
        }
    }

    return pendingBuild;
}

QWidget *FirstRunWizard::createSummaryPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);
    layout->setContentsMargins(4, 2, 4, 4);

    // Header
    auto *headerBox = new QWidget(page);
    auto *headerLay = new QVBoxLayout(headerBox);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(2);
    auto *lblTitle = new QLabel(tr("Ready to Launch"), headerBox);
    lblTitle->setStyleSheet("font-size: 16px; font-weight: 700; color: #38bdf8; background: transparent;");
    auto *lblSub = new QLabel(tr("Review your setup before launching BubezView. You can go back to adjust any option."), headerBox);
    lblSub->setStyleSheet("font-size: 11px; opacity: 0.85; background: transparent;");
    headerLay->addWidget(lblTitle);
    headerLay->addWidget(lblSub);
    layout->addWidget(headerBox);

    // Summary Card (5 spacious key-value rows with subtle dividers and unclipped text height)
    auto *grpSummary = new QGroupBox(tr("Selected Configuration"), page);
    auto *laySummary = new QVBoxLayout(grpSummary);
    laySummary->setContentsMargins(14, 12, 14, 10);
    laySummary->setSpacing(0);

    auto addRow = [&](const QString &iconAndLabel, QLabel *&valueLabel, bool addDivider) {
        auto *rowWidget = new QWidget(grpSummary);
        rowWidget->setMinimumHeight(30);
        rowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        rowWidget->setStyleSheet("background: transparent; border: none;");
        auto *rowLay = new QHBoxLayout(rowWidget);
        rowLay->setContentsMargins(4, 5, 4, 5);
        rowLay->setSpacing(14);

        auto *lblKey = new QLabel(iconAndLabel, rowWidget);
        lblKey->setFixedWidth(135);
        lblKey->setMinimumHeight(20);
        lblKey->setStyleSheet("font-size: 12px; font-weight: 700; background: transparent; border: none;");

        valueLabel = new QLabel(rowWidget);
        valueLabel->setWordWrap(true);
        valueLabel->setMinimumHeight(20);
        valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        valueLabel->setStyleSheet("font-size: 12px; font-weight: 500; background: transparent; border: none;");

        rowLay->addWidget(lblKey, 0, Qt::AlignVCenter);
        rowLay->addWidget(valueLabel, 1, Qt::AlignVCenter);
        laySummary->addWidget(rowWidget);

        if (addDivider) {
            auto *line = new QFrame(grpSummary);
            line->setFrameShape(QFrame::HLine);
            line->setFixedHeight(1);
            line->setStyleSheet("background-color: rgba(128, 128, 128, 0.22); border: none;");
            laySummary->addWidget(line);
        }
    };

    addRow(tr("🎨  Appearance"), m_lblSumAppearance, true);
    addRow(tr("⌨️  Desktop"), m_lblSumDesktop, true);
    addRow(tr("💻  Hardware"), m_lblSumHardware, true);
    addRow(tr("⚙️  Engines"), m_lblSumStrategy, true);
    addRow(tr("⚡  Standby"), m_lblSumStandby, false);

    layout->addWidget(grpSummary);

    // Dynamic Action Card (rounded container instead of inline RichText div)
    m_cardSumAction = new QWidget(page);
    m_cardSumAction->setObjectName("summaryActionCard");
    m_cardSumAction->setAttribute(Qt::WA_StyledBackground, true);
    auto *actionLay = new QVBoxLayout(m_cardSumAction);
    actionLay->setContentsMargins(14, 10, 14, 10);
    actionLay->setSpacing(4);

    m_lblSumActionTitle = new QLabel(m_cardSumAction);
    m_lblSumActionTitle->setWordWrap(true);
    m_lblSumActionTitle->setStyleSheet("font-size: 13px; font-weight: 700; border: none; background: transparent;");

    m_lblSumActionDesc = new QLabel(m_cardSumAction);
    m_lblSumActionDesc->setTextFormat(Qt::RichText);
    m_lblSumActionDesc->setWordWrap(true);
    m_lblSumActionDesc->setStyleSheet("font-size: 11px; border: none; background: transparent;");

    actionLay->addWidget(m_lblSumActionTitle);
    actionLay->addWidget(m_lblSumActionDesc);

    layout->addWidget(m_cardSumAction);
    layout->addStretch(1);
    return page;
}

void FirstRunWizard::refreshSummaryPage()
{
    // 1. Language & Theme
    const QString langText = m_comboLanguage ? m_comboLanguage->currentText() : tr("System Default");
    const ThemePreset activePreset = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    QString themeName;
    if (activePreset == ThemePreset::ModernLight || activePreset == ThemePreset::BreezeLight) {
        themeName = tr("Modern Light");
    } else if (activePreset == ThemePreset::DarkSlate) {
        themeName = tr("Dark Slate");
    } else {
        themeName = tr("Zinc Gray");
    }
    if (m_lblSumAppearance) {
        m_lblSumAppearance->setText(QStringLiteral("%1  •  %2").arg(langText, themeName));
    }

    // 2. Desktop Integration & Spacebar
    const bool integrate = !m_chkDesktopIntegration || m_chkDesktopIntegration->isChecked();
    const DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
    const QString deName = DesktopIntegrator::desktopName();
    const bool nativeSpace = (de == DesktopIntegrator::DesktopEnv::KDE ||
                              de == DesktopIntegrator::DesktopEnv::GNOME ||
                              de == DesktopIntegrator::DesktopEnv::Cinnamon);
    if (m_lblSumDesktop) {
        if (integrate) {
            m_lblSumDesktop->setText(nativeSpace
                ? tr("Spacebar active (%1)").arg(deName)
                : tr("Enabled (%1 — global shortcut)").arg(deName));
        } else {
            m_lblSumDesktop->setText(tr("Disabled (manual launch only)"));
        }
    }

    // 3. Detected Hardware & Form Factor
    if (m_lblSumHardware) {
        QString cpuModel = CpuInfo::modelName();
        if (cpuModel.isEmpty()) cpuModel = tr("x86-64 Processor");
        const QString isaTier = CpuInfo::microarchitectureTier().toUpper();
        QString ffLabel;
        if (CpuInfo::isVirtualMachine()) {
            ffLabel = tr("VM");
        } else if (CpuInfo::detectedFormFactor() == CpuInfo::FormFactor::Laptop) {
            ffLabel = tr("Laptop");
        } else {
            ffLabel = tr("Desktop");
        }
        m_lblSumHardware->setText(
            QStringLiteral("%1  (%2T • x86-64-%3 • %4)")
                .arg(cpuModel,
                     QString::number(CpuInfo::logicalCores()),
                     isaTier.isEmpty() ? QStringLiteral("v3") : isaTier,
                     ffLabel)
        );
    }

    // 4. Compilation Strategy
    if (m_lblSumStrategy) {
        if (m_selectedTier == 0) {
            m_lblSumStrategy->setText(tr("System Packages (requires sudo)"));
        } else if (m_selectedTier == 1) {
            m_lblSumStrategy->setText(tr("Compile Missing (Home, no password)"));
        } else {
            m_lblSumStrategy->setText(tr("Maximum Performance (native CPU build)"));
        }
    }

    // 5. Fast Restart Standby
    if (m_lblSumStandby) {
        const QString standbyText = m_comboStandby ? m_comboStandby->currentText() : tr("30 minutes");
        m_lblSumStandby->setText(standbyText);
    }

    // 6. Dynamic Action Preview (single disk scan shared with m_btnNext label)
    const QStringList pending = pendingBuildComponents();
    if (m_btnNext) {
        m_btnNext->setText(pending.isEmpty()
            ? tr("Launch BubezView")
            : tr("Compile & Launch BubezView"));
    }

    if (m_cardSumAction && m_lblSumActionTitle && m_lblSumActionDesc) {
        if (pending.isEmpty()) {
            m_cardSumAction->setStyleSheet(
                "#summaryActionCard {"
                "  background-color: rgba(16, 185, 129, 0.12);"
                "  border: 1px solid rgba(16, 185, 129, 0.40);"
                "  border-radius: 10px;"
                "}"
            );
            m_lblSumActionTitle->setStyleSheet("font-size: 13px; font-weight: 700; color: #10b981; border: none; background: transparent;");
            m_lblSumActionTitle->setText(tr("✅ All set — immediate launch"));
            m_lblSumActionDesc->setText(tr("All core viewing engines are ready. BubezView will open right away."));
        } else {
            m_cardSumAction->setStyleSheet(
                "#summaryActionCard {"
                "  background-color: rgba(2, 132, 199, 0.12);"
                "  border: 1px solid rgba(56, 189, 248, 0.40);"
                "  border-radius: 10px;"
                "}"
            );
            m_lblSumActionTitle->setStyleSheet("font-size: 13px; font-weight: 700; color: #38bdf8; border: none; background: transparent;");
            m_lblSumActionTitle->setText(tr("🛠️ Local user-space build queued"));
            m_lblSumActionDesc->setText(
                tr("Components to compile in your Home folder (no password required): <b>%1</b>")
                    .arg(pending.join(QStringLiteral(", ")))
            );
        }
    }
}

void FirstRunWizard::updateNextButtonStyle()
{
    if (!m_btnNext) return;

    if (currentId() == Page_Summary) {
        // Emerald green finish button to clearly distinguish final confirmation from "Next >"
        m_btnNext->setStyleSheet(
            "QPushButton {"
            "  background-color: #10b981;"
            "  color: #ffffff;"
            "  border: 1px solid #059669;"
            "  border-radius: 6px;"
            "  padding: 6px 16px;"
            "  font-weight: 700;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #059669;"
            "}"
        );
    } else {
        m_btnNext->setStyleSheet(
            "QPushButton {"
            "  background-color: #0284c7;"
            "  color: #ffffff;"
            "  border: 1px solid #0369a1;"
            "  border-radius: 6px;"
            "  padding: 6px 16px;"
            "  font-weight: 700;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #0369a1;"
            "}"
        );
    }
}

void FirstRunWizard::updateThemeStyles()
{
    FramelessDialog::updateThemeStyles();

    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    const bool isLight = (active == ThemePreset::ModernLight || active == ThemePreset::BreezeLight);

    QString cardBg;
    QString cardBorder;
    QString pillBg;
    QString pillBorder;
    QString pillColor;
    QString textCol;
    QString comboBg;
    QString comboBorder;

    if (active == ThemePreset::ZincGray) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 57, 62, 0.78), stop:0.5 rgba(46, 47, 51, 0.82), stop:1 rgba(38, 39, 43, 0.88))");
        cardBorder = QStringLiteral("rgba(255, 255, 255, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        pillBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
        pillColor = QStringLiteral("#ffffff");
        textCol = QStringLiteral("#f3f4f6");
        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e)");
        comboBorder = QStringLiteral("rgba(255, 255, 255, 0.22)");
    } else if (isLight) {
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 255, 255, 0.85), stop:1 rgba(240, 243, 248, 0.80))");
        cardBorder = QStringLiteral("rgba(15, 23, 42, 0.12)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e2e6ec)");
        pillBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
        pillColor = QStringLiteral("#1e293b");
        textCol = QStringLiteral("#1e293b");
        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e8ebf0)");
        comboBorder = QStringLiteral("rgba(0, 0, 0, 0.14)");
    } else { // DarkSlate
        cardBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(30, 41, 59, 0.65), stop:0.5 rgba(22, 30, 46, 0.70), stop:1 rgba(15, 22, 34, 0.78))");
        cardBorder = QStringLiteral("rgba(56, 189, 248, 0.22)");
        pillBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        pillBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
        pillColor = QStringLiteral("#7dd3fc");
        textCol = QStringLiteral("#f8fafc");
        comboBg = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #253347, stop:1 #1a2434)");
        comboBorder = QStringLiteral("rgba(56, 189, 248, 0.38)");
    }

    const QString boxStyle = QString(
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "  background: %3;"
        "  margin-top: 12px;"
        "  padding: 12px 14px 10px 14px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 14px;"
        "  top: 1px;"
        "  padding: 2px 10px;"
        "  background: %4;"
        "  color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel { color: %1; }"
        "QRadioButton { color: %1; }"
        "QCheckBox { color: %1; }"
        "QComboBox {"
        "  background: %7;"
        "  color: %1;"
        "  border: 1px solid %8;"
        "  border-radius: 8px;"
        "  padding: 4px 10px;"
        "}"
    ).arg(textCol, cardBorder, cardBg, pillBg, pillColor, pillBorder, comboBg, comboBorder);

    if (m_pageStack) {
        m_pageStack->setStyleSheet(boxStyle);
    }

    // Refresh theme card buttons
    QString curTheme = ThemeManager::presetToString(active);
    styleThemeButton(m_btnThemeZinc, curTheme == QLatin1String("zinc_gray"));
    styleThemeButton(m_btnThemeDarkSlate, curTheme == QLatin1String("dark_slate"));
    styleThemeButton(m_btnThemeModernLight, curTheme == QLatin1String("modern_light"));

    // Refresh compilation tier card buttons
    styleTierButton(m_btnTierSystem, m_selectedTier == 0);
    styleTierButton(m_btnTierMissing, m_selectedTier == 1);
    styleTierButton(m_btnTierMax, m_selectedTier == 2);

    // Style bottom navigation buttons
    QString navBtnStyle;
    if (isLight) {
        navBtnStyle =
            "QPushButton {"
            "  background-color: #e2e8f0;"
            "  color: #1e293b;"
            "  border: 1px solid #cbd5e1;"
            "  border-radius: 6px;"
            "  padding: 6px 14px;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: #cbd5e1; }";
    } else {
        navBtnStyle =
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  color: #f3f4f6;"
            "  border: 1px solid rgba(255, 255, 255, 0.14);"
            "  border-radius: 6px;"
            "  padding: 6px 14px;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); }";
    }

    if (m_btnBack) m_btnBack->setStyleSheet(navBtnStyle);
    if (m_btnCancel) m_btnCancel->setStyleSheet(navBtnStyle);

    updateNextButtonStyle();
}

void FirstRunWizard::accept()
{
    QSettings settings("Bubez", "BubezView");

    // 1. Mark first run completed
    settings.setValue("first_run_completed", true);

    // 2. Language
    if (m_comboLanguage) {
        QString lang = m_comboLanguage->currentData().toString();
        settings.setValue("language", lang);
    }

    // 3. Defaults applied silently without asking:
    ThemeManager::setUiScalePercent(100);
    settings.setValue("ui_scale", 100);

    ThemeManager::setTransition(TransitionEffect::CrossFade);
    settings.setValue("transition", static_cast<int>(TransitionEffect::CrossFade));

    ThemeManager::setIconStyle(IconStyle::Linear);
    settings.setValue("icon_style", static_cast<int>(IconStyle::Linear));

    settings.setValue("remember_geometry", false);
    settings.setValue("quick_close_on_esc", true);
    settings.setValue("quick_close_on_space", false);
    settings.setValue("cache/preload_adjacent", true);
    settings.setValue("ui/filmstrip", false);
    settings.setValue("show_filmstrip", false);

    // 4. Desktop Integration & Spacebar
    DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
    const bool isSupported = (de == DesktopIntegrator::DesktopEnv::KDE ||
                              de == DesktopIntegrator::DesktopEnv::GNOME ||
                              de == DesktopIntegrator::DesktopEnv::Cinnamon);
    QString chosenShortcut = isSupported ? QStringLiteral("Space") : QStringLiteral("Ctrl+Space");
    settings.setValue("shortcut", chosenShortcut);

    bool integrateDesktop = !m_chkDesktopIntegration || m_chkDesktopIntegration->isChecked();
    if (integrateDesktop) {
        DesktopIntegrator::installAppMenu();
        DesktopIntegrator::enableContextMenu();
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            DesktopIntegrator::enableDolphin(chosenShortcut);
        } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
            DesktopIntegrator::enableGnomeNemo();
        }
        settings.setValue("integration/dolphin", true);
        settings.setValue("integration/desktop", true);
    } else {
        DesktopIntegrator::uninstallAppMenu();
        DesktopIntegrator::disableContextMenu();
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            DesktopIntegrator::disableDolphin();
        } else if (de == DesktopIntegrator::DesktopEnv::GNOME || de == DesktopIntegrator::DesktopEnv::Cinnamon) {
            DesktopIntegrator::disableGnomeNemo();
        }
        settings.setValue("integration/dolphin", false);
        settings.setValue("integration/desktop", false);
    }

    // 5. Office (LibreOffice): auto-detected quietly; if installed enable, else on-demand
    bool hasLo = !QStandardPaths::findExecutable("libreoffice").isEmpty() ||
                 !QStandardPaths::findExecutable("soffice").isEmpty();
    settings.setValue("modules/office_enabled", hasLo);
    settings.setValue("office_use_libreoffice", hasLo);
    ThemeManager::setUseLibreOffice(hasLo);

    // 6. Modules selection: all engines ready on-demand
    settings.setValue("modules/pdf_enabled", true);
    settings.setValue("modules/image_enabled", true);
    settings.setValue("modules/video_enabled", true);
    settings.setValue("modules/audio_enabled", true);
    settings.setValue("modules/archive_enabled", true);
    settings.setValue("modules/media_enabled", true);

    settings.setValue("modules/ocr_enabled", true);
    settings.setValue("modules/qpdf_enabled", true);
    settings.setValue("modules/openssl_enabled", true);
    settings.setValue("modules/mpv_enabled", true);
    settings.setValue("modules/equalizer_enabled", true);
    settings.setValue("modules/taglib_enabled", true);
    settings.setValue("modules/sevenzip_enabled", true);
    settings.setValue("modules/image_formats_enabled", true);
    settings.setValue("modules/preset", "minimal");

    // 7. Compilation Strategy (3 levels)
    int tier = m_selectedTier;
    settings.setValue("build_tier", tier);
    BuildConfig &cfg = BuildConfig::instance();
    if (tier == 0) {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::SystemPackages);
    } else if (tier == 1) {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::MissingOnly);
    } else {
        cfg.applyHardwareProfile(BuildConfig::HardwareProfile::AllStable);
    }
    cfg.save();

    // 8. Fast Restart Standby (default 30 min)
    int standbyMin = m_comboStandby ? m_comboStandby->currentData().toInt() : 30;
    settings.setValue("background_keep_alive_minutes", standbyMin);

    settings.sync();
    FramelessDialog::accept();
}

void FirstRunWizard::reject()
{
    // Restore initial theme preset if the user previewed other themes before cancelling
    if (ThemeManager::currentPreset() != m_initialPreset) {
        ThemeManager::setPreset(m_initialPreset);
        ThemeManager::applyTheme(qApp, qobject_cast<QMainWindow *>(parentWidget()));
    }

    // Roll back temporary Spacebar auto-test integration if it was originally disabled
    if (m_spacebarTestModifiedSystem && !m_initialDesktopIntegration) {
        const DesktopIntegrator::DesktopEnv de = DesktopIntegrator::currentDesktop();
        if (de == DesktopIntegrator::DesktopEnv::KDE) {
            DesktopIntegrator::disableDolphin();
        } else if (de == DesktopIntegrator::DesktopEnv::GNOME ||
                   de == DesktopIntegrator::DesktopEnv::Cinnamon) {
            DesktopIntegrator::disableGnomeNemo();
        }
    }

    FramelessDialog::reject();
}
