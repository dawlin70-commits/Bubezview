/**
 * @file text_viewer.h
 * @brief High-performance text and source code viewer/editor widget.
 */

#pragma once

#include <QPlainTextEdit>
#include <QString>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>

class LineNumberArea;
class TextSyntaxHighlighter;

/**
 * @brief High-performance text and source code viewer/editor widget.
 *
 * Provides syntax highlighting, line numbers gutter, circular search,
 * find & replace, text font zoom, whitespace rendering, go to line,
 * smart line editing shortcuts, and disk persistence.
 */
class TextViewer : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit TextViewer(QWidget *parent = nullptr);
    ~TextViewer() override;

    /**
     * @brief Refreshes visual appearance based on the active ThemePreset.
     */
    void updateTheme();

    /**
     * @brief Loads a text or source code file from disk.
     * @param filePath Absolute path of the text file.
     * @return True if read and populated successfully; false on I/O error.
     */
    bool loadFile(const QString &filePath);

    /**
     * @brief Saves current text buffer to disk.
     * @param targetPath Target file path, or empty to save to the current file path.
     * @return True if successfully written; false on error.
     */
    bool saveFile(const QString &targetPath = QString());

    /**
     * @brief Gets total number of lines in the loaded document.
     */
    int lineCount() const;

    /**
     * @brief Gets total character count of the loaded document.
     */
    qint64 charCount() const;

    /**
     * @brief Gets total word count of the loaded document.
     */
    int wordCount() const;

    /**
     * @brief Returns whether edit mode is currently enabled.
     */
    bool isEditable() const { return !isReadOnly(); }

    /**
     * @brief Enables or disables edit mode.
     * @param editable True to allow typing and modifying text, false for read-only.
     */
    void setEditable(bool editable);

    /**
     * @brief Checks if document is editable; if read-only, prompts user to activate editing mode.
     * @return True if edit mode is confirmed and active; false if user declined or cancelled.
     */
    bool ensureEditable();

    /**
     * @brief Checks if the file path or content corresponds to source code / script / config.
     */
    bool isCodeFile() const;

    /**
     * @brief Gets the path of the currently loaded file.
     */
    QString currentFilePath() const { return m_currentFilePath; }

    // --- Search & Replace ---
    bool findNext(const QString &text, bool matchCase, bool *wrapped = nullptr);
    bool findPrev(const QString &text, bool matchCase, bool *wrapped = nullptr);
    bool replaceNext(const QString &findText, const QString &replaceText, bool matchCase);
    int replaceAll(const QString &findText, const QString &replaceText, bool matchCase);

    /**
     * @brief Checks whether a given key event would modify text content.
     */
    bool isModifyingKey(QKeyEvent *event) const;

    // --- Line Numbers Gutter ---
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void setLineNumbersVisible(bool visible);
    bool isLineNumbersVisible() const { return m_lineNumbersVisible; }

    // --- Syntax Highlighting ---
    void setSyntaxHighlightingEnabled(bool enabled);
    bool isSyntaxHighlightingEnabled() const;

    // --- Visual Options ---
    void setWordWrapEnabled(bool enabled);
    bool isWordWrapEnabled() const { return lineWrapMode() == QPlainTextEdit::WidgetWidth; }
    void toggleWordWrap();

    void setShowWhitespace(bool show);
    bool isShowWhitespace() const { return m_showWhitespace; }

    // --- Font Zoom ---
    void zoomInText(int range = 1);
    void zoomOutText(int range = 1);
    void resetTextZoom();

    // --- Navigation & Smart Editing ---
    void gotoLine(int lineNum);
    void gotoLineDialog();
    void duplicateCurrentLine();
    void deleteCurrentLine();
    void toggleComment();

public slots:
    void cut();
    void paste();

signals:
    /**
     * @brief Emitted when edit mode is toggled between read-only and editable.
     */
    void editModeChanged(bool editable);

    /**
     * @brief Emitted when document modification status changes.
     */
    void modificationChanged(bool modified);

    /**
     * @brief Emitted when visual options (wrap, line numbers, zoom) change.
     */
    void visualOptionsChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount = 0);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    QString m_currentFilePath;
    int m_basePointSize = 10;
    bool m_lineNumbersVisible = true;
    bool m_showWhitespace = false;

    LineNumberArea *m_lineNumberArea = nullptr;
    TextSyntaxHighlighter *m_highlighter = nullptr;
};
