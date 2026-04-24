/*
 * TitanShare Daemon v2.0 — Main Entry Point
 *
 * Native C++ daemon for Arch Linux.
 * Uses mDNS/Avahi to advertise on LAN — Android app auto-discovers.
 * Pairing is confirmed with a 6-digit PIN displayed in the GUI.
 */

#include "config.hpp"
#include "utils/logger.hpp"
#include "utils/network.hpp"
#include "server/tcp_server.hpp"
#include "auth/session_manager.hpp"
#include "commands/command_dispatcher.hpp"
#include "discovery/mdns_advertiser.hpp"
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
    Logger::info("MAIN", " Native C++ • Arch Linux • LAN Discovery");
    Logger::info("MAIN", "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    // ─── Signal Handling ──────────────────────────────────────
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    // ─── Create Data Directories ──────────────────────────────
    try {
        std::filesystem::create_directories(config::DATA_DIR);
        std::filesystem::create_directories(config::RECEIVED_FILES_DIR);
    } catch (const std::exception& e) {
        Logger::error("MAIN", "Failed to create data dirs: " + std::string(e.what()));
    }

    // ─── Initialize Session Manager (generates PIN) ───────────
    auto sessionMgr = std::make_shared<SessionManager>();
    sessionMgr->generateSession(true);   // reuse persisted PIN if fresh start

    std::string localIp = Network::getLocalIp();
    Logger::info("MAIN", "🌐 Local IP  : " + localIp);
    Logger::info("MAIN", "🔌 TCP Port  : " + std::to_string(config::TCP_PORT));
    Logger::info("MAIN", "🔑 Pairing PIN: " + sessionMgr->currentKey());

    // ─── mDNS Advertiser ──────────────────────────────────────
    MdnsAdvertiser mdns;
    mdns.start(sessionMgr->currentKey(), config::TCP_PORT);

    // ─── PIN Refresh Timer (background thread) ────────────────
    std::thread pinRefreshThread([&sessionMgr, &mdns]() {
        while (g_running) {
            std::this_thread::sleep_for(
                std::chrono::seconds(config::PAIRING_PIN_SECS));
            if (!g_running) break;

            sessionMgr->generateSession(false);   // fresh PIN
            mdns.updatePin(sessionMgr->currentKey());

            Logger::info("MAIN", "🔄 PIN refreshed: " + sessionMgr->currentKey());
        }
    });
    pinRefreshThread.detach();

    // ─── Initialize Command Dispatcher ────────────────────────
    auto dispatcher = std::make_shared<CommandDispatcher>();
    dispatcher->init();

    // ─── Initialize Notification Bridge ───────────────────────
    NotificationBridge notifBridge;
    notifBridge.setCallback([](const Notification& n) {
        Logger::info("NOTIF", "🔔 " + n.appName + ": " + n.summary);
    });
    notifBridge.startMonitoring();

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

    Logger::info("MAIN", "🚀 Daemon running — waiting for Android to connect on LAN...");
    server.run(); // Blocking event loop

    // ─── Cleanup ──────────────────────────────────────────────
    Logger::info("MAIN", "Shutting down...");
    mdns.stop();
    notifBridge.stopMonitoring();
    g_server = nullptr;

#ifdef HAS_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    Logger::info("MAIN", "👋 Goodbye.");
    return 0;
}
