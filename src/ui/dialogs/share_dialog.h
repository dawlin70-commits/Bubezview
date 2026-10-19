/**
 * @file share_dialog.h
 * @brief Modal dialog for sharing files locally via QR code and embedded HTTP server.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>

class QTcpServer;

/**
 * @class ShareDialog
 * @brief Micro HTTP server and QR code display dialog for zero-configuration local transfers.
 *
 * Spawns a temporary lightweight QTcpServer on a free local port, binds the requested
 * file for single-item HTTP download, generates a QR code pointing to the IP/port URL,
 * and presents it to the user so smartphones or other devices on the same LAN can fetch it.
 */
class ShareDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the ShareDialog.
     * @param filePath Absolute path of the file to be shared.
     * @param parent Optional parent widget.
     */
    explicit ShareDialog(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Destructor. Stops the TCP server and closes active sockets.
     */
    ~ShareDialog() override;

    /**
     * @brief Helper to share directly via LocalSend without opening the QR code dialog.
     * @param filePath Absolute path to the file.
     * @param parent Optional parent widget.
     */
    static void shareViaLocalSend(const QString &filePath, QWidget *parent = nullptr);

private slots:
    /**
     * @brief Slot triggered when the user clicks the LocalSend action button.
     */
    void onSendViaLocalSend();

    /**
     * @brief Handles incoming TCP connection on the temporary embedded HTTP server.
     */
    void onNewConnection();

private:
    /**
     * @brief Initializes UI widgets, layout, and style.
     */
    void setupUi();

    /**
     * @brief Starts the listening QTcpServer on an arbitrary available port.
     */
    void startHttpServer();

    /**
     * @brief Generates and renders the QR code image into m_qrLabel.
     */
    void generateQrCode();

    QString m_filePath;
    QTcpServer *m_tcpServer;
    QString m_serverUrl;

    QLabel *m_qrLabel;
    QLabel *m_infoLabel;
    QLabel *m_urlLabel;
    QPushButton *m_btnLocalSend;
    QPushButton *m_btnClose;
};

