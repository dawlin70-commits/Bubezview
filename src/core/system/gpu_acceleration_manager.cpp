/**
 * @file gpu_acceleration_manager.cpp
 * @brief Implementation of GpuAccelerationManager.
 */

#include "gpu_acceleration_manager.h"

#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QSettings>
#include <QDebug>

GpuAccelerationManager &GpuAccelerationManager::instance()
{
    static GpuAccelerationManager s_instance;
    return s_instance;
}

GpuAccelerationManager::GpuAccelerationManager(QObject *parent)
    : QObject(parent)
{
}

void GpuAccelerationManager::setupSurfaceFormat()
{
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1); // Enable VSync for 60/120/144Hz tear-free rendering
    fmt.setAlphaBufferSize(0); // Strictly 0 alpha: window must NEVER be translucent to the desktop
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    QSurfaceFormat::setDefaultFormat(fmt);
}

void GpuAccelerationManager::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    // Headless / offscreen test environments cannot initialize an OpenGL context
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        m_gpuAvailable = false;
        m_vendor = QStringLiteral("Qt Offscreen");
        m_renderer = QStringLiteral("Software Rasterizer");
        m_version = QStringLiteral("N/A");
        m_glslVersion = QStringLiteral("N/A");
        return;
    }

    probeGpu();
}

void GpuAccelerationManager::probeGpu()
{
    QOpenGLContext ctx;
    if (!ctx.create()) {
        m_gpuAvailable = false;
        m_vendor = QStringLiteral("Software");
        m_renderer = QStringLiteral("Software Fallback");
        m_version = QStringLiteral("N/A");
        m_glslVersion = QStringLiteral("N/A");
        return;
    }

    QOffscreenSurface surf;
    surf.create();
    if (ctx.makeCurrent(&surf)) {
        QOpenGLFunctions funcs(&ctx);
        funcs.initializeOpenGLFunctions();

        const char *v = reinterpret_cast<const char*>(funcs.glGetString(GL_VENDOR));
        const char *r = reinterpret_cast<const char*>(funcs.glGetString(GL_RENDERER));
        const char *ver = reinterpret_cast<const char*>(funcs.glGetString(GL_VERSION));
        const char *glsl = reinterpret_cast<const char*>(funcs.glGetString(GL_SHADING_LANGUAGE_VERSION));

        if (v) m_vendor = QString::fromUtf8(v);
        if (r) m_renderer = QString::fromUtf8(r);
        if (ver) m_version = QString::fromUtf8(ver);
        if (glsl) m_glslVersion = QString::fromUtf8(glsl);

        m_gpuAvailable = true;
        ctx.doneCurrent();
    } else {
        m_gpuAvailable = false;
    }
    surf.destroy();
}

bool GpuAccelerationManager::isGpuAccelerationEnabled() const
{
    QSettings settings("Bubez", "BubezView");
    return settings.value("performance/gpu_acceleration", true).toBool();
}

void GpuAccelerationManager::setGpuAccelerationEnabled(bool enabled)
{
    QSettings settings("Bubez", "BubezView");
    settings.setValue("performance/gpu_acceleration", enabled);
    emit gpuAccelerationChanged(enabled);
}

QWidget *GpuAccelerationManager::createAcceleratedViewport(QWidget *parent) const
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        return new QWidget(parent);
    }

    if (m_gpuAvailable && isGpuAccelerationEnabled()) {
        return new QOpenGLWidget(parent);
    }

    return new QWidget(parent);
}
