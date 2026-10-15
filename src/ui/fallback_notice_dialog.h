/**
 * @file fallback_notice_dialog.h
 * @brief Dialog informing users that a system package is being used instead of a compiled source binary.
 */

#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QCheckBox;
class QPushButton;

/**
 * @class FallbackNoticeDialog
 * @brief Modal dialog shown when the High-Performance profile is active, but an engine or extension
 * falls back to a generic distro/system executable instead of a source-compiled binary.
 * Provides an option to suppress future notices per-component.
 */
class FallbackNoticeDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the fallback notification dialog.
     * @param componentId Unique component identifier (e.g. "tesseract", "qpdf").
     * @param displayName Human-readable title of the component (e.g. "Tesseract OCR").
     * @param executablePath Path of the detected system executable (e.g. "/usr/bin/tesseract").
     * @param parent Optional parent widget.
     */
    explicit FallbackNoticeDialog(const QString &componentId,
                                 const QString &displayName,
                                 const QString &executablePath,
                                 QWidget *parent = nullptr);
    ~FallbackNoticeDialog() override = default;

    /**
     * @brief Checks if notifications for the given component are suppressed in user settings.
     * @param componentId Component ID.
     * @return True if user previously checked 'Do not show again'.
     */
    static bool isNoticeSuppressed(const QString &componentId);

    /**
     * @brief Sets whether notifications for the component should be suppressed.
     * @param componentId Component ID.
     * @param suppress True to suppress.
     */
    static void setNoticeSuppressed(const QString &componentId, bool suppress);

    /**
     * @brief Convenience helper that checks if HighPerformance is active, a source build is missing,
     * a system binary is found, and notice is not suppressed. If so, shows the dialog.
     * @param componentId Component ID (e.g. "tesseract", "qpdf").
     * @param displayName Human-readable title.
     * @param systemPath Detected system executable path.
     * @param parent Parent widget.
     * @return True if user chooses to proceed with system binary; false if user triggered compilation or cancelled.
     */
    static bool checkAndPrompt(const QString &componentId,
                               const QString &displayName,
                               const QString &systemPath,
                               QWidget *parent = nullptr);

    /**
     * @brief Returns true if the user requested to compile the component from source right now.
     */
    bool wantsCompilation() const { return m_wantsCompilation; }

private slots:
    void onProceedClicked();
    void onCompileClicked();

private:
    QString m_componentId;
    QCheckBox *m_chkDontShowAgain;
    bool m_wantsCompilation;
};
