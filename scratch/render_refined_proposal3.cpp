#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPixmap>
#include <QPainter>
#include <iostream>

// Helper to create the outer Settings Dialog window shell
static QWidget* createDialogShell(QWidget* &contentContainer, const QString &themePreset = "zinc")
{
    QWidget *window = new QWidget();
    window->resize(820, 580);
    
    QString winBg = (themePreset == "zinc") ? "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5a5b61, stop:0.5 #505156, stop:1 #444549)" : "#0f172a";
    QString textCol = (themePreset == "zinc") ? "#ffffff" : "#f8fafc";
    
    window->setStyleSheet(QString(
        "QWidget#DialogShell {"
        "  background: %1;"
        "  color: %2;"
        "  font-family: 'Inter', 'Segoe UI', 'Ubuntu', sans-serif;"
        "}"
    ).arg(winBg, textCol));
    window->setObjectName("DialogShell");

    QVBoxLayout *mainLayout = new QVBoxLayout(window);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    // Top tab bar
    QHBoxLayout *tabBarLay = new QHBoxLayout();
    tabBarLay->setContentsMargins(0, 0, 0, 0);
    tabBarLay->setSpacing(4);

    QStringList tabs = {"Aspetto", "Integrazione Desktop", "Prestazioni", "Estensioni && Moduli", "Strumenti di sistema"};
    for (const QString &t : tabs) {
        QPushButton *btnTab = new QPushButton(t, window);
        btnTab->setFixedHeight(32);
        btnTab->setCursor(Qt::PointingHandCursor);
        if (t.contains("Estensioni")) {
            btnTab->setStyleSheet(
                "QPushButton {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
                "  color: #ffffff;"
                "  font-weight: 600;"
                "  font-size: 11.5px;"
                "  border: 1px solid rgba(255, 255, 255, 0.28);"
                "  border-radius: 9px;"
                "  padding: 0 14px;"
                "}"
            );
        } else {
            btnTab->setStyleSheet(
                "QPushButton {"
                "  background: transparent;"
                "  color: #d1d5db;"
                "  font-weight: 500;"
                "  font-size: 11.5px;"
                "  border: 1px solid transparent;"
                "  border-radius: 9px;"
                "  padding: 0 14px;"
                "}"
            );
        }
        tabBarLay->addWidget(btnTab);
    }
    tabBarLay->addStretch();
    mainLayout->addLayout(tabBarLay);

    // Central card area (matching QTabWidget::pane in BubezView)
    QFrame *centralCard = new QFrame(window);
    centralCard->setStyleSheet(
        "QFrame#CentralCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(76, 77, 82, 0.85), stop:1 rgba(60, 61, 66, 0.90));"
        "  border: 1px solid rgba(255, 255, 255, 0.14);"
        "  border-radius: 14px;"
        "}"
    );
    centralCard->setObjectName("CentralCard");

    QVBoxLayout *cardLay = new QVBoxLayout(centralCard);
    cardLay->setContentsMargins(14, 12, 14, 12);
    cardLay->setSpacing(10);

    contentContainer = new QWidget(centralCard);
    cardLay->addWidget(contentContainer, 1);

    mainLayout->addWidget(centralCard, 1);

    // Bottom action row
    QHBoxLayout *bottomLay = new QHBoxLayout();
    bottomLay->setContentsMargins(0, 0, 0, 0);
    bottomLay->addStretch();

    QPushButton *btnClose = new QPushButton("Chiudi", window);
    btnClose->setFixedSize(92, 32);
    btnClose->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255, 255, 255, 0.22);"
        "  border-radius: 8px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}"
    );
    bottomLay->addWidget(btnClose);
    mainLayout->addLayout(bottomLay);

    return window;
}

enum class InstallState {
    InstalledOk,
    Broken,
    NotInstalled
};

enum class SourceMode {
    SystemPackage,
    SourceBuild
};

// Mode with 3 dedicated action icons:
// 1. Install / Uninstall / Health
// 2. Switch Package (📦) <-> Source (⚡)
// 3. Advanced Options (···)
static QWidget* createTile3Icons(const QString &icon, const QString &name, InstallState installState, SourceMode sourceMode, bool canCompile)
{
    QFrame *tile = new QFrame();
    tile->setFixedHeight(46);
    tile->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.05);"
        "  border: 1px solid rgba(255, 255, 255, 0.10);"
        "  border-radius: 9px;"
        "}"
        "QFrame:hover {"
        "  background: rgba(255, 255, 255, 0.09);"
        "  border-color: rgba(255, 255, 255, 0.24);"
        "}"
    );

    QHBoxLayout *lay = new QHBoxLayout(tile);
    lay->setContentsMargins(12, 4, 8, 4);
    lay->setSpacing(8);

    QLabel *lblIcon = new QLabel(icon, tile);
    lblIcon->setStyleSheet("font-size: 16px; background: transparent; border: none;");
    lblIcon->setFixedWidth(24);
    lay->addWidget(lblIcon);

    QLabel *lblName = new QLabel(name, tile);
    lblName->setStyleSheet("font-size: 12px; font-weight: 600; color: #ffffff; background: transparent; border: none;");
    lay->addWidget(lblName, 1);

    // -------------------------------------------------------------
    // ICON 1: Install / Uninstall / State action button
    // -------------------------------------------------------------
    QPushButton *btnInstall = new QPushButton(tile);
    btnInstall->setFixedSize(28, 26);
    btnInstall->setCursor(Qt::PointingHandCursor);

    if (installState == InstallState::InstalledOk) {
        btnInstall->setText("🟢");
        btnInstall->setStyleSheet(
            "QPushButton {"
            "  background: rgba(16, 185, 129, 0.16);"
            "  border: 1px solid rgba(16, 185, 129, 0.40);"
            "  border-radius: 6px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(239, 68, 68, 0.25);"
            "  border-color: rgba(239, 68, 68, 0.50);"
            "}"
        );
        btnInstall->setToolTip("Installato e funzionante (Clicca per disinstallare o gestire)");
    } else if (installState == InstallState::Broken) {
        btnInstall->setText("⚠️");
        btnInstall->setStyleSheet(
            "QPushButton {"
            "  background: rgba(239, 68, 68, 0.22);"
            "  border: 1px solid rgba(239, 68, 68, 0.55);"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(239, 68, 68, 0.35);"
            "}"
        );
        btnInstall->setToolTip("Componente rotto o non funzionante (Clicca per riparare / reinstallare)");
    } else { // NotInstalled
        btnInstall->setText("⚪");
        btnInstall->setStyleSheet(
            "QPushButton {"
            "  background: rgba(255, 255, 255, 0.06);"
            "  border: 1px solid rgba(255, 255, 255, 0.16);"
            "  border-radius: 6px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(16, 185, 129, 0.25);"
            "  border-color: #10b981;"
            "}"
        );
        btnInstall->setToolTip("Non installato (Clicca per installare)");
    }
    lay->addWidget(btnInstall);

    // -------------------------------------------------------------
    // ICON 2: Switch between Package (📦) <-> Source (⚡)
    // -------------------------------------------------------------
    QPushButton *btnSwitchMode = new QPushButton(tile);
    btnSwitchMode->setFixedSize(28, 26);
    btnSwitchMode->setCursor(Qt::PointingHandCursor);

    if (sourceMode == SourceMode::SystemPackage) {
        btnSwitchMode->setText("📦");
        btnSwitchMode->setStyleSheet(
            "QPushButton {"
            "  background: rgba(16, 185, 129, 0.14);"
            "  border: 1px solid rgba(16, 185, 129, 0.35);"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(56, 189, 248, 0.25);"
            "  border-color: #38bdf8;"
            "}"
        );
        if (canCompile) {
            btnSwitchMode->setToolTip("Origine: Pacchetto di sistema (Clicca per commutare a sorgente nativa ⚡)");
        } else {
            btnSwitchMode->setEnabled(false);
            btnSwitchMode->setToolTip("Origine: Pacchetto di sistema (Compilazione da sorgente non prevista)");
        }
    } else { // SourceMode::SourceBuild
        btnSwitchMode->setText("⚡");
        btnSwitchMode->setStyleSheet(
            "QPushButton {"
            "  background: rgba(56, 189, 248, 0.18);"
            "  border: 1px solid rgba(56, 189, 248, 0.45);"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(16, 185, 129, 0.25);"
            "  border-color: #10b981;"
            "}"
        );
        btnSwitchMode->setToolTip("Origine: Compilato da sorgente (Clicca per commutare a pacchetto di sistema 📦)");
    }
    lay->addWidget(btnSwitchMode);

    // -------------------------------------------------------------
    // ICON 3: Advanced Options (···)
    // -------------------------------------------------------------
    QPushButton *btnDots = new QPushButton("···", tile);
    btnDots->setFixedSize(28, 26);
    btnDots->setCursor(Qt::PointingHandCursor);
    btnDots->setStyleSheet(
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.08);"
        "  color: #f4f4f5;"
        "  border: 1px solid rgba(255, 255, 255, 0.16);"
        "  border-radius: 6px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 0.20);"
        "  border-color: rgba(255, 255, 255, 0.35);"
        "}"
    );
    btnDots->setToolTip("Opzioni avanzate (Diagnostica dettagliata, percorsi, log)");
    lay->addWidget(btnDots);

    return tile;
}

static QWidget* buildProposal3With3Icons()
{
    QWidget *content;
    QWidget *win = createDialogShell(content, "zinc");

    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    // Header: Top Filter Capsules + Diagnostics button (NO "Moduli & Motori" title!)
    QHBoxLayout *hdr = new QHBoxLayout();
    hdr->setSpacing(10);

    QFrame *segFrame = new QFrame(content);
    segFrame->setStyleSheet(
        "QFrame { background: rgba(50, 51, 56, 0.85); border: 1px solid rgba(255, 255, 255, 0.14); border-radius: 14px; }"
    );
    QHBoxLayout *segLay = new QHBoxLayout(segFrame);
    segLay->setContentsMargins(3, 3, 3, 3);
    segLay->setSpacing(2);

    QStringList segs = {"Documenti (6)", "Archivi (4)", "Audio && Video (3)", "Ufficio && Grafica (2)"};
    for (int i = 0; i < segs.size(); ++i) {
        QPushButton *b = new QPushButton(segs[i], segFrame);
        b->setFixedHeight(26);
        b->setCursor(Qt::PointingHandCursor);
        if (i == 0) {
            b->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71); color: #ffffff; font-weight: bold; font-size: 11px; border: 1px solid rgba(255, 255, 255, 0.30); border-radius: 11px; padding: 0 12px;");
        } else {
            b->setStyleSheet("background: transparent; color: #d1d5db; font-size: 11px; font-weight: 500; border-radius: 11px; padding: 0 10px; border: none;");
        }
        segLay->addWidget(b);
    }
    hdr->addWidget(segFrame);
    hdr->addStretch();

    QPushButton *btnDiag = new QPushButton("🩺 Verifica componenti...", content);
    btnDiag->setFixedHeight(32);
    btnDiag->setCursor(Qt::PointingHandCursor);
    btnDiag->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255, 255, 255, 0.25);"
        "  border-radius: 8px;"
        "  font-size: 11.5px;"
        "  font-weight: 600;"
        "  padding: 0 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #82838a;"
        "  border-color: rgba(255, 255, 255, 0.40);"
        "}"
    );
    hdr->addWidget(btnDiag);
    lay->addLayout(hdr);

    // Card 1: QGroupBox with exact BubezView Liquid Glass styling
    QGroupBox *card1 = new QGroupBox("Documenti (PDF && Testo)", content);
    card1->setStyleSheet(
        "QGroupBox {"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255, 255, 255, 0.14);"
        "  border-radius: 14px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(76, 77, 82, 0.78), stop:0.5 rgba(65, 66, 71, 0.82), stop:1 rgba(54, 55, 60, 0.88));"
        "  margin-top: 14px;"
        "  padding: 16px 14px 12px 14px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 14px;"
        "  top: 1px;"
        "  padding: 2px 12px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255, 255, 255, 0.28);"
        "  border-radius: 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
    );

    QVBoxLayout *card1Lay = new QVBoxLayout(card1);
    card1Lay->setContentsMargins(6, 10, 6, 6);
    card1Lay->setSpacing(8);

    // 2-Column Grid
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    struct ItemData {
        QString icon;
        QString name;
        InstallState state;
        SourceMode mode;
        bool canCompile;
    };

    QList<ItemData> items = {
        {"📄", "Visualizzatore PDF", InstallState::InstalledOk, SourceMode::SystemPackage, true},
        {"🖼️", "Estrattore immagini", InstallState::InstalledOk, SourceMode::SystemPackage, false},
        {"🌐", "Font asiatici cMap", InstallState::InstalledOk, SourceMode::SystemPackage, false},
        {"🔧", "Riparatore QPDF", InstallState::Broken, SourceMode::SystemPackage, false},         // Problema / Rotto
        {"⚡", "MuPDF Vettoriale", InstallState::InstalledOk, SourceMode::SourceBuild, true},       // Sorgente nativa
        {"📝", "Motore OCR Tesseract", InstallState::NotInstalled, SourceMode::SystemPackage, true} // Non installato
    };

    for (int i = 0; i < items.size(); ++i) {
        QWidget *t = createTile3Icons(items[i].icon, items[i].name, items[i].state, items[i].mode, items[i].canCompile);
        grid->addWidget(t, i / 2, i % 2);
    }
    card1Lay->addLayout(grid);
    lay->addWidget(card1);

    // Card 2: Actions & Legend for the 3 icons
    QGroupBox *card2 = new QGroupBox("Guida rapida ai controlli (3 pulsanti per componente)", content);
    card2->setStyleSheet(card1->styleSheet());
    QVBoxLayout *card2Lay = new QVBoxLayout(card2);
    card2Lay->setContentsMargins(12, 10, 12, 8);
    card2Lay->setSpacing(8);

    QHBoxLayout *legLay = new QHBoxLayout();
    legLay->setSpacing(24);

    auto addLegendBlock = [&](const QString &title, const QString &desc, const QString &titleColor) {
        QVBoxLayout *bLay = new QVBoxLayout();
        bLay->setSpacing(2);
        QLabel *lTitle = new QLabel(title, card2);
        lTitle->setStyleSheet(QString("font-size: 11.5px; font-weight: bold; color: %1; background: transparent; border: none;").arg(titleColor));
        QLabel *lDesc = new QLabel(desc, card2);
        lDesc->setStyleSheet("font-size: 10.5px; color: #d1d5db; background: transparent; border: none;");
        bLay->addWidget(lTitle);
        bLay->addWidget(lDesc);
        legLay->addLayout(bLay);
    };

    addLegendBlock("1. Stato / Installazione", "🟢 Operativo  |  ⚪ Non presente  |  ⚠️ Problema", "#34d399");
    addLegendBlock("2. Switch Sorgente / Pacchetto", "📦 Pacchetto distro  ↔  ⚡ Compilato nativo", "#38bdf8");
    addLegendBlock("3. Opzioni avanzate", "··· Diagnostica singola, percorsi, log", "#e4e4e7");

    legLay->addStretch();
    card2Lay->addLayout(legLay);
    lay->addWidget(card2);

    lay->addStretch(1);

    return win;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QString outDir = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral(".");

    QWidget *win = buildProposal3With3Icons();
    win->show();
    app.processEvents();
    QPixmap p = win->grab();
    p.save(outDir + "/proposta_3_raffinata_3_icone.png");
    delete win;

    std::cout << "Successfully generated proposta_3_raffinata_3_icone.png in " << outDir.toStdString() << std::endl;
    return 0;
}
