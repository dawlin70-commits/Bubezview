/**
 * @file engine_prompt_card.h
 * @brief Widget displaying engine activation choices when a required engine plugin is inactive.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "core/plugin_manager.h"

/**
 * @class EnginePromptCard
 * @brief Centered card shown in viewer stack when opening a file for an inactive engine.
 */
class EnginePromptCard : public QWidget {
    Q_OBJECT

public:
    explicit EnginePromptCard(QWidget *parent = nullptr);
    ~EnginePromptCard() override = default;

    /**
     * @brief Configures prompt for a specific missing file category and file path.
     * @param category Target file category.
     * @param filePath Path to the document attempted to open.
     */
    void setTargetFile(FileCategory category, const QString &filePath);

signals:
    /**
     * @brief Emitted when an engine was successfully activated, requesting document reload.
     * @param filePath Path to the file to reload.
     */
    void engineActivated(const QString &filePath);

    /**
     * @brief Emitted when user clicks to open advanced compilation dialog.
     */
    void compileRequested();

private slots:
    void onActivateBinaryClicked();

private:
    FileCategory m_category;
    QString m_filePath;

    QLabel *m_lblIcon;
    QLabel *m_lblTitle;
    QLabel *m_lblSubtitle;
    QLabel *m_lblDetails;
    QPushButton *m_btnActivateBinary;
    QPushButton *m_btnCompileSource;
    QLabel *m_lblStatus;
};
