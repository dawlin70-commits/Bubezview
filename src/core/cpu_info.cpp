/**
 * @file cpu_info.cpp
 * @brief Implementation of host CPU microarchitecture detection and cache topology.
 */

#include "cpu_info.h"

#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QDir>
#include <QFileInfo>

QString CpuInfo::modelName()
{
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Unknown x86_64 Processor");
    }

    QByteArray data = file.readAll();
    QString content = QString::fromUtf8(data);
    QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("model name"), Qt::CaseInsensitive)) {
            int colonIdx = line.indexOf(QLatin1Char(':'));
            if (colonIdx != -1) {
                return line.mid(colonIdx + 1).trimmed();
            }
        }
    }
    return QStringLiteral("Generic x86_64 Processor");
}

int CpuInfo::logicalCores()
{
    int count = QThread::idealThreadCount();
    return (count > 0) ? count : 1;
}

QStringList CpuInfo::cpuFlags()
{
    static QStringList s_flags;
    if (!s_flags.isEmpty()) {
        return s_flags;
    }

    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly)) {
        return s_flags;
    }

    QByteArray data = file.readAll();
    QString content = QString::fromUtf8(data);
    QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("flags"), Qt::CaseInsensitive)) {
            int colonIdx = line.indexOf(QLatin1Char(':'));
            if (colonIdx != -1) {
                QString flagsStr = line.mid(colonIdx + 1).trimmed();
                s_flags = flagsStr.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                break;
            }
        }
    }
    return s_flags;
}

bool CpuInfo::hasFlag(const QString &flag)
{
    const QStringList &flags = cpuFlags();
    return flags.contains(flag.toLower(), Qt::CaseInsensitive);
}

QString CpuInfo::microarchitectureTier()
{
    // x86-64-v4: AVX-512 foundation & vector length extensions
    if (hasFlag(QStringLiteral("avx512f")) && hasFlag(QStringLiteral("avx512vl"))) {
        return QStringLiteral("v4");
    }

    // x86-64-v3: AVX2, FMA, BMI1, BMI2
    if (hasFlag(QStringLiteral("avx2")) && hasFlag(QStringLiteral("fma")) && hasFlag(QStringLiteral("bmi2"))) {
        return QStringLiteral("v3");
    }

    // x86-64-v2: SSE4.2, POPCNT, CMPXCHG16B
    if (hasFlag(QStringLiteral("sse4_2")) && hasFlag(QStringLiteral("popcnt"))) {
        return QStringLiteral("v2");
    }

    return QStringLiteral("v1");
}

bool CpuInfo::isV3OrHigher()
{
    QString tier = microarchitectureTier();
    return (tier == QLatin1String("v3") || tier == QLatin1String("v4"));
}

QString CpuInfo::cacheSummary()
{
    QString l1i, l1d, l2, l3;
    QDir cacheDir("/sys/devices/system/cpu/cpu0/cache");
    if (cacheDir.exists()) {
        QStringList entries = cacheDir.entryList(QStringList() << "index*", QDir::Dirs);
        for (const QString &entry : entries) {
            QString path = cacheDir.filePath(entry);
            QFile fileLevel(path + "/level");
            QFile fileType(path + "/type");
            QFile fileSize(path + "/size");

            QString level, type, size;
            if (fileLevel.open(QIODevice::ReadOnly)) {
                level = QString::fromUtf8(fileLevel.readAll()).trimmed();
            }
            if (fileType.open(QIODevice::ReadOnly)) {
                type = QString::fromUtf8(fileType.readAll()).trimmed();
            }
            if (fileSize.open(QIODevice::ReadOnly)) {
                size = QString::fromUtf8(fileSize.readAll()).trimmed();
            }

            if (level == "1" && type.contains("Instruction", Qt::CaseInsensitive)) {
                l1i = size;
            } else if (level == "1" && type.contains("Data", Qt::CaseInsensitive)) {
                l1d = size;
            } else if (level == "2") {
                l2 = size;
            } else if (level == "3") {
                l3 = size;
            }
        }
    }

    auto formatCache = [](QString s) -> QString {
        s = s.trimmed().toUpper();
        if (s.endsWith(QLatin1Char('K'))) {
            return s.left(s.length() - 1) + QStringLiteral(" KB");
        }
        if (s.endsWith(QLatin1Char('M'))) {
            return s.left(s.length() - 1) + QStringLiteral(" MB");
        }
        return s;
    };

    QStringList parts;
    if (!l1i.isEmpty()) parts << QString("L1i: %1").arg(formatCache(l1i));
    if (!l1d.isEmpty()) parts << QString("L1d: %1").arg(formatCache(l1d));
    if (!l2.isEmpty())  parts << QString("L2: %1").arg(formatCache(l2));
    if (!l3.isEmpty())  parts << QString("L3: %1").arg(formatCache(l3));

    if (parts.isEmpty()) {
        return QStringLiteral("Standard Cache Hierarchy");
    }
    return parts.join(QStringLiteral(" | "));
}
