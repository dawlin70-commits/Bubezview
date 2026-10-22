#include "image_inspector_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFileInfo>
#include <QDateTime>
#include <QMimeDatabase>
#include <QDesktopServices>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileIconProvider>
#include <QDialogButtonBox>
#include <numeric>

ImageInspectorDialog::ImageInspectorDialog(const QString &filePath, const QImage &image, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_image(image)
{
    setWindowTitle(tr("Image Properties & EXIF Inspector"));
    resize(640, 520);
    setMinimumSize(540, 420);

    // Parse EXIF and GPS metadata
    m_exif = ExifParser::parseFile(filePath);

    setupUi();
}

void ImageInspectorDialog::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    // 1. Top Header Banner
    auto *headerWidget = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    QFileInfo fi(m_filePath);
    QFileIconProvider iconProvider;
    QIcon fileIcon = iconProvider.icon(fi);
    auto *iconLabel = new QLabel(headerWidget);
    iconLabel->setPixmap(fileIcon.pixmap(48, 48));
    headerLayout->addWidget(iconLabel);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);
    auto *nameLabel = new QLabel(fi.fileName(), headerWidget);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerTextLayout->addWidget(nameLabel);

    QString subText;
    if (!m_image.isNull()) {
        double mp = (static_cast<double>(m_image.width()) * m_image.height()) / 1000000.0;
        subText = QString("%1 × %2 px (%3 MP) • %4")
                      .arg(m_image.width())
                      .arg(m_image.height())
                      .arg(mp, 0, 'f', 1)
                      .arg(fi.exists() ? QLocale().formattedDataSize(fi.size()) : QString());
    } else if (fi.exists()) {
        subText = QLocale().formattedDataSize(fi.size());
    }
    auto *subLabel = new QLabel(subText, headerWidget);
    subLabel->setStyleSheet("color: #888888;");
    headerTextLayout->addWidget(subLabel);

    headerLayout->addLayout(headerTextLayout, 1);
    rootLayout->addWidget(headerWidget);

    // 2. Tabs
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createGeneralTab(), tr("General"));
    m_tabWidget->addTab(createCameraTab(), tr("Camera && EXIF"));
    m_tabWidget->addTab(createGpsTab(), tr("GPS && Location"));
    rootLayout->addWidget(m_tabWidget, 1);

    // 3. Bottom Button Box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    rootLayout->addWidget(buttonBox);
}

static QLabel *createSelectableLabel(const QString &text)
{
    auto *lbl = new QLabel(text);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return lbl;
}

QWidget *ImageInspectorDialog::createGeneralTab()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget(scroll);
    auto *form = new QFormLayout(container);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    QFileInfo fi(m_filePath);

    // Path
    form->addRow(tr("Full Path:"), createSelectableLabel(fi.absoluteFilePath()));

    // File Size
    if (fi.exists()) {
        QString sizeStr = QString("%1 (%L2 bytes)").arg(QLocale().formattedDataSize(fi.size())).arg(fi.size());
        form->addRow(tr("File Size:"), createSelectableLabel(sizeStr));
    }

    // MIME Type & Format
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(m_filePath);
    form->addRow(tr("MIME Type:"), createSelectableLabel(mime.name()));
    if (!mime.comment().isEmpty()) {
        form->addRow(tr("Format:"), createSelectableLabel(mime.comment()));
    }

    // Resolution & Aspect Ratio
    if (!m_image.isNull()) {
        form->addRow(tr("Resolution:"), createSelectableLabel(QString("%1 × %2 pixels").arg(m_image.width()).arg(m_image.height())));

        double mp = (static_cast<double>(m_image.width()) * m_image.height()) / 1000000.0;
        form->addRow(tr("Megapixels:"), createSelectableLabel(QString("%1 MP").arg(mp, 0, 'f', 2)));

        // Greatest common divisor for aspect ratio
        int w = m_image.width();
        int h = m_image.height();
        int gcd = std::gcd(w, h);
        if (gcd > 0 && w / gcd < 100 && h / gcd < 100) {
            form->addRow(tr("Aspect Ratio:"), createSelectableLabel(QString("%1:%2 (%3:1)")
                                                                        .arg(w / gcd)
                                                                        .arg(h / gcd)
                                                                        .arg(static_cast<double>(w) / static_cast<double>(h), 0, 'f', 2)));
        } else {
            form->addRow(tr("Aspect Ratio:"), createSelectableLabel(QString("%1:1")
                                                                        .arg(static_cast<double>(w) / static_cast<double>(h), 0, 'f', 2)));
        }

        // Color depth and format
        QString depthStr = QString("%1-bit").arg(m_image.depth());
        if (m_image.hasAlphaChannel()) {
            depthStr += tr(" (32-bit RGBA with Alpha Channel)");
        } else {
            depthStr += tr(" (24-bit RGB Opaque)");
        }
        form->addRow(tr("Color Depth:"), createSelectableLabel(depthStr));
        form->addRow(tr("Transparency:"), createSelectableLabel(m_image.hasAlphaChannel() ? tr("Yes (Alpha Channel Present)") : tr("No (Fully Opaque)")));
    }

    // Dates
    if (fi.exists()) {
        form->addRow(tr("Created:"), createSelectableLabel(fi.birthTime().isValid() ? QLocale().toString(fi.birthTime(), QLocale::ShortFormat) : tr("Unknown")));
        form->addRow(tr("Last Modified:"), createSelectableLabel(QLocale().toString(fi.lastModified(), QLocale::ShortFormat)));
    }

    scroll->setWidget(container);
    return scroll;
}

QWidget *ImageInspectorDialog::createCameraTab()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget(scroll);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    const bool hasCameraFields = !m_exif.make.isEmpty() || !m_exif.model.isEmpty() ||
                                 !m_exif.lensModel.isEmpty() || !m_exif.software.isEmpty() ||
                                 !m_exif.dateTimeOriginal.isEmpty() || !m_exif.exposureTime.isEmpty() ||
                                 !m_exif.fNumber.isEmpty() || m_exif.isoSpeed > 0 ||
                                 m_exif.focalLength > 0.0 || m_exif.flash >= 0;

    if (!m_exif.hasExif || !hasCameraFields) {
        auto *emptyLabel = new QLabel(tr("No camera EXIF metadata was found in this file.<br><br>"
                                         "<small style='color: #888888;'>Metadata may have been stripped during export, "
                                         "removed for privacy on social networks, or the image was created digitally.</small>"),
                                      container);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setWordWrap(true);
        layout->addStretch();
        layout->addWidget(emptyLabel);
        layout->addStretch();
        scroll->setWidget(container);
        return scroll;
    }

    auto *form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    if (!m_exif.make.isEmpty()) form->addRow(tr("Camera Make:"), createSelectableLabel(m_exif.make));
    if (!m_exif.model.isEmpty()) form->addRow(tr("Camera Model:"), createSelectableLabel(m_exif.model));
    if (!m_exif.lensModel.isEmpty()) form->addRow(tr("Lens:"), createSelectableLabel(m_exif.lensModel));
    if (!m_exif.software.isEmpty()) form->addRow(tr("Software / Firmware:"), createSelectableLabel(m_exif.software));
    if (!m_exif.dateTimeOriginal.isEmpty()) form->addRow(tr("Date Taken:"), createSelectableLabel(m_exif.dateTimeOriginal));
    if (!m_exif.exposureTime.isEmpty()) form->addRow(tr("Exposure Time:"), createSelectableLabel(m_exif.exposureTime));
    if (!m_exif.fNumber.isEmpty()) form->addRow(tr("Aperture:"), createSelectableLabel(m_exif.fNumber));
    if (m_exif.isoSpeed > 0) form->addRow(tr("ISO Sensitivity:"), createSelectableLabel(QString("ISO %1").arg(m_exif.isoSpeed)));
    if (m_exif.focalLength > 0.0) {
        QString flStr = QString("%1 mm").arg(m_exif.focalLength, 0, 'f', 1);
        if (m_exif.focalLength35mm > 0) {
            flStr += QString(" (35mm eq: %1 mm)").arg(m_exif.focalLength35mm);
        }
        form->addRow(tr("Focal Length:"), createSelectableLabel(flStr));
    }
    if (m_exif.flash >= 0) {
        form->addRow(tr("Flash:"), createSelectableLabel(m_exif.flash ? tr("Fired") : tr("Did not fire")));
    }

    layout->addLayout(form);
    layout->addStretch();

    scroll->setWidget(container);
    return scroll;
}

QWidget *ImageInspectorDialog::createGpsTab()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget(scroll);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    if (!m_exif.gps.hasGps) {
        auto *emptyLabel = new QLabel(tr("📍 No geographic GPS location data was found in this image.<br><br>"
                                         "<small style='color: #888888;'>Geotagging is only present when location services "
                                         "are enabled on your camera or smartphone when taking the photo.</small>"),
                                      container);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setWordWrap(true);
        layout->addStretch();
        layout->addWidget(emptyLabel);
        layout->addStretch();
        scroll->setWidget(container);
        return scroll;
    }

    // Geotagged Banner
    auto *badgeWidget = new QWidget(container);
    auto *badgeLayout = new QHBoxLayout(badgeWidget);
    badgeLayout->setContentsMargins(12, 8, 12, 8);
    badgeWidget->setStyleSheet("background-color: rgba(46, 160, 67, 0.15); border: 1px solid rgba(46, 160, 67, 0.4); border-radius: 6px;");
    auto *badgeLabel = new QLabel(tr("<b>📍 Geotagged Image:</b> Embedded GPS coordinates verified."), badgeWidget);
    badgeLayout->addWidget(badgeLabel);
    layout->addWidget(badgeWidget);

    auto *form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    form->addRow(tr("Coordinates (DMS):"), createSelectableLabel(m_exif.gps.formattedDms()));
    form->addRow(tr("Coordinates (Decimal):"), createSelectableLabel(m_exif.gps.formattedDecimal()));
    if (m_exif.gps.altitude > 0.0) {
        QString altStr = QString("%1 m (%2)")
                             .arg(m_exif.gps.altitude, 0, 'f', 1)
                             .arg(m_exif.gps.belowSeaLevel ? tr("Below sea level") : tr("Above sea level"));
        form->addRow(tr("Altitude:"), createSelectableLabel(altStr));
    }
    layout->addLayout(form);

    // Interactive Action Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    auto *btnOsm = new QPushButton(tr("🌐 Open in OpenStreetMap"), container);
    btnOsm->setCursor(Qt::PointingHandCursor);
    btnOsm->setToolTip(tr("View exact capture location on OpenStreetMap in your browser"));
    connect(btnOsm, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(m_exif.gps.openStreetMapUrl());
    });
    btnLayout->addWidget(btnOsm);

    auto *btnGoogle = new QPushButton(tr("🗺️ Open in Google Maps"), container);
    btnGoogle->setCursor(Qt::PointingHandCursor);
    btnGoogle->setToolTip(tr("View location on Google Maps in your browser"));
    connect(btnGoogle, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(m_exif.gps.googleMapsUrl());
    });
    btnLayout->addWidget(btnGoogle);

    auto *btnCopy = new QPushButton(tr("📋 Copy Coordinates"), container);
    btnCopy->setCursor(Qt::PointingHandCursor);
    btnCopy->setToolTip(tr("Copy decimal latitude and longitude to system clipboard"));
    connect(btnCopy, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_exif.gps.formattedDecimal());
    });
    btnLayout->addWidget(btnCopy);

    layout->addLayout(btnLayout);
    layout->addStretch();

    scroll->setWidget(container);
    return scroll;
}
