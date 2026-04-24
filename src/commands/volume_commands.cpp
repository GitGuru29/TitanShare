#include "commands/volume_commands.hpp"
#include "utils/process.hpp"
#include "utils/logger.hpp"

namespace titanshare {

std::string VolumeCommands::execute(const std::string& cmd) {
    std::string sysCmd;

    if (cmd == "volume_up") {
        sysCmd = "pactl set-sink-volume @DEFAULT_SINK@ +5%";
    } else if (cmd == "volume_down") {
        sysCmd = "pactl set-sink-volume @DEFAULT_SINK@ -5%";
    } else if (cmd == "mute") {
        sysCmd = "pactl set-sink-mute @DEFAULT_SINK@ toggle";
    } else {
        return "CMD_UNKNOWN\n";
    }

    Logger::info("VOLUME", "🔊 Executing: " + sysCmd);
    auto result = Process::exec(sysCmd);

    return result.success() ? "CMD_OK\n" : "CMD_FAIL\n";
}

} // namespace titanshare
