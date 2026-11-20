/**
 * @file contextual_action_bar.h
 * @brief Contextual temporary action bar for active PDF editing tools.
 *
 * Appears directly below the primary header bar when an editing tool
 * (Text, Highlighter, Shapes, Redaction, Rubber Stamp) is active, offering
 * fast, specialized 1-click parameter adjustments without taking up side
 * panel space or squishing the document viewport.
 */

#pragma once

#include <QWidget>
#include <QFont>
#include <QColor>
#include <QComboBox>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QLabel>
#include "theme_manager.h"
#include "core/interfaces/i_pdf_viewer.h"

/**
 * @brief Operating modes for the ContextualActionBar.
 */
enum class ContextBarMode {
    Hidden,     ///< Bar is collapsed / invisible (height = 0)
    Text,       ///< Text insertion / typewriter formatting options
    Highlight,  ///< Dedicated Highlighter options (colors, opacity slider, text vs spotlight)
    Shapes,     ///< Geometric shapes (rect, arrow, circle, line, stroke, fill)
    Redact,     ///< Redaction & blackout (visual overlay vs sanitize)
    Stamp       ///< Rubber stamp bar (preset texts, colors, rotation, drag placement)
};

/**
 * @brief Temporary slide-down action bar offering contextual tool options.
 */
class ContextualActionBar : public QWidget
{
    Q_OBJECT

public:
    explicit ContextualActionBar(QWidget *parent = nullptr);
    ~ContextualActionBar() override = default;

    /**
     * @brief Returns a compact minimum size hint so opening the contextual
     *        action bar never forces the main window to expand horizontally.
     */
    QSize minimumSizeHint() const override;

    /**
     * @brief Activates the specified mode and adjusts visibility.
     * @param mode Target mode (Hidden, Text, Highlight, Shapes, Redact, Stamp).
     */
    void setMode(ContextBarMode mode);

    /**
     * @brief Returns current active mode.
     */
    ContextBarMode mode() const { return m_mode; }

    /**
     * @brief Re-styles controls to match the active theme preset.
     * @param preset Active ThemePreset.
     */
    void applyThemePreset(ThemePreset preset);

    // --- Text Mode Getters & Setters ---
    QFont textFont() const;
    void setTextFont(const QFont &font);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color);

    bool isWhiteout() const;
    void setWhiteout(bool whiteout);

    PdfTextBgMode textBgMode() const { return m_textBgMode; }
    void setTextBgMode(PdfTextBgMode mode);

    QColor textBgColor() const { return m_textBgColor; }
    void setTextBgColor(const QColor &color);

    QString textContent() const;
    void setTextContent(const QString &text);

    // --- Highlighter Mode Getters & Setters ---
    QColor highlighterColor() const { return m_highlighterColor; }
    void setHighlighterColor(const QColor &color);

    double highlighterOpacity() const;
    void setHighlighterOpacity(double opacity);

    bool isSpotlight() const;
    void setSpotlight(bool spotlight);

    // --- Shapes Mode Getters & Setters ---
    int activeShapeType() const; // 0=Rect, 1=Arrow, 2=Circle, 3=Line
    void setActiveShapeType(int shapeType);

    int strokeWidth() const;
    void setStrokeWidth(int width);

    QColor strokeColor() const { return m_strokeColor; }
    void setStrokeColor(const QColor &color);

    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor &color);

    // --- Redaction Mode Getters & Setters ---
    int redactStyle() const; // 0=Blackout, 1=Whiteout
    void setRedactStyle(int style);

    bool isRedactPermanent() const;
    void setRedactPermanent(bool permanent);

    // --- Stamp Mode Getters & Setters ---
    QString stampText() const;
    void setStampText(const QString &text);

    QColor stampColor() const { return m_stampColor; }
    void setStampColor(const QColor &color);

    qreal stampAngle() const;
    void setStampAngle(qreal angle);

    int stampStyle() const;
    void setStampStyle(int style);

    int stampSize() const;
    void setStampSize(int size);

    void setStampProperties(const QString &text, const QColor &color, qreal angle, int style, int size);
    void emitStampProperties();

    QComboBox *stampComboBox() const { return m_cmbStampPreset; }
    QComboBox *stampStyleComboBox() const { return m_cmbStampStyle; }

    // Legacy compatibility helpers
    int activeDrawingTool() const;
    void setActiveDrawingTool(int toolId);



signals:
    /**
     * @brief Emitted when any text property (font, color, whiteout) is modified.
     */
    void textPropertiesChanged(const QFont &font, const QColor &color, bool whiteout);

    /**
     * @brief Emitted when text background coverage mode or color is modified.
     */
    void textBgChanged(PdfTextBgMode mode, const QColor &color);

    /**
     * @brief Emitted when the user completes in-place text editing by clicking Done.
     */
    void textDoneRequested();

    /**
     * @brief Emitted when direct text replacement is requested from the contextual bar.
     */
    void textReplaceRequested(const QString &text, const QFont &font, const QColor &color, bool whiteout);

    /**
     * @brief Emitted when highlighter parameters (color, opacity, spotlight) change.
     */
    void highlightPropertiesChanged(const QColor &color, double opacity, bool isSpotlight);

    /**
     * @brief Emitted when shape parameters (type, width, stroke, fill) change.
     */
    void shapePropertiesChanged(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);

    /**
     * @brief Emitted when redaction parameters change.
     */
    void redactPropertiesChanged(int style, bool permanent);

    /**
     * @brief Emitted when rubber stamp parameters change.
     */
    void stampPropertiesChanged(const QString &text, const QColor &color, qreal angle, int style, int fontSize);

    /**
     * @brief Emitted when interactive rubber stamp property modification is finalized (e.g. slider released).
     */
    void stampPropertiesCommitted();

    /**
     * @brief Legacy compatibility signal for drawing property changes.
     */
    void drawingPropertiesChanged(int toolId, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);

    /**
     * @brief Emitted when the user dismisses the contextual bar.
     */
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    void setupTextPage();
    void setupHighlightPage();
    void setupShapesPage();
    void setupRedactPage();
    void setupStampPage();

    void updateThemeStyles();
    void updateColorSwatchButton(QToolButton *btn, const QColor &color);
    void openColorDialog(int colorTarget); // 0=Text, 1=Highlight, 2=Stroke, 3=Fill, 4=Stamp

    ContextBarMode m_mode = ContextBarMode::Hidden;
    ThemePreset m_currentPreset = ThemePreset::DarkSlate;

    QStackedWidget *m_pagesStack = nullptr;

    // --- 1. Text Page Controls ---
    QWidget *m_textPage = nullptr;
    QLineEdit *m_txtEditContent = nullptr;
    QPushButton *m_btnApplyText = nullptr;
    QComboBox *m_cmbFontFamily = nullptr;
    QComboBox *m_cmbFontSize = nullptr;
    QToolButton *m_btnBold = nullptr;
    QToolButton *m_btnItalic = nullptr;
    QToolButton *m_btnTextColor = nullptr;
    QComboBox *m_cmbTextBgMode = nullptr;
    QToolButton *m_btnTextBgColor = nullptr;
    QToolButton *m_btnCloseText = nullptr;
    QColor m_textColor = Qt::black;
    PdfTextBgMode m_textBgMode = PdfTextBgMode::Transparent;
    QColor m_textBgColor = Qt::white;

    // --- 2. Highlighter Page Controls ---
    QWidget *m_highlightPage = nullptr;
    QToolButton *m_btnHlColor = nullptr;
    QSlider *m_sldHlOpacity = nullptr;
    QLabel *m_lblHlOpacityVal = nullptr;
    QToolButton *m_btnCloseHighlight = nullptr;
    QColor m_highlighterColor = QColor(QStringLiteral("#fef08a")); // Warm highlighter yellow
    QList<QToolButton*> m_hlPaletteButtons;

    // --- 3. Shapes Page Controls ---
    QWidget *m_shapesPage = nullptr;
    QToolButton *m_btnShapeSelector = nullptr;
    QMenu *m_menuShapes = nullptr;
    int m_activeShapeType = 0;
    QComboBox *m_cmbShapeStrokeWidth = nullptr;
    QToolButton *m_btnShapeStrokeColor = nullptr;
    QToolButton *m_btnShapeFillColor = nullptr;
    QToolButton *m_btnCloseShapes = nullptr;
    QColor m_strokeColor = QColor(QStringLiteral("#ef4444")); // Red default
    QColor m_fillColor = Qt::transparent;

    void updateShapeSelectorButton();
    void updateStrokeColorButton();
    void updateFillColorButton();

    // --- 4. Redaction Page Controls ---
    QWidget *m_redactPage = nullptr;
    QButtonGroup *m_redactStyleGroup = nullptr;
    QToolButton *m_btnRedactBlack = nullptr;
    QToolButton *m_btnRedactWhite = nullptr;
    QButtonGroup *m_redactSecGroup = nullptr;
    QToolButton *m_btnRedactVisual = nullptr;
    QToolButton *m_btnRedactSanitize = nullptr;
    QToolButton *m_btnCloseRedact = nullptr;

    // --- 5. Rubber Stamp Page Controls ---
    QWidget *m_stampPage = nullptr;
    QComboBox *m_cmbStampPreset = nullptr;
    QComboBox *m_cmbStampStyle = nullptr;
    QButtonGroup *m_stampColorGroup = nullptr;
    QToolButton *m_btnStampRed = nullptr;
    QToolButton *m_btnStampGreen = nullptr;
    QToolButton *m_btnStampBlue = nullptr;
    QToolButton *m_btnStampPalette = nullptr;
    QSlider *m_sliderStampAngle = nullptr;
    QLabel *m_lblStampAngle = nullptr;
    QToolButton *m_btnResetAngle = nullptr;
    QSlider *m_sliderStampSize = nullptr;
    QLabel *m_lblStampSize = nullptr;
    QToolButton *m_btnResetSize = nullptr;
    QToolButton *m_btnCloseStamp = nullptr;
    QColor m_stampColor = QColor(QStringLiteral("#dc2626")); // Classic stamp red
};
