#pragma once
/*
 * TitanShare Daemon — Client Session
 * Per-client TCP state machine: AUTH → HEADER → DATA
 * Mirrors the protocol from the original Node.js daemon.js
 */

#include <string>
#include <vector>
#include <memory>

namespace titanshare {

class SessionManager;
class CommandDispatcher;

enum class SessionStage {
    AUTH,     // Waiting for session key
    HEADER,  // Waiting for CMD: or FILE_START: header
    DATA     // Receiving file binary data
};

class ClientSession {
public:
    ClientSession(int fd, const std::string& remoteIp,
                  std::shared_ptr<SessionManager> sessionMgr,
                  std::shared_ptr<CommandDispatcher> dispatcher);
    ~ClientSession();

    // Called by TcpServer when data arrives
    void onData(const char* data, size_t len);

    int fd() const { return m_fd; }
    const std::string& remoteIp() const { return m_remoteIp; }

private:
    void processBuffer();
    void handleAuth(const std::string& line);
    void handleHeader(const std::string& line);
    void handleFileData();

    void sendResponse(const std::string& response);

    int m_fd;
    std::string m_remoteIp;
    SessionStage m_stage = SessionStage::AUTH;
    std::vector<char> m_buffer;
    std::string m_sessionKey;

    // File transfer state
    std::string m_fileName;
    size_t m_expectedBytes = 0;
    int m_fileFd = -1;

    std::shared_ptr<SessionManager> m_sessionMgr;
    std::shared_ptr<CommandDispatcher> m_dispatcher;
};

} // namespace titanshare
