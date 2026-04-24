#pragma once
#include <string>

namespace bybridge {

class PowerCommands {
public:
    // Execute a power command: "shutdown", "reboot", "sleep"
    static std::string execute(const std::string& cmd);
};

} // namespace bybridge
