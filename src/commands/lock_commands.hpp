#pragma once
#include <string>

namespace titanshare {

class LockCommands {
public:
    // Execute lock/unlock/wakeup commands
    static std::string execute(const std::string& cmd);

private:
    // Advanced unlock: find session on seat0, unlock + activate + wake screen
    static std::string unlockSession();
};

} // namespace titanshare
