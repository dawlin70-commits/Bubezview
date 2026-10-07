/**
 * @file settings_extra_tab.h
 * @brief General application settings, language, diagnostics, and setup wizard page.
 */

#pragma once

#include <QWidget>

class QComboBox;
class QCheckBox;

/**
 * @class SettingsExtraTab
 * @brief Settings notebook tab managing interface language, LibreOffice engine integration,
 *        debug modes, system diagnostics, and guided onboarding.
 */
class SettingsExtraTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the general/extra settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsExtraTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsExtraTab() override = default;

signals:
    /**
     * @brief Emitted when the user completes the setup wizard and changes theme.
     */
    void themeChanged();

    /**
     * @brief Emitted when the setup wizard completes, to allow updating integration statuses.
     */
    void setupWizardCompleted();

private slots:
    void onLanguageChanged(int index);
    void onUseLibreOfficeToggled(bool checked);
    void copyDiagnosticsToClipboard();
    void launchSetupWizard();

private:
    void setupUi();

    QComboBox *m_comboLanguage = nullptr;
    QCheckBox *m_chkUseLibreOffice = nullptr;
    QCheckBox *m_chkFilmstrip = nullptr;
    QCheckBox *m_chkDebugMode = nullptr;
};
