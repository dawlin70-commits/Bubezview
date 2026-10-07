/**
 * @file syntax_highlighter.cpp
 * @brief Implementation of the native multi-language syntax highlighter.
 */

#include "syntax_highlighter.h"
#include <QFont>
#include <QColor>

TextSyntaxHighlighter::TextSyntaxHighlighter(QTextDocument *parent, bool isDark)
    : QSyntaxHighlighter(parent)
    , m_isDark(isDark)
{
    setupRules();
}

void TextSyntaxHighlighter::applyTheme(bool isDark)
{
    if (m_isDark != isDark) {
        m_isDark = isDark;
        setupRules();
        rehighlight();
    }
}

void TextSyntaxHighlighter::setHighlightingEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        rehighlight();
    }
}

void TextSyntaxHighlighter::setupRules()
{
    m_rules.clear();

    // 1. Color Palettes
    QColor keywordColor  = m_isDark ? QColor("#c678dd") : QColor("#7928ca"); // Purple / Magenta
    QColor typeColor     = m_isDark ? QColor("#61afef") : QColor("#0070f3"); // Cyan / Blue
    QColor stringColor   = m_isDark ? QColor("#98c379") : QColor("#22863a"); // Green
    QColor numberColor   = m_isDark ? QColor("#d19a66") : QColor("#b08800"); // Orange / Ochre
    QColor commentColor  = m_isDark ? QColor("#7f848e") : QColor("#6a737d"); // Muted Gray
    QColor preprocColor  = m_isDark ? QColor("#e06c75") : QColor("#d73a49"); // Red / Coral

    // Formats
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(keywordColor);
    keywordFormat.setFontWeight(QFont::Bold);

    QTextCharFormat typeFormat;
    typeFormat.setForeground(typeColor);
    typeFormat.setFontWeight(QFont::Medium);

    QTextCharFormat stringFormat;
    stringFormat.setForeground(stringColor);

    QTextCharFormat numberFormat;
    numberFormat.setForeground(numberColor);

    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(commentColor);
    singleLineCommentFormat.setFontItalic(true);

    m_multiLineCommentFormat.setForeground(commentColor);
    m_multiLineCommentFormat.setFontItalic(true);

    QTextCharFormat preprocessorFormat;
    preprocessorFormat.setForeground(preprocColor);

    // 2. Keyword Patterns
    const QStringList keywordPatterns = {
        "\\bdef\\b", "\\bclass\\b", "\\bimport\\b", "\\bfrom\\b", "\\breturn\\b",
        "\\bif\\b", "\\belse\\b", "\\belif\\b", "\\bfor\\b", "\\bwhile\\b",
        "\\btry\\b", "\\bexcept\\b", "\\bfinally\\b", "\\bwith\\b", "\\bas\\b",
        "\\bpass\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\bTrue\\b", "\\bFalse\\b",
        "\\bNone\\b", "\\band\\b", "\\bor\\b", "\\bnot\\b", "\\bin\\b", "\\bis\\b",
        "\\blambda\\b", "\\basync\\b", "\\bawait\\b", "\\byield\\b",
        "\\bstruct\\b", "\\benum\\b", "\\bnamespace\\b", "\\btemplate\\b", "\\btypename\\b",
        "\\bpublic\\b", "\\bprotected\\b", "\\bprivate\\b", "\\bconst\\b", "\\bstatic\\b",
        "\\bconstexpr\\b", "\\bexplicit\\b", "\\bvirtual\\b", "\\boverride\\b", "\\bfinal\\b",
        "\\bnew\\b", "\\bdelete\\b", "\\bswitch\\b", "\\bcase\\b", "\\bdefault\\b",
        "\\bsizeof\\b", "\\btypedef\\b", "\\busing\\b", "\\bfunction\\b", "\\blet\\b",
        "\\bvar\\b", "\\bnull\\b", "\\bundefined\\b", "\\bfn\\b", "\\bmut\\b",
        "\\bpub\\b", "\\bimpl\\b", "\\btrait\\b", "\\bselect\\b", "\\binsert\\b",
        "\\bupdate\\b", "\\bwhere\\b", "\\bjoin\\b", "\\becho\\b", "\\blocal\\b",
        "\\bfi\\b", "\\bdone\\b", "\\besac\\b"
    };

    for (const QString &pattern : keywordPatterns) {
        m_rules.append({QRegularExpression(pattern), keywordFormat});
    }

    // 3. Type Patterns
    const QStringList typePatterns = {
        "\\bint\\b", "\\bchar\\b", "\\bbool\\b", "\\bfloat\\b", "\\bdouble\\b",
        "\\bvoid\\b", "\\bsize_t\\b", "\\bint32_t\\b", "\\bint64_t\\b",
        "\\buint32_t\\b", "\\buint64_t\\b", "\\bQString\\b", "\\bQByteArray\\b",
        "\\bstd\\b", "\\bstring\\b", "\\bvector\\b", "\\bauto\\b", "\\bself\\b",
        "\\bthis\\b", "\\bnullptr\\b"
    };

    for (const QString &pattern : typePatterns) {
        m_rules.append({QRegularExpression(pattern), typeFormat});
    }

    // 4. Numbers (Integers, Floats, Hex)
    m_rules.append({QRegularExpression("\\b0x[0-9a-fA-F]+\\b"), numberFormat});
    m_rules.append({QRegularExpression("\\b[0-9]+(\\.[0-9]+)?\\b"), numberFormat});

    // 5. Preprocessor and Decorators
    m_rules.append({QRegularExpression("^\\s*#[a-zA-Z_]+"), preprocessorFormat});
    m_rules.append({QRegularExpression("@[a-zA-Z_][a-zA-Z0-9_]*"), preprocessorFormat});

    // 6. Strings (Double quotes and Single quotes)
    m_rules.append({QRegularExpression("\"([^\"\\\\]|\\\\.)*\""), stringFormat});
    m_rules.append({QRegularExpression("'([^'\\\\]|\\\\.)*'"), stringFormat});

    // 7. Single-line Comments (C/C++ //, Python/Shell #, SQL --)
    m_rules.append({QRegularExpression("//[^\n]*"), singleLineCommentFormat});
    m_rules.append({QRegularExpression("#[^\n]*"), singleLineCommentFormat});
    m_rules.append({QRegularExpression("--[^\n]*"), singleLineCommentFormat});

    // 8. Multi-line Comments (/* ... */)
    m_commentStartRegex = QRegularExpression("/\\*");
    m_commentEndRegex = QRegularExpression("\\*/");
}

void TextSyntaxHighlighter::highlightBlock(const QString &text)
{
    if (!m_enabled) {
        return;
    }

    // Apply standard regex rules
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comment tracking
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(m_commentStartRegex);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch;
        int endIndex = text.indexOf(m_commentEndRegex, startIndex, &endMatch);
        int commentLength = 0;

        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }

        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        startIndex = text.indexOf(m_commentStartRegex, startIndex + commentLength);
    }
}
