#pragma once

#include <QDialog>
#include <QImage>
#include <QWidget>
#include <QVector>
#include <QRect>
#include "core/services/image_processor.h"

#include <QSlider>
class QSpinBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QTabWidget;
class QScrollArea;
class QButtonGroup;
class QToolButton;
class QComboBox;

/**
 * @brief Dynamic real-time RGB & Luminance histogram visualization widget.
 */
class HistogramWidget : public QWidget {
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget *parent = nullptr);
    void updateHistogram(const QImage &image);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<int> m_histR;
    QVector<int> m_histG;
    QVector<int> m_histB;
    QVector<int> m_histLuma;
    bool m_clippedShadows = false;
    bool m_clippedHighlights = false;
    int m_maxCount = 1;
};

/**
 * @brief Interactive canvas viewport supporting live rendering, split-screen (Before/After),
 * zoom, hold-to-compare, and mouse drag privacy redaction selection.
 */
class PhotoStudioViewport : public QWidget {
    Q_OBJECT

public:
    explicit PhotoStudioViewport(QWidget *parent = nullptr);

    void setOriginalImage(const QImage &image);
    void setModifiedImage(const QImage &image);
    void setHoverImage(const QImage &image);
    void clearHoverImage();

    void setSplitViewEnabled(bool enabled);
    bool isSplitViewEnabled() const { return m_splitViewEnabled; }

    void setHoldCompareActive(bool active);
    bool isHoldCompareActive() const { return m_holdCompareActive; }

    void setPrivacyToolActive(bool active);
    bool isPrivacyToolActive() const { return m_privacyToolActive; }

    void setEyedropperActive(bool active);
    bool isEyedropperActive() const { return m_eyedropperActive; }

    void setFourWayCompare(bool active,
                           const QImage &orig = QImage(),
                           const QImage &natural = QImage(),
                           const QImage &vivid = QImage(),
                           const QImage &shadows = QImage());
    bool isFourWayCompareActive() const { return m_fourWayActive; }

    QRect selectedImageRect() const;
    void clearSelection();

signals:
    void splitRatioChanged(double ratio);
    void selectionCompleted(const QRect &imageRect);
    void eyedropperSampled(const QPoint &imagePoint);
    void fourWayQuadrantSelected(int quadrantIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRect imageViewportRect() const;
    QPoint widgetToImage(const QPoint &pt) const;
    QRect widgetToImage(const QRect &rect) const;

    QImage m_originalImage;
    QImage m_modifiedImage;
    QImage m_hoverImage;

    bool m_hasHoverImage = false;
    bool m_splitViewEnabled = false;
    bool m_holdCompareActive = false;
    bool m_privacyToolActive = false;
    bool m_eyedropperActive = false;
    bool m_fourWayActive = false;

    QImage m_fourWayOrig;
    QImage m_fourWayNatural;
    QImage m_fourWayVivid;
    QImage m_fourWayShadows;

    double m_splitRatio = 0.5;
    bool m_draggingSplit = false;

    bool m_selectingRect = false;
    QPoint m_selectionStart;
    QRect m_currentSelectionRect;
};

/**
 * @brief Interactive filter card with live thumbnail preview and mouse hover detection.
 */
class FilterCardWidget : public QWidget {
    Q_OBJECT

public:
    FilterCardWidget(ImageProcessor::FilterPreset preset,
                     const QString &title,
                     const QImage &thumbnail,
                     QWidget *parent = nullptr);

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    ImageProcessor::FilterPreset preset() const { return m_preset; }
    void setThumbnail(const QImage &thumbnail) {
        m_thumbnail = thumbnail;
        update();
    }

signals:
    void clicked(ImageProcessor::FilterPreset preset);
    void hovered(ImageProcessor::FilterPreset preset);
    void unhovered();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    ImageProcessor::FilterPreset m_preset;
    QString m_title;
    QImage m_thumbnail;
    bool m_selected = false;
    bool m_hovered = false;
};

/**
 * @brief High-precision photographic slider with center-zero notch, bipolar active fill,
 * and double-click reset to default value.
 */
class PhotoSlider : public QSlider {
    Q_OBJECT

public:
    explicit PhotoSlider(Qt::Orientation orientation, int min, int max, int defaultVal, QWidget *parent = nullptr);

    int defaultValue() const { return m_defaultVal; }
    void setDefaultValue(int val) { m_defaultVal = val; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int m_defaultVal = 0;
    bool m_hovered = false;
};

/**
 * @brief Full-featured, modern photo studio dialog for BubezView.
 *
 * Provides real-time interactive previews, hover-over filter previews,
 * adjustable filter intensity, split-screen (Before/After), color curves,
 * exposure and tone adjustments, sharpening, vignetting, and privacy redaction.
 */
class PhotoStudioDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the photo studio dialog.
     * @param originalImage Image to edit.
     * @param parent Optional parent widget.
     */
    explicit PhotoStudioDialog(const QImage &originalImage, QWidget *parent = nullptr);
    ~PhotoStudioDialog() override;

    /**
     * @brief Returns the final processed image after user accepts the dialog.
     */
    QImage resultImage() const { return m_finalImage; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void onFilterCardClicked(ImageProcessor::FilterPreset preset);
    void onFilterCardHovered(ImageProcessor::FilterPreset preset);
    void onFilterCardUnhovered();

    void onFilterIntensityChanged(int value);
    void onManualValuesChanged();
    void onAutoEnhanceClicked();
    void onSplitViewToggled(bool checked);
    void onResetAllClicked();
    void onApplyPrivacyRedaction(const QString &mode);
    void onEyedropperToggled(bool checked);
    void onFourWayCompareToggled(bool checked);
    void onEyedropperSampled(const QPoint &pt);
    void onFourWayQuadrantSelected(int quadrantIndex);

private:
    void setupUi();
    void generateFilterThumbnails();
    void updateProcessing(bool updateHistogram = true);

    QImage m_sourceImage;
    QImage m_previewBase;      ///< Downscaled image (max 1000px) for blazing fast 60fps slider updates
    QImage m_finalImage;
    QImage m_currentFilteredBase;

    PhotoStudioViewport *m_viewport = nullptr;
    HistogramWidget *m_histogram = nullptr;

    // Filters Gallery
    QList<FilterCardWidget*> m_filterCards;
    ImageProcessor::FilterPreset m_currentPreset = ImageProcessor::FilterPreset::Original;
    PhotoSlider *m_sliderIntensity = nullptr;
    QLabel *m_lblIntensity = nullptr;

    // Light Controls
    PhotoSlider *m_sliderBrightness = nullptr;
    PhotoSlider *m_sliderContrast = nullptr;
    PhotoSlider *m_sliderHighlights = nullptr;
    PhotoSlider *m_sliderShadows = nullptr;

    // Color Controls
    PhotoSlider *m_sliderTemperature = nullptr;
    PhotoSlider *m_sliderSaturation = nullptr;
    PhotoSlider *m_sliderHue = nullptr;
    PhotoSlider *m_sliderRed = nullptr;
    PhotoSlider *m_sliderGreen = nullptr;
    PhotoSlider *m_sliderBlue = nullptr;

    // Detail Controls
    PhotoSlider *m_sliderSharpen = nullptr;
    PhotoSlider *m_sliderBlur = nullptr;
    PhotoSlider *m_sliderVignette = nullptr;
    QCheckBox *m_chkInvert = nullptr;

    // Privacy Controls
    QToolButton *m_btnPrivacyPixelate = nullptr;
    QToolButton *m_btnPrivacyBlur = nullptr;
    QLabel *m_lblPrivacyStatus = nullptr;

    // Viewport & Action Buttons
    QToolButton *m_btnSplitView = nullptr;
    QToolButton *m_btnCompare = nullptr;
    QToolButton *m_btnAutoFix = nullptr;
    QPushButton *m_btnReset = nullptr;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnApply = nullptr;

    // Auto-Fix Dedicated Section Controls
    QCheckBox *m_chkAutoFixEnable = nullptr;
    PhotoSlider *m_sliderAutoIntensity = nullptr;
    QLabel *m_lblAutoIntensity = nullptr;
    QComboBox *m_comboAutoProfile = nullptr;
    QCheckBox *m_chkProtectHighlights = nullptr;
    QCheckBox *m_chkRecoverShadows = nullptr;
    QCheckBox *m_chkNeutralizeWb = nullptr;
    QCheckBox *m_chkProtectSkin = nullptr;
    QCheckBox *m_chkDenoiseShadows = nullptr;
    QCheckBox *m_chkCenterWeighted = nullptr;
    QToolButton *m_btnEyedropper = nullptr;
    QToolButton *m_btnFourWay = nullptr;
    QLabel *m_lblEyedropperStatus = nullptr;
    QPoint m_neutralSamplePoint = QPoint(-1, -1);
    QTabWidget *m_tabs = nullptr;

    ImageProcessor::AutoEnhanceOptions currentAutoOptions() const;

    QImage m_autoEnhancedCache;
    ImageProcessor::AutoEnhanceOptions m_cachedAutoOptions;
    bool m_autoCacheValid = false;

    bool m_updatingControls = false;
};
