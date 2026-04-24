#include "commands/power_commands.hpp"
#include "utils/process.hpp"
#include "utils/logger.hpp"

namespace bybridge {

std::string PowerCommands::execute(const std::string& cmd) {
    std::string sysCmd;

    if (cmd == "shutdown") {
        sysCmd = "systemctl poweroff";
    } else if (cmd == "reboot") {
        sysCmd = "systemctl reboot";
    } else if (cmd == "sleep") {
        sysCmd = "systemctl suspend";
    } else {
        return "CMD_UNKNOWN\n";
    }

    Logger::info("POWER", "⚡ Executing: " + sysCmd);
    auto result = Process::exec(sysCmd);

    return result.success() ? "CMD_OK\n" : "CMD_FAIL\n";
}

} // namespace bybridge
