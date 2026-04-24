# ByBridge Daemon

Native C++ daemon for Arch Linux that bridges your Android phone with your Linux desktop. A complete rewrite of the original Node.js ByBridge project.

## Features

- 📁 **File Sharing** — Send files from Android to Linux over WiFi
- 🔊 **Volume Control** — Adjust system volume from your phone
- ⚡ **Power Control** — Shutdown, reboot, sleep from Android
- 🔒 **Lock/Unlock** — Lock screen and biometric unlock via phone fingerprint
- 🖱️ **Remote Input** — Virtual touchpad and keyboard from Android
- 📊 **System Monitor** — CPU, RAM, Temperature, Storage, Battery stats on phone
- 🔔 **Notification Sync** — Forward desktop notifications to Android
- 📱 **QR Pairing** — Secure session-based pairing via QR code scan
- 🪞 **Screen Mirror** — Stream desktop to Android (GStreamer)

## Requirements

**Arch Linux packages:**

```bash
sudo pacman -S base-devel cmake nlohmann-json qrencode libpng dbus
```

**Permissions:**

```bash
# Virtual input (mouse/keyboard emulation)
sudo chmod 0666 /dev/uinput
# OR add user to input group
sudo usermod -aG input $USER
```

## Build

```bash
git clone https://github.com/yourusername/bybridge-daemon.git
cd bybridge-daemon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Install & Run

```bash
# Install binary + service
sudo make install
sudo cp ../systemd/bybridge.service /etc/systemd/system/
sudo systemctl daemon-reload

# Start
sudo systemctl enable --now bybridge

# Check logs
journalctl -u bybridge -f
```

## Manual Run (Development)

```bash
./build/bybridge-daemon
```

The QR code is saved to `/var/lib/bybridge/session_qr.png` — scan it with the ByBridge Android app.

## Protocol

Uses a TCP-based protocol on port 9999, compatible with the ByBridge Android app:

1. **Auth**: Client sends session key → `AUTH_OK` / `AUTH_FAIL`
2. **Commands**: `CMD:<command>\n` → `CMD_OK\n` / `CMD_FAIL\n` / JSON response
3. **Files**: `FILE_START:<name>:<size>\n` → binary data → `FILE_END\n`

### Supported Commands

| Command | Action |
|---|---|
| `shutdown` | `systemctl poweroff` |
| `reboot` | `systemctl reboot` |
| `sleep` | `systemctl suspend` |
| `lock` | `loginctl lock-session` |
| `unlock` | Find seat0 session, unlock + activate + wake |
| `volume_up` | `pactl +5%` |
| `volume_down` | `pactl -5%` |
| `mute` | `pactl toggle mute` |
| `get_info` | Returns system info JSON |
| `MOUSE_MOVE:dx:dy` | Move virtual mouse |
| `MOUSE_CLICK:LEFT/RIGHT` | Click |
| `KEY_TYPE:text` | Type text |
| `KEY_PRESS:ENTER` | Press special key |

## Project Structure

```
bybridge-daemon/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # Entry point, signal handling
│   ├── config.hpp            # All constants and defaults
│   ├── server/               # TCP server + client sessions
│   ├── auth/                 # Session key management
│   ├── commands/             # Power, volume, lock, system info
│   ├── file_transfer/        # File receive protocol
│   ├── input/                # Virtual mouse + keyboard (uinput)
│   ├── notifications/        # D-Bus notification bridge
│   ├── qr/                   # QR code PNG generator
│   └── utils/                # Logger, network, process exec
├── systemd/
│   └── bybridge.service      # Systemd unit file
└── config/
    └── bybridge.conf         # Runtime configuration
```

## License

MIT
