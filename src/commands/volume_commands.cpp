#include "commands/volume_commands.hpp"
#include "utils/process.hpp"
#include "utils/logger.hpp"

#include <sstream>
#include <unistd.h>

namespace titanshare {

std::string VolumeCommands::execute(const std::string& cmd) {
    // 1. Find the active graphical user
    auto listResult = Process::exec("/usr/bin/loginctl list-sessions --no-legend");
    std::string sessionUser = "msfvenom"; // fallback
    if (listResult.success()) {
        std::istringstream stream(listResult.output);
        std::string line, s_id, s_uid, s_user, s_seat;
        while (std::getline(stream, line)) {
            std::istringstream ls(line);
            if (ls >> s_id >> s_uid >> s_user >> s_seat) {
                if (s_seat == "seat0") { sessionUser = s_user; break; }
            }
        }
    }

    // 2. Build the command. If root, impersonate the user so PulseAudio/PipeWire accepts it.
    std::string prefix = (getuid() == 0)
        ? "runuser -u " + sessionUser + " -- env XDG_RUNTIME_DIR=/run/user/$(id -u " + sessionUser + ") "
        : "";

    std::string sysCmd;
    if (cmd == "volume_up") {
        sysCmd = prefix + "pactl set-sink-volume @DEFAULT_SINK@ +5%";
    } else if (cmd == "volume_down") {
        sysCmd = prefix + "pactl set-sink-volume @DEFAULT_SINK@ -5%";
    } else if (cmd == "mute") {
        sysCmd = prefix + "pactl set-sink-mute @DEFAULT_SINK@ toggle";
    } else {
        return "CMD_UNKNOWN\n";
    }

    Logger::info("VOLUME", "🔊 Executing: " + sysCmd);
    auto result = Process::exec(sysCmd);

    return result.success() ? "CMD_OK\n" : "CMD_FAIL\n";
}

} // namespace titanshare
