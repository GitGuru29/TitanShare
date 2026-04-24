#pragma once
/*
 * ByBridge Daemon — Configuration Constants
 * All compile-time defaults. Can be overridden via bybridge.conf at runtime.
 */

#include <string>
#include <cstdint>

namespace bybridge {
namespace config {

// ─── Network ────────────────────────────────────────────────────────
constexpr uint16_t TCP_PORT           = 9999;   // Main daemon TCP port
constexpr uint16_t MIRROR_PORT        = 5001;   // GStreamer UDP mirror port
constexpr uint16_t WS_PORT            = 7001;   // WebSocket bridge port
constexpr int      MAX_CLIENTS        = 64;     // Max simultaneous TCP clients
constexpr int      BACKLOG            = 16;     // TCP listen backlog

// ─── Session / Auth ─────────────────────────────────────────────────
constexpr int      SESSION_KEY_BYTES  = 16;     // 16 bytes = 32 hex chars
constexpr int      MAX_RECENT_KEYS    = 5;      // Rolling window of valid keys
constexpr int      QR_REFRESH_SECS    = 5;      // QR regeneration interval

// ─── Paths (defaults, overridable via config file) ──────────────────
inline const std::string DATA_DIR            = "/var/lib/bybridge";
inline const std::string RECEIVED_FILES_DIR  = DATA_DIR + "/received_files";
inline const std::string QR_IMAGE_PATH       = DATA_DIR + "/session_qr.png";
inline const std::string QR_JSON_PATH        = DATA_DIR + "/session_qr.json";
inline const std::string SESSION_FILE_PATH   = DATA_DIR + "/last_session.json";
inline const std::string CONFIG_FILE_PATH    = "/etc/bybridge/bybridge.conf";

// ─── System Info Sources ────────────────────────────────────────────
inline const std::string PROC_STAT           = "/proc/stat";
inline const std::string PROC_MEMINFO        = "/proc/meminfo";
inline const std::string SYS_VENDOR          = "/sys/class/dmi/id/sys_vendor";
inline const std::string PRODUCT_NAME        = "/sys/class/dmi/id/product_name";
inline const std::string THERMAL_BASE        = "/sys/class/thermal";
inline const std::string POWER_SUPPLY_BASE   = "/sys/class/power_supply";

// ─── Input Device ───────────────────────────────────────────────────
inline const std::string UINPUT_PATH         = "/dev/uinput";
inline const std::string MOUSE_DEVICE_NAME   = "ByBridge Mouse";
inline const std::string KEYBOARD_DEVICE_NAME = "ByBridge Keyboard";

// ─── Buffer Sizes ───────────────────────────────────────────────────
constexpr size_t   READ_BUFFER_SIZE   = 65536;  // 64KB per-client read buffer
constexpr size_t   FILE_WRITE_CHUNK   = 16384;  // 16KB file write chunks

// ─── Daemon Info ────────────────────────────────────────────────────
inline const std::string DAEMON_NAME    = "bybridge-daemon";
inline const std::string DAEMON_VERSION = "2.0.0";

} // namespace config
} // namespace bybridge
