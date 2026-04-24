#include "commands/system_info.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <sys/statvfs.h>
#include <sys/utsname.h>

namespace bybridge {

SystemInfo::SystemInfo() {
    // Initialize CPU baseline
    m_prevCpu = readCpuTimes();
}

std::string SystemInfo::readSysFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    // Trim whitespace
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' '))
        content.pop_back();
    return content;
}

SystemInfo::CpuTimes SystemInfo::readCpuTimes() {
    CpuTimes times{};
    std::ifstream file(config::PROC_STAT);
    if (!file.is_open()) return times;

    std::string line;
    std::getline(file, line);

    // Format: cpu  user nice system idle iowait irq softirq steal ...
    std::istringstream iss(line);
    std::string label;
    iss >> label; // "cpu"

    long long values[10] = {};
    int count = 0;
    while (count < 10 && iss >> values[count]) count++;

    // work = user + nice + system + irq + softirq + steal
    long long work = values[0] + values[1] + values[2] + values[5] + values[6] + values[7];
    // idle = idle + iowait
    long long idle = values[3] + values[4];

    times.work = work;
    times.total = work + idle;
    return times;
}

double SystemInfo::getCpuUsage() {
    CpuTimes cur = readCpuTimes();
    long long dWork = cur.work - m_prevCpu.work;
    long long dTotal = cur.total - m_prevCpu.total;
    m_prevCpu = cur;

    if (dTotal == 0) return 0.0;
    return (static_cast<double>(dWork) / static_cast<double>(dTotal)) * 100.0;
}

void SystemInfo::getMemoryInfo(double& usagePercent, long& usedMB, long& totalMB) {
    std::ifstream file(config::PROC_MEMINFO);
    if (!file.is_open()) {
        usagePercent = 0.0;
        usedMB = 0;
        totalMB = 0;
        return;
    }

    long memTotal = 0, memAvailable = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 9) == "MemTotal:") {
            std::istringstream iss(line.substr(9));
            iss >> memTotal; // in kB
        }
        else if (line.substr(0, 13) == "MemAvailable:") {
            std::istringstream iss(line.substr(13));
            iss >> memAvailable; // in kB
        }
    }

    totalMB = memTotal / 1024;
    long usedKB = memTotal - memAvailable;
    usedMB = usedKB / 1024;

    if (memTotal > 0) {
        usagePercent = (static_cast<double>(usedKB) / static_cast<double>(memTotal)) * 100.0;
    } else {
        usagePercent = 0.0;
    }
}

double SystemInfo::getCpuTemperature() {
    // Try thermal zones first
    namespace fs = std::filesystem;
    try {
        for (auto& entry : fs::directory_iterator(config::THERMAL_BASE)) {
            std::string name = entry.path().filename().string();
            if (name.find("thermal_zone") == 0) {
                std::string typePath = entry.path().string() + "/type";
                std::string tempPath = entry.path().string() + "/temp";

                std::string type = readSysFile(typePath);
                // Prefer x86_pkg_temp, coretemp, or any CPU-related zone
                if (type.find("x86_pkg") != std::string::npos ||
                    type.find("coretemp") != std::string::npos ||
                    type.find("cpu") != std::string::npos ||
                    type.find("k10temp") != std::string::npos ||
                    type.find("zenpower") != std::string::npos) {

                    std::string tempStr = readSysFile(tempPath);
                    if (!tempStr.empty()) {
                        long milliDeg = std::stol(tempStr);
                        return milliDeg / 1000.0;
                    }
                }
            }
        }

        // Fallback: try thermal_zone0 regardless of type
        std::string fallback = config::THERMAL_BASE + "/thermal_zone0/temp";
        std::string tempStr = readSysFile(fallback);
        if (!tempStr.empty()) {
            return std::stol(tempStr) / 1000.0;
        }
    } catch (...) {}

    // Try hwmon as last resort
    try {
        for (auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
            std::string tempPath = entry.path().string() + "/temp1_input";
            std::string tempStr = readSysFile(tempPath);
            if (!tempStr.empty()) {
                return std::stol(tempStr) / 1000.0;
            }
        }
    } catch (...) {}

    return 0.0;
}

void SystemInfo::getStorageInfo(double& usagePercent, long& usedGB, long& totalGB) {
    struct statvfs stat{};
    if (statvfs("/", &stat) != 0) {
        usagePercent = 0.0;
        usedGB = 0;
        totalGB = 0;
        return;
    }

    unsigned long long totalBytes = static_cast<unsigned long long>(stat.f_blocks) * stat.f_frsize;
    unsigned long long freeBytes = static_cast<unsigned long long>(stat.f_bfree) * stat.f_frsize;
    unsigned long long usedBytes = totalBytes - freeBytes;

    totalGB = static_cast<long>(totalBytes / (1024ULL * 1024 * 1024));
    usedGB = static_cast<long>(usedBytes / (1024ULL * 1024 * 1024));

    if (totalBytes > 0) {
        usagePercent = (static_cast<double>(usedBytes) / static_cast<double>(totalBytes)) * 100.0;
    } else {
        usagePercent = 0.0;
    }
}

int SystemInfo::getBatteryPercent() {
    namespace fs = std::filesystem;
    try {
        for (auto& entry : fs::directory_iterator(config::POWER_SUPPLY_BASE)) {
            std::string name = entry.path().filename().string();
            if (name.find("BAT") == 0) {
                std::string capPath = entry.path().string() + "/capacity";
                std::string capStr = readSysFile(capPath);
                if (!capStr.empty()) {
                    return std::stoi(capStr);
                }
            }
        }
    } catch (...) {}
    return 0;
}

std::string SystemInfo::getBrand() {
    return readSysFile(config::SYS_VENDOR);
}

std::string SystemInfo::getModel() {
    return readSysFile(config::PRODUCT_NAME);
}

std::string SystemInfo::getOsVersion() {
    struct utsname uts{};
    if (uname(&uts) == 0) {
        return std::string(uts.sysname) + " " + std::string(uts.release);
    }
    return "Unknown";
}

nlohmann::json SystemInfo::collect() {
    nlohmann::json data;

    // Device info
    std::string brand = getBrand();
    std::string model = getModel();
    data["brand"] = brand.empty() ? "Unknown" : brand;
    data["model"] = model.empty() ? "Unknown" : model;

    // OS
    data["os_version"] = getOsVersion();

    // CPU
    double cpuLoad = getCpuUsage();
    std::ostringstream cpuStr;
    cpuStr << std::fixed << std::setprecision(1) << cpuLoad;
    data["cpu_load"] = cpuStr.str();

    // CPU Temperature (NEW)
    double cpuTemp = getCpuTemperature();
    std::ostringstream tempStr;
    tempStr << std::fixed << std::setprecision(1) << cpuTemp;
    data["cpu_temp"] = tempStr.str();

    // RAM
    double ramUsage;
    long ramUsed, ramTotal;
    getMemoryInfo(ramUsage, ramUsed, ramTotal);
    std::ostringstream ramStr;
    ramStr << std::fixed << std::setprecision(1) << ramUsage;
    data["ramUsage"] = ramStr.str();
    data["ramUsed"] = std::to_string(ramUsed / 1024); // GB
    data["ramTotal"] = std::to_string(ramTotal / 1024); // GB

    // Storage
    double storageUsage;
    long storageUsed, storageTotal;
    getStorageInfo(storageUsage, storageUsed, storageTotal);
    std::ostringstream storageStr;
    storageStr << std::fixed << std::setprecision(1) << storageUsage;
    data["storage_usage"] = storageStr.str();
    data["storageUsed"] = std::to_string(storageUsed);
    data["storageTotal"] = std::to_string(storageTotal);

    // Battery
    data["battery"] = getBatteryPercent();

    // Network speed (placeholder — same as original Node.js daemon)
    data["net_speed"] = "0 Mbps";

    return data;
}

} // namespace bybridge
