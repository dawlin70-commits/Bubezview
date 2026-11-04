/**
 * @file pdf_signature_dialog.cpp
 * @brief Implementation of multi-tab signature creation and management dialog.
 */

#include "pdf_signature_dialog.h"
#include <QStyle>
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
        if (m_currentPath.length() < 0.5) {
            m_currentPath.lineTo(event->position() + QPointF(0.5, 0.5));
        }
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

    // Save to library toggle (visible only when creating a new signature in Image or Draw tabs)
    m_chkSaveToLibrary = new QCheckBox(tr("💾 Save this signature to personal library for future 1-click use"), this);
    m_chkSaveToLibrary->setChecked(true);
    m_chkSaveToLibrary->setVisible(false);

    setupSavedTab();   // Tab 0: Saved signatures (DEFAULT)
    setupImageTab();   // Tab 1: Import from photo or scan with Instant Alpha
    setupDrawTab();    // Tab 2: Freehand drawing pad
    setupDigitalTab(); // Tab 3: Digital cryptographic certificate info

    m_tabs->setCurrentIndex(0);
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

void PdfSignatureDialog::setupSavedTab()
{
    m_tabSaved = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_tabSaved);
    layout->setSpacing(10);

    // Top action row with the two dedicated creation buttons
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_btnAddFromImage = new QPushButton(tr("📷 Add from Image / Scan..."), m_tabSaved);
    m_btnAddFromImage->setCursor(Qt::PointingHandCursor);
    m_btnAddFromImage->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0284C7; color: white; font-weight: bold; padding: 7px 14px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #0369A1; }"
    ));
    connect(m_btnAddFromImage, &QPushButton::clicked, this, [this]() {
        m_tabs->setCurrentWidget(m_tabImage);
        onBrowseImageClicked();
    });
    topRow->addWidget(m_btnAddFromImage);

    m_btnAddFreehand = new QPushButton(tr("✍️ Add Freehand Drawing..."), m_tabSaved);
    m_btnAddFreehand->setCursor(Qt::PointingHandCursor);
    m_btnAddFreehand->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #475569; color: white; font-weight: bold; padding: 7px 14px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #334155; }"
    ));
    connect(m_btnAddFreehand, &QPushButton::clicked, this, [this]() {
        m_tabs->setCurrentWidget(m_tabDraw);
    });
    topRow->addWidget(m_btnAddFreehand);

    topRow->addStretch(1);

    m_btnDeleteSaved = new QPushButton(tr("🗑️ Delete Selected"), m_tabSaved);
    m_btnDeleteSaved->setEnabled(false);
    connect(m_btnDeleteSaved, &QPushButton::clicked, this, &PdfSignatureDialog::onDeleteSavedClicked);
    topRow->addWidget(m_btnDeleteSaved);

    layout->addLayout(topRow);

    // Visual list of saved signatures
    m_listSaved = new QListWidget(m_tabSaved);
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
    connect(m_listSaved, &QListWidget::itemDoubleClicked, this, &PdfSignatureDialog::onAcceptClicked);
    layout->addWidget(m_listSaved, 1);

    // Empty state label when no signatures exist yet
    m_lblEmptySaved = new QLabel(
        tr("No saved signatures yet.<br>Click <b>'📷 Add from Image / Scan'</b> or <b>'✍️ Add Freehand Drawing'</b> above to create your first signature."),
        m_tabSaved);
    m_lblEmptySaved->setAlignment(Qt::AlignCenter);
    m_lblEmptySaved->setStyleSheet(QStringLiteral("color: #64748B; font-size: 13px; padding: 24px; background-color: #F8FAFC; border: 1px dashed #CBD5E1; border-radius: 8px;"));
    m_lblEmptySaved->setVisible(false);
    layout->addWidget(m_lblEmptySaved, 1);

    m_tabs->addTab(m_tabSaved, tr("📁 Saved Signatures"));
}

void PdfSignatureDialog::setupImageTab()
{
    m_tabImage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_tabImage);
    layout->setSpacing(10);

    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *btnBack = new QPushButton(tr("← Back to Saved Signatures"), m_tabImage);
    btnBack->setCursor(Qt::PointingHandCursor);
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        m_tabs->setCurrentWidget(m_tabSaved);
    });
    headerRow->addWidget(btnBack);
    headerRow->addStretch(1);
    layout->addLayout(headerRow);

    m_lblImagePreview = new QLabel(m_tabImage);
    m_lblImagePreview->setMinimumHeight(170);
    m_lblImagePreview->setAlignment(Qt::AlignCenter);
    m_lblImagePreview->setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: 2px dashed #94A3B8; border-radius: 8px; color: #475569; font-weight: 500;"));
    m_lblImagePreview->setText(tr("No image loaded.<br>Click 'Browse Photo / Scan' below to import."));
    m_lblImagePreview->installEventFilter(this);
    m_lblImagePreview->setCursor(Qt::CrossCursor);
    m_lblImagePreview->setToolTip(tr("Click anywhere on the preview to sample the background color to key out (Instant Alpha)"));
    layout->addWidget(m_lblImagePreview, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    m_btnBrowseImage = new QPushButton(tr("📂 Browse Photo / Scan..."), m_tabImage);
    m_btnBrowseImage->setStyleSheet(QStringLiteral("font-weight: 600; padding: 6px 12px;"));
    connect(m_btnBrowseImage, &QPushButton::clicked, this, &PdfSignatureDialog::onBrowseImageClicked);
    btnRow->addWidget(m_btnBrowseImage);

    m_btnResetKeyColor = new QPushButton(tr("Key: #FFFFFF"), m_tabImage);
    m_btnResetKeyColor->setStyleSheet(QStringLiteral("background-color: #FFFFFF; color: #000000; font-weight: bold; padding: 4px 10px; border: 1px solid #94A3B8; border-radius: 4px;"));
    m_btnResetKeyColor->setToolTip(tr("Current key color to remove (click preview to sample, or click here to reset to White)"));
    connect(m_btnResetKeyColor, &QPushButton::clicked, this, [this]() {
        m_targetKeyColor = Qt::white;
        m_seedPoint = QPoint(-1, -1);
        m_btnResetKeyColor->setText(tr("Key: #FFFFFF"));
        m_btnResetKeyColor->setStyleSheet(QStringLiteral("background-color: #FFFFFF; color: #000000; font-weight: bold; padding: 4px 10px; border: 1px solid #94A3B8; border-radius: 4px;"));
        processImportedImage();
    });
    btnRow->addWidget(m_btnResetKeyColor);

    btnRow->addStretch(1);
    layout->addLayout(btnRow);

    // Transparency options
    m_chkAutoRemoveWhite = new QCheckBox(tr("✨ Automatic background removal (Instant Alpha)"), m_tabImage);
    m_chkAutoRemoveWhite->setChecked(true);
    connect(m_chkAutoRemoveWhite, &QCheckBox::toggled, this, &PdfSignatureDialog::processImportedImage);
    layout->addWidget(m_chkAutoRemoveWhite);

    m_chkContiguousOnly = new QCheckBox(tr("🪄 Contiguous Flood-Fill (Preserves inner loops of letters/shapes)"), m_tabImage);
    m_chkContiguousOnly->setChecked(true);
    m_chkContiguousOnly->setToolTip(tr("Flood-fills outward from the sampled point, leaving isolated pixels inside loops untouched"));
    connect(m_chkContiguousOnly, &QCheckBox::toggled, this, &PdfSignatureDialog::processImportedImage);
    layout->addWidget(m_chkContiguousOnly);

    QHBoxLayout *threshLayout = new QHBoxLayout();
    QLabel *lblThresh = new QLabel(tr("Sensitivity / Tolerance:"), m_tabImage);
    threshLayout->addWidget(lblThresh);

    m_sliderThreshold = new QSlider(Qt::Horizontal, m_tabImage);
    m_sliderThreshold->setRange(5, 75);
    m_sliderThreshold->setValue(25);
    connect(m_sliderThreshold, &QSlider::valueChanged, this, [this](int val) {
        if (m_lblToleranceVal) {
            m_lblToleranceVal->setText(QStringLiteral("%1%").arg(val));
        }
        processImportedImage();
    });
    threshLayout->addWidget(m_sliderThreshold, 1);

    m_lblToleranceVal = new QLabel(QStringLiteral("25%"), m_tabImage);
    m_lblToleranceVal->setFixedWidth(36);
    m_lblToleranceVal->setAlignment(Qt::AlignCenter);
    threshLayout->addWidget(m_lblToleranceVal);

    layout->addLayout(threshLayout);

    m_tabs->addTab(m_tabImage, tr("📷 Photo / Scan Import"));
}

void PdfSignatureDialog::setupDrawTab()
{
    m_tabDraw = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_tabDraw);
    layout->setSpacing(10);

    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *btnBack = new QPushButton(tr("← Back to Saved Signatures"), m_tabDraw);
    btnBack->setCursor(Qt::PointingHandCursor);
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        m_tabs->setCurrentWidget(m_tabSaved);
    });
    headerRow->addWidget(btnBack);
    headerRow->addStretch(1);
    layout->addLayout(headerRow);

    m_canvas = new SignatureCanvasWidget(m_tabDraw);
    layout->addWidget(m_canvas, 1);

    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    ctrlLayout->setSpacing(8);

    QLabel *lblColor = new QLabel(tr("Ink:"), m_tabDraw);
    ctrlLayout->addWidget(lblColor);

    m_btnColorBlack = new QPushButton(m_tabDraw);
    m_btnColorBlack->setFixedSize(24, 24);
    m_btnColorBlack->setStyleSheet(QStringLiteral("background-color: #000000; border-radius: 12px; border: 2px solid #64748B;"));
    connect(m_btnColorBlack, &QPushButton::clicked, this, [this]() { onColorChanged(Qt::black); });
    ctrlLayout->addWidget(m_btnColorBlack);

    m_btnColorBlue = new QPushButton(m_tabDraw);
    m_btnColorBlue->setFixedSize(24, 24);
    m_btnColorBlue->setStyleSheet(QStringLiteral("background-color: #1D4ED8; border-radius: 12px; border: 2px solid #64748B;"));
    connect(m_btnColorBlue, &QPushButton::clicked, this, [this]() { onColorChanged(QColor(QStringLiteral("#1D4ED8"))); });
    ctrlLayout->addWidget(m_btnColorBlue);

    QLabel *lblWidth = new QLabel(tr("Stroke:"), m_tabDraw);
    ctrlLayout->addWidget(lblWidth);

    m_sliderWidth = new QSlider(Qt::Horizontal, m_tabDraw);
    m_sliderWidth->setRange(1, 8);
    m_sliderWidth->setValue(3);
    m_sliderWidth->setFixedWidth(100);
    connect(m_sliderWidth, &QSlider::valueChanged, m_canvas, &SignatureCanvasWidget::setStrokeWidth);
    ctrlLayout->addWidget(m_sliderWidth);

    ctrlLayout->addStretch(1);

    m_btnClearCanvas = new QPushButton(tr("🗑️ Clear"), m_tabDraw);
    connect(m_btnClearCanvas, &QPushButton::clicked, m_canvas, &SignatureCanvasWidget::clearCanvas);
    ctrlLayout->addWidget(m_btnClearCanvas);

    layout->addLayout(ctrlLayout);
    m_tabs->addTab(m_tabDraw, tr("✍️ Draw Freehand"));
}

void PdfSignatureDialog::setupDigitalTab()
{
    m_tabDigital = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_tabDigital);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    QHBoxLayout *headerRow = new QHBoxLayout();
    QPushButton *btnBack = new QPushButton(tr("← Back to Saved Signatures"), m_tabDigital);
    btnBack->setCursor(Qt::PointingHandCursor);
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        m_tabs->setCurrentWidget(m_tabSaved);
    });
    headerRow->addWidget(btnBack);
    headerRow->addStretch(1);
    layout->addLayout(headerRow);

    QLabel *lblInfo = new QLabel(
        tr("<b>🔐 Cryptographic Digital Signatures (PAdES & CAdES .p7m)</b><br><br>"
           "Verify existing cryptographic signatures, inspect X.509 certificate chains, "
           "generate a personal self-signed certificate (<code>.p12</code>), or digitally sign this PDF:<br><br>"
           "• <b>PAdES (Embedded PDF Signature)</b> — Standard signed <code>.pdf</code> readable by any PDF viewer<br>"
           "• <b>CAdES (PKCS#7 Envelope)</b> — Signed <code>.pdf.p7m</code> envelope standard"),
        m_tabDigital);
    lblInfo->setWordWrap(true);
    lblInfo->setStyleSheet(QStringLiteral(
        "background: rgba(2, 132, 199, 0.10); border: 1px solid rgba(2, 132, 199, 0.35); "
        "border-radius: 8px; padding: 14px; font-size: 12px;"));
    layout->addWidget(lblInfo);

    QPushButton *btnOpenHub = new QPushButton(tr("🔐 Open Cryptographic Signature Hub..."), m_tabDigital);
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

    m_tabs->addTab(m_tabDigital, tr("🔒 Digital Certificate"));
}

void PdfSignatureDialog::onTabChanged(int index)
{
    // Show 'Save to library' checkbox only when creating new signatures in Image (1) or Draw (2) tabs
    if (m_chkSaveToLibrary) {
        m_chkSaveToLibrary->setVisible(index == 1 || index == 2);
    }
    if (m_btnOk) {
        if (index == 0) {
            m_btnOk->setText(tr("Apply Signature ➔"));
            m_btnOk->setEnabled(!m_listSaved->selectedItems().isEmpty());
        } else if (index == 1) {
            m_btnOk->setText(tr("Save & Apply Signature ➔"));
            m_btnOk->setEnabled(!m_processedImportedImage.isNull());
        } else if (index == 2) {
            m_btnOk->setText(tr("Save & Apply Signature ➔"));
            m_btnOk->setEnabled(true);
        } else if (index == 3) {
            m_btnOk->setText(tr("Open Digital Signature Hub ➔"));
            m_btnOk->setEnabled(true);
        }
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
        if (m_btnOk && m_tabs && m_tabs->currentIndex() == 1) {
            m_btnOk->setEnabled(!m_processedImportedImage.isNull());
        }
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

    if (!m_chkAutoRemoveWhite->isChecked()) {
        m_processedImportedImage = m_rawImportedImage.convertToFormat(QImage::Format_ARGB32);
    } else {
        int tolerance = m_sliderThreshold->value();
        bool contiguous = m_chkContiguousOnly ? m_chkContiguousOnly->isChecked() : true;
        m_processedImportedImage = ImageProcessor::instantAlpha(
            m_rawImportedImage,
            m_targetKeyColor,
            tolerance,
            contiguous,
            m_seedPoint,
            14
        );
    }

    // Render with 8x8 checkerboard pattern to visualize transparency
    QSize targetSize = m_lblImagePreview->size() - QSize(20, 20);
    if (targetSize.width() < 10 || targetSize.height() < 10) {
        targetSize = QSize(360, 160);
    }
    QImage scaled = m_processedImportedImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap previewPix(scaled.size());
    QPainter p(&previewPix);
    const int checkSize = 8;
    for (int y = 0; y < scaled.height(); y += checkSize) {
        for (int x = 0; x < scaled.width(); x += checkSize) {
            bool isDarkCheck = ((x / checkSize) + (y / checkSize)) % 2 == 1;
            p.fillRect(x, y, checkSize, checkSize, isDarkCheck ? QColor(QStringLiteral("#E2E8F0")) : QColor(QStringLiteral("#FFFFFF")));
        }
    }
    p.drawImage(0, 0, scaled);
    p.end();

    m_lblImagePreview->setPixmap(previewPix);
}

bool PdfSignatureDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lblImagePreview && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_rawImportedImage.isNull()) {
            QPixmap currentPix = m_lblImagePreview->pixmap();
            if (!currentPix.isNull()) {
                QRect pixRect = QStyle::alignedRect(
                    Qt::LeftToRight, Qt::AlignCenter, currentPix.size(), m_lblImagePreview->rect());
                QPoint clickPos = mouseEvent->pos();
                if (pixRect.contains(clickPos)) {
                    QPoint localPt = clickPos - pixRect.topLeft();
                    double scaleX = static_cast<double>(m_rawImportedImage.width()) / currentPix.width();
                    double scaleY = static_cast<double>(m_rawImportedImage.height()) / currentPix.height();
                    int imgX = std::clamp(static_cast<int>(localPt.x() * scaleX), 0, m_rawImportedImage.width() - 1);
                    int imgY = std::clamp(static_cast<int>(localPt.y() * scaleY), 0, m_rawImportedImage.height() - 1);

                    m_targetKeyColor = m_rawImportedImage.pixelColor(imgX, imgY);
                    m_seedPoint = QPoint(imgX, imgY);
                    if (m_btnResetKeyColor) {
                        m_btnResetKeyColor->setText(tr("Key: %1").arg(m_targetKeyColor.name().toUpper()));
                        m_btnResetKeyColor->setStyleSheet(QStringLiteral("background-color: %1; color: %2; font-weight: bold; padding: 4px 10px; border: 1px solid #94A3B8; border-radius: 4px;")
                            .arg(m_targetKeyColor.name())
                            .arg(m_targetKeyColor.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF")));
                    }
                    processImportedImage();
                    return true;
                }
            }
        }
    }
    return QDialog::eventFilter(watched, event);
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
            auto item = new QListWidgetItem(icon, fi.completeBaseName(), m_listSaved);
            item->setData(Qt::UserRole, fi.absoluteFilePath());
        }
    }

    bool hasItems = (m_listSaved->count() > 0);
    m_listSaved->setVisible(hasItems);
    if (m_lblEmptySaved) {
        m_lblEmptySaved->setVisible(!hasItems);
    }
    if (hasItems) {
        m_listSaved->setCurrentRow(0);
    }
    onSavedItemSelectionChanged();
}

void PdfSignatureDialog::onSavedItemSelectionChanged()
{
    bool hasSelection = !m_listSaved->selectedItems().isEmpty();
    m_btnDeleteSaved->setEnabled(hasSelection);
    if (m_tabs && m_tabs->currentIndex() == 0 && m_btnOk) {
        m_btnOk->setEnabled(hasSelection);
    }
}

void PdfSignatureDialog::onDeleteSavedClicked()
{
    auto items = m_listSaved->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(Qt::UserRole).toString();
    if (QFile::remove(path)) {
        delete items.first();
    }
    loadSavedSignatures();
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
        // Tab 0: Saved signatures
        auto items = m_listSaved->selectedItems();
        if (items.isEmpty()) {
            QMessageBox::warning(this, tr("Select Signature"), tr("Please select a saved signature from the list."));
            return;
        }
        QString path = items.first()->data(Qt::UserRole).toString();
        result.load(path);
    } else if (currentTab == 1) {
        // Tab 1: Image import
        if (m_processedImportedImage.isNull()) {
            QMessageBox::warning(this, tr("No Image"), tr("Please select an image file first."));
            return;
        }
        result = m_processedImportedImage;
    } else if (currentTab == 2) {
        // Tab 2: Freehand Draw
        if (m_canvas->isEmpty()) {
            QMessageBox::warning(this, tr("No Signature"), tr("Please draw your signature on the canvas first."));
            return;
        }
        result = m_canvas->renderSignatureImage();
    }

    if (result.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to generate signature image."));
        return;
    }

    m_finalSignature = result;

    // Save to user's personal signature directory if created in Image (1) or Draw (2) tab
    if (currentTab != 0 && m_chkSaveToLibrary->isChecked()) {
        QString filename = QStringLiteral("sig_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
        QString savePath = signaturesDirPath() + QStringLiteral("/") + filename;
        m_finalSignature.save(savePath, "PNG");
    }

    accept();
}
