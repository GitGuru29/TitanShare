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
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <poll.h>

namespace titanshare {

static void publishTransferState(bool active, bool isSending, const std::string& filename, float progress) {
    nlohmann::json j;
    j["active"] = active;
    j["is_sending"] = isSending;
    j["filename"] = filename;
    j["progress"] = progress;
    
    std::ofstream ofs("/run/titanshare/transfer.json");
    if (ofs.is_open()) {
        ofs << j.dump();
        ofs.close();
    }
}

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

// ─── O(1) buffer helpers ─────────────────────────────────────────────────────

void ClientSession::consumeBuffer(size_t n) {
    m_bufferOffset += n;
    // Compact when we've consumed more than 256 KB of dead space
    // to avoid unbounded memory growth.
    if (m_bufferOffset > 262144) {
        compactBuffer();
    }
}

void ClientSession::compactBuffer() {
    if (m_bufferOffset == 0) return;
    size_t remaining = m_buffer.size() - m_bufferOffset;
    if (remaining > 0) {
        std::memmove(m_buffer.data(), m_buffer.data() + m_bufferOffset, remaining);
    }
    m_buffer.resize(remaining);
    m_bufferOffset = 0;
}

// ─── Data entry point ─────────────────────────────────────────────────────────

void ClientSession::onData(const char* data, size_t len) {
    m_buffer.insert(m_buffer.end(), data, data + len);
    processBuffer();
}

void ClientSession::sendResponse(const std::string& response) {
    send(m_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
}

// ─── State machine ────────────────────────────────────────────────────────────

void ClientSession::processBuffer() {
    while (bufSize() > 0) {
        if (m_stage == SessionStage::AUTH) {
            // Find newline in the readable portion
            const char* start = bufData();
            const char* nl = static_cast<const char*>(
                std::memchr(start, '\n', bufSize()));
            if (!nl) break;

            size_t lineLen = nl - start;
            std::string line(start, lineLen);
            consumeBuffer(lineLen + 1); // consume line + newline

            if (!line.empty() && line.back() == '\r') line.pop_back();
            handleAuth(line);
        }
        else if (m_stage == SessionStage::HEADER) {
            const char* start = bufData();
            const char* nl = static_cast<const char*>(
                std::memchr(start, '\n', bufSize()));
            if (!nl) break;

            size_t lineLen = nl - start;
            std::string line(start, lineLen);
            consumeBuffer(lineLen + 1);

            if (!line.empty() && line.back() == '\r') line.pop_back();
            handleHeader(line);
        }
        else if (m_stage == SessionStage::DATA) {
            handleFileData();
            break; // handleFileData drains what it can; re-enter on next recv
        }
    }
}

// ─── Auth ─────────────────────────────────────────────────────────────────────

void ClientSession::handleAuth(const std::string& line) {
    std::string pin = line;

    // Strip optional "AUTH:" prefix
    if (pin.size() > 5 && pin.substr(0, 5) == "AUTH:") {
        pin = pin.substr(5);
    }

    // Trim whitespace
    while (!pin.empty() && (pin.back() == ' ' || pin.back() == '\t' || pin.back() == '\r'))
        pin.pop_back();
    while (!pin.empty() && (pin.front() == ' ' || pin.front() == '\t'))
        pin.erase(pin.begin());

    if (m_sessionMgr->validateKey(pin)) {
        m_sessionKey = pin;
        sendResponse("AUTH_OK\n");
        m_stage = SessionStage::HEADER;
        Logger::info("AUTH", "✅ Device paired via PIN: " + m_remoteIp);
    } else {
        sendResponse("AUTH_FAIL\n");
        Logger::warn("AUTH", "❌ Wrong PIN from: " + m_remoteIp + " (got: " + pin + ")");
    }
}

// ─── Header ───────────────────────────────────────────────────────────────────

void ClientSession::handleHeader(const std::string& line) {
    if (line.size() >= 4 && line.substr(0, 4) == "CMD:") {
        std::string cmd = line.substr(4);
        while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());

        // ── Linux → Android: list files available to push ──────────────────
        if (cmd == "push_file_list") {
            pushFileList();
            return;
        }

        // ── Linux → Android: stream a specific file to Android ──────────────
        if (cmd.size() > 10 && cmd.substr(0, 10) == "push_file:") {
            pushFile(cmd.substr(10));
            return;
        }

        Logger::info("CMD", "Command: " + cmd + " from " + m_remoteIp);
        std::string response = m_dispatcher->dispatch(cmd, m_fd);
        if (!response.empty()) {
            sendResponse(response);
        }
    }
    else if (line.size() >= 11 && line.substr(0, 11) == "FILE_START:") {
        // Parse FILE_START:<filename>:<size>
        // filename itself may contain colons (e.g. timestamps), so find the LAST colon
        // for size, everything before that is the filename.
        auto lastColon = line.rfind(':');
        if (lastColon == std::string::npos || lastColon <= 11) {
            sendResponse("CMD_FAIL\n");
            return;
        }

        m_fileName = line.substr(11, lastColon - 11);
        std::replace(m_fileName.begin(), m_fileName.end(), '/', '_');
        std::replace(m_fileName.begin(), m_fileName.end(), '\\', '_');

        try {
            m_expectedBytes = std::stoull(line.substr(lastColon + 1));
        } catch (...) {
            sendResponse("CMD_FAIL\n");
            return;
        }

        if (m_expectedBytes == 0) {
            sendResponse("CMD_FAIL\n");
            Logger::error("FILE", "Received FILE_START with size=0, rejecting");
            return;
        }

        std::filesystem::create_directories(config::RECEIVED_FILES_DIR);

        std::string filePath = config::RECEIVED_FILES_DIR + "/" + m_fileName;
        m_fileFd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (m_fileFd < 0) {
            Logger::error("FILE", "Failed to create file: " + filePath +
                          " (" + strerror(errno) + ")");
            sendResponse("CMD_FAIL\n");
            return;
        }

        m_receivedBytes = 0;
        m_stage = SessionStage::DATA;
        sendResponse("READY_FOR_FILE\n");
        Logger::info("FILE", "📥 Receiving: " + m_fileName +
                     " (" + std::to_string(m_expectedBytes) + " bytes)");
    }
    else {
        sendResponse("UNKNOWN_REQUEST\n");
    }
}

// ─── File Data ────────────────────────────────────────────────────────────────

void ClientSession::handleFileData() {
    if (bufSize() == 0 || m_fileFd < 0) return;

    // How many bytes we still need from the stream
    size_t remaining = m_expectedBytes - m_receivedBytes;
    size_t toWrite   = std::min(bufSize(), remaining);

    if (toWrite > 0) {
        // write() directly from the buffer read-head — no extra copy
        ssize_t written = write(m_fileFd, bufData(), toWrite);
        if (written < 0) {
            Logger::error("FILE", "write() failed: " + std::string(strerror(errno)));
            close(m_fileFd);
            m_fileFd = -1;
            sendResponse("CMD_FAIL\n");
            m_stage = SessionStage::HEADER;
            // Clear the whole buffer — stream is corrupt
            m_buffer.clear();
            m_bufferOffset = 0;
            return;
        }
        m_receivedBytes += static_cast<size_t>(written);
        consumeBuffer(static_cast<size_t>(written)); // O(1) advance
    }

    // Check completion
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastProgressUpdate).count() > 100 || m_receivedBytes == m_expectedBytes) {
        publishTransferState(true, false, m_fileName, static_cast<float>(m_receivedBytes) / static_cast<float>(m_expectedBytes));
        m_lastProgressUpdate = now;
    }

    if (m_receivedBytes >= m_expectedBytes) {
        publishTransferState(false, false, "", 1.0f);
        close(m_fileFd);
        m_fileFd = -1;

        // Drain optional legacy "FILE_END\n" sentinel that old Android client sends
        const std::string sentinel = "FILE_END\n";
        if (bufSize() >= sentinel.size() &&
            std::memcmp(bufData(), sentinel.data(), sentinel.size()) == 0) {
            consumeBuffer(sentinel.size());
            Logger::debug("FILE", "Drained legacy FILE_END sentinel");
        }

        m_receivedBytes = 0;
        m_stage = SessionStage::HEADER;

        sendResponse("FILE_OK\n");
        Logger::info("FILE", "✅ File received: " + m_fileName +
                     " (" + std::to_string(m_expectedBytes) + " bytes)");

        // Compact now so the next command parse starts on a clean buffer
        compactBuffer();
    }
}

// ─── Linux → Android: File Push ──────────────────────────────────────────────

void ClientSession::pushFileList() {
    namespace fs = std::filesystem;
    using json   = nlohmann::json;

    // Create the drop folder if it doesn't exist yet
    std::error_code ec;
    fs::create_directories(config::SEND_TO_ANDROID_DIR, ec);

    json files = json::array();
    if (fs::exists(config::SEND_TO_ANDROID_DIR)) {
        for (auto& entry : fs::directory_iterator(config::SEND_TO_ANDROID_DIR, ec)) {
            if (!entry.is_regular_file()) continue;
            json f;
            f["name"] = entry.path().filename().string();
            f["size"] = entry.file_size();
            files.push_back(f);
        }
    }

    json response;
    response["type"]  = "push_file_list";
    response["files"] = files;
    sendResponse(response.dump() + "\n");

    Logger::info("PUSH", "📋 File list sent (" + std::to_string(files.size()) + " files)");
}

void ClientSession::pushFile(const std::string& filename) {
    namespace fs = std::filesystem;

    // Sanitize: strip any path traversal
    std::string safe = filename;
    auto lastSlash = safe.find_last_of("/\\");
    if (lastSlash != std::string::npos) safe = safe.substr(lastSlash + 1);

    std::string filePath = config::SEND_TO_ANDROID_DIR + "/" + safe;

    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
        sendResponse("PUSH_ERROR:file_not_found\n");
        Logger::warn("PUSH", "❌ Requested file not found: " + safe);
        return;
    }

    uint64_t fileSize = fs::file_size(filePath);
    if (fileSize == 0) {
        sendResponse("PUSH_ERROR:empty_file\n");
        return;
    }

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        sendResponse("PUSH_ERROR:cannot_open\n");
        Logger::error("PUSH", "❌ Cannot open: " + filePath);
        return;
    }

    // Send header: FILE_PUSH:<name>:<size>\n
    std::string header = "FILE_PUSH:" + safe + ":" + std::to_string(fileSize) + "\n";
    sendResponse(header);

    Logger::info("PUSH", "📤 Pushing: " + safe + " (" + std::to_string(fileSize) + " bytes)");

    // Stream the raw bytes
    constexpr size_t CHUNK = 131072; // 128 KB
    std::vector<char> buf(CHUNK);
    uint64_t sent = 0;
    auto lastUpdate = std::chrono::steady_clock::now();

    while (sent < fileSize) {
        ifs.read(buf.data(), static_cast<std::streamsize>(std::min(CHUNK, static_cast<size_t>(fileSize - sent))));
        std::streamsize n = ifs.gcount();
        if (n <= 0) break;
        
        ssize_t totalWritten = 0;
        while (totalWritten < n) {
            ssize_t written = ::send(m_fd, buf.data() + totalWritten, static_cast<size_t>(n - totalWritten), MSG_NOSIGNAL);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    struct pollfd pfd{};
                    pfd.fd = m_fd;
                    pfd.events = POLLOUT;
                    poll(&pfd, 1, 10000); // Wait up to 10s for buffer to open up
                    continue;
                }
                publishTransferState(false, true, "", 0.0f);
                Logger::error("PUSH", "❌ send() failed: " + std::string(strerror(errno)));
                return;
            }
            totalWritten += written;
        }
        sent += static_cast<uint64_t>(totalWritten);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() > 100 || sent == fileSize) {
            publishTransferState(true, true, safe, static_cast<float>(sent) / static_cast<float>(fileSize));
            lastUpdate = now;
        }
    }

    publishTransferState(false, true, "", 1.0f);
    Logger::info("PUSH", "✅ Push complete: " + safe);
}

} // namespace titanshare
