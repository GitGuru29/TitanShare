#include "utils/network.hpp"
#include "utils/logger.hpp"

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

namespace bybridge {

std::string Network::getLocalIp() {
    struct ifaddrs* ifAddrList = nullptr;
    std::string result = "127.0.0.1";

    if (getifaddrs(&ifAddrList) == -1) {
        Logger::error("NET", "getifaddrs() failed: " + std::string(strerror(errno)));
        return result;
    }

    for (struct ifaddrs* ifa = ifAddrList; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        // Skip loopback
        std::string name(ifa->ifa_name);
        if (name == "lo") continue;

        auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
        result = buf;
        break; // Take the first non-loopback IPv4
    }

    freeifaddrs(ifAddrList);
    return result;
}

} // namespace bybridge
