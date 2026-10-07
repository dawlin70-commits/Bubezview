#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

/**
 * @brief An in-window search and replace bar widget for finding/replacing text in documents (Text, PDF).
 *
 * Sits compactly floating in the document container.
 * Provides controls for next/previous match navigation, case-sensitivity toggling,
 * match status feedback, replace operations, and quick dismissal with the Escape key.
 */
class SearchBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBarWidget(QWidget *parent = nullptr);

    /**
     * @brief Gets the current query string in the search input field.
     * @return Query text.
     */
    QString searchText() const;

    /**
     * @brief Gets the current text in the replace field.
     * @return Replace text.
     */
    QString replaceText() const;

    /**
     * @brief Checks if the "Match Case" option is enabled.
     * @return True if search is case-sensitive; false otherwise.
     */
    bool isCaseSensitive() const;

    /**
     * @brief Checks if the replace row is currently visible.
     * @return True if replace controls are displayed.
     */
    bool isReplaceVisible() const;

    /**
     * @brief Sets the search text in the input field and selects it.
     * @param text Query text to populate.
     */
    void setSearchText(const QString &text);

    /**
     * @brief Sets the replacement text in the replace field.
     * @param text Replacement text.
     */
    void setReplaceText(const QString &text);

    /**
     * @brief Displays custom status text (e.g. error or informational banner).
     * @param status Text message to display.
     * @param isError True to style with error colors; false for neutral styling.
     */
    void setStatus(const QString &status, bool isError = false);

    /**
     * @brief Automatically formats status based on search results.
     * @param found True if at least one match was found.
     * @param wrapped True if search reached the boundary and wrapped to the start/end.
     */
    void updateStatus(bool found, bool wrapped);

    /**
     * @brief Focuses the search line edit and selects all contents.
     */
    void focusSearchInput();

    /**
     * @brief Focuses the replace line edit and selects all contents.
     */
    void focusReplaceInput();

public slots:
    /**
     * @brief Shows the search bar in search-only mode and sets focus to the search line edit.
     */
    void showAndFocus();

    /**
     * @brief Shows the search bar with replace controls expanded and sets focus.
     */
    void showReplaceAndFocus();

    /**
     * @brief Toggles or sets visibility of the replace input row.
     * @param visible True to show replace row; false to collapse.
     */
    void setReplaceVisible(bool visible);

    /**
     * @brief Hides the search bar and emits searchClosed().
     */
    void hideSearch();

signals:
    /**
     * @brief Emitted when the user requests the next match (Enter or Next button).
     * @param text Query string.
     * @param caseSensitive True if case matching is enabled.
     */
    void findNextRequested(const QString &text, bool caseSensitive);

    /**
     * @brief Emitted when the user requests the previous match (Shift+Enter or Prev button).
     * @param text Query string.
     * @param caseSensitive True if case matching is enabled.
     */
    void findPreviousRequested(const QString &text, bool caseSensitive);

    /**
     * @brief Emitted when the user changes text in the search input.
     * @param text New text.
     */
    void textChanged(const QString &text);

    /**
     * @brief Emitted when user clicks Replace or presses Return in replace input.
     * @param findText The string to find.
     * @param replaceText The replacement string.
     * @param caseSensitive True if case matching is enabled.
     */
    void replaceRequested(const QString &findText, const QString &replaceText, bool caseSensitive);

    /**
     * @brief Emitted when user clicks Replace All.
     * @param findText The string to find.
     * @param replaceText The replacement string.
     * @param caseSensitive True if case matching is enabled.
     */
    void replaceAllRequested(const QString &findText, const QString &replaceText, bool caseSensitive);

    /**
     * @brief Emitted when the search bar is closed by the user (Esc or Close button).
     */
    void searchClosed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void updateWidgetSize();

    QVBoxLayout *m_mainLayout;
    QWidget *m_findRowWidget;
    QWidget *m_replaceRowWidget;

    QLineEdit *m_lineEdit;
    QPushButton *m_btnPrev;
    QPushButton *m_btnNext;
    QCheckBox *m_chkCase;
    QPushButton *m_btnToggleReplace;
    QLabel *m_statusLabel;
    QPushButton *m_btnClose;

    QLineEdit *m_replaceEdit;
    QPushButton *m_btnReplace;
    QPushButton *m_btnReplaceAll;
};
