/**
 * @file settings_dialog.cpp
 * @brief Implementation of the settings dialog coordinator.
 */

#include "settings_dialog.h"
#include "settings/settings_appearance_tab.h"
#include "settings/settings_integration_tab.h"
#include "settings/settings_performance_tab.h"
#include "settings/settings_extensions_tab.h"
#include "settings/settings_extra_tab.h"
#include "build_config.h"
#include "build_console_dialog.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QCursor>
#include <QWindow>

SettingsDialog::SettingsDialog(QWidget *parent, TabIndex initialTab)
    : QDialog(parent)
{
    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.load();
    m_initialPdfStatic = bcfg.staticPdf();
    m_initialArchiveStatic = bcfg.staticArchive();
    m_initialOfficeStatic = bcfg.staticOffice();
    m_initialEnableOffice = bcfg.enableInternalOffice();

    setupUi();
    setCurrentTab(initialTab);
    refreshStatuses();
    updatePendingBanner();

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

SettingsDialog::~SettingsDialog()
{
    if (m_isCursorOverridden) {
        QGuiApplication::restoreOverrideCursor();
        m_isCursorOverridden = false;
    }
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void SettingsDialog::setCurrentTab(TabIndex tab)
{
    if (m_tabWidget) {
        m_tabWidget->setCurrentIndex(static_cast<int>(tab));
    }
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings — BubezView"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);

    resize(840, 580);
    setMinimumSize(780, 540);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(0);

    m_container = new QFrame(this);
    m_container->setObjectName("SettingsContainer");
    m_container->setMouseTracking(true);

    auto *shadow = new QGraphicsDropShadowEffect(m_container);
    shadow->setBlurRadius(22.0);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 120));
    m_container->setGraphicsEffect(shadow);

    rootLayout->addWidget(m_container);

    QVBoxLayout *containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(16, 12, 16, 14);
    containerLayout->setSpacing(10);

    // Draggable custom header bar
    m_headerBar = new QWidget(m_container);
    m_headerBar->setObjectName("SettingsHeaderBar");
    m_headerBar->setMouseTracking(true);
    QHBoxLayout *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(2, 2, 2, 4);
    headerLayout->setSpacing(8);

    m_lblIcon = new QLabel(m_headerBar);
    m_lblIcon->setPixmap(QIcon(":/icons/bubezview.png").pixmap(20, 20));
    headerLayout->addWidget(m_lblIcon);

    m_lblTitle = new QLabel(tr("Settings — BubezView"), m_headerBar);
    m_lblTitle->setStyleSheet("font-size: 13px; font-weight: 700; background: transparent;");
    headerLayout->addWidget(m_lblTitle);

    headerLayout->addStretch();

    m_btnTitleClose = new QPushButton("✕", m_headerBar);
    m_btnTitleClose->setFixedSize(26, 26);
    m_btnTitleClose->setCursor(Qt::PointingHandCursor);
    m_btnTitleClose->setToolTip(tr("Close"));
    connect(m_btnTitleClose, &QPushButton::clicked, this, &QDialog::accept);
    headerLayout->addWidget(m_btnTitleClose);

    containerLayout->addWidget(m_headerBar);

    m_tabWidget = new QTabWidget(m_container);

    m_appearanceTab = new SettingsAppearanceTab(this);
    connect(m_appearanceTab, &SettingsAppearanceTab::themeChanged, this, [this]() {
        updateAllThemeStyles();
        emit themeChanged();
    });
    m_tabWidget->addTab(m_appearanceTab, tr("Appearance"));

    m_integrationTab = new SettingsIntegrationTab(this);
    m_tabWidget->addTab(m_integrationTab, tr("Desktop Integration"));

    m_performanceTab = new SettingsPerformanceTab(this);
    connect(m_performanceTab, &SettingsPerformanceTab::pendingStateChanged, this, &SettingsDialog::updatePendingBanner);
    connect(m_performanceTab, &SettingsPerformanceTab::recompileRequested, this, [this](const QString &t) { startRecompilation(t); });
    connect(m_performanceTab, &SettingsPerformanceTab::openExtensionsTabRequested, this, [this]() { setCurrentTab(TabIndex::Extensions); });
    m_tabWidget->addTab(m_performanceTab, tr("Performance"));

    m_extensionsTab = new SettingsExtensionsTab(this);
    connect(m_extensionsTab, &SettingsExtensionsTab::pendingStateChanged, this, &SettingsDialog::updatePendingBanner);
    m_tabWidget->addTab(m_extensionsTab, tr("Extensions && Modules"));

    m_extraTab = new SettingsExtraTab(this);
    connect(m_extraTab, &SettingsExtraTab::themeChanged, this, [this]() {
        updateAllThemeStyles();
        emit themeChanged();
    });
    connect(m_extraTab, &SettingsExtraTab::setupWizardCompleted, this, &SettingsDialog::refreshStatuses);
    connect(m_extraTab, &SettingsExtraTab::recompileRequested, this, [this](const QString &t) { startRecompilation(t); });
    m_tabWidget->addTab(m_extraTab, tr("System Tools"));

    containerLayout->addWidget(m_tabWidget, 1);

    // Pending recompile notification banner
    m_bannerPending = new QFrame(m_container);
    m_bannerPending->setStyleSheet(
        "QFrame {"
        "  background-color: rgba(245, 158, 11, 0.14);"
        "  border: 1px solid rgba(245, 158, 11, 0.42);"
        "  border-radius: 12px;"
        "}"
    );
    QHBoxLayout *layBanner = new QHBoxLayout(m_bannerPending);
    layBanner->setContentsMargins(14, 10, 14, 10);
    layBanner->setSpacing(10);

    m_lblBannerMsg = new QLabel(tr("⚠️ <b>Pending Changes:</b> Build options were modified. Recompilation is required to apply."), m_bannerPending);
    m_lblBannerMsg->setStyleSheet("color: #f59e0b; font-size: 11px; border: none; background: transparent;");
    m_lblBannerMsg->setWordWrap(true);
    layBanner->addWidget(m_lblBannerMsg, 1);

    m_btnBannerRevert = new QPushButton(tr("Revert"), m_bannerPending);
    m_btnBannerRevert->setCursor(Qt::PointingHandCursor);
    m_btnBannerRevert->setStyleSheet("QPushButton { padding: 5px 12px; border-radius: 8px; font-size: 11px; }");
    connect(m_btnBannerRevert, &QPushButton::clicked, this, &SettingsDialog::onRevertPendingClicked);
    layBanner->addWidget(m_btnBannerRevert);

    m_btnBannerRecompile = new QPushButton(tr("⚙️ Recompile && Apply Now"), m_bannerPending);
    m_btnBannerRecompile->setCursor(Qt::PointingHandCursor);
    m_btnBannerRecompile->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #2563eb); color: white; font-weight: bold; padding: 6px 14px; border-radius: 8px; font-size: 11px; border: 1px solid rgba(125, 211, 252, 0.55); } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7dd3fc, stop:1 #3b82f6); }"
    );
    connect(m_btnBannerRecompile, &QPushButton::clicked, this, [this]() { startRecompilation(); });
    layBanner->addWidget(m_btnBannerRecompile);

    m_bannerPending->hide();
    containerLayout->addWidget(m_bannerPending);

    // Bottom action bar
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_btnClose = new QPushButton(tr("Close"), m_container);
    m_btnClose->setMinimumWidth(110);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_btnClose);
    containerLayout->addLayout(bottomLayout);

    updateAllThemeStyles();
}

void SettingsDialog::updateAllThemeStyles()
{
    ThemePreset active = ThemeManager::effectivePreset(ThemeManager::currentPreset());
    ThemeManager::applyTheme(qobject_cast<QApplication*>(QApplication::instance()), nullptr);

    if (m_appearanceTab) m_appearanceTab->updateThemeStyles();
    if (m_integrationTab) m_integrationTab->updateThemeStyles();
    if (m_performanceTab) m_performanceTab->updateThemeStyles();
    if (m_extensionsTab) m_extensionsTab->updateThemeStyles();
    if (m_extraTab) m_extraTab->updateThemeStyles();

    if (m_container) {
        if (active == ThemePreset::ZincGray) {
            m_container->setStyleSheet(
                "QFrame#SettingsContainer {"
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
                "QFrame#SettingsContainer {"
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
                "QFrame#SettingsContainer {"
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

    if (m_btnClose) {
        if (active == ThemePreset::ZincGray) {
            m_btnClose->setStyleSheet(
                "QPushButton {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #585960, stop:1 #47484e);"
                "  color: #ffffff;"
                "  border: 1px solid rgba(255, 255, 255, 0.22);"
                "  border-radius: 10px;"
                "  padding: 7px 22px;"
                "  font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "  background-color: #63646c;"
                "}"
            );
        } else if (ThemeManager::isDarkTheme()) {
            m_btnClose->setStyleSheet(
                "QPushButton {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1);"
                "  color: #ffffff;"
                "  border: 1px solid rgba(56, 189, 248, 0.45);"
                "  border-radius: 10px;"
                "  padding: 7px 22px;"
                "  font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #38bdf8, stop:1 #0284c7);"
                "}"
            );
        } else {
            m_btnClose->setStyleSheet(
                "QPushButton {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284c7, stop:1 #0369a1);"
                "  color: #ffffff;"
                "  border: 1px solid rgba(2, 132, 199, 0.35);"
                "  border-radius: 10px;"
                "  padding: 7px 22px;"
                "  font-weight: 600;"
                "}"
                "QPushButton:hover {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0369a1, stop:1 #075985);"
                "}"
            );
        }
    }
}

void SettingsDialog::refreshStatuses()
{
    if (m_integrationTab) {
        m_integrationTab->refreshStatuses();
    }
}

bool SettingsDialog::hasPendingBuildChanges() const
{
    bool perfPending = (m_performanceTab && m_performanceTab->hasPendingChanges());

    BuildConfig &bcfg = BuildConfig::instance();
    bool pdfChanged = (bcfg.staticPdf() != m_initialPdfStatic);
    bool archChanged = (bcfg.staticArchive() != m_initialArchiveStatic);
    bool offStatChanged = (bcfg.staticOffice() != m_initialOfficeStatic);
    bool offChanged = (bcfg.enableInternalOffice() != m_initialEnableOffice);

    return perfPending || pdfChanged || archChanged || offStatChanged || offChanged;
}

void SettingsDialog::updatePendingBanner()
{
    if (!m_bannerPending) return;
    m_bannerPending->setVisible(hasPendingBuildChanges());
}

void SettingsDialog::onRevertPendingClicked()
{
    if (m_performanceTab) {
        m_performanceTab->revertPendingChanges();
    }

    BuildConfig &bcfg = BuildConfig::instance();
    bcfg.setStaticPdf(m_initialPdfStatic);
    bcfg.setStaticArchive(m_initialArchiveStatic);
    bcfg.setStaticOffice(m_initialOfficeStatic);
    bcfg.setEnableInternalOffice(m_initialEnableOffice);
    bcfg.save();

    if (m_extensionsTab) {
        m_extensionsTab->refreshAllRows();
    }

    updatePendingBanner();
}

void SettingsDialog::startRecompilation()
{
    startRecompilation(QString());
}

void SettingsDialog::startRecompilation(const QString &target)
{
    // 1. Check optional dependency prompt for librevenge if enabled
    BuildConfig &cfg = BuildConfig::instance();
    if (cfg.enableInternalOffice() && !BuildConfig::isPkgInstalled("librevenge-0.0")) {
        QMessageBox::StandardButton res = QMessageBox::question(this, tr("Missing Dependency"),
            tr("The Internal Office Viewer requires 'librevenge', which is not currently detected on your system.<br><br>Would you like the installer to attempt installing it via your system package manager?"),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::No) {
            return;
        }
    }

    // 2. Persist updated compiler optimization level to disk (if not targeting a specific debug rebuild)
    if (m_performanceTab && target.isEmpty()) {
        cfg.setOptimizationLevel(m_performanceTab->optimizationLevel());
    }
    cfg.save();

    // 3. Launch the dedicated floating build console
    if (!m_buildConsoleDialog) {
        m_buildConsoleDialog = new BuildConsoleDialog(this);
        connect(m_buildConsoleDialog, &BuildConsoleDialog::buildSucceeded, this, [this]() {
            if (m_performanceTab) {
                m_performanceTab->onBuildSucceeded();
            }
            if (m_extensionsTab) {
                m_extensionsTab->refreshAllRows();
            }
            BuildConfig &bcfg = BuildConfig::instance();
            m_initialPdfStatic = bcfg.staticPdf();
            m_initialArchiveStatic = bcfg.staticArchive();
            m_initialOfficeStatic = bcfg.staticOffice();
            m_initialEnableOffice = bcfg.enableInternalOffice();
            updatePendingBanner();
        });
    }

    m_buildConsoleDialog->show();
    m_buildConsoleDialog->raise();
    m_buildConsoleDialog->activateWindow();
    if (!target.isEmpty()) {
        m_buildConsoleDialog->startBuildTarget(target);
    } else {
        m_buildConsoleDialog->startBuild();
    }
}

Qt::Edges SettingsDialog::calculateResizeEdges(const QPoint &globalPos) const
{
    QPoint localPos = mapFromGlobal(globalPos);
    QRect r = rect();

    if (localPos.x() < 0 || localPos.y() < 0 || localPos.x() > r.width() || localPos.y() > r.height()) {
        return Qt::Edges();
    }

    constexpr int BORDER = 14;
    Qt::Edges edges;
    if (localPos.x() <= BORDER) {
        edges |= Qt::LeftEdge;
    } else if (localPos.x() >= r.width() - BORDER) {
        edges |= Qt::RightEdge;
    }

    if (localPos.y() <= BORDER) {
        edges |= Qt::TopEdge;
    } else if (localPos.y() >= r.height() - BORDER) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void SettingsDialog::updateResizeCursor(Qt::Edges edges)
{
    if (!edges) {
        if (m_isCursorOverridden) {
            unsetCursor();
            while (QGuiApplication::overrideCursor()) {
                QGuiApplication::restoreOverrideCursor();
            }
            m_isCursorOverridden = false;
        }
        return;
    }

    QCursor resizeCursor;
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
        (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        resizeCursor = Qt::SizeFDiagCursor;
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
               (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        resizeCursor = Qt::SizeBDiagCursor;
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        resizeCursor = Qt::SizeHorCursor;
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        resizeCursor = Qt::SizeVerCursor;
    }

    setCursor(resizeCursor);
    m_isCursorOverridden = true;
}

void SettingsDialog::performManualResize(const QPoint &globalPos)
{
    QPoint delta = globalPos - m_resizeStartPos;
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

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *widget = qobject_cast<QWidget*>(watched);
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
        } else if (me->buttons() == Qt::NoButton) {
            Qt::Edges edges = calculateResizeEdges(globalPos);
            updateResizeCursor(edges);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Let close button handle its own click
            if (m_btnTitleClose && (watched == m_btnTitleClose || m_btnTitleClose->geometry().contains(m_headerBar->mapFromGlobal(me->globalPosition().toPoint())))) {
                break;
            }

            const QPoint globalPos = me->globalPosition().toPoint();
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
                updateResizeCursor(calculateResizeEdges(me->globalPosition().toPoint()));
                return true;
            }
            if (m_isDragging) {
                m_isDragging = false;
                return true;
            }
        }
        break;
    }
    case QEvent::Leave:
    case QEvent::WindowDeactivate:
    case QEvent::FocusOut: {
        if (m_isCursorOverridden && !m_isResizing) {
            updateResizeCursor(Qt::Edges());
        }
        break;
    }
    default:
        break;
    }

    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::mousePressEvent(QMouseEvent *event)
{
    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event)
{
    QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *event)
{
    QDialog::mouseReleaseEvent(event);
}
