#pragma once

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QVector>
#include <QObject>
#include <functional>
#include "core/equalizer_manager.h"

/**
 * @struct AudioMetadata
 * @brief Container for standard audio tag metadata and track properties.
 */
struct AudioMetadata {
    QString title;          ///< Song or track title
    QString artist;         ///< Artist or performer name
    QString album;          ///< Album name
    QString albumArtist;    ///< Album artist if distinct
    QString genre;          ///< Genre (e.g. Rock, Pop, Classical)
    QString year;           ///< Release year (e.g. 2024)
    QString date;           ///< Publication / release date (e.g. 2024-05-18)
    QString trackNumber;    ///< Track sequence number
    QString comment;        ///< Track comment / notes
    qint64 durationMs = 0;  ///< Total duration in milliseconds
    int sampleRate = 0;     ///< Audio sample rate in Hz (e.g. 44100, 48000, 96000)
    int channels = 0;       ///< Number of audio channels (1 = mono, 2 = stereo, etc.)
    int bitRate = 0;        ///< Audio bitrate in kbps (e.g. 320)
    int bitsPerSample = 0;  ///< Bit depth (e.g. 16, 24, 32)
    QString codecName;      ///< Audio codec identifier (e.g. flac, mp3, aac, opus)
    bool hasEmbeddedCover = false; ///< True if cover art exists inside the file
};

/**
 * @enum ChannelMode
 * @brief Spatial and channel routing modes.
 */
enum class ChannelMode {
    Original = 0,   ///< Unmodified stereo / multi-channel audio
    StereoToMono,   ///< Downmix Stereo -> Mono
    SwapChannels,   ///< Swap Left and Right channels
    LeftOnly,       ///< Left channel isolated on both stereo ears
    RightOnly,      ///< Right channel isolated on both stereo ears
    VocalRemover    ///< Center cancellation / Karaoke mode (removes vocals)
};

/**
 * @struct CueTrack
 * @brief Represents an individual song or movement parsed from a CUE sheet.
 */
struct CueTrack {
    int trackNumber = 0;       ///< Track sequence index
    QString title;             ///< Song title
    QString performer;         ///< Artist / performer
    qint64 startMs = 0;        ///< Track start offset in milliseconds
    qint64 durationMs = 0;     ///< Track duration in milliseconds (0 = until next or EOF)
};

/**
 * @struct AudioTrimOptions
 * @brief Options for trimming audio or saving a sub-clip.
 */
struct AudioTrimOptions {
    qint64 startMs = 0;             ///< Clip start position in milliseconds
    qint64 endMs = 0;               ///< Clip end position in milliseconds
    double fadeInSec = 0.0;         ///< Fade-in duration in seconds (0 = none)
    double fadeOutSec = 0.0;        ///< Fade-out duration in seconds (0 = none)
    bool normalizeLoudness = false; ///< True to apply EBU R128 loudness normalization
    bool voiceFilter = false;       ///< True to apply vocal enhancer / highpass+lowpass filter (legacy)
    int lowCutHz = 0;               ///< Low-cut / high-pass filter frequency in Hz (0 = disabled, e.g. 110 for traffic)
    int denoiseDb = 0;              ///< Spectral FFT denoise attenuation in dB (0 = disabled, e.g. 14 for fan hiss)
    int dehumHz = 0;                ///< De-hum notch filter (0 = disabled, 50 = EU 50Hz, 60 = US 60Hz)
    bool declick = false;           ///< True to apply de-clicker / de-pop for vinyl scratches or microphone pops (adeclick)
    bool deesser = false;           ///< True to apply de-esser for sharp harsh vocal sibilants (deesser)
    bool voiceEnhancer = false;     ///< Speech normalization + 3 kHz vocal presence boost
    bool noiseGate = false;         ///< Silence background room noise during pauses between words
    ChannelMode channelMode = ChannelMode::Original; ///< Spatial/channel remix mode
    int pitchSemitones = 0;         ///< Pitch shift in semitones (-12 to +12, 0 = unaltered)
    double timeStretch = 1.0;       ///< Time stretch speed factor (0.5 to 2.0, 1.0 = normal)
    bool reverseAudio = false;      ///< Reverse audio playback (areverse)
    bool removeSilence = false;     ///< Intelligent silence removal (silenceremove)
    int targetSampleRate = 0;       ///< Target sample rate in Hz (0 = keep original, 44100, 48000, 96000, 192000)
    int targetBitrateKbps = 0;      ///< Target bitrate in kbps (0 = auto/default, 128, 192, 256, 320)
    int highCutHz = 0;              ///< High-cut / low-pass filter frequency in Hz (0 = disabled, e.g. 16000)
    int voiceFreqHz = 3000;         ///< Voice presence center frequency in Hz
    double voiceGainDb = 3.5;       ///< Voice presence boost gain in dB
    int deessFreqHz = 6500;         ///< De-esser center frequency in Hz
    double deessDepthDb = -6.0;     ///< De-esser attenuation depth in dB
    bool equalizerEnabled = false;  ///< True if 10-band equalizer is applied
    EqualizerPreset equalizerPreset; ///< Equalizer preset / curve data
    bool losslessCopy = true;       ///< True to use fast stream-copy without re-encoding (if no filters)
    QString outputFormat;           ///< Output format: "copy", "mp3", "flac", "wav", "aac", "ogg"
};

/**
 * @class AudioProcessor
 * @brief Core engine and utility provider for BubezView audio suite.
 *
 * Implements high-performance, non-blocking asynchronous operations using
 * the system's ffmpeg / ffprobe tools for lossless trimming, audio extraction,
 * normalization, waveform rendering, tag reading/writing, and cover art retrieval.
 */
class AudioProcessor : public QObject {
    Q_OBJECT

public:
    explicit AudioProcessor(QObject *parent = nullptr);
    ~AudioProcessor() override;

    /**
     * @brief Checks if ffmpeg is available in the current execution environment.
     * @return True if ffmpeg executable is found in PATH; false otherwise.
     */
    static bool isFfmpegAvailable();

    /**
     * @brief Reads metadata tags and technical properties from an audio or video file.
     * @param filePath Absolute path to the media file.
     * @return Populated AudioMetadata structure.
     */
    static AudioMetadata readMetadata(const QString &filePath);

    /**
     * @brief Extracts cover art pixmap from embedded audio tags or adjacent directory image files.
     * @param filePath Absolute path to audio file.
     * @param maxDimension Maximum width/height for the returned pixmap.
     * @return Scaled QPixmap if cover was found; null QPixmap otherwise.
     */
    static QPixmap extractCoverArt(const QString &filePath, int maxDimension = 300);

    /**
     * @brief Generates normalized amplitude peaks (0.0 to 1.0) for fast waveform display.
     * @param filePath Absolute path to audio file.
     * @param numSamples Number of waveform bar peaks to produce (e.g. 200).
     * @return Vector of float amplitudes between 0.0f and 1.0f.
     */
    static QVector<float> generateWaveformPeaks(const QString &filePath, int numSamples = 200);

    /**
     * @brief Generates stereo amplitude peaks (0.0 to 1.0) for dual-channel (L and R) waveform display.
     * @param filePath Absolute path to audio file.
     * @param numSamples Number of waveform samples per channel to produce (e.g. 150).
     * @return Pair of vectors: first = Left channel peaks, second = Right channel peaks.
     */
    static QPair<QVector<float>, QVector<float>> generateStereoWaveformPeaks(const QString &filePath, int numSamples = 150);

    /**
     * @brief Extracts a continuous mono float PCM stream (-1.0f to 1.0f) from audio file for waveform and real-time FFT visualization.
     * @param filePath Path to media file.
     * @param targetSampleRate Target sample rate (e.g. 22050 Hz).
     * @param maxDurationSec Maximum duration to extract in seconds (default 300 = 5 min).
     * @return Vector of normalized float samples.
     */
    static QVector<float> extractPcmData(const QString &filePath, int targetSampleRate = 22050, int maxDurationSec = 300);

    /**
     * @brief Computes frequency band magnitudes (e.g. 36 logarithmic bands) for real-time FFT spectrum display.
     * @param pcmSamples Vector of normalized mono float samples (-1.0 to 1.0).
     * @param sampleRate Sampling rate of PCM samples (e.g. 44100).
     * @param numBands Number of output frequency bands (e.g. 36).
     * @return Vector of normalized band energies (0.0f to 1.0f).
     */
    static QVector<float> computeFftMagnitudes(const QVector<float> &pcmSamples, int sampleRate = 44100, int numBands = 36);

    /**
     * @brief Trims an audio file with the specified options and outputs to destination path.
     * @param inputPath Source file path.
     * @param outputPath Destination file path.
     * @param options Trim options (start/end points, fades, normalization, format).
     * @param progressCallback Optional progress reporter callback (0 to 100).
     * @return True if processing finished successfully; false otherwise.
     */
    static bool trimAudio(const QString &inputPath,
                          const QString &outputPath,
                          const AudioTrimOptions &options,
                          std::function<void(int)> progressCallback = nullptr);

    /**
     * @brief Extracts audio track from a video file and saves it in target format.
     * @param videoPath Source video path.
     * @param outputPath Destination audio file path.
     * @param format Audio format: "mp3", "flac", "wav", "aac", "ogg".
     * @param progressCallback Optional progress callback.
     * @return True on success; false on failure.
     */
    static bool extractAudioFromVideo(const QString &videoPath,
                                      const QString &outputPath,
                                      const QString &format = "mp3",
                                      std::function<void(int)> progressCallback = nullptr);

    /**
     * @brief Writes updated metadata tags and optional album cover image into a file.
     * @param filePath Target media file.
     * @param metadata Updated tags to write.
     * @param coverImagePath Optional path to new cover image (.jpg or .png) to embed, or empty.
     * @param removeCover True if cover art should be stripped.
     * @return True on success; false on error.
     */
    static bool writeMetadata(const QString &filePath,
                              const AudioMetadata &metadata,
                              const QString &coverImagePath = QString(),
                              bool removeCover = false);

    /**
     * @brief Renames an audio file on disk to a new file name within the same folder.
     * @param oldPath Current full path to the file.
     * @param newFileName Target new filename (e.g. "01 - Master of Puppets.mp3").
     * @param outNewPath Populated with the new absolute path upon success.
     * @param outError Populated with error message if renaming fails.
     * @return True if renamed successfully; false otherwise.
     */
    static bool renameAudioFile(const QString &oldPath,
                                const QString &newFileName,
                                QString &outNewPath,
                                QString &outError);

    /**
     * @brief Detects non-silent audio boundaries (leading and trailing silence) using ffmpeg silencedetect.
     * @param filePath Absolute path to media file.
     * @param outStartMs Populated with start position in milliseconds where sound begins.
     * @param outEndMs Populated with end position in milliseconds where sound ends.
     * @param silenceThresholdDb Silence threshold in dB (default -45 dB).
     * @return True if sound boundaries were successfully detected; false otherwise.
     */
    static bool detectAudioBoundaries(const QString &filePath,
                                      qint64 &outStartMs,
                                      qint64 &outEndMs,
                                      int silenceThresholdDb = -45);

    /**
     * @brief Parses a standard CD/Hi-Res CUE sheet (.cue) into individual track markers.
     * @param cuePath Absolute path to .cue file.
     * @param totalAudioDurationMs Optional total duration of associated audio file in milliseconds.
     * @return List of parsed CueTrack structures.
     */
    static QList<CueTrack> parseCueSheet(const QString &cuePath, qint64 totalAudioDurationMs = 0);

    /**
     * @brief Splits an audio file into individual tracks according to a CUE sheet.
     * @param audioPath Absolute path to the source audio file.
     * @param cuePath Absolute path to the CUE sheet file.
     * @param outputDir Destination directory for split tracks.
     * @param progressCallback Optional progress reporter callback: (currentTrack, totalTracks, trackTitle).
     * @return True if all tracks were split successfully; false otherwise.
     */
    static bool splitCueSheet(const QString &audioPath,
                              const QString &cuePath,
                              const QString &outputDir,
                              std::function<void(int, int, const QString&)> progressCallback = nullptr);
};
