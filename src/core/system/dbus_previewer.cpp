#include "dbus_previewer.h"
#include <QUrl>
#include <QFileInfo>
#include <QDebug>

NautilusPreviewer1Adaptor::NautilusPreviewer1Adaptor(DBusPreviewer *parent)
    : QDBusAbstractAdaptor(parent)
    , m_previewer(parent)
{
}

void NautilusPreviewer1Adaptor::ShowFile(const QString &uri, int xid, bool closeIfAlreadyShown)
{
    if (m_previewer) m_previewer->ShowFile(uri, xid, closeIfAlreadyShown);
}

void NautilusPreviewer1Adaptor::Close()
{
    if (m_previewer) m_previewer->Close();
}

NautilusPreviewer2Adaptor::NautilusPreviewer2Adaptor(DBusPreviewer *parent)
    : QDBusAbstractAdaptor(parent)
    , m_previewer(parent)
{
}

QString NautilusPreviewer2Adaptor::parentHandle() const
{
    return m_previewer ? m_previewer->lastParentHandle() : QString();
}

bool NautilusPreviewer2Adaptor::visible() const
{
    return m_previewer && m_previewer->isPreviewVisible();
}

void NautilusPreviewer2Adaptor::ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown)
{
    if (m_previewer) m_previewer->ShowFile(uri, windowHandle, closeIfAlreadyShown);
}

void NautilusPreviewer2Adaptor::Close()
{
    if (m_previewer) m_previewer->Close();
}

NautilusPreviewer3Adaptor::NautilusPreviewer3Adaptor(DBusPreviewer *parent)
    : QDBusAbstractAdaptor(parent)
    , m_previewer(parent)
{
}

void NautilusPreviewer3Adaptor::ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown, const QString &activationToken)
{
    if (m_previewer) m_previewer->ShowFile(uri, windowHandle, closeIfAlreadyShown, activationToken);
}

void NautilusPreviewer3Adaptor::Close()
{
    if (m_previewer) m_previewer->Close();
}

DBusPreviewer::DBusPreviewer(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    // Attach dedicated GNOME D-Bus adaptors so Nautilus 42, 43+ and 45+ all match their exact interface names
    new NautilusPreviewer1Adaptor(this);
    new NautilusPreviewer2Adaptor(this);
    new NautilusPreviewer3Adaptor(this);
}

bool DBusPreviewer::isPreviewVisible() const
{
    return m_window && m_window->isVisible();
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

void DBusPreviewer::ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown)
{
    m_lastParentHandle = windowHandle;
    ShowFile(uri, 0, closeIfAlreadyShown);
}

void DBusPreviewer::ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown, const QString &activationToken)
{
    m_lastParentHandle = windowHandle;
    if (!activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }
    ShowFile(uri, 0, closeIfAlreadyShown);
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
