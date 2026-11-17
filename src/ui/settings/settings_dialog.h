/**
 * @file settings_dialog.h
 * @brief Application settings and desktop preferences dialog coordinator.
 */

#pragma once

#include <QDialog>
#include <QPoint>
#include <QRect>

class QTabWidget;
class QFrame;
class QLabel;
class QPushButton;
class BuildConsoleDialog;
class QGraphicsDropShadowEffect;
class QMouseEvent;

class SettingsAppearanceTab;
class SettingsIntegrationTab;
class SettingsPerformanceTab;
class SettingsExtensionsTab;
class SettingsExtraTab;

/**
 * @class SettingsDialog
 * @brief Coordinator dialog managing themes, desktop integration, compiler tuning,
 *        modular extensions, and general preferences.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @enum TabIndex
     * @brief Identifies the functional pages available in the settings notebook.
     */
    enum class TabIndex {
        Appearance = 0,         ///< Theme and transition configuration
        DesktopIntegration = 1, ///< Dolphin, GNOME/Nemo, and desktop shortcut integrations
        Performance = 2,        ///< Compiler level (-O2/-O3), prefetching, and standby
        Extensions = 3,         ///< Interactive functional module catalog
        SystemTools = 4,        ///< System maintenance, diagnostics, cache, and integrations
        Extra = 4               ///< Backwards-compatible alias for SystemTools
    };

    /**
     * @brief Constructs the settings dialog.
     * @param parent Optional parent widget.
     * @param initialTab Active tab when the dialog is first opened.
     */
    explicit SettingsDialog(QWidget *parent = nullptr, TabIndex initialTab = TabIndex::Appearance);

    /**
     * @brief Destructor.
     */
    ~SettingsDialog() override;

    /**
     * @brief Switches the active view to the specified tab.
     * @param tab Target tab to activate.
     */
    void setCurrentTab(TabIndex tab);

    /**
     * @brief Returns the extensions tab instance.
     */
    SettingsExtensionsTab *extensionsTab() const { return m_extensionsTab; }

signals:
    /**
     * @brief Emitted when the user changes theme preset or transition style.
     */
    void themeChanged();

public slots:
    /**
     * @brief Refreshes desktop integration statuses across tabs.
     */
    void refreshStatuses();

    /**
     * @brief Initiates in-place recompilation via the floating build console.
     */
    void startRecompilation();

    /**
     * @brief Initiates in-place recompilation for a specific target.
     * @param target Specific CMake target to build (e.g. "bubezview").
     */
    void startRecompilation(const QString &target);

    /**
     * @brief Reverts pending build and module changes back to active configuration.
     */
    void onRevertPendingClicked();

    /**
     * @brief Updates theme and card styling across all tabs and dialog controls.
     */
    void updateAllThemeStyles();

private slots:
    /**
     * @brief Evaluates whether pending build changes exist and updates banner visibility.
     */
    void updatePendingBanner();

private:
    void setupUi();
    bool hasPendingBuildChanges() const;

    QTabWidget *m_tabWidget = nullptr;

    SettingsAppearanceTab *m_appearanceTab = nullptr;
    SettingsIntegrationTab *m_integrationTab = nullptr;
    SettingsPerformanceTab *m_performanceTab = nullptr;
    SettingsExtensionsTab *m_extensionsTab = nullptr;
    SettingsExtraTab *m_extraTab = nullptr;

    // Initial build config snapshot for pending changes tracking
    bool m_initialPdfStatic = false;
    bool m_initialArchiveStatic = false;
    bool m_initialOfficeStatic = false;
    bool m_initialEnableOffice = false;

    // Pending Recompile Notification Banner & Floating Console
    QFrame *m_bannerPending = nullptr;
    QLabel *m_lblBannerMsg = nullptr;
    QPushButton *m_btnBannerRecompile = nullptr;
    QPushButton *m_btnBannerRevert = nullptr;
    QPushButton *m_btnClose = nullptr;
    BuildConsoleDialog *m_buildConsoleDialog = nullptr;

    // Rounded Floating Container & Custom Header Bar
    QFrame *m_container = nullptr;
    QWidget *m_headerBar = nullptr;
    QLabel *m_lblIcon = nullptr;
    QLabel *m_lblTitle = nullptr;
    QPushButton *m_btnTitleClose = nullptr;
    QPoint m_dragStartPosition;
    QPoint m_windowStartPosition;
    bool m_isDragging = false;

    // Resizing State & Helpers
    Qt::Edges calculateResizeEdges(const QPoint &globalPos) const;
    void updateResizeCursor(Qt::Edges edges);
    void performManualResize(const QPoint &globalPos);

    bool m_isCursorOverridden = false;
    bool m_isResizing = false;
    Qt::Edges m_resizeEdges;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeom;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};
