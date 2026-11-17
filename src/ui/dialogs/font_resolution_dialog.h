/**
 * @file font_resolution_dialog.h
 * @brief Dialog offering user interactive resolution options for missing fonts and glyphs in PDFs.
 */

#pragma once

#include "frameless_dialog.h"
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class FontResolutionDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit FontResolutionDialog(const QString &rawFontName,
                                 const QString &missingGlyphs,
                                 const QFont &currentFont,
                                 QWidget *parent = nullptr);

    QFont resolvedFont() const { return m_currentFont; }
    bool fontWasResolved() const { return m_fontResolved; }

signals:
    void fontChanged(const QFont &newFont);

private slots:
    void onInstallDistroPackage();
    void onDownloadGoogleFont();
    void onLoadLocalFontFile();
    void onChooseAlternativeFont();

private:
    void setupUi();
    void updateStatusDisplay();

    QString m_rawFontName;
    QString m_cleanFamily;
    QString m_missingGlyphs;
    QFont m_currentFont;
    bool m_fontResolved = false;

    QLabel *m_lblFontTitle = nullptr;
    QLabel *m_lblStatusBadge = nullptr;
    QLabel *m_lblDistroDesc = nullptr;
    QPushButton *m_btnDistroInstall = nullptr;
    QPushButton *m_btnGoogleDownload = nullptr;
};
