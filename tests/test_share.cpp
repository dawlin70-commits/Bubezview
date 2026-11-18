#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QAction>
#include "share_manager.h"
#include "share_dialog.h"
#include "vector_icons.h"

class TestShare : public QObject
{
    Q_OBJECT

private slots:
    void testAvailabilityChecks();
    void testExecutableLookup();
    void testMenuPopulation();
    void testShareDialogUI();
    void testVectorIcons();
};

void TestShare::testAvailabilityChecks()
{
    // KDE Connect, xdg-email, and localsend should be available on this system
    bool kde = ShareManager::isAvailable({"kdeconnect-cli", "org.kde.kdeconnect"});
    QVERIFY(kde);

    bool localSend = ShareManager::isAvailable({"localsend_app", "org.localsend.localsend_app", "localsend"});
    QVERIFY(localSend);

    // Dummy tool should NOT be available
    bool dummy = ShareManager::isAvailable({"nonexistent_binary_xyz_12345", "org.nonexistent.app"});
    QVERIFY(!dummy);
}

void TestShare::testExecutableLookup()
{
    QStringList exe = ShareManager::getExecutable({"localsend_app", "org.localsend.localsend_app", "localsend"});
    QVERIFY(!exe.isEmpty());
    QVERIFY(exe.first().contains("localsend"));

    QStringList kdeExe = ShareManager::getExecutable({"kdeconnect-cli", "org.kde.kdeconnect"});
    QVERIFY(!kdeExe.isEmpty());
    QVERIFY(kdeExe.first().contains("kdeconnect"));
}

void TestShare::testMenuPopulation()
{
    QMenu menu("Condividi");
    ShareManager manager(nullptr, []() { return QString("/home/davide/Progetti/BubezView/test_images/sample.jpg"); });
    manager.populateMenu(&menu);

    // On this machine: KDE Connect, LocalSend, Email, and QR Code must all be added (at least 4 actions)
    QVERIFY(menu.actions().size() >= 4);

    QStringList actionTexts;
    for (QAction *act : menu.actions()) {
        actionTexts.append(act->text());
    }

    QVERIFY(actionTexts.contains(QObject::tr("Send to device (KDE Connect)...")));
    QVERIFY(actionTexts.contains(QObject::tr("Send with LocalSend...")));
    QVERIFY(actionTexts.contains(QObject::tr("Send via Email")));
    QVERIFY(actionTexts.contains(QObject::tr("Generate QR Code for smartphone...")));

    // Verify Email submenu items (System, Gmail, Outlook, Yahoo)
    QAction *emailAct = nullptr;
    for (QAction *act : menu.actions()) {
        if (act->text() == QObject::tr("Send via Email")) {
            emailAct = act;
            break;
        }
    }
    QVERIFY(emailAct != nullptr);
    QVERIFY(emailAct->menu() != nullptr);
    QStringList emailSubTexts;
    for (QAction *sub : emailAct->menu()->actions()) {
        emailSubTexts.append(sub->text());
    }
    QVERIFY(std::any_of(emailSubTexts.begin(), emailSubTexts.end(), [](const QString &t) { return t.contains("Gmail"); }));
    QVERIFY(std::any_of(emailSubTexts.begin(), emailSubTexts.end(), [](const QString &t) { return t.contains("Outlook"); }));
    QVERIFY(std::any_of(emailSubTexts.begin(), emailSubTexts.end(), [](const QString &t) { return t.contains("Yahoo"); }));

    // Verify paged document image submenu appears when isPagedDocument=true
    QMenu pdfMenu("SharePDF");
    ShareManager pdfManager(nullptr, []() { return QString("/tmp/dummy.pdf"); });
    pdfManager.setImageProvider([]() { return QImage(10, 10, QImage::Format_RGB32); }, true);
    pdfManager.populateMenu(&pdfMenu);
    QAction *pdfEmailAct = nullptr;
    for (QAction *act : pdfMenu.actions()) {
        if (act->text() == QObject::tr("Send via Email")) {
            pdfEmailAct = act;
            break;
        }
    }
    QVERIFY(pdfEmailAct != nullptr && pdfEmailAct->menu() != nullptr);
    bool hasPageAsImg = false;
    for (QAction *sub : pdfEmailAct->menu()->actions()) {
        if (sub->text().contains("Current Page as Image") && sub->menu() != nullptr) {
            hasPageAsImg = true;
            break;
        }
    }
    QVERIFY(hasPageAsImg);

    // Verify last email provider persistence
    ShareManager::EmailProvider prev = ShareManager::lastEmailProvider();
    ShareManager::setLastEmailProvider(ShareManager::EmailProvider::Gmail);
    QCOMPARE(ShareManager::lastEmailProvider(), ShareManager::EmailProvider::Gmail);
    ShareManager::setLastEmailProvider(prev);

    menu.show();
    QTest::qWait(100);
    QPixmap menuPix = menu.grab();
    menuPix.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/share_menu_preview.png");
}

void TestShare::testShareDialogUI()
{
    QString testPath = "/home/davide/Progetti/BubezView/test/documento_firmato_test_pag1.jpg";
    ShareDialog dlg(testPath);
    dlg.resize(440, 520);
    dlg.show();
    QTest::qWait(150);

    // Verify Copy Path button works and populates clipboard
    bool foundCopyBtn = false;
    for (QPushButton *btn : dlg.findChildren<QPushButton*>()) {
        if (btn->text().contains("Copy Path") || btn->text().contains("Copia")) {
            foundCopyBtn = true;
            btn->click();
            QCOMPARE(QApplication::clipboard()->text(), testPath);
            break;
        }
    }
    QVERIFY(foundCopyBtn);

    QPixmap pix = dlg.grab();
    pix.save("/home/davide/.gemini/antigravity/brain/4e70622e-50ab-4665-afcd-a70645769460/share_dialog_preview.png");
    QVERIFY(!pix.isNull());
}

void TestShare::testVectorIcons()
{
    // Verify that all new vector icons produce valid, crisp, non-null pixmaps
    QList<VectorIcons::Type> types = {
        VectorIcons::Type::Folder,
        VectorIcons::Type::FolderDownloads,
        VectorIcons::Type::FolderPictures,
        VectorIcons::Type::FolderDocuments,
        VectorIcons::Type::FolderMusic,
        VectorIcons::Type::FolderVideos,
        VectorIcons::Type::FolderDesktop,
        VectorIcons::Type::Code,
        VectorIcons::Type::Zip,
        VectorIcons::Type::Image,
        VectorIcons::Type::Media,
        VectorIcons::Type::Document,
        VectorIcons::Type::Pdf,
        VectorIcons::Type::Office,
        VectorIcons::Type::Text
    };

    for (VectorIcons::Type t : types) {
        QIcon icon = VectorIcons::createIcon(t);
        QVERIFY(!icon.isNull());
        QPixmap pix = icon.pixmap(96, 96);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(96, 96));
    }
}

QTEST_MAIN(TestShare)
#include "test_share.moc"
