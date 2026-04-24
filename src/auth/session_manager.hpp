#pragma once
/*
 * TitanShare Daemon — Session Manager
 * Generates pairing PINs, manages LAN discovery auth,
 * and validates incoming authentication attempts.
 */

#include <string>
#include <vector>
#include <mutex>

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

private:
    std::string generatePin();         ///< Random PAIRING_PIN_DIGITS decimal string
    void persistSession();
    bool loadPersistedSession();

    mutable std::mutex m_mutex;
    std::string m_currentPin;
    std::string m_currentIp;
    std::string m_hostname;
    std::vector<std::string> m_recentPins;
};

} // namespace titanshare
