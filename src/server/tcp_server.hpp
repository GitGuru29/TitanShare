#pragma once
/*
 * TitanShare Daemon — TCP Server
 * Epoll-based asynchronous TCP server supporting multiple clients.
 */

#include <functional>
#include <unordered_map>
#include <memory>
#include <atomic>

namespace titanshare {

class ClientSession;
class SessionManager;
class CommandDispatcher;

class TcpServer {
public:
    TcpServer(uint16_t port,
              std::shared_ptr<SessionManager> sessionMgr,
              std::shared_ptr<CommandDispatcher> dispatcher);
    ~TcpServer();

    // Start listening and enter the event loop (blocking)
    void run();

    // Signal the server to stop
    void stop();

private:
    void acceptConnection();
    void handleClientData(int clientFd);
    void removeClient(int clientFd);
    int setNonBlocking(int fd);

    uint16_t m_port;
    int m_serverFd = -1;
    int m_epollFd = -1;
    std::atomic<bool> m_running{false};

    std::shared_ptr<SessionManager> m_sessionMgr;
    std::shared_ptr<CommandDispatcher> m_dispatcher;
    std::unordered_map<int, std::unique_ptr<ClientSession>> m_clients;
};

} // namespace titanshare
