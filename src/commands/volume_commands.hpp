#pragma once
#include <string>

namespace titanshare {

class VolumeCommands {
public:
    // Execute a volume command: "volume_up", "volume_down", "mute"
    static std::string execute(const std::string& cmd);
};

} // namespace titanshare
