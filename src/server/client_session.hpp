#pragma once
/*
 * TitanShare Daemon — Client Session
 * Per-client TCP state machine: AUTH → HEADER → DATA
 */

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <openssl/ssl.h>

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
                  std::shared_ptr<CommandDispatcher> dispatcher,
                  SSL* ssl = nullptr);
    ~ClientSession();

    void onData(const char* data, size_t len);

    int fd() const { return m_fd; }
    SSL* ssl() const { return m_ssl; }
    const std::string& remoteIp() const { return m_remoteIp; }

private:
    void processBuffer();
    void handleAuth(const std::string& line);
    void handleHeader(const std::string& line);
    void handleFileData();
    void sendResponse(const std::string& response);
    void pushFileList();                        ///< Linux→Android: list sendable files
    void pushFile(const std::string& filename); ///< Linux→Android: stream file bytes

    void consumeBuffer(size_t n);
    void compactBuffer();

    size_t bufSize() const { return m_buffer.size() - m_bufferOffset; }
    const char* bufData() const { return m_buffer.data() + m_bufferOffset; }

    int m_fd;
    SSL* m_ssl = nullptr;
    std::string m_remoteIp;
    SessionStage m_stage = SessionStage::AUTH;
    std::vector<char> m_buffer;
    size_t m_bufferOffset = 0;  // read head into m_buffer
    std::string m_sessionKey;
    int m_authAttempts = 0;     // PIN guesses made on this connection
    bool m_closed = false;      // true once we've aborted this socket

    // File transfer state
    std::string m_fileName;
    uint64_t m_expectedBytes = 0;   // Total bytes declared in FILE_START header
    uint64_t m_receivedBytes = 0;   // Bytes written to disk so far
    std::chrono::steady_clock::time_point m_lastProgressUpdate;
    int m_fileFd = -1;

    std::shared_ptr<SessionManager> m_sessionMgr;
    std::shared_ptr<CommandDispatcher> m_dispatcher;
};

} // namespace titanshare
