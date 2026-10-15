#include "equalizer_manager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

EqualizerManager &EqualizerManager::instance()
{
    static EqualizerManager s_instance;
    return s_instance;
}

const QVector<int> &EqualizerManager::bandFrequencies()
{
    static const QVector<int> s_bands = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    return s_bands;
}

QString EqualizerManager::formatFrequency(int freqHz)
{
    if (freqHz >= 1000) {
        return QString("%1 kHz").arg(freqHz / 1000);
    }
    return QString("%1 Hz").arg(freqHz);
}

EqualizerManager::EqualizerManager()
{
    initFactoryPresets();
    loadCustomPresets();
}

void EqualizerManager::initFactoryPresets()
{
    m_factoryPresets.clear();

    auto makeFactory = [](const QString &name, const QVector<double> &gains, double preAmp = 0.0, double width = 1.0, bool limiter = true) {
        EqualizerPreset p;
        p.name = name;
        p.isFactory = true;
        p.preAmpDb = preAmp;
        p.bandGainsDb = gains;
        p.limiterEnabled = limiter;
        p.stereoWidth = width;
        return p;
    };

    m_factoryPresets.append(makeFactory(tr("Flat (Default)"), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0.0, 1.0, false));
    m_factoryPresets.append(makeFactory(tr("Rock"), {4.5, 3.5, 2.0, -1.0, -1.5, 0.5, 2.0, 3.5, 4.5, 5.0}));
    m_factoryPresets.append(makeFactory(tr("Pop"), {1.5, 2.5, 3.0, 2.0, 1.0, 2.0, 3.0, 2.5, 2.0, 1.5}));
    m_factoryPresets.append(makeFactory(tr("Classical"), {3.0, 2.5, 2.0, 1.0, -0.5, -0.5, 0.5, 2.0, 2.5, 3.0}));
    m_factoryPresets.append(makeFactory(tr("Jazz"), {3.0, 2.0, 1.5, 1.5, -1.0, -1.0, 1.0, 2.0, 3.0, 3.5}));
    m_factoryPresets.append(makeFactory(tr("Club / Dance"), {6.0, 5.0, 3.5, 1.0, 0.0, 1.0, 2.5, 3.5, 4.5, 3.0}));
    m_factoryPresets.append(makeFactory(tr("Bass Boost"), {7.0, 6.0, 4.5, 2.5, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0}));
    m_factoryPresets.append(makeFactory(tr("Treble Boost"), {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.5, 4.5, 6.0, 7.5}));
    m_factoryPresets.append(makeFactory(tr("Vocal Presence"), {-2.0, -1.5, -1.0, 1.0, 3.0, 4.5, 3.5, 2.0, 1.0, -0.5}));
    m_factoryPresets.append(makeFactory(tr("Acoustic"), {3.0, 2.5, 1.5, 0.5, 1.0, 1.5, 2.5, 3.5, 3.0, 2.5}));
}

void EqualizerManager::loadCustomPresets()
{
    m_customPresets.clear();
    QSettings settings("Bubez", "BubezView");
    int size = settings.beginReadArray("audio/custom_eq_presets");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        EqualizerPreset p;
        p.name = settings.value("name").toString();
        p.isFactory = false;
        p.preAmpDb = settings.value("preAmpDb", 0.0).toDouble();
        p.limiterEnabled = settings.value("limiterEnabled", true).toBool();
        p.stereoWidth = settings.value("stereoWidth", 1.0).toDouble();

        QVariantList list = settings.value("bandGainsDb").toList();
        p.bandGainsDb.resize(10);
        for (int k = 0; k < 10; ++k) {
            p.bandGainsDb[k] = (k < list.size()) ? list[k].toDouble() : 0.0;
        }

        if (!p.name.isEmpty()) {
            m_customPresets.append(p);
        }
    }
    settings.endArray();
}

void EqualizerManager::saveCustomPresets()
{
    QSettings settings("Bubez", "BubezView");
    settings.beginWriteArray("audio/custom_eq_presets");
    for (int i = 0; i < m_customPresets.size(); ++i) {
        settings.setArrayIndex(i);
        const EqualizerPreset &p = m_customPresets[i];
        settings.setValue("name", p.name);
        settings.setValue("preAmpDb", p.preAmpDb);
        settings.setValue("limiterEnabled", p.limiterEnabled);
        settings.setValue("stereoWidth", p.stereoWidth);

        QVariantList list;
        for (double g : p.bandGainsDb) {
            list.append(g);
        }
        settings.setValue("bandGainsDb", list);
    }
    settings.endArray();
}

QList<EqualizerPreset> EqualizerManager::allPresets() const
{
    QList<EqualizerPreset> list = m_factoryPresets;
    list.append(m_customPresets);
    return list;
}

EqualizerPreset EqualizerManager::presetByName(const QString &name) const
{
    for (const auto &p : m_customPresets) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) return p;
    }
    for (const auto &p : m_factoryPresets) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) return p;
    }
    return m_factoryPresets.isEmpty() ? EqualizerPreset() : m_factoryPresets.first();
}

bool EqualizerManager::saveCustomPreset(const EqualizerPreset &preset)
{
    QString trimmedName = preset.name.trimmed();
    if (trimmedName.isEmpty()) return false;

    // Check if name collides with a factory preset
    for (const auto &fp : m_factoryPresets) {
        if (fp.name.compare(trimmedName, Qt::CaseInsensitive) == 0) {
            return false; // Reserved
        }
    }

    EqualizerPreset p = preset;
    p.name = trimmedName;
    p.isFactory = false;

    // Replace if exists, or append
    bool replaced = false;
    for (int i = 0; i < m_customPresets.size(); ++i) {
        if (m_customPresets[i].name.compare(trimmedName, Qt::CaseInsensitive) == 0) {
            m_customPresets[i] = p;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_customPresets.append(p);
    }

    saveCustomPresets();
    emit presetsChanged();
    return true;
}

bool EqualizerManager::deleteCustomPreset(const QString &presetName)
{
    for (int i = 0; i < m_customPresets.size(); ++i) {
        if (m_customPresets[i].name.compare(presetName, Qt::CaseInsensitive) == 0) {
            m_customPresets.removeAt(i);
            saveCustomPresets();
            emit presetsChanged();
            return true;
        }
    }
    return false;
}

QString EqualizerManager::buildFfmpegFilterString(const EqualizerPreset &preset)
{
    QStringList filters;

    // 1. Pre-Amplification
    if (std::abs(preset.preAmpDb) > 0.05) {
        filters << QString("volume=%1dB").arg(preset.preAmpDb, 0, 'f', 2);
    }

    // 2. 10 Parametric Equalizer Bands
    const auto &freqs = bandFrequencies();
    for (int i = 0; i < freqs.size() && i < preset.bandGainsDb.size(); ++i) {
        double gain = preset.bandGainsDb[i];
        if (std::abs(gain) > 0.1) {
            filters << QString("equalizer=f=%1:width_type=o:w=1:g=%2")
                           .arg(freqs[i])
                           .arg(gain, 0, 'f', 1);
        }
    }

    // 3. Stereo Width Imaging
    if (std::abs(preset.stereoWidth - 1.0) > 0.05) {
        // extrastereo: m=0.0 is original, m=-1.0 is mono, m=1.0 is double width
        double m = preset.stereoWidth - 1.0;
        filters << QString("extrastereo=m=%1").arg(m, 0, 'f', 2);
    }

    // 4. True-Peak Limiter (prevents digital clipping from EQ boosts)
    if (preset.limiterEnabled) {
        filters << "alimiter=limit=0.99:attack=7:release=100:asc=1";
    }

    return filters.join(",");
}

QString EqualizerManager::buildMpvAfString(const EqualizerPreset &preset)
{
    QStringList filters;

    // 1. Pre-Amplification
    if (std::abs(preset.preAmpDb) > 0.05) {
        filters << QString("lavfi=[volume=%1dB]").arg(preset.preAmpDb, 0, 'f', 2);
    }

    // 2. 10 Equalizer Bands
    const auto &freqs = bandFrequencies();
    for (int i = 0; i < freqs.size() && i < preset.bandGainsDb.size(); ++i) {
        double gain = preset.bandGainsDb[i];
        if (std::abs(gain) > 0.1) {
            filters << QString("lavfi=[equalizer=f=%1:width_type=o:w=1:g=%2]")
                           .arg(freqs[i])
                           .arg(gain, 0, 'f', 1);
        }
    }

    // 3. Stereo Width
    if (std::abs(preset.stereoWidth - 1.0) > 0.05) {
        double m = preset.stereoWidth - 1.0;
        filters << QString("lavfi=[extrastereo=m=%1]").arg(m, 0, 'f', 2);
    }

    // 4. Limiter
    if (preset.limiterEnabled) {
        filters << "lavfi=[alimiter=limit=0.98:attack=5:release=50]";
    }

    return filters.join(",");
}
