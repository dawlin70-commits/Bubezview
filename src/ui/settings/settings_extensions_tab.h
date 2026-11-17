/**
 * @file settings_extensions_tab.h
 * @brief Extensions and modular functional engines configuration page.
 */

#pragma once

#include <QWidget>
#include <QStringList>
#include <QMap>

class QPushButton;
class QGroupBox;
class QGridLayout;
class QFrame;
class ExtensionRowWidget;

/**
 * @class SettingsExtensionsTab
 * @brief Settings notebook tab presenting the catalog of functional engines and extensions
 *        (PDF, Archive, Media, Office, Graphics), supporting 2-column Liquid Glass tile layout,
 *        compact segmented category capsules, and direct 3-button control per component.
 */
class SettingsExtensionsTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the extensions catalog tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsExtensionsTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsExtensionsTab() override = default;

    /**
     * @brief Refreshes all extension row widgets across category tabs.
     */
    void refreshAllRows();

    /**
     * @brief Switches the active category capsule and populates its grid.
     */
    void selectCategory(const QString &catId);

    /**
     * @brief Updates category pill tabs and card styling to match the active theme.
     */
    void updateThemeStyles();

signals:
    /**
     * @brief Emitted when any module's status or configuration changes.
     */
    void pendingStateChanged();

private slots:
    void runGlobalDiagnostics();
    void onCategoryCapsuleClicked(const QString &catId);

private:
    void setupUi();
    void populateCategoryGrid(const QString &catId);
    void updateCapsuleSelection();

    QString m_activeCategory;

    // Header controls
    QFrame *m_categoryCapsulesFrame = nullptr;
    QMap<QString, QPushButton*> m_catButtons;
    QPushButton *m_btnGlobalDiag = nullptr;

    // Active Category Tiles Card (2 columns)
    QGroupBox *m_grpActiveCategory = nullptr;
    QGridLayout *m_gridLayout = nullptr;
};
