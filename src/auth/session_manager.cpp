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
#include <nlohmann/json.hpp>

namespace titanshare {

using json = nlohmann::json;

SessionManager::SessionManager() {
    m_currentIp = Network::getLocalIp();
}

std::string SessionManager::generateRandomHex(int bytes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);

    std::ostringstream oss;
    for (int i = 0; i < bytes; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << dist(gen);
    }
    return oss.str();
}

void SessionManager::generateSession(bool preserveKey) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_currentIp = Network::getLocalIp();

    if (preserveKey && loadPersistedSession()) {
        Logger::info("QR", "♻ Reusing session key: " + m_currentKey.substr(0, 8) + "...");
    } else {
        m_currentKey = generateRandomHex(config::SESSION_KEY_BYTES);
    }

    // Add to recent keys (rolling window)
    m_recentKeys.push_back(m_currentKey);
    if (m_recentKeys.size() > static_cast<size_t>(config::MAX_RECENT_KEYS)) {
        m_recentKeys.erase(m_recentKeys.begin());
    }

    persistSession();
}

bool SessionManager::validateKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::find(m_recentKeys.begin(), m_recentKeys.end(), key) != m_recentKeys.end();
}

std::string SessionManager::currentKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentKey;
}

std::string SessionManager::currentIp() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentIp;
}

std::string SessionManager::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    json j;
    j["ip"] = m_currentIp;
    j["session"] = m_currentKey;
    return j.dump();
}

void SessionManager::persistSession() {
    try {
        std::filesystem::create_directories(
            std::filesystem::path(config::SESSION_FILE_PATH).parent_path());

        json j;
        j["ip"] = m_currentIp;
        j["session"] = m_currentKey;

        // Write session file
        std::ofstream ofs(config::SESSION_FILE_PATH);
        if (ofs.is_open()) {
            ofs << j.dump();
        }

        // Write QR JSON
        std::ofstream qrOfs(config::QR_JSON_PATH);
        if (qrOfs.is_open()) {
            qrOfs << j.dump();
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
        if (j.contains("session")) {
            m_currentKey = j["session"].get<std::string>();
            return true;
        }
    } catch (...) {}
    return false;
}

} // namespace titanshare
