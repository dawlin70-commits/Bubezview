/**
 * @file share_manager.h
 * @brief Cross-platform and Linux desktop sharing integrations for BubezView.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMenu>
#include <QWidget>
#include <functional>

/**
 * @class ShareManager
 * @brief Manages sending files to external sharing protocols and local transfer apps.
 *
 * Provides menu generation and subprocess execution for tools such as
 * KDE Connect, LocalSend, Bluetooth OBEX push, Telegram, Email (xdg-email / mailto),
 * and local HTTP/QR code transfers.
 */
class ShareManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a ShareManager instance.
     * @param parentWidget Parent widget context for UI actions and dialog parenting.
     * @param getFilePathCallback Callback returning the path of the currently active file.
     */
    explicit ShareManager(QWidget *parentWidget, std::function<QString()> getFilePathCallback);

    /**
     * @brief Destructor.
     */
    ~ShareManager() override = default;

    /**
     * @brief Populates an existing menu with sharing actions.
     * @param menu Target menu to populate.
     */
    void populateMenu(QMenu *menu);

    /**
     * @brief Creates a standalone share menu.
     * @param parent Optional parent widget for the created menu.
     * @return Pointer to the freshly created QMenu.
     */
    QMenu* createShareMenu(QWidget *parent = nullptr);

    /**
     * @brief Checks if any of the candidate executable names or Flatpak IDs are installed.
     * @param commandsOrFlatpaks List of candidate binary names or Flatpak app IDs.
     * @return True if at least one candidate is available, false otherwise.
     */
    static bool isAvailable(const QStringList &commandsOrFlatpaks);

    /**
     * @brief Resolves the launch argument list for the available backend (native binary or flatpak run).
     * @param commandsOrFlatpaks Candidate executable names or Flatpak app IDs.
     * @return Argument list ready for QProcess execution.
     */
    static QStringList getExecutable(const QStringList &commandsOrFlatpaks);

    /**
     * @brief Shares the given file via KDE Connect (kdeconnect-handler or kdeconnect-cli).
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget for notification or modal dialogs.
     */
    static void shareKdeConnect(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Shares the file via LocalSend LAN transfer.
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareLocalSend(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Shares the file via Bluetooth OBEX push (bluetooth-sendto).
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareBluetooth(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Sends the file via Telegram Desktop client.
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareTelegram(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Sends the file as an email attachment using xdg-email or Thunderbird.
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareEmail(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Opens the local HTTP web server and QR code sharing dialog.
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareQrCode(const QString &filePath, QWidget *parent = nullptr);

private:
    QWidget *m_parent;
    std::function<QString()> m_getFilePath;
};

