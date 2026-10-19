#pragma once

#include <QDialog>
#include <QStringList>
#include <QMap>
#include <QIcon>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;

/**
 * @brief Metadata describing an action available for toolbar placement.
 */
struct ToolbarActionInfo {
    QString id;       ///< Unique identifier for the action (e.g. "prev_file", "zoom_in")
    QString text;     ///< Localized display label
    QIcon icon;       ///< Associated icon
    QString tooltip;  ///< Descriptive tooltip / shortcut summary
};

/**
 * @brief KDE-style toolbar configuration dialog.
 *
 * Provides a side-by-side interface with Available Actions and Current Toolbar Actions,
 * allowing users to add, remove, reorder, insert separators, and restore defaults.
 */
class ConfigureToolbarDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the toolbar customization dialog.
     * @param allActions Map of all registered actions keyed by unique action ID.
     * @param currentActions List of action IDs currently placed on the toolbar.
     * @param defaultActions Default list of action IDs when restoring defaults.
     * @param parent Optional parent widget.
     */
    explicit ConfigureToolbarDialog(const QMap<QString, ToolbarActionInfo> &allActions,
                                   const QStringList &currentActions,
                                   const QStringList &defaultActions,
                                   QWidget *parent = nullptr);
    ~ConfigureToolbarDialog() override = default;

    /**
     * @brief Returns the updated list of action IDs for the toolbar.
     * @return Ordered list of action IDs (including "separator" entries).
     */
    QStringList selectedActionIds() const;

signals:
    /**
     * @brief Emitted when changes are applied or accepted.
     * @param actionIds The new list of toolbar action IDs.
     */
    void applied(const QStringList &actionIds);

private slots:
    void onSearchTextChanged(const QString &text);
    void onAddClicked();
    void onRemoveClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onInsertSeparatorClicked();
    void onRestoreDefaultsClicked();
    void onApplyClicked();
    void onAccepted();

private:
    void populateAvailableList();
    void populateCurrentList(const QStringList &actionIds);
    void updateButtonsState();

    QMap<QString, ToolbarActionInfo> m_allActions;
    QStringList m_defaultActions;

    QLineEdit *m_searchEdit;
    QListWidget *m_availableList;
    QListWidget *m_currentList;

    QPushButton *m_btnAdd;
    QPushButton *m_btnRemove;
    QPushButton *m_btnMoveUp;
    QPushButton *m_btnMoveDown;
    QPushButton *m_btnInsertSeparator;
    QPushButton *m_btnDefaults;
    QPushButton *m_btnApply;
};
