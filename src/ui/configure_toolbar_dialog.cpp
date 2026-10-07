#include "configure_toolbar_dialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QIcon>
#include <QStyle>

ConfigureToolbarDialog::ConfigureToolbarDialog(const QMap<QString, ToolbarActionInfo> &allActions,
                                               const QStringList &currentActions,
                                               const QStringList &defaultActions,
                                               QWidget *parent)
    : QDialog(parent)
    , m_allActions(allActions)
    , m_defaultActions(defaultActions)
{
    setWindowTitle(tr("Configure Toolbars — BubezView"));
    resize(640, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Top instruction
    QLabel *headerLabel = new QLabel(tr("Customize the actions displayed on the main toolbar:"), this);
    headerLabel->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(headerLabel);

    // Columns Container
    QHBoxLayout *columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(8);

    // --- Left Column: Available Actions ---
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(6);
    QLabel *lblAvailable = new QLabel(tr("Available actions:"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter actions..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ConfigureToolbarDialog::onSearchTextChanged);

    m_availableList = new QListWidget(this);
    m_availableList->setIconSize(QSize(20, 20));
    connect(m_availableList, &QListWidget::itemDoubleClicked, this, &ConfigureToolbarDialog::onAddClicked);
    connect(m_availableList, &QListWidget::itemSelectionChanged, this, &ConfigureToolbarDialog::updateButtonsState);

    leftLayout->addWidget(lblAvailable);
    leftLayout->addWidget(m_searchEdit);
    leftLayout->addWidget(m_availableList);
    columnsLayout->addLayout(leftLayout, 1);

    // --- Center Column: Add / Remove Buttons ---
    QVBoxLayout *centerLayout = new QVBoxLayout();
    centerLayout->setSpacing(8);
    centerLayout->addStretch();

    m_btnAdd = new QPushButton(tr("Add ➔"), this);
    m_btnAdd->setIcon(QIcon::fromTheme("go-next"));
    m_btnAdd->setToolTip(tr("Add selected action to toolbar"));
    connect(m_btnAdd, &QPushButton::clicked, this, &ConfigureToolbarDialog::onAddClicked);
    centerLayout->addWidget(m_btnAdd);

    m_btnRemove = new QPushButton(tr("⬅ Remove"), this);
    m_btnRemove->setIcon(QIcon::fromTheme("go-previous"));
    m_btnRemove->setToolTip(tr("Remove selected action from toolbar"));
    connect(m_btnRemove, &QPushButton::clicked, this, &ConfigureToolbarDialog::onRemoveClicked);
    centerLayout->addWidget(m_btnRemove);

    centerLayout->addStretch();
    columnsLayout->addLayout(centerLayout);

    // --- Right Column: Current Actions ---
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(6);
    QLabel *lblCurrent = new QLabel(tr("Current toolbar actions:"), this);

    QHBoxLayout *rightContentLayout = new QHBoxLayout();
    rightContentLayout->setSpacing(6);

    m_currentList = new QListWidget(this);
    m_currentList->setIconSize(QSize(20, 20));
    connect(m_currentList, &QListWidget::itemDoubleClicked, this, &ConfigureToolbarDialog::onRemoveClicked);
    connect(m_currentList, &QListWidget::itemSelectionChanged, this, &ConfigureToolbarDialog::updateButtonsState);
    rightContentLayout->addWidget(m_currentList, 1);

    // Side buttons for reordering and separators
    QVBoxLayout *reorderLayout = new QVBoxLayout();
    reorderLayout->setSpacing(6);

    m_btnMoveUp = new QPushButton("▲", this);
    m_btnMoveUp->setToolTip(tr("Move action up"));
    connect(m_btnMoveUp, &QPushButton::clicked, this, &ConfigureToolbarDialog::onMoveUpClicked);
    reorderLayout->addWidget(m_btnMoveUp);

    m_btnMoveDown = new QPushButton("▼", this);
    m_btnMoveDown->setToolTip(tr("Move action down"));
    connect(m_btnMoveDown, &QPushButton::clicked, this, &ConfigureToolbarDialog::onMoveDownClicked);
    reorderLayout->addWidget(m_btnMoveDown);

    reorderLayout->addSpacing(12);

    m_btnInsertSeparator = new QPushButton(tr("+ Separator"), this);
    m_btnInsertSeparator->setToolTip(tr("Insert a separator after the selected action"));
    connect(m_btnInsertSeparator, &QPushButton::clicked, this, &ConfigureToolbarDialog::onInsertSeparatorClicked);
    reorderLayout->addWidget(m_btnInsertSeparator);

    reorderLayout->addStretch();
    rightContentLayout->addLayout(reorderLayout);

    rightLayout->addWidget(lblCurrent);
    rightLayout->addLayout(rightContentLayout);
    columnsLayout->addLayout(rightLayout, 1);

    mainLayout->addLayout(columnsLayout, 1);

    // --- Bottom Controls ---
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    m_btnDefaults = new QPushButton(tr("Defaults"), this);
    m_btnDefaults->setIcon(QIcon::fromTheme("edit-undo"));
    m_btnDefaults->setToolTip(tr("Reset toolbar to standard default actions"));
    connect(m_btnDefaults, &QPushButton::clicked, this, &ConfigureToolbarDialog::onRestoreDefaultsClicked);
    bottomLayout->addWidget(m_btnDefaults);

    bottomLayout->addStretch();

    m_btnApply = new QPushButton(tr("Apply"), this);
    connect(m_btnApply, &QPushButton::clicked, this, &ConfigureToolbarDialog::onApplyClicked);
    bottomLayout->addWidget(m_btnApply);

    QPushButton *btnOk = new QPushButton(tr("OK"), this);
    btnOk->setDefault(true);
    connect(btnOk, &QPushButton::clicked, this, &ConfigureToolbarDialog::onAccepted);
    bottomLayout->addWidget(btnOk);

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnCancel);

    mainLayout->addLayout(bottomLayout);

    // Initial population
    populateAvailableList();
    populateCurrentList(currentActions);
    updateButtonsState();
}

void ConfigureToolbarDialog::populateAvailableList()
{
    m_availableList->clear();
    for (auto it = m_allActions.constBegin(); it != m_allActions.constEnd(); ++it) {
        const ToolbarActionInfo &info = it.value();
        QListWidgetItem *item = new QListWidgetItem(info.icon, info.text, m_availableList);
        item->setData(Qt::UserRole, info.id);
        item->setToolTip(info.tooltip);
    }
}

void ConfigureToolbarDialog::populateCurrentList(const QStringList &actionIds)
{
    m_currentList->clear();
    for (const QString &id : actionIds) {
        if (id == "separator") {
            QListWidgetItem *item = new QListWidgetItem(tr("--- separator ---"), m_currentList);
            item->setData(Qt::UserRole, "separator");
            item->setForeground(QBrush(QColor(128, 128, 128)));
        } else if (m_allActions.contains(id)) {
            const ToolbarActionInfo &info = m_allActions[id];
            QListWidgetItem *item = new QListWidgetItem(info.icon, info.text, m_currentList);
            item->setData(Qt::UserRole, info.id);
            item->setToolTip(info.tooltip);
        }
    }
}

void ConfigureToolbarDialog::onSearchTextChanged(const QString &text)
{
    QString query = text.trimmed();
    for (int i = 0; i < m_availableList->count(); ++i) {
        QListWidgetItem *item = m_availableList->item(i);
        bool match = query.isEmpty() || item->text().contains(query, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void ConfigureToolbarDialog::onAddClicked()
{
    QListWidgetItem *curAvailable = m_availableList->currentItem();
    if (!curAvailable) return;

    QString actionId = curAvailable->data(Qt::UserRole).toString();
    if (!m_allActions.contains(actionId)) return;

    const ToolbarActionInfo &info = m_allActions[actionId];
    int insertRow = m_currentList->currentRow();
    if (insertRow < 0) {
        insertRow = m_currentList->count();
    } else {
        insertRow += 1;
    }

    QListWidgetItem *newItem = new QListWidgetItem(info.icon, info.text);
    newItem->setData(Qt::UserRole, info.id);
    newItem->setToolTip(info.tooltip);
    m_currentList->insertItem(insertRow, newItem);
    m_currentList->setCurrentRow(insertRow);
    updateButtonsState();
}

void ConfigureToolbarDialog::onRemoveClicked()
{
    int row = m_currentList->currentRow();
    if (row < 0) return;

    delete m_currentList->takeItem(row);
    if (row < m_currentList->count()) {
        m_currentList->setCurrentRow(row);
    } else if (m_currentList->count() > 0) {
        m_currentList->setCurrentRow(m_currentList->count() - 1);
    }
    updateButtonsState();
}

void ConfigureToolbarDialog::onMoveUpClicked()
{
    int row = m_currentList->currentRow();
    if (row <= 0) return;

    QListWidgetItem *item = m_currentList->takeItem(row);
    m_currentList->insertItem(row - 1, item);
    m_currentList->setCurrentRow(row - 1);
    updateButtonsState();
}

void ConfigureToolbarDialog::onMoveDownClicked()
{
    int row = m_currentList->currentRow();
    if (row < 0 || row >= m_currentList->count() - 1) return;

    QListWidgetItem *item = m_currentList->takeItem(row);
    m_currentList->insertItem(row + 1, item);
    m_currentList->setCurrentRow(row + 1);
    updateButtonsState();
}

void ConfigureToolbarDialog::onInsertSeparatorClicked()
{
    int insertRow = m_currentList->currentRow();
    if (insertRow < 0) {
        insertRow = m_currentList->count();
    } else {
        insertRow += 1;
    }

    QListWidgetItem *sepItem = new QListWidgetItem(tr("--- separator ---"));
    sepItem->setData(Qt::UserRole, "separator");
    sepItem->setForeground(QBrush(QColor(128, 128, 128)));
    m_currentList->insertItem(insertRow, sepItem);
    m_currentList->setCurrentRow(insertRow);
    updateButtonsState();
}

void ConfigureToolbarDialog::onRestoreDefaultsClicked()
{
    populateCurrentList(m_defaultActions);
    updateButtonsState();
}

void ConfigureToolbarDialog::onApplyClicked()
{
    emit applied(selectedActionIds());
}

void ConfigureToolbarDialog::onAccepted()
{
    emit applied(selectedActionIds());
    accept();
}

QStringList ConfigureToolbarDialog::selectedActionIds() const
{
    QStringList ids;
    for (int i = 0; i < m_currentList->count(); ++i) {
        ids.append(m_currentList->item(i)->data(Qt::UserRole).toString());
    }
    return ids;
}

void ConfigureToolbarDialog::updateButtonsState()
{
    m_btnAdd->setEnabled(m_availableList->currentItem() != nullptr);

    int curRow = m_currentList->currentRow();
    bool hasCur = (curRow >= 0);
    m_btnRemove->setEnabled(hasCur);
    m_btnMoveUp->setEnabled(hasCur && curRow > 0);
    m_btnMoveDown->setEnabled(hasCur && curRow < m_currentList->count() - 1);
}
