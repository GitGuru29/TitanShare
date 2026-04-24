#include "notifications/notification_bridge.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <dbus/dbus.h>
#include <cstring>

namespace titanshare {

NotificationBridge::NotificationBridge() = default;

NotificationBridge::~NotificationBridge() {
    stopMonitoring();
}

void NotificationBridge::setCallback(NotificationCallback cb) {
    m_callback = std::move(cb);
}

void NotificationBridge::startMonitoring() {
    if (m_running) return;
    m_running = true;
    m_monitorThread = std::thread(&NotificationBridge::monitorLoop, this);
    Logger::info("NOTIF", "🔔 Notification monitoring started");
}

void NotificationBridge::stopMonitoring() {
    m_running = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
}

void NotificationBridge::monitorLoop() {
    DBusError err;
    dbus_error_init(&err);

    // Connect to the session bus
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        Logger::error("NOTIF", "D-Bus connection error: " + std::string(err.message));
        dbus_error_free(&err);
        return;
    }
    if (!conn) {
        Logger::error("NOTIF", "D-Bus: null connection");
        return;
    }

    // Monitor for notification signals - use multiple approaches for compatibility
    // Method 1: Listen for signals from the notification daemon
    const char* signalRule = "type='signal',interface='org.freedesktop.Notifications'";
    dbus_bus_add_match(conn, signalRule, &err);
    dbus_connection_flush(conn);

    if (dbus_error_is_set(&err)) {
        Logger::warn("NOTIF", "D-Bus signal match failed: " + std::string(err.message));
        dbus_error_free(&err);
    }

    // Method 2: Also try method_call without eavesdrop
    dbus_error_init(&err);
    const char* methodRule = "type='method_call',interface='org.freedesktop.Notifications',member='Notify'";
    dbus_bus_add_match(conn, methodRule, &err);
    dbus_connection_flush(conn);

    if (dbus_error_is_set(&err)) {
        Logger::warn("NOTIF", "D-Bus method match note: " + std::string(err.message));
        dbus_error_free(&err);
    }

    Logger::info("NOTIF", "Listening for D-Bus notifications...");

    while (m_running) {
        // Non-blocking read with timeout
        dbus_connection_read_write(conn, 100); // 100ms timeout

        DBusMessage* msg = dbus_connection_pop_message(conn);
        if (!msg) continue;

        // Check if it's a Notify call
        if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")) {
            DBusMessageIter args;
            if (dbus_message_iter_init(msg, &args)) {
                Notification notif;

                // Arg 0: app_name (string)
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
                    const char* val = nullptr;
                    dbus_message_iter_get_basic(&args, &val);
                    if (val) notif.appName = val;
                    dbus_message_iter_next(&args);
                }

                // Arg 1: replaces_id (uint32) — skip
                dbus_message_iter_next(&args);

                // Arg 2: app_icon (string)
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
                    const char* val = nullptr;
                    dbus_message_iter_get_basic(&args, &val);
                    if (val) notif.icon = val;
                    dbus_message_iter_next(&args);
                }

                // Arg 3: summary (string)
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
                    const char* val = nullptr;
                    dbus_message_iter_get_basic(&args, &val);
                    if (val) notif.summary = val;
                    dbus_message_iter_next(&args);
                }

                // Arg 4: body (string)
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
                    const char* val = nullptr;
                    dbus_message_iter_get_basic(&args, &val);
                    if (val) notif.body = val;
                }

                if (m_callback && !notif.summary.empty()) {
                    m_callback(notif);
                }
            }
        }

        dbus_message_unref(msg);
    }

    dbus_connection_unref(conn);
    Logger::info("NOTIF", "Notification monitoring stopped");
}

void NotificationBridge::showDesktopNotification(const std::string& title,
                                                  const std::string& body,
                                                  const std::string& appName) {
    // Use notify-send for simplicity
    std::string cmd = "notify-send ";
    cmd += "-a '" + appName + "' ";
    cmd += "'" + title + "' ";
    cmd += "'" + body + "' 2>/dev/null || true";
    Process::execDetached(cmd);
}

} // namespace titanshare
