/**
 * @file pdf_crypto_signature_dialog.h
 * @brief Cryptographic digital signature verification and signing dialog for PDF and .p7m documents.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QList>

class QTabWidget;
class QTextBrowser;
class QLineEdit;
class QPushButton;
class QLabel;

/**
 * @struct PdfDigitalSignatureInfo
 * @brief Holds parsed details of a digital signature embedded in a PDF or .p7m file.
 */
struct PdfDigitalSignatureInfo {
    int index = 1;                  ///< 1-based signature number
    QString signerName;             ///< Common Name (CN) of signer
    QString signerFullDn;           ///< Full Distinguished Name
    QString issuerDn;               ///< Certificate Authority (Issuer)
    QString signingTime;            ///< Timestamp of signature
    QString hashAlgorithm;          ///< e.g. SHA-256
    QString signatureType;          ///< e.g. adbe.pkcs7.detached / ETSI.CAdES.detached / P7M
    bool isSignatureValid = false;  ///< Cryptographic digest integrity valid
    bool isCertTrusted = false;     ///< Certificate chain validation status
    bool coversWholeDocument = true;///< True if signature covers the entire PDF
    QString rawReport;              ///< Full raw verification report
};

/**
 * @class PdfCryptoSignatureDialog
 * @brief Provides UI for inspecting existing PAdES/CAdES digital signatures,
 *        signing PDFs with a PKCS#12 (.p12/.pfx) certificate, and generating self-signed certificates.
 */
class PdfCryptoSignatureDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the cryptographic signature dialog.
     * @param filePath Path to the active PDF or .p7m document.
     * @param preverifiedSigs Optional list of already parsed signatures.
     * @param defaultTab 0 for Verification/Details tab, 1 for Sign Document tab.
     * @param parent Parent widget.
     */
    explicit PdfCryptoSignatureDialog(const QString &filePath,
                                      const QList<PdfDigitalSignatureInfo> &preverifiedSigs = {},
                                      int defaultTab = 0,
                                      QWidget *parent = nullptr);
    ~PdfCryptoSignatureDialog() override = default;

    /**
     * @brief Inspects a PDF or .p7m file for digital signatures using pdfsig and openssl.
     * @param filePath Path to PDF or .p7m file.
     * @return List of detected digital signatures.
     */
    static QList<PdfDigitalSignatureInfo> verifyDocumentSignatures(const QString &filePath);

    /**
     * @brief Extracts the inner PDF from a CAdES .pdf.p7m envelope using openssl.
     * @param p7mPath Source .p7m file path.
     * @param outputPdfPath Target temporary PDF file path.
     * @param verifyOut Optional output string capturing OpenSSL verification details.
     * @return True if extraction succeeded and output is a valid PDF.
     */
    static bool extractPdfFromP7m(const QString &p7mPath, const QString &outputPdfPath, QString *verifyOut = nullptr);

    /**
     * @brief Returns the path of the newly signed PDF if signing succeeded.
     */
    QString signedOutputPath() const { return m_signedOutputPath; }

private slots:
    void onBrowseCertClicked();
    void onGenerateSelfSignedClicked();
    void onSignDocumentClicked();

private:
    void setupUi(int defaultTab);
    void populateVerificationReport();

    QString m_filePath;
    QList<PdfDigitalSignatureInfo> m_signatures;
    QString m_signedOutputPath;

    QTabWidget *m_tabs;
    QTextBrowser *m_txtVerifyReport;
    QLineEdit *m_editCertPath;
    QLineEdit *m_editCertPassword;
    QLineEdit *m_editSignReason;
    QLabel *m_lblSignStatus;
    QPushButton *m_btnSignNow;
};
