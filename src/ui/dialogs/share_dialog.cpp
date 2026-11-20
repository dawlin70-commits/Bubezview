#include "share_dialog.h"
#include "share_manager.h"
#include "core/services/theme_manager.h"

#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QPainter>
#include <QProcess>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <qrencode.h>

ShareDialog::ShareDialog(const QString &filePath, QWidget *parent)
    : FramelessDialog(parent)
    , m_filePath(filePath)
    , m_tcpServer(nullptr)
    , m_qrLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_urlLabel(nullptr)
    , m_btnCopyPath(nullptr)
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

    // Detect LAN IPv4 address (skip virtual/container bridges like docker0, br-*, veth*, virbr*)
    QString localIp = QStringLiteral("127.0.0.1");
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString ifName = iface.name().toLower();
        if (ifName.startsWith(QLatin1String("docker")) ||
            ifName.startsWith(QLatin1String("br-")) ||
            ifName.startsWith(QLatin1String("veth")) ||
            ifName.startsWith(QLatin1String("virbr")) ||
            ifName.startsWith(QLatin1String("tun")) ||
            ifName.startsWith(QLatin1String("tap"))) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback()) {
                continue;
            }
            const quint32 ipv4 = addr.toIPv4Address();
            const quint8 octet1 = static_cast<quint8>((ipv4 >> 24) & 0xFF);
            const quint8 octet2 = static_cast<quint8>((ipv4 >> 16) & 0xFF);
            const bool isPrivateLan =
                (octet1 == 192 && octet2 == 168) ||
                (octet1 == 10) ||
                (octet1 == 172 && octet2 >= 16 && octet2 <= 31);

            if (isPrivateLan) {
                localIp = addr.toString();
                break;
            } else if (localIp == QLatin1String("127.0.0.1")) {
                localIp = addr.toString();
            }
        }
        if (localIp != QLatin1String("127.0.0.1")) {
            break;
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
            const QByteArray req = socket->readAll();
            // Avoid handling duplicate readyRead events once a response is already streaming
            if (socket->property("bubez_handled").toBool()) {
                return;
            }
            socket->setProperty("bubez_handled", true);

            const bool isGet = req.startsWith("GET ");
            const bool isHead = req.startsWith("HEAD ");
            if (!isGet && !isHead) {
                socket->write("HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }

            // Reject browser favicon requests without reading the shared file
            if (req.startsWith("GET /favicon.ico") || req.startsWith("HEAD /favicon.ico")) {
                socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }

            auto *file = new QFile(m_filePath, socket);
            if (!file->open(QIODevice::ReadOnly)) {
                delete file;
                socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile not found");
                socket->disconnectFromHost();
                return;
            }

            const QFileInfo fi(m_filePath);
            QByteArray header;
            header.append("HTTP/1.1 200 OK\r\n");
            header.append("Content-Type: application/octet-stream\r\n");
            header.append("Content-Disposition: attachment; filename=\"" + fi.fileName().toUtf8() + "\"\r\n");
            header.append("Content-Length: " + QByteArray::number(file->size()) + "\r\n");
            header.append("Connection: close\r\n\r\n");
            socket->write(header);

            if (isHead || file->size() == 0) {
                file->close();
                socket->disconnectFromHost();
                return;
            }

            // Stream the file in 64 KB chunks so large videos/archives never spike RAM
            constexpr qint64 kChunkSize = 64 * 1024;
            auto pumpNextChunk = [socket, file]() {
                if (!socket || !file || !file->isOpen()) return;
                while (socket->bytesToWrite() < kChunkSize * 4 && !file->atEnd()) {
                    const QByteArray chunk = file->read(kChunkSize);
                    if (chunk.isEmpty()) break;
                    socket->write(chunk);
                }
                if (file->atEnd()) {
                    file->close();
                    socket->disconnectFromHost();
                }
            };

            connect(socket, &QTcpSocket::bytesWritten, socket, [pumpNextChunk](qint64) {
                pumpNextChunk();
            });
            pumpNextChunk();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void ShareDialog::setupUi()
{
    const bool isDark = ThemeManager::isDarkTheme();
    const QString titleCol = isDark ? QStringLiteral("#f8fafc") : QStringLiteral("#0f172a");
    const QString subCol = isDark ? QStringLiteral("#94a3b8") : QStringLiteral("#475569");
    const QString urlCol = isDark ? QStringLiteral("#38bdf8") : QStringLiteral("#0369a1");
    const QString closeBg = isDark ? QStringLiteral("rgba(51, 65, 85, 0.85)") : QStringLiteral("rgba(0, 0, 0, 0.06)");
    const QString closeHoverBg = isDark ? QStringLiteral("rgba(71, 85, 105, 0.95)") : QStringLiteral("rgba(0, 0, 0, 0.12)");
    const QString closeText = isDark ? QStringLiteral("#f1f5f9") : QStringLiteral("#334155");
    const QString closeBorder = isDark ? QStringLiteral("rgba(255, 255, 255, 0.14)") : QStringLiteral("rgba(0, 0, 0, 0.12)");

    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(14);

    QFileInfo fi(m_filePath);

    QLabel *titleLabel = new QLabel(QString(tr("📲 Share: %1")).arg(fi.fileName()), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: %1; padding-bottom: 2px;").arg(titleCol));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);

    m_qrLabel = new QLabel(this);
    m_qrLabel->setFixedSize(260, 260);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setStyleSheet("background-color: #ffffff; border-radius: 16px; border: 1.5px solid rgba(56, 189, 248, 0.35); padding: 12px;");
    mainLayout->addWidget(m_qrLabel, 0, Qt::AlignCenter);

    m_infoLabel = new QLabel(tr("Scan with your phone (make sure you are on the same Wi-Fi network):"), this);
    m_infoLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: %1;").arg(subCol));
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_infoLabel);

    m_urlLabel = new QLabel(m_serverUrl, this);
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_urlLabel->setStyleSheet(QStringLiteral(
        "color: %1; background-color: rgba(56, 189, 248, 0.14); border: 1px solid rgba(56, 189, 248, 0.38); border-radius: 12px; padding: 8px 12px; font-size: 12px; font-weight: bold; font-family: monospace;")
        .arg(urlCol));
    m_urlLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_urlLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    m_btnCopyPath = new QPushButton(tr("📋 Copy Path"), this);
    m_btnCopyPath->setCursor(Qt::PointingHandCursor);
    m_btnCopyPath->setStyleSheet(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #10b981, stop:1 #059669);"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   border: 1px solid rgba(255, 255, 255, 0.25);"
        "   border-radius: 10px;"
        "   padding: 8px 16px;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #34d399, stop:1 #10b981); }"
    );
    connect(m_btnCopyPath, &QPushButton::clicked, this, [this]() {
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(m_filePath);
            m_btnCopyPath->setText(tr("✓ Copied!"));
            QTimer::singleShot(2000, this, [this]() {
                if (m_btnCopyPath) {
                    m_btnCopyPath->setText(tr("📋 Copy Path"));
                }
            });
        }
    });
    btnLayout->addWidget(m_btnCopyPath);

    m_btnLocalSend = new QPushButton(tr("🚀 Send with LocalSend"), this);
    m_btnLocalSend->setCursor(Qt::PointingHandCursor);
    m_btnLocalSend->setStyleSheet(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb);"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   border: 1px solid rgba(125, 211, 252, 0.55);"
        "   border-radius: 10px;"
        "   padding: 8px 16px;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6); }"
    );
    connect(m_btnLocalSend, &QPushButton::clicked, this, &ShareDialog::onSendViaLocalSend);
    btnLayout->addWidget(m_btnLocalSend);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 10px;"
        "   padding: 8px 16px;"
        "}"
        "QPushButton:hover { background-color: %4; }")
        .arg(closeBg, closeText, closeBorder, closeHoverBg));
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
