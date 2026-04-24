#pragma once
/*
 * ByBridge Daemon — Session Manager
 * Generates cryptographic session keys, manages QR pairing,
 * and validates incoming authentication attempts.
 */

#include <string>
#include <vector>
#include <mutex>

namespace bybridge {

class SessionManager {
public:
    SessionManager();

    // Generate a new session key (or reuse persisted one)
    void generateSession(bool preserveKey = false);

    // Validate an incoming session key
    bool validateKey(const std::string& key) const;

    // Get current session info
    std::string currentKey() const;
    std::string currentIp() const;
    std::string toJson() const;

private:
    std::string generateRandomHex(int bytes);
    void persistSession();
    bool loadPersistedSession();

    mutable std::mutex m_mutex;
    std::string m_currentKey;
    std::string m_currentIp;
    std::vector<std::string> m_recentKeys;
};

} // namespace bybridge
