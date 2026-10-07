/**
 * @file syntax_highlighter.h
 * @brief Native multi-language syntax highlighting engine for code and text files.
 */

#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QList>

/**
 * @brief Lightweight, zero-dependency syntax highlighter supporting standard programming and config languages.
 */
class TextSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /**
     * @brief Constructs a syntax highlighter attached to a document.
     * @param parent Target text document.
     * @param isDark Initial theme mode (true for dark, false for light).
     */
    explicit TextSyntaxHighlighter(QTextDocument *parent = nullptr, bool isDark = true);

    /**
     * @brief Updates color palette according to active theme preset.
     * @param isDark True if dark theme is active, false for light theme.
     */
    void applyTheme(bool isDark);

    /**
     * @brief Enables or disables syntax highlighting.
     * @param enabled True to highlight tokens; false to render plain text.
     */
    void setHighlightingEnabled(bool enabled);

    /**
     * @brief Checks if highlighting is currently active.
     */
    bool isHighlightingEnabled() const { return m_enabled; }

protected:
    /**
     * @brief Core Qt syntax parsing callback for each block of text.
     * @param text Text line/block to format.
     */
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupRules();

    bool m_enabled = true;
    bool m_isDark = true;
    QList<HighlightingRule> m_rules;

    QRegularExpression m_commentStartRegex;
    QRegularExpression m_commentEndRegex;
    QTextCharFormat m_multiLineCommentFormat;
};
