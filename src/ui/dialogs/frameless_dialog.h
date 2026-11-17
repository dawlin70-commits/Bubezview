/**
 * @file frameless_dialog.h
 * @brief Reusable base class for all modal and modeless dialogs in BubezView,
 *        featuring a frameless liquid glass container, drop shadow, draggable
 *        header bar, title, icon, and uniform close button matching SettingsDialog.
 */

#pragma once

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPoint>
#include <QRect>

class FramelessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FramelessDialog(QWidget *parent = nullptr);
    ~FramelessDialog() override;

    /** @brief Sets the dialog title displayed in the custom header bar. */
    void setDialogTitle(const QString &title);
    void setWindowTitle(const QString &title);

    /** @brief Sets the dialog icon displayed in the custom header bar. */
    void setDialogIcon(const QPixmap &pixmap);
    void setWindowIcon(const QIcon &icon);

    /** @brief Sets whether the top-right close button is visible. */
    void setCloseButtonVisible(bool visible);

    /** @brief Sets whether the dialog can be manually resized by dragging its borders. */
    void setResizable(bool resizable);
    bool isResizable() const { return m_resizable; }

    /** @brief Returns the inner container frame with rounded corners and drop shadow. */
    QFrame *container() const { return m_container; }

    /** @brief Returns the draggable header bar widget. */
    QWidget *headerBar() const { return m_headerBar; }

    /** @brief Returns the title label in the header bar. */
    QLabel *titleLabel() const { return m_lblTitle; }

    /** @brief Returns the top-right close button. */
    QPushButton *closeButton() const { return m_btnTitleClose; }

    /** @brief Returns the content layout where subclasses place their widgets. */
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

    /** @brief Applies theme styles to the container, title, and close button. */
    virtual void updateThemeStyles();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupBaseUi();
    Qt::Edges calculateResizeEdges(const QPoint &globalPos) const;
    void updateResizeCursor(Qt::Edges edges);
    void performManualResize(const QPoint &globalPos);

    QFrame *m_container = nullptr;
    QWidget *m_headerBar = nullptr;
    QLabel *m_lblIcon = nullptr;
    QLabel *m_lblTitle = nullptr;
    QPushButton *m_btnTitleClose = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;

    bool m_isDragging = false;
    QPoint m_dragStartPosition;
    QPoint m_windowStartPosition;

    bool m_isResizing = false;
    bool m_resizable = true;
    Qt::Edges m_resizeEdges;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeom;
    bool m_isCursorOverridden = false;
};
