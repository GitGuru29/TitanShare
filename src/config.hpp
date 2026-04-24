#pragma once
/*
 * TitanShare Daemon — Configuration Constants
 * All compile-time defaults. Can be overridden via titanshare.conf at runtime.
 */

#include <string>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>

namespace titanshare {
namespace config {

// ─── Network ────────────────────────────────────────────────────────
constexpr uint16_t TCP_PORT           = 9999;   // Main daemon TCP port
constexpr uint16_t MIRROR_PORT        = 5001;   // GStreamer UDP mirror port
constexpr uint16_t WS_PORT            = 7001;   // WebSocket bridge port
constexpr int      MAX_CLIENTS        = 64;     // Max simultaneous TCP clients
constexpr int      BACKLOG            = 16;     // TCP listen backlog

// ─── mDNS / Discovery ───────────────────────────────────────────────
inline const std::string MDNS_SERVICE_TYPE = "_titanshare._tcp";

// ─── Pairing PIN ────────────────────────────────────────────────────
constexpr int      PAIRING_PIN_SECS   = 300;    // PIN refresh interval (5 min)
constexpr int      PAIRING_PIN_DIGITS = 6;      // PIN length shown on screen
constexpr int      MAX_RECENT_PINS    = 3;      // Rolling window of valid PINs

// ─── Paths (runtime-resolved) ───────────────────────────────────────
// Uses /var/lib/titanshare when root, ~/.local/share/titanshare otherwise
inline std::string getDataDir() {
    if (getuid() == 0) {
        return "/var/lib/titanshare";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/share/titanshare";
    }
    return "/tmp/titanshare";
}

inline std::string DATA_DIR            = getDataDir();
inline std::string RECEIVED_FILES_DIR  = DATA_DIR + "/received_files";
inline std::string SESSION_FILE_PATH   = DATA_DIR + "/last_session.json";
inline const std::string CONFIG_FILE_PATH    = "/etc/titanshare/titanshare.conf";

// IPC — daemon writes this; GUI QFileSystemWatcher reads it
inline const std::string PIN_IPC_PATH        = "/tmp/titanshare-pin.json";

// ─── System Info Sources ────────────────────────────────────────────
inline const std::string PROC_STAT           = "/proc/stat";
inline const std::string PROC_MEMINFO        = "/proc/meminfo";
inline const std::string SYS_VENDOR          = "/sys/class/dmi/id/sys_vendor";
inline const std::string PRODUCT_NAME        = "/sys/class/dmi/id/product_name";
inline const std::string THERMAL_BASE        = "/sys/class/thermal";
inline const std::string POWER_SUPPLY_BASE   = "/sys/class/power_supply";

// ─── Input Device ───────────────────────────────────────────────────
inline const std::string UINPUT_PATH         = "/dev/uinput";
inline const std::string MOUSE_DEVICE_NAME   = "TitanShare Mouse";
inline const std::string KEYBOARD_DEVICE_NAME = "TitanShare Keyboard";

// ─── Buffer Sizes ───────────────────────────────────────────────────
constexpr size_t   READ_BUFFER_SIZE   = 65536;  // 64KB per-client read buffer
constexpr size_t   FILE_WRITE_CHUNK   = 16384;  // 16KB file write chunks

// ─── Daemon Info ────────────────────────────────────────────────────
inline const std::string DAEMON_NAME    = "titanshare-daemon";
inline const std::string DAEMON_VERSION = "2.0.0";

} // namespace config
} // namespace titanshare
