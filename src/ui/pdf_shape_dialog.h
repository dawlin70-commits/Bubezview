/**
 * @file pdf_shape_dialog.h
 * @brief Dialog for selecting and configuring geometric shapes and arrows for PDF markup.
 */

#pragma once

#include <QDialog>
#include <QColor>

class QRadioButton;
class QSpinBox;
class QPushButton;
class QCheckBox;

enum class PdfShapeType {
    Rectangle,
    Ellipse,
    Arrow,
    Line,
    Freehand
};

/**
 * @brief Dialog allowing configuration of geometric shape type, stroke color, width, and fill.
 */
class PdfShapeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfShapeDialog(QWidget *parent = nullptr);
    ~PdfShapeDialog() override;

    PdfShapeType shapeType() const;
    QColor strokeColor() const { return m_strokeColor; }
    int strokeWidth() const;
    QColor fillColor() const;

private slots:
    void onStrokeColorClicked();
    void onFillColorClicked();

private:
    void setupUi();

    QRadioButton *m_radioRect;
    QRadioButton *m_radioEllipse;
    QRadioButton *m_radioArrow;
    QRadioButton *m_radioLine;
    QRadioButton *m_radioFreehand;

    QSpinBox *m_spinWidth;
    QPushButton *m_btnStrokeColor;
    QColor m_strokeColor;

    QCheckBox *m_chkEnableFill;
    QPushButton *m_btnFillColor;
    QColor m_fillColor;

    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
};
