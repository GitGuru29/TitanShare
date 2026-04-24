#pragma once
/*
 * TitanShare Daemon — System Info Collector
 * Reads CPU usage, RAM, temperature, storage, battery from procfs/sysfs.
 * No external tools required — pure Linux kernel interfaces.
 */

#include <nlohmann/json.hpp>
#include <string>

namespace titanshare {

class SystemInfo {
public:
    SystemInfo();

    // Collect all system metrics, return as JSON object
    nlohmann::json collect();

private:
    // CPU usage (delta-based from /proc/stat)
    double getCpuUsage();

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

    // CPU delta tracking
    struct CpuTimes {
        long long work = 0;
        long long total = 0;
    };
    CpuTimes m_prevCpu;
    CpuTimes readCpuTimes();
};

} // namespace titanshare
