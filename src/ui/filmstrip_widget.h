/**
 * @file filmstrip_widget.h
 * @brief Horizontal thumbnail filmstrip widget docked at the bottom of the viewer.
 */

#pragma once

#include <QWidget>
#include <QListWidget>
#include <QStringList>
#include <QThread>
#include <atomic>


class FilmstripWorker;

/**
 * @class FilmstripWidget
 * @brief Horizontal scrolling preview bar displaying cards for all sibling files in the folder.
 *
 * Allows instantaneous visual discovery and click-to-navigate across images, PDFs,
 * text files, and media in the active directory. Thumbnails are generated asynchronously.
 */
class FilmstripWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a FilmstripWidget.
     * @param parent Optional parent widget.
     */
    explicit FilmstripWidget(QWidget *parent = nullptr);

    /**
     * @brief Destructor. Ensures background thumbnail workers terminate safely.
     */
    ~FilmstripWidget() override;

    /**
     * @brief Sets the list of sibling files and highlights the currently active index.
     * @param files Absolute paths of sibling files.
     * @param currentIndex Index of currently viewed file.
     */
    void setFiles(const QStringList &files, int currentIndex);

    /**
     * @brief Updates the active file index and scrolls it into view.
     * @param index New active index.
     */
    void setCurrentIndex(int index);

    /**
     * @brief Checks whether the filmstrip has valid file items.
     * @return True if there are sibling files loaded.
     */
    bool hasFiles() const;

    /**
     * @brief Updates thumbnail icon for a single file item in the filmstrip.
     * @param filePath Absolute path of the file.
     * @param image Updated thumbnail image.
     */
    void updateThumbnail(const QString &filePath, const QImage &image);

signals:
    /**
     * @brief Emitted when the user clicks or selects a thumbnail in the filmstrip.
     * @param filePath Absolute path of the selected file.
     */
    void fileSelected(const QString &filePath);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onThumbnailReady(const QString &filePath, const QImage &thumbnail);

private:
    void setupUi();
    void stopWorker();

    QListWidget *m_listWidget;
    QStringList m_fileList;
    int m_currentIndex;

    FilmstripWorker *m_worker;
};

/**
 * @class FilmstripWorker
 * @brief Background worker generating small raster thumbnails for the filmstrip.
 */
class FilmstripWorker : public QThread
{
    Q_OBJECT

public:
    explicit FilmstripWorker(const QStringList &paths, QObject *parent = nullptr);
    ~FilmstripWorker() override;

    void cancel();

signals:
    void thumbnailReady(const QString &path, const QImage &thumb);

protected:
    void run() override;

private:
    QStringList m_paths;
    std::atomic<bool> m_cancelled;
};

