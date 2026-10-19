#include "ocr_dialog.h"
#include "ocr_language_dialog.h"
#include "ocr_manager.h"
#include "ocr_snipper_overlay.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QDir>
#include <QFont>

OcrDialog::OcrDialog(const QString &initialText,
                     const QString &sourceName,
                     const QString &sourcePath,
                     bool isPdf,
                     int currentPage,
                     int totalPages,
                     std::function<QImage(int)> pageRenderer,
                     QWidget *parent)
    : QDialog(parent)
    , m_sourceName(sourceName)
    , m_sourcePath(sourcePath)
    , m_isPdf(isPdf)
    , m_currentPage(currentPage)
    , m_totalPages(totalPages > 0 ? totalPages : 1)
    , m_pageRenderer(pageRenderer)
    , m_singlePageText(initialText)
    , m_isShowingFullDoc(false)
    , m_isCleaned(false)
{
    setWindowTitle(tr("Extracted Text (OCR Tesseract) 👁️"));
    resize(720, 540);

    setupUi();
    refreshLanguagesCombo();
    m_textEdit->setPlainText(initialText);
    updateStats();

    connect(&OcrManager::instance(), &OcrManager::languagesUpdated,
            this, &OcrDialog::refreshLanguagesCombo);
}

void OcrDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    // 1. Top Header (Document Info + Stats)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QString docTitle = m_sourceName.isEmpty() ? tr("Document OCR") : m_sourceName;
    m_lblInfo = new QLabel(QString("<b>📄 %1</b>").arg(docTitle), this);
    m_lblStats = new QLabel(this);
    m_lblStats->setStyleSheet("color: gray; font-size: 11px;");

    headerLayout->addWidget(m_lblInfo);
    headerLayout->addStretch();
    headerLayout->addWidget(m_lblStats);
    mainLayout->addLayout(headerLayout);

    // 2. Control Toolbar (Scope + Snip Area + Language Selector + Settings Gear + Re-run)
    QHBoxLayout *ctrlLayout = new QHBoxLayout();

    // PDF Scope Buttons
    if (m_isPdf && m_totalPages > 1) {
        m_btnScopePage = new QPushButton(tr("📄 Page %1").arg(m_currentPage + 1), this);
        m_btnScopePage->setCheckable(true);
        m_btnScopePage->setChecked(true);
        connect(m_btnScopePage, &QPushButton::clicked, this, &OcrDialog::handleScopePageClicked);

        m_btnScopeAll = new QPushButton(tr("📚 All %1 Pages").arg(m_totalPages), this);
        m_btnScopeAll->setCheckable(true);
        m_btnScopeAll->setChecked(false);
        connect(m_btnScopeAll, &QPushButton::clicked, this, &OcrDialog::handleScopeAllClicked);

        ctrlLayout->addWidget(m_btnScopePage);
        ctrlLayout->addWidget(m_btnScopeAll);
    } else {
        m_btnScopePage = nullptr;
        m_btnScopeAll = nullptr;
    }

    // Interactive Snippet Area OCR button
    m_btnSnipArea = new QPushButton(tr("✂️ Snip Area"), this);
    m_btnSnipArea->setToolTip(tr("Temporarily hide dialog and drag a rectangle to OCR only a selected area"));
    connect(m_btnSnipArea, &QPushButton::clicked, this, &OcrDialog::startAreaSnipping);
    ctrlLayout->addWidget(m_btnSnipArea);
    ctrlLayout->addSpacing(12);

    // Language selection
    QLabel *lblLang = new QLabel(tr("🌐 Language:"), this);
    m_comboLanguage = new QComboBox(this);
    m_comboLanguage->setMinimumWidth(160);

    m_btnSettingsGear = new QPushButton(tr("⚙️"), this);
    m_btnSettingsGear->setToolTip(tr("Manage & Download OCR Languages..."));
    m_btnSettingsGear->setFixedWidth(36);
    connect(m_btnSettingsGear, &QPushButton::clicked, this, &OcrDialog::openLanguageManager);

    m_btnRunOcr = new QPushButton(tr("⚡ Re-extract"), this);
    m_btnRunOcr->setToolTip(tr("Re-run OCR recognition with the chosen language"));
    connect(m_btnRunOcr, &QPushButton::clicked, this, &OcrDialog::runExtraction);

    ctrlLayout->addWidget(lblLang);
    ctrlLayout->addWidget(m_comboLanguage);
    ctrlLayout->addWidget(m_btnSettingsGear);
    ctrlLayout->addWidget(m_btnRunOcr);
    ctrlLayout->addStretch();
    mainLayout->addLayout(ctrlLayout);

    // Progress bar (initially hidden)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(12);
    mainLayout->addWidget(m_progressBar);

    // 3. Central Text Editor
    m_textEdit = new QPlainTextEdit(this);
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::Monospace);
    m_textEdit->setFont(font);
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &OcrDialog::updateStats);
    mainLayout->addWidget(m_textEdit, 1);

    // 4. Bottom Action Bar (Close on left, Clean + Save + Copy on right)
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);

    m_btnCleanText = new QPushButton(tr("🧹 Clean Text"), this);
    m_btnCleanText->setToolTip(tr("Filter out image tags, isolated symbols, and layout noise"));
    connect(m_btnCleanText, &QPushButton::clicked, this, &OcrDialog::cleanText);

    m_btnSave = new QPushButton(tr("💾 Save as .txt"), this);
    connect(m_btnSave, &QPushButton::clicked, this, &OcrDialog::saveToFile);

    m_btnCopy = new QPushButton(tr("📋 Copy to Clipboard"), this);
    m_btnCopy->setStyleSheet("font-weight: bold;");
    connect(m_btnCopy, &QPushButton::clicked, this, &OcrDialog::copyToClipboard);

    btnLayout->addWidget(m_btnClose);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnCleanText);
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCopy);
    mainLayout->addLayout(btnLayout);
}

void OcrDialog::refreshLanguagesCombo()
{
    QString currentCode = currentSelectedLanguageCode();
    m_comboLanguage->clear();

    const QStringList installed = OcrManager::instance().installedLanguages();

    // Offer combined ita+eng if both are present
    if (installed.contains("ita") && installed.contains("eng")) {
        m_comboLanguage->addItem(tr("Italian + English (ita+eng)"), "ita+eng");
    }

    for (const QString &code : installed) {
        OcrManager::LanguageInfo info = OcrManager::instance().languageInfo(code);
        QString label = QString("%1 (%2)").arg(info.nativeName, code);
        m_comboLanguage->addItem(label, code);
    }

    if (m_comboLanguage->count() == 0) {
        m_comboLanguage->addItem(tr("English (eng)"), "eng");
    }

    // Restore previously selected code or system default
    int idx = -1;
    if (!currentCode.isEmpty()) {
        idx = m_comboLanguage->findData(currentCode);
    }
    if (idx < 0) {
        QString sys = OcrManager::instance().detectSystemLanguageCode();
        if (installed.contains("ita") && installed.contains("eng")) {
            idx = m_comboLanguage->findData("ita+eng");
        } else {
            idx = m_comboLanguage->findData(sys);
        }
    }
    if (idx >= 0) {
        m_comboLanguage->setCurrentIndex(idx);
    }
}

QString OcrDialog::currentSelectedLanguageCode() const
{
    if (!m_comboLanguage || m_comboLanguage->currentIndex() < 0) return QString();
    return m_comboLanguage->currentData().toString();
}

void OcrDialog::openLanguageManager()
{
    OcrLanguageDialog dlg(this);
    dlg.exec();
    refreshLanguagesCombo();
}

void OcrDialog::handleScopePageClicked()
{
    m_isShowingFullDoc = false;
    if (m_btnScopePage) m_btnScopePage->setChecked(true);
    if (m_btnScopeAll) m_btnScopeAll->setChecked(false);
    m_textEdit->setPlainText(m_singlePageText);
    updateStats();
}

void OcrDialog::handleScopeAllClicked()
{
    if (m_btnScopePage) m_btnScopePage->setChecked(false);
    if (m_btnScopeAll) m_btnScopeAll->setChecked(true);
    m_isShowingFullDoc = true;

    if (m_fullDocText.isEmpty()) {
        runExtraction();
    } else {
        m_textEdit->setPlainText(m_fullDocText);
        updateStats();
    }
}

void OcrDialog::runExtraction()
{
    QString langCode = currentSelectedLanguageCode();
    if (langCode.isEmpty()) langCode = "eng";

    m_btnRunOcr->setEnabled(false);
    m_progressBar->setVisible(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (m_isPdf && m_pageRenderer) {
        if (m_isShowingFullDoc) {
            // Extract all pages sequentially
            m_progressBar->setRange(0, m_totalPages);
            m_progressBar->setValue(0);

            QStringList allPagesText;
            for (int i = 0; i < m_totalPages; ++i) {
                m_progressBar->setValue(i);
                QApplication::processEvents();

                QImage pageImg = m_pageRenderer(i);
                if (!pageImg.isNull()) {
                    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_ocr_page_XXXXXX.png");
                    tempFile.setAutoRemove(true);
                    if (tempFile.open()) {
                        pageImg.save(tempFile.fileName(), "PNG");
                        tempFile.close();

                        QString text, err;
                        if (OcrManager::instance().runOcr(tempFile.fileName(), langCode, text, err)) {
                            allPagesText.append(QString("=== %1 %2 / %3 ===\n\n%4\n")
                                .arg(tr("PAGE"), QString::number(i + 1), QString::number(m_totalPages), text));
                        }
                    }
                }
            }
            m_progressBar->setValue(m_totalPages);
            m_fullDocText = allPagesText.join("\n\n");
            m_textEdit->setPlainText(m_fullDocText);
        } else {
            // Single page
            m_progressBar->setRange(0, 0); // Indeterminate
            QImage pageImg = m_pageRenderer(m_currentPage);
            if (!pageImg.isNull()) {
                QTemporaryFile tempFile(QDir::tempPath() + "/bubez_ocr_page_XXXXXX.png");
                tempFile.setAutoRemove(true);
                if (tempFile.open()) {
                    pageImg.save(tempFile.fileName(), "PNG");
                    tempFile.close();

                    QString text, err;
                    if (OcrManager::instance().runOcr(tempFile.fileName(), langCode, text, err)) {
                        m_singlePageText = text;
                        m_textEdit->setPlainText(m_singlePageText);
                    } else {
                        QMessageBox::warning(this, tr("OCR Error"), err);
                    }
                }
            }
        }
    } else {
        // Single Image file
        m_progressBar->setRange(0, 0);
        QString text, err;
        if (OcrManager::instance().runOcr(m_sourcePath, langCode, text, err)) {
            m_singlePageText = text;
            m_textEdit->setPlainText(m_singlePageText);
        } else {
            QMessageBox::warning(this, tr("OCR Error"), err);
        }
    }

    QApplication::restoreOverrideCursor();
    m_progressBar->setVisible(false);
    m_btnRunOcr->setEnabled(true);
    updateStats();
}

void OcrDialog::updateStats()
{
    QString content = m_textEdit->toPlainText();
    int chars = content.length();
    int words = content.isEmpty() ? 0 : content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    m_lblStats->setText(tr("📊 %1 words | %2 characters").arg(words).arg(chars));
}

void OcrDialog::copyToClipboard()
{
    QApplication::clipboard()->setText(m_textEdit->toPlainText());
    m_btnCopy->setText(tr("✓ Copied!"));
    m_btnCopy->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; border-radius: 4px;");
    QTimer::singleShot(2000, this, &OcrDialog::restoreCopyButton);
}

void OcrDialog::restoreCopyButton()
{
    m_btnCopy->setText(tr("📋 Copy to Clipboard"));
    m_btnCopy->setStyleSheet("font-weight: bold;");
}

void OcrDialog::saveToFile()
{
    QString baseName = m_sourceName.isEmpty() ? "ocr_extracted" : m_sourceName;
    int dotIdx = baseName.lastIndexOf('.');
    if (dotIdx > 0) baseName = baseName.left(dotIdx);
    QString defaultPath = QDir::homePath() + "/" + baseName + "_ocr.txt";

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Extracted Text"), defaultPath, tr("Text Files (*.txt);;All Files (*)")
    );

    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_textEdit->toPlainText().toUtf8());
            file.close();
            QMessageBox::information(this, tr("File Saved"), tr("Text saved successfully to:<br><b>%1</b>").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save file:<br>%1").arg(file.errorString()));
        }
    }
}

void OcrDialog::cleanText()
{
    if (m_isCleaned) {
        m_textEdit->setPlainText(m_rawTextBeforeClean);
        m_btnCleanText->setText(tr("🧹 Clean Text"));
        m_btnCleanText->setToolTip(tr("Filter out image tags, isolated symbols, and layout noise"));
        m_isCleaned = false;
    } else {
        m_rawTextBeforeClean = m_textEdit->toPlainText();
        QString cleaned = sanitizeOcrArtifacts(m_rawTextBeforeClean);
        m_textEdit->setPlainText(cleaned);
        m_btnCleanText->setText(tr("↩️ Restore Raw"));
        m_btnCleanText->setToolTip(tr("Revert back to uncleaned raw OCR output"));
        m_isCleaned = true;
    }
    updateStats();
}

void OcrDialog::startAreaSnipping()
{
    hide();

    QWidget *target = parentWidget();
    if (!target) {
        const auto topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget *w : topLevelWidgets) {
            if (w != this && w->isVisible() && !w->windowTitle().isEmpty()) {
                target = w;
                break;
            }
        }
    }
    if (!target) target = QApplication::activeWindow();

    OcrSnipperOverlay *overlay = new OcrSnipperOverlay(target);

    // If viewing a PDF with high-res rendering available:
    if (m_isPdf && m_pageRenderer) {
        overlay->setHighDpiCropper([this](const QRectF &normRect) -> QImage {
            QImage fullPage = m_pageRenderer(m_currentPage);
            if (fullPage.isNull()) return QImage();

            int rx = qBound(0, static_cast<int>(normRect.x() * fullPage.width()), fullPage.width() - 1);
            int ry = qBound(0, static_cast<int>(normRect.y() * fullPage.height()), fullPage.height() - 1);
            int rw = qBound(1, static_cast<int>(normRect.width() * fullPage.width()), fullPage.width() - rx);
            int rh = qBound(1, static_cast<int>(normRect.height() * fullPage.height()), fullPage.height() - ry);

            return fullPage.copy(rx, ry, rw, rh);
        });
    }

    connect(overlay, &OcrSnipperOverlay::snippetCaptured, this, [this](const QImage &snippet) {
        QTemporaryFile tempFile(QDir::tempPath() + "/bubez_ocr_snippet_XXXXXX.png");
        tempFile.setAutoRemove(true);
        if (tempFile.open()) {
            snippet.save(tempFile.fileName(), "PNG");
            tempFile.close();

            QString langCode = currentSelectedLanguageCode();
            if (langCode.isEmpty()) langCode = "eng";

            QString text, err;
            if (OcrManager::instance().runOcr(tempFile.fileName(), langCode, text, err)) {
                m_singlePageText = text;
                m_textEdit->setPlainText(text);
                QApplication::clipboard()->setText(text);
                m_btnCopy->setText(tr("✓ Copied!"));
                m_btnCopy->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; border-radius: 4px;");
                QTimer::singleShot(2000, this, &OcrDialog::restoreCopyButton);
            } else {
                QMessageBox::warning(this, tr("OCR Error"), err);
            }
        }

        m_isCleaned = false;
        m_btnCleanText->setText(tr("🧹 Clean Text"));
        updateStats();

        show();
        raise();
        activateWindow();
    });

    connect(overlay, &OcrSnipperOverlay::snippingCanceled, this, [this]() {
        show();
        raise();
        activateWindow();
    });

    overlay->show();
}

QString OcrDialog::sanitizeOcrArtifacts(const QString &rawText)
{
    const QStringList lines = rawText.split('\n');
    QStringList cleanedLines;

    for (QString line : lines) {
        QString trimmed = line.trimmed();

        // 1. Remove bracketed image placeholders e.g. [image], [image1], [imagel], [figure], etc.
        if (QRegularExpression("^\\[\\s*(?:image|imagel|img|figure|photo|screenshot|fig|graphic)\\d*\\s*\\]$",
                               QRegularExpression::CaseInsensitiveOption).match(trimmed).hasMatch()) {
            continue;
        }

        // 2. Remove isolated lines containing only single or double punctuation / non-alphanumeric noise
        // (like '&', '|', '~', '---', but preserve page separators starting with '===')
        if (!trimmed.startsWith("===") && QRegularExpression("^[^\\p{L}\\p{N}]{1,3}$").match(trimmed).hasMatch()) {
            continue;
        }

        // 3. Remove spurious icon prefix noise before titles, like "Ba | Immagini:" -> "Immagini:"
        trimmed.remove(QRegularExpression("^[a-zA-Z0-9]{1,3}\\s*[|/]\\s*"));

        // 4. Remove stray leading single-letter pipe artifacts like "f | "
        trimmed.remove(QRegularExpression("^[a-zA-Z]\\s*[|/]\\s*"));

        cleanedLines.append(trimmed);
    }

    // 5. Normalize multiple blank lines (max 1 empty line between paragraphs)
    QString result;
    bool prevEmpty = false;
    for (const QString &cl : cleanedLines) {
        if (cl.isEmpty()) {
            if (!prevEmpty && !result.isEmpty()) {
                result += "\n";
                prevEmpty = true;
            }
        } else {
            if (!result.isEmpty()) result += "\n";
            result += cl;
            prevEmpty = false;
        }
    }

    return result.trimmed();
}
