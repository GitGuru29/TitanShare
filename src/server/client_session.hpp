#pragma once
/*
 * TitanShare Daemon — Client Session
 * Per-client TCP state machine: AUTH → HEADER → DATA
 */

#include <string>
#include <vector>
#include <memory>

namespace titanshare {

class SessionManager;
class CommandDispatcher;

enum class SessionStage {
    AUTH,    // Waiting for PIN
    HEADER,  // Waiting for CMD: or FILE_START:
    DATA     // Receiving file binary data
};

class ClientSession {
public:
    ClientSession(int fd, const std::string& remoteIp,
                  std::shared_ptr<SessionManager> sessionMgr,
                  std::shared_ptr<CommandDispatcher> dispatcher);
    ~ClientSession();

    void onData(const char* data, size_t len);

    int fd() const { return m_fd; }
    const std::string& remoteIp() const { return m_remoteIp; }

private:
    void processBuffer();
    void handleAuth(const std::string& line);
    void handleHeader(const std::string& line);
    void handleFileData();
    void sendResponse(const std::string& response);

    // O(1) buffer consumption: advance m_bufferOffset instead of erasing.
    // Compact only when offset grows large (avoids O(n) shifts per recv).
    void consumeBuffer(size_t n);
    void compactBuffer();

    size_t bufSize() const { return m_buffer.size() - m_bufferOffset; }
    const char* bufData() const { return m_buffer.data() + m_bufferOffset; }

    int m_fd;
    std::string m_remoteIp;
    SessionStage m_stage = SessionStage::AUTH;
    std::vector<char> m_buffer;
    size_t m_bufferOffset = 0;  // read head into m_buffer
    std::string m_sessionKey;

    // File transfer state
    std::string m_fileName;
    size_t m_expectedBytes = 0;   // Total bytes declared in FILE_START header
    size_t m_receivedBytes = 0;   // Bytes written to disk so far
    int m_fileFd = -1;

    std::shared_ptr<SessionManager> m_sessionMgr;
    std::shared_ptr<CommandDispatcher> m_dispatcher;
};

} // namespace titanshare
