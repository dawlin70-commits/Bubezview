/**
 * @file dbus_previewer.h
 * @brief FreeDesktop D-Bus interface for file managers (org.nemo.Preview / org.gnome.NautilusPreviewer).
 */

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include "mainwindow.h"

/**
 * @class DBusPreviewer
 * @brief Exposes the org.nemo.Preview D-Bus service interface.
 *
 * Allows desktop file managers like Nemo or Nautilus to request instantaneous file previews
 * when the user presses Space, positioning and showing/hiding the preview window seamlessly.
 */
class DBusPreviewer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nemo.Preview")

public:
    /**
     * @brief Constructs the DBusPreviewer service wrapper.
     * @param window Pointer to the main window preview controller.
     * @param parent Optional parent QObject.
     */
    explicit DBusPreviewer(MainWindow *window, QObject *parent = nullptr);

public slots:
    /**
     * @brief D-Bus method invoked to preview a file URI (Nemo / GNOME <= 42).
     * @param uri URI of the file to preview (e.g. file:///path/to/file).
     * @param xid Window ID of the caller file manager window for transient positioning.
     * @param closeIfAlreadyShown If true and the current file is already shown, toggles preview off.
     */
    Q_SCRIPTABLE void ShowFile(const QString &uri, int xid, bool closeIfAlreadyShown);

    /**
     * @brief D-Bus overload for GNOME 43+ (org.gnome.NautilusPreviewer2).
     * @param uri URI of the file to preview.
     * @param windowHandle Wayland/X11 parent window handle string.
     * @param closeIfAlreadyShown If true and the current file is already shown, toggles preview off.
     */
    Q_SCRIPTABLE void ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown);

    /**
     * @brief D-Bus overload for GNOME 45+ (org.gnome.NautilusPreviewer3).
     * @param uri URI of the file to preview.
     * @param windowHandle Wayland/X11 parent window handle string.
     * @param closeIfAlreadyShown If true and the current file is already shown, toggles preview off.
     * @param activationToken Wayland xdg-activation token.
     */
    Q_SCRIPTABLE void ShowFile(const QString &uri, const QString &windowHandle, bool closeIfAlreadyShown, const QString &activationToken);

    /**
     * @brief D-Bus method invoked by a second BubezView instance to preview a file URI
     *        while forwarding a Wayland XDG_ACTIVATION_TOKEN so KWin/compositors raise
     *        the standby window cleanly above the file manager.
     * @param uri URI of the file to preview (e.g. file:///path/to/file).
     * @param activationToken Wayland xdg-activation token captured from the launcher process.
     */
    Q_SCRIPTABLE void ShowFileWithToken(const QString &uri, const QString &activationToken);

    /**
     * @brief D-Bus method to request closing the active preview window.
     */
    Q_SCRIPTABLE void Close();

private:
    MainWindow *m_window;
    QString m_lastUri;
};

