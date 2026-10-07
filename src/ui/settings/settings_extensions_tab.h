/**
 * @file settings_extensions_tab.h
 * @brief Extensions and modular functional engines configuration page.
 */

#pragma once

#include <QWidget>

class QTabWidget;

/**
 * @class SettingsExtensionsTab
 * @brief Settings notebook tab presenting the catalog of functional engines and extensions
 *        (PDF, Archive, Media, Office), supporting one-click install/uninstall, binary vs source builds,
 *        and component diagnostics.
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

signals:
    /**
     * @brief Emitted when any module's status or configuration changes.
     */
    void pendingStateChanged();

private:
    void setupUi();
    QWidget *createCategoryPage(const QString &catFilter);

    QTabWidget *m_catTabs = nullptr;
};
