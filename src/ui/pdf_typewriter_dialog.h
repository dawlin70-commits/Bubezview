/**
 * @file pdf_typewriter_dialog.h
 * @brief Interactive typewriter dialog for entering and styling text on PDF documents.
 */

#pragma once

#include <QDialog>
#include <QFont>
#include <QColor>

class QTextEdit;
class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

/**
 * @brief Dialog allowing users to type, style, and configure text to imprint onto a PDF page.
 */
class PdfTypewriterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PdfTypewriterDialog(QWidget *parent = nullptr);
    ~PdfTypewriterDialog() override;

    QString text() const;
    QFont selectedFont() const;
    QColor textColor() const { return m_currentColor; }
    bool isWhiteout() const;

    void setText(const QString &text);
    void setSelectedFont(const QFont &font);
    void setTextColor(const QColor &color);
    void setWhiteout(bool whiteout);

private slots:
    void onFontChanged();
    void onColorPresetClicked();
    void onCustomColorClicked();
    void updatePreview();

private:
    void setupUi();

    QTextEdit *m_txtInput;
    QFontComboBox *m_comboFont;
    QSpinBox *m_spinSize;
    QPushButton *m_btnBold;
    QPushButton *m_btnItalic;
    QCheckBox *m_chkWhiteout;

    QPushButton *m_btnColorBlack;
    QPushButton *m_btnColorBlue;
    QPushButton *m_btnColorRed;
    QPushButton *m_btnColorCustom;
    QColor m_currentColor;

    QLabel *m_lblPreview;
    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
};
