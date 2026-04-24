#pragma once
/*
 * TitanShare Daemon — Notification Bridge
 * Monitors D-Bus for desktop notifications and forwards them to
 * connected Android clients. Also receives notifications from Android
 * and displays them on the Linux desktop.
 */

#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace titanshare {

struct Notification {
    std::string appName;
    std::string summary;
    std::string body;
    std::string icon;
};

class NotificationBridge {
public:
    using NotificationCallback = std::function<void(const Notification&)>;

    NotificationBridge();
    ~NotificationBridge();

    // Set callback for when a desktop notification is intercepted
    void setCallback(NotificationCallback cb);

    // Start monitoring D-Bus for notifications (runs in background thread)
    void startMonitoring();

    // Stop monitoring
    void stopMonitoring();

    // Show a notification on the Linux desktop (received from Android)
    static void showDesktopNotification(const std::string& title, const std::string& body,
                                        const std::string& appName = "TitanShare");

private:
    void monitorLoop();

    NotificationCallback m_callback;
    std::atomic<bool> m_running{false};
    std::thread m_monitorThread;
};

} // namespace titanshare
