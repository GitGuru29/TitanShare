#include "server/tcp_server.hpp"
#include "server/client_session.hpp"
#include "auth/session_manager.hpp"
#include "commands/command_dispatcher.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace bybridge {

TcpServer::TcpServer(uint16_t port,
                     std::shared_ptr<SessionManager> sessionMgr,
                     std::shared_ptr<CommandDispatcher> dispatcher)
    : m_port(port)
    , m_sessionMgr(std::move(sessionMgr))
    , m_dispatcher(std::move(dispatcher))
{}

TcpServer::~TcpServer() {
    stop();
    if (m_serverFd >= 0) close(m_serverFd);
    if (m_epollFd >= 0) close(m_epollFd);
}

int TcpServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void TcpServer::run() {
    // Create socket
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) {
        Logger::error("TCP", "socket() failed: " + std::string(strerror(errno)));
        return;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlocking(m_serverFd);

    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_serverFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::error("TCP", "bind() failed on port " + std::to_string(m_port) + ": " + strerror(errno));
        return;
    }

    if (listen(m_serverFd, config::BACKLOG) < 0) {
        Logger::error("TCP", "listen() failed: " + std::string(strerror(errno)));
        return;
    }

    // Create epoll
    m_epollFd = epoll_create1(0);
    if (m_epollFd < 0) {
        Logger::error("TCP", "epoll_create1() failed: " + std::string(strerror(errno)));
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = m_serverFd;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_serverFd, &ev);

    m_running = true;
    Logger::info("TCP", "🚀 Daemon listening on port " + std::to_string(m_port));

    // Event loop
    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (m_running) {
        int nfds = epoll_wait(m_epollFd, events, MAX_EVENTS, 1000); // 1s timeout
        if (nfds < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            Logger::error("TCP", "epoll_wait() failed: " + std::string(strerror(errno)));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == m_serverFd) {
                acceptConnection();
            } else {
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    removeClient(events[i].data.fd);
                } else if (events[i].events & EPOLLIN) {
                    handleClientData(events[i].data.fd);
                }
            }
        }
    }

    Logger::info("TCP", "Server loop ended");
}

void TcpServer::stop() {
    m_running = false;
}

void TcpServer::acceptConnection() {
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    int clientFd = accept(m_serverFd, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Logger::error("TCP", "accept() failed: " + std::string(strerror(errno)));
        }
        return;
    }

    if (m_clients.size() >= static_cast<size_t>(config::MAX_CLIENTS)) {
        Logger::warn("TCP", "Max clients reached, rejecting connection");
        close(clientFd);
        return;
    }

    setNonBlocking(clientFd);

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = clientFd;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, clientFd, &ev);

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));

    m_clients[clientFd] = std::make_unique<ClientSession>(
        clientFd, std::string(ipStr), m_sessionMgr, m_dispatcher);

    Logger::info("TCP", "🔗 Client connected: " + std::string(ipStr) +
                 " (fd=" + std::to_string(clientFd) + ")");
}

void TcpServer::handleClientData(int clientFd) {
    auto it = m_clients.find(clientFd);
    if (it == m_clients.end()) return;

    char buf[config::READ_BUFFER_SIZE];
    while (true) {
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if (n > 0) {
            it->second->onData(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            // Client disconnected
            removeClient(clientFd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            Logger::error("TCP", "recv() error on fd=" + std::to_string(clientFd));
            removeClient(clientFd);
            return;
        }
    }
}

void TcpServer::removeClient(int clientFd) {
    auto it = m_clients.find(clientFd);
    if (it != m_clients.end()) {
        Logger::info("TCP", "❌ Client disconnected (fd=" + std::to_string(clientFd) + ")");
        m_clients.erase(it);
    }
    epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
    close(clientFd);
}

} // namespace bybridge
