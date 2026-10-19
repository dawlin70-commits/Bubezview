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

    QUrl url(uri);
    QString localPath = url.isLocalFile() ? url.toLocalFile() : uri;

    if (closeIfAlreadyShown && m_window->isVisible() && m_lastUri == uri) {
        m_window->close();
        m_lastUri.clear();
        return;
    }

    m_lastUri = uri;
    MainWindow::OpenResult result = m_window->openFile(localPath);
    if (result != MainWindow::OpenResult::DelegatedToPlugin) {
        m_window->show();
        m_window->raise();
        m_window->activateWindow();
    }
}

void DBusPreviewer::Close()
{
    m_window->close();
    m_lastUri.clear();
}
