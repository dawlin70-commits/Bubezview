/**
 * @file pdf_signature_dialog.cpp
 * @brief Implementation of multi-tab signature creation and management dialog.
 */

#include "pdf_signature_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QListWidget>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>
#include <QPainter>
#include <QMouseEvent>
#include <QStandardPaths>

// ============================================================================
// SignatureCanvasWidget Implementation
// ============================================================================

SignatureCanvasWidget::SignatureCanvasWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentColor(Qt::black)
    , m_currentWidth(3)
    , m_isDrawing(false)
{
    setMinimumSize(420, 180);
    setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: 2px dashed #94A3B8; border-radius: 8px;"));
    setCursor(Qt::CrossCursor);
}

void SignatureCanvasWidget::clearCanvas()
{
    m_paths.clear();
    m_currentPath = QPainterPath();
    update();
}

void SignatureCanvasWidget::setStrokeColor(const QColor &color)
{
    m_currentColor = color;
}

void SignatureCanvasWidget::setStrokeWidth(int width)
{
    m_currentWidth = width;
}

void SignatureCanvasWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);

    // Draw baseline guide
    painter.setPen(QPen(QColor(226, 232, 240), 1, Qt::DashLine));
    int baselineY = height() * 3 / 4;
    painter.drawLine(20, baselineY, width() - 20, baselineY);

    if (isEmpty() && !m_isDrawing) {
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 10, QFont::Normal));
        painter.setPen(QColor(148, 163, 184));
        painter.drawText(rect(), Qt::AlignCenter, tr("Sign here with your mouse, touchpad, or pen"));
        return;
    }

    // Render committed strokes
    for (const auto &stroke : m_paths) {
        painter.setPen(QPen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(stroke.path);
    }

    // Render current active stroke
    if (m_isDrawing) {
        painter.setPen(QPen(m_currentColor, m_currentWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(m_currentPath);
    }
}

void SignatureCanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDrawing = true;
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(event->position());
        event->accept();
        update();
    }
}

void SignatureCanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDrawing) {
        m_currentPath.lineTo(event->position());
        event->accept();
        update();
    }
}

void SignatureCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDrawing) {
        m_isDrawing = false;
        m_currentPath.lineTo(event->position());
        m_paths.push_back({m_currentPath, m_currentColor, m_currentWidth});
        m_currentPath = QPainterPath();
        event->accept();
        update();
    }
}

QImage SignatureCanvasWidget::renderSignatureImage() const
{
    if (isEmpty()) {
        return QImage();
    }

    // Compute bounding rect of all strokes
    QRectF totalBounds;
    for (const auto &s : m_paths) {
        totalBounds = totalBounds.united(s.path.boundingRect());
    }

    int padding = 12;
    QRect renderRect = totalBounds.adjusted(-padding, -padding, padding, padding).toAlignedRect();
    renderRect = renderRect.intersected(rect());

    if (renderRect.width() <= 0 || renderRect.height() <= 0) {
        return QImage();
    }

    // Render with 100% transparent background
    QImage image(renderRect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-renderRect.topLeft());

    for (const auto &stroke : m_paths) {
        painter.setPen(QPen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(stroke.path);
    }

    return image;
}

// ============================================================================
// PdfSignatureDialog Implementation
// ============================================================================

PdfSignatureDialog::PdfSignatureDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("✒️ Signature Studio"));
    resize(520, 480);
    setMinimumWidth(460);
    setupUi();
    loadSavedSignatures();
}

PdfSignatureDialog::~PdfSignatureDialog() = default;

QString PdfSignatureDialog::signaturesDirPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/bubezview");
    }
    QString dirPath = base + QStringLiteral("/signatures");
    QDir().mkpath(dirPath);
    return dirPath;
}

void PdfSignatureDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    m_tabs = new QTabWidget(this);

    // Save to library toggle
    m_chkSaveToLibrary = new QCheckBox(tr("💾 Save this signature to personal library for future 1-click use"), this);
    m_chkSaveToLibrary->setChecked(true);

    setupDrawTab();
    setupImageTab();
    setupSavedTab();
    setupDigitalTab();

    connect(m_tabs, &QTabWidget::currentChanged, this, &PdfSignatureDialog::onTabChanged);

    mainLayout->addWidget(m_tabs, 1);
    mainLayout->addWidget(m_chkSaveToLibrary);

    // Dialog Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(tr("Apply Signature ➔"), this);
    m_btnOk->setDefault(true);
    m_btnOk->setStyleSheet(QStringLiteral("background-color: #0284C7; color: white; font-weight: bold; padding: 6px 18px; border-radius: 6px;"));
    connect(m_btnOk, &QPushButton::clicked, this, &PdfSignatureDialog::onAcceptClicked);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

void PdfSignatureDialog::setupDrawTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(10);

    m_canvas = new SignatureCanvasWidget(tab);
    layout->addWidget(m_canvas, 1);

    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    ctrlLayout->setSpacing(8);

    QLabel *lblColor = new QLabel(tr("Ink:"), tab);
    ctrlLayout->addWidget(lblColor);

    m_btnColorBlack = new QPushButton(tab);
    m_btnColorBlack->setFixedSize(24, 24);
    m_btnColorBlack->setStyleSheet(QStringLiteral("background-color: #000000; border-radius: 12px; border: 2px solid #64748B;"));
    connect(m_btnColorBlack, &QPushButton::clicked, this, [this]() { onColorChanged(Qt::black); });
    ctrlLayout->addWidget(m_btnColorBlack);

    m_btnColorBlue = new QPushButton(tab);
    m_btnColorBlue->setFixedSize(24, 24);
    m_btnColorBlue->setStyleSheet(QStringLiteral("background-color: #1D4ED8; border-radius: 12px; border: 2px solid #64748B;"));
    connect(m_btnColorBlue, &QPushButton::clicked, this, [this]() { onColorChanged(QColor(QStringLiteral("#1D4ED8"))); });
    ctrlLayout->addWidget(m_btnColorBlue);

    QLabel *lblWidth = new QLabel(tr("Stroke:"), tab);
    ctrlLayout->addWidget(lblWidth);

    m_sliderWidth = new QSlider(Qt::Horizontal, tab);
    m_sliderWidth->setRange(1, 8);
    m_sliderWidth->setValue(3);
    m_sliderWidth->setFixedWidth(100);
    connect(m_sliderWidth, &QSlider::valueChanged, m_canvas, &SignatureCanvasWidget::setStrokeWidth);
    ctrlLayout->addWidget(m_sliderWidth);

    ctrlLayout->addStretch(1);

    m_btnClearCanvas = new QPushButton(tr("🗑️ Clear"), tab);
    connect(m_btnClearCanvas, &QPushButton::clicked, m_canvas, &SignatureCanvasWidget::clearCanvas);
    ctrlLayout->addWidget(m_btnClearCanvas);

    layout->addLayout(ctrlLayout);
    m_tabs->addTab(tab, tr("✍️ Draw Freehand"));
}

void PdfSignatureDialog::setupImageTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(10);

    m_lblImagePreview = new QLabel(tab);
    m_lblImagePreview->setMinimumHeight(160);
    m_lblImagePreview->setAlignment(Qt::AlignCenter);
    m_lblImagePreview->setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: 2px dashed #94A3B8; border-radius: 8px; color: #475569; font-weight: 500;"));
    m_lblImagePreview->setText(tr("No image loaded.<br>Click 'Browse Photo / Scan' below to import."));
    layout->addWidget(m_lblImagePreview, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    m_btnBrowseImage = new QPushButton(tr("📂 Browse Photo / Scan..."), tab);
    m_btnBrowseImage->setStyleSheet(QStringLiteral("font-weight: 600; padding: 6px 12px;"));
    connect(m_btnBrowseImage, &QPushButton::clicked, this, &PdfSignatureDialog::onBrowseImageClicked);
    btnRow->addWidget(m_btnBrowseImage);
    btnRow->addStretch(1);
    layout->addLayout(btnRow);

    // Transparency Slider
    m_chkAutoRemoveWhite = new QCheckBox(tr("✨ Automatic paper background removal (White -> Transparent)"), tab);
    m_chkAutoRemoveWhite->setChecked(true);
    connect(m_chkAutoRemoveWhite, &QCheckBox::toggled, this, &PdfSignatureDialog::processImportedImage);
    layout->addWidget(m_chkAutoRemoveWhite);

    QHBoxLayout *threshLayout = new QHBoxLayout();
    QLabel *lblThresh = new QLabel(tr("Background Filter Sensitivity:"), tab);
    threshLayout->addWidget(lblThresh);

    m_sliderThreshold = new QSlider(Qt::Horizontal, tab);
    m_sliderThreshold->setRange(160, 255);
    m_sliderThreshold->setValue(225);
    connect(m_sliderThreshold, &QSlider::valueChanged, this, &PdfSignatureDialog::onThresholdSliderChanged);
    threshLayout->addWidget(m_sliderThreshold, 1);
    layout->addLayout(threshLayout);

    m_tabs->addTab(tab, tr("📷 Photo / Scan Import"));
}

void PdfSignatureDialog::setupSavedTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(10);

    m_listSaved = new QListWidget(tab);
    m_listSaved->setViewMode(QListView::IconMode);
    m_listSaved->setIconSize(QSize(140, 70));
    m_listSaved->setGridSize(QSize(160, 100));
    m_listSaved->setSpacing(10);
    m_listSaved->setStyleSheet(
        "QListWidget { background-color: #F8FAFC; border: 1px solid #CBD5E1; border-radius: 8px; }"
        "QListWidget::item { background-color: #FFFFFF; color: #1E293B; border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px; }"
        "QListWidget::item:selected { border: 2px solid #0284C7; background-color: #E0F2FE; color: #0369A1; }"
    );
    connect(m_listSaved, &QListWidget::itemSelectionChanged, this, &PdfSignatureDialog::onSavedItemSelectionChanged);
    layout->addWidget(m_listSaved, 1);

    QHBoxLayout *actLayout = new QHBoxLayout();
    m_btnDeleteSaved = new QPushButton(tr("🗑️ Delete Selected"), tab);
    m_btnDeleteSaved->setEnabled(false);
    connect(m_btnDeleteSaved, &QPushButton::clicked, this, &PdfSignatureDialog::onDeleteSavedClicked);
    actLayout->addWidget(m_btnDeleteSaved);
    actLayout->addStretch(1);
    layout->addLayout(actLayout);

    m_tabs->addTab(tab, tr("📁 Saved Signatures"));
}

void PdfSignatureDialog::setupDigitalTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    QLabel *lblInfo = new QLabel(
        tr("<b>🔐 Cryptographic Digital Signatures (PAdES & CAdES .p7m)</b><br><br>"
           "Verify existing cryptographic signatures, inspect X.509 certificate chains, "
           "generate a personal self-signed certificate (<code>.p12</code>), or digitally sign this PDF:<br><br>"
           "• <b>PAdES (Embedded PDF Signature)</b> — Standard signed <code>.pdf</code> readable by any PDF viewer<br>"
           "• <b>CAdES (PKCS#7 Envelope)</b> — Signed <code>.pdf.p7m</code> envelope standard"),
        tab);
    lblInfo->setWordWrap(true);
    lblInfo->setStyleSheet(QStringLiteral(
        "background: rgba(2, 132, 199, 0.10); border: 1px solid rgba(2, 132, 199, 0.35); "
        "border-radius: 8px; padding: 14px; font-size: 12px;"));
    layout->addWidget(lblInfo);

    QPushButton *btnOpenHub = new QPushButton(tr("🔐 Open Cryptographic Signature Hub..."), tab);
    btnOpenHub->setCursor(Qt::PointingHandCursor);
    btnOpenHub->setStyleSheet(QStringLiteral(
        "background-color: #2563EB; color: white; font-weight: bold; font-size: 13px; "
        "padding: 10px 18px; border-radius: 6px;"));
    connect(btnOpenHub, &QPushButton::clicked, this, [this]() {
        m_cryptoSigRequested = true;
        accept();
    });
    layout->addWidget(btnOpenHub);
    layout->addStretch(1);

    m_tabs->addTab(tab, tr("🔒 Digital Certificate"));
}

void PdfSignatureDialog::onTabChanged(int index)
{
    // Hide 'Save to library' checkbox on Saved Signatures and Digital Certificate tabs
    if (m_chkSaveToLibrary) {
        m_chkSaveToLibrary->setVisible(index == 0 || index == 1);
    }
    if (m_btnOk) {
        m_btnOk->setText(index == 3 ? tr("Open Digital Signature Hub ➔") : tr("Apply Signature ➔"));
    }
}

void PdfSignatureDialog::onColorChanged(const QColor &color)
{
    if (m_canvas) {
        m_canvas->setStrokeColor(color);
    }
}

void PdfSignatureDialog::onBrowseImageClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Signature Image / Scan"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff);;All Files (*)")
    );
    if (!path.isEmpty()) {
        m_importedSourcePath = path;
        m_rawImportedImage.load(path);
        processImportedImage();
    }
}

void PdfSignatureDialog::onThresholdSliderChanged(int /*value*/)
{
    processImportedImage();
}

void PdfSignatureDialog::processImportedImage()
{
    if (m_rawImportedImage.isNull() || !m_lblImagePreview || !m_sliderThreshold || !m_chkAutoRemoveWhite) {
        return;
    }

    QImage src = m_rawImportedImage.convertToFormat(QImage::Format_ARGB32);
    if (!m_chkAutoRemoveWhite->isChecked()) {
        m_processedImportedImage = src;
    } else {
        int threshold = m_sliderThreshold->value();
        m_processedImportedImage = QImage(src.size(), QImage::Format_ARGB32);

        for (int y = 0; y < src.height(); ++y) {
            const QRgb *srcRow = reinterpret_cast<const QRgb*>(src.constScanLine(y));
            QRgb *dstRow = reinterpret_cast<QRgb*>(m_processedImportedImage.scanLine(y));
            for (int x = 0; x < src.width(); ++x) {
                QRgb pixel = srcRow[x];
                int r = qRed(pixel);
                int g = qGreen(pixel);
                int b = qBlue(pixel);
                int luminance = (r * 299 + g * 587 + b * 114) / 1000;

                if (luminance >= threshold) {
                    dstRow[x] = qRgba(r, g, b, 0); // Transparent
                } else if (luminance >= threshold - 30) {
                    // Smooth edge falloff
                    double alphaFactor = 1.0 - (static_cast<double>(luminance - (threshold - 30)) / 30.0);
                    int alpha = static_cast<int>(qAlpha(pixel) * alphaFactor);
                    dstRow[x] = qRgba(r, g, b, alpha);
                } else {
                    dstRow[x] = pixel;
                }
            }
        }
    }

    QSize targetSize = m_lblImagePreview->size() - QSize(20, 20);
    if (targetSize.width() < 10 || targetSize.height() < 10) {
        targetSize = QSize(280, 140);
    }
    QPixmap previewPix = QPixmap::fromImage(m_processedImportedImage)
        .scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_lblImagePreview->setPixmap(previewPix);
}

void PdfSignatureDialog::loadSavedSignatures()
{
    m_listSaved->clear();
    QDir dir(signaturesDirPath());
    QStringList filters;
    filters << QStringLiteral("*.png");
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    for (const auto &fi : files) {
        QPixmap pix(fi.absoluteFilePath());
        if (!pix.isNull()) {
            QIcon icon(pix);
            auto item = new QListWidgetItem(icon, fi.baseName(), m_listSaved);
            item->setData(Qt::UserRole, fi.absoluteFilePath());
        }
    }
}

void PdfSignatureDialog::onSavedItemSelectionChanged()
{
    m_btnDeleteSaved->setEnabled(!m_listSaved->selectedItems().isEmpty());
}

void PdfSignatureDialog::onDeleteSavedClicked()
{
    auto items = m_listSaved->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(Qt::UserRole).toString();
    if (QFile::remove(path)) {
        delete items.first();
    }
}

void PdfSignatureDialog::onAcceptClicked()
{
    int currentTab = m_tabs->currentIndex();
    if (currentTab == 3) {
        m_cryptoSigRequested = true;
        accept();
        return;
    }

    QImage result;

    if (currentTab == 0) {
        // Draw tab
        if (m_canvas->isEmpty()) {
            QMessageBox::warning(this, tr("No Signature"), tr("Please draw your signature on the canvas first."));
            return;
        }
        result = m_canvas->renderSignatureImage();
    } else if (currentTab == 1) {
        // Image tab
        if (m_processedImportedImage.isNull()) {
            QMessageBox::warning(this, tr("No Image"), tr("Please select an image file first."));
            return;
        }
        result = m_processedImportedImage;
    } else if (currentTab == 2) {
        // Saved tab
        auto items = m_listSaved->selectedItems();
        if (items.isEmpty()) {
            QMessageBox::warning(this, tr("Select Signature"), tr("Please select a saved signature from the list."));
            return;
        }
        QString path = items.first()->data(Qt::UserRole).toString();
        result.load(path);
    }

    if (result.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to generate signature image."));
        return;
    }

    m_finalSignature = result;

    // Save to user's personal signature directory if requested
    if (currentTab != 2 && m_chkSaveToLibrary->isChecked()) {
        QString filename = QStringLiteral("sig_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
        QString savePath = signaturesDirPath() + QStringLiteral("/") + filename;
        m_finalSignature.save(savePath, "PNG");
    }

    accept();
}
