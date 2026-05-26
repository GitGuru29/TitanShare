#pragma once
/*
 * TitanShare Daemon — System Info Collector
 * Reads CPU usage, RAM, temperature, storage, battery from procfs/sysfs.
 * No external tools required — pure Linux kernel interfaces.
 */

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace titanshare {

class SystemInfo {
public:
    SystemInfo();

    // Collect all system metrics, return as JSON object
    nlohmann::json collect();

private:
    // CPU usage (delta-based from /proc/stat)
    double getCpuUsage();

    // Per-core usage — returns vector of [0..100] per logical core
    std::vector<double> getPerCoreCpuUsage();

    // CPU frequency in GHz (average across online cores)
    double getCpuFreqGhz();

    // Number of logical CPU cores
    int getCpuCoreCount();

    // RAM from /proc/meminfo
    void getMemoryInfo(double& usagePercent, long& usedMB, long& totalMB);

    // CPU temperature from /sys/class/thermal
    double getCpuTemperature();

    // Storage from statvfs()
    void getStorageInfo(double& usagePercent, long& usedGB, long& totalGB);

    // Battery from /sys/class/power_supply
    int getBatteryPercent();

    // Device brand/model from DMI
    std::string getBrand();
    std::string getModel();

    // OS version
    std::string getOsVersion();

    // Helper: read a sysfs file
    std::string readSysFile(const std::string& path);

    // CPU delta tracking — index 0 = aggregate "cpu", 1..N = "cpu0".."cpuN-1"
    struct CpuTimes {
        long long work  = 0;
        long long total = 0;
    };
    CpuTimes              m_prevCpu;        // aggregate
    std::vector<CpuTimes> m_prevCoreCpu;    // per-core
    CpuTimes readCpuTimes();
    std::vector<CpuTimes> readAllCoreTimes();
};

} // namespace titanshare
