#include "image_inspector_dialog.h"
#include "core/services/theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QMimeDatabase>
#include <QImageReader>
#include <QDesktopServices>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileIconProvider>
#include <QDialogButtonBox>
#include <numeric>

namespace {
QString formatUnixPermissions(QFile::Permissions p)
{
    QString s;
    s.reserve(9);
    s += (p & QFile::ReadUser)  ? QLatin1Char('r') : QLatin1Char('-');
    s += (p & QFile::WriteUser) ? QLatin1Char('w') : QLatin1Char('-');
    s += (p & QFile::ExeUser)   ? QLatin1Char('x') : QLatin1Char('-');
    s += (p & QFile::ReadGroup) ? QLatin1Char('r') : QLatin1Char('-');
    s += (p & QFile::WriteGroup)? QLatin1Char('w') : QLatin1Char('-');
    s += (p & QFile::ExeGroup)  ? QLatin1Char('x') : QLatin1Char('-');
    s += (p & QFile::ReadOther) ? QLatin1Char('r') : QLatin1Char('-');
    s += (p & QFile::WriteOther)? QLatin1Char('w') : QLatin1Char('-');
    s += (p & QFile::ExeOther)  ? QLatin1Char('x') : QLatin1Char('-');
    return s;
}
} // namespace

ImageInspectorDialog::ImageInspectorDialog(const QString &filePath,
                                           const QImage &image,
                                           QWidget *parent,
                                           const QList<QPair<QString, QString>> &extraFields)
    : FramelessDialog(parent)
    , m_filePath(filePath)
    , m_image(image)
    , m_extraFields(extraFields)
{
    const QFileInfo fi(filePath);
    if (!m_image.isNull()) {
        m_isImageFile = true;
        m_imageSize = m_image.size();
    } else if (fi.isFile()) {
        QImageReader reader(filePath);
        if (reader.canRead()) {
            m_isImageFile = true;
            m_imageSize = reader.size();
        }
    }

    if (m_isImageFile) {
        setWindowTitle(tr("Image Properties & EXIF Inspector"));
        resize(640, 520);
        setMinimumSize(540, 420);
        m_exif = ExifParser::parseFile(filePath);
    } else {
        setWindowTitle(tr("File Properties & Metadata"));
        resize(560, 400);
        setMinimumSize(460, 320);
    }

    setupUi();
}

void ImageInspectorDialog::setupUi()
{
    const bool isDark = ThemeManager::isDarkTheme();
    const QString textCol = isDark ? QStringLiteral("#f1f5f9") : QStringLiteral("#0f172a");
    const QString subCol = isDark ? QStringLiteral("#94a3b8") : QStringLiteral("#64748b");
    const QString paneBg = isDark ? QStringLiteral("rgba(255, 255, 255, 0.03)") : QStringLiteral("rgba(15, 23, 42, 0.03)");
    const QString borderCol = isDark ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(15, 23, 42, 0.12)");

    setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }"
        "QTabWidget::pane { border: 1px solid %3; border-radius: 8px; background: %2; }"
        "QTabBar::tab { color: %4; padding: 6px 14px; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { color: %1; font-weight: 600; background: %2; border: 1px solid %3; border-bottom: none; }"
    ).arg(textCol, paneBg, borderCol, subCol));

    auto *rootLayout = contentLayout();
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(12);

    // 1. Top Header Banner
    auto *headerWidget = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    QFileInfo fi(m_filePath);
    static QFileIconProvider iconProvider;
    QIcon fileIcon = iconProvider.icon(fi);
    auto *iconLabel = new QLabel(headerWidget);
    iconLabel->setPixmap(fileIcon.pixmap(48, 48));
    headerLayout->addWidget(iconLabel);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);
    auto *nameLabel = new QLabel(fi.fileName().isEmpty() ? m_filePath : fi.fileName(), headerWidget);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerTextLayout->addWidget(nameLabel);

    QString subText;
    if (m_isImageFile && m_imageSize.isValid() && m_imageSize.width() > 0 && m_imageSize.height() > 0) {
        double mp = (static_cast<double>(m_imageSize.width()) * m_imageSize.height()) / 1000000.0;
        subText = QString("%1 × %2 px (%3 MP) • %4")
                      .arg(m_imageSize.width())
                      .arg(m_imageSize.height())
                      .arg(mp, 0, 'f', 1)
                      .arg(fi.exists() ? QLocale().formattedDataSize(fi.size()) : QString());
    } else if (fi.isDir()) {
        subText = tr("Folder");
    } else if (fi.exists()) {
        subText = QLocale().formattedDataSize(fi.size());
    }
    auto *subLabel = new QLabel(subText, headerWidget);
    subLabel->setStyleSheet(QStringLiteral("color: %1;").arg(subCol));
    headerTextLayout->addWidget(subLabel);

    headerLayout->addLayout(headerTextLayout, 1);
    rootLayout->addWidget(headerWidget);

    // 2. Content (Tabs for images, clean General view for non-image files/folders)
    if (m_isImageFile) {
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->addTab(createGeneralTab(), tr("General"));
        m_tabWidget->addTab(createCameraTab(), tr("Camera && EXIF"));
        m_tabWidget->addTab(createGpsTab(), tr("GPS && Location"));
        rootLayout->addWidget(m_tabWidget, 1);
    } else {
        QWidget *genTab = createGeneralTab();
        genTab->setStyleSheet(QStringLiteral("QScrollArea { border: 1px solid %1; border-radius: 8px; background: %2; }")
                              .arg(borderCol, paneBg));
        rootLayout->addWidget(genTab, 1);
    }

    // 3. Bottom Button Box
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    rootLayout->addWidget(buttonBox);
}

static QLabel *createSelectableLabel(const QString &text)
{
    auto *lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return lbl;
}

QWidget *ImageInspectorDialog::createGeneralTab()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *container = new QWidget(scroll);
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *form = new QFormLayout(container);
    form->setContentsMargins(14, 14, 14, 14);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    QFileInfo fi(m_filePath);

    // Path
    form->addRow(tr("Full Path:"), createSelectableLabel(fi.absoluteFilePath()));

    // Directory vs File metrics
    if (fi.isDir()) {
        QDir d(m_filePath);
        const QFileInfoList subEntries = d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::NoSort);
        int fileCount = 0;
        int dirCount = 0;
        qint64 dirSize = 0;
        for (const QFileInfo &sub : subEntries) {
            if (sub.fileName().startsWith(QLatin1Char('.'))) continue;
            if (sub.isDir()) ++dirCount;
            else {
                ++fileCount;
                dirSize += sub.size();
            }
        }
        form->addRow(tr("Type:"), createSelectableLabel(tr("Folder")));
        form->addRow(tr("Contents:"), createSelectableLabel(
            tr("%1 items (%2 files, %3 folders)").arg(fileCount + dirCount).arg(fileCount).arg(dirCount)));
        form->addRow(tr("Direct Size:"), createSelectableLabel(QLocale().formattedDataSize(dirSize)));
    } else if (fi.exists()) {
        QString sizeStr = QString("%1 (%L2 bytes)").arg(QLocale().formattedDataSize(fi.size())).arg(fi.size());
        form->addRow(tr("File Size:"), createSelectableLabel(sizeStr));

        // MIME Type & Format
        QMimeDatabase mimeDb;
        QMimeType mime = mimeDb.mimeTypeForFile(m_filePath);
        form->addRow(tr("MIME Type:"), createSelectableLabel(mime.name()));
        if (!mime.comment().isEmpty()) {
            form->addRow(tr("Format:"), createSelectableLabel(mime.comment()));
        }
    }

    // Resolution & Aspect Ratio
    if (m_isImageFile && m_imageSize.isValid() && m_imageSize.width() > 0 && m_imageSize.height() > 0) {
        const int w = m_imageSize.width();
        const int h = m_imageSize.height();
        form->addRow(tr("Resolution:"), createSelectableLabel(QString("%1 × %2 pixels").arg(w).arg(h)));

        double mp = (static_cast<double>(w) * h) / 1000000.0;
        form->addRow(tr("Megapixels:"), createSelectableLabel(QString("%1 MP").arg(mp, 0, 'f', 2)));

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

        if (!m_image.isNull()) {
            QString depthStr = QString("%1-bit").arg(m_image.depth());
            if (m_image.hasAlphaChannel()) {
                depthStr += tr(" (32-bit RGBA with Alpha Channel)");
            } else {
                depthStr += tr(" (24-bit RGB Opaque)");
            }
            form->addRow(tr("Color Depth:"), createSelectableLabel(depthStr));
            form->addRow(tr("Transparency:"), createSelectableLabel(m_image.hasAlphaChannel() ? tr("Yes (Alpha Channel Present)") : tr("No (Fully Opaque)")));
        }
    }

    // Extra caller-supplied metadata rows (e.g. text line/char counts)
    for (const auto &pair : m_extraFields) {
        form->addRow(pair.first, createSelectableLabel(pair.second));
    }

    // Dates & Permissions
    if (fi.exists()) {
        form->addRow(tr("Created:"), createSelectableLabel(fi.birthTime().isValid() ? QLocale().toString(fi.birthTime(), QLocale::ShortFormat) : tr("Unknown")));
        form->addRow(tr("Last Modified:"), createSelectableLabel(QLocale().toString(fi.lastModified(), QLocale::ShortFormat)));
        auto *permLbl = createSelectableLabel(formatUnixPermissions(fi.permissions()));
        permLbl->setStyleSheet(QStringLiteral("font-family: monospace; font-weight: 600;"));
        form->addRow(tr("Permissions:"), permLbl);
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
