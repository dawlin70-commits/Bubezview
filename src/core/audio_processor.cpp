#include "audio_processor.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImage>
#include <QRegularExpression>
#include <cmath>
#include <cstring>
#include <complex>
#include <vector>
#include <algorithm>

AudioProcessor::AudioProcessor(QObject *parent)
    : QObject(parent)
{
}

AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::isFfmpegAvailable()
{
    static int s_available = -1;
    if (s_available != -1) {
        return s_available == 1;
    }

    QProcess proc;
    proc.start("ffmpeg", QStringList() << "-version");
    if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
        s_available = 1;
    } else {
        s_available = 0;
    }
    return s_available == 1;
}

AudioMetadata AudioProcessor::readMetadata(const QString &filePath)
{
    AudioMetadata meta;
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return meta;
    }

    // Execute ffprobe with JSON output format
    QProcess proc;
    QStringList args;
    args << "-v" << "quiet"
         << "-print_format" << "json"
         << "-show_format"
         << "-show_streams"
         << filePath;

    proc.start("ffprobe", args);
    if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
        return meta;
    }

    QByteArray output = proc.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (!doc.isObject()) return meta;

    QJsonObject root = doc.object();
    QJsonObject formatObj = root["format"].toObject();
    QJsonObject tagsObj = formatObj["tags"].toObject();

    // Duration & Bitrate from format container
    if (formatObj.contains("duration")) {
        meta.durationMs = static_cast<qint64>(formatObj["duration"].toString().toDouble() * 1000.0);
    }
    if (formatObj.contains("bit_rate")) {
        meta.bitRate = static_cast<int>(formatObj["bit_rate"].toString().toLongLong() / 1000);
    }

    // Stream properties (Sample rate, channels, bit depth)
    QJsonArray streams = root["streams"].toArray();
    for (const QJsonValue &v : streams) {
        QJsonObject st = v.toObject();
        QString codecType = st["codec_type"].toString();
        if (codecType == "audio" && meta.sampleRate == 0) {
            meta.codecName = st["codec_name"].toString().toUpper();
            meta.sampleRate = st["sample_rate"].toString().toInt();
            meta.channels = st["channels"].toInt();
            if (st.contains("bits_per_raw_sample") && !st["bits_per_raw_sample"].toString().isEmpty()) {
                meta.bitsPerSample = st["bits_per_raw_sample"].toString().toInt();
            } else if (st.contains("bits_per_sample")) {
                meta.bitsPerSample = st["bits_per_sample"].toInt();
            }
            if (meta.bitsPerSample == 0 && (meta.codecName == "FLAC" || meta.codecName == "WAV")) {
                QString sampleFmt = st["sample_fmt"].toString();
                if (sampleFmt.contains("32")) meta.bitsPerSample = 32;
                else if (sampleFmt.contains("24")) meta.bitsPerSample = 24;
                else meta.bitsPerSample = 16;
            }
        } else if (codecType == "video") {
            // Attached picture indicates embedded cover
            QJsonObject disp = st["disposition"].toObject();
            if (disp["attached_pic"].toInt() == 1) {
                meta.hasEmbeddedCover = true;
            }
        }

        // Check tags within the stream itself (e.g. OGG Vorbis streams)
        if (st.contains("tags")) {
            QJsonObject streamTags = st["tags"].toObject();
            for (auto it = streamTags.begin(); it != streamTags.end(); ++it) {
                QString key = it.key().toLower();
                QString val = it.value().toString().trimmed();
                if (key == "title" && meta.title.isEmpty()) meta.title = val;
                else if (key == "artist" && meta.artist.isEmpty()) meta.artist = val;
                else if (key == "album" && meta.album.isEmpty()) meta.album = val;
                else if ((key == "album_artist" || key == "albumartist") && meta.albumArtist.isEmpty()) meta.albumArtist = val;
                else if (key == "date") {
                    if (meta.date.isEmpty()) meta.date = val;
                    if (meta.year.isEmpty()) meta.year = val.left(4);
                } else if (key == "year") {
                    if (meta.year.isEmpty()) meta.year = val;
                    if (meta.date.isEmpty()) meta.date = val;
                }
                else if (key == "track" && meta.trackNumber.isEmpty()) meta.trackNumber = val;
                else if ((key == "comment" || key == "description") && meta.comment.isEmpty()) meta.comment = val;
            }
        }
    }

    // Read format tags case-insensitively
    for (auto it = tagsObj.begin(); it != tagsObj.end(); ++it) {
        QString key = it.key().toLower();
        QString val = it.value().toString().trimmed();
        if (key == "title" && meta.title.isEmpty()) meta.title = val;
        else if (key == "artist" && meta.artist.isEmpty()) meta.artist = val;
        else if (key == "album" && meta.album.isEmpty()) meta.album = val;
        else if ((key == "album_artist" || key == "albumartist") && meta.albumArtist.isEmpty()) meta.albumArtist = val;
        else if (key == "genre" && meta.genre.isEmpty()) meta.genre = val;
        else if (key == "date") {
            if (meta.date.isEmpty()) meta.date = val;
            if (meta.year.isEmpty()) meta.year = val.left(4);
        } else if (key == "year") {
            if (meta.year.isEmpty()) meta.year = val;
            if (meta.date.isEmpty()) meta.date = val;
        }
        else if (key == "track" && meta.trackNumber.isEmpty()) meta.trackNumber = val;
        else if ((key == "comment" || key == "description") && meta.comment.isEmpty()) meta.comment = val;
    }

    return meta;
}

QPixmap AudioProcessor::extractCoverArt(const QString &filePath, int maxDimension)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return QPixmap();
    }

    // 1. First, check adjacent folder cover files (fastest, no process execution)
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    QStringList candidates = {"cover.jpg", "cover.png", "folder.jpg", "folder.png", "front.jpg", "front.png", "album.jpg", "album.png"};
    for (const QString &cand : candidates) {
        QString candidatePath = dir.filePath(cand);
        if (QFileInfo::exists(candidatePath)) {
            QPixmap pix(candidatePath);
            if (!pix.isNull()) {
                return pix.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }

    // 2. Extract embedded cover from stream with ffmpeg
    QTemporaryFile tempFile(QDir::tempPath() + "/bubez_cover_XXXXXX.jpg");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        return QPixmap();
    }
    QString tempPath = tempFile.fileName();
    tempFile.close();

    QProcess proc;
    QStringList args;
    args << "-y"
         << "-v" << "quiet"
         << "-i" << filePath
         << "-an"
         << "-vcodec" << "copy"
         << tempPath;

    proc.start("ffmpeg", args);
    if (proc.waitForFinished(2000) && proc.exitCode() == 0 && QFileInfo::exists(tempPath)) {
        QPixmap pix(tempPath);
        QFile::remove(tempPath);
        if (!pix.isNull()) {
            return pix.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    QFile::remove(tempPath);
    return QPixmap();
}

QVector<float> AudioProcessor::generateWaveformPeaks(const QString &filePath, int numSamples)
{
    QVector<float> peaks;
    peaks.fill(0.05f, numSamples);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath) || numSamples <= 0) {
        return peaks;
    }

    // Fast 8kHz 8-bit mono raw audio dump via ffmpeg for instant peak computing
    QProcess proc;
    QStringList args;
    args << "-v" << "quiet"
         << "-i" << filePath
         << "-vn"
         << "-ac" << "1"
         << "-ar" << "8000"
         << "-f" << "u8"
         << "-";

    proc.start("ffmpeg", args);
    if (!proc.waitForStarted(2000)) {
        return peaks;
    }

    QByteArray rawData;
    while (proc.waitForReadyRead(300)) {
        rawData.append(proc.readAllStandardOutput());
    }
    rawData.append(proc.readAllStandardOutput());
    proc.waitForFinished(1000);

    if (rawData.isEmpty()) {
        return peaks;
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(rawData.constData());
    int totalBytes = rawData.size();
    int chunkSize = totalBytes / numSamples;
    if (chunkSize < 1) chunkSize = 1;

    float maxAmp = 0.001f;
    for (int i = 0; i < numSamples; ++i) {
        int startIdx = i * chunkSize;
        int endIdx = qMin(startIdx + chunkSize, totalBytes);
        float peak = 0.0f;
        for (int k = startIdx; k < endIdx; ++k) {
            float amp = std::abs(static_cast<float>(bytes[k]) - 128.0f) / 128.0f;
            if (amp > peak) peak = amp;
        }
        peaks[i] = peak;
        if (peak > maxAmp) maxAmp = peak;
    }

    // Normalize peaks between 0.05 and 1.0
    for (int i = 0; i < numSamples; ++i) {
        peaks[i] = qMax(0.05f, peaks[i] / maxAmp);
    }

    return peaks;
}

QPair<QVector<float>, QVector<float>> AudioProcessor::generateStereoWaveformPeaks(const QString &filePath, int numSamples)
{
    QVector<float> peaksL;
    QVector<float> peaksR;
    peaksL.fill(0.04f, numSamples);
    peaksR.fill(0.04f, numSamples);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath) || numSamples <= 0) {
        return qMakePair(peaksL, peaksR);
    }

    // Fast 8kHz 8-bit stereo raw audio dump via ffmpeg for instant peak computing
    QProcess proc;
    QStringList args;
    args << "-v" << "quiet"
         << "-i" << filePath
         << "-vn"
         << "-ac" << "2"
         << "-ar" << "8000"
         << "-f" << "u8"
         << "-";

    proc.start("ffmpeg", args);
    if (!proc.waitForStarted(2000)) {
        return qMakePair(peaksL, peaksR);
    }

    QByteArray rawData;
    while (proc.waitForReadyRead(300)) {
        rawData.append(proc.readAllStandardOutput());
    }
    rawData.append(proc.readAllStandardOutput());
    proc.waitForFinished(1000);

    if (rawData.isEmpty()) {
        return qMakePair(peaksL, peaksR);
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(rawData.constData());
    int totalFrames = rawData.size() / 2; // 2 bytes per stereo frame (L, R)
    int chunkSize = totalFrames / numSamples;
    if (chunkSize < 1) chunkSize = 1;

    float maxAmpL = 0.001f;
    float maxAmpR = 0.001f;

    for (int i = 0; i < numSamples; ++i) {
        int startFrame = i * chunkSize;
        int endFrame = qMin(startFrame + chunkSize, totalFrames);
        float peakL = 0.0f;
        float peakR = 0.0f;

        for (int f = startFrame; f < endFrame; ++f) {
            float ampL = std::abs(static_cast<float>(bytes[f * 2]) - 128.0f) / 128.0f;
            float ampR = std::abs(static_cast<float>(bytes[f * 2 + 1]) - 128.0f) / 128.0f;
            if (ampL > peakL) peakL = ampL;
            if (ampR > peakR) peakR = ampR;
        }

        peaksL[i] = peakL;
        peaksR[i] = peakR;
        if (peakL > maxAmpL) maxAmpL = peakL;
        if (peakR > maxAmpR) maxAmpR = peakR;
    }

    // Normalize peaks between 0.04 and 1.0
    for (int i = 0; i < numSamples; ++i) {
        peaksL[i] = qMax(0.04f, peaksL[i] / maxAmpL);
        peaksR[i] = qMax(0.04f, peaksR[i] / maxAmpR);
    }

    return qMakePair(peaksL, peaksR);
}

QVector<float> AudioProcessor::extractPcmData(const QString &filePath, int targetSampleRate, int maxDurationSec)
{
    QVector<float> pcm;
    if (filePath.isEmpty() || !QFileInfo::exists(filePath) || targetSampleRate <= 0) {
        return pcm;
    }

    QProcess proc;
    QStringList args;
    args << "-v" << "quiet";
    if (maxDurationSec > 0) {
        args << "-t" << QString::number(maxDurationSec);
    }
    args << "-i" << filePath
         << "-vn"
         << "-ac" << "1"
         << "-ar" << QString::number(targetSampleRate)
         << "-f" << "s16le"
         << "-";

    proc.start("ffmpeg", args);
    if (!proc.waitForFinished(6000) || proc.exitCode() != 0) {
        return pcm;
    }

    QByteArray raw = proc.readAllStandardOutput();
    int numSamples = raw.size() / 2;
    if (numSamples <= 0) return pcm;

    pcm.resize(numSamples);
    const qint16 *s16 = reinterpret_cast<const qint16 *>(raw.constData());
    for (int i = 0; i < numSamples; ++i) {
        pcm[i] = static_cast<float>(s16[i]) / 32768.0f;
    }

    return pcm;
}

QVector<float> AudioProcessor::computeFftMagnitudes(const QVector<float> &pcmSamples, int sampleRate, int numBands)
{
    QVector<float> bands(numBands, 0.0f);
    int N = pcmSamples.size();
    if (N < 64 || sampleRate <= 0 || numBands <= 0) {
        return bands;
    }

    // Ensure power of 2 size up to 1024 for high-speed computation
    int fftSize = 1;
    while (fftSize * 2 <= N && fftSize < 1024) {
        fftSize *= 2;
    }

    // Apply Hann window and copy real/imaginary parts
    std::vector<std::complex<double>> x(fftSize);
    for (int i = 0; i < fftSize; ++i) {
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
        x[i] = std::complex<double>(pcmSamples[i] * w, 0.0);
    }

    // In-place Cooley-Tukey Radix-2 FFT
    // 1. Bit-reversal permutation
    for (int i = 1, j = 0; i < fftSize; ++i) {
        int bit = fftSize >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(x[i], x[j]);
        }
    }

    // 2. Butterfly computations
    for (int len = 2; len <= fftSize; len <<= 1) {
        double angle = -2.0 * M_PI / len;
        std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < fftSize; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<double> u = x[i + j];
                std::complex<double> v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    // Compute magnitudes and bin into logarithmic frequency bands (20Hz to 20kHz)
    double minLog = std::log10(20.0);
    double maxLog = std::log10(20000.0);
    double freqPerBin = static_cast<double>(sampleRate) / fftSize;

    QVector<int> binCounts(numBands, 0);
    int halfSize = fftSize / 2;

    for (int k = 1; k < halfSize; ++k) {
        double freq = k * freqPerBin;
        if (freq < 20.0 || freq > 20000.0) continue;

        double logF = std::log10(freq);
        double frac = (logF - minLog) / (maxLog - minLog);
        int bandIdx = static_cast<int>(frac * numBands);
        bandIdx = std::clamp(bandIdx, 0, numBands - 1);

        double mag = std::abs(x[k]) / (fftSize / 2);
        bands[bandIdx] += static_cast<float>(mag);
        binCounts[bandIdx]++;
    }

    // Normalize and scale with psychoacoustic logarithmic boost
    for (int b = 0; b < numBands; ++b) {
        if (binCounts[b] > 0) {
            bands[b] /= binCounts[b];
        }
        // Gain boost for visual punch
        float val = bands[b] * 3.5f;
        bands[b] = std::clamp(val, 0.0f, 1.0f);
    }

    return bands;
}

bool AudioProcessor::trimAudio(const QString &inputPath,
                              const QString &outputPath,
                              const AudioTrimOptions &options,
                              std::function<void(int)> progressCallback)
{
    if (inputPath.isEmpty() || outputPath.isEmpty() || !QFileInfo::exists(inputPath)) {
        return false;
    }

    QProcess proc;
    QStringList args;
    args << "-y";

    // Start time offset
    double startSec = static_cast<double>(options.startMs) / 1000.0;
    args << "-ss" << QString::number(startSec, 'f', 3);

    // If duration specified
    if (options.endMs > options.startMs) {
        double durationSec = static_cast<double>(options.endMs - options.startMs) / 1000.0;
        args << "-t" << QString::number(durationSec, 'f', 3);
    }

    args << "-i" << inputPath;

    // Filters check (Fade in, Fade out, Loudnorm, Voice filter)
    QStringList filters;
    double durationSec = (options.endMs > options.startMs) ? static_cast<double>(options.endMs - options.startMs) / 1000.0 : 0.0;

    if (options.fadeInSec > 0.0) {
        filters << QString("afade=t=in:ss=0:d=%1").arg(options.fadeInSec, 0, 'f', 2);
    }
    if (options.fadeOutSec > 0.0 && durationSec > options.fadeOutSec) {
        double fadeStart = durationSec - options.fadeOutSec;
        filters << QString("afade=t=out:st=%1:d=%2").arg(fadeStart, 0, 'f', 2).arg(options.fadeOutSec, 0, 'f', 2);
    }
    // 1. Acoustic low-cut filter (traffic, wind, desk handling rumble)
    if (options.lowCutHz > 0) {
        filters << QString("highpass=f=%1").arg(options.lowCutHz);
    }
    // 1b. Acoustic high-cut filter (low-pass to remove ultrasonic noise / harsh highs)
    if (options.highCutHz > 0 && options.highCutHz < 20000) {
        filters << QString("lowpass=f=%1").arg(options.highCutHz);
    }
    // 2. Electrical hum notch filter (50Hz EU / 60Hz US mains hum)
    if (options.dehumHz > 0) {
        filters << QString("bandreject=f=%1:w=5").arg(options.dehumHz);
    }
    // 3. De-clicker / De-pop for vinyl scratches or microphone pops
    if (options.declick) {
        filters << "adeclick=w=55:o=75:a=2:m=a";
    }
    // 4. Adaptive FFT spectral denoiser (PC fan whine, microphone hiss, continuous static)
    if (options.denoiseDb > 0) {
        filters << QString("afftdn=nr=%1:nf=-25:tn=1").arg(options.denoiseDb);
    }
    // 5. De-esser (parametric attenuation for sharp harsh vocal sibilants)
    if (options.deesser) {
        int dFreq = options.deessFreqHz > 0 ? options.deessFreqHz : 6500;
        double dDepth = options.deessDepthDb < 0 ? options.deessDepthDb : -6.0;
        filters << QString("equalizer=f=%1:width_type=q:w=1.5:g=%2").arg(dFreq).arg(dDepth, 0, 'f', 1);
    }
    // 6. Noise gate (suppresses background ambient hiss during pauses between words)
    if (options.noiseGate) {
        filters << "agate=threshold=-35dB:ratio=3:range=-40dB:attack=10:release=150";
    }
    // 7. Speech clarity & vocal presence enhancement (speech normalization + parametric presence bump)
    if (options.voiceEnhancer) {
        int vFreq = options.voiceFreqHz > 0 ? options.voiceFreqHz : 3000;
        double vGain = options.voiceGainDb > 0 ? options.voiceGainDb : 3.5;
        filters << QString("speechnorm=e=4:r=0.0001:l=1,equalizer=f=%1:width_type=q:w=1:g=%2").arg(vFreq).arg(vGain, 0, 'f', 1);
    } else if (options.voiceFilter && options.lowCutHz == 0 && options.denoiseDb == 0) {
        // Fallback for legacy caller
        filters << "highpass=f=80,lowpass=f=8000";
    }
    // 8. EBU R128 loudness normalization
    if (options.normalizeLoudness) {
        filters << "loudnorm=I=-16:TP=-1.5:LRA=11";
    }

    // 9. Spatial and channel remix mode
    switch (options.channelMode) {
    case ChannelMode::StereoToMono:
        filters << "pan=mono|c0=0.5*c0+0.5*c1";
        break;
    case ChannelMode::SwapChannels:
        filters << "pan=stereo|c0=c1|c1=c0";
        break;
    case ChannelMode::LeftOnly:
        filters << "pan=stereo|c0=c0|c1=c0";
        break;
    case ChannelMode::RightOnly:
        filters << "pan=stereo|c0=c1|c1=c1";
        break;
    case ChannelMode::VocalRemover:
        filters << "pan=stereo|c0=c0-c1|c1=c1-c0";
        break;
    default:
        break;
    }

    // 10. Pitch Shift in semitones (without altering speed)
    if (options.pitchSemitones != 0) {
        double scale = std::pow(2.0, static_cast<double>(options.pitchSemitones) / 12.0);
        filters << QString("rubberband=pitch=%1").arg(scale, 0, 'f', 4);
    }

    // 11. Time Stretch / Speed factor (without altering pitch)
    if (std::abs(options.timeStretch - 1.0) > 0.01) {
        double speed = std::clamp(options.timeStretch, 0.5, 2.0);
        filters << QString("atempo=%1").arg(speed, 0, 'f', 2);
    }

    // 12. Audio Reverse (playback backwards)
    if (options.reverseAudio) {
        filters << "areverse";
    }

    // 13. Automatic silence removal
    if (options.removeSilence) {
        filters << "silenceremove=start_periods=1:start_duration=0.1:start_threshold=-50dB:stop_periods=1:stop_duration=0.5:stop_threshold=-50dB";
    }

    // 14. 10-Band Graphic Equalizer, Pre-Amp, True-Peak Limiter, & Stereo Widener
    if (options.equalizerEnabled) {
        QString eqFilter = EqualizerManager::buildFfmpegFilterString(options.equalizerPreset);
        if (!eqFilter.isEmpty()) {
            filters << eqFilter;
        }
    }

    bool hasAudioFilters = !filters.isEmpty();
    if (!hasAudioFilters && options.losslessCopy && options.outputFormat == "copy") {
        args << "-c" << "copy";
    } else {
        if (hasAudioFilters) {
            args << "-af" << filters.join(",");
        }

        // Custom Sample Rate override
        if (options.targetSampleRate > 0) {
            args << "-ar" << QString::number(options.targetSampleRate);
        }

        // Custom Bitrate determination
        int bitrate = options.targetBitrateKbps > 0 ? options.targetBitrateKbps : 320;

        QString outFmt = options.outputFormat.toLower();
        if (outFmt == "mp3") {
            args << "-c:a" << "libmp3lame" << "-b:a" << QString("%1k").arg(bitrate);
        } else if (outFmt == "flac") {
            args << "-c:a" << "flac";
        } else if (outFmt == "wav") {
            args << "-c:a" << "pcm_s16le";
        } else if (outFmt == "aac" || outFmt == "m4a") {
            int aacBitrate = options.targetBitrateKbps > 0 ? options.targetBitrateKbps : 256;
            args << "-c:a" << "aac" << "-b:a" << QString("%1k").arg(aacBitrate);
        } else if (outFmt == "ogg") {
            args << "-c:a" << "libvorbis";
            if (options.targetBitrateKbps > 0) {
                args << "-b:a" << QString("%1k").arg(options.targetBitrateKbps);
            } else {
                args << "-q:a" << "6";
            }
        }
    }

    args << outputPath;

    if (progressCallback) progressCallback(10);
    proc.start("ffmpeg", args);
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return false;
    }
    if (progressCallback) progressCallback(100);

    return (proc.exitCode() == 0 && QFileInfo::exists(outputPath));
}

bool AudioProcessor::extractAudioFromVideo(const QString &videoPath,
                                          const QString &outputPath,
                                          const QString &format,
                                          std::function<void(int)> progressCallback)
{
    if (videoPath.isEmpty() || outputPath.isEmpty() || !QFileInfo::exists(videoPath)) {
        return false;
    }

    QProcess proc;
    QStringList args;
    args << "-y" << "-i" << videoPath << "-vn";

    QString fmt = format.toLower();
    if (fmt == "copy") {
        args << "-c:a" << "copy";
    } else if (fmt == "mp3") {
        args << "-c:a" << "libmp3lame" << "-b:a" << "320k";
    } else if (fmt == "flac") {
        args << "-c:a" << "flac";
    } else if (fmt == "wav") {
        args << "-c:a" << "pcm_s16le";
    } else if (fmt == "aac" || fmt == "m4a") {
        args << "-c:a" << "aac" << "-b:a" << "256k";
    } else if (fmt == "ogg") {
        args << "-c:a" << "libvorbis" << "-q:a" << "6";
    } else {
        args << "-c:a" << "libmp3lame" << "-b:a" << "320k";
    }

    args << outputPath;

    if (progressCallback) progressCallback(20);
    proc.start("ffmpeg", args);
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        return false;
    }
    if (progressCallback) progressCallback(100);

    return (proc.exitCode() == 0 && QFileInfo::exists(outputPath));
}

bool AudioProcessor::writeMetadata(const QString &filePath,
                                  const AudioMetadata &metadata,
                                  const QString &coverImagePath,
                                  bool removeCover)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return false;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix();
    QString tempOut = QDir::tempPath() + QString("/bubez_meta_%1.%2").arg(QFileInfo(filePath).baseName(), ext);

    QProcess proc;
    QStringList args;
    args << "-y" << "-i" << filePath;

    bool embeddingCover = (!coverImagePath.isEmpty() && QFileInfo::exists(coverImagePath));
    if (embeddingCover) {
        args << "-i" << coverImagePath;
        args << "-map" << "0:a" << "-map" << "1:0";
        args << "-c" << "copy";
        args << "-disposition:v:0" << "attached_pic";
    } else if (removeCover) {
        args << "-vn" << "-c:a" << "copy";
    } else {
        args << "-c" << "copy";
    }

    // Metadata tags
    if (!metadata.title.isEmpty()) args << "-metadata" << QString("title=%1").arg(metadata.title);
    if (!metadata.artist.isEmpty()) args << "-metadata" << QString("artist=%1").arg(metadata.artist);
    if (!metadata.album.isEmpty()) args << "-metadata" << QString("album=%1").arg(metadata.album);
    if (!metadata.albumArtist.isEmpty()) args << "-metadata" << QString("album_artist=%1").arg(metadata.albumArtist);
    if (!metadata.genre.isEmpty()) args << "-metadata" << QString("genre=%1").arg(metadata.genre);
    if (!metadata.date.isEmpty()) {
        args << "-metadata" << QString("date=%1").arg(metadata.date);
        if (metadata.year.isEmpty()) {
            args << "-metadata" << QString("year=%1").arg(metadata.date.left(4));
        }
    }
    if (!metadata.year.isEmpty()) {
        args << "-metadata" << QString("year=%1").arg(metadata.year);
        if (metadata.date.isEmpty()) {
            args << "-metadata" << QString("date=%1").arg(metadata.year);
        }
    }
    if (!metadata.trackNumber.isEmpty()) args << "-metadata" << QString("track=%1").arg(metadata.trackNumber);
    if (!metadata.comment.isEmpty()) args << "-metadata" << QString("comment=%1").arg(metadata.comment);

    args << tempOut;

    proc.start("ffmpeg", args);
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0) {
        QFile::remove(tempOut);
        return false;
    }

    // Replace original file safely with updated version
    if (QFile::remove(filePath)) {
        if (QFile::rename(tempOut, filePath)) {
            return true;
        }
    }
    return false;
}

bool AudioProcessor::renameAudioFile(const QString &oldPath,
                                    const QString &newFileName,
                                    QString &outNewPath,
                                    QString &outError)
{
    if (oldPath.isEmpty() || !QFileInfo::exists(oldPath)) {
        outError = "Original file does not exist.";
        return false;
    }

    // Sanitize new file name: remove path separators and invalid characters
    QString cleanName = QFileInfo(newFileName).fileName().trimmed();
    static const QRegularExpression invalidChars(R"([<>:"/\\|?*])");
    cleanName.remove(invalidChars);
    if (cleanName.isEmpty()) {
        outError = "New file name is invalid.";
        return false;
    }

    QFileInfo fi(oldPath);
    QDir parentDir = fi.dir();
    QString targetPath = parentDir.filePath(cleanName);

    if (QFileInfo(oldPath).canonicalFilePath() == QFileInfo(targetPath).canonicalFilePath()) {
        // Same file on disk, nothing to change
        outNewPath = targetPath;
        return true;
    }

    if (QFileInfo::exists(targetPath)) {
        outError = QString("Target file already exists: %1").arg(cleanName);
        return false;
    }

    if (!QFile::rename(oldPath, targetPath)) {
        outError = QString("Failed to rename file to: %1").arg(cleanName);
        return false;
    }

    outNewPath = targetPath;
    return true;
}

bool AudioProcessor::detectAudioBoundaries(const QString &filePath,
                                          qint64 &outStartMs,
                                          qint64 &outEndMs,
                                          int silenceThresholdDb)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return false;
    }

    AudioMetadata meta = readMetadata(filePath);
    if (meta.durationMs <= 0) {
        return false;
    }

    outStartMs = 0;
    outEndMs = meta.durationMs;

    QProcess proc;
    QStringList args;
    args << "-i" << filePath
         << "-af" << QString("silencedetect=noise=%1dB:d=0.2").arg(silenceThresholdDb)
         << "-f" << "null" << "-";

    proc.start("ffmpeg", args);
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        return false;
    }

    QString output = QString::fromUtf8(proc.readAllStandardError());
    QStringList lines = output.split('\n');

    double firstSilenceEnd = -1.0;
    double lastSilenceStart = -1.0;

    static const QRegularExpression reStart("silence_start:\\s*([0-9.]+)");
    static const QRegularExpression reEnd("silence_end:\\s*([0-9.]+)");

    for (const QString &line : lines) {
        auto mEnd = reEnd.match(line);
        if (mEnd.hasMatch() && firstSilenceEnd < 0.0) {
            firstSilenceEnd = mEnd.captured(1).toDouble();
        }

        auto mStart = reStart.match(line);
        if (mStart.hasMatch()) {
            lastSilenceStart = mStart.captured(1).toDouble();
        }
    }

    if (firstSilenceEnd > 0.0) {
        outStartMs = static_cast<qint64>(firstSilenceEnd * 1000.0);
    }
    if (lastSilenceStart > 0.0) {
        qint64 endMsCandidate = static_cast<qint64>(lastSilenceStart * 1000.0);
        if (endMsCandidate > outStartMs) {
            outEndMs = endMsCandidate;
        }
    }

    return true;
}

QList<CueTrack> AudioProcessor::parseCueSheet(const QString &cuePath, qint64 totalAudioDurationMs)
{
    QList<CueTrack> tracks;
    QFile file(cuePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return tracks;
    }

    QTextStream in(&file);
    QString globalPerformer;
    CueTrack currentTrack;
    bool inTrack = false;

    static const QRegularExpression rePerformer("^\\s*PERFORMER\\s+\"?(.*?)\"?\\s*$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reTitle("^\\s*TITLE\\s+\"?(.*?)\"?\\s*$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reTrack("^\\s*TRACK\\s+(\\d+)\\s+AUDIO", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reIndex("^\\s*INDEX\\s+01\\s+(\\d+):(\\d+):(\\d+)", QRegularExpression::CaseInsensitiveOption);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        auto mPerf = rePerformer.match(line);
        if (mPerf.hasMatch()) {
            if (!inTrack) {
                globalPerformer = mPerf.captured(1).trimmed();
            } else {
                currentTrack.performer = mPerf.captured(1).trimmed();
            }
            continue;
        }

        auto mTrk = reTrack.match(line);
        if (mTrk.hasMatch()) {
            if (inTrack) {
                if (currentTrack.performer.isEmpty()) {
                    currentTrack.performer = globalPerformer;
                }
                tracks.append(currentTrack);
            }
            currentTrack = CueTrack();
            currentTrack.trackNumber = mTrk.captured(1).toInt();
            currentTrack.performer = globalPerformer;
            inTrack = true;
            continue;
        }

        auto mTit = reTitle.match(line);
        if (mTit.hasMatch() && inTrack) {
            currentTrack.title = mTit.captured(1).trimmed();
            continue;
        }

        auto mIdx = reIndex.match(line);
        if (mIdx.hasMatch() && inTrack) {
            int mm = mIdx.captured(1).toInt();
            int ss = mIdx.captured(2).toInt();
            int ff = mIdx.captured(3).toInt(); // 75 frames per second
            qint64 ms = (static_cast<qint64>(mm) * 60 + ss) * 1000 + (static_cast<qint64>(ff) * 1000 / 75);
            currentTrack.startMs = ms;
            continue;
        }
    }

    if (inTrack) {
        if (currentTrack.performer.isEmpty()) {
            currentTrack.performer = globalPerformer;
        }
        tracks.append(currentTrack);
    }

    // Calculate durations from track offsets
    for (int i = 0; i < tracks.size(); ++i) {
        if (i + 1 < tracks.size()) {
            tracks[i].durationMs = tracks[i + 1].startMs - tracks[i].startMs;
        } else if (totalAudioDurationMs > tracks[i].startMs) {
            tracks[i].durationMs = totalAudioDurationMs - tracks[i].startMs;
        }
    }

    return tracks;
}

bool AudioProcessor::splitCueSheet(const QString &audioPath,
                                  const QString &cuePath,
                                  const QString &outputDir,
                                  std::function<void(int, int, const QString&)> progressCallback)
{
    if (!QFileInfo::exists(audioPath) || !QFileInfo::exists(cuePath)) {
        return false;
    }

    QDir outDir(outputDir);
    if (!outDir.exists()) {
        outDir.mkpath(".");
    }

    AudioMetadata meta = readMetadata(audioPath);
    QList<CueTrack> tracks = parseCueSheet(cuePath, meta.durationMs);
    if (tracks.isEmpty()) {
        return false;
    }

    QFileInfo audioFi(audioPath);
    QString ext = audioFi.suffix().toLower();
    if (ext.isEmpty()) ext = "mp3";

    for (int i = 0; i < tracks.size(); ++i) {
        const CueTrack &trk = tracks[i];
        if (progressCallback) {
            progressCallback(i + 1, tracks.size(), trk.title);
        }

        QString safeTitle = trk.title;
        safeTitle.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
        if (safeTitle.isEmpty()) {
            safeTitle = QString("Track_%1").arg(trk.trackNumber, 2, 10, QChar('0'));
        }

        QString outFileName = QString("%1 - %2.%3")
            .arg(trk.trackNumber, 2, 10, QChar('0'))
            .arg(safeTitle)
            .arg(ext);
        QString outFilePath = outDir.absoluteFilePath(outFileName);

        QProcess proc;
        QStringList args;
        args << "-y";

        double startSec = static_cast<double>(trk.startMs) / 1000.0;
        args << "-ss" << QString::number(startSec, 'f', 3);

        if (trk.durationMs > 0) {
            double durSec = static_cast<double>(trk.durationMs) / 1000.0;
            args << "-t" << QString::number(durSec, 'f', 3);
        }

        args << "-i" << audioPath;
        args << "-c" << "copy";
        args << "-metadata" << QString("title=%1").arg(trk.title)
             << "-metadata" << QString("artist=%1").arg(trk.performer)
             << "-metadata" << QString("track=%1").arg(trk.trackNumber);
        args << outFilePath;

        proc.start("ffmpeg", args);
        if (!proc.waitForFinished(60000)) {
            proc.kill();
            return false;
        }
    }

    return true;
}
