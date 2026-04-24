#include "commands/lock_commands.hpp"
#include "utils/process.hpp"
#include "utils/logger.hpp"

#include <sstream>
#include <vector>
#include <unistd.h>

namespace bybridge {

std::string LockCommands::execute(const std::string& cmd) {
    if (cmd == "lock") {
        Logger::info("LOCK", "🔒 Locking session");
        auto result = Process::exec("loginctl lock-session");
        return result.success() ? "CMD_OK\n" : "CMD_FAIL\n";
    }
    else if (cmd == "unlock") {
        return unlockSession();
    }
    else if (cmd == "wakeup") {
        Logger::info("LOCK", "💡 Waking screen");
        Process::exec("xset dpms force on 2>/dev/null || true");
        return "CMD_OK\n";
    }

    return "CMD_UNKNOWN\n";
}

std::string LockCommands::unlockSession() {
    Logger::info("UNLOCK", "--- Starting Unlock Sequence ---");

    // Find the active session on seat0
    auto listResult = Process::exec("/usr/bin/loginctl list-sessions --no-legend");
    if (!listResult.success()) {
        Logger::error("UNLOCK", "❌ Failed to list sessions");
        return "CMD_FAIL\n";
    }

    std::string sessionId;
    std::string sessionUser;

    std::istringstream stream(listResult.output);
    std::string line;
    while (std::getline(stream, line)) {
        // Parse: SESSION UID USER SEAT TTY
        std::istringstream lineStream(line);
        std::vector<std::string> parts;
        std::string part;
        while (lineStream >> part) {
            parts.push_back(part);
        }

        if (parts.size() >= 4 && parts[3] == "seat0") {
            sessionId = parts[0];
            sessionUser = parts[2];
            break;
        }
    }

    if (sessionId.empty() || sessionUser.empty()) {
        Logger::error("UNLOCK", "❌ No active graphical session found on seat0");
        return "CMD_FAIL\n";
    }

    Logger::info("UNLOCK", "✅ Target: User='" + sessionUser +
                 "' Session='" + sessionId + "'");

    bool isRoot = (getuid() == 0);
    std::string unlockCmd, activateCmd, xsetCmd;

    if (isRoot) {
        Logger::info("UNLOCK", "Running as ROOT. Using runuser impersonation.");
        unlockCmd = "runuser -u " + sessionUser +
                    " -- /usr/bin/loginctl unlock-session " + sessionId;
        activateCmd = "runuser -u " + sessionUser +
                      " -- /usr/bin/loginctl activate " + sessionId;
        xsetCmd = "runuser -u " + sessionUser +
                  " -- env DISPLAY=:0 xset dpms force on";
    } else {
        Logger::info("UNLOCK", "Running as '" + sessionUser + "' (Non-Root).");
        unlockCmd = "/usr/bin/loginctl unlock-session " + sessionId;
        activateCmd = "/usr/bin/loginctl activate " + sessionId;
        xsetCmd = "DISPLAY=:0 xset dpms force on";
    }

    Logger::info("UNLOCK", "🚀 Executing Unlock: " + unlockCmd);
    auto unlockResult = Process::exec(unlockCmd);
    if (!unlockResult.success()) {
        Logger::error("UNLOCK", "❌ Unlock failed");
    } else {
        Logger::info("UNLOCK", "🔓 Unlock signal sent");
    }

    // Activate session
    Logger::info("UNLOCK", "🚀 Executing Activate: " + activateCmd);
    Process::execDetached(activateCmd);

    // Wake screen
    Logger::info("UNLOCK", "💡 Waking screen: " + xsetCmd);
    Process::execDetached(xsetCmd);

    return "CMD_OK\n";
}

} // namespace bybridge
