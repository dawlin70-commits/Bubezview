#include "pdf_viewer.h"
#include "pdf_document_ops.h"
#include "../../core/theme_manager.h"
#include "../../ui/pdf_typewriter_dialog.h"
#include "../../ui/pdf_signature_dialog.h"
#include "../../ui/pdf_image_dialog.h"
#include "../../ui/pdf_shape_dialog.h"
#include "../../ui/pdf_watermark_dialog.h"
#include "../../ui/pdf_tools_dialog.h"
#include <poppler/qt6/poppler-qt6.h>
#include <poppler/qt6/poppler-annotation.h>
#include <poppler/qt6/poppler-form.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QScrollBar>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QtGui/QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QDateTime>
#include <QDebug>
#include <cmath>

PdfViewer::PdfViewer(QWidget *parent)
    : QWidget(parent)
    , m_splitter(nullptr)
    , m_tocContainer(nullptr)
    , m_lblTocTitle(nullptr)
    , m_btnCloseToc(nullptr)
    , m_tocView(nullptr)
    , m_tocModel(nullptr)
    , m_scrollArea(nullptr)
    , m_pageWidget(nullptr)
    , m_navBar(nullptr)
    , m_btnPrevPage(nullptr)
    , m_lblPage(nullptr)
    , m_spinPage(nullptr)
    , m_lblPageCount(nullptr)
    , m_btnNextPage(nullptr)
    , m_currentPage(0)
    , m_pageCount(0)
    , m_zoomFactor(1.0)
    , m_fitToWidthMode(true)
    , m_rotationDegrees(0)
    , m_zoomToolActive(false)
    , m_hasOutline(false)
    , m_isModified(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(2);

    // 1. Table of Contents (TOC) Sidebar
    m_tocContainer = new QWidget(m_splitter);
    QVBoxLayout *tocLayout = new QVBoxLayout(m_tocContainer);
    tocLayout->setContentsMargins(4, 6, 4, 6);
    tocLayout->setSpacing(4);

    QHBoxLayout *tocHeaderLayout = new QHBoxLayout();
    m_lblTocTitle = new QLabel(tr("📑 Document Outline"), m_tocContainer);
    tocHeaderLayout->addWidget(m_lblTocTitle);
    tocHeaderLayout->addStretch();

    m_btnCloseToc = new QPushButton("✕", m_tocContainer);
    m_btnCloseToc->setFixedSize(20, 20);
    m_btnCloseToc->setFlat(true);
    m_btnCloseToc->setCursor(Qt::PointingHandCursor);
    connect(m_btnCloseToc, &QPushButton::clicked, this, &PdfViewer::toggleTocSidebar);
    tocHeaderLayout->addWidget(m_btnCloseToc);
    tocLayout->addLayout(tocHeaderLayout);

    m_tocView = new QTreeView(m_tocContainer);
    m_tocModel = new QStandardItemModel(this);
    m_tocView->setModel(m_tocModel);
    m_tocView->setHeaderHidden(true);
    m_tocView->setAnimated(true);
    m_tocView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_tocView, &QTreeView::clicked, this, &PdfViewer::onTocItemActivated);
    tocLayout->addWidget(m_tocView);

    m_splitter->addWidget(m_tocContainer);
    m_tocContainer->hide(); // Initially hidden until an outline is confirmed

    // 2. Main Page Scroll Canvas
    m_scrollArea = new QScrollArea(m_splitter);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_pageWidget = new PdfPageWidget(m_scrollArea);
    connect(m_pageWidget, &PdfPageWidget::selectionChanged, this, [this](bool hasSel) {
        emit selectionChanged(hasSel);
        emit historyChanged();
    });
    if (m_pageWidget->transformBox()) {
        connect(m_pageWidget->transformBox(), &PdfTransformBox::applied, this, &PdfViewer::historyChanged);
        connect(m_pageWidget->transformBox(), &PdfTransformBox::cancelled, this, &PdfViewer::historyChanged);
    }
    connect(m_pageWidget, &PdfPageWidget::copyDocumentRequested, this, [this]() {
        QApplication::clipboard()->setText(extractAllText());
    });
    connect(m_pageWidget, &PdfPageWidget::addStampAnnotationRequested, this, &PdfViewer::onAddStampAnnotation);
    connect(m_pageWidget, &PdfPageWidget::addStampAnnotationsRequested, this, &PdfViewer::onAddStampAnnotations);
    connect(m_pageWidget, &PdfPageWidget::addTextAnnotationRequested, this, &PdfViewer::onAddTextAnnotation);
    connect(m_pageWidget, &PdfPageWidget::insertImageRequested, this, &PdfViewer::openInsertImageDialog);
    connect(m_pageWidget, &PdfPageWidget::snapshotAreaSelected, this, &PdfViewer::onSnapshotAreaSelected);
    connect(m_pageWidget, &PdfPageWidget::cutAreaRequested, this, &PdfViewer::onCutAreaRequested);
    connect(m_pageWidget, &PdfPageWidget::copyAreaRequested, this, &PdfViewer::onCopyAreaRequested);
    connect(m_pageWidget, &PdfPageWidget::formFieldModified, this, &PdfViewer::onFormFieldModified);
    connect(m_pageWidget, &PdfPageWidget::undoRequested, this, &PdfViewer::undo);
    connect(m_pageWidget, &PdfPageWidget::redoRequested, this, &PdfViewer::redo);
    connect(m_pageWidget, &PdfPageWidget::zoomWheelRequested, this, [this](const QPoint &pagePos, double factor) {
        QPoint viewportPos = m_scrollArea->viewport()->mapFromGlobal(m_pageWidget->mapToGlobal(pagePos));
        zoomAt(viewportPos, factor);
    });
    connect(m_pageWidget, &PdfPageWidget::activePageChanged, this, [this](int pageIdx) {
        if (pageIdx >= 0 && pageIdx < m_pageCount && pageIdx != m_currentPage) {
            m_currentPage = pageIdx;
            if (m_spinPage && m_spinPage->value() != m_currentPage + 1) {
                QSignalBlocker blocker(m_spinPage);
                m_spinPage->setValue(m_currentPage + 1);
            }
            updateNavButtons();
            emit pageChanged(m_currentPage + 1, m_pageCount);
        }
    });

    m_scrollArea->setWidget(m_pageWidget);
    m_splitter->addWidget(m_scrollArea);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, &PdfViewer::onVerticalScrollChanged);

    m_scrollArea->viewport()->installEventFilter(this);
    m_pageWidget->installEventFilter(this);
    if (m_pageWidget->transformBox()) {
        m_pageWidget->transformBox()->installEventFilter(this);
    }

    // Give majority width to the document page
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter);

    // 3. Native Bottom Page Navigation Bar with Reader Pill Container
    m_navBar = new QWidget(this);
    m_navBar->setFixedHeight(44);
    QHBoxLayout *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(8, 4, 8, 4);
    navLayout->setSpacing(0);

    m_pillContainer = new QWidget(m_navBar);
    m_pillContainer->setFixedHeight(32);
    QHBoxLayout *pillLayout = new QHBoxLayout(m_pillContainer);
    pillLayout->setContentsMargins(6, 2, 8, 2);
    pillLayout->setSpacing(6);

    m_btnPrevPage = new QToolButton(m_pillContainer);
    m_btnPrevPage->setText("◀");
    m_btnPrevPage->setToolTip(tr("Previous Page (Page Up / Left Arrow)"));
    m_btnPrevPage->setAutoRaise(true);
    m_btnPrevPage->setFixedSize(26, 26);
    m_btnPrevPage->setCursor(Qt::PointingHandCursor);
    connect(m_btnPrevPage, &QToolButton::clicked, this, &PdfViewer::prevPage);

    m_lblPage = new QLabel(tr("Page"), m_pillContainer);

    m_spinPage = new QSpinBox(m_pillContainer);
    m_spinPage->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spinPage->setRange(1, 1);
    m_spinPage->setValue(1);
    m_spinPage->setAlignment(Qt::AlignCenter);
    m_spinPage->setFixedWidth(46);
    m_spinPage->setFixedHeight(24);
    connect(m_spinPage, &QSpinBox::valueChanged, this, [this](int val) {
        if (val - 1 != m_currentPage) {
            scrollToPage(val - 1);
        }
    });

    m_lblPageCount = new QLabel(tr("of 0"), m_pillContainer);

    m_btnNextPage = new QToolButton(m_pillContainer);
    m_btnNextPage->setText("▶");
    m_btnNextPage->setToolTip(tr("Next Page (Page Down / Right Arrow)"));
    m_btnNextPage->setAutoRaise(true);
    m_btnNextPage->setFixedSize(26, 26);
    m_btnNextPage->setCursor(Qt::PointingHandCursor);
    connect(m_btnNextPage, &QToolButton::clicked, this, &PdfViewer::nextPage);

    pillLayout->addWidget(m_btnPrevPage);
    pillLayout->addWidget(m_lblPage);
    pillLayout->addWidget(m_spinPage);
    pillLayout->addWidget(m_lblPageCount);
    pillLayout->addWidget(m_btnNextPage);

    navLayout->addStretch();
    navLayout->addWidget(m_pillContainer);
    navLayout->addStretch();

    mainLayout->addWidget(m_navBar);

    // Apply active theme styling
    updateTheme();
}

PdfViewer::~PdfViewer()
{
    disconnect();
    if (m_pageWidget) {
        m_pageWidget->disconnect();
    }
    resetDocument();
    clearUndoRedoStacks();
}

void PdfViewer::resetDocument()
{
    if (m_pageWidget) {
        m_pageWidget->clear();
        m_pageWidget->setFilePath(QString());
    }
    m_doc.reset();
}

void PdfViewer::updateTheme()
{
    ThemePreset preset = ThemeManager::currentPreset();

    if (preset == ThemePreset::SystemNative) {
        m_splitter->setStyleSheet("QSplitter::handle { background-color: palette(mid); }");
        if (m_tocContainer) {
            m_tocContainer->setStyleSheet("background-color: palette(window); border-right: 1px solid palette(mid);");
        }
        if (m_lblTocTitle) {
            m_lblTocTitle->setStyleSheet("font-weight: bold; color: palette(window-text); font-size: 11px;");
        }
        if (m_btnCloseToc) {
            m_btnCloseToc->setStyleSheet("color: palette(window-text); font-size: 10px; border: none; padding: 0;");
        }
        if (m_tocView) {
            m_tocView->setStyleSheet(
                "QTreeView { background-color: palette(base); color: palette(text); border: none; font-size: 12px; }"
                "QTreeView::item { padding: 4px 2px; border-radius: 4px; }"
                "QTreeView::item:hover { background-color: palette(alternate-base); color: palette(text); }"
                "QTreeView::item:selected { background-color: palette(highlight); color: palette(highlighted-text); }"
            );
        }
        if (m_scrollArea) {
            // Soft neutral light canvas for native light theme with visible scrollbars
            m_scrollArea->setStyleSheet(
                "QScrollArea { background-color: #e4e7eb; border: none; }\n" +
                ThemeManager::scrollBarStyleSheet(preset)
            );
        }
    } else if (preset == ThemePreset::DarkSlate) {
        m_splitter->setStyleSheet("QSplitter::handle { background-color: #3b3b3b; }");
        if (m_tocContainer) {
            m_tocContainer->setStyleSheet("background-color: #1e2226; border-right: 1px solid #2d333b;");
        }
        if (m_lblTocTitle) {
            m_lblTocTitle->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
        }
        if (m_btnCloseToc) {
            m_btnCloseToc->setStyleSheet("color: #888888; font-size: 10px; border: none; padding: 0;");
        }
        if (m_tocView) {
            m_tocView->setStyleSheet(
                "QTreeView { background-color: #1e2226; color: #cccccc; border: none; font-size: 12px; }"
                "QTreeView::item { padding: 4px 2px; border-radius: 4px; }"
                "QTreeView::item:hover { background-color: #2d333b; color: white; }"
                "QTreeView::item:selected { background-color: #383c42; color: white; }"
            );
        }
        if (m_scrollArea) {
            m_scrollArea->setStyleSheet(
                "QScrollArea { background-color: #22262b; border: none; }\n" +
                ThemeManager::scrollBarStyleSheet(preset)
            );
        }
    } else { // BreezeDark
        m_splitter->setStyleSheet("QSplitter::handle { background-color: #3b3b3b; }");
        if (m_tocContainer) {
            m_tocContainer->setStyleSheet("background-color: #1e1e1e; border-right: 1px solid #333333;");
        }
        if (m_lblTocTitle) {
            m_lblTocTitle->setStyleSheet("font-weight: bold; color: #d4d4d4; font-size: 11px;");
        }
        if (m_btnCloseToc) {
            m_btnCloseToc->setStyleSheet("color: #888888; font-size: 10px; border: none; padding: 0;");
        }
        if (m_tocView) {
            m_tocView->setStyleSheet(
                "QTreeView { background-color: #1e1e1e; color: #cccccc; border: none; font-size: 12px; }"
                "QTreeView::item { padding: 4px 2px; border-radius: 4px; }"
                "QTreeView::item:hover { background-color: #2a2d2e; color: white; }"
                "QTreeView::item:selected { background-color: #04395e; color: white; }"
            );
        }
        if (m_scrollArea) {
            m_scrollArea->setStyleSheet(
                "QScrollArea { background-color: #2b2b2b; border: none; }\n" +
                ThemeManager::scrollBarStyleSheet(preset)
            );
        }
    }

    if (m_navBar) {
        if (preset == ThemePreset::SystemNative) {
            m_navBar->setStyleSheet("background-color: transparent; border: none;");
            if (m_pillContainer) {
                m_pillContainer->setStyleSheet(
                    "QWidget { background-color: palette(window); border: 1px solid palette(mid); border-radius: 16px; }"
                );
            }
            if (m_btnPrevPage) {
                m_btnPrevPage->setStyleSheet(
                    "QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 13px; font-weight: bold; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background-color: palette(alternate-base); border-color: palette(mid); }"
                    "QToolButton:pressed { background-color: palette(mid); }"
                    "QToolButton:disabled { color: palette(mid); }"
                );
            }
            if (m_btnNextPage) {
                m_btnNextPage->setStyleSheet(
                    "QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 13px; font-weight: bold; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background-color: palette(alternate-base); border-color: palette(mid); }"
                    "QToolButton:pressed { background-color: palette(mid); }"
                    "QToolButton:disabled { color: palette(mid); }"
                );
            }
            if (m_lblPage) m_lblPage->setStyleSheet("background: transparent; color: palette(window-text); font-size: 11px; font-weight: 500; border: none;");
            if (m_lblPageCount) m_lblPageCount->setStyleSheet("background: transparent; color: palette(window-text); font-size: 11px; font-weight: 500; border: none;");
            if (m_spinPage) {
                m_spinPage->setStyleSheet(
                    "QSpinBox { background-color: palette(base); color: palette(text); border: 1px solid palette(mid); border-radius: 6px; font-size: 12px; font-weight: bold; padding: 1px 2px; }"
                    "QSpinBox:focus { border: 1px solid palette(highlight); }"
                );
            }
        } else if (preset == ThemePreset::DarkSlate) {
            m_navBar->setStyleSheet("background-color: transparent; border: none;");
            if (m_pillContainer) {
                m_pillContainer->setStyleSheet(
                    "QWidget { background-color: #21262d; border: 1px solid #30363d; border-radius: 16px; }"
                );
            }
            if (m_btnPrevPage) {
                m_btnPrevPage->setStyleSheet(
                    "QToolButton { color: #c9d1d9; background: transparent; border: 1px solid transparent; border-radius: 13px; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background: #30363d; border-color: #484f58; color: #58a6ff; }"
                    "QToolButton:pressed { background: #161b22; }"
                    "QToolButton:disabled { color: #484f58; }"
                );
            }
            if (m_btnNextPage) {
                m_btnNextPage->setStyleSheet(
                    "QToolButton { color: #c9d1d9; background: transparent; border: 1px solid transparent; border-radius: 13px; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background: #30363d; border-color: #484f58; color: #58a6ff; }"
                    "QToolButton:pressed { background: #161b22; }"
                    "QToolButton:disabled { color: #484f58; }"
                );
            }
            if (m_lblPage) m_lblPage->setStyleSheet("background: transparent; color: #8b949e; font-size: 11px; font-weight: 500; border: none;");
            if (m_lblPageCount) m_lblPageCount->setStyleSheet("background: transparent; color: #8b949e; font-size: 11px; font-weight: 500; border: none;");
            if (m_spinPage) {
                m_spinPage->setStyleSheet(
                    "QSpinBox { background: #0d1117; color: #f0f6fc; border: 1px solid #30363d; border-radius: 6px; font-size: 12px; font-weight: bold; padding: 1px 2px; }"
                    "QSpinBox:focus { border: 1px solid #58a6ff; }"
                );
            }
        } else { // BreezeDark
            m_navBar->setStyleSheet("background-color: transparent; border: none;");
            if (m_pillContainer) {
                m_pillContainer->setStyleSheet(
                    "QWidget { background-color: #2a2e32; border: 1px solid #383e44; border-radius: 16px; }"
                );
            }
            if (m_btnPrevPage) {
                m_btnPrevPage->setStyleSheet(
                    "QToolButton { color: #eff0f1; background: transparent; border: 1px solid transparent; border-radius: 13px; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background: #31363b; border-color: #3daee9; color: #3daee9; }"
                    "QToolButton:pressed { background: #232629; }"
                    "QToolButton:disabled { color: #555a5f; }"
                );
            }
            if (m_btnNextPage) {
                m_btnNextPage->setStyleSheet(
                    "QToolButton { color: #eff0f1; background: transparent; border: 1px solid transparent; border-radius: 13px; font-size: 11px; padding: 0px; }"
                    "QToolButton:hover { background: #31363b; border-color: #3daee9; color: #3daee9; }"
                    "QToolButton:pressed { background: #232629; }"
                    "QToolButton:disabled { color: #555a5f; }"
                );
            }
            if (m_lblPage) m_lblPage->setStyleSheet("background: transparent; color: #bdc3c7; font-size: 11px; font-weight: 500; border: none;");
            if (m_lblPageCount) m_lblPageCount->setStyleSheet("background: transparent; color: #bdc3c7; font-size: 11px; font-weight: 500; border: none;");
            if (m_spinPage) {
                m_spinPage->setStyleSheet(
                    "QSpinBox { background: #1b1e20; color: #eff0f1; border: 1px solid #383e44; border-radius: 6px; font-size: 12px; font-weight: bold; padding: 1px 2px; }"
                    "QSpinBox:focus { border: 1px solid #3daee9; }"
                );
            }
        }
    }

    if (m_pageWidget) {
        m_pageWidget->updateTheme();
    }
}

bool PdfViewer::loadFile(const QString &filePath, bool preserveHistory)
{
    const QString targetPath = filePath;
    QList<QString> savedUndo;
    QList<QString> savedRedo;
    if (preserveHistory) {
        savedUndo = m_undoStack;
        savedRedo = m_redoStack;
        m_undoStack.clear();
        m_redoStack.clear();
    }

    clear();
    if (preserveHistory) {
        m_undoStack = savedUndo;
        m_redoStack = savedRedo;
    }

    m_filePath = targetPath;

    m_doc = Poppler::Document::load(targetPath);
    if (!m_doc || m_doc->isLocked()) {
        resetDocument();
        return false;
    }

    // Set render hints for crisp text rendering
    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = 0;
    m_zoomFactor = 1.0;
    m_fitToWidthMode = true;
    m_rotationDegrees = 0;

    populateOutline();
    renderCurrentPage();
    updateNavButtons();
    emit pageChanged(m_currentPage + 1, m_pageCount);
    m_isModified = false;
    emit documentModified(false);
    return true;
}

bool PdfViewer::loadDocument(std::unique_ptr<Poppler::Document> doc, const QString &filePath, const QImage &initialPageImage)
{
    Q_UNUSED(initialPageImage);
    clear();

    m_filePath = filePath;
    m_doc = std::move(doc);
    if (!m_doc || m_doc->isLocked()) {
        resetDocument();
        return false;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = 0;
    m_zoomFactor = 1.0;
    m_fitToWidthMode = true;
    m_rotationDegrees = 0;
    m_zoomToolActive = false;

    populateOutline();
    renderCurrentPage();
    updateNavButtons();

    m_isModified = false;
    emit documentModified(false);
    emit pageChanged(m_currentPage + 1, m_pageCount);
    return true;
}

void PdfViewer::clear()
{
    resetDocument();
    m_filePath.clear();
    m_currentPage = 0;
    m_pageCount = 0;
    m_zoomFactor = 1.0;
    m_hasOutline = false;
    m_isModified = false;
    m_tocModel->clear();
    m_tocContainer->hide();
    clearUndoRedoStacks();
    if (m_spinPage) {
        QSignalBlocker blocker(m_spinPage);
        m_spinPage->setRange(1, 1);
        m_spinPage->setValue(1);
    }
    updateNavButtons();
    emit outlineAvailabilityChanged(false);
    emit selectionChanged(false);
    emit documentModified(false);
}

QSize PdfViewer::pageSize() const
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) {
        return QSize();
    }
    std::unique_ptr<Poppler::Page> page(m_doc->page(m_currentPage));
    if (!page) return QSize();
    return page->pageSize();
}

QImage PdfViewer::renderPageForOcr(int pageIndex, int dpi) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) {
        return QImage();
    }
    std::unique_ptr<Poppler::Page> page(m_doc->page(pageIndex));
    if (!page) return QImage();

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    return page->renderToImage(dpi, dpi, -1, -1, -1, -1, r);
}

QImage PdfViewer::renderCurrentPageForOcr(int dpi) const
{
    return renderPageForOcr(m_currentPage, dpi);
}

void PdfViewer::renderCurrentPage(int /*transitionDirection*/)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) {
        m_pageWidget->clear();
        updateNavButtons();
        return;
    }

    int availWidth = m_scrollArea->viewport()->width() - 36;
    m_pageWidget->setDocument(m_doc.get(), m_zoomFactor, m_fitToWidthMode, m_rotationDegrees, availWidth);
    m_pageWidget->setFilePath(m_filePath);
    m_pageWidget->setActivePage(m_currentPage);
    updateNavButtons();
}

void PdfViewer::populateOutline()
{
    m_tocModel->clear();
    if (!m_doc) {
        m_hasOutline = false;
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    QVector<Poppler::OutlineItem> rootItems = m_doc->outline();
    if (rootItems.isEmpty()) {
        m_hasOutline = false;
        m_tocContainer->hide();
        emit outlineAvailabilityChanged(false);
        return;
    }

    m_hasOutline = true;
    buildOutlineBranch(m_tocModel->invisibleRootItem(), rootItems);
    emit outlineAvailabilityChanged(true);
}

void PdfViewer::buildOutlineBranch(QStandardItem *parentItem, const QVector<Poppler::OutlineItem> &items)
{
    for (const auto &item : items) {
        if (item.isNull()) continue;

        QStandardItem *treeItem = new QStandardItem(item.name());
        auto dest = item.destination();
        if (dest) {
            // Save target page number (1-based) in user role data
            treeItem->setData(dest->pageNumber(), Qt::UserRole);
        }

        parentItem->appendRow(treeItem);

        if (item.hasChildren()) {
            buildOutlineBranch(treeItem, item.children());
        }
    }
}

void PdfViewer::onTocItemActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QVariant pageVar = index.data(Qt::UserRole);
    if (pageVar.isValid()) {
        int pageNum = pageVar.toInt();
        if (pageNum >= 1 && pageNum <= m_pageCount) {
            setPage(pageNum - 1);
        }
    }
}

bool PdfViewer::isTocVisible() const
{
    return m_tocContainer && m_tocContainer->isVisible();
}

void PdfViewer::toggleTocSidebar()
{
    if (!m_tocContainer) return;
    setTocVisible(!m_tocContainer->isVisible());
}

void PdfViewer::setTocVisible(bool visible)
{
    if (!m_tocContainer) return;
    if (visible && !m_hasOutline) {
        // Document has no outline
        return;
    }

    m_tocContainer->setVisible(visible);
    if (visible) {
        // Set reasonable sidebar width ratio
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() >= 2 && sizes[0] < 50) {
            sizes[0] = 220;
            sizes[1] = std::max(300, sizes[1] - 220);
            m_splitter->setSizes(sizes);
        }
        m_tocView->expandAll();
    }
}

void PdfViewer::copySelectedText()
{
    if (m_pageWidget) {
        m_pageWidget->copySelectionToClipboard();
    }
}

void PdfViewer::cutSelectedText()
{
    if (m_pageWidget) {
        if (m_pageWidget->hasAreaSelection()) {
            m_pageWidget->cutSelectedArea();
        } else {
            m_pageWidget->cutSelection();
        }
    }
}

void PdfViewer::pasteText()
{
    if (m_pageWidget) {
        m_pageWidget->pasteFromClipboard(m_pageWidget->visibleCenter());
    }
}

bool PdfViewer::hasTextSelection() const
{
    return m_pageWidget && m_pageWidget->hasSelection();
}

bool PdfViewer::hasAreaSelection() const
{
    return m_pageWidget && m_pageWidget->hasAreaSelection();
}

void PdfViewer::startSelectArea()
{
    if (m_pageWidget) {
        m_pageWidget->startSelectArea();
    }
}

int PdfViewer::extractEmbeddedImages(const QString &targetDir)
{
    return PdfDocumentOps::extractEmbeddedImages(m_filePath, targetDir);
}


void PdfViewer::rotateClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 90) % 360;
    renderCurrentPage();
    setModified(true);
}

void PdfViewer::rotateCounterClockwise()
{
    m_rotationDegrees = (m_rotationDegrees + 270) % 360;
    renderCurrentPage();
    setModified(true);
}

void PdfViewer::prepareForManualZoom()
{
    if (m_fitToWidthMode && m_doc && m_currentPage >= 0 && m_currentPage < m_pageCount) {
        std::unique_ptr<Poppler::Page> page(m_doc->page(m_currentPage));
        if (page) {
            QSize originalSize = page->pageSize();
            int availWidth = m_scrollArea->viewport()->width() - 40;
            if (availWidth > 100 && originalSize.width() > 0) {
                double targetDpi = (static_cast<double>(availWidth) / originalSize.width()) * 72.0;
                m_zoomFactor = targetDpi / 96.0;
            }
        }
        m_fitToWidthMode = false;
    }
}

void PdfViewer::zoomAt(const QPoint &viewportPos, double factor)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) return;
    prepareForManualZoom();
    m_fitToWidthMode = false;

    QPoint widgetPos = m_pageWidget->mapFrom(m_scrollArea->viewport(), viewportPos);
    double relX = 0.5;
    double relY = 0.5;
    if (m_pageWidget->width() > 0 && m_pageWidget->height() > 0) {
        relX = qBound(0.0, static_cast<double>(widgetPos.x()) / m_pageWidget->width(), 1.0);
        relY = qBound(0.0, static_cast<double>(widgetPos.y()) / m_pageWidget->height(), 1.0);
    }

    m_zoomFactor = std::max(0.2, std::min(6.0, m_zoomFactor * factor));
    renderCurrentPage();

    int newX = qRound(relX * m_pageWidget->width()) - viewportPos.x();
    int newY = qRound(relY * m_pageWidget->height()) - viewportPos.y();

    m_scrollArea->horizontalScrollBar()->setValue(newX);
    m_scrollArea->verticalScrollBar()->setValue(newY);

    emit zoomChanged(m_zoomFactor);
}

void PdfViewer::toggleZoomTool(bool active)
{
    m_zoomToolActive = active;
    if (m_pageWidget) {
        m_pageWidget->setToolMode(m_zoomToolActive ? PdfToolMode::Zoom : PdfToolMode::SelectText);
    }
    if (m_scrollArea && m_scrollArea->viewport()) {
        if (m_zoomToolActive) {
            QPixmap zoomPix = QIcon::fromTheme("zoom-in").pixmap(24, 24);
            QCursor c = zoomPix.isNull() ? Qt::CrossCursor : QCursor(zoomPix, 8, 8);
            m_scrollArea->viewport()->setCursor(c);
        } else {
            m_scrollArea->viewport()->unsetCursor();
        }
    }
}


void PdfViewer::nextPage()
{
    if (!m_doc || m_currentPage >= m_pageCount - 1) return;
    scrollToPage(m_currentPage + 1);
}

void PdfViewer::prevPage()
{
    if (!m_doc || m_currentPage <= 0) return;
    scrollToPage(m_currentPage - 1);
}

void PdfViewer::setPage(int page)
{
    scrollToPage(page);
}

void PdfViewer::scrollToPage(int pageIndex)
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount || !m_pageWidget) return;

    m_currentPage = pageIndex;
    m_pageWidget->setActivePage(pageIndex);

    int topY = m_pageWidget->pageTopY(pageIndex);
    if (topY >= 0 && m_scrollArea && m_scrollArea->verticalScrollBar()) {
        m_scrollArea->verticalScrollBar()->setValue(topY - 8);
    }

    if (m_spinPage && m_spinPage->value() != m_currentPage + 1) {
        QSignalBlocker blocker(m_spinPage);
        m_spinPage->setValue(m_currentPage + 1);
    }
    updateNavButtons();
    emit pageChanged(m_currentPage + 1, m_pageCount);
}

void PdfViewer::onVerticalScrollChanged(int /*value*/)
{
    if (!m_doc || m_pageCount <= 0 || !m_pageWidget || !m_scrollArea) return;

    int vpCenterY = m_scrollArea->verticalScrollBar()->value() + m_scrollArea->viewport()->height() / 3;
    int pageIdx = m_pageWidget->pageIndexAtY(vpCenterY);
    if (pageIdx >= 0 && pageIdx < m_pageCount && pageIdx != m_currentPage) {
        m_currentPage = pageIdx;
        m_pageWidget->setActivePage(pageIdx);

        if (m_spinPage && m_spinPage->value() != m_currentPage + 1) {
            QSignalBlocker blocker(m_spinPage);
            m_spinPage->setValue(m_currentPage + 1);
        }
        updateNavButtons();
        emit pageChanged(m_currentPage + 1, m_pageCount);
    }
}

void PdfViewer::updateNavButtons()
{
    if (m_btnPrevPage) {
        m_btnPrevPage->setEnabled(m_pageCount > 1 && m_currentPage > 0);
    }
    if (m_btnNextPage) {
        m_btnNextPage->setEnabled(m_pageCount > 1 && m_currentPage < m_pageCount - 1);
    }
    if (m_spinPage) {
        m_spinPage->setEnabled(m_pageCount > 0);
        m_spinPage->setRange(1, std::max(1, m_pageCount));
        if (m_spinPage->value() != m_currentPage + 1) {
            QSignalBlocker blocker(m_spinPage);
            m_spinPage->setValue(m_currentPage + 1);
        }
    }
    if (m_lblPageCount) {
        m_lblPageCount->setText(tr("of %1").arg(m_pageCount));
    }
}

void PdfViewer::zoomIn()
{
    prepareForManualZoom();
    m_fitToWidthMode = false;
    m_zoomFactor = std::min(5.0, m_zoomFactor * 1.25);
    renderCurrentPage();
    emit zoomChanged(m_zoomFactor);
}

void PdfViewer::zoomOut()
{
    prepareForManualZoom();
    m_fitToWidthMode = false;
    m_zoomFactor = std::max(0.2, m_zoomFactor / 1.25);
    renderCurrentPage();
    emit zoomChanged(m_zoomFactor);
}

void PdfViewer::resetZoom()
{
    m_fitToWidthMode = false;
    m_zoomFactor = 1.0;
    renderCurrentPage();
    emit zoomChanged(m_zoomFactor);
}

void PdfViewer::fitToWidth()
{
    m_fitToWidthMode = true;
    m_zoomFactor = 1.0;
    renderCurrentPage();
    emit zoomChanged(m_zoomFactor);
}

void PdfViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWidthMode && m_doc) {
        renderCurrentPage();
    }
}

bool PdfViewer::eventFilter(QObject *watched, QEvent *event)
{
    bool isWheelSource = (watched == m_scrollArea->viewport() ||
                          watched == m_pageWidget ||
                          (m_pageWidget && watched == m_pageWidget->transformBox()));
    if (isWheelSource && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent*>(event);
        if (handleViewportWheel(we, watched)) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool PdfViewer::handleViewportWheel(QWheelEvent *event, QObject *source)
{
    if (!m_doc || m_pageCount <= 0) {
        return false;
    }

    // Ctrl + Wheel or Zoom Tool: Zooming
    if (m_zoomToolActive || (event->modifiers() & Qt::ControlModifier)) {
        double factor = (event->angleDelta().y() > 0) ? 1.25 : (1.0 / 1.25);
        QPoint viewportPos = (source == m_pageWidget)
            ? m_scrollArea->viewport()->mapFromGlobal(m_pageWidget->mapToGlobal(event->position().toPoint()))
            : event->position().toPoint();
        zoomAt(viewportPos, factor);
        event->accept();
        return true;
    }

    // Normal mouse wheel: smooth continuous vertical scroll without sudden snaps
    int deltaY = event->angleDelta().y();
    if (deltaY == 0) {
        return false;
    }

    QScrollBar *vbar = m_scrollArea->verticalScrollBar();
    if (vbar && vbar->maximum() > vbar->minimum()) {
        int step = deltaY / 2;
        vbar->setValue(vbar->value() - step);
        event->accept();
        return true;
    }

    return false;
}

void PdfViewer::wheelEvent(QWheelEvent *event)
{
    if (handleViewportWheel(event, this)) {
        return;
    }
    QWidget::wheelEvent(event);
}

bool PdfViewer::findNext(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (!m_doc || text.isEmpty() || m_pageCount <= 0) return false;

    Poppler::Page::SearchFlags flags = Poppler::Page::NoSearchFlags;
    if (!matchCase) flags |= Poppler::Page::IgnoreCase;

    // Search forward from next page, or current page if multiple occurrences
    int startPage = (m_currentPage + 1) % m_pageCount;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage + step) % m_pageCount;
        auto page = m_doc->page(pageIdx);
        if (page) {
            QList<QRectF> matches = page->search(text, flags);
            if (!matches.isEmpty()) {
                if (wrapped && pageIdx <= m_currentPage && step > 0) {
                    *wrapped = true;
                }
                setPage(pageIdx);
                return true;
            }
        }
    }

    // Also check current page itself if only single page doc
    auto curPage = m_doc->page(m_currentPage);
    if (curPage) {
        QList<QRectF> matches = curPage->search(text, flags);
        if (!matches.isEmpty()) {
            return true;
        }
    }

    return false;
}

bool PdfViewer::findPrev(const QString &text, bool matchCase, bool *wrapped)
{
    if (wrapped) *wrapped = false;
    if (!m_doc || text.isEmpty() || m_pageCount <= 0) return false;

    Poppler::Page::SearchFlags flags = Poppler::Page::NoSearchFlags;
    if (!matchCase) flags |= Poppler::Page::IgnoreCase;

    int startPage = (m_currentPage - 1 + m_pageCount) % m_pageCount;
    for (int step = 0; step < m_pageCount; ++step) {
        int pageIdx = (startPage - step + m_pageCount) % m_pageCount;
        auto page = m_doc->page(pageIdx);
        if (page) {
            QList<QRectF> matches = page->search(text, flags);
            if (!matches.isEmpty()) {
                if (wrapped && pageIdx >= m_currentPage && step > 0) {
                    *wrapped = true;
                }
                setPage(pageIdx);
                return true;
            }
        }
    }

    auto curPage = m_doc->page(m_currentPage);
    if (curPage) {
        QList<QRectF> matches = curPage->search(text, flags);
        if (!matches.isEmpty()) {
            return true;
        }
    }

    return false;
}

QString PdfViewer::extractAllText() const
{
    if (!m_doc || m_pageCount <= 0) return QString();
    QString allText;
    for (int i = 0; i < m_pageCount; ++i) {
        auto page = m_doc->page(i);
        if (page) {
            allText += QString("--- Page %1 ---\n").arg(i + 1);
            allText += page->text(QRectF());
            allText += "\n\n";
        }
    }
    return allText;
}

void PdfViewer::setModified(bool mod)
{
    if (m_isModified != mod) {
        m_isModified = mod;
        emit documentModified(m_isModified);
    }
}

bool PdfViewer::save()
{
    if (m_filePath.isEmpty() || !m_doc) return false;
    return saveAs(m_filePath);
}

bool PdfViewer::saveAs(const QString &targetPath)
{
    if (!m_doc || targetPath.isEmpty()) return false;

    int rot = m_rotationDegrees;
    bool ok = PdfDocumentOps::saveDocumentWithRotation(m_doc.get(), m_filePath, targetPath, rot);
    if (!ok) return false;

    int savedPage = m_currentPage;
    double savedZoom = m_zoomFactor;

    // Reload document cleanly from saved file
    loadFile(targetPath);
    m_currentPage = savedPage;
    m_zoomFactor = savedZoom;
    m_rotationDegrees = rot;
    renderCurrentPage();

    setModified(false);
    return true;
}

bool PdfViewer::exportCurrentPage(const QString &format, const QString &outputPath, int dpi)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) return false;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return false;

    return PdfDocumentOps::exportPage(page.get(), format, outputPath, dpi, m_rotationDegrees);
}

void PdfViewer::openTypewriterDialog()
{
    if (!m_doc) return;
    PdfTypewriterDialog dlg(window());
    if (dlg.exec() == QDialog::Accepted) {
        m_pageWidget->startTypewriter(dlg.text(), dlg.selectedFont(), dlg.textColor(), dlg.isWhiteout());
    }
}

void PdfViewer::openToolsDialog(int defaultTab)
{
    if (!m_doc) return;
    PdfToolsDialog dlg(defaultTab, window());
    if (dlg.exec() == QDialog::Accepted) {
        switch (dlg.selectedAction()) {
        case PdfToolAction::Highlighter:
            startHighlighter(dlg.highlighterColor(), dlg.highlighterOpacity());
            break;
        case PdfToolAction::Shape:
            m_pageWidget->startShape(static_cast<int>(dlg.shapeType()), dlg.shapeStrokeWidth(),
                                     dlg.shapeStrokeColor(), dlg.shapeFillColor());
            break;
        case PdfToolAction::Redact:
            m_pageWidget->startRedaction();
            break;
        case PdfToolAction::Watermark:
            m_pageWidget->startWatermark(dlg.watermarkText(), dlg.watermarkAngle(),
                                         dlg.watermarkOpacity(), dlg.watermarkColor());
            break;
        case PdfToolAction::InsertImage:
            m_pageWidget->startInsertImage(dlg.imageToInsert());
            break;
        case PdfToolAction::Signature: {
            QImage sig = dlg.signatureImage();
            if (!sig.isNull()) {
                m_pageWidget->startSignature(sig);
            }
            break;
        }
        default:
            break;
        }
    }
}

void PdfViewer::startHighlighter(const QColor &color, double opacity)
{
    if (!m_doc) return;
    m_pageWidget->setContinuousMode(true);
    m_pageWidget->startHighlighter(color, opacity);
}

void PdfViewer::startShape(int shapeType, int strokeWidth, const QColor &strokeColor, const QColor &fillColor)
{
    if (!m_doc) return;
    m_pageWidget->setContinuousMode(true);
    m_pageWidget->startShape(shapeType, strokeWidth, strokeColor, fillColor);
}

void PdfViewer::exitContinuousToolMode()
{
    if (m_pageWidget) {
        m_pageWidget->setContinuousMode(false);
        m_pageWidget->cancelActiveTool();
    }
}

void PdfViewer::openSignatureDialog()
{
    if (!m_doc) return;
    PdfSignatureDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QImage sigImg = dlg.signatureImage();
        if (!sigImg.isNull()) {
            m_pageWidget->startSignature(sigImg);
        }
    }
}

void PdfViewer::openWatermarkDialog()
{
    if (!m_doc) return;
    m_pageWidget->startWatermark();
}

void PdfViewer::openShapeDialog()
{
    if (!m_doc) return;
    startShape(0, 3, Qt::red, Qt::transparent);
}

void PdfViewer::startSnapshot()
{
    if (!m_doc) return;
    m_pageWidget->startSnapshot();
}

void PdfViewer::startRedaction()
{
    if (!m_doc) return;
    m_pageWidget->startRedaction();
}

void PdfViewer::onAddStampAnnotations(const QList<QPair<QImage, QRectF>> &stamps)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount || stamps.isEmpty()) return;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return;

    pushUndoState();

    constexpr int MaxStampDim = 1800;
    for (const auto &pair : stamps) {
        QImage stampImg = pair.first;
        if (stampImg.width() > MaxStampDim || stampImg.height() > MaxStampDim) {
            stampImg = stampImg.scaled(MaxStampDim, MaxStampDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        auto *stamp = new Poppler::StampAnnotation();
        stamp->setStampCustomImage(stampImg);
        stamp->setBoundary(pair.second);
        page->addAnnotation(stamp);
        delete stamp;
    }

    setModified(true);
    renderCurrentPage();
}

void PdfViewer::onAddStampAnnotation(const QImage &image, const QRectF &normalizedRect)
{
    onAddStampAnnotations({{image, normalizedRect}});
}

void PdfViewer::onAddTextAnnotation(const QString &text, const QFont &font, const QColor &color,
                                    const QRectF &normalizedRect, bool whiteout)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount || text.isEmpty()) return;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return;

    pushUndoState();

    // If whiteout is requested, place a clean white stamp underneath the text box
    if (whiteout) {
        QImage whiteImg(16, 16, QImage::Format_RGB32);
        whiteImg.fill(Qt::white);
        auto *whiteStamp = new Poppler::StampAnnotation();
        whiteStamp->setStampCustomImage(whiteImg);
        whiteStamp->setBoundary(normalizedRect);
        page->addAnnotation(whiteStamp);
        delete whiteStamp;
    }

    // Create native vector FreeText / TypeWriter annotation without any border box
    auto *ann = new Poppler::TextAnnotation(Poppler::TextAnnotation::InPlace);
    ann->setInplaceIntent(Poppler::TextAnnotation::TypeWriter);
    ann->setInplaceAlign(Poppler::TextAnnotation::InplaceAlignLeft);
    ann->setContents(text);

    QFont annotFont = font;
    if (annotFont.bold() && annotFont.italic()) {
        annotFont.setStyleName(QStringLiteral("Bold Italic"));
    } else if (annotFont.bold()) {
        annotFont.setStyleName(QStringLiteral("Bold"));
    } else if (annotFont.italic()) {
        annotFont.setStyleName(QStringLiteral("Italic"));
    }

    ann->setTextFont(annotFont);
    ann->setTextColor(color);
    ann->setBoundary(normalizedRect);

    // Ensure zero border stroke around the text
    Poppler::Annotation::Style annotStyle = ann->style();
    annotStyle.setWidth(0.0);
    annotStyle.setColor(QColor(0, 0, 0, 0));
    ann->setStyle(annotStyle);

    page->addAnnotation(ann);
    delete ann;

    setModified(true);
    renderCurrentPage();
}

void PdfViewer::onSnapshotAreaSelected(const QRectF &pointRect)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) return;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return;

    // Convert 72 DPI point coordinates to 200 DPI pixel coordinates for fast, high-res capture
    double scale = 200.0 / 72.0;
    int x = qRound(pointRect.x() * scale);
    int y = qRound(pointRect.y() * scale);
    int w = qRound(pointRect.width() * scale);
    int h = qRound(pointRect.height() * scale);

    if (w <= 0 || h <= 0) return;

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    QImage snapImg = page->renderToImage(200.0, 200.0, x, y, w, h, r);
    if (!snapImg.isNull()) {
        QApplication::clipboard()->setImage(snapImg);
        QToolTip::showText(QCursor::pos(), tr("📸 Snapshot copied to clipboard at 200 DPI (%1 × %2 px)").arg(w).arg(h));
    }
}

void PdfViewer::onCopyAreaRequested(const QRectF &pointRect)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) return;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return;

    // Convert 72 DPI point coordinates to 200 DPI pixel coordinates
    double scale = 200.0 / 72.0;
    int x = qRound(pointRect.x() * scale);
    int y = qRound(pointRect.y() * scale);
    int w = qRound(pointRect.width() * scale);
    int h = qRound(pointRect.height() * scale);

    if (w <= 0 || h <= 0) return;

    Poppler::Page::Rotation r = Poppler::Page::Rotate0;
    if (m_rotationDegrees == 90) r = Poppler::Page::Rotate90;
    else if (m_rotationDegrees == 180) r = Poppler::Page::Rotate180;
    else if (m_rotationDegrees == 270) r = Poppler::Page::Rotate270;

    QImage snapImg = page->renderToImage(200.0, 200.0, x, y, w, h, r);
    if (!snapImg.isNull()) {
        QApplication::clipboard()->setImage(snapImg);
        QToolTip::showText(QCursor::pos(), tr("📋 Area copied to clipboard at 200 DPI (%1 × %2 px)").arg(w).arg(h));
    }
}

void PdfViewer::onCutAreaRequested(const QRectF &pointRect)
{
    if (!m_doc || m_currentPage < 0 || m_currentPage >= m_pageCount) return;

    std::unique_ptr<Poppler::Page> page = m_doc->page(m_currentPage);
    if (!page) return;

    // 1. Copy area to clipboard at 200 DPI
    onCopyAreaRequested(pointRect);

    // 2. Whiteout the cut area cleanly on the PDF with lightweight stamp
    QSizeF pageSizePt = page->pageSizeF();
    if (pageSizePt.width() > 0 && pageSizePt.height() > 0) {
        QRectF normRect(pointRect.x() / pageSizePt.width(),
                        pointRect.y() / pageSizePt.height(),
                        pointRect.width() / pageSizePt.width(),
                        pointRect.height() / pageSizePt.height());

        QImage whiteImg(16, 16, QImage::Format_RGB32);
        whiteImg.fill(Qt::white);

        onAddStampAnnotation(whiteImg, normRect);
        QToolTip::showText(QCursor::pos(), tr("✂️ Area cut. Press Ctrl+Z to undo anytime."));
    }
}

void PdfViewer::openInsertImageDialog()
{
    if (!m_doc) return;
    PdfImageDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QImage img = dlg.selectedImage();
        if (!img.isNull()) {
            m_pageWidget->startInsertImage(img);
        }
    }
}

bool PdfViewer::insertBlankPage(int afterPageIndex)
{
    if (m_filePath.isEmpty()) return false;
    QString srcPath = currentDocumentSourcePath();
    if (!QFile::exists(srcPath)) return false;

    pushUndoState();

    int targetIndex = (afterPageIndex >= 0) ? afterPageIndex : m_currentPage;
    QString outPath;
    QString errorMsg;
    bool ok = PdfDocumentOps::insertBlankPage(srcPath, m_pageCount, targetIndex, outPath, errorMsg);
    if (!ok) {
        if (!errorMsg.isEmpty()) {
            QMessageBox::warning(window(), tr("QPDF Required"), errorMsg);
        }
        return false;
    }

    // Atomically load new document without touching user's source file on disk
    int newTargetPage = targetIndex + 1;
    if (!m_activeSnapshotPath.isEmpty() && QFile::exists(m_activeSnapshotPath) &&
        !m_undoStack.contains(m_activeSnapshotPath) && !m_redoStack.contains(m_activeSnapshotPath)) {
        QString toRemove = m_activeSnapshotPath;
        resetDocument();
        QFile::remove(toRemove);
    } else {
        resetDocument();
    }

    m_activeSnapshotPath = outPath;
    m_doc = Poppler::Document::load(m_activeSnapshotPath);
    if (!m_doc || m_doc->isLocked()) {
        resetDocument();
        return false;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = std::min(newTargetPage, m_pageCount - 1);
    populateOutline();
    renderCurrentPage();
    updateNavButtons();
    emit pageChanged(m_currentPage + 1, m_pageCount);
    setModified(true);
    return true;
}

void PdfViewer::onFormFieldModified()
{
    setModified(true);
}

QString PdfViewer::currentDocumentSourcePath() const
{
    if (!m_activeSnapshotPath.isEmpty() && QFile::exists(m_activeSnapshotPath)) {
        return m_activeSnapshotPath;
    }
    return m_filePath;
}

void PdfViewer::clearUndoRedoStacks()
{
    for (const QString &path : m_undoStack) {
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
    m_undoStack.clear();

    for (const QString &path : m_redoStack) {
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
    m_redoStack.clear();

    if (!m_activeSnapshotPath.isEmpty() && QFile::exists(m_activeSnapshotPath)) {
        resetDocument();
        QFile::remove(m_activeSnapshotPath);
        m_activeSnapshotPath.clear();
    }
    emit historyChanged();
}

void PdfViewer::pushUndoState()
{
    if (!m_doc) return;

    // Create a temporary snapshot of the current PDF document state
    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_undo_XXXXXX.pdf");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) return;
    QString snapshotPath = tempFile.fileName();
    tempFile.close();
    QFile::remove(snapshotPath);

    bool saved = false;
    // Fast path: if this is the very first modification on a pristine file, copy directly
    if (m_undoStack.isEmpty() && m_activeSnapshotPath.isEmpty() && !m_isModified && !m_filePath.isEmpty() && QFile::exists(m_filePath)) {
        saved = QFile::copy(m_filePath, snapshotPath);
    }
    if (!saved) {
        std::unique_ptr<Poppler::PDFConverter> conv = m_doc->pdfConverter();
        if (conv) {
            conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
            conv->setOutputFileName(snapshotPath);
            saved = conv->convert();
        }
    }
    if (!saved) {
        if (QFile::exists(snapshotPath)) QFile::remove(snapshotPath);
        return;
    }

    // Clear redo stack on new action and purge its temp files
    for (const QString &path : m_redoStack) {
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
    m_redoStack.clear();

    m_undoStack.append(snapshotPath);
    while (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
        QString oldest = m_undoStack.takeFirst();
        if (QFile::exists(oldest)) {
            QFile::remove(oldest);
        }
    }
    m_isModified = true;
    emit documentModified(true);
    emit historyChanged();
}

bool PdfViewer::canUndo() const
{
    if (m_pageWidget) {
        if (m_pageWidget->transformBox() && m_pageWidget->transformBox()->isVisible()) {
            return true;
        }
        if (m_pageWidget->hasAreaSelection()) {
            return true;
        }
    }
    return !m_undoStack.isEmpty();
}

bool PdfViewer::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void PdfViewer::undo()
{
    // 1. If an uncommitted transform box or area selection is active, cancel it first
    if (m_pageWidget) {
        if (m_pageWidget->transformBox() && m_pageWidget->transformBox()->isVisible()) {
            m_pageWidget->transformBox()->dismiss();
            m_pageWidget->setToolMode(PdfToolMode::SelectText);
            m_pageWidget->update();
            emit documentModified(m_isModified);
            emit historyChanged();
            return;
        }
        if (m_pageWidget->hasAreaSelection()) {
            m_pageWidget->clearAreaSelection();
            emit documentModified(m_isModified);
            emit historyChanged();
            return;
        }
    }

    if (!canUndo() || !m_doc || m_undoStack.isEmpty()) return;

    // 2. Snapshot current state for redo
    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_redo_XXXXXX.pdf");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) return;
    QString redoSnapshotPath = tempFile.fileName();
    tempFile.close();
    QFile::remove(redoSnapshotPath);

    std::unique_ptr<Poppler::PDFConverter> conv = m_doc->pdfConverter();
    if (conv) {
        conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
        conv->setOutputFileName(redoSnapshotPath);
        if (conv->convert()) {
            m_redoStack.append(redoSnapshotPath);
        } else {
            if (QFile::exists(redoSnapshotPath)) QFile::remove(redoSnapshotPath);
        }
    } else {
        if (QFile::exists(redoSnapshotPath)) QFile::remove(redoSnapshotPath);
    }

    // 3. Pop latest snapshot from undo stack
    QString undoSnapshotPath = m_undoStack.takeLast();
    if (!QFile::exists(undoSnapshotPath)) return;

    int savedPage = m_currentPage;
    double savedZoom = m_zoomFactor;
    int savedRot = m_rotationDegrees;

    // Release current active snapshot if not in undo/redo stack
    if (!m_activeSnapshotPath.isEmpty() && QFile::exists(m_activeSnapshotPath) &&
        !m_undoStack.contains(m_activeSnapshotPath) && !m_redoStack.contains(m_activeSnapshotPath)) {
        QString toRemove = m_activeSnapshotPath;
        resetDocument();
        QFile::remove(toRemove);
    } else {
        resetDocument();
    }

    m_activeSnapshotPath = undoSnapshotPath;
    m_doc = Poppler::Document::load(m_activeSnapshotPath);
    if (!m_doc || m_doc->isLocked()) {
        resetDocument();
        return;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = std::min(savedPage, m_pageCount - 1);
    m_zoomFactor = savedZoom;
    m_rotationDegrees = savedRot;

    populateOutline();
    renderCurrentPage();
    updateNavButtons();
    emit pageChanged(m_currentPage + 1, m_pageCount);

    setModified(!m_undoStack.isEmpty());
    emit historyChanged();
}

void PdfViewer::redo()
{
    if (!canRedo() || !m_doc || m_redoStack.isEmpty()) return;

    // 1. Snapshot current state for undo
    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_undo_XXXXXX.pdf");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) return;
    QString undoSnapshotPath = tempFile.fileName();
    tempFile.close();
    QFile::remove(undoSnapshotPath);

    std::unique_ptr<Poppler::PDFConverter> conv = m_doc->pdfConverter();
    if (conv) {
        conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
        conv->setOutputFileName(undoSnapshotPath);
        if (conv->convert()) {
            m_undoStack.append(undoSnapshotPath);
        } else {
            if (QFile::exists(undoSnapshotPath)) QFile::remove(undoSnapshotPath);
        }
    } else {
        if (QFile::exists(undoSnapshotPath)) QFile::remove(undoSnapshotPath);
    }

    // 2. Pop state from redo stack
    QString redoSnapshotPath = m_redoStack.takeLast();
    if (!QFile::exists(redoSnapshotPath)) return;

    int savedPage = m_currentPage;
    double savedZoom = m_zoomFactor;
    int savedRot = m_rotationDegrees;

    if (!m_activeSnapshotPath.isEmpty() && QFile::exists(m_activeSnapshotPath) &&
        !m_undoStack.contains(m_activeSnapshotPath) && !m_redoStack.contains(m_activeSnapshotPath)) {
        QString toRemove = m_activeSnapshotPath;
        resetDocument();
        QFile::remove(toRemove);
    } else {
        resetDocument();
    }

    m_activeSnapshotPath = redoSnapshotPath;
    m_doc = Poppler::Document::load(m_activeSnapshotPath);
    if (!m_doc || m_doc->isLocked()) {
        resetDocument();
        return;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_pageCount = m_doc->numPages();
    m_currentPage = std::min(savedPage, m_pageCount - 1);
    m_zoomFactor = savedZoom;
    m_rotationDegrees = savedRot;

    populateOutline();
    renderCurrentPage();
    updateNavButtons();
    emit pageChanged(m_currentPage + 1, m_pageCount);

    setModified(true);
    emit historyChanged();
}


