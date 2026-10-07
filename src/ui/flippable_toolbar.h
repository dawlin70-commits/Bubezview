/**
 * @file flippable_toolbar.h
 * @brief Container widget providing a 400ms 3D vertical card-flip between Main Toolbar and Annotation Toolbars.
 *
 * Implements a card-flip animation around the X-axis:
 * - When user activates Highlighter or Shapes & Arrows, the main toolbar flips 3D vertically to reveal
 *   a dedicated annotation toolstrip with zero layout shifting of the PDF canvas.
 * - Pressing Escape or clicking '✕ Exit' flips the card back to the main navigation toolbar.
 */

#pragma once

#include <QWidget>
#include <QToolBar>
#include <QVariantAnimation>
#include <QColor>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QButtonGroup;

/**
 * @brief Active mode of the flippable toolbar.
 */
enum class FlippableToolbarMode {
    Main,
    Highlighter,
    Shapes
};

/**
 * @brief 3D card-flip container managing the main toolbar and annotation toolbars.
 */
class FlippableToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit FlippableToolbar(QWidget *parent = nullptr);
    ~FlippableToolbar() override = default;

    /**
     * @brief Access the standard customizable main QToolBar.
     */
    QToolBar *mainToolBar() const { return m_mainToolBar; }

    /**
     * @brief Current active toolbar mode.
     */
    FlippableToolbarMode currentMode() const { return m_currentMode; }

    /**
     * @brief Target mode during animation or current mode when idle.
     */
    FlippableToolbarMode targetMode() const { return m_targetMode; }

    /**
     * @brief Whether the 3D flip animation is currently in progress.
     */
    bool isFlipping() const { return m_isFlipping; }

    /**
     * @brief Initiates 3D vertical card-flip to the Highlighter toolstrip.
     * @param color Initial highlighter color.
     * @param opacity Initial highlighter opacity (0.1 to 0.9).
     */
    void flipToHighlighter(const QColor &color = QColor(255, 235, 59), double opacity = 0.40);

    /**
     * @brief Initiates 3D vertical card-flip to the Shapes toolstrip.
     * @param shapeType 0: Rectangle, 1: Ellipse, 2: Arrow, 3: Line.
     * @param strokeWidth Stroke thickness.
     * @param strokeColor Outline color.
     * @param fillColor Fill color (can be transparent).
     */
    void flipToShapes(int shapeType = 0, int strokeWidth = 2,
                      const QColor &strokeColor = QColor(239, 68, 68),
                      const QColor &fillColor = QColor(239, 68, 68, 40));

    /**
     * @brief Returns current active shape type (0: Rect, 1: Ellipse, 2: Arrow, 3: Line).
     */
    int shapeType() const { return m_shapeType; }

    /**
     * @brief Initiates 3D vertical card-flip back to the standard Main Toolbar.
     */
    void flipToMain();

signals:
    /**
     * @brief Emitted when the user changes highlighter parameters.
     */
    void highlighterParamsChanged(const QColor &color, double opacity);

    /**
     * @brief Emitted when the user changes shape parameters.
     */
    void shapeParamsChanged(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);

    /**
     * @brief Emitted when the user exits annotation mode via '✕ Exit' or Escape key.
     */
    void annotationModeExited();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAnimationValueChanged(const QVariant &value);
    void onAnimationFinished();

private:
    void setupHighlighterBar();
    void setupShapesBar();
    void startFlipAnimation(FlippableToolbarMode targetMode);
    void updateHighlighterColorButtons();
    void updateShapeButtons();

    QToolBar *m_mainToolBar = nullptr;
    QWidget *m_annotationContainer = nullptr;
    QWidget *m_highlighterBar = nullptr;
    QWidget *m_shapesBar = nullptr;

    FlippableToolbarMode m_currentMode = FlippableToolbarMode::Main;
    FlippableToolbarMode m_targetMode = FlippableToolbarMode::Main;

    // Flip animation state
    QVariantAnimation *m_flipAnimation = nullptr;
    qreal m_flipAngle = 0.0;     ///< Current rotation in degrees: 0° to 180°
    bool m_isFlipping = false;
    QPixmap m_frontPixmap;
    QPixmap m_backPixmap;

    // --- Highlighter toolstrip controls ---
    QColor m_hlColor;
    double m_hlOpacity = 0.40;
    QSlider *m_hlOpacitySlider = nullptr;
    QLabel *m_hlOpacityLabel = nullptr;
    QList<QPushButton*> m_hlColorButtons;

    // --- Shapes toolstrip controls ---
    int m_shapeType = 0;
    int m_shapeStrokeWidth = 2;
    QSpinBox *m_spinShapeWidth = nullptr;
    QColor m_shapeStrokeColor;
    QColor m_shapeFillColor;
    QButtonGroup *m_shapeTypeGroup = nullptr;
    QList<QPushButton*> m_shapeStrokeColorButtons;
    QPushButton *m_btnShapeFillToggle = nullptr;
};
