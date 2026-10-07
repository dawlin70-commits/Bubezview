#pragma once

#include <QWidget>
#include <mpv/client.h>

class MpvGLWidget;

/**
 * @brief Video rendering container widget for libmpv.
 *
 * Utilizes hardware-accelerated OpenGL (QOpenGLWidget + mpv_render_context)
 * when running in graphical environments (Wayland / X11), and degrades
 * gracefully to a lightweight fallback in headless / offscreen environments.
 */
class MpvVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MpvVideoWidget(QWidget *parent = nullptr);
    ~MpvVideoWidget() override;

    /**
     * @brief Associates the mpv instance with the video rendering widget.
     * @param mpv Pointer to the initialized mpv_handle.
     */
    void setMpvHandle(mpv_handle *mpv);

    /**
     * @brief Safely releases the OpenGL mpv render context before mpv_destroy.
     */
    void cleanup();

private:
    MpvGLWidget *m_glWidget;
};
