#include "search_bar_widget.h"
#include <QKeyEvent>
#include <QIcon>

SearchBarWidget::SearchBarWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_findRowWidget(nullptr)
    , m_replaceRowWidget(nullptr)
    , m_lineEdit(nullptr)
    , m_btnPrev(nullptr)
    , m_btnNext(nullptr)
    , m_chkCase(nullptr)
    , m_btnToggleReplace(nullptr)
    , m_statusLabel(nullptr)
    , m_btnClose(nullptr)
    , m_replaceEdit(nullptr)
    , m_btnReplace(nullptr)
    , m_btnReplaceAll(nullptr)
{
    setupUi();
    hide();
}

void SearchBarWidget::setupUi()
{
    setStyleSheet(
        "SearchBarWidget {"
        "   background-color: palette(window);"
        "   border: 1px solid palette(mid);"
        "   border-radius: 8px;"
        "}"
        "QLineEdit {"
        "   background-color: palette(base);"
        "   color: palette(text);"
        "   border: 1px solid palette(mid);"
        "   border-radius: 4px;"
        "   padding: 2px 6px;"
        "   font-size: 11px;"
        "}"
        "QPushButton {"
        "   background-color: palette(button);"
        "   color: palette(button-text);"
        "   border: 1px solid palette(mid);"
        "   border-radius: 4px;"
        "   padding: 2px 6px;"
        "   font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "   background-color: palette(highlight);"
        "   color: palette(highlighted-text);"
        "}"
        "QCheckBox {"
        "   color: palette(window-text);"
        "   font-size: 10px;"
        "}"
    );

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 4, 6, 4);
    m_mainLayout->setSpacing(3);

    // --- Find Row ---
    m_findRowWidget = new QWidget(this);
    QHBoxLayout *findLay = new QHBoxLayout(m_findRowWidget);
    findLay->setContentsMargins(0, 0, 0, 0);
    findLay->setSpacing(4);

    m_btnToggleReplace = new QPushButton("▸", m_findRowWidget);
    m_btnToggleReplace->setToolTip(tr("Toggle Replace (Ctrl+H)"));
    m_btnToggleReplace->setCursor(Qt::PointingHandCursor);
    m_btnToggleReplace->setFixedSize(20, 22);
    findLay->addWidget(m_btnToggleReplace);

    m_lineEdit = new QLineEdit(m_findRowWidget);
    m_lineEdit->setPlaceholderText(tr("Find..."));
    m_lineEdit->setClearButtonEnabled(true);
    m_lineEdit->setMinimumWidth(130);
    findLay->addWidget(m_lineEdit, 1);

    m_statusLabel = new QLabel(m_findRowWidget);
    m_statusLabel->setStyleSheet("font-size: 10px; padding-left: 2px; padding-right: 2px;");
    findLay->addWidget(m_statusLabel);

    m_btnPrev = new QPushButton("▲", m_findRowWidget);
    m_btnPrev->setToolTip(tr("Previous (Shift+Enter)"));
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    m_btnPrev->setFixedSize(24, 24);
    findLay->addWidget(m_btnPrev);

    m_btnNext = new QPushButton("▼", m_findRowWidget);
    m_btnNext->setToolTip(tr("Next (Enter)"));
    m_btnNext->setCursor(Qt::PointingHandCursor);
    m_btnNext->setFixedSize(24, 24);
    findLay->addWidget(m_btnNext);

    m_chkCase = new QCheckBox(tr("Aa"), m_findRowWidget);
    m_chkCase->setToolTip(tr("Match Case"));
    m_chkCase->setCursor(Qt::PointingHandCursor);
    findLay->addWidget(m_chkCase);

    m_btnClose = new QPushButton("✕", m_findRowWidget);
    m_btnClose->setToolTip(tr("Close search (Esc)"));
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setFixedSize(22, 22);
    findLay->addWidget(m_btnClose);

    m_mainLayout->addWidget(m_findRowWidget);

    // --- Replace Row ---
    m_replaceRowWidget = new QWidget(this);
    QHBoxLayout *repLay = new QHBoxLayout(m_replaceRowWidget);
    repLay->setContentsMargins(0, 0, 0, 0);
    repLay->setSpacing(4);

    // Indent spacer matching toggle button width
    QLabel *spacer = new QLabel("  ", m_replaceRowWidget);
    spacer->setFixedWidth(20);
    repLay->addWidget(spacer);

    m_replaceEdit = new QLineEdit(m_replaceRowWidget);
    m_replaceEdit->setPlaceholderText(tr("Replace with..."));
    m_replaceEdit->setClearButtonEnabled(true);
    m_replaceEdit->setMinimumWidth(130);
    repLay->addWidget(m_replaceEdit, 1);

    m_btnReplace = new QPushButton(tr("Replace"), m_replaceRowWidget);
    m_btnReplace->setToolTip(tr("Replace next match"));
    m_btnReplace->setCursor(Qt::PointingHandCursor);
    m_btnReplace->setFixedHeight(24);
    repLay->addWidget(m_btnReplace);

    m_btnReplaceAll = new QPushButton(tr("Replace All"), m_replaceRowWidget);
    m_btnReplaceAll->setToolTip(tr("Replace all occurrences"));
    m_btnReplaceAll->setCursor(Qt::PointingHandCursor);
    m_btnReplaceAll->setFixedHeight(24);
    repLay->addWidget(m_btnReplaceAll);

    m_replaceRowWidget->hide();
    m_mainLayout->addWidget(m_replaceRowWidget);

    updateWidgetSize();

    // --- Connections ---
    connect(m_btnToggleReplace, &QPushButton::clicked, this, [this]() {
        setReplaceVisible(!isReplaceVisible());
    });

    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        emit findNextRequested(m_lineEdit->text(), m_chkCase->isChecked());
    });

    connect(m_lineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_statusLabel->clear();
        emit textChanged(text);
        if (!text.isEmpty()) {
            emit findNextRequested(text, m_chkCase->isChecked());
        }
    });

    connect(m_btnNext, &QPushButton::clicked, this, [this]() {
        emit findNextRequested(m_lineEdit->text(), m_chkCase->isChecked());
    });

    connect(m_btnPrev, &QPushButton::clicked, this, [this]() {
        emit findPreviousRequested(m_lineEdit->text(), m_chkCase->isChecked());
    });

    connect(m_replaceEdit, &QLineEdit::returnPressed, this, [this]() {
        emit replaceRequested(m_lineEdit->text(), m_replaceEdit->text(), m_chkCase->isChecked());
    });

    connect(m_btnReplace, &QPushButton::clicked, this, [this]() {
        emit replaceRequested(m_lineEdit->text(), m_replaceEdit->text(), m_chkCase->isChecked());
    });

    connect(m_btnReplaceAll, &QPushButton::clicked, this, [this]() {
        emit replaceAllRequested(m_lineEdit->text(), m_replaceEdit->text(), m_chkCase->isChecked());
    });

    connect(m_btnClose, &QPushButton::clicked, this, &SearchBarWidget::hideSearch);
}

void SearchBarWidget::updateWidgetSize()
{
    if (m_replaceRowWidget && m_replaceRowWidget->isVisible()) {
        setFixedSize(410, 68);
    } else {
        setFixedSize(380, 36);
    }
}

QString SearchBarWidget::searchText() const
{
    return m_lineEdit ? m_lineEdit->text() : QString();
}

QString SearchBarWidget::replaceText() const
{
    return m_replaceEdit ? m_replaceEdit->text() : QString();
}

bool SearchBarWidget::isCaseSensitive() const
{
    return m_chkCase && m_chkCase->isChecked();
}

bool SearchBarWidget::isReplaceVisible() const
{
    return m_replaceRowWidget && m_replaceRowWidget->isVisible();
}

void SearchBarWidget::setSearchText(const QString &text)
{
    if (m_lineEdit) {
        m_lineEdit->setText(text);
        m_lineEdit->selectAll();
    }
}

void SearchBarWidget::setReplaceText(const QString &text)
{
    if (m_replaceEdit) {
        m_replaceEdit->setText(text);
        m_replaceEdit->selectAll();
    }
}

void SearchBarWidget::setStatus(const QString &status, bool isError)
{
    if (m_statusLabel) {
        m_statusLabel->setText(status);
        m_statusLabel->setStyleSheet(isError ? "color: #ef4444; font-size: 11px; font-weight: bold; padding-left: 6px;"
                                             : "color: palette(window-text); font-size: 11px; padding-left: 6px;");
    }
}

void SearchBarWidget::updateStatus(bool found, bool wrapped)
{
    if (!found) {
        setStatus(tr("Phrase not found"), true);
    } else if (wrapped) {
        setStatus(tr("Wrapped to start"), false);
    } else {
        setStatus(QString(), false);
    }
}

void SearchBarWidget::focusSearchInput()
{
    if (m_lineEdit) {
        m_lineEdit->setFocus();
        m_lineEdit->selectAll();
    }
}

void SearchBarWidget::focusReplaceInput()
{
    if (m_replaceEdit) {
        m_replaceEdit->setFocus();
        m_replaceEdit->selectAll();
    }
}

void SearchBarWidget::showAndFocus()
{
    setReplaceVisible(false);
    show();
    focusSearchInput();
}

void SearchBarWidget::showReplaceAndFocus()
{
    setReplaceVisible(true);
    show();
    if (m_lineEdit && m_lineEdit->text().isEmpty()) {
        focusSearchInput();
    } else {
        focusReplaceInput();
    }
}

void SearchBarWidget::setReplaceVisible(bool visible)
{
    if (m_replaceRowWidget) {
        m_replaceRowWidget->setVisible(visible);
        if (m_btnToggleReplace) {
            m_btnToggleReplace->setText(visible ? "▾" : "▸");
        }
        updateWidgetSize();
    }
}

void SearchBarWidget::hideSearch()
{
    hide();
    emit searchClosed();
}

void SearchBarWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hideSearch();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_replaceEdit && m_replaceEdit->hasFocus()) {
            emit replaceRequested(m_lineEdit->text(), m_replaceEdit->text(), m_chkCase->isChecked());
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier) {
            emit findPreviousRequested(m_lineEdit->text(), m_chkCase->isChecked());
        } else {
            emit findNextRequested(m_lineEdit->text(), m_chkCase->isChecked());
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
