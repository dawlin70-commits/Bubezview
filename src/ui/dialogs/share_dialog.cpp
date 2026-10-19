#include "share_dialog.h"
#include "share_manager.h"

#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QUrl>
#include <qrencode.h>

ShareDialog::ShareDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_tcpServer(nullptr)
    , m_qrLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_urlLabel(nullptr)
    , m_btnLocalSend(nullptr)
    , m_btnClose(nullptr)
{
    setWindowTitle(tr("Share File — BubezView"));
    resize(400, 520);
    startHttpServer();
    setupUi();
    generateQrCode();
}

ShareDialog::~ShareDialog()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
}

void ShareDialog::startHttpServer()
{
    if (m_filePath.isEmpty() || !QFile::exists(m_filePath)) return;

    // Detect LAN IP (prefer 192.168.x.x, 10.x.x.x, 172.16-31.x.x)
    QString localIp = "127.0.0.1";
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            QString ipStr = addr.toString();
            if (ipStr.startsWith("192.168.") || ipStr.startsWith("10.") || ipStr.startsWith("172.")) {
                localIp = ipStr;
                break;
            } else if (localIp == "127.0.0.1") {
                localIp = ipStr;
            }
        }
    }

    m_tcpServer = new QTcpServer(this);
    quint16 port = 8080;
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
        // Fallback to any random open port
        if (!m_tcpServer->listen(QHostAddress::AnyIPv4, 0)) {
            m_serverUrl = "file://" + QFileInfo(m_filePath).absoluteFilePath();
            return;
        }
    }
    port = m_tcpServer->serverPort();

    connect(m_tcpServer, &QTcpServer::newConnection, this, &ShareDialog::onNewConnection);

    QFileInfo fi(m_filePath);
    m_serverUrl = QString("http://%1:%2/%3").arg(localIp).arg(port).arg(QString::fromUtf8(QUrl::toPercentEncoding(fi.fileName())));
}

void ShareDialog::onNewConnection()
{
    if (!m_tcpServer) return;

    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray req = socket->readAll();
            if (req.startsWith("GET")) {
                QFile file(m_filePath);
                if (file.open(QIODevice::ReadOnly)) {
                    QByteArray fileData = file.readAll();
                    QFileInfo fi(m_filePath);
                    QByteArray header;
                    header.append("HTTP/1.1 200 OK\r\n");
                    header.append("Content-Type: application/octet-stream\r\n");
                    header.append("Content-Disposition: attachment; filename=\"" + fi.fileName().toUtf8() + "\"\r\n");
                    header.append("Content-Length: " + QByteArray::number(fileData.size()) + "\r\n");
                    header.append("Connection: close\r\n\r\n");
                    socket->write(header);
                    socket->write(fileData);
                } else {
                    socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile not found");
                }
            }
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void ShareDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(14);

    QFileInfo fi(m_filePath);

    QLabel *titleLabel = new QLabel(QString(tr("📲 Share: %1")).arg(fi.fileName()), this);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; padding-bottom: 2px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);

    m_qrLabel = new QLabel(this);
    m_qrLabel->setFixedSize(260, 260);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setStyleSheet("background-color: #ffffff; border-radius: 12px; border: 2px solid rgba(128, 128, 128, 0.4); padding: 10px;");
    mainLayout->addWidget(m_qrLabel, 0, Qt::AlignCenter);

    m_infoLabel = new QLabel(tr("Scan with your phone (make sure you are on the same Wi-Fi network):"), this);
    m_infoLabel->setStyleSheet("font-size: 12px;");
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_infoLabel);

    m_urlLabel = new QLabel(m_serverUrl, this);
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_urlLabel->setStyleSheet("color: #0369a1; background-color: #f0f9ff; border: 1px solid #7dd3fc; border-radius: 6px; padding: 6px 10px; font-size: 12px; font-weight: bold; font-family: monospace;");
    m_urlLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_urlLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    m_btnLocalSend = new QPushButton(tr("🚀 Send with LocalSend"), this);
    m_btnLocalSend->setCursor(Qt::PointingHandCursor);
    m_btnLocalSend->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 16px;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(m_btnLocalSend, &QPushButton::clicked, this, &ShareDialog::onSendViaLocalSend);
    btnLayout->addWidget(m_btnLocalSend);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet(
        "QPushButton {"
        "   background-color: #334155;"
        "   color: #f1f5f9;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 16px;"
        "}"
        "QPushButton:hover { background-color: #475569; }"
    );
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnClose);

    mainLayout->addLayout(btnLayout);
}

void ShareDialog::generateQrCode()
{
    if (m_serverUrl.isEmpty()) {
        if (!m_filePath.isEmpty()) {
            m_serverUrl = "file://" + QFileInfo(m_filePath).absoluteFilePath();
        } else {
            m_qrLabel->setText(tr("No path to share."));
            return;
        }
    }

    QRcode *qr = QRcode_encodeString(m_serverUrl.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr) {
        m_qrLabel->setText(tr("Unable to generate QR Code."));
        return;
    }

    int qrWidth = qr->width > 0 ? qr->width : 1;
    int scale = 240 / qrWidth;
    if (scale < 1) scale = 1;
    int imgSize = qrWidth * scale;

    QImage image(imgSize, imgSize, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < qrWidth; ++y) {
        for (int x = 0; x < qrWidth; ++x) {
            unsigned char b = qr->data[y * qrWidth + x];
            if (b & 0x01) {
                painter.drawRect(x * scale, y * scale, scale, scale);
            }
        }
    }
    painter.end();
    QRcode_free(qr);

    m_qrLabel->setPixmap(QPixmap::fromImage(image).scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ShareDialog::onSendViaLocalSend()
{
    ShareManager::shareLocalSend(m_filePath, this);
}

void ShareDialog::shareViaLocalSend(const QString &filePath, QWidget *parent)
{
    ShareManager::shareLocalSend(filePath, parent);
}
