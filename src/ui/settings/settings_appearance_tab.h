/**
 * @file settings_appearance_tab.h
 * @brief Appearance configuration page for themes and transition animations.
 */

#pragma once

#include <QWidget>
#include <QAbstractButton>
#include <QPropertyAnimation>

class QComboBox;
class QLabel;
class QGroupBox;

/**
 * @class CapsuleToggleSwitch
 * @brief Modern iOS/macOS-style animated capsule toggle switch.
 */
class CapsuleToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal thumbOffset READ thumbOffset WRITE setThumbOffset)

public:
    explicit CapsuleToggleSwitch(QWidget *parent = nullptr);

    qreal thumbOffset() const { return m_thumbOffset; }
    void setThumbOffset(qreal offset);

    void setCheckedAnimated(bool checked);

    QSize sizeHint() const override { return QSize(48, 26); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_thumbOffset = 0.0;
    QPropertyAnimation *m_anim = nullptr;
};

/**
 * @class SettingsAppearanceTab
 * @brief Settings notebook tab managing interface color themes, transitions, language, and window geometry.
 */
class SettingsAppearanceTab : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the appearance settings tab.
     * @param parent Optional parent widget.
     */
    explicit SettingsAppearanceTab(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SettingsAppearanceTab() override = default;

public slots:
    /**
     * @brief Refreshes stylesheet and colors according to the active theme preset.
     */
    void updateThemeStyles();

signals:
    /**
     * @brief Emitted when the user selects a new theme preset.
     */
    void themeChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onThemeChanged(int index);
    void onTransitionChanged(int index);
    void onLanguageChanged(int index);
    void onScaleChanged(int index);
    void onIconStyleChanged(int index);

private:
    void setupUi();

    QComboBox *m_comboTheme = nullptr;
    QComboBox *m_comboTransition = nullptr;
    QComboBox *m_comboLanguage = nullptr;
    CapsuleToggleSwitch *m_switchRememberGeometry = nullptr;

    QLabel *m_lblRememberGeometry = nullptr;

    QGroupBox *m_grpTheme = nullptr;
    QGroupBox *m_grpTransition = nullptr;
    QGroupBox *m_grpLang = nullptr;
    QGroupBox *m_grpWindow = nullptr;
    QGroupBox *m_grpScale = nullptr;
    QComboBox *m_comboScale = nullptr;
    QGroupBox *m_grpIconStyle = nullptr;
    QComboBox *m_comboIconStyle = nullptr;
};
