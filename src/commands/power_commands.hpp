#pragma once
#include <string>

namespace titanshare {

class PowerCommands {
public:
    // Execute a power command: "shutdown", "reboot", "sleep"
    static std::string execute(const std::string& cmd);
};

} // namespace titanshare
