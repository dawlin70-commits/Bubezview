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
#include <QImage>
#include <functional>

/**
 * @class ShareManager
 * @brief Manages sending files to external sharing protocols and local transfer apps.
 *
 * Provides menu generation and subprocess execution for tools such as
 * KDE Connect, LocalSend, Bluetooth OBEX push, Telegram, Email (xdg-email / Webmail),
 * and local HTTP/QR code transfers.
 */
class ShareManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Supported email delivery targets.
     */
    enum class EmailProvider {
        SystemDefault,
        Gmail,
        Outlook,
        Yahoo
    };

    /**
     * @brief Constructs a ShareManager instance.
     * @param parentWidget Parent widget context for UI actions and dialog parenting.
     * @param getFilePathCallback Callback returning the path of the currently active file.
     */
    explicit ShareManager(QWidget *parentWidget, std::function<QString()> getFilePathCallback);

    /**
     * @brief Registers an optional callback returning an image representation of the current view.
     * @param getImageCallback Callback returning a QImage (e.g. loaded image or rendered PDF page).
     * @param isPagedDocument True when the current view is a multi-page document (PDF/Office).
     */
    void setImageProvider(std::function<QImage()> getImageCallback, bool isPagedDocument = false);

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
     * @brief Sends the file as an email attachment using the selected provider (desktop client or webmail).
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     * @param provider Target email delivery provider.
     * @param imageToCopy Optional QImage to place in the clipboard (for instant Ctrl+V paste in webmail).
     * @param asImageOnly When true and imageToCopy is valid, exports/attaches only the image instead of the original file.
     */
    static void shareEmail(const QString &filePath,
                           QWidget *parent = nullptr,
                           EmailProvider provider = EmailProvider::SystemDefault,
                           const QImage &imageToCopy = QImage(),
                           bool asImageOnly = false);

    /**
     * @brief Returns the last used EmailProvider from settings.
     */
    static EmailProvider lastEmailProvider();

    /**
     * @brief Saves the given EmailProvider as the last used provider.
     */
    static void setLastEmailProvider(EmailProvider provider);

    /**
     * @brief Opens the local HTTP web server and QR code sharing dialog.
     * @param filePath Absolute path of the file to share.
     * @param parent Parent widget.
     */
    static void shareQrCode(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Ensures that a target path is a single shareable file.
     * If @p filePath is a directory, compresses it on-the-fly into a temporary .zip archive.
     * @param filePath File or directory path to prepare for sharing.
     * @param parent Optional parent widget for progress/error dialogs.
     * @return Path to the shareable file (or temporary .zip archive), or empty string on failure.
     */
    static QString ensureShareableFile(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief Registers a custom directory-to-zip compressor function (e.g. backed by the libarchive plugin).
     * @param compressor Callback taking (folderPath, destZipPath, parentWidget) and returning true on success.
     */
    static void setFolderCompressor(std::function<bool(const QString &, const QString &, QWidget *)> compressor);

private:
    QWidget *m_parent;
    std::function<QString()> m_getFilePath;
    std::function<QImage()> m_getImage;
    bool m_isPagedDocument = false;
};


