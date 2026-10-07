/**
 * @file line_number_area.h
 * @brief Side gutter widget for displaying line numbers in TextViewer.
 */

#pragma once

#include <QWidget>
#include <QSize>
#include <QPaintEvent>

class TextViewer;

/**
 * @brief Gutter widget painted beside TextViewer's text area to render line numbers.
 */
class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a line number area attached to the given text viewer.
     * @param viewer Parent text viewer widget.
     */
    explicit LineNumberArea(TextViewer *viewer);

    /**
     * @brief Provides recommended size matching the gutter width calculated by the viewer.
     * @return Gutter dimension size hint.
     */
    QSize sizeHint() const override;

protected:
    /**
     * @brief Paints line numbers by delegating to the parent text viewer.
     * @param event Paint event with update rectangle.
     */
    void paintEvent(QPaintEvent *event) override;

private:
    TextViewer *m_viewer;
};
