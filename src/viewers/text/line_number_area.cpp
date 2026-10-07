/**
 * @file line_number_area.cpp
 * @brief Implementation of LineNumberArea gutter widget.
 */

#include "line_number_area.h"
#include "text_viewer.h"

LineNumberArea::LineNumberArea(TextViewer *viewer)
    : QWidget(viewer)
    , m_viewer(viewer)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_viewer->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_viewer->lineNumberAreaPaintEvent(event);
}
