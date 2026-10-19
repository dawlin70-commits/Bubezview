/**
 * @file text_viewer.cpp
 * @brief Implementation of the high-performance text and code viewer/editor.
 */

#include "text_viewer.h"
#include "line_number_area.h"
#include "syntax_highlighter.h"
#include "theme_manager.h"
#include <QFile>
#include <QTextStream>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QKeyEvent>
#include <QFileInfo>
#include <QSet>
#include <QPainter>
#include <QTextBlock>
#include <QInputDialog>
#include <QRegularExpression>
#include <QTextEdit>

TextViewer::TextViewer(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(m_basePointSize);
    setFont(monoFont);

    // 1. Line numbers gutter
    m_lineNumberArea = new LineNumberArea(this);
    connect(this, &TextViewer::blockCountChanged, this, &TextViewer::updateLineNumberAreaWidth);
    connect(this, &TextViewer::updateRequest, this, &TextViewer::updateLineNumberArea);
    connect(this, &TextViewer::cursorPositionChanged, this, &TextViewer::highlightCurrentLine);

    // 2. Syntax highlighting engine
    m_highlighter = new TextSyntaxHighlighter(document(), true);

    // 3. Modification tracking
    connect(document(), &QTextDocument::modificationChanged, this, &TextViewer::modificationChanged);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    updateTheme();
}

TextViewer::~TextViewer()
{
}

void TextViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();
    bool isDark = (preset != ThemePreset::SystemNative);

    if (m_highlighter) {
        m_highlighter->applyTheme(isDark);
    }

    if (!isDark) {
        setStyleSheet(QStringLiteral(R"(
            QPlainTextEdit {
                background-color: palette(base);
                color: palette(text);
                border: none;
                padding: 4px;
                selection-background-color: palette(highlight);
                selection-color: palette(highlighted-text);
            }
        )") + "\n" + ThemeManager::scrollBarStyleSheet(preset));
    } else {
        setStyleSheet(QStringLiteral(R"(
            QPlainTextEdit {
                background-color: #1e1e1e;
                color: #d4d4d4;
                border: none;
                padding: 4px;
                selection-background-color: #264f78;
                selection-color: #ffffff;
            }
        )") + "\n" + ThemeManager::scrollBarStyleSheet(preset));
    }

    highlightCurrentLine();
    if (m_lineNumberArea) {
        m_lineNumberArea->update();
    }
}

bool TextViewer::loadFile(const QString &filePath)
{
    m_currentFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    constexpr qint64 maxPreviewBytes = 15 * 1024 * 1024; // 15 MB limit
    if (file.size() > maxPreviewBytes) {
        QByteArray chunk = file.read(maxPreviewBytes);
        QString text = QString::fromUtf8(chunk);
        text += QString("\n\n--- [%1] ---").arg(tr("Preview truncated: file size exceeds 15 MB"));
        setPlainText(text);
        setReadOnly(true);
        document()->setModified(false);
        emit editModeChanged(false);
        emit modificationChanged(false);
        updateLineNumberAreaWidth(0);
        return true;
    }

    // Read with UTF-8
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    setPlainText(in.readAll());
    file.close();

    setReadOnly(true);
    document()->setModified(false);

    // Auto-enable syntax highlighting for source code and config files
    if (m_highlighter) {
        m_highlighter->setHighlightingEnabled(isCodeFile() || m_highlighter->isHighlightingEnabled());
    }

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    emit editModeChanged(false);
    emit modificationChanged(false);
    return true;
}

bool TextViewer::saveFile(const QString &targetPath)
{
    QString path = targetPath.isEmpty() ? m_currentFilePath : targetPath;
    if (path.isEmpty()) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << toPlainText();
    file.close();

    m_currentFilePath = path;
    document()->setModified(false);
    emit modificationChanged(false);
    return true;
}

int TextViewer::lineCount() const
{
    return blockCount();
}

qint64 TextViewer::charCount() const
{
    return toPlainText().length();
}

int TextViewer::wordCount() const
{
    QString text = toPlainText();
    if (text.isEmpty()) return 0;
    static const QRegularExpression wsReg("\\s+");
    return text.split(wsReg, Qt::SkipEmptyParts).size();
}

void TextViewer::setEditable(bool editable)
{
    if (isReadOnly() == !editable) return;
    setReadOnly(!editable);
    highlightCurrentLine();
    emit editModeChanged(editable);
}

bool TextViewer::ensureEditable()
{
    if (!isReadOnly()) return true;

    // Safety guard for headless / automated test executions or standalone instances
    if (!isVisible() && parentWidget() == nullptr) {
        setEditable(true);
        return true;
    }

    QMessageBox msg(this->window());
    msg.setIcon(QMessageBox::Question);
    msg.setWindowTitle(tr("Read-Only Document"));

    if (isCodeFile()) {
        msg.setText(tr("<h3>Source Code / Configuration File</h3>"
                       "<p>This file contains source code or configuration settings.</p>"
                       "<p>Manual editing may affect script or application execution.</p>"
                       "<p>Do you want to enable edit mode to start typing and making changes?</p>"));
    } else {
        msg.setText(tr("<h3>File is Read-Only</h3>"
                       "<p>This text file is currently open in read-only mode to prevent accidental changes.</p>"
                       "<p>Do you want to enable edit mode to start typing and making changes?</p>"));
    }

    QAbstractButton *btnEnable = msg.addButton(tr("Enable Editing"), QMessageBox::AcceptRole);
    QAbstractButton *btnCancel = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
    Q_UNUSED(btnCancel);
    msg.setDefaultButton(static_cast<QPushButton*>(btnEnable));
    msg.exec();

    if (msg.clickedButton() == btnEnable) {
        setEditable(true);
        return true;
    }
    return false;
}

bool TextViewer::isCodeFile() const
{
    if (m_currentFilePath.isEmpty()) return false;
    static const QSet<QString> codeExts = {
        "c", "cpp", "cxx", "cc", "h", "hpp", "hxx",
        "py", "pyw", "js", "ts", "jsx", "tsx", "html", "htm", "css", "scss",
        "json", "xml", "yaml", "yml", "toml", "ini", "conf", "cfg",
        "sh", "bash", "zsh", "fish", "bat", "cmd", "ps1",
        "java", "kt", "kts", "rs", "go", "rb", "php", "lua", "sql",
        "cmake", "mak", "makefile"
    };
    QFileInfo fi(m_currentFilePath);
    QString ext = fi.suffix().toLower();
    if (codeExts.contains(ext)) return true;
    if (fi.fileName().compare("Makefile", Qt::CaseInsensitive) == 0 ||
        fi.fileName().compare("CMakeLists.txt", Qt::CaseInsensitive) == 0) {
        return true;
    }
    return false;
}

// --- Search & Replace ---
bool TextViewer::findNext(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (text.isEmpty()) return false;

    QTextDocument::FindFlags flags;
    if (matchCase) flags |= QTextDocument::FindCaseSensitively;

    bool found = find(text, flags);
    if (!found) {
        QTextCursor cur = textCursor();
        cur.movePosition(QTextCursor::Start);
        setTextCursor(cur);
        found = find(text, flags);
        if (found && wrapped) {
            *wrapped = true;
        }
    }
    return found;
}

bool TextViewer::findPrev(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (text.isEmpty()) return false;

    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (matchCase) flags |= QTextDocument::FindCaseSensitively;

    bool found = find(text, flags);
    if (!found) {
        QTextCursor cur = textCursor();
        cur.movePosition(QTextCursor::End);
        setTextCursor(cur);
        found = find(text, flags);
        if (found && wrapped) {
            *wrapped = true;
        }
    }
    return found;
}

bool TextViewer::replaceNext(const QString &findText, const QString &replaceText, bool matchCase)
{
    if (!ensureEditable()) return false;
    if (findText.isEmpty()) return false;

    QTextCursor cur = textCursor();
    bool isMatch = cur.hasSelection() &&
        (matchCase ? (cur.selectedText() == findText)
                   : (cur.selectedText().compare(findText, Qt::CaseInsensitive) == 0));

    if (isMatch) {
        cur.insertText(replaceText);
        setTextCursor(cur);
        findNext(findText, matchCase);
        return true;
    }

    if (findNext(findText, matchCase)) {
        cur = textCursor();
        cur.insertText(replaceText);
        setTextCursor(cur);
        findNext(findText, matchCase);
        return true;
    }
    return false;
}

int TextViewer::replaceAll(const QString &findText, const QString &replaceText, bool matchCase)
{
    if (!ensureEditable()) return 0;
    if (findText.isEmpty()) return 0;

    QTextDocument::FindFlags flags;
    if (matchCase) flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cur(document());
    cur.beginEditBlock();
    int count = 0;
    while (!cur.isNull() && !cur.atEnd()) {
        cur = document()->find(findText, cur, flags);
        if (!cur.isNull()) {
            cur.insertText(replaceText);
            ++count;
        }
    }
    cur.endEditBlock();
    return count;
}

// --- Line Number Gutter ---
int TextViewer::lineNumberAreaWidth()
{
    if (!m_lineNumbersVisible) return 0;
    int digits = 1;
    int maxLines = qMax(1, blockCount());
    while (maxLines >= 10) {
        maxLines /= 10;
        ++digits;
    }
    int space = fontMetrics().horizontalAdvance(QLatin1Char('9')) * qMax(3, digits) + 14;
    return space;
}

void TextViewer::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextViewer::updateLineNumberArea(const QRect &rect, int dy)
{
    if (!m_lineNumbersVisible) return;
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void TextViewer::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!m_lineNumbersVisible) return;
    QPainter painter(m_lineNumberArea);
    bool isDark = (ThemeManager::currentPreset() != ThemePreset::SystemNative);
    painter.fillRect(event->rect(), isDark ? QColor("#181818") : palette().color(QPalette::Window));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    painter.setPen(isDark ? QColor("#6c7086") : QColor("#888888"));

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void TextViewer::setLineNumbersVisible(bool visible)
{
    if (m_lineNumbersVisible != visible) {
        m_lineNumbersVisible = visible;
        m_lineNumberArea->setVisible(visible);
        updateLineNumberAreaWidth(0);
        emit visualOptionsChanged();
    }
}

// --- Syntax Highlighting ---
void TextViewer::setSyntaxHighlightingEnabled(bool enabled)
{
    if (m_highlighter) {
        m_highlighter->setHighlightingEnabled(enabled);
        emit visualOptionsChanged();
    }
}

bool TextViewer::isSyntaxHighlightingEnabled() const
{
    return m_highlighter ? m_highlighter->isHighlightingEnabled() : false;
}

// --- Visual Options ---
void TextViewer::setWordWrapEnabled(bool enabled)
{
    setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    emit visualOptionsChanged();
}

void TextViewer::toggleWordWrap()
{
    setWordWrapEnabled(!isWordWrapEnabled());
}

void TextViewer::setShowWhitespace(bool show)
{
    if (m_showWhitespace != show) {
        m_showWhitespace = show;
        QTextOption opt = document()->defaultTextOption();
        if (show) {
            opt.setFlags(opt.flags() | QTextOption::ShowTabsAndSpaces);
        } else {
            opt.setFlags(opt.flags() & ~QTextOption::ShowTabsAndSpaces);
        }
        document()->setDefaultTextOption(opt);
        emit visualOptionsChanged();
    }
}

void TextViewer::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    bool isDark = (ThemeManager::currentPreset() != ThemePreset::SystemNative);
    QColor lineColor = isDark ? QColor(45, 52, 68, 140) : QColor(230, 240, 255, 180);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

// --- Font Zoom ---
void TextViewer::zoomInText(int range)
{
    QFont f = font();
    int newSize = qBound(6, f.pointSize() + range, 48);
    f.setPointSize(newSize);
    setFont(f);
    updateLineNumberAreaWidth(0);
    emit visualOptionsChanged();
}

void TextViewer::zoomOutText(int range)
{
    zoomInText(-range);
}

void TextViewer::resetTextZoom()
{
    QFont f = font();
    f.setPointSize(m_basePointSize);
    setFont(f);
    updateLineNumberAreaWidth(0);
    emit visualOptionsChanged();
}

// --- Navigation & Smart Editing ---
void TextViewer::gotoLine(int lineNum)
{
    QTextBlock block = document()->findBlockByNumber(qMax(0, lineNum - 1));
    if (block.isValid()) {
        QTextCursor cur = textCursor();
        cur.setPosition(block.position());
        setTextCursor(cur);
        centerCursor();
    }
}

void TextViewer::gotoLineDialog()
{
    int total = blockCount();
    int current = textCursor().blockNumber() + 1;
    bool ok = false;
    int line = QInputDialog::getInt(this->window(), tr("Go to Line"),
                                    tr("Line number (1 - %1):").arg(total),
                                    current, 1, qMax(1, total), 1, &ok);
    if (ok) {
        gotoLine(line);
    }
}

void TextViewer::duplicateCurrentLine()
{
    if (!ensureEditable()) return;

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        int end = cursor.selectionEnd();
        cursor.setPosition(end);
        cursor.insertText(text);
    } else {
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n" + line);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void TextViewer::deleteCurrentLine()
{
    if (!ensureEditable()) return;

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
    if (!cursor.hasSelection()) {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void TextViewer::toggleComment()
{
    if (!ensureEditable()) return;

    // Detect comment token: # for python/shell/yaml, // for others
    QString commentPrefix = "// ";
    QString ext = QFileInfo(m_currentFilePath).suffix().toLower();
    if (ext == "py" || ext == "sh" || ext == "bash" || ext == "zsh" ||
        ext == "yaml" || ext == "yml" || ext == "conf" || ext == "ini") {
        commentPrefix = "# ";
    }

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();

    // Determine if all selected blocks are already commented
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock b = document()->findBlockByNumber(i);
        QString text = b.text().trimmed();
        if (!text.isEmpty() && !text.startsWith(commentPrefix.trimmed())) {
            allCommented = false;
            break;
        }
    }

    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock b = document()->findBlockByNumber(i);
        QTextCursor lineCursor(b);
        QString text = b.text();
        if (text.trimmed().isEmpty()) continue;

        if (allCommented) {
            // Remove comment
            int idx = text.indexOf(commentPrefix);
            if (idx != -1) {
                lineCursor.setPosition(b.position() + idx);
                lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, commentPrefix.length());
                lineCursor.removeSelectedText();
            } else {
                idx = text.indexOf(commentPrefix.trimmed());
                if (idx != -1) {
                    lineCursor.setPosition(b.position() + idx);
                    lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, commentPrefix.trimmed().length());
                    lineCursor.removeSelectedText();
                }
            }
        } else {
            // Add comment at start of block
            lineCursor.setPosition(b.position());
            lineCursor.insertText(commentPrefix);
        }
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
}

// --- Key & Event Handling ---
bool TextViewer::isModifyingKey(QKeyEvent *event) const
{
    int key = event->key();

    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Meta ||
        key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_CapsLock ||
        key == Qt::Key_NumLock || key == Qt::Key_ScrollLock) {
        return false;
    }

    Qt::KeyboardModifiers mods = event->modifiers();

    // Ctrl shortcuts: Cut (X) and Paste (V) modify content
    if (mods & Qt::ControlModifier) {
        return (key == Qt::Key_V || key == Qt::Key_X);
    }

    if (mods & (Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }

    if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right ||
        key == Qt::Key_PageUp || key == Qt::Key_PageDown || key == Qt::Key_Home || key == Qt::Key_End ||
        key == Qt::Key_Escape || (key >= Qt::Key_F1 && key <= Qt::Key_F35)) {
        return false;
    }

    if (key == Qt::Key_Backspace || key == Qt::Key_Delete || key == Qt::Key_Return ||
        key == Qt::Key_Enter || key == Qt::Key_Tab) {
        return true;
    }

    QString text = event->text();
    return (!text.isEmpty() && text.at(0).isPrint());
}

void TextViewer::cut()
{
    if (ensureEditable()) {
        QPlainTextEdit::cut();
    }
}

void TextViewer::paste()
{
    if (ensureEditable()) {
        QPlainTextEdit::paste();
    }
}

void TextViewer::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // 1. Productivity shortcuts
    if ((mods & Qt::ControlModifier) && key == Qt::Key_D) {
        duplicateCurrentLine();
        event->accept();
        return;
    }
    if ((mods & (Qt::ControlModifier | Qt::ShiftModifier)) == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_K) {
        deleteCurrentLine();
        event->accept();
        return;
    }
    if ((mods & Qt::ControlModifier) && key == Qt::Key_Slash) {
        toggleComment();
        event->accept();
        return;
    }
    if ((mods & Qt::ControlModifier) && key == Qt::Key_G) {
        gotoLineDialog();
        event->accept();
        return;
    }
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
        if (!isReadOnly() && textCursor().hasSelection()) {
            // Block indent / unindent
            QTextCursor cursor = textCursor();
            cursor.beginEditBlock();
            int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
            int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();
            for (int i = startBlock; i <= endBlock; ++i) {
                QTextBlock b = document()->findBlockByNumber(i);
                QTextCursor lineCursor(b);
                if (key == Qt::Key_Tab) {
                    lineCursor.setPosition(b.position());
                    lineCursor.insertText("    "); // 4 spaces
                } else {
                    QString text = b.text();
                    int spacesToRemove = 0;
                    for (int s = 0; s < qMin(4, text.length()); ++s) {
                        if (text.at(s) == ' ') ++spacesToRemove;
                        else if (text.at(s) == '\t') { spacesToRemove = 1; break; }
                        else break;
                    }
                    if (spacesToRemove > 0) {
                        lineCursor.setPosition(b.position());
                        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, spacesToRemove);
                        lineCursor.removeSelectedText();
                    }
                }
            }
            cursor.endEditBlock();
            event->accept();
            return;
        }
    }

    // 2. Read-only guard with prompt on modifying key
    if (isReadOnly()) {
        if (isModifyingKey(event)) {
            if (ensureEditable()) {
                QPlainTextEdit::keyPressEvent(event);
            } else {
                event->accept(); // Swallow to prevent triggering window-level shortcuts
            }
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void TextViewer::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    if (m_lineNumberArea) {
        m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }
}

void TextViewer::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomInText(1);
        } else if (event->angleDelta().y() < 0) {
            zoomOutText(1);
        }
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}
