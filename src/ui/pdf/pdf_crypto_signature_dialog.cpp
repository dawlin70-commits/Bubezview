/**
 * @file pdf_crypto_signature_dialog.cpp
 * @brief Implementation of PdfCryptoSignatureDialog for PAdES and CAdES (.p7m) digital signatures.
 */

#include "pdf_crypto_signature_dialog.h"
#include "build_config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>

PdfCryptoSignatureDialog::PdfCryptoSignatureDialog(const QString &filePath,
                                                   const QList<PdfDigitalSignatureInfo> &preverifiedSigs,
                                                   int defaultTab,
                                                   QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_signatures(preverifiedSigs)
    , m_tabs(nullptr)
    , m_txtVerifyReport(nullptr)
    , m_editCertPath(nullptr)
    , m_editCertPassword(nullptr)
    , m_editSignReason(nullptr)
    , m_lblSignStatus(nullptr)
    , m_btnSignNow(nullptr)
{
    setWindowTitle(tr("Cryptographic Digital Signatures (PAdES / CAdES)"));
    resize(620, 500);

    if (m_signatures.isEmpty() && !m_filePath.isEmpty()) {
        m_signatures = verifyDocumentSignatures(m_filePath);
    }

    setupUi(defaultTab);
    populateVerificationReport();
}

void PdfCryptoSignatureDialog::setupUi(int defaultTab)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    m_tabs = new QTabWidget(this);

    // =========================================================================
    // TAB 1: Signature Verification & Certificate Details
    // =========================================================================
    QWidget *tabVerify = new QWidget();
    QVBoxLayout *verifyLay = new QVBoxLayout(tabVerify);
    verifyLay->setSpacing(10);
    verifyLay->setContentsMargins(12, 12, 12, 12);

    m_txtVerifyReport = new QTextBrowser(tabVerify);
    m_txtVerifyReport->setOpenExternalLinks(false);
    m_txtVerifyReport->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #f8fafc;"
        "  color: #1e293b;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  font-size: 12px;"
        "}"
    );
    verifyLay->addWidget(m_txtVerifyReport, 1);

    m_tabs->addTab(tabVerify, tr("🔍 Verify Signatures"));

    // =========================================================================
    // TAB 2: Digitally Sign Document (PKCS#12 / X.509)
    // =========================================================================
    QWidget *tabSign = new QWidget();
    QVBoxLayout *signLay = new QVBoxLayout(tabSign);
    signLay->setSpacing(12);
    signLay->setContentsMargins(12, 12, 12, 12);

    QLabel *lblSignIntro = new QLabel(
        tr("Apply a cryptographic digital signature (PAdES SHA-256 or CAdES <code>.pdf.p7m</code>) "
           "using a personal X.509 certificate (<code>.p12</code> / <code>.pfx</code>)."),
        tabSign
    );
    lblSignIntro->setWordWrap(true);
    lblSignIntro->setStyleSheet("color: #475569; font-size: 11px;");
    signLay->addWidget(lblSignIntro);

    QGroupBox *grpCert = new QGroupBox(tr("Digital Certificate"), tabSign);
    QFormLayout *formLay = new QFormLayout(grpCert);
    formLay->setSpacing(10);

    QHBoxLayout *certRow = new QHBoxLayout();
    m_editCertPath = new QLineEdit(grpCert);
    m_editCertPath->setPlaceholderText(tr("Select .p12 or .pfx certificate file..."));
    certRow->addWidget(m_editCertPath, 1);

    QPushButton *btnBrowse = new QPushButton(tr("Browse..."), grpCert);
    btnBrowse->setCursor(Qt::PointingHandCursor);
    connect(btnBrowse, &QPushButton::clicked, this, &PdfCryptoSignatureDialog::onBrowseCertClicked);
    certRow->addWidget(btnBrowse);

    QPushButton *btnGenSelf = new QPushButton(tr("✨ Create Personal Certificate"), grpCert);
    btnGenSelf->setCursor(Qt::PointingHandCursor);
    btnGenSelf->setToolTip(tr("Generate a new self-signed personal cryptographic certificate (.p12) using OpenSSL"));
    connect(btnGenSelf, &QPushButton::clicked, this, &PdfCryptoSignatureDialog::onGenerateSelfSignedClicked);
    certRow->addWidget(btnGenSelf);

    formLay->addRow(tr("Certificate (.p12/.pfx):"), certRow);

    m_editCertPassword = new QLineEdit(grpCert);
    m_editCertPassword->setEchoMode(QLineEdit::Password);
    m_editCertPassword->setPlaceholderText(tr("Certificate password / PIN"));
    formLay->addRow(tr("Password / PIN:"), m_editCertPassword);

    m_editSignReason = new QLineEdit(grpCert);
    m_editSignReason->setPlaceholderText(tr("Document approval and authenticity"));
    m_editSignReason->setText(tr("Digitally approved"));
    formLay->addRow(tr("Signing Reason:"), m_editSignReason);

    signLay->addWidget(grpCert);

    // Check if default personal cert exists
    QString defaultCert = QDir::homePath() + "/.local/share/bubezview/certificates/personal_cert.p12";
    if (QFile::exists(defaultCert)) {
        m_editCertPath->setText(defaultCert);
    }

    m_lblSignStatus = new QLabel(tabSign);
    m_lblSignStatus->setWordWrap(true);
    m_lblSignStatus->setStyleSheet("color: #0284c7; font-size: 11px;");
    signLay->addWidget(m_lblSignStatus);

    signLay->addStretch(1);

    QHBoxLayout *signActionRow = new QHBoxLayout();
    signActionRow->addStretch();
    m_btnSignNow = new QPushButton(tr("✒️ Sign & Save Document..."), tabSign);
    m_btnSignNow->setCursor(Qt::PointingHandCursor);
    m_btnSignNow->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 7px 18px; border-radius: 5px; font-size: 12px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnSignNow, &QPushButton::clicked, this, &PdfCryptoSignatureDialog::onSignDocumentClicked);
    signActionRow->addWidget(m_btnSignNow);
    signLay->addLayout(signActionRow);

    m_tabs->addTab(tabSign, tr("✒️ Sign Document"));
    m_tabs->setCurrentIndex(defaultTab);

    mainLayout->addWidget(m_tabs, 1);

    QHBoxLayout *bottomLay = new QHBoxLayout();
    bottomLay->addStretch();
    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setCursor(Qt::PointingHandCursor);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLay->addWidget(btnClose);
    mainLayout->addLayout(bottomLay);
}

void PdfCryptoSignatureDialog::populateVerificationReport()
{
    if (!m_txtVerifyReport) return;

    if (m_signatures.isEmpty()) {
        m_txtVerifyReport->setHtml(
            QString("<div style='text-align: center; padding: 30px; color: #64748b;'>"
                    "<h3>%1</h3>"
                    "<p>%2</p>"
                    "</div>")
                .arg(tr("No Digital Signatures Detected"),
                     tr("This PDF document does not contain any PAdES or CAdES cryptographic signatures.<br>"
                        "You can digitally sign it using the <b>'Sign Document'</b> tab."))
        );
        return;
    }

    QString html;
    html += QString("<h3 style='margin-top:0; color:#0f172a;'>%1 (%2)</h3>")
                .arg(tr("Detected Digital Signatures"))
                .arg(m_signatures.size());

    for (const PdfDigitalSignatureInfo &sig : m_signatures) {
        QString badgeColor = sig.isSignatureValid ? "#059669" : "#dc2626";
        QString badgeBg    = sig.isSignatureValid ? "#ecfdf5" : "#fef2f2";
        QString badgeText  = sig.isSignatureValid ? tr("✓ VALID CRYPTOGRAPHIC SIGNATURE (Document Integrity Verified)")
                                                  : tr("⚠ INVALID SIGNATURE OR MODIFIED DOCUMENT");

        html += QString(
            "<div style='border: 1px solid %1; background-color: %2; border-radius: 6px; padding: 10px; margin-bottom: 12px;'>"
            "<div style='color: %1; font-weight: bold; font-size: 13px; margin-bottom: 6px;'>%3</div>"
            "<table cellspacing='4' cellpadding='0' style='font-size: 12px; color: #1e293b;'>"
            "<tr><td><b>%4:</b></td><td>%5</td></tr>"
            "<tr><td><b>%6:</b></td><td>%7</td></tr>"
            "<tr><td><b>%8:</b></td><td>%9</td></tr>"
            "<tr><td><b>%10:</b></td><td><code>%11</code> (%12)</td></tr>"
            "</table>"
            "</div>"
        ).arg(
            badgeColor,
            badgeBg,
            badgeText,
            tr("Signer"), sig.signerName.isEmpty() ? tr("Not specified") : sig.signerName.toHtmlEscaped(),
            tr("Certificate Authority / DN"), sig.signerFullDn.isEmpty() ? sig.issuerDn.toHtmlEscaped() : sig.signerFullDn.toHtmlEscaped(),
            tr("Signing Time"), sig.signingTime.isEmpty() ? tr("Not available") : sig.signingTime.toHtmlEscaped(),
            tr("Algorithm / Format"), sig.hashAlgorithm.isEmpty() ? "SHA-256" : sig.hashAlgorithm,
            sig.signatureType.isEmpty() ? "PAdES" : sig.signatureType
        );

        if (!sig.rawReport.isEmpty()) {
            html += QString("<details><summary style='cursor:pointer; color:#475569; font-size:11px;'>%1</summary>"
                            "<pre style='background:#0f172a; color:#e2e8f0; padding:8px; border-radius:4px; font-size:10px;'>%2</pre></details><br>")
                        .arg(tr("Show full technical verification output (pdfsig / OpenSSL)"), sig.rawReport.toHtmlEscaped());
        }
    }

    m_txtVerifyReport->setHtml(html);
}

QList<PdfDigitalSignatureInfo> PdfCryptoSignatureDialog::verifyDocumentSignatures(const QString &filePath)
{
    QList<PdfDigitalSignatureInfo> results;
    if (filePath.isEmpty() || !QFile::exists(filePath)) return results;

    // 1. Check if it is a .p7m (CAdES) file
    if (filePath.endsWith(".p7m", Qt::CaseInsensitive)) {
        QProcess proc;
        // Inspect PKCS#7 / CMS certificate info using openssl pkcs7
        proc.start("openssl", {"pkcs7", "-inform", "DER", "-in", filePath, "-print_certs", "-noout", "-text"});
        proc.waitForFinished(4000);
        QString certText = QString::fromUtf8(proc.readAllStandardOutput() + proc.readAllStandardError());

        // Verify CMS integrity
        QProcess verifyProc;
        QTemporaryFile tmpOut;
        if (tmpOut.open()) {
            QString tmpPath = tmpOut.fileName();
            tmpOut.close();
            verifyProc.start("openssl", {"cms", "-verify", "-inform", "DER", "-in", filePath, "-noverify", "-out", tmpPath});
            verifyProc.waitForFinished(5000);
        }
        bool cmsValid = (verifyProc.exitCode() == 0);
        QString verifyErr = QString::fromUtf8(verifyProc.readAllStandardError()).trimmed();

        PdfDigitalSignatureInfo info;
        info.index = 1;
        info.signatureType = "CAdES (.p7m)";
        info.hashAlgorithm = "SHA-256";
        info.isSignatureValid = cmsValid;
        info.rawReport = verifyErr + "\n\n" + certText;

        // Parse Subject CN
        QRegularExpression reSubject("Subject:.*CN\\s*=\\s*([^,\\n/]+)");
        auto matchSub = reSubject.match(certText);
        if (matchSub.hasMatch()) {
            info.signerName = matchSub.captured(1).trimmed();
        } else {
            info.signerName = tr("CAdES Signed Document (.p7m)");
        }

        QRegularExpression reIssuer("Issuer:.*CN\\s*=\\s*([^,\\n/]+)");
        auto matchIss = reIssuer.match(certText);
        if (matchIss.hasMatch()) {
            info.issuerDn = matchIss.captured(1).trimmed();
            info.signerFullDn = info.issuerDn;
            info.isCertTrusted = (info.issuerDn != info.signerName);
        }

        results.append(info);
        return results;
    }

    // 2. Standard PDF: check with pdfsig
    QString pdfsigExe = QStandardPaths::findExecutable("pdfsig");
    if (pdfsigExe.isEmpty()) return results;

    QProcess proc;
    proc.start(pdfsigExe, {"-nocert", filePath});
    proc.waitForFinished(5000);

    QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty() || output.contains("does not contain any signatures", Qt::CaseInsensitive)) {
        return results;
    }

    // Split by "Signature #" blocks
    const QStringList lines = output.split('\n');
    PdfDigitalSignatureInfo currentSig;
    bool inSig = false;

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("Signature #")) {
            if (inSig) {
                results.append(currentSig);
            }
            currentSig = PdfDigitalSignatureInfo();
            currentSig.rawReport = output;
            inSig = true;
        } else if (inSig) {
            if (trimmed.startsWith("Signer Certificate Common Name:")) {
                currentSig.signerName = trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("Signer full Distinguished Name:")) {
                currentSig.signerFullDn = trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("Signing Time:")) {
                currentSig.signingTime = trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("Signing Hash Algorithm:")) {
                currentSig.hashAlgorithm = trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("Signature Type:")) {
                currentSig.signatureType = trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("Signature Validation:")) {
                QString val = trimmed.section(':', 1).trimmed();
                currentSig.isSignatureValid = val.contains("Signature is Valid", Qt::CaseInsensitive);
            } else if (trimmed.startsWith("Certificate Validation:")) {
                QString val = trimmed.section(':', 1).trimmed();
                currentSig.isCertTrusted = val.contains("Certificate is Trusted", Qt::CaseInsensitive);
            }
        }
    }
    if (inSig) {
        results.append(currentSig);
    }

    return results;
}

bool PdfCryptoSignatureDialog::extractPdfFromP7m(const QString &p7mPath, const QString &outputPdfPath, QString *verifyOut)
{
    QString opensslExe = QStandardPaths::findExecutable("openssl");
    if (opensslExe.isEmpty()) return false;

    // 1. Try standard DER CMS verification/extraction without CA chain enforcement (-noverify checks digest integrity)
    {
        QProcess proc;
        proc.start(opensslExe, {"cms", "-verify", "-inform", "DER", "-in", p7mPath, "-noverify", "-out", outputPdfPath});
        proc.waitForFinished(6000);
        if (verifyOut) *verifyOut = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (proc.exitCode() == 0 && QFileInfo(outputPdfPath).size() > 10) {
            return true;
        }
    }

    // 2. Fallback: Try PEM or SMIME format if the .p7m is base64 encoded
    {
        QProcess proc;
        proc.start(opensslExe, {"smime", "-verify", "-in", p7mPath, "-noverify", "-out", outputPdfPath});
        proc.waitForFinished(6000);
        if (proc.exitCode() == 0 && QFileInfo(outputPdfPath).size() > 10) {
            return true;
        }
    }

    return false;
}

void PdfCryptoSignatureDialog::onBrowseCertClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Digital Certificate (.p12 / .pfx)"),
        QDir::homePath(),
        tr("PKCS#12 Certificates (*.p12 *.pfx);;All Files (*)")
    );
    if (!path.isEmpty()) {
        m_editCertPath->setText(path);
    }
}

void PdfCryptoSignatureDialog::onGenerateSelfSignedClicked()
{
    bool ok = false;
    QString commonName = QInputDialog::getText(
        this,
        tr("Create Personal Certificate"),
        tr("Enter your Full Name (or Organization) for the certificate:"),
        QLineEdit::Normal,
        qEnvironmentVariable("USER", "BubezView User"),
        &ok
    );
    if (!ok || commonName.trimmed().isEmpty()) return;

    QString password = QInputDialog::getText(
        this,
        tr("Certificate Password"),
        tr("Choose a password to protect your .p12 certificate:"),
        QLineEdit::Password,
        QString(),
        &ok
    );
    if (!ok) return;

    QString certDir = QDir::homePath() + "/.local/share/bubezview/certificates";
    QDir().mkpath(certDir);

    QString keyPath  = certDir + "/personal_key.pem";
    QString crtPath  = certDir + "/personal_cert.pem";
    QString p12Path  = certDir + "/personal_cert.p12";

    // 1. Generate RSA 2048 private key and self-signed X.509 certificate valid for 10 years
    QString cleanCn = commonName.trimmed();
    cleanCn.replace(QLatin1Char('/'), QLatin1Char('-'));
    cleanCn.replace(QLatin1Char('='), QLatin1Char('-'));
    QString subj = QString("/CN=%1/O=BubezView Personal Digital ID").arg(cleanCn);
    QProcess reqProc;
    reqProc.start("openssl", {
        "req", "-x509", "-newkey", "rsa:2048",
        "-keyout", keyPath,
        "-out", crtPath,
        "-days", "3650",
        "-nodes",
        "-subj", subj
    });
    reqProc.waitForFinished(8000);

    if (reqProc.exitCode() != 0) {
        QMessageBox::warning(this, tr("OpenSSL Error"),
            tr("Failed to generate X.509 private key:\n%1").arg(QString::fromUtf8(reqProc.readAllStandardError())));
        return;
    }

    // 2. Export to PKCS#12 (.p12) with friendly name "bubez_signer"
    QProcess p12Proc;
    p12Proc.start("openssl", {
        "pkcs12", "-export",
        "-out", p12Path,
        "-inkey", keyPath,
        "-in", crtPath,
        "-name", "bubez_signer",
        "-passout", "pass:" + password
    });
    p12Proc.waitForFinished(6000);

    QFile::remove(keyPath);
    QFile::remove(crtPath);

    if (p12Proc.exitCode() != 0 || !QFile::exists(p12Path)) {
        QMessageBox::warning(this, tr("PKCS#12 Export Error"),
            tr("Failed to create .p12 certificate file:\n%1").arg(QString::fromUtf8(p12Proc.readAllStandardError())));
        return;
    }

    m_editCertPath->setText(p12Path);
    m_editCertPassword->setText(password);
    m_lblSignStatus->setText(tr("✓ Personal certificate created and selected: <code>%1</code>").arg(p12Path.toHtmlEscaped()));
    QMessageBox::information(this, tr("Certificate Created"),
        tr("Your personal digital certificate (<b>%1</b>) has been generated and is ready for signing!").arg(commonName.toHtmlEscaped()));
}

void PdfCryptoSignatureDialog::onSignDocumentClicked()
{
    QString certPath = m_editCertPath->text().trimmed();
    if (certPath.isEmpty() || !QFile::exists(certPath)) {
        QMessageBox::warning(this, tr("Missing Certificate"),
            tr("Please select a valid certificate file (<code>.p12</code> / <code>.pfx</code>) or click <b>'Create Personal Certificate'</b>."));
        return;
    }

    QFileInfo srcInfo(m_filePath);
    QString baseName = srcInfo.completeBaseName();
    if (baseName.endsWith(".pdf", Qt::CaseInsensitive)) {
        baseName.chop(4);
    }
    QString suggestedOut = srcInfo.absoluteDir().filePath(baseName + "_signed.pdf");

    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Signed Document"),
        suggestedOut,
        tr("Signed PDF Document (*.pdf);;CAdES Cryptographic Envelope (*.pdf.p7m)")
    );
    if (savePath.isEmpty()) return;

    const bool overwriteSource = (QFileInfo(savePath).absoluteFilePath() == srcInfo.absoluteFilePath());
    const QString targetWritePath = overwriteSource ? (savePath + QStringLiteral(".bubez_sign_tmp")) : savePath;
    if (overwriteSource) {
        QFile::remove(targetWritePath);
    }

    QString password = m_editCertPassword->text();
    QString reason   = m_editSignReason->text().trimmed();

    // If user chose .p7m (CAdES), sign directly via OpenSSL CMS (works everywhere without NSS tools)
    if (savePath.endsWith(".p7m", Qt::CaseInsensitive)) {
        QTemporaryDir tmpDir;
        if (!tmpDir.isValid()) return;
        QString pemCert = tmpDir.filePath("signer_cert.pem");
        QString pemKey  = tmpDir.filePath("signer_key.pem");

        // Extract cert and key from .p12
        QProcess extCert;
        extCert.start("openssl", {"pkcs12", "-in", certPath, "-clcerts", "-nokeys", "-out", pemCert, "-passin", "pass:" + password});
        extCert.waitForFinished(5000);

        QProcess extKey;
        extKey.start("openssl", {"pkcs12", "-in", certPath, "-nocerts", "-nodes", "-out", pemKey, "-passin", "pass:" + password});
        extKey.waitForFinished(5000);

        if (extCert.exitCode() != 0 || extKey.exitCode() != 0) {
            QMessageBox::warning(this, tr("Invalid Password or Certificate"),
                tr("Failed to unlock the .p12 certificate. Please verify that the password is correct."));
            return;
        }

        QProcess cmsSign;
        cmsSign.start("openssl", {
            "cms", "-sign", "-binary", "-nodetach",
            "-in", m_filePath,
            "-signer", pemCert,
            "-inkey", pemKey,
            "-outform", "DER",
            "-out", targetWritePath,
            "-md", "sha256"
        });
        cmsSign.waitForFinished(10000);

        if (cmsSign.exitCode() == 0 && QFile::exists(targetWritePath)) {
            if (overwriteSource) {
                QFile::remove(savePath);
                QFile::rename(targetWritePath, savePath);
            }
            m_signedOutputPath = savePath;
            QMessageBox::information(this, tr("Signature Completed"),
                tr("Document digitally signed in CAdES (<code>.p7m</code>) format successfully!"));
            accept();
            return;
        } else {
            if (overwriteSource) QFile::remove(targetWritePath);
            QMessageBox::warning(this, tr("CAdES Signing Error"),
                tr("Error during OpenSSL signing:\n%1").arg(QString::fromUtf8(cmsSign.readAllStandardError())));
            return;
        }
    }

    // Standard PDF PAdES signature via pdfsig + temporary NSS db (or fallback if NSS tools unavailable)
    QString pdfsigExe   = QStandardPaths::findExecutable("pdfsig");
    QString certutilExe = QStandardPaths::findExecutable("certutil");
    QString pk12utilExe = QStandardPaths::findExecutable("pk12util");

    // If source is a .p7m file and user wants PAdES .pdf output, extract inner PDF first
    QString pdfSourceForPades = m_filePath;
    QTemporaryFile tempExtractedPdf(QDir::tempPath() + "/bubez_p7m_src_XXXXXX.pdf");
    if (m_filePath.endsWith(".p7m", Qt::CaseInsensitive)) {
        if (!tempExtractedPdf.open()) {
            QMessageBox::warning(this, tr("Extraction Error"),
                tr("Unable to create a temporary file to extract the PDF from the .p7m envelope."));
            return;
        }
        pdfSourceForPades = tempExtractedPdf.fileName();
        tempExtractedPdf.close();
        if (!extractPdfFromP7m(m_filePath, pdfSourceForPades)) {
            QMessageBox::warning(this, tr("Extraction Error"),
                tr("Failed to extract a valid PDF document from the .p7m cryptographic envelope."));
            return;
        }
    }

    if (!pdfsigExe.isEmpty() && !certutilExe.isEmpty() && !pk12utilExe.isEmpty()) {
        QTemporaryDir nssDir;
        if (nssDir.isValid()) {
            // 1. Initialize temporary NSS DB with empty password
            QProcess initDb;
            initDb.start(certutilExe, {"-N", "-d", "sql:" + nssDir.path(), "--empty-password"});
            initDb.waitForFinished(4000);

            // 2. Import .p12 into temporary NSS DB
            QProcess impP12;
            impP12.start(pk12utilExe, {"-i", certPath, "-d", "sql:" + nssDir.path(), "-W", password, "-K", ""});
            impP12.waitForFinished(5000);

            // 3. List nickname inside NSS DB
            QProcess listNick;
            listNick.start(pdfsigExe, {"-nssdir", "sql:" + nssDir.path(), "-list-nicks"});
            listNick.waitForFinished(4000);
            QString nickOut = QString::fromUtf8(listNick.readAllStandardOutput() + listNick.readAllStandardError());

            QString nickname = "bubez_signer";
            for (const QString &line : nickOut.split('\n')) {
                if (line.startsWith(" * ")) {
                    nickname = line.mid(3).trimmed();
                    break;
                }
            }

            // 4. Sign PDF with pdfsig -add-signature -etsi
            QStringList signArgs = {
                "-nssdir", "sql:" + nssDir.path(),
                "-add-signature",
                "-etsi",
                "-nick", nickname,
                "-digest", "SHA256"
            };
            if (!reason.isEmpty()) {
                signArgs << "-reason" << reason;
            }
            signArgs << pdfSourceForPades << targetWritePath;

            QProcess signProc;
            signProc.start(pdfsigExe, signArgs);
            signProc.waitForFinished(10000);

            if (signProc.exitCode() == 0 && QFile::exists(targetWritePath)) {
                if (overwriteSource) {
                    QFile::remove(savePath);
                    QFile::rename(targetWritePath, savePath);
                }
                m_signedOutputPath = savePath;
                QMessageBox::information(this, tr("PDF Document Signed"),
                    tr("The PAdES digital signature (ETSI.CAdES.detached SHA-256) was applied successfully to:\n<b>%1</b>").arg(savePath.toHtmlEscaped()));
                accept();
                return;
            } else if (overwriteSource) {
                QFile::remove(targetWritePath);
            }
        }
    }

    // Fallback if libnss3-tools (certutil/pk12util) is not installed inside container: sign as .p7m
    QString p7mSave = savePath.endsWith(".p7m", Qt::CaseInsensitive) ? savePath : (savePath + ".p7m");
    const bool overwriteP7mSource = (QFileInfo(p7mSave).absoluteFilePath() == srcInfo.absoluteFilePath());
    const QString p7mWritePath = overwriteP7mSource ? (p7mSave + QStringLiteral(".bubez_sign_tmp")) : p7mSave;
    if (overwriteP7mSource) {
        QFile::remove(p7mWritePath);
    }

    QTemporaryDir tmpDir;
    QString pemCert = tmpDir.filePath("signer_cert.pem");
    QString pemKey  = tmpDir.filePath("signer_key.pem");

    QProcess extCert;
    extCert.start("openssl", {"pkcs12", "-in", certPath, "-clcerts", "-nokeys", "-out", pemCert, "-passin", "pass:" + password});
    extCert.waitForFinished(5000);

    QProcess extKey;
    extKey.start("openssl", {"pkcs12", "-in", certPath, "-nocerts", "-nodes", "-out", pemKey, "-passin", "pass:" + password});
    extKey.waitForFinished(5000);

    if (extCert.exitCode() != 0 || extKey.exitCode() != 0) {
        QMessageBox::warning(this, tr("Invalid Password"),
            tr("Unable to open the .p12 certificate. Please check your password."));
        return;
    }

    QProcess cmsSign;
    cmsSign.start("openssl", {
        "cms", "-sign", "-binary", "-nodetach",
        "-in", m_filePath,
        "-signer", pemCert,
        "-inkey", pemKey,
        "-outform", "DER",
        "-out", p7mWritePath,
        "-md", "sha256"
    });
    cmsSign.waitForFinished(10000);

    if (cmsSign.exitCode() == 0 && QFile::exists(p7mWritePath)) {
        if (overwriteP7mSource) {
            QFile::remove(p7mSave);
            QFile::rename(p7mWritePath, p7mSave);
        }
        m_signedOutputPath = p7mSave;
        QMessageBox::information(this, tr("Digital Signature Completed"),
            tr("Document cryptographically signed with OpenSSL at:\n<b>%1</b>").arg(p7mSave.toHtmlEscaped()));
        accept();
    } else {
        if (overwriteP7mSource) QFile::remove(p7mWritePath);
        QMessageBox::warning(this, tr("Signing Error"),
            tr("Could not complete the digital signature operation."));
    }
}
