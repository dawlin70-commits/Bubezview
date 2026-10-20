/**
 * @file test_gpu.cpp
 * @brief Automated unit test for GpuAccelerationManager.
 */

#include <QTest>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QSettings>
#include "core/system/gpu_acceleration_manager.h"

class TestGpu : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testSurfaceFormat();
    void testSingleton();
    void testSettingsToggle();
    void testCreateViewport();
    void testDiagnostics();
};

void TestGpu::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    GpuAccelerationManager::setupSurfaceFormat();
    GpuAccelerationManager::instance().initialize();
}

void TestGpu::testSurfaceFormat()
{
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    QCOMPARE(fmt.swapBehavior(), QSurfaceFormat::DoubleBuffer);
    QCOMPARE(fmt.swapInterval(), 1);
    QCOMPARE(fmt.alphaBufferSize(), 0);
}

void TestGpu::testSingleton()
{
    GpuAccelerationManager &mgr1 = GpuAccelerationManager::instance();
    GpuAccelerationManager &mgr2 = GpuAccelerationManager::instance();
    QCOMPARE(&mgr1, &mgr2);
}

void TestGpu::testSettingsToggle()
{
    GpuAccelerationManager &mgr = GpuAccelerationManager::instance();
    bool initial = mgr.isGpuAccelerationEnabled();

    QSignalSpy spy(&mgr, &GpuAccelerationManager::gpuAccelerationChanged);

    mgr.setGpuAccelerationEnabled(!initial);
    QCOMPARE(mgr.isGpuAccelerationEnabled(), !initial);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), !initial);

    // Restore
    mgr.setGpuAccelerationEnabled(initial);
    QCOMPARE(mgr.isGpuAccelerationEnabled(), initial);
    QCOMPARE(spy.count(), 1);
}

void TestGpu::testCreateViewport()
{
    GpuAccelerationManager &mgr = GpuAccelerationManager::instance();
    QWidget *vp = mgr.createAcceleratedViewport(nullptr);
    QVERIFY(vp != nullptr);
    delete vp;
}

void TestGpu::testDiagnostics()
{
    GpuAccelerationManager &mgr = GpuAccelerationManager::instance();
    QVERIFY(!mgr.gpuVendor().isEmpty());
    QVERIFY(!mgr.gpuRenderer().isEmpty());
    QVERIFY(!mgr.glVersion().isEmpty());
    QVERIFY(!mgr.glslVersion().isEmpty());
    QCOMPARE(mgr.estimatedVramUsage(), QStringLiteral("~40–80 MB"));
}

QTEST_MAIN(TestGpu)
#include "test_gpu.moc"
