/**
 * @file engine_prompt_card.cpp
 * @brief Implementation of EnginePromptCard for on-demand modular engine activation.
 */

#include "engine_prompt_card.h"
#include "core/system/package_manager_helper.h"
#include "core/services/plugin_manager.h"
#include "vector_icons.h"
#include <QFrame>
#include <QFileInfo>
#include <QMessageBox>

EnginePromptCard::EnginePromptCard(QWidget *parent)
    : QWidget(parent)
    , m_category(FileCategory::Unknown)
    , m_lblIcon(nullptr)
    , m_lblTitle(nullptr)
    , m_lblSubtitle(nullptr)
    , m_lblDetails(nullptr)
    , m_btnActivateBinary(nullptr)
    , m_btnCompileSource(nullptr)
    , m_lblStatus(nullptr)
{
    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setAlignment(Qt::AlignCenter);
    mainLay->setContentsMargins(24, 24, 24, 24);

    QFrame *card = new QFrame(this);
    card->setObjectName("engineCardFrame");
    card->setStyleSheet(
        "#engineCardFrame {"
        "  background-color: rgba(30, 41, 59, 0.85);"
        "  border: 1px solid rgba(148, 163, 184, 0.25);"
        "  border-radius: 12px;"
        "  padding: 28px;"
        "}"
    );
    card->setFixedWidth(540);

    QVBoxLayout *cardLay = new QVBoxLayout(card);
    cardLay->setSpacing(16);
    cardLay->setAlignment(Qt::AlignCenter);

    m_lblIcon = new QLabel(card);
    m_lblIcon->setAlignment(Qt::AlignCenter);
    cardLay->addWidget(m_lblIcon);

    m_lblTitle = new QLabel(card);
    m_lblTitle->setAlignment(Qt::AlignCenter);
    m_lblTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #f8fafc;");
    cardLay->addWidget(m_lblTitle);

    m_lblSubtitle = new QLabel(card);
    m_lblSubtitle->setAlignment(Qt::AlignCenter);
    m_lblSubtitle->setStyleSheet("font-size: 13px; color: #94a3b8;");
    m_lblSubtitle->setWordWrap(true);
    cardLay->addWidget(m_lblSubtitle);

    m_lblDetails = new QLabel(card);
    m_lblDetails->setAlignment(Qt::AlignLeft);
    m_lblDetails->setStyleSheet(
        "font-size: 12px; color: #cbd5e1; line-height: 1.5; padding: 12px;"
        "background-color: rgba(15, 23, 42, 0.55); border-radius: 8px;"
        "border: 1px solid rgba(148, 163, 184, 0.15);"
    );
    m_lblDetails->setWordWrap(true);
    cardLay->addWidget(m_lblDetails);

    // Actions
    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->setSpacing(12);

    m_btnActivateBinary = new QPushButton(tr("🟢 Activate (System Binary)"), card);
    m_btnActivateBinary->setCursor(Qt::PointingHandCursor);
    m_btnActivateBinary->setStyleSheet(
        "QPushButton {"
        "  background-color: #0284c7; color: white; font-weight: bold;"
        "  padding: 8px 16px; border-radius: 6px; border: none;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnActivateBinary, &QPushButton::clicked, this, &EnginePromptCard::onActivateBinaryClicked);
    btnLay->addWidget(m_btnActivateBinary);

    m_btnCompileSource = new QPushButton(tr("⚙️ Compile from Source..."), card);
    m_btnCompileSource->setCursor(Qt::PointingHandCursor);
    m_btnCompileSource->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(51, 65, 85, 0.8); color: #e2e8f0;"
        "  padding: 8px 16px; border-radius: 6px; border: 1px solid rgba(148, 163, 184, 0.3);"
        "}"
        "QPushButton:hover { background-color: rgba(71, 85, 105, 0.9); }"
    );
    connect(m_btnCompileSource, &QPushButton::clicked, this, &EnginePromptCard::compileRequested);
    btnLay->addWidget(m_btnCompileSource);

    cardLay->addLayout(btnLay);

    m_lblStatus = new QLabel(card);
    m_lblStatus->setAlignment(Qt::AlignCenter);
    m_lblStatus->setStyleSheet("color: #38bdf8; font-size: 11px;");
    m_lblStatus->hide();
    cardLay->addWidget(m_lblStatus);

    mainLay->addWidget(card);
}

void EnginePromptCard::setTargetFile(FileCategory category, const QString &filePath)
{
    m_category = category;
    m_filePath = filePath;
    QFileInfo fi(filePath);

    QString engineName;
    QString pkgHint;
    QString desc;
    QColor iconColor("#38bdf8");

    switch (category) {
    case FileCategory::Pdf:
        engineName = tr("PDF Document Engine");
        pkgHint = "poppler-qt6";
        iconColor = QColor("#ef4444");
        desc = tr("High-resolution vector PDF rendering, annotations, and bookmarks require the Poppler engine.<br><br>"
                  "You can activate it instantly using your distro's system packages, or compile the plugin from source.");
        break;
    case FileCategory::Archive:
        engineName = tr("Compressed Archive Engine");
        pkgHint = "libarchive";
        iconColor = QColor("#f59e0b");
        desc = tr("Browsing and extracting ZIP, TAR, 7Z, and ISO archives requires the LibArchive engine.<br><br>"
                  "You can activate it instantly using your distro's system packages, or compile the plugin from source.");
        break;
    case FileCategory::Media:
        engineName = tr("Media & Playback Engine");
        pkgHint = "mpv";
        iconColor = QColor("#10b981");
        desc = tr("Audio and video playback with hardware acceleration requires the MPV engine.<br><br>"
                  "You can activate it instantly using your distro's system packages.");
        break;
    default:
        engineName = tr("Plugin Engine Required");
        pkgHint = "unknown";
        desc = tr("This file requires an optional engine module that is currently inactive.");
        break;
    }

    m_lblTitle->setText(tr("%1 Inactive").arg(engineName));
    m_lblSubtitle->setText(tr("File: <b>%1</b>").arg(fi.fileName()));
    m_lblDetails->setText(desc);

    m_lblIcon->setPixmap(VectorIcons::createIcon(VectorIcons::Type::Document, iconColor).pixmap(56, 56));
    m_lblStatus->hide();
}

void EnginePromptCard::onActivateBinaryClicked()
{
    QString compId;
    if (m_category == FileCategory::Pdf) compId = "poppler-qt6";
    else if (m_category == FileCategory::Archive) compId = "libarchive";
    else if (m_category == FileCategory::Media) compId = "mpv";

    m_lblStatus->show();
    m_lblStatus->setText(tr("Checking and installing packages..."));

    bool ok = PackageManagerHelper::installComponentInteractive(compId, this);
    if (ok) {
        // Refresh plugin discovery
        PluginManager::instance().discoverPlugins();
        if (PluginManager::instance().isEngineAvailable(m_category)) {
            m_lblStatus->setText(tr("🟢 Engine activated successfully!"));
            emit engineActivated(m_filePath);
            return;
        }
    }

    m_lblStatus->setText(tr("🟡 Engine is not yet active. Check system packages or compile from source."));
}
