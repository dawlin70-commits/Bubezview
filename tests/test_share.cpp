#include <QtTest>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include "share_manager.h"
#include "share_dialog.h"

class TestShare : public QObject
{
    Q_OBJECT

private slots:
    void testAvailabilityChecks();
    void testExecutableLookup();
    void testMenuPopulation();
    void testShareDialogUI();
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
    QVERIFY(actionTexts.contains(QObject::tr("Send via Email...")));
    QVERIFY(actionTexts.contains(QObject::tr("Generate QR Code for smartphone...")));

    menu.show();
    QTest::qWait(100);
    QPixmap menuPix = menu.grab();
    menuPix.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/share_menu_preview.png");
}

void TestShare::testShareDialogUI()
{
    QString testPath = "/home/davide/Progetti/BubezView/test/documento_firmato_test_pag1.jpg";
    ShareDialog dlg(testPath);
    dlg.resize(400, 520);
    dlg.show();
    QTest::qWait(150);

    QPixmap pix = dlg.grab();
    pix.save("/home/davide/.gemini/antigravity/brain/c8aaa138-5782-4f22-ba66-d5f04036bb68/share_dialog_preview.png");
    QVERIFY(!pix.isNull());
}

QTEST_MAIN(TestShare)
#include "test_share.moc"
