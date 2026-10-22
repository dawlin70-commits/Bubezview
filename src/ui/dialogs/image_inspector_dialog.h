#pragma once

#include <QDialog>
#include <QImage>
#include <QString>
#include "core/services/exif_parser.h"

class QTabWidget;
class QLabel;
class QPushButton;

/**
 * @brief Rich property inspector dialog for image files, displaying technical metadata,
 * camera EXIF parameters, and GPS location coordinates with interactive map lookup.
 */
class ImageInspectorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the inspector dialog.
     * @param filePath Absolute path to the inspected image file.
     * @param image Loaded QImage instance (used for dimensions, format, and color depth).
     * @param parent Optional parent widget.
     */
    explicit ImageInspectorDialog(const QString &filePath, const QImage &image, QWidget *parent = nullptr);

private:
    void setupUi();
    QWidget *createGeneralTab();
    QWidget *createCameraTab();
    QWidget *createGpsTab();

    QString m_filePath;
    QImage m_image;
    ExifData m_exif;

    QTabWidget *m_tabWidget = nullptr;
};
