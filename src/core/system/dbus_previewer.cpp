#include "dbus_previewer.h"
#include <QUrl>
#include <QFileInfo>
#include <QDebug>

DBusPreviewer::DBusPreviewer(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void DBusPreviewer::ShowFile(const QString &uri, int xid, bool closeIfAlreadyShown)
{
    Q_UNUSED(xid);

    if (!m_window || uri.isEmpty()) return;

    QUrl url(uri);
    QString localPath;
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    } else if (uri.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        QUrl inputUrl = QUrl::fromUserInput(uri);
        localPath = inputUrl.isLocalFile() ? inputUrl.toLocalFile() : url.toLocalFile();
    } else {
        localPath = uri;
    }

    if (localPath.isEmpty()) return;

    if (closeIfAlreadyShown && m_window->isVisible() && m_lastUri == uri) {
        m_window->close();
        m_lastUri.clear();
        return;
    }

    m_window->cancelStandbyTimer();
    m_lastUri = uri;
    MainWindow::OpenResult result = m_window->openFile(localPath);
    if (result != MainWindow::OpenResult::DelegatedToPlugin) {
        if (m_window->isMinimized()) {
            m_window->showNormal();
        } else {
            m_window->show();
        }
        m_window->raise();
        m_window->activateWindow();
    }
}

void DBusPreviewer::ShowFileWithToken(const QString &uri, const QString &activationToken)
{
    if (!activationToken.isEmpty()) {
        // Qt6 Wayland QPA reads XDG_ACTIVATION_TOKEN during activateWindow() to request
        // compositor focus transfer without triggering Focus Stealing Prevention.
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }
    ShowFile(uri, 0, false);
}

void DBusPreviewer::Close()
{
    if (!m_window) return;
    m_window->close();
    m_lastUri.clear();
}
