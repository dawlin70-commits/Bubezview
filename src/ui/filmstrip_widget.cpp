#include "filmstrip_widget.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QImageReader>
#include <poppler/qt6/poppler-qt6.h>
#include <QScrollBar>

// ============================================================================
// FilmstripWidget Implementation
// ============================================================================

FilmstripWidget::FilmstripWidget(QWidget *parent)
    : QWidget(parent)
    , m_listWidget(nullptr)
    , m_currentIndex(-1)
    , m_worker(nullptr)
{
    setFixedHeight(102);
    setupUi();
}

FilmstripWidget::~FilmstripWidget()
{
    stopWorker();
}

void FilmstripWidget::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setFlow(QListView::LeftToRight);
    m_listWidget->setMovement(QListView::Static);
    m_listWidget->setWrapping(false);
    m_listWidget->setSpacing(6);
    m_listWidget->setIconSize(QSize(60, 48));
    m_listWidget->setGridSize(QSize(82, 88));
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_listWidget->setStyleSheet(
        "QListWidget {"
        "   background-color: #16181b;"
        "   border: none;"
        "   border-top: 1px solid #2d333b;"
        "   padding: 4px 8px;"
        "   color: #94a3b8;"
        "   font-size: 11px;"
        "}"
        "QListWidget::item {"
        "   background-color: #21252b;"
        "   border: 1px solid #333942;"
        "   border-radius: 6px;"
        "   padding: 4px;"
        "   margin: 2px;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #2c323b;"
        "   border-color: #4b5563;"
        "   color: #f1f5f9;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: rgba(56, 189, 248, 0.22);"
        "   border: 1.8px solid #38bdf8;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "}"
        "QScrollBar:horizontal {"
        "   background: #16181b;"
        "   height: 6px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: #3e4451;"
        "   min-width: 24px;"
        "   border-radius: 3px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "   background: #525c6b;"
        "}"
    );

    connect(m_listWidget, &QListWidget::itemClicked, this, &FilmstripWidget::onItemClicked);

    layout->addWidget(m_listWidget);
}

void FilmstripWidget::stopWorker()
{
    if (m_worker) {
        m_worker->cancel();
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
}

void FilmstripWidget::setFiles(const QStringList &files, int currentIndex)
{
    stopWorker();
    m_fileList = files;
    m_currentIndex = currentIndex;
    m_listWidget->clear();

    if (files.isEmpty()) return;

    QFileIconProvider iconProvider;
    QStringList candidatesForThumbs;

    for (int i = 0; i < files.size(); ++i) {
        const QString &path = files.at(i);
        QFileInfo fi(path);

        // Truncate overly long file names for clean card display
        QString dispName = fi.fileName();
        if (dispName.length() > 11) {
            dispName = dispName.left(8) + "…";
        }

        QIcon initialIcon = iconProvider.icon(fi);
        QListWidgetItem *item = new QListWidgetItem(initialIcon, dispName, m_listWidget);
        item->setData(Qt::UserRole, path);
        item->setToolTip(fi.fileName());
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

        QString ext = fi.suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" ||
            ext == "bmp" || ext == "svg" || ext == "gif" || ext == "pdf") {
            candidatesForThumbs.append(path);
        }
    }

    setCurrentIndex(currentIndex);

    // Launch background thumbnail generation
    if (!candidatesForThumbs.isEmpty()) {
        m_worker = new FilmstripWorker(candidatesForThumbs, this);
        connect(m_worker, &FilmstripWorker::thumbnailReady, this, &FilmstripWidget::onThumbnailReady);
        m_worker->start();
    }
}

void FilmstripWidget::setCurrentIndex(int index)
{
    m_currentIndex = index;
    if (index >= 0 && index < m_listWidget->count()) {
        m_listWidget->setCurrentRow(index);
        QListWidgetItem *item = m_listWidget->item(index);
        if (item) {
            m_listWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }
}

bool FilmstripWidget::hasFiles() const
{
    return !m_fileList.isEmpty();
}

void FilmstripWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit fileSelected(path);
    }
}

void FilmstripWidget::onThumbnailReady(const QString &filePath, const QImage &thumbnail)
{
    if (thumbnail.isNull()) return;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == filePath) {
            item->setIcon(QIcon(QPixmap::fromImage(thumbnail)));
            break;
        }
    }
}

// ============================================================================
// FilmstripWorker Implementation
// ============================================================================

FilmstripWorker::FilmstripWorker(const QStringList &paths, QObject *parent)
    : QThread(parent)
    , m_paths(paths)
    , m_cancelled(false)
{
}

FilmstripWorker::~FilmstripWorker()
{
    cancel();
    wait();
}

void FilmstripWorker::cancel()
{
    m_cancelled.store(true, std::memory_order_relaxed);
}

void FilmstripWorker::run()
{
    static const QStringList rasterExts = {
        "png", "jpg", "jpeg", "webp", "bmp", "gif", "svg"
    };

    for (const QString &path : m_paths) {
        if (m_cancelled.load(std::memory_order_relaxed)) break;

        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        if (rasterExts.contains(ext)) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                QSize scaled = sz.scaled(120, 96, Qt::KeepAspectRatio);
                reader.setScaledSize(scaled);
                QImage img = reader.read();
                if (!img.isNull()) {
                    emit thumbnailReady(path, img);
                }
            }
        } else if (ext == "pdf") {
            std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(path));
            if (doc && !doc->isLocked() && doc->numPages() > 0) {
                std::unique_ptr<Poppler::Page> page(doc->page(0));
                if (page) {
                    QImage img = page->renderToImage(72.0, 72.0);
                    if (!img.isNull()) {
                        emit thumbnailReady(path, img.scaled(120, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
            }
        }
    }
}
