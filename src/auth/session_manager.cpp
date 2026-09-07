#include "auth/session_manager.hpp"
#include "utils/network.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

namespace titanshare {

using json = nlohmann::json;

SessionManager::SessionManager() {
    m_currentIp = Network::getLocalIp();

    // Resolve hostname once
    char buf[256]{};
    gethostname(buf, sizeof(buf) - 1);
    m_hostname = buf;
}

std::string SessionManager::generatePin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Produce exactly PAIRING_PIN_DIGITS decimal digits (no leading zeros)
    int digits      = config::PAIRING_PIN_DIGITS;
    int minVal      = 1;
    for (int i = 1; i < digits; ++i) minVal *= 10;
    int maxVal      = minVal * 10 - 1;
    std::uniform_int_distribution<int> dist(minVal, maxVal);

    return std::to_string(dist(gen));
}

void SessionManager::generateSession(bool preservePin) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_currentIp = Network::getLocalIp();

    if (preservePin && loadPersistedSession()) {
        Logger::info("PIN", "♻ Reusing pairing PIN: " + m_currentPin);
    } else {
        m_currentPin = generatePin();
        Logger::info("PIN", "🔑 New pairing PIN: " + m_currentPin);
    }

    // Add to rolling window of recently valid PINs
    m_recentPins.push_back(m_currentPin);
    if (m_recentPins.size() > static_cast<size_t>(config::MAX_RECENT_PINS)) {
        m_recentPins.erase(m_recentPins.begin());
    }

    persistSession();
}

bool SessionManager::validateKey(const std::string& pin) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::find(m_recentPins.begin(), m_recentPins.end(), pin)
           != m_recentPins.end();
}

std::string SessionManager::currentKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPin;
}

std::string SessionManager::currentIp() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentIp;
}

// ─── Brute-force protection ──────────────────────────────────────────────

bool SessionManager::isIpBlocked(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_blockedUntil.find(ip);
    if (it == m_blockedUntil.end()) return false;

    if (it->second <= std::chrono::steady_clock::now()) {
        // Ban expired — clear all bookkeeping for this IP
        m_blockedUntil.erase(it);
        m_authFailures.erase(ip);
        m_banCount.erase(ip);
        return false;
    }
    return true;
}

void SessionManager::registerAuthFailure(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);

    int failures = ++m_authFailures[ip];
    if (failures >= config::AUTH_FAIL_THRESHOLD) {
        // Escalate: each ban doubles the duration, capped at AUTH_BAN_MAX_SECS
        int secs = config::AUTH_BAN_INITIAL_SECS;
        int nBans = m_banCount[ip]++;
        for (int i = 0; i < nBans && secs < config::AUTH_BAN_MAX_SECS; ++i) {
            secs *= 2;
        }
        secs = std::min(secs, config::AUTH_BAN_MAX_SECS);

        m_blockedUntil[ip] = std::chrono::steady_clock::now() +
                             std::chrono::seconds(secs);
        m_authFailures[ip] = 0;  // restart the window; next threshold escalates again

        Logger::warn("AUTH", "🚫 IP " + ip + " banned for " +
                     std::to_string(secs) + "s after repeated failures");
    }
}

void SessionManager::registerAuthSuccess(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authFailures.erase(ip);
    m_blockedUntil.erase(ip);
    m_banCount.erase(ip);
}

std::string SessionManager::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    json j;
    j["ip"]   = m_currentIp;
    j["port"] = config::TCP_PORT;
    j["pin"]  = m_currentPin;
    j["host"] = m_hostname;
    return j.dump();
}

void SessionManager::setStrictFilePerms(const std::string& file) {
    // PIN files must never be readable by other local users/processes.
    if (chmod(file.c_str(), S_IRUSR | S_IWUSR) != 0) {
        Logger::warn("SESSION", "chmod(0600) on " + file + " failed: " +
                     std::string(strerror(errno)));
    }
}

void SessionManager::persistSession() {
    try {
        std::filesystem::create_directories(
            std::filesystem::path(config::SESSION_FILE_PATH).parent_path());

        json j;
        j["ip"]   = m_currentIp;
        j["pin"]  = m_currentPin;
        j["host"] = m_hostname;

        // Write session file (then restrict perms so only the owner can read it)
        std::ofstream ofs(config::SESSION_FILE_PATH);
        if (ofs.is_open()) {
            ofs << j.dump();
            ofs.close();
            setStrictFilePerms(config::SESSION_FILE_PATH);
        }

        // Write IPC file for GUI (QFileSystemWatcher picks this up)
        std::filesystem::create_directories(
            std::filesystem::path(config::PIN_IPC_PATH).parent_path());
        std::ofstream ipc(config::PIN_IPC_PATH);
        if (ipc.is_open()) {
            json ipcJson;
            ipcJson["pin"]  = m_currentPin;
            ipcJson["host"] = m_hostname;
            ipcJson["ip"]   = m_currentIp;
            ipcJson["port"] = config::TCP_PORT;
            ipc << ipcJson.dump();
            ipc.close();
            setStrictFilePerms(config::PIN_IPC_PATH);
        }
    } catch (const std::exception& e) {
        Logger::error("SESSION", "Failed to persist session: " + std::string(e.what()));
    }
}

bool SessionManager::loadPersistedSession() {
    try {
        if (!std::filesystem::exists(config::SESSION_FILE_PATH)) return false;

        std::ifstream ifs(config::SESSION_FILE_PATH);
        if (!ifs.is_open()) return false;

        json j = json::parse(ifs);
        if (j.contains("pin")) {
            m_currentPin = j["pin"].get<std::string>();
            return true;
        }
    } catch (...) {}
    return false;
}

} // namespace titanshare
