/**
 * @file settings_extensions_tab.cpp
 * @brief Implementation of the extensions catalog settings tab.
 */

#include "settings_extensions_tab.h"
#include "extension_feature_model.h"
#include "extension_row_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QTabWidget>
#include <QScrollArea>

SettingsExtensionsTab::SettingsExtensionsTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsExtensionsTab::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(14, 12, 14, 12);

    QLabel *lblHeader = new QLabel(
        tr("<b>Plugins & Extensions</b>"),
        this
    );
    lblHeader->setWordWrap(true);
    layout->addWidget(lblHeader);

    // Category Tabs Widget (Styled as modern segmented pill chips)
    m_catTabs = new QTabWidget(this);
    m_catTabs->setObjectName("ExtensionCategoryTabs");
    m_catTabs->setStyleSheet(
        "#ExtensionCategoryTabs::pane {"
        "    border: none;"
        "    background-color: transparent;"
        "    margin-top: 6px;"
        "}"
        "#ExtensionCategoryTabs > QTabBar {"
        "    background-color: transparent;"
        "}"
        "#ExtensionCategoryTabs > QTabBar::tab {"
        "    background-color: rgba(100, 116, 139, 0.12);"
        "    color: #475569;"
        "    border: 1px solid rgba(100, 116, 139, 0.22);"
        "    border-radius: 13px;"
        "    padding: 5px 14px;"
        "    margin-right: 6px;"
        "    font-weight: 600;"
        "    font-size: 11px;"
        "}"
        "#ExtensionCategoryTabs > QTabBar::tab:hover {"
        "    background-color: rgba(2, 132, 199, 0.12);"
        "    color: #0284c7;"
        "    border-color: rgba(2, 132, 199, 0.35);"
        "}"
        "#ExtensionCategoryTabs > QTabBar::tab:selected {"
        "    background-color: #0284c7;"
        "    color: #ffffff;"
        "    border: 1px solid #0284c7;"
        "}"
    );

    m_catTabs->addTab(createCategoryPage("All"), tr("All"));

    // Lazy load specific category pages only when clicked by user
    QWidget *dummyPdf = new QWidget();
    dummyPdf->setProperty("catFilter", "PDF");
    m_catTabs->addTab(dummyPdf, tr("PDF"));

    QWidget *dummyArchive = new QWidget();
    dummyArchive->setProperty("catFilter", "Archive");
    m_catTabs->addTab(dummyArchive, tr("Archives"));

    QWidget *dummyMedia = new QWidget();
    dummyMedia->setProperty("catFilter", "Media");
    m_catTabs->addTab(dummyMedia, tr("Audio && Video"));

    QWidget *dummyOffice = new QWidget();
    dummyOffice->setProperty("catFilter", "Office");
    m_catTabs->addTab(dummyOffice, tr("Office"));

    QWidget *dummyImage = new QWidget();
    dummyImage->setProperty("catFilter", "Image");
    m_catTabs->addTab(dummyImage, tr("Images && Photos"));

    connect(m_catTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index > 0) {
            QWidget *w = m_catTabs->widget(index);
            if (w && w->property("catFilter").isValid()) {
                QString filter = w->property("catFilter").toString();
                QString title = m_catTabs->tabText(index);
                QWidget *realPage = createCategoryPage(filter);
                m_catTabs->removeTab(index);
                m_catTabs->insertTab(index, realPage, title);
                m_catTabs->setCurrentIndex(index);
                w->deleteLater();
            }
        }
    });

    layout->addWidget(m_catTabs, 1);
}

QWidget *SettingsExtensionsTab::createCategoryPage(const QString &catFilter)
{
    QWidget *page = new QWidget();
    QVBoxLayout *pageLay = new QVBoxLayout(page);
    pageLay->setContentsMargins(4, 8, 16, 16);
    pageLay->setSpacing(8);

    QList<ExtensionModuleInfo> mods;
    if (catFilter.isEmpty() || catFilter == "All") {
        mods = ExtensionFeatureRegistry::allModules();
    } else {
        mods = ExtensionFeatureRegistry::modulesForCategory(catFilter);
    }

    QString lastCategory;
    for (const auto &m : mods) {
        // In the "All" tab, insert a subtle divider when category transitions
        if (catFilter == "All" && m.category != lastCategory) {
            lastCategory = m.category;
            QFrame *sepFrame = new QFrame(page);
            sepFrame->setFrameShape(QFrame::NoFrame);
            QHBoxLayout *sepHBox = new QHBoxLayout(sepFrame);
            sepHBox->setContentsMargins(4, 8, 4, 4);
            sepHBox->setSpacing(8);

            QFrame *lLine = new QFrame(sepFrame);
            lLine->setFrameShape(QFrame::HLine);
            lLine->setStyleSheet("color: rgba(100, 116, 139, 0.25);");

            QString catTitle = lastCategory;
            if (catTitle == "PDF") catTitle = tr("PDF");
            else if (catTitle == "Archive") catTitle = tr("Archives");
            else if (catTitle == "Office") catTitle = tr("Office Documents");
            else if (catTitle == "Media") catTitle = tr("Audio & Video");
            else if (catTitle == "Image") catTitle = tr("Images & Photos");

            QLabel *lblCat = new QLabel(QString("— %1 —").arg(catTitle), sepFrame);
            lblCat->setStyleSheet("font-size: 11px; font-weight: bold; color: #64748b;");

            QFrame *rLine = new QFrame(sepFrame);
            rLine->setFrameShape(QFrame::HLine);
            rLine->setStyleSheet("color: rgba(100, 116, 139, 0.25);");

            sepHBox->addWidget(lLine, 1);
            sepHBox->addWidget(lblCat, 0, Qt::AlignCenter);
            sepHBox->addWidget(rLine, 1);

            pageLay->addWidget(sepFrame);
        }

        ExtensionRowWidget *row = new ExtensionRowWidget(m, page);
        connect(row, &ExtensionRowWidget::statusChanged, this, [this]() {
            refreshAllRows();
            emit pendingStateChanged();
        });
        pageLay->addWidget(row);
    }

    pageLay->addSpacing(14);
    pageLay->addStretch();

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(page);

    return scrollArea;
}

void SettingsExtensionsTab::refreshAllRows()
{
    const auto rows = findChildren<ExtensionRowWidget*>();
    for (ExtensionRowWidget *row : rows) {
        row->refreshState();
    }
}
