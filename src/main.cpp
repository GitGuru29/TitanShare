/*
 * TitanShare Daemon v2.0 — Main Entry Point
 *
 * Native C++ daemon for Arch Linux.
 * Replaces the original Node.js daemon.js + input_driver.c
 *
 * Protocol-compatible with the TitanShare Android app.
 */

#include "config.hpp"
#include "utils/logger.hpp"
#include "utils/network.hpp"
#include "server/tcp_server.hpp"
#include "auth/session_manager.hpp"
#include "commands/command_dispatcher.hpp"
#include "qr/qr_generator.hpp"
#include "notifications/notification_bridge.hpp"

#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <iostream>

#ifdef __has_include
#if __has_include(<systemd/sd-daemon.h>)
#include <systemd/sd-daemon.h>
#define HAS_SYSTEMD 1
#endif
#endif

namespace {
    std::atomic<bool> g_running{true};
    titanshare::TcpServer* g_server = nullptr;

    void signalHandler(int sig) {
        titanshare::Logger::info("MAIN", "Received signal " + std::to_string(sig) + ", shutting down...");
        g_running = false;
        if (g_server) g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    using namespace titanshare;

    // ─── Initialize Logger ────────────────────────────────────
    Logger::init(LogLevel::INFO);
    Logger::info("MAIN", "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Logger::info("MAIN", " TitanShare Daemon v" + config::DAEMON_VERSION);
    Logger::info("MAIN", " Native C++ • Arch Linux");
    Logger::info("MAIN", "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    // ─── Signal Handling ──────────────────────────────────────
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe

    // ─── Create Data Directories ──────────────────────────────
    try {
        std::filesystem::create_directories(config::DATA_DIR);
        std::filesystem::create_directories(config::RECEIVED_FILES_DIR);
    } catch (const std::exception& e) {
        Logger::error("MAIN", "Failed to create data dirs: " + std::string(e.what()));
        Logger::info("MAIN", "Trying HOME-based fallback...");
    }

    // ─── Initialize Session Manager ───────────────────────────
    auto sessionMgr = std::make_shared<SessionManager>();
    sessionMgr->generateSession(true);

    // ─── Generate Initial QR Code ─────────────────────────────
    QrGenerator::generate(sessionMgr->toJson(), config::QR_IMAGE_PATH);
    Logger::info("MAIN", "📱 QR Code: " + config::QR_IMAGE_PATH);

    // ─── QR Refresh Timer (background thread) ─────────────────
    std::thread qrThread([&sessionMgr]() {
        while (g_running) {
            std::this_thread::sleep_for(
                std::chrono::seconds(config::QR_REFRESH_SECS));
            if (!g_running) break;

            sessionMgr->generateSession(true);
            QrGenerator::generate(sessionMgr->toJson(), config::QR_IMAGE_PATH);
        }
    });
    qrThread.detach();

    // ─── Initialize Command Dispatcher ────────────────────────
    auto dispatcher = std::make_shared<CommandDispatcher>();
    dispatcher->init();

    // ─── Initialize Notification Bridge ───────────────────────
    NotificationBridge notifBridge;
    notifBridge.setCallback([](const Notification& n) {
        Logger::info("NOTIF", "🔔 " + n.appName + ": " + n.summary);
        // TODO: Forward to connected Android clients
    });
    notifBridge.startMonitoring();

    // ─── Print Status ─────────────────────────────────────────
    std::string localIp = Network::getLocalIp();
    Logger::info("MAIN", "🌐 Local IP: " + localIp);
    Logger::info("MAIN", "🔌 TCP Port: " + std::to_string(config::TCP_PORT));
    Logger::info("MAIN", "📂 Files: " + config::RECEIVED_FILES_DIR);
    Logger::info("MAIN", "🎯 Mirror Port: " + std::to_string(config::MIRROR_PORT));

    // ─── Notify systemd we're ready ───────────────────────────
#ifdef HAS_SYSTEMD
    sd_notify(0, "READY=1");
    Logger::info("MAIN", "✅ Notified systemd: READY");
#endif

    // ─── Start TCP Server (blocks until shutdown) ─────────────
    TcpServer server(config::TCP_PORT, sessionMgr, dispatcher);
    g_server = &server;

    Logger::info("MAIN", "🚀 Daemon is running. Waiting for connections...");
    server.run(); // Blocking event loop

    // ─── Cleanup ──────────────────────────────────────────────
    Logger::info("MAIN", "Shutting down...");
    notifBridge.stopMonitoring();
    g_server = nullptr;

#ifdef HAS_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    Logger::info("MAIN", "👋 Goodbye.");
    return 0;
}
