#pragma once
/*
 * TitanShare Daemon — Command Dispatcher
 * Routes CMD:xxx strings to the appropriate handler module.
 */

#include <string>
#include <memory>

namespace titanshare {

class VirtualInput;
class SystemInfo;

class CommandDispatcher {
public:
    CommandDispatcher();
    ~CommandDispatcher();

    // Initialize subsystems (call after construction)
    void init();

    // Dispatch a command, return the response string to send back
    // clientFd is passed for commands that need to send async/JSON responses
    std::string dispatch(const std::string& cmd, int clientFd);

private:
    std::string handlePowerCommand(const std::string& cmd);
    std::string handleVolumeCommand(const std::string& cmd);
    std::string handleLockCommand(const std::string& cmd);
    std::string handleInputCommand(const std::string& cmd);
    std::string handleSystemInfo(int clientFd);
    std::string handleMirrorCommand(const std::string& cmd, int clientFd);

    std::unique_ptr<VirtualInput> m_input;
    std::unique_ptr<SystemInfo> m_sysInfo;
};

} // namespace titanshare
