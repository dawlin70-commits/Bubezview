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
        tr("<b>Modular Functional Engines & Extensions</b><br>"
           "<span style='color: #64748b; font-size: 11px;'>Instant one-click installation, package updates, integrity verification, and custom source builds.</span>"),
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
    m_catTabs->addTab(createCategoryPage("PDF"), tr("Document Engines"));
    m_catTabs->addTab(createCategoryPage("Media"), tr("Audio && Media"));
    m_catTabs->addTab(createCategoryPage("Archive"), tr("Archiving"));
    m_catTabs->addTab(createCategoryPage("Office"), tr("Office"));

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
            if (catTitle == "PDF") catTitle = tr("Document & PDF Engines");
            else if (catTitle == "Archive") catTitle = tr("Archiving & Compression Codecs");
            else if (catTitle == "Office") catTitle = tr("Office & Document Parsers");
            else if (catTitle == "Media") catTitle = tr("Audio & Multimedia Framework");

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
