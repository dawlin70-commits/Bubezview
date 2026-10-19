/**
 * @file pdf_watermark_dialog.h
 * @brief Dialog for configuring text watermarks and stamps for PDF documents.
 */

#pragma once

#include <QDialog>
#include <QColor>

class QComboBox;
class QLineEdit;
class QSlider;
class QSpinBox;
class QPushButton;
class QLabel;

/**
 * @brief Dialog for watermark stamp customization (preset labels, custom text, angle, opacity, color, styles).
 */
class PdfWatermarkDialog : public QDialog
{
    Q_OBJECT

public:
    /** @brief Constructs the watermark and document stamp dialog. */
    explicit PdfWatermarkDialog(QWidget *parent = nullptr);
    ~PdfWatermarkDialog() override;

    /** @brief Returns the trimmed watermark stamp text. */
    QString watermarkText() const;
    /** @brief Returns the rotation angle in degrees [-180, 180]. */
    qreal angle() const;
    /** @brief Returns the watermark opacity in [0.1, 1.0]. */
    qreal opacity() const;
    /** @brief Returns the selected stamp color. */
    QColor color() const { return m_color; }
    /** @brief Returns the selected border style index (0=Rounded, 1=Double, 2=Dotted, 3=Plain). */
    int style() const;

    /** @brief Sets the current watermark text (selecting a matching preset if available). */
    void setWatermarkText(const QString &text);
    /** @brief Sets the rotation angle in degrees. */
    void setAngle(qreal angle);
    /** @brief Sets the watermark opacity in [0.1, 1.0]. */
    void setOpacity(qreal opacity);
    /** @brief Sets the watermark stamp color. */
    void setColor(const QColor &color);
    /** @brief Sets the border style index. */
    void setStyle(int style);

signals:
    /**
     * @brief Emitted in real-time when any watermark parameter changes.
     */
    void watermarkChanged(const QString &text, qreal angle, qreal opacity, const QColor &color, int style);

private slots:
    void onPresetChanged(int index);
    void onColorClicked();
    void emitChanged();

private:
    void setupUi();

    QComboBox *m_comboPresets;
    QLineEdit *m_txtCustom;
    QComboBox *m_comboStyle;
    QSpinBox *m_spinAngle;
    QSlider *m_sliderAngle;
    QSlider *m_sliderOpacity;
    QLabel *m_lblOpacityVal;
    QPushButton *m_btnColor;
    QColor m_color;

    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
};
