#pragma once
/*
 * TitanShare Daemon — Network Utilities
 * Local IP detection and socket helpers.
 */

#include <string>

namespace titanshare {

class Network {
public:
    // Get the first non-loopback IPv4 address
    static std::string getLocalIp();
};

} // namespace titanshare
