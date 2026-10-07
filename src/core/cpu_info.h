/**
 * @file cpu_info.h
 * @brief Host CPU microarchitecture detection and cache introspection for BubezView.
 */

#pragma once

#include <QString>
#include <QStringList>

/**
 * @class CpuInfo
 * @brief Provides host processor model inspection, cache topology, and x86-64 microarchitecture tier classification (v1 to v4).
 */
class CpuInfo {
public:
    /**
     * @brief Retrieves the human-readable CPU model string (e.g. "AMD Ryzen 9 3900X 12-Core Processor").
     * @return Model name string.
     */
    static QString modelName();

    /**
     * @brief Returns the total count of logical CPU threads available on the host.
     * @return Number of logical cores.
     */
    static int logicalCores();

    /**
     * @brief Determines the highest supported x86-64 microarchitecture level ("v1", "v2", "v3", or "v4").
     * @return Microarchitecture tier identifier.
     */
    static QString microarchitectureTier();

    /**
     * @brief Checks if host processor meets or exceeds x86-64-v3 requirements (AVX2, FMA, BMI1, BMI2).
     * @return True if v3 or v4 is supported; false for v1 or v2 legacy processors.
     */
    static bool isV3OrHigher();

    /**
     * @brief Inspects Linux sysfs cache topology to build a structured cache summary string.
     * @return Formatted cache description (e.g. "L1i: 32K | L1d: 32K | L2: 512K | L3: 16M").
     */
    static QString cacheSummary();

    /**
     * @brief Checks whether the host CPU supports a specific hardware instruction flag.
     * @param flag Name of flag in /proc/cpuinfo (e.g. "avx2", "avx512f", "clzero").
     * @return True if instruction is supported in hardware.
     */
    static bool hasFlag(const QString &flag);

    /**
     * @brief Returns list of all CPU instruction flags detected on the primary core.
     * @return List of flag strings.
     */
    static QStringList cpuFlags();
};
