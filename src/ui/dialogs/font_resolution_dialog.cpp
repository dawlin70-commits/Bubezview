/**
 * @file font_resolution_dialog.cpp
 * @brief Implementation of font resolution dialog with distro, Google Fonts, local file, and fallback options.
 */

#include "font_resolution_dialog.h"
#include "font_resolution_helper.h"
#include "package_manager_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QApplication>
#include <QCursor>

FontResolutionDialog::FontResolutionDialog(const QString &rawFontName,
                                           const QString &missingGlyphs,
                                           const QFont &currentFont,
                                           QWidget *parent)
    : FramelessDialog(parent)
    , m_rawFontName(rawFontName)
    , m_cleanFamily(FontResolutionHelper::cleanFontFamilyName(rawFontName))
    , m_missingGlyphs(missingGlyphs)
    , m_currentFont(currentFont)
{
    setWindowTitle(tr("PDF Font Resolution"));
    setMinimumWidth(520);
    setStyleSheet(QStringLiteral(
        "QLabel { color: #f8fafc; }"
        "QGroupBox { font-weight: bold; border: 1px solid #334155; border-radius: 8px; margin-top: 12px; padding-top: 14px; color: #cbd5e1; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QPushButton { background-color: #3b82f6; color: white; border: none; border-radius: 6px; padding: 8px 14px; font-weight: 600; font-size: 13px; }"
        "QPushButton:hover { background-color: #2563eb; }"
        "QPushButton:pressed { background-color: #1d4ed8; }"
        "QPushButton#BtnSecondary { background-color: #334155; color: #f8fafc; border: 1px solid #475569; }"
        "QPushButton#BtnSecondary:hover { background-color: #475569; }"
    ));

    setupUi();
    updateStatusDisplay();
}

void FontResolutionDialog::setupUi()
{
    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Header with font name and status
    m_lblFontTitle = new QLabel(this);
    m_lblFontTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
    mainLayout->addWidget(m_lblFontTitle);

    m_lblStatusBadge = new QLabel(this);
    m_lblStatusBadge->setWordWrap(true);
    mainLayout->addWidget(m_lblStatusBadge);

    // Option 1: Distro Package Manager
    QString distroPkg = FontResolutionHelper::findDistroFontPackage(m_cleanFamily);
    QString distroName = PackageManagerHelper::distroDisplayName();
    QGroupBox *boxDistro = new QGroupBox(tr("📦 System Repository (%1)").arg(distroName), this);
    QVBoxLayout *distroLayout = new QVBoxLayout(boxDistro);
    m_lblDistroDesc = new QLabel(tr("Recommended package for your distribution: <b>%1</b>").arg(distroPkg), boxDistro);
    m_lblDistroDesc->setWordWrap(true);
    m_lblDistroDesc->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px;"));
    distroLayout->addWidget(m_lblDistroDesc);

    QHBoxLayout *distroBtnLayout = new QHBoxLayout();
    m_btnDistroInstall = new QPushButton(tr("Install package '%1'").arg(distroPkg), boxDistro);
    m_btnDistroInstall->setCursor(Qt::PointingHandCursor);
    connect(m_btnDistroInstall, &QPushButton::clicked, this, &FontResolutionDialog::onInstallDistroPackage);
    distroBtnLayout->addWidget(m_btnDistroInstall);
    distroBtnLayout->addStretch();
    distroLayout->addLayout(distroBtnLayout);
    mainLayout->addWidget(boxDistro);

    // Option 2: Google Fonts Free Download
    QString freeAlt = FontResolutionHelper::closestFreeAlternative(m_cleanFamily);
    QGroupBox *boxGoogle = new QGroupBox(tr("🌐 Google Fonts (Free Open Catalog)"), this);
    QVBoxLayout *googleLayout = new QVBoxLayout(boxGoogle);
    QLabel *lblGoogleDesc = new QLabel(
        tr("Automatically downloads the original font or a faithful metric alternative (<b>%1</b>) without root privileges.").arg(freeAlt),
        boxGoogle);
    lblGoogleDesc->setWordWrap(true);
    lblGoogleDesc->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px;"));
    googleLayout->addWidget(lblGoogleDesc);

    QHBoxLayout *googleBtnLayout = new QHBoxLayout();
    m_btnGoogleDownload = new QPushButton(tr("Download and apply free font"), boxGoogle);
    m_btnGoogleDownload->setCursor(Qt::PointingHandCursor);
    connect(m_btnGoogleDownload, &QPushButton::clicked, this, &FontResolutionDialog::onDownloadGoogleFont);
    googleBtnLayout->addWidget(m_btnGoogleDownload);
    googleBtnLayout->addStretch();
    googleLayout->addLayout(googleBtnLayout);
    mainLayout->addWidget(boxGoogle);

    // Option 3 & 4: Local File and Manual Fallback
    QGroupBox *boxLocal = new QGroupBox(tr("📁 Local File or System Font"), this);
    QVBoxLayout *localLayout = new QVBoxLayout(boxLocal);

    QHBoxLayout *localBtnLayout = new QHBoxLayout();
    QPushButton *btnLoadFile = new QPushButton(tr("Load font file (.ttf / .otf)..."), boxLocal);
    btnLoadFile->setObjectName(QStringLiteral("BtnSecondary"));
    btnLoadFile->setCursor(Qt::PointingHandCursor);
    connect(btnLoadFile, &QPushButton::clicked, this, &FontResolutionDialog::onLoadLocalFontFile);
    localBtnLayout->addWidget(btnLoadFile);

    QPushButton *btnChooseOther = new QPushButton(tr("Choose alternative font..."), boxLocal);
    btnChooseOther->setObjectName(QStringLiteral("BtnSecondary"));
    btnChooseOther->setCursor(Qt::PointingHandCursor);
    connect(btnChooseOther, &QPushButton::clicked, this, &FontResolutionDialog::onChooseAlternativeFont);
    localBtnLayout->addWidget(btnChooseOther);
    localLayout->addLayout(localBtnLayout);
    mainLayout->addWidget(boxLocal);

    // Bottom close button
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setObjectName(QStringLiteral("BtnSecondary"));
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);
}

void FontResolutionDialog::updateStatusDisplay()
{
    m_lblFontTitle->setText(tr("PDF Font: %1").arg(m_cleanFamily));

    bool installed = FontResolutionHelper::isFontFamilyInstalled(m_cleanFamily);
    if (!installed) {
        m_lblStatusBadge->setStyleSheet(QStringLiteral(
            "background-color: #78350f; color: #fef3c7; border: 1px solid #b45309; border-radius: 6px; padding: 8px 12px; font-size: 13px;"));
        m_lblStatusBadge->setText(
            tr("⚠️ <b>Font not installed on the system.</b><br>The PDF engine is using a temporary fallback font. "
               "Install the package or load the font for optimal rendering."));
    } else if (!m_missingGlyphs.isEmpty()) {
        m_lblStatusBadge->setStyleSheet(QStringLiteral(
            "background-color: #831843; color: #fce7f3; border: 1px solid #be185d; border-radius: 6px; padding: 8px 12px; font-size: 13px;"));
        m_lblStatusBadge->setText(
            tr("⚠️ <b>Missing characters in font:</b> %1<br>The installed font does not include all glyphs required for the typed text.")
                .arg(m_missingGlyphs));
    } else {
        m_lblStatusBadge->setStyleSheet(QStringLiteral(
            "background-color: #064e3b; color: #d1fae5; border: 1px solid #059669; border-radius: 6px; padding: 8px 12px; font-size: 13px;"));
        m_lblStatusBadge->setText(tr("✓ <b>Font installed and fully supported.</b>"));
    }
}

void FontResolutionDialog::onInstallDistroPackage()
{
    QString pkg = FontResolutionHelper::findDistroFontPackage(m_cleanFamily);
    if (pkg.isEmpty()) {
        QMessageBox::information(this, tr("No package found"),
                                tr("No specific font package found in repositories for '%1'.").arg(m_cleanFamily));
        return;
    }

    bool success = FontResolutionHelper::installDistroFontPackage(pkg, this);
    if (success) {
        m_fontResolved = true;
        updateStatusDisplay();
        emit fontChanged(m_currentFont);
        QMessageBox::information(this, tr("Installation complete"),
                                tr("Font package '%1' was installed successfully.").arg(pkg));
        accept();
    }
}

void FontResolutionDialog::onDownloadGoogleFont()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString loadedFamily;
    QString error;

    // Try target family first, fallback to closest free equivalent
    bool ok = FontResolutionHelper::downloadGoogleFont(m_cleanFamily, &loadedFamily, &error);
    if (!ok) {
        QString freeAlt = FontResolutionHelper::closestFreeAlternative(m_cleanFamily);
        ok = FontResolutionHelper::downloadGoogleFont(freeAlt, &loadedFamily, &error);
    }
    QApplication::restoreOverrideCursor();

    if (ok && !loadedFamily.isEmpty()) {
        m_currentFont.setFamily(loadedFamily);
        m_fontResolved = true;
        emit fontChanged(m_currentFont);
        QMessageBox::information(this, tr("Download complete"),
                                tr("Font '%1' was downloaded and applied to the document.").arg(loadedFamily));
        accept();
    } else {
        QMessageBox::warning(this, tr("Download failed"),
                            error.isEmpty() ? tr("Failed to download font from Google Fonts.") : error);
    }
}

void FontResolutionDialog::onLoadLocalFontFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select Font File"),
        QString(),
        tr("Font Files (*.ttf *.otf *.woff2 *.ttc);;All Files (*.*)"));

    if (filePath.isEmpty()) return;

    QString loadedFamily;
    if (FontResolutionHelper::loadFontFromFile(filePath, &loadedFamily)) {
        m_currentFont.setFamily(loadedFamily);
        m_fontResolved = true;
        emit fontChanged(m_currentFont);
        QMessageBox::information(this, tr("Font loaded"),
                                tr("Font '%1' was loaded successfully and applied.").arg(loadedFamily));
        accept();
    } else {
        QMessageBox::critical(this, tr("Font Error"),
                             tr("Unable to load the specified font file."));
    }
}

void FontResolutionDialog::onChooseAlternativeFont()
{
    bool ok = false;
    QFont chosen = QFontDialog::getFont(&ok, m_currentFont, this, tr("Choose Alternative Font"));
    if (ok) {
        m_currentFont = chosen;
        m_fontResolved = true;
        emit fontChanged(m_currentFont);
        accept();
    }
}
