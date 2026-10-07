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
    explicit PdfWatermarkDialog(QWidget *parent = nullptr);
    ~PdfWatermarkDialog() override;

    QString watermarkText() const;
    qreal angle() const;
    qreal opacity() const;
    QColor color() const { return m_color; }
    int style() const;

    void setWatermarkText(const QString &text);
    void setAngle(qreal angle);
    void setOpacity(qreal opacity);
    void setColor(const QColor &color);
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
