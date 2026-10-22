#pragma once

#include <QDialog>

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;

/**
 * @brief Modal dialog allowing the user to resize an image by exact pixels or percentage,
 * with optional aspect-ratio locking, quick presets, and interpolation quality options.
 */
class ImageResizeDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the resize dialog initialized with original image dimensions.
     * @param originalWidth Width of the image in pixels.
     * @param originalHeight Height of the image in pixels.
     * @param parent Optional parent widget.
     */
    explicit ImageResizeDialog(int originalWidth, int originalHeight, QWidget *parent = nullptr);

    /**
     * @brief Returns the chosen target width in pixels.
     */
    int targetWidth() const;

    /**
     * @brief Returns the chosen target height in pixels.
     */
    int targetHeight() const;

    /**
     * @brief Returns the chosen Qt transformation interpolation mode.
     */
    Qt::TransformationMode transformationMode() const;

private slots:
    void onPixelWidthChanged(int w);
    void onPixelHeightChanged(int h);
    void onPercentWidthChanged(double p);
    void onPercentHeightChanged(double p);
    void onKeepRatioToggled(bool keep);
    void onPresetSelected(int index);

private:
    void setupUi();
    void updateSummary();

    int m_origWidth = 1;
    int m_origHeight = 1;
    bool m_updating = false;

    QSpinBox *m_spinPixelWidth = nullptr;
    QSpinBox *m_spinPixelHeight = nullptr;
    QDoubleSpinBox *m_spinPercentWidth = nullptr;
    QDoubleSpinBox *m_spinPercentHeight = nullptr;
    QCheckBox *m_chkKeepRatio = nullptr;
    QComboBox *m_comboPresets = nullptr;
    QComboBox *m_comboQuality = nullptr;
    QLabel *m_lblSummary = nullptr;
};
