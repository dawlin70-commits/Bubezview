/**
 * @file test_media_system.cpp
 * @brief Automated unit test for SystemMediaViewer native playback engine.
 */

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QLabel>
#include <QImage>
#include "viewers/media/system_media_viewer.h"

class TestMediaSystem : public QObject
{
    Q_OBJECT

private slots:
    void testInstantiation();
    void testAudioOnlyDetection();
    void testSiblingFiles();
    void testAudioSiblingFilteringAndRewind();
    void testSeekDragAndRelease();
    void testControlsAndMixerReset();
    void testInvalidFileHandling();
    void testFolderCoverLoading();
    void testProModeUpgradeCallback();
    void testVisualSnapshot();
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

void TestMediaSystem::testAudioSiblingFilteringAndRewind()
{
    SystemMediaViewer viewer;
    QStringList mixedSiblings = {
        QStringLiteral("/path/to/00_info.txt"),
        QStringLiteral("/path/to/01_song.flac"),
        QStringLiteral("/path/to/cover.jpg"),
        QStringLiteral("/path/to/02_song.mp3"),
        QStringLiteral("/path/to/playlist.m3u")
    };
    viewer.setSiblingFiles(mixedSiblings);

    QStringList visited;
    viewer.setOnTrackChangeRequested([&](const QString &track) {
        visited.append(track);
    });

    // Advancing tracks in audio mode must skip non-audio files (.txt, .jpg, .m3u)
    QMetaObject::invokeMethod(&viewer, "playNextTrack");
    QMetaObject::invokeMethod(&viewer, "playNextTrack");
    QMetaObject::invokeMethod(&viewer, "playNextTrack");

    QCOMPARE(visited.size(), 3);
    QCOMPARE(visited[0], QStringLiteral("/path/to/01_song.flac"));
    QCOMPARE(visited[1], QStringLiteral("/path/to/02_song.mp3"));
    QCOMPARE(visited[2], QStringLiteral("/path/to/01_song.flac"));

    // Seek past 3 seconds and verify playPreviousTrack rewinds to 0 instead of switching tracks
    visited.clear();
    viewer.requestSeek(5000, false);
    QMetaObject::invokeMethod(&viewer, "playPreviousTrack");
    QVERIFY(visited.isEmpty());

    // Now at 0ms, playPreviousTrack should switch to the previous audio track
    QMetaObject::invokeMethod(&viewer, "playPreviousTrack");
    QCOMPARE(visited.size(), 1);
    QCOMPARE(visited[0], QStringLiteral("/path/to/02_song.mp3"));
}

void TestMediaSystem::testSeekDragAndRelease()
{
    SystemMediaViewer viewer;
    // Simulate rapid drag updates (exact = false) followed by mouse release (exact = true)
    for (int i = 1; i <= 10; ++i) {
        viewer.requestSeek(i * 1000, false);
    }
    viewer.requestSeek(10000, true);
    viewer.toggleTracklistDrawer();
    viewer.toggleTracklistDrawer();
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

void TestMediaSystem::testFolderCoverLoading()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Create a mock audio file
    QString audioPath = tempDir.filePath("track.mp3");
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("MockAudioData");
    audioFile.close();

    // Create a cover.jpg in the same directory
    QString coverPath = tempDir.filePath("cover.jpg");
    QImage mockCover(100, 100, QImage::Format_RGB32);
    mockCover.fill(Qt::cyan);
    QVERIFY(mockCover.save(coverPath, "JPG"));

    SystemMediaViewer viewer;
    QVERIFY(viewer.loadFile(audioPath));

    // Find the cover label
    QLabel *coverLabel = viewer.widget()->findChild<QLabel*>();
    QVERIFY(coverLabel != nullptr);
    viewer.clear();
}

void TestMediaSystem::testProModeUpgradeCallback()
{
    SystemMediaViewer viewer;
    bool callbackInvoked = false;
    viewer.setOnUpgradeToProRequested([&]() {
        callbackInvoked = true;
    });

    QPushButton *btnPro = viewer.proModeButton();
    QVERIFY(btnPro != nullptr);
    QVERIFY(!btnPro->text().isEmpty());

    // Clicking the Pro Mode button must trigger the upgrade callback
    btnPro->click();
    QVERIFY(callbackInvoked);
}

void TestMediaSystem::testVisualSnapshot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString audioPath = tempDir.filePath("sample_track.mp3");
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("MockAudioData");
    audioFile.close();

    // Create an elegant cover image
    QString coverPath = tempDir.filePath("cover.jpg");
    QImage mockCover(400, 400, QImage::Format_RGB32);
    mockCover.fill(QColor("#0369a1"));
    {
        QPainter p(&mockCover);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient grad(0, 0, 400, 400);
        grad.setColorAt(0.0, QColor("#38bdf8"));
        grad.setColorAt(0.5, QColor("#0284c7"));
        grad.setColorAt(1.0, QColor("#0f172a"));
        p.fillRect(mockCover.rect(), grad);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(48);
        f.setBold(true);
        p.setFont(f);
        p.drawText(mockCover.rect(), Qt::AlignCenter, QStringLiteral("ALBUM\nART"));
    }
    QVERIFY(mockCover.save(coverPath, "JPG"));

    SystemMediaViewer viewer;
    viewer.resize(820, 520);
    viewer.loadFile(audioPath);
    viewer.show();
    QCoreApplication::processEvents();

    QPixmap pixmap = viewer.grab();
    pixmap.save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/media_viewer_restored.png", "PNG");

    // Compact layout snapshot
    viewer.resize(520, 420);
    QCoreApplication::processEvents();
    QPixmap compactPix = viewer.grab();
    compactPix.save("/home/davide/.gemini/antigravity/brain/8466bc25-4ae4-4586-8bb5-2ec02a7ab3e7/media_viewer_restored_compact.png", "PNG");
}

QTEST_MAIN(TestMediaSystem)
#include "test_media_system.moc"
