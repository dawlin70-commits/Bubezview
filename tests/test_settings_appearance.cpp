/**
 * @file test_settings_appearance.cpp
 * @brief Automated unit test and UI snapshot generator for SettingsAppearanceTab.
 */

#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QGroupBox>
#include <QComboBox>
#include <QTabWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTranslator>
#include <QAbstractItemView>
#include <QListView>
#include "ui/settings/settings_appearance_tab.h"
#include "core/services/theme_manager.h"

class TestSettingsAppearance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAppearanceLayoutAndControls();
    void testToggleSwitches();
    void testThemeSwitching();
    void testUiSnapshots();
};

void TestSettingsAppearance::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    ThemeManager::setPreset(ThemePreset::DarkSlate);
}

void TestSettingsAppearance::testAppearanceLayoutAndControls()
{
    SettingsAppearanceTab tab;
    tab.resize(760, 500);
    tab.show();
    QVERIFY(tab.isVisible());

    // 1. Verify 6 symmetric group boxes exist (Theme, Transitions, Language, Window, Scale, IconStyle)
    QList<QGroupBox*> groups = tab.findChildren<QGroupBox*>();
    QCOMPARE(groups.size(), 6);

    // 2. Verify dropdowns exist (Theme, Transition, Language, Scale, IconStyle)
    QList<QComboBox*> combos = tab.findChildren<QComboBox*>();
    QCOMPARE(combos.size(), 5);

    // 3. Verify the single CapsuleToggleSwitch widget exists for Window Geometry
    QList<CapsuleToggleSwitch*> switches = tab.findChildren<CapsuleToggleSwitch*>();
    QCOMPARE(switches.size(), 1);
}

void TestSettingsAppearance::testToggleSwitches()
{
    CapsuleToggleSwitch sw;
    sw.setCheckedAnimated(false);
    QCOMPARE(sw.isChecked(), false);
    QCOMPARE(sw.thumbOffset(), 0.0);

    sw.toggle();
    QCOMPARE(sw.isChecked(), true);

    sw.toggle();
    QCOMPARE(sw.isChecked(), false);
}

void TestSettingsAppearance::testThemeSwitching()
{
    SettingsAppearanceTab tab;
    QSignalSpy spy(&tab, &SettingsAppearanceTab::themeChanged);

    // Find theme combobox (has 3 items: System, DarkSlate, ModernLight)
    QComboBox *comboTheme = nullptr;
    for (auto *cb : tab.findChildren<QComboBox*>()) {
        if (cb->findData(static_cast<int>(ThemePreset::System)) >= 0) {
            comboTheme = cb;
            break;
        }
    }
    QVERIFY(comboTheme != nullptr);
    QCOMPARE(comboTheme->count(), 4);

    // Switch to ZincGray
    int idxZinc = comboTheme->findData(static_cast<int>(ThemePreset::ZincGray));
    QVERIFY(idxZinc >= 0);
    comboTheme->setCurrentIndex(idxZinc);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(ThemeManager::currentPreset(), ThemePreset::ZincGray);

    // Switch to ModernLight
    int idxLight = comboTheme->findData(static_cast<int>(ThemePreset::ModernLight));
    QVERIFY(idxLight >= 0);
    comboTheme->setCurrentIndex(idxLight);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(ThemeManager::currentPreset(), ThemePreset::ModernLight);

    // Switch to System
    int idxSys = comboTheme->findData(static_cast<int>(ThemePreset::System));
    QVERIFY(idxSys >= 0);
    comboTheme->setCurrentIndex(idxSys);

    QCOMPARE(spy.count(), 3);
    QCOMPARE(ThemeManager::currentPreset(), ThemePreset::System);

    // Switch back to DarkSlate
    int idxDark = comboTheme->findData(static_cast<int>(ThemePreset::DarkSlate));
    QVERIFY(idxDark >= 0);
    comboTheme->setCurrentIndex(idxDark);

    QCOMPARE(spy.count(), 4);
    QCOMPARE(ThemeManager::currentPreset(), ThemePreset::DarkSlate);
}

void TestSettingsAppearance::testUiSnapshots()
{
    QString artDir = QStringLiteral("/home/davide/.gemini/antigravity/brain/4e70622e-50ab-4665-afcd-a70645769460");

    // Load Italian translator
    QTranslator trans;
    trans.load(QStringLiteral("/home/davide/Progetti/BubezView/build_deb/translations/bubezview_it.qm"));
    qApp->installTranslator(&trans);

    // Helper lambda to build full dialog
    auto createDialog = [](ThemePreset preset) -> QDialog* {
        ThemeManager::setPreset(preset);
        QDialog *dlg = new QDialog();
        dlg->setWindowTitle(QString::fromUtf8("Impostazioni — BubezView"));
        dlg->resize(820, 640);
        ThemeManager::applyTheme(qApp, nullptr);

        QVBoxLayout *mainLay = new QVBoxLayout(dlg);
        mainLay->setContentsMargins(14, 14, 14, 14);
        mainLay->setSpacing(12);

        QTabWidget *tabWidget = new QTabWidget(dlg);
        SettingsAppearanceTab *appTab = new SettingsAppearanceTab(tabWidget);
        tabWidget->addTab(appTab, QString::fromUtf8("Aspetto"));
        tabWidget->addTab(new QWidget(tabWidget), QString::fromUtf8("Integrazione Desktop"));
        tabWidget->addTab(new QWidget(tabWidget), QString::fromUtf8("Prestazioni"));
        tabWidget->addTab(new QWidget(tabWidget), QString::fromUtf8("Estensioni & Moduli"));
        tabWidget->addTab(new QWidget(tabWidget), QString::fromUtf8("Diagnostica & Manutenzione"));
        mainLay->addWidget(tabWidget);

        QHBoxLayout *botLay = new QHBoxLayout();
        botLay->addStretch();
        QPushButton *btnChiudi = new QPushButton(QString::fromUtf8("Chiudi"), dlg);
        btnChiudi->setFixedSize(100, 32);
        botLay->addWidget(btnChiudi);
        mainLay->addLayout(botLay);

        return dlg;
    };

    // 1. Dark Slate (Proposal 1 Liquid Slate) Snapshot
    {
        QDialog *dlgDark = createDialog(ThemePreset::DarkSlate);
        dlgDark->show();
        QTest::qWait(120);
        QPixmap pixDark = dlgDark->grab();
        pixDark.save(QStringLiteral("%1/settings_dialog_completo_dark.png").arg(artDir));
        dlgDark->deleteLater();
    }

    // 2. Zinc Gray (#505156 base with #727379 pills/controls) Snapshot
    {
        QDialog *dlgZinc = createDialog(ThemePreset::ZincGray);
        dlgZinc->show();
        QTest::qWait(120);
        QPixmap pixZinc = dlgZinc->grab();
        pixZinc.save(QStringLiteral("%1/settings_dialog_completo_zinc.png").arg(artDir));
        dlgZinc->deleteLater();
    }

    // 3. Light Theme Snapshot
    {
        QDialog *dlgLight = createDialog(ThemePreset::ModernLight);
        dlgLight->show();
        QTest::qWait(120);
        QPixmap pixLight = dlgLight->grab();
        pixLight.save(QStringLiteral("%1/settings_dialog_completo_light.png").arg(artDir));
        dlgLight->deleteLater();
    }

    // 4. Open Dropdown Popup in ModernLight (Verifying fix for black-on-black bug)
    {
        ThemeManager::setPreset(ThemePreset::ModernLight);
        QDialog *dlg = createDialog(ThemePreset::ModernLight);
        dlg->show();
        QTest::qWait(80);

        SettingsAppearanceTab *appTab = dlg->findChild<SettingsAppearanceTab*>();
        QVERIFY(appTab != nullptr);
        QList<QComboBox*> combos = appTab->findChildren<QComboBox*>();
        QVERIFY(!combos.isEmpty());
        QComboBox *comboTheme = combos.first();

        comboTheme->showPopup();
        QTest::qWait(80);

        if (comboTheme->view()) {
            QModelIndex idx = comboTheme->model()->index(2, 0);
            comboTheme->view()->setCurrentIndex(idx);
            QTest::qWait(80);
        }

        QWidget *popupWin = QApplication::activePopupWidget();
        if (popupWin) {
            QPixmap pixComboPopup = popupWin->grab();
            pixComboPopup.save(QStringLiteral("%1/settings_dropdown_popup_zinc_fixed.png").arg(artDir));
        }
        comboTheme->hidePopup();
        dlg->deleteLater();
    }

    // Restore DarkSlate
    ThemeManager::setPreset(ThemePreset::DarkSlate);
}

QTEST_MAIN(TestSettingsAppearance)
#include "test_settings_appearance.moc"
