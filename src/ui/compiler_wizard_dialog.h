/**
 * @file compiler_wizard_dialog.h
 * @brief Interactive compiler flag configuration wizard and CPU instruction tuning tool.
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QLabel;
class QTextEdit;
class QPushButton;
class QComboBox;
class QScrollArea;

/**
 * @enum FlagSafetyLevel
 * @brief Classification level for compiler flags denoting their safety and pertinence to BubezView.
 */
enum class FlagSafetyLevel {
    Recommended,    ///< Green: High performance, zero risk, highly pertinent to BubezView
    Conditional     ///< Yellow: Situational, image/math specific, safe
};

/**
 * @struct CompilerFlagDef
 * @brief Definition of an individual compiler flag, its categorization, and educational description.
 */
struct CompilerFlagDef {
    QString flag;               ///< Command line flag (e.g. "-march=native", "-flto=auto")
    QString title;              ///< Human-readable feature name
    QString category;           ///< Functional group (Architecture, UI & C++, Images & Math)
    FlagSafetyLevel safety;     ///< Safety classification
    QString description;        ///< Clear description of what it enables in hardware and impact on BubezView
    QString requiredTier;       ///< Minimum CPU tier required ("v3", "v4", or "all")
    bool isDefaultActive;       ///< Default toggle state in recommended preset
    QString mutualGroup;        ///< Mutual exclusion group identifier (e.g. "arch")
};

/**
 * @class CompilerWizardDialog
 * @brief Dialog window allowing users to customize fine-grained compiler and microarchitecture flags.
 */
class CompilerWizardDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the Compiler Wizard dialog.
     * @param parent Optional parent widget.
     */
    explicit CompilerWizardDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CompilerWizardDialog() override;

    /**
     * @brief Builds and returns the generated CMAKE_CXX_FLAGS command-line argument string.
     * @return Generated flag string (e.g. "-march=native -flto=auto ...").
     */
    QString generatedFlags() const;

signals:
    /**
     * @brief Emitted when the user saves changes to the configuration.
     */
    void configurationSaved();

    /**
     * @brief Emitted when the user clicks the "Save & Recompile Now" action.
     * @param target Specific CMake target to build (empty for full build).
     */
    void recompileRequested(const QString &target = QString());

    /**
     * @brief Emitted when the user chooses manual plugin management in Extensions tab.
     */
    void openExtensionsTabRequested();

private slots:
    /**
     * @brief Applies a predefined flag configuration profile.
     * @param index Preset index.
     */
    void applyPreset(int index);

    /**
     * @brief Updates preview box and handles mutual exclusion whenever a flag checkbox changes.
     * @param flagIndex Index of the toggled flag.
     * @param checked New checked state.
     */
    void onFlagToggled(int flagIndex, bool checked);

    /**
     * @brief Displays an educational pop-up explaining the target compiler flag in detail.
     * @param flagIndex Index into internal flag database.
     */
    void showFlagInfo(int flagIndex);

    /**
     * @brief Saves selected flags into BuildConfig and closes dialog with Accepted result.
     */
    void onSaveClicked();

    /**
     * @brief Saves selected flags and triggers in-place recompilation with plugin prompt if needed.
     */
    void onSaveAndRecompileClicked();

    /**
     * @brief Copies generated CMAKE_CXX_FLAGS string to system clipboard.
     */
    void copyToClipboard();

private:
    /**
     * @brief Initializes the curated list of compiler flags.
     */
    void initDatabase();

    /**
     * @brief Sets up primary dialog geometry, header card, scroll area, and control buttons.
     */
    void setupUi();

    /**
     * @brief Recomputes and updates the preview text box showing current CMAKE_CXX_FLAGS.
     */
    void updatePreview();

    /**
     * @brief Checks if current flag selection benefits from plugin recompilation.
     */
    bool requiresPluginRecompilation() const;

    QList<CompilerFlagDef> m_flags;
    QList<QCheckBox*> m_checkboxes;
    bool m_isUpdatingFlags = false;

    QLabel *m_lblCpuModel = nullptr;
    QLabel *m_lblCpuTier = nullptr;
    QLabel *m_lblCpuCores = nullptr;
    QLabel *m_lblCpuCache = nullptr;
    QLabel *m_lblActiveStatus = nullptr;

    QComboBox *m_comboPreset = nullptr;
    QTextEdit *m_txtPreview = nullptr;
    QLabel *m_lblScopeHint = nullptr;
    QPushButton *m_btnCopy = nullptr;
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnSaveRecompile = nullptr;
    QPushButton *m_btnCancel = nullptr;
};
