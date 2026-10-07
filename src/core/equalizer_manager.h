#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QList>
#include <QSettings>

/**
 * @struct EqualizerPreset
 * @brief Container for a 10-band equalizer configuration, pre-amp, limiter, and spatial width.
 */
struct EqualizerPreset {
    QString name;                       ///< Preset display name
    bool isFactory = false;             ///< True if built-in factory preset (read-only)
    double preAmpDb = 0.0;              ///< Pre-amplification gain in dB (-12.0 to +12.0)
    QVector<double> bandGainsDb;        ///< 10 band gains in dB: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k
    bool limiterEnabled = true;         ///< True if true-peak anti-clipping limiter is active
    double stereoWidth = 1.0;           ///< Stereo imaging factor: 0.0 (Mono), 1.0 (Normal), 2.0 (Wide)

    EqualizerPreset() {
        bandGainsDb.fill(0.0, 10);
    }
};

/**
 * @class EqualizerManager
 * @brief Singleton manager providing centralized equalizer preset persistence,
 * factory presets, and filter string synthesis for both live playback and offline export.
 */
class EqualizerManager : public QObject {
    Q_OBJECT

public:
    static EqualizerManager &instance();

    /**
     * @brief Returns standard center frequencies for the 10 ISO equalizer bands.
     * @return Vector of frequencies in Hz: {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000}.
     */
    static const QVector<int> &bandFrequencies();

    /**
     * @brief Formats a band center frequency for UI display.
     * @param freqHz Frequency in Hz.
     * @return Formatted string (e.g. "31 Hz", "1 kHz").
     */
    static QString formatFrequency(int freqHz);

    /**
     * @brief Returns all available presets (factory presets followed by user custom presets).
     * @return Ordered list of EqualizerPreset.
     */
    QList<EqualizerPreset> allPresets() const;

    /**
     * @brief Finds a preset by name.
     * @param name Name of preset.
     * @return Preset if found; Flat preset otherwise.
     */
    EqualizerPreset presetByName(const QString &name) const;

    /**
     * @brief Saves or updates a user custom preset to persistent storage.
     * @param preset Preset to save.
     * @return True on success; false if preset is invalid or name is reserved by factory preset.
     */
    bool saveCustomPreset(const EqualizerPreset &preset);

    /**
     * @brief Deletes a user custom preset from persistent storage.
     * @param presetName Name of custom preset to remove.
     * @return True if deleted; false if not found or if factory preset.
     */
    bool deleteCustomPreset(const QString &presetName);

    /**
     * @brief Builds an FFmpeg audio filter graph string representing this equalizer configuration.
     * @param preset Equalizer parameters.
     * @return Filter string suitable for ffmpeg -af (e.g. "volume=...dB,equalizer=...,alimiter=...").
     */
    static QString buildFfmpegFilterString(const EqualizerPreset &preset);

    /**
     * @brief Builds a libmpv audio filter string representing this equalizer configuration.
     * @param preset Equalizer parameters.
     * @return Filter string suitable for mpv 'af' property.
     */
    static QString buildMpvAfString(const EqualizerPreset &preset);

signals:
    /**
     * @brief Emitted whenever presets are added, modified, or deleted to update both UI instances.
     */
    void presetsChanged();

private:
    EqualizerManager();
    ~EqualizerManager() override = default;
    Q_DISABLE_COPY(EqualizerManager)

    void initFactoryPresets();
    void loadCustomPresets();
    void saveCustomPresets();

    QList<EqualizerPreset> m_factoryPresets;
    QList<EqualizerPreset> m_customPresets;
};
