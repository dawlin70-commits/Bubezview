#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTabWidget>
#include <QScrollArea>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QDir>
#include <iostream>

// Helper to create the outer Settings Dialog window shell
static QWidget* createDialogShell(const QString &activeTabName, QWidget* &contentContainer)
{
    QWidget *window = new QWidget();
    window->resize(820, 580);
    window->setStyleSheet(
        "QWidget#DialogShell {"
        "  background-color: #3b3c40;"
        "  color: #f4f4f5;"
        "  font-family: 'Inter', 'Segoe UI', 'Ubuntu', sans-serif;"
        "}"
    );
    window->setObjectName("DialogShell");

    QVBoxLayout *mainLayout = new QVBoxLayout(window);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    // Fake top tab bar
    QHBoxLayout *tabBarLay = new QHBoxLayout();
    tabBarLay->setContentsMargins(0, 0, 0, 0);
    tabBarLay->setSpacing(4);

    QStringList tabs = {"Aspetto", "Integrazione Desktop", "Prestazioni", "Estensioni && Moduli", "Strumenti di sistema"};
    for (const QString &t : tabs) {
        QPushButton *btnTab = new QPushButton(t, window);
        btnTab->setFixedHeight(32);
        btnTab->setCursor(Qt::PointingHandCursor);
        if (t == activeTabName) {
            btnTab->setStyleSheet(
                "QPushButton {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71);"
                "  color: #ffffff;"
                "  font-weight: bold;"
                "  font-size: 11px;"
                "  border: 1px solid rgba(255, 255, 255, 0.28);"
                "  border-radius: 8px;"
                "  padding: 0 12px;"
                "}"
            );
        } else {
            btnTab->setStyleSheet(
                "QPushButton {"
                "  background: transparent;"
                "  color: #a1a1aa;"
                "  font-weight: 500;"
                "  font-size: 11px;"
                "  border: 1px solid transparent;"
                "  border-radius: 8px;"
                "  padding: 0 12px;"
                "}"
            );
        }
        tabBarLay->addWidget(btnTab);
    }
    tabBarLay->addStretch();
    mainLayout->addLayout(tabBarLay);

    // Central card area
    QFrame *centralCard = new QFrame(window);
    centralCard->setStyleSheet(
        "QFrame#CentralCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(76, 77, 82, 0.40), stop:1 rgba(54, 55, 60, 0.50));"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 12px;"
        "}"
    );
    centralCard->setObjectName("CentralCard");

    QVBoxLayout *cardLay = new QVBoxLayout(centralCard);
    cardLay->setContentsMargins(14, 12, 14, 12);
    cardLay->setSpacing(10);

    contentContainer = new QWidget(centralCard);
    cardLay->addWidget(contentContainer, 1);

    mainLayout->addWidget(centralCard, 1);

    // Bottom action row with Close button
    QHBoxLayout *bottomLay = new QHBoxLayout();
    bottomLay->setContentsMargins(0, 0, 0, 0);
    bottomLay->addStretch();

    QPushButton *btnClose = new QPushButton("Chiudi", window);
    btnClose->setFixedHeight(32);
    btnClose->setFixedWidth(90);
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

// Helper to create a single 2-column compact extension tile
static QWidget* createExtensionTile(const QString &iconText, const QString &name, const QString &statusText, const QString &badgeType)
{
    QFrame *tile = new QFrame();
    tile->setFixedHeight(46);
    tile->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.045);"
        "  border: 1px solid rgba(255, 255, 255, 0.10);"
        "  border-radius: 9px;"
        "}"
        "QFrame:hover {"
        "  background: rgba(255, 255, 255, 0.08);"
        "  border-color: rgba(255, 255, 255, 0.22);"
        "}"
    );

    QHBoxLayout *lay = new QHBoxLayout(tile);
    lay->setContentsMargins(10, 4, 8, 4);
    lay->setSpacing(8);

    QLabel *lblIcon = new QLabel(iconText, tile);
    lblIcon->setStyleSheet("font-size: 15px; background: transparent; border: none;");
    lblIcon->setFixedWidth(22);
    lay->addWidget(lblIcon);

    QLabel *lblName = new QLabel(name, tile);
    lblName->setStyleSheet("font-size: 11.5px; font-weight: 600; color: #f4f4f5; background: transparent; border: none;");
    lay->addWidget(lblName, 1);

    // Badge
    QLabel *lblBadge = new QLabel(statusText, tile);
    lblBadge->setFixedHeight(24);
    lblBadge->setAlignment(Qt::AlignCenter);

    if (badgeType == "sys") {
        lblBadge->setStyleSheet(
            "background: rgba(16, 185, 129, 0.14);"
            "color: #34d399;"
            "border: 1px solid rgba(16, 185, 129, 0.32);"
            "border-radius: 6px;"
            "padding: 0 8px;"
            "font-size: 10.5px;"
            "font-weight: 600;"
        );
    } else if (badgeType == "src") {
        lblBadge->setStyleSheet(
            "background: rgba(56, 189, 248, 0.14);"
            "color: #38bdf8;"
            "border: 1px solid rgba(56, 189, 248, 0.32);"
            "border-radius: 6px;"
            "padding: 0 8px;"
            "font-size: 10.5px;"
            "font-weight: 600;"
        );
    } else {
        lblBadge->setStyleSheet(
            "background: rgba(255, 255, 255, 0.06);"
            "color: #a1a1aa;"
            "border: 1px solid rgba(255, 255, 255, 0.15);"
            "border-radius: 6px;"
            "padding: 0 8px;"
            "font-size: 10.5px;"
            "font-weight: 500;"
        );
    }
    lay->addWidget(lblBadge);

    // 3 dots button
    QPushButton *btnDots = new QPushButton("···", tile);
    btnDots->setFixedSize(26, 24);
    btnDots->setStyleSheet(
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.06);"
        "  color: #e4e4e7;"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 6px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 0.15);"
        "}"
    );
    lay->addWidget(btnDots);

    return tile;
}

// -----------------------------------------------------------------------------
// Proposal 1: 2-Column Grid with Category Tabs (No "Tutti" tab)
// -----------------------------------------------------------------------------
static QWidget* buildProposal1()
{
    QWidget *content;
    QWidget *win = createDialogShell("Estensioni && Moduli", content);

    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    // Header
    QHBoxLayout *hdr = new QHBoxLayout();
    QVBoxLayout *titleLay = new QVBoxLayout();
    titleLay->setSpacing(2);
    QLabel *lblTitle = new QLabel("Moduli funzionali ed estensioni", content);
    lblTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    QLabel *lblSub = new QLabel("Stato di attivazione e sorgente dei motori di BubezView", content);
    lblSub->setStyleSheet("font-size: 11px; color: #a1a1aa;");
    titleLay->addWidget(lblTitle);
    titleLay->addWidget(lblSub);
    hdr->addLayout(titleLay, 1);

    QPushButton *btnDiag = new QPushButton("🩺 Verifica componenti...", content);
    btnDiag->setFixedHeight(28);
    btnDiag->setStyleSheet(
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.08);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255, 255, 255, 0.20);"
        "  border-radius: 6px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 0 12px;"
        "}"
    );
    hdr->addWidget(btnDiag);
    lay->addLayout(hdr);

    // Sub-tabs (No "Tutti")
    QHBoxLayout *catTabs = new QHBoxLayout();
    catTabs->setSpacing(6);
    QStringList cats = {"📄 Documenti (PDF)", "📦 Archivi", "🎬 Audio & Video", "🎨 Ufficio & Grafica"};
    for (int i = 0; i < cats.size(); ++i) {
        QPushButton *btn = new QPushButton(cats[i], content);
        btn->setFixedHeight(28);
        if (i == 0) {
            btn->setStyleSheet(
                "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71); color: #fff; font-weight: bold; font-size: 11px; border: 1px solid rgba(255,255,255,0.25); border-radius: 14px; padding: 0 14px; }"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton { background: rgba(255,255,255,0.06); color: #a1a1aa; font-weight: 500; font-size: 11px; border: 1px solid rgba(255,255,255,0.12); border-radius: 14px; padding: 0 14px; }"
            );
        }
        catTabs->addWidget(btn);
    }
    catTabs->addStretch();
    lay->addLayout(catTabs);

    // 2-Column Grid of tiles
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    struct Item { QString icon; QString name; QString status; QString type; };
    QList<Item> pdfItems = {
        {"📄", "Visualizzatore PDF (Poppler)", "📦 Sistema", "sys"},
        {"🖼️", "Estrazione Immagini (pdfimages)", "📦 Sistema", "sys"},
        {"🌐", "Supporto Font Asiatici (cMap)", "📦 Sistema", "sys"},
        {"🔧", "Linearizzazione & Riparazione (QPDF)", "📦 Sistema", "sys"},
        {"⚡", "MuPDF Advanced Engine (mutool)", "⚡ Sorgente", "src"},
        {"📝", "Riconoscimento Testo OCR (Tesseract)", "📦 Sistema", "sys"},
        {"🔒", "Firme Digitali Crittografiche (OpenSSL)", "📦 Sistema", "sys"},
        {"⚙️", "Compilatore Moduli Nativo", "● Attivo", "sys"}
    };

    for (int i = 0; i < pdfItems.size(); ++i) {
        int r = i / 2;
        int c = i % 2;
        grid->addWidget(createExtensionTile(pdfItems[i].icon, pdfItems[i].name, pdfItems[i].status, pdfItems[i].type), r, c);
    }
    lay->addLayout(grid);
    lay->addStretch(1);

    return win;
}

// -----------------------------------------------------------------------------
// Proposal 2: 4 Themed Cards (No subtabs, everything visible at once)
// -----------------------------------------------------------------------------
static QWidget* buildProposal2()
{
    QWidget *content;
    QWidget *win = createDialogShell("Estensioni && Moduli", content);

    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    // Header
    QHBoxLayout *hdr = new QHBoxLayout();
    QVBoxLayout *titleLay = new QVBoxLayout();
    titleLay->setSpacing(2);
    QLabel *lblTitle = new QLabel("Moduli funzionali ed estensioni", content);
    lblTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    QLabel *lblSub = new QLabel("Panoramica completa di tutti i componenti organizzati per ambito d'uso", content);
    lblSub->setStyleSheet("font-size: 11px; color: #a1a1aa;");
    titleLay->addWidget(lblTitle);
    titleLay->addWidget(lblSub);
    hdr->addLayout(titleLay, 1);

    QPushButton *btnDiag = new QPushButton("🩺 Verifica componenti...", content);
    btnDiag->setFixedHeight(28);
    btnDiag->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.08); color: #fff; border: 1px solid rgba(255,255,255,0.20); border-radius: 6px; font-size: 11px; font-weight: 600; padding: 0 12px; }"
    );
    hdr->addWidget(btnDiag);
    lay->addLayout(hdr);

    auto makeCard = [](const QString &catTitle, const QList<QPair<QString, QString>> &items) -> QWidget* {
        QFrame *card = new QFrame();
        card->setStyleSheet(
            "QFrame { background: rgba(255, 255, 255, 0.035); border: 1px solid rgba(255, 255, 255, 0.09); border-radius: 10px; }"
        );
        QVBoxLayout *cLay = new QVBoxLayout(card);
        cLay->setContentsMargins(10, 8, 10, 8);
        cLay->setSpacing(6);

        QLabel *lbl = new QLabel(catTitle, card);
        lbl->setStyleSheet("font-size: 11px; font-weight: bold; color: #38bdf8; background: transparent; border: none;");
        cLay->addWidget(lbl);

        QGridLayout *g = new QGridLayout();
        g->setSpacing(6);
        for (int i = 0; i < items.size(); ++i) {
            g->addWidget(createExtensionTile("•", items[i].first, items[i].second, items[i].second.contains("Sistema") ? "sys" : (items[i].second.contains("Sorgente") ? "src" : "off")), i / 2, i % 2);
        }
        cLay->addLayout(g);
        return card;
    };

    QScrollArea *scroll = new QScrollArea(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget#ScrollW { background: transparent; }");

    QWidget *scrollW = new QWidget(scroll);
    scrollW->setObjectName("ScrollW");
    QVBoxLayout *sLay = new QVBoxLayout(scrollW);
    sLay->setContentsMargins(0, 0, 4, 0);
    sLay->setSpacing(8);

    sLay->addWidget(makeCard("📄 Documenti & PDF", {
        {"Visualizzatore PDF (Poppler)", "📦 Sistema"}, {"Estrazione Immagini (pdfimages)", "📦 Sistema"},
        {"Supporto Font Asiatici (cMap)", "📦 Sistema"}, {"Linearizzazione & Riparazione (QPDF)", "📦 Sistema"},
        {"MuPDF Advanced Engine", "⚡ Sorgente"}, {"Riconoscimento Testo OCR", "📦 Sistema"}
    }));

    sLay->addWidget(makeCard("📦 Archivi & Compressione", {
        {"Gestione Archivi (LibArchive)", "📦 Sistema"}, {"Compressione Zstandard", "📦 Sistema"},
        {"Compressione XZ", "📦 Sistema"}, {"Motore 7-Zip Extra (7zz)", "⚪ Non installato"}
    }));

    sLay->addWidget(makeCard("🎬 Audio, Video & Grafica", {
        {"Elaboratore Audio DSP (FFTW3)", "📦 Sistema"}, {"Metadati Audio (TagLib)", "📦 Sistema"},
        {"Conversione Office (LibreOffice)", "📦 Sistema"}, {"Generatore Codici QR", "📦 Sistema"}
    }));

    scroll->setWidget(scrollW);
    lay->addWidget(scroll, 1);

    return win;
}

// -----------------------------------------------------------------------------
// Proposal 3: Unified 2-Column Grid with Inline Filter Pills (Control Center style)
// -----------------------------------------------------------------------------
static QWidget* buildProposal3()
{
    QWidget *content;
    QWidget *win = createDialogShell("Estensioni && Moduli", content);

    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    // Compact Header with Inline Filter Bar
    QHBoxLayout *hdr = new QHBoxLayout();
    hdr->setSpacing(10);

    QLabel *lblTitle = new QLabel("Moduli & Motori", content);
    lblTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    hdr->addWidget(lblTitle);

    // Segmented filter capsules
    QFrame *segFrame = new QFrame(content);
    segFrame->setStyleSheet(
        "QFrame { background: rgba(0, 0, 0, 0.25); border: 1px solid rgba(255, 255, 255, 0.12); border-radius: 14px; }"
    );
    QHBoxLayout *segLay = new QHBoxLayout(segFrame);
    segLay->setContentsMargins(2, 2, 2, 2);
    segLay->setSpacing(2);

    QStringList segs = {"Documenti (6)", "Archivi (4)", "Media (3)", "Ufficio (2)"};
    for (int i = 0; i < segs.size(); ++i) {
        QPushButton *b = new QPushButton(segs[i], segFrame);
        b->setFixedHeight(24);
        if (i == 0) {
            b->setStyleSheet("background: #0284c7; color: #fff; font-weight: bold; font-size: 10.5px; border-radius: 12px; padding: 0 10px; border: none;");
        } else {
            b->setStyleSheet("background: transparent; color: #a1a1aa; font-size: 10.5px; border-radius: 12px; padding: 0 8px; border: none;");
        }
        segLay->addWidget(b);
    }
    hdr->addWidget(segFrame);
    hdr->addStretch();

    QPushButton *btnDiag = new QPushButton("🩺 Verifica componenti", content);
    btnDiag->setFixedHeight(28);
    btnDiag->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.08); color: #fff; border: 1px solid rgba(255,255,255,0.20); border-radius: 6px; font-size: 11px; font-weight: 600; padding: 0 12px; }"
    );
    hdr->addWidget(btnDiag);
    lay->addLayout(hdr);

    // Quick Stats Bar
    QFrame *statsBar = new QFrame(content);
    statsBar->setFixedHeight(32);
    statsBar->setStyleSheet("background: rgba(16, 185, 129, 0.08); border: 1px solid rgba(16, 185, 129, 0.22); border-radius: 8px;");
    QHBoxLayout *stLay = new QHBoxLayout(statsBar);
    stLay->setContentsMargins(12, 0, 12, 0);
    QLabel *lblStats = new QLabel("✓ 6 motori attivi su 6 • Nessun errore di rendering • Tutti i pacchetti pronti all'uso", statsBar);
    lblStats->setStyleSheet("color: #34d399; font-size: 11px; font-weight: 500; background: transparent; border: none;");
    stLay->addWidget(lblStats);
    lay->addWidget(statsBar);

    // 2-Column Grid
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    struct Item { QString icon; QString name; QString status; QString type; };
    QList<Item> items = {
        {"📄", "Visualizzatore PDF", "● Sistema", "sys"},
        {"🖼️", "Estrattore immagini", "● Sistema", "sys"},
        {"🌐", "Font asiatici cMap", "● Sistema", "sys"},
        {"🔧", "Riparatore QPDF", "● Sistema", "sys"},
        {"⚡", "MuPDF Vettoriale", "⚡ Sorgente", "src"},
        {"📝", "Motore OCR Tesseract", "● Sistema", "sys"}
    };

    for (int i = 0; i < items.size(); ++i) {
        grid->addWidget(createExtensionTile(items[i].icon, items[i].name, items[i].status, items[i].type), i / 2, i % 2);
    }
    lay->addLayout(grid);
    lay->addStretch(1);

    return win;
}

// -----------------------------------------------------------------------------
// Proposal 4: Master-Detail (Categories list on the left, 2-column grid on the right)
// -----------------------------------------------------------------------------
static QWidget* buildProposal4()
{
    QWidget *content;
    QWidget *win = createDialogShell("Estensioni && Moduli", content);

    QHBoxLayout *mainSplit = new QHBoxLayout(content);
    mainSplit->setContentsMargins(0, 0, 0, 0);
    mainSplit->setSpacing(14);

    // Left Sidebar
    QFrame *sideBar = new QFrame(content);
    sideBar->setFixedWidth(210);
    sideBar->setStyleSheet(
        "QFrame { background: rgba(0, 0, 0, 0.20); border: 1px solid rgba(255, 255, 255, 0.10); border-radius: 10px; }"
    );
    QVBoxLayout *sideLay = new QVBoxLayout(sideBar);
    sideLay->setContentsMargins(8, 10, 8, 10);
    sideLay->setSpacing(4);

    QLabel *lblCatHdr = new QLabel("CATEGORIE", sideBar);
    lblCatHdr->setStyleSheet("font-size: 10px; font-weight: bold; color: #71717a; padding: 2px 6px; border: none; background: transparent;");
    sideLay->addWidget(lblCatHdr);

    struct CatNav { QString icon; QString title; int count; bool sel; };
    QList<CatNav> navs = {
        {"📄", "Documenti & PDF", 6, true},
        {"📦", "Archivi & File", 4, false},
        {"🎬", "Audio & Video", 3, false},
        {"🎨", "Grafica & Ufficio", 2, false}
    };

    for (const auto &n : navs) {
        QPushButton *btnNav = new QPushButton(sideBar);
        btnNav->setFixedHeight(36);
        QHBoxLayout *btnLay = new QHBoxLayout(btnNav);
        btnLay->setContentsMargins(10, 0, 10, 0);
        QLabel *lIcon = new QLabel(n.icon + "  " + n.title, btnNav);
        lIcon->setStyleSheet("font-size: 11px; border: none; background: transparent;");
        QLabel *lCount = new QLabel(QString::number(n.count), btnNav);
        lCount->setStyleSheet("font-size: 10px; color: #a1a1aa; font-weight: bold; border: none; background: transparent;");
        btnLay->addWidget(lIcon, 1);
        btnLay->addWidget(lCount);

        if (n.sel) {
            btnNav->setStyleSheet("QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a7b82, stop:1 #6a6b71); border: 1px solid rgba(255,255,255,0.22); border-radius: 8px; color: #fff; font-weight: bold; }");
        } else {
            btnNav->setStyleSheet("QPushButton { background: transparent; border: 1px solid transparent; border-radius: 8px; color: #a1a1aa; font-weight: 500; } QPushButton:hover { background: rgba(255,255,255,0.06); }");
        }
        sideLay->addWidget(btnNav);
    }
    sideLay->addStretch();

    QPushButton *btnSideDiag = new QPushButton("🩺 Verifica rapida", sideBar);
    btnSideDiag->setFixedHeight(30);
    btnSideDiag->setStyleSheet("background: rgba(255,255,255,0.08); color: #fff; border: 1px solid rgba(255,255,255,0.18); border-radius: 6px; font-size: 10.5px; font-weight: 600;");
    sideLay->addWidget(btnSideDiag);

    mainSplit->addWidget(sideBar);

    // Right Content Area
    QWidget *rightArea = new QWidget(content);
    QVBoxLayout *rightLay = new QVBoxLayout(rightArea);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(10);

    QHBoxLayout *rHdr = new QHBoxLayout();
    QLabel *lblSection = new QLabel("Documenti & Testo (PDF)", rightArea);
    lblSection->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    rHdr->addWidget(lblSection, 1);

    QPushButton *btnInstAll = new QPushButton("⚡ Installa consigliati", rightArea);
    btnInstAll->setFixedHeight(26);
    btnInstAll->setStyleSheet("background: #0284c7; color: #fff; border: 1px solid #38bdf8; border-radius: 6px; font-size: 10.5px; font-weight: bold; padding: 0 10px;");
    rHdr->addWidget(btnInstAll);
    rightLay->addLayout(rHdr);

    QGridLayout *rGrid = new QGridLayout();
    rGrid->setSpacing(8);

    struct Item { QString icon; QString name; QString status; QString type; };
    QList<Item> items = {
        {"📄", "Visualizzatore PDF", "● Sistema", "sys"},
        {"🖼️", "Estrattore immagini", "● Sistema", "sys"},
        {"🌐", "Font asiatici cMap", "● Sistema", "sys"},
        {"🔧", "Riparatore QPDF", "● Sistema", "sys"},
        {"⚡", "MuPDF Vettoriale", "⚡ Sorgente", "src"},
        {"📝", "OCR Tesseract", "● Sistema", "sys"}
    };

    for (int i = 0; i < items.size(); ++i) {
        rGrid->addWidget(createExtensionTile(items[i].icon, items[i].name, items[i].status, items[i].type), i / 2, i % 2);
    }
    rightLay->addLayout(rGrid);
    rightLay->addStretch(1);

    mainSplit->addWidget(rightArea, 1);

    return win;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString outDir = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral(".");

    // Generate Proposal 1
    {
        QWidget *w1 = buildProposal1();
        w1->show();
        app.processEvents();
        QPixmap p = w1->grab();
        p.save(outDir + "/proposta_1_griglia_2col_schede.png");
        delete w1;
    }

    // Generate Proposal 2
    {
        QWidget *w2 = buildProposal2();
        w2->show();
        app.processEvents();
        QPixmap p = w2->grab();
        p.save(outDir + "/proposta_2_quattro_card_tematiche.png");
        delete w2;
    }

    // Generate Proposal 3
    {
        QWidget *w3 = buildProposal3();
        w3->show();
        app.processEvents();
        QPixmap p = w3->grab();
        p.save(outDir + "/proposta_3_griglia_unificata_pillole.png");
        delete w3;
    }

    // Generate Proposal 4
    {
        QWidget *w4 = buildProposal4();
        w4->show();
        app.processEvents();
        QPixmap p = w4->grab();
        p.save(outDir + "/proposta_4_master_detail_laterale.png");
        delete w4;
    }

    std::cout << "Successfully generated all 4 proposals in " << outDir.toStdString() << std::endl;
    return 0;
}
