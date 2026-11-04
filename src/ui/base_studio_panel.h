#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include "vector_icons.h"
#include "core/services/theme_manager.h"

/**
 * @brief Reusable, theme-adaptive base panel shell for all BubezView media editing studios.
 *
 * Provides a standardized visual scaffolding consisting of:
 * - Header bar: Custom icon, localized title, and standard action buttons (Compare, Reset, Close).
 * - Segmented capsule tab bar (StudioTabBar): Dynamically displays and activates registered pages.
 * - Central content area: QStackedWidget hosting custom format-specific studio pages.
 * - Footer bar: Standard Cancel and Apply Changes action buttons.
 * - Automated Theme Engine: High-contrast Dark & Light theme adaptation and modern rounded scrollbars.
 */
class BaseStudioPanel : public QWidget {
    Q_OBJECT

public:
    explicit BaseStudioPanel(QWidget *parent = nullptr);
    ~BaseStudioPanel() override = default;

    /**
     * @brief Configures the title text displayed in the header bar.
     * @param title Localized title string.
     */
    void setHeaderTitle(const QString &title);

    /**
     * @brief Configures the icon displayed next to the title in the header bar.
     * @param icon Vector icon enum identifier.
     * @param accentColor Optional accent tint color.
     */
    void setHeaderIcon(VectorIcons::UiIcon icon, const QColor &accentColor = QColor());

    /**
     * @brief Registers a content page with the panel, adding a corresponding capsule tab button.
     * @param title Localized title for the capsule tab button.
     * @param pageWidget The widget containing the page's controls.
     * @return The 0-based index of the newly added page.
     */
    int addTabPage(const QString &title, QWidget *pageWidget);

    /**
     * @brief Activates the page at the given index.
     * @param index 0-based tab index.
     */
    void setCurrentTab(int index);

    /**
     * @brief Returns the index of the currently active page.
     * @return 0-based tab index.
     */
    int currentTabIndex() const;

    /**
     * @brief Shows or hides the hold-to-compare button in the header bar.
     * @param visible True to show; false to hide.
     */
    void setCompareButtonVisible(bool visible);

    /**
     * @brief Shows or hides the reset button in the header bar.
     * @param visible True to show; false to hide.
     */
    void setResetButtonVisible(bool visible);

    /**
     * @brief Updates the label on the primary apply button in the footer.
     * @param text Localized button label (defaults to tr("Apply Changes")).
     */
    void setApplyButtonText(const QString &text);

    /**
     * @brief Enables or disables the primary apply button.
     * @param enabled True to enable.
     */
    void setApplyButtonEnabled(bool enabled);

    /**
     * @brief Creates a standardized, frameless, transparent QScrollArea hosting the given container.
     * @param content Container widget holding the controls.
     * @param parent Optional parent widget.
     * @return Ready-to-use QScrollArea configured with horizontal scrollbar disabled and transparent viewport.
     */
    static QScrollArea *createStandardScrollArea(QWidget *content, QWidget *parent = nullptr);

    /**
     * @brief Updates colors, palettes, icons, and scrollbars according to the active theme preset.
     */
    virtual void updateTheme();

signals:
    /**
     * @brief Emitted when the user toggles or holds the compare button.
     * @param showOriginal True when holding to view untouched original media.
     */
    void compareToggled(bool showOriginal);

    /**
     * @brief Emitted when the user clicks the reset button in the header.
     */
    void resetRequested();

    /**
     * @brief Emitted when the user clicks the close (X) button in the header.
     */
    void closeRequested();

    /**
     * @brief Emitted when the user clicks Cancel in the footer.
     */
    void cancelRequested();

    /**
     * @brief Emitted when the user clicks Apply Changes in the footer.
     */
    void applyRequested();

    /**
     * @brief Emitted when the user switches between tab pages.
     * @param index 0-based index of the new page.
     */
    void tabChanged(int index);

protected:
    /**
     * @brief Paints the widget's styled background to support Qt style sheet rendering.
     * @param event Paint event.
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief Consumes mouse wheel events to prevent them from bubbling to underlying document/media viewer.
     * @param event Wheel event.
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Optional hook for derived studio panels to customize icons or append custom QSS rules.
     * @param qss Stylesheet string to append rules to.
     * @param isDark True if the active theme preset is dark.
     */
    virtual void customizeTheme(QString &qss, bool isDark);

    // Sub-widgets accessible to derived classes
    QLabel *m_headerIconLabel = nullptr;
    QLabel *m_lblTitle = nullptr;
    QToolButton *m_btnCompare = nullptr;
    QToolButton *m_btnReset = nullptr;
    QToolButton *m_btnClose = nullptr;

    QWidget *m_tabBarWidget = nullptr;
    QHBoxLayout *m_tabLayout = nullptr;
    QList<QPushButton*> m_tabButtons;
    QStackedWidget *m_stackedWidget = nullptr;

    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnApply = nullptr;

private slots:
    void onTabButtonClicked(int index);

private:
    void setupBaseUi();
    VectorIcons::UiIcon m_currentHeaderIcon = VectorIcons::UiIcon::Palette;
    QColor m_currentHeaderIconAccent;
};
