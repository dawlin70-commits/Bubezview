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
     * @brief Lazily ensures the OpenGL video surface widget is created.
     * Called only when playing an actual video file, avoiding OpenGL context
     * and native window surface recreation when opening audio-only files.
     */
    void ensureGlWidgetCreated();

    /**
     * @brief Safely releases the OpenGL mpv render context before mpv_destroy.
     */
    void cleanup();

    /**
     * @brief Checks if the underlying mpv OpenGL render context has been created and is operational.
     */
    bool isContextReady() const;

signals:
    /**
     * @brief Emitted when the OpenGL surface and libmpv render context are successfully created.
     */
    void contextReady();

private:
    MpvGLWidget *m_glWidget = nullptr;
    mpv_handle *m_mpv = nullptr;
};
