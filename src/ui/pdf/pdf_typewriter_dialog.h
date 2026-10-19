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
    /** @brief Constructs the PDF typewriter text entry dialog. */
    explicit PdfTypewriterDialog(QWidget *parent = nullptr);
    ~PdfTypewriterDialog() override;

    /** @brief Returns the entered plain text. */
    QString text() const;
    /** @brief Returns the configured font family, point size, and style. */
    QFont selectedFont() const;
    /** @brief Returns the selected text ink color. */
    QColor textColor() const { return m_currentColor; }
    /** @brief Returns true if an opaque white background box should be drawn behind the text. */
    bool isWhiteout() const;

    /** @brief Pre-populates the text editor content. */
    void setText(const QString &text);
    /** @brief Pre-populates the font family, size, bold, and italic controls. */
    void setSelectedFont(const QFont &font);
    /** @brief Sets the active ink color. */
    void setTextColor(const QColor &color);
    /** @brief Sets the whiteout background checkbox state. */
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
