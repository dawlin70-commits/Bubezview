#pragma once

#include "frameless_dialog.h"
#include <QImage>
#include <QSize>
#include <QString>
#include <QList>
#include <QPair>
#include "core/services/exif_parser.h"

class QTabWidget;
class QLabel;
class QPushButton;

/**
 * @brief Unified property inspector dialog for files, folders, and images.
 * Displays general filesystem/MIME metadata and permissions for any item, and adds
 * technical image metrics, camera EXIF parameters, and GPS location tabs for images.
 */
class ImageInspectorDialog : public FramelessDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the inspector dialog.
     * @param filePath Absolute path to the inspected file or folder.
     * @param image Optional already-loaded QImage instance (used for dimensions, format, and color depth).
     * @param parent Optional parent widget.
     * @param extraFields Optional additional key/value metadata rows (e.g. text lines/chars).
     */
    explicit ImageInspectorDialog(const QString &filePath,
                                  const QImage &image = QImage(),
                                  QWidget *parent = nullptr,
                                  const QList<QPair<QString, QString>> &extraFields = {});

private:
    void setupUi();
    QWidget *createGeneralTab();
    QWidget *createCameraTab();
    QWidget *createGpsTab();

    QString m_filePath;
    QImage m_image;
    QSize m_imageSize;
    bool m_isImageFile = false;
    ExifData m_exif;
    QList<QPair<QString, QString>> m_extraFields;

    QTabWidget *m_tabWidget = nullptr;
};
