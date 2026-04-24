#include "server/client_session.hpp"
#include "auth/session_manager.hpp"
#include "commands/command_dispatcher.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace bybridge {

ClientSession::ClientSession(int fd, const std::string& remoteIp,
                             std::shared_ptr<SessionManager> sessionMgr,
                             std::shared_ptr<CommandDispatcher> dispatcher)
    : m_fd(fd)
    , m_remoteIp(remoteIp)
    , m_sessionMgr(std::move(sessionMgr))
    , m_dispatcher(std::move(dispatcher))
{}

ClientSession::~ClientSession() {
    if (m_fileFd >= 0) {
        close(m_fileFd);
        m_fileFd = -1;
    }
}

void ClientSession::onData(const char* data, size_t len) {
    m_buffer.insert(m_buffer.end(), data, data + len);
    processBuffer();
}

void ClientSession::sendResponse(const std::string& response) {
    send(m_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
}

void ClientSession::processBuffer() {
    while (!m_buffer.empty()) {
        if (m_stage == SessionStage::AUTH) {
            // Find newline
            auto it = std::find(m_buffer.begin(), m_buffer.end(), '\n');
            if (it == m_buffer.end()) break;

            std::string line(m_buffer.begin(), it);
            m_buffer.erase(m_buffer.begin(), it + 1);

            // Trim \r if present
            if (!line.empty() && line.back() == '\r') line.pop_back();

            handleAuth(line);
        }
        else if (m_stage == SessionStage::HEADER) {
            auto it = std::find(m_buffer.begin(), m_buffer.end(), '\n');
            if (it == m_buffer.end()) break;

            std::string line(m_buffer.begin(), it);
            m_buffer.erase(m_buffer.begin(), it + 1);

            if (!line.empty() && line.back() == '\r') line.pop_back();

            handleHeader(line);
        }
        else if (m_stage == SessionStage::DATA) {
            handleFileData();
            break; // handleFileData manages its own buffer consumption
        }
    }
}

void ClientSession::handleAuth(const std::string& line) {
    std::string key = line;
    // Trim whitespace
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());

    if (m_sessionMgr->validateKey(key)) {
        m_sessionKey = key;
        sendResponse("AUTH_OK\n");
        m_stage = SessionStage::HEADER;
        Logger::info("AUTH", "✅ Client authenticated: " + m_remoteIp);
    } else {
        sendResponse("AUTH_FAIL\n");
        Logger::warn("AUTH", "❌ Auth failed from: " + m_remoteIp);
        // Connection will be closed by the server
    }
}

void ClientSession::handleHeader(const std::string& line) {
    if (line.substr(0, 4) == "CMD:") {
        std::string cmd = line.substr(4);
        // Trim
        while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());

        Logger::info("CMD", "Command: " + cmd + " from " + m_remoteIp);

        // Dispatch to command handler
        std::string response = m_dispatcher->dispatch(cmd, m_fd);
        if (!response.empty()) {
            sendResponse(response);
        }
    }
    else if (line.substr(0, 11) == "FILE_START:") {
        // Parse FILE_START:<filename>:<size>
        auto firstColon = line.find(':', 11);
        if (firstColon == std::string::npos) {
            sendResponse("CMD_FAIL\n");
            return;
        }

        m_fileName = line.substr(11, firstColon - 11);
        // Sanitize filename — replace path separators
        std::replace(m_fileName.begin(), m_fileName.end(), '/', '_');
        std::replace(m_fileName.begin(), m_fileName.end(), '\\', '_');

        try {
            m_expectedBytes = std::stoull(line.substr(firstColon + 1));
        } catch (...) {
            sendResponse("CMD_FAIL\n");
            return;
        }

        // Ensure received_files directory exists
        std::filesystem::create_directories(config::RECEIVED_FILES_DIR);

        std::string filePath = config::RECEIVED_FILES_DIR + "/" + m_fileName;
        m_fileFd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (m_fileFd < 0) {
            Logger::error("FILE", "Failed to create file: " + filePath);
            sendResponse("CMD_FAIL\n");
            return;
        }

        m_stage = SessionStage::DATA;
        sendResponse("READY_FOR_FILE\n");
        Logger::info("FILE", "📥 Receiving: " + m_fileName +
                     " (" + std::to_string(m_expectedBytes) + " bytes)");
    }
    else {
        sendResponse("UNKNOWN_REQUEST\n");
    }
}

void ClientSession::handleFileData() {
    // Look for FILE_END\n sentinel in buffer
    const std::string sentinel = "FILE_END\n";

    auto it = std::search(m_buffer.begin(), m_buffer.end(),
                          sentinel.begin(), sentinel.end());

    if (it != m_buffer.end()) {
        // Write everything before the sentinel
        size_t dataLen = std::distance(m_buffer.begin(), it);
        if (dataLen > 0 && m_fileFd >= 0) {
            write(m_fileFd, m_buffer.data(), dataLen);
        }

        // Close file
        if (m_fileFd >= 0) {
            close(m_fileFd);
            m_fileFd = -1;
        }

        // Remove data + sentinel from buffer
        m_buffer.erase(m_buffer.begin(), it + sentinel.size());

        m_stage = SessionStage::HEADER;
        sendResponse("FILE_OK\n");
        Logger::info("FILE", "✅ File received: " + m_fileName);
    }
    else {
        // Write all buffered data to file (keep it flowing)
        if (!m_buffer.empty() && m_fileFd >= 0) {
            // Keep last few bytes in case sentinel is split across reads
            size_t keep = std::min(m_buffer.size(), sentinel.size());
            size_t writeLen = m_buffer.size() - keep;

            if (writeLen > 0) {
                write(m_fileFd, m_buffer.data(), writeLen);
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + writeLen);
            }
        }
    }
}

} // namespace bybridge
