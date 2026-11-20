#include "mpv_video_widget.h"
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QMetaObject>
#include <mpv/render_gl.h>

static void *get_proc_address(void *, const char *name)
{
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    return glctx ? reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name))) : nullptr;
}

/**
 * @brief Internal OpenGL surface widget that connects directly to libmpv OpenGL render API.
 */
class MpvGLWidget : public QOpenGLWidget
{
public:
    explicit MpvGLWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
        , m_mpv(nullptr)
        , m_mpvGL(nullptr)
    {
    }

    ~MpvGLWidget() override
    {
        cleanup();
    }

    void setMpvHandle(mpv_handle *mpv)
    {
        m_mpv = mpv;
    }

    bool isContextReady() const
    {
        return m_mpvGL != nullptr;
    }

    void cleanup()
    {
        if (m_mpvGL) {
            // Unregister update callback first so libmpv worker threads cannot post events during teardown
            mpv_render_context_set_update_callback(m_mpvGL, nullptr, nullptr);
            QCoreApplication::removePostedEvents(this);
            if (isValid() && context()) {
                makeCurrent();
                mpv_render_context_free(m_mpvGL);
                doneCurrent();
            } else {
                mpv_render_context_free(m_mpvGL);
            }
            m_mpvGL = nullptr;
        }
    }

protected:
    void initializeGL() override
    {
        if (!m_mpv || m_mpvGL || !isValid() || !context()) {
            return;
        }

        mpv_opengl_init_params gl_init{get_proc_address, nullptr};
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        if (mpv_render_context_create(&m_mpvGL, m_mpv, params) >= 0 && m_mpvGL) {
            mpv_render_context_set_update_callback(m_mpvGL, MpvGLWidget::onMpvUpdate, this);
            if (m_mpv) {
                mpv_set_property_string(m_mpv, "vid", "auto");
            }
            auto *parentVid = qobject_cast<MpvVideoWidget *>(parent());
            if (parentVid) {
                emit parentVid->contextReady();
            }
        }
    }

    void paintGL() override
    {
        if (!m_mpvGL || !isValid() || !context()) {
            return;
        }

        const qreal dpr = devicePixelRatio();
        const int w = static_cast<int>(width() * dpr);
        const int h = static_cast<int>(height() * dpr);

        mpv_opengl_fbo fbo{
            static_cast<int>(defaultFramebufferObject()),
            w,
            h,
            0
        };

        int flip_y = 1;
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        mpv_render_context_render(m_mpvGL, params);
    }

    void resizeGL(int w, int h) override
    {
        Q_UNUSED(w);
        Q_UNUSED(h);
    }

private:
    static void onMpvUpdate(void *ctx)
    {
        auto *widget = static_cast<QWidget *>(ctx);
        QMetaObject::invokeMethod(widget, "update", Qt::QueuedConnection);
    }

    mpv_handle *m_mpv;
    mpv_render_context *m_mpvGL;
};

MpvVideoWidget::MpvVideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_glWidget(nullptr)
    , m_mpv(nullptr)
{
    setStyleSheet("background-color: #000000;");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    ensureGlWidgetCreated();
}

MpvVideoWidget::~MpvVideoWidget()
{
    cleanup();
}

void MpvVideoWidget::setMpvHandle(mpv_handle *mpv)
{
    m_mpv = mpv;
    if (m_glWidget) {
        m_glWidget->setMpvHandle(mpv);
    }
}

void MpvVideoWidget::ensureGlWidgetCreated()
{
    if (m_glWidget) return;
    if (QGuiApplication::platformName() == "offscreen") return;

    m_glWidget = new MpvGLWidget(this);
    if (m_mpv) {
        m_glWidget->setMpvHandle(m_mpv);
    }
    layout()->addWidget(m_glWidget);
}

void MpvVideoWidget::cleanup()
{
    if (m_glWidget) {
        m_glWidget->cleanup();
    }
}

bool MpvVideoWidget::isContextReady() const
{
    return m_glWidget && m_glWidget->isContextReady();
}
