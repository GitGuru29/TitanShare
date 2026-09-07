#pragma once
/*
 * TitanShare Daemon — Session Manager
 * Generates pairing PINs, manages LAN discovery auth,
 * and validates incoming authentication attempts.
 */

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace titanshare {

class SessionManager {
public:
    SessionManager();

    // Generate a new 6-digit PIN (or reuse persisted one)
    void generateSession(bool preservePin = false);

    // Validate an incoming PIN from Android
    bool validateKey(const std::string& pin) const;

    // Get current session info
    std::string currentKey() const;   ///< Returns current PIN string
    std::string currentIp() const;
    std::string toJson() const;       ///< {"ip","port","pin","host"}

    // Brute-force protection: track failures and ban offending IPs.
    // A blocked IP gets an exponential backoff ban (30s, 60s, ...) capped at 1h.
    bool isIpBlocked(const std::string& ip);
    void registerAuthFailure(const std::string& ip);
    void registerAuthSuccess(const std::string& ip);

private:
    std::string generatePin();         ///< Random PAIRING_PIN_DIGITS decimal string
    void persistSession();
    bool loadPersistedSession();

    void setStrictFilePerms(const std::string& file);   ///< chmod 0600 single file

    mutable std::mutex m_mutex;
    std::string m_currentPin;
    std::string m_currentIp;
    std::string m_hostname;
    std::vector<std::string> m_recentPins;

    // Per-IP auth attempts -> ban bookkeeping (guarded by m_mutex)
    std::unordered_map<std::string, int>                                   m_authFailures;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_blockedUntil;
    std::unordered_map<std::string, int>                                   m_banCount;
};

} // namespace titanshare
