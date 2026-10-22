/**
 * @file test_media_system.cpp
 * @brief Automated unit test for SystemMediaViewer native playback engine.
 */

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include "viewers/media/system_media_viewer.h"

class TestMediaSystem : public QObject
{
    Q_OBJECT

private slots:
    void testInstantiation();
    void testAudioOnlyDetection();
    void testSiblingFiles();
    void testControlsAndMixerReset();
    void testInvalidFileHandling();
};

void TestMediaSystem::testInstantiation()
{
    SystemMediaViewer viewer;
    QVERIFY(viewer.widget() != nullptr);
    QCOMPARE(viewer.isBitPerfect(), false);
    QVERIFY(!viewer.currentAudioDevice().isEmpty());
}

void TestMediaSystem::testAudioOnlyDetection()
{
    SystemMediaViewer viewer;

    QTemporaryFile fileAudio("test_song_XXXXXX.mp3");
    QVERIFY(fileAudio.open());
    fileAudio.write("ID3MockAudioData");
    fileAudio.close();

    viewer.loadFile(fileAudio.fileName());
    QVERIFY(viewer.isAudioOnly());

    QTemporaryFile fileVideo("test_clip_XXXXXX.mp4");
    QVERIFY(fileVideo.open());
    fileVideo.write("ftypisomMockVideoData");
    fileVideo.close();

    viewer.loadFile(fileVideo.fileName());
    QVERIFY(!viewer.isAudioOnly());
}

void TestMediaSystem::testSiblingFiles()
{
    SystemMediaViewer viewer;
    QStringList siblings = {
        QStringLiteral("/path/to/track1.mp3"),
        QStringLiteral("/path/to/track2.mp3"),
        QStringLiteral("/path/to/track3.mp3")
    };
    viewer.setSiblingFiles(siblings);

    bool callbackInvoked = false;
    QString receivedTrack;
    viewer.setOnTrackChangeRequested([&](const QString &track) {
        callbackInvoked = true;
        receivedTrack = track;
    });

    QMetaObject::invokeMethod(&viewer, "playNextTrack");
    QVERIFY(callbackInvoked);
    QCOMPARE(receivedTrack, QStringLiteral("/path/to/track1.mp3"));
}

void TestMediaSystem::testControlsAndMixerReset()
{
    SystemMediaViewer viewer;
    viewer.resetMixer();
    viewer.togglePlayPause();
    viewer.stop();
    viewer.clear();
}

void TestMediaSystem::testInvalidFileHandling()
{
    SystemMediaViewer viewer;
    bool loaded = viewer.loadFile(QStringLiteral("/non_existent_folder_xyz/file_123.mp4"));
    QVERIFY(!loaded);
}

QTEST_MAIN(TestMediaSystem)
#include "test_media_system.moc"
