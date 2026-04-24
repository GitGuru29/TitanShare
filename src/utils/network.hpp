#pragma once
/*
 * ByBridge Daemon — Network Utilities
 * Local IP detection and socket helpers.
 */

#include <string>

namespace bybridge {

class Network {
public:
    // Get the first non-loopback IPv4 address
    static std::string getLocalIp();
};

} // namespace bybridge
