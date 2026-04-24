#include "commands/command_dispatcher.hpp"
#include "commands/power_commands.hpp"
#include "commands/volume_commands.hpp"
#include "commands/lock_commands.hpp"
#include "commands/system_info.hpp"
#include "input/virtual_input.hpp"
#include "utils/logger.hpp"
#include "utils/network.hpp"
#include "config.hpp"

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <cstring>

namespace titanshare {

using json = nlohmann::json;

CommandDispatcher::CommandDispatcher() = default;
CommandDispatcher::~CommandDispatcher() = default;

void CommandDispatcher::init() {
    // Initialize virtual input devices
    m_input = std::make_unique<VirtualInput>();
    if (!m_input->init()) {
        Logger::error("DISPATCH", "⚠️  Virtual input init failed. Mouse/Keyboard won't work.");
        Logger::error("DISPATCH", "    Check: sudo chmod 0666 /dev/uinput");
    }

    // Initialize system info collector
    m_sysInfo = std::make_unique<SystemInfo>();
}

std::string CommandDispatcher::dispatch(const std::string& cmd, int clientFd) {
    // ─── Power Commands ──────────────────────────────────────
    if (cmd == "shutdown" || cmd == "reboot" || cmd == "sleep") {
        return handlePowerCommand(cmd);
    }

    // ─── Volume Commands ─────────────────────────────────────
    if (cmd == "volume_up" || cmd == "volume_down" || cmd == "mute") {
        return handleVolumeCommand(cmd);
    }

    // ─── Lock / Unlock / Wakeup ──────────────────────────────
    if (cmd == "lock" || cmd == "unlock" || cmd == "wakeup") {
        return handleLockCommand(cmd);
    }

    // ─── System Info ─────────────────────────────────────────
    if (cmd == "get_info") {
        return handleSystemInfo(clientFd);
    }

    // ─── Mirror Control ──────────────────────────────────────
    if (cmd == "START_MIRROR" || cmd == "STOP_MIRROR") {
        return handleMirrorCommand(cmd, clientFd);
    }

    // ─── Mouse Commands ──────────────────────────────────────
    if (cmd.substr(0, 11) == "MOUSE_MOVE:") {
        return handleInputCommand(cmd);
    }
    if (cmd.substr(0, 13) == "MOUSE_SCROLL:") {
        return handleInputCommand(cmd);
    }
    if (cmd.substr(0, 12) == "MOUSE_CLICK:") {
        return handleInputCommand(cmd);
    }
    if (cmd.substr(0, 11) == "MOUSE_DOWN:") {
        return handleInputCommand(cmd);
    }
    if (cmd.substr(0, 9) == "MOUSE_UP:") {
        return handleInputCommand(cmd);
    }

    // ─── Keyboard Commands ───────────────────────────────────
    if (cmd.substr(0, 9) == "KEY_TYPE:") {
        return handleInputCommand(cmd);
    }
    if (cmd.substr(0, 10) == "KEY_PRESS:") {
        return handleInputCommand(cmd);
    }

    Logger::warn("DISPATCH", "Unknown command: " + cmd);
    return "CMD_UNKNOWN\n";
}

std::string CommandDispatcher::handlePowerCommand(const std::string& cmd) {
    return PowerCommands::execute(cmd);
}

std::string CommandDispatcher::handleVolumeCommand(const std::string& cmd) {
    return VolumeCommands::execute(cmd);
}

std::string CommandDispatcher::handleLockCommand(const std::string& cmd) {
    return LockCommands::execute(cmd);
}

std::string CommandDispatcher::handleInputCommand(const std::string& cmd) {
    if (!m_input) return "CMD_FAIL\n";

    // Mouse move
    if (cmd.substr(0, 11) == "MOUSE_MOVE:") {
        auto data = cmd.substr(11);
        auto sep = data.find(':');
        if (sep != std::string::npos) {
            int dx = std::stoi(data.substr(0, sep));
            int dy = std::stoi(data.substr(sep + 1));
            m_input->moveMouse(dx, dy);
        }
        return ""; // No response for mouse moves (performance)
    }

    if (cmd.substr(0, 13) == "MOUSE_SCROLL:") {
        int dy = std::stoi(cmd.substr(13));
        m_input->scrollWheel(dy);
        return "";
    }

    if (cmd.substr(0, 12) == "MOUSE_CLICK:") {
        std::string btn = cmd.substr(12);
        m_input->clickMouse(btn);
        return "";
    }

    if (cmd.substr(0, 11) == "MOUSE_DOWN:") {
        std::string btn = cmd.substr(11);
        m_input->mouseDown(btn);
        return "";
    }

    if (cmd.substr(0, 9) == "MOUSE_UP:") {
        std::string btn = cmd.substr(9);
        m_input->mouseUp(btn);
        return "";
    }

    if (cmd.substr(0, 9) == "KEY_TYPE:") {
        std::string text = cmd.substr(9);
        m_input->typeText(text);
        return "";
    }

    if (cmd.substr(0, 10) == "KEY_PRESS:") {
        std::string key = cmd.substr(10);
        m_input->pressKey(key);
        return "";
    }

    return "CMD_FAIL\n";
}

std::string CommandDispatcher::handleSystemInfo(int /*clientFd*/) {
    auto info = m_sysInfo->collect();

    json response;
    response["type"] = "system_info";
    response["data"] = info;

    return response.dump() + "\n";
}

std::string CommandDispatcher::handleMirrorCommand(const std::string& cmd, int /*clientFd*/) {
    if (cmd == "START_MIRROR") {
        json response;
        response["type"] = "MIRROR_READY";
        response["ip"] = Network::getLocalIp();
        response["port"] = config::MIRROR_PORT;
        return response.dump() + "\n";
    }
    else if (cmd == "STOP_MIRROR") {
        return "CMD_OK\n";
    }
    return "CMD_FAIL\n";
}

} // namespace titanshare
