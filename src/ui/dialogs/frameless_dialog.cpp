/**
 * @file frameless_dialog.cpp
 * @brief Implementation of the uniform frameless dialog base class.
 */

#include "frameless_dialog.h"
#include "core/services/theme_manager.h"

#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QWindow>
#include <QGuiApplication>
#include <QApplication>
#include <QIcon>

FramelessDialog::FramelessDialog(QWidget *parent)
    : QDialog(parent)
{
    while (QGuiApplication::overrideCursor()) {
        QGuiApplication::restoreOverrideCursor();
    }
    setupBaseUi();
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

FramelessDialog::~FramelessDialog()
{
    unsetCursor();
    while (QGuiApplication::overrideCursor()) {
        QGuiApplication::restoreOverrideCursor();
    }
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void FramelessDialog::setupBaseUi()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(0);

    m_container = new QFrame(this);
    m_container->setObjectName("DialogContainer");
    m_container->setMouseTracking(true);

    auto *shadow = new QGraphicsDropShadowEffect(m_container);
    shadow->setBlurRadius(22.0);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 120));
    m_container->setGraphicsEffect(shadow);

    rootLayout->addWidget(m_container);

    auto *containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(16, 12, 16, 14);
    containerLayout->setSpacing(10);

    // Draggable custom header bar
    m_headerBar = new QWidget(m_container);
    m_headerBar->setObjectName("DialogHeaderBar");
    m_headerBar->setMouseTracking(true);
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(2, 2, 2, 4);
    headerLayout->setSpacing(8);

    m_lblIcon = new QLabel(m_headerBar);
    m_lblIcon->setPixmap(QIcon(":/icons/bubezview.png").pixmap(20, 20));
    headerLayout->addWidget(m_lblIcon);

    m_lblTitle = new QLabel(m_headerBar);
    m_lblTitle->setStyleSheet("font-size: 13px; font-weight: 700; background: transparent;");
    headerLayout->addWidget(m_lblTitle);

    headerLayout->addStretch();

    m_btnTitleClose = new QPushButton("✕", m_headerBar);
    m_btnTitleClose->setFixedSize(26, 26);
    m_btnTitleClose->setCursor(Qt::PointingHandCursor);
    m_btnTitleClose->setToolTip(tr("Close"));
    connect(m_btnTitleClose, &QPushButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(m_btnTitleClose);

    containerLayout->addWidget(m_headerBar);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(10);
    containerLayout->addLayout(m_contentLayout, 1);

    updateThemeStyles();
}

void FramelessDialog::setDialogTitle(const QString &title)
{
    QDialog::setWindowTitle(title);
    if (m_lblTitle) {
        m_lblTitle->setText(title);
    }
}

void FramelessDialog::setWindowTitle(const QString &title)
{
    setDialogTitle(title);
}

void FramelessDialog::setDialogIcon(const QPixmap &pixmap)
{
    if (m_lblIcon) {
        m_lblIcon->setPixmap(pixmap.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void FramelessDialog::setWindowIcon(const QIcon &icon)
{
    QDialog::setWindowIcon(icon);
    setDialogIcon(icon.pixmap(20, 20));
}

void FramelessDialog::setCloseButtonVisible(bool visible)
{
    if (m_btnTitleClose) {
        m_btnTitleClose->setVisible(visible);
    }
}

void FramelessDialog::updateThemeStyles()
{
    const ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());

    if (m_container) {
        if (active == ThemePreset::ZincGray) {
            m_container->setStyleSheet(
                "QFrame#DialogContainer {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #323337, stop:0.5 #28292c, stop:1 #1e1f22);"
                "  border: 1px solid rgba(255, 255, 255, 0.15);"
                "  border-radius: 16px;"
                "}"
            );
            if (m_lblTitle) m_lblTitle->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: 700; background: transparent;");
            if (m_btnTitleClose) {
                m_btnTitleClose->setStyleSheet(
                    "QPushButton {"
                    "  background: rgba(255, 255, 255, 0.08);"
                    "  color: #d1d5db;"
                    "  border: 1px solid rgba(255, 255, 255, 0.14);"
                    "  border-radius: 13px;"
                    "  font-size: 12px;"
                    "  font-weight: bold;"
                    "}"
                    "QPushButton:hover {"
                    "  background: #ef4444;"
                    "  color: #ffffff;"
                    "  border: 1px solid #dc2626;"
                    "}"
                );
            }
        } else if (ThemeManager::isDarkTheme()) {
            m_container->setStyleSheet(
                "QFrame#DialogContainer {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #25292e, stop:0.5 #1e2126, stop:1 #17191c);"
                "  border: 1px solid rgba(255, 255, 255, 0.14);"
                "  border-radius: 16px;"
                "}"
            );
            if (m_lblTitle) m_lblTitle->setStyleSheet("color: #eff0f1; font-size: 13px; font-weight: 700; background: transparent;");
            if (m_btnTitleClose) {
                m_btnTitleClose->setStyleSheet(
                    "QPushButton {"
                    "  background: rgba(255, 255, 255, 0.06);"
                    "  color: #9ca3af;"
                    "  border: 1px solid rgba(255, 255, 255, 0.10);"
                    "  border-radius: 13px;"
                    "  font-size: 12px;"
                    "  font-weight: bold;"
                    "}"
                    "QPushButton:hover {"
                    "  background: #ef4444;"
                    "  color: #ffffff;"
                    "  border: 1px solid #dc2626;"
                    "}"
                );
            }
        } else {
            m_container->setStyleSheet(
                "QFrame#DialogContainer {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ebeef2, stop:0.5 #e2e5ea, stop:1 #d8dce2);"
                "  border: 1px solid rgba(0, 0, 0, 0.12);"
                "  border-radius: 16px;"
                "}"
            );
            if (m_lblTitle) m_lblTitle->setStyleSheet("color: #1e293b; font-size: 13px; font-weight: 700; background: transparent;");
            if (m_btnTitleClose) {
                m_btnTitleClose->setStyleSheet(
                    "QPushButton {"
                    "  background: rgba(0, 0, 0, 0.05);"
                    "  color: #64748b;"
                    "  border: 1px solid rgba(0, 0, 0, 0.08);"
                    "  border-radius: 13px;"
                    "  font-size: 12px;"
                    "  font-weight: bold;"
                    "}"
                    "QPushButton:hover {"
                    "  background: #ef4444;"
                    "  color: #ffffff;"
                    "  border: 1px solid #dc2626;"
                    "}"
                );
            }
        }
    }
}

void FramelessDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateThemeStyles();
}

void FramelessDialog::hideEvent(QHideEvent *event)
{
    unsetCursor();
    while (QGuiApplication::overrideCursor()) {
        QGuiApplication::restoreOverrideCursor();
    }
    QDialog::hideEvent(event);
}

void FramelessDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        updateThemeStyles();
    } else if (event->type() == QEvent::WindowTitleChange) {
        if (m_lblTitle) {
            m_lblTitle->setText(windowTitle());
        }
    } else if (event->type() == QEvent::WindowIconChange) {
        if (m_lblIcon && !windowIcon().isNull()) {
            m_lblIcon->setPixmap(windowIcon().pixmap(20, 20));
        }
    }
}

void FramelessDialog::setResizable(bool resizable)
{
    m_resizable = resizable;
    if (!m_resizable) {
        unsetCursor();
    }
}

Qt::Edges FramelessDialog::calculateResizeEdges(const QPoint &globalPos) const
{
    if (!m_resizable || isMaximized() || isFullScreen()) {
        return Qt::Edges();
    }
    // Only allow resizing if dialog is set to be resizable
    if (maximumSize() == minimumSize()) {
        return Qt::Edges();
    }

    const QPoint localPos = mapFromGlobal(globalPos);
    const int margin = 8;
    Qt::Edges edges;

    if (localPos.x() >= 0 && localPos.x() <= margin) {
        edges |= Qt::LeftEdge;
    }
    if (localPos.x() >= width() - margin && localPos.x() <= width()) {
        edges |= Qt::RightEdge;
    }
    if (localPos.y() >= 0 && localPos.y() <= margin) {
        edges |= Qt::TopEdge;
    }
    if (localPos.y() >= height() - margin && localPos.y() <= height()) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void FramelessDialog::updateResizeCursor(Qt::Edges edges)
{
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void FramelessDialog::performManualResize(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_resizeStartPos;
    QRect newGeom = m_resizeStartGeom;

    const int minW = minimumWidth();
    const int minH = minimumHeight();

    if (m_resizeEdges & Qt::LeftEdge) {
        int targetWidth = m_resizeStartGeom.width() - delta.x();
        if (targetWidth >= minW) {
            newGeom.setLeft(m_resizeStartGeom.left() + delta.x());
        } else {
            newGeom.setLeft(m_resizeStartGeom.right() - minW);
        }
    }
    if (m_resizeEdges & Qt::RightEdge) {
        int targetWidth = m_resizeStartGeom.width() + delta.x();
        if (targetWidth >= minW) {
            newGeom.setRight(m_resizeStartGeom.left() + targetWidth);
        }
    }
    if (m_resizeEdges & Qt::TopEdge) {
        int targetHeight = m_resizeStartGeom.height() - delta.y();
        if (targetHeight >= minH) {
            newGeom.setTop(m_resizeStartGeom.top() + delta.y());
        } else {
            newGeom.setTop(m_resizeStartGeom.bottom() - minH);
        }
    }
    if (m_resizeEdges & Qt::BottomEdge) {
        int targetHeight = m_resizeStartGeom.height() + delta.y();
        if (targetHeight >= minH) {
            newGeom.setBottom(m_resizeStartGeom.top() + targetHeight);
        }
    }

    setGeometry(newGeom);
}

bool FramelessDialog::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget*>(watched);
    if (!widget || widget->window() != this) {
        return QDialog::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent*>(event);
        const QPoint globalPos = me->globalPosition().toPoint();

        if (m_isResizing && (me->buttons() & Qt::LeftButton)) {
            performManualResize(globalPos);
            return true;
        } else if (m_isDragging && (me->buttons() & Qt::LeftButton)) {
            QPoint delta = globalPos - m_dragStartPosition;
            move(m_windowStartPosition + delta);
            return true;
        } else if (m_resizable && me->buttons() == Qt::NoButton) {
            Qt::Edges edges = calculateResizeEdges(globalPos);
            updateResizeCursor(edges);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (m_btnTitleClose && (watched == m_btnTitleClose || m_btnTitleClose->geometry().contains(m_headerBar->mapFromGlobal(me->globalPosition().toPoint())))) {
                break;
            }

            const QPoint globalPos = me->globalPosition().toPoint();
            if (m_resizable) {
                Qt::Edges edges = calculateResizeEdges(globalPos);
                if (edges) {
                    if (windowHandle() && windowHandle()->startSystemResize(edges)) {
                        me->accept();
                        return true;
                    }
                    m_isResizing = true;
                    m_resizeEdges = edges;
                    m_resizeStartPos = globalPos;
                    m_resizeStartGeom = geometry();
                    me->accept();
                    return true;
                }
            }

            // Check if pressing on header bar region
            QPoint localPos = mapFromGlobal(globalPos);
            if (localPos.y() <= 55) {
                if (windowHandle() && windowHandle()->startSystemMove()) {
                    me->accept();
                    return true;
                }
                m_isDragging = true;
                m_dragStartPosition = globalPos;
                m_windowStartPosition = pos();
                me->accept();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (m_isResizing) {
                m_isResizing = false;
                if (m_resizable) {
                    updateResizeCursor(calculateResizeEdges(me->globalPosition().toPoint()));
                } else {
                    unsetCursor();
                }
            }
            if (m_isDragging) {
                m_isDragging = false;
            }
        }
        break;
    }
    case QEvent::Leave:
    case QEvent::WindowDeactivate:
    case QEvent::FocusOut: {
        if (!m_isResizing) {
            unsetCursor();
            while (QGuiApplication::overrideCursor()) {
                QGuiApplication::restoreOverrideCursor();
            }
        }
        break;
    }
    default:
        break;
    }

    return QDialog::eventFilter(watched, event);
}
