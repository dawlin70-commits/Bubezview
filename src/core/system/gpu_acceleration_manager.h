/**
 * @file gpu_acceleration_manager.h
 * @brief Hardware GPU acceleration manager and OpenGL diagnostics.
 *
 * Detects GPU hardware capabilities, manages global OpenGL surface formats (VSync, MSAA),
 * provides accelerated viewports for image and document rendering, and tracks VRAM efficiency.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QWidget>

class QOpenGLWidget;

/**
 * @brief Singleton managing GPU acceleration state, OpenGL formats, and diagnostics.
 */
class GpuAccelerationManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Access the singleton instance.
     */
    static GpuAccelerationManager &instance();

    /**
     * @brief Configures default QSurfaceFormat (VSync, double-buffering, multi-sampling).
     * Must be called before Q(Gui)Application is instantiated.
     */
    static void setupSurfaceFormat();

    /**
     * @brief Initializes GPU detection and probes OpenGL driver capabilities.
     * Must be called after Q(Gui)Application is instantiated.
     */
    void initialize();

    /**
     * @brief Whether hardware OpenGL acceleration is available and supported by driver.
     */
    bool isGpuAvailable() const { return m_gpuAvailable; }

    /**
     * @brief Whether GPU acceleration is enabled by user in settings.
     */
    bool isGpuAccelerationEnabled() const;

    /**
     * @brief Sets GPU acceleration enabled state and saves to settings.
     * @param enabled True to enable hardware GPU acceleration; false for software fallback.
     */
    void setGpuAccelerationEnabled(bool enabled);

    /**
     * @brief Detected GPU vendor string (e.g. "NVIDIA Corporation", "AMD", "Intel").
     */
    QString gpuVendor() const { return m_vendor; }

    /**
     * @brief Detected GPU renderer string (e.g. "NVIDIA GeForce RTX 4070 Laptop GPU").
     */
    QString gpuRenderer() const { return m_renderer; }

    /**
     * @brief Detected OpenGL driver version string.
     */
    QString glVersion() const { return m_version; }

    /**
     * @brief Detected OpenGL shading language (GLSL) version.
     */
    QString glslVersion() const { return m_glslVersion; }

    /**
     * @brief Estimated VRAM footprint of UI hardware acceleration.
     * @return Formatted string (e.g. "~40–80 MB").
     */
    QString estimatedVramUsage() const { return QStringLiteral("~40–80 MB"); }

    /**
     * @brief Creates a hardware accelerated viewport widget if GPU acceleration is active,
     * or a standard QWidget software fallback otherwise.
     * @param parent Parent widget (typically QScrollArea).
     * @return New QOpenGLWidget or QWidget.
     */
    QWidget *createAcceleratedViewport(QWidget *parent = nullptr) const;

signals:
    /**
     * @brief Emitted when GPU acceleration setting is toggled.
     * @param enabled New acceleration state.
     */
    void gpuAccelerationChanged(bool enabled);

private:
    explicit GpuAccelerationManager(QObject *parent = nullptr);
    ~GpuAccelerationManager() override = default;
    Q_DISABLE_COPY(GpuAccelerationManager)

    void probeGpu();

    bool m_initialized = false;
    bool m_gpuAvailable = false;
    QString m_vendor = QStringLiteral("Unknown");
    QString m_renderer = QStringLiteral("Unknown");
    QString m_version = QStringLiteral("Unknown");
    QString m_glslVersion = QStringLiteral("Unknown");
};
