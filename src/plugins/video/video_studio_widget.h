/**
 * @file video_studio_widget.h
 * @brief Embedded Video Studio side panel implemented inside bubez_video.so.
 */

#pragma once

#include "core/interfaces/i_video_studio_plugin.h"

#include <QScrollArea>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QList>

/**
 * @class VideoStudioWidget
 * @brief Docked right-hand video editing workspace hosted inside SystemMediaViewer Edit Mode.
 */
class VideoStudioWidget : public IVideoStudioWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the modular Video Studio side panel.
     * @param parent Optional parent widget.
     */
    explicit VideoStudioWidget(QWidget *parent = nullptr);
    ~VideoStudioWidget() override;

    /** @copydoc IVideoStudioWidget::setTargetVideo */
    void setTargetVideo(const QString &filePath, qint64 durationMs) override;

    /** @copydoc IVideoStudioWidget::setPlaybackPosition */
    void setPlaybackPosition(qint64 positionMs, qint64 durationMs) override;

    /** @copydoc IVideoStudioWidget::updateTheme */
    void updateTheme() override;

    /** @copydoc IVideoStudioWidget::cancelActiveOperations */
    void cancelActiveOperations() override;

    /** @copydoc IVideoStudioWidget::setOnOutputCreated */
    void setOnOutputCreated(std::function<void(const QString &outputPath)> cb) override;

private:
    void setupUi();
    bool ensureFfmpegAvailable();
    void stopActiveProcess(bool removePartialOutput);
    void runFfmpegJob(const QStringList &args, const QString &outputPath, const QString &busyMessage);
    void setBusy(bool busy, const QString &status = QString());

    void exportTrimmedVideo();
    void extractAudioTrack();
    void captureFrameSnapshot();
    void exportTransformedVideo();

    static QString formatTimestamp(qint64 ms);
    static bool parseTimestampMs(const QString &text, qint64 *outMs);
    static bool isSameFilePath(const QString &pathA, const QString &pathB);

    QString m_videoPath;
    qint64 m_currentPosMs = 0;
    qint64 m_durationMs = 0;
    bool m_endAutoFilled = true;
    QProcess *m_activeProcess = nullptr;
    QString m_pendingOutputPath;
    QString m_processLog;
    std::function<void(const QString &)> m_onOutputCreated;

    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_lblStatus = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_btnCancel = nullptr;
    QList<QPushButton *> m_actionButtons;

    // Trim / Cut controls
    QLineEdit *m_editStart = nullptr;
    QLineEdit *m_editEnd = nullptr;
    QCheckBox *m_chkLosslessTrim = nullptr;

    // Audio extraction controls
    QComboBox *m_comboAudioFormat = nullptr;

    // Transform & optimize controls
    QComboBox *m_comboRotate = nullptr;
    QComboBox *m_comboScale = nullptr;
    QComboBox *m_comboSpeed = nullptr;
    QCheckBox *m_chkMuteAudio = nullptr;
};
