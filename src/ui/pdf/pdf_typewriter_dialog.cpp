/**
 * @file pdf_typewriter_dialog.cpp
 * @brief Implementation of interactive typewriter dialog for PDF documents.
 */

#include "pdf_typewriter_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

PdfTypewriterDialog::PdfTypewriterDialog(QWidget *parent)
    : QDialog(parent)
    , m_currentColor(Qt::black)
{
    setWindowTitle(tr("✍️ Typewriter - Write on PDF"));
    resize(480, 420);
    setMinimumWidth(440);
    setupUi();
    updatePreview();
}

PdfTypewriterDialog::~PdfTypewriterDialog() = default;

void PdfTypewriterDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Text Input
    QLabel *lblPrompt = new QLabel(tr("<b>Enter text to insert into the document:</b>"), this);
    mainLayout->addWidget(lblPrompt);

    m_txtInput = new QTextEdit(this);
    m_txtInput->setPlaceholderText(tr("Type notes, signatures, dates, or form data here..."));
    m_txtInput->setMinimumHeight(80);
    connect(m_txtInput, &QTextEdit::textChanged, this, &PdfTypewriterDialog::updatePreview);
    mainLayout->addWidget(m_txtInput);

    // Font and Styling Bar
    QHBoxLayout *fontLayout = new QHBoxLayout();
    fontLayout->setSpacing(8);

    m_comboFont = new QFontComboBox(this);
    connect(m_comboFont, &QFontComboBox::currentFontChanged, this, &PdfTypewriterDialog::onFontChanged);
    fontLayout->addWidget(m_comboFont, 1);

    m_spinSize = new QSpinBox(this);
    m_spinSize->setRange(6, 72);
    m_spinSize->setValue(12);
    m_spinSize->setSuffix(QStringLiteral(" pt"));
    connect(m_spinSize, &QSpinBox::valueChanged, this, &PdfTypewriterDialog::onFontChanged);
    fontLayout->addWidget(m_spinSize);

    m_btnBold = new QPushButton(QStringLiteral("B"), this);
    m_btnBold->setCheckable(true);
    m_btnBold->setFixedWidth(32);
    m_btnBold->setStyleSheet(QStringLiteral("font-weight: bold;"));
    connect(m_btnBold, &QPushButton::toggled, this, &PdfTypewriterDialog::onFontChanged);
    fontLayout->addWidget(m_btnBold);

    m_btnItalic = new QPushButton(QStringLiteral("I"), this);
    m_btnItalic->setCheckable(true);
    m_btnItalic->setFixedWidth(32);
    m_btnItalic->setStyleSheet(QStringLiteral("font-style: italic;"));
    connect(m_btnItalic, &QPushButton::toggled, this, &PdfTypewriterDialog::onFontChanged);
    fontLayout->addWidget(m_btnItalic);

    mainLayout->addLayout(fontLayout);

    // Color and Whiteout Options
    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->setSpacing(6);

    QLabel *lblColor = new QLabel(tr("Ink:"), this);
    colorLayout->addWidget(lblColor);

    m_btnColorBlack = new QPushButton(this);
    m_btnColorBlack->setFixedSize(26, 26);
    m_btnColorBlack->setStyleSheet(QStringLiteral("background-color: #000000; border-radius: 13px; border: 2px solid #64748B;"));
    m_btnColorBlack->setCursor(Qt::PointingHandCursor);
    connect(m_btnColorBlack, &QPushButton::clicked, this, &PdfTypewriterDialog::onColorPresetClicked);
    colorLayout->addWidget(m_btnColorBlack);

    m_btnColorBlue = new QPushButton(this);
    m_btnColorBlue->setFixedSize(26, 26);
    m_btnColorBlue->setStyleSheet(QStringLiteral("background-color: #1D4ED8; border-radius: 13px; border: 2px solid #64748B;"));
    m_btnColorBlue->setCursor(Qt::PointingHandCursor);
    connect(m_btnColorBlue, &QPushButton::clicked, this, &PdfTypewriterDialog::onColorPresetClicked);
    colorLayout->addWidget(m_btnColorBlue);

    m_btnColorRed = new QPushButton(this);
    m_btnColorRed->setFixedSize(26, 26);
    m_btnColorRed->setStyleSheet(QStringLiteral("background-color: #DC2626; border-radius: 13px; border: 2px solid #64748B;"));
    m_btnColorRed->setCursor(Qt::PointingHandCursor);
    connect(m_btnColorRed, &QPushButton::clicked, this, &PdfTypewriterDialog::onColorPresetClicked);
    colorLayout->addWidget(m_btnColorRed);

    m_btnColorCustom = new QPushButton(tr("Palette..."), this);
    m_btnColorCustom->setCursor(Qt::PointingHandCursor);
    connect(m_btnColorCustom, &QPushButton::clicked, this, &PdfTypewriterDialog::onCustomColorClicked);
    colorLayout->addWidget(m_btnColorCustom);

    colorLayout->addStretch(1);

    m_chkWhiteout = new QCheckBox(tr("🔲 Whiteout Background (cover underlying text)"), this);
    connect(m_chkWhiteout, &QCheckBox::toggled, this, &PdfTypewriterDialog::updatePreview);
    colorLayout->addWidget(m_chkWhiteout);

    mainLayout->addLayout(colorLayout);

    // Live Visual Preview
    QGroupBox *grpPreview = new QGroupBox(tr("Live Preview"), this);
    QVBoxLayout *prevLayout = new QVBoxLayout(grpPreview);
    m_lblPreview = new QLabel(grpPreview);
    m_lblPreview->setMinimumHeight(60);
    m_lblPreview->setAlignment(Qt::AlignCenter);
    m_lblPreview->setStyleSheet(QStringLiteral("border: 1px dashed #475569; border-radius: 6px; padding: 8px;"));
    prevLayout->addWidget(m_lblPreview);
    mainLayout->addWidget(grpPreview);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(tr("Place on Document ➔"), this);
    m_btnOk->setDefault(true);
    m_btnOk->setStyleSheet(QStringLiteral("background-color: #0284C7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 6px;"));
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

QString PdfTypewriterDialog::text() const
{
    return m_txtInput->toPlainText();
}

QFont PdfTypewriterDialog::selectedFont() const
{
    QFont font = m_comboFont->currentFont();
    font.setStyleName(QString());
    font.setPointSize(m_spinSize->value());
    font.setBold(m_btnBold->isChecked());
    font.setItalic(m_btnItalic->isChecked());
    return font;
}

bool PdfTypewriterDialog::isWhiteout() const
{
    return m_chkWhiteout->isChecked();
}

void PdfTypewriterDialog::setText(const QString &text)
{
    m_txtInput->setPlainText(text);
    updatePreview();
}

void PdfTypewriterDialog::setSelectedFont(const QFont &font)
{
    m_comboFont->setCurrentFont(font);
    int ptSize = font.pointSize();
    if (ptSize <= 0) ptSize = font.pixelSize();
    if (ptSize <= 0) ptSize = 12;
    m_spinSize->setValue(ptSize);
    m_btnBold->setChecked(font.bold());
    m_btnItalic->setChecked(font.italic());
    updatePreview();
}

void PdfTypewriterDialog::setTextColor(const QColor &color)
{
    m_currentColor = color;
    updatePreview();
}

void PdfTypewriterDialog::setWhiteout(bool whiteout)
{
    m_chkWhiteout->setChecked(whiteout);
    updatePreview();
}

void PdfTypewriterDialog::onFontChanged()
{
    updatePreview();
}

void PdfTypewriterDialog::onColorPresetClicked()
{
    QObject *senderObj = sender();
    if (senderObj == m_btnColorBlack) {
        m_currentColor = Qt::black;
    } else if (senderObj == m_btnColorBlue) {
        m_currentColor = QColor(QStringLiteral("#1D4ED8"));
    } else if (senderObj == m_btnColorRed) {
        m_currentColor = QColor(QStringLiteral("#DC2626"));
    }
    updatePreview();
}

void PdfTypewriterDialog::onCustomColorClicked()
{
    QColor chosen = QColorDialog::getColor(m_currentColor, this, tr("Select Ink Color"));
    if (chosen.isValid()) {
        m_currentColor = chosen;
        updatePreview();
    }
}

void PdfTypewriterDialog::updatePreview()
{
    QString content = m_txtInput->toPlainText();
    const bool hasText = !content.trimmed().isEmpty();
    if (m_btnOk) {
        m_btnOk->setEnabled(hasText);
    }
    if (!hasText) {
        content = tr("(Type text above to preview)");
    }
    m_lblPreview->setText(content);
    m_lblPreview->setFont(selectedFont());

    QString bgStyle = m_chkWhiteout->isChecked() ? QStringLiteral("background-color: #FFFFFF;") : QStringLiteral("background-color: transparent;");
    m_lblPreview->setStyleSheet(QString("border: 1px dashed #475569; border-radius: 6px; padding: 8px; color: %1; %2")
                               .arg(m_currentColor.name())
                               .arg(bgStyle));
}
