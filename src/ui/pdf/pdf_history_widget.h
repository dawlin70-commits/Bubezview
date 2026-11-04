/**
 * @file pdf_history_widget.h
 * @brief Interactive visual history panel and snapshot carousel for PDF modifications.
 */

#pragma once

#include <QWidget>
#include <QDialog>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QList>
#include "core/interfaces/i_pdf_viewer.h"

/**
 * @class PdfHistoryCard
 * @brief Discrete clickable slide card representing a past snapshot with thumbnail preview.
 */
class PdfHistoryCard : public QFrame
{
    Q_OBJECT

public:
    explicit PdfHistoryCard(const PdfHistoryStep &step, QWidget *parent = nullptr);
    int stepIndex() const { return m_stepIndex; }
    bool isCurrent() const { return m_isCurrent; }

signals:
    void clicked(int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateStyle(bool hovered);

    int m_stepIndex;
    bool m_isCurrent;
    QLabel *m_lblThumbnail;
    QLabel *m_lblTitle;
    QLabel *m_lblTime;
    QLabel *m_lblBadge;
};

/**
 * @class PdfHistoryWidget
 * @brief Spacious scrollable carousel of visual snapshots allowing 1-click restore to past edits.
 */
class PdfHistoryWidget : public QDialog
{
    Q_OBJECT

public:
    explicit PdfHistoryWidget(QWidget *parent = nullptr);

    /**
     * @brief Populates the carousel with recorded history steps.
     * @param steps Ordered list of history state entries.
     */
    void setHistorySteps(const QList<PdfHistoryStep> &steps);

    /**
     * @brief Re-styles the history viewer for light/dark themes.
     */
    void updateTheme();

signals:
    /**
     * @brief Emitted when the user selects a past snapshot card.
     * @param index Target snapshot step index.
     */
    void stepSelected(int index);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QScrollArea *m_scrollArea;
    QWidget *m_cardContainer;
    QHBoxLayout *m_cardLayout;
    QLabel *m_lblEmpty;
    QLabel *m_lblHeader;
    QLabel *m_lblSubtitle;
    QToolButton *m_btnClose;
};
