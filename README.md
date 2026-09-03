# TitanShare Daemon

Native C++ daemon for Arch Linux that bridges your Android phone with your Linux desktop. A complete rewrite of the original Node.js TitanShare project.

## Features

- 📁 **File Sharing** — Bidirectional file transfer with progress tracking
- 🔊 **Volume Control** — Adjust system volume from your phone
- ⚡ **Power Control** — Shutdown, reboot, sleep from Android
- 🔒 **Lock/Unlock** — Lock screen and biometric unlock via phone fingerprint
- 🖱️ **Remote Input** — Virtual touchpad and keyboard from Android
- 📊 **System Monitor** — CPU, RAM, Temperature, Storage, Battery stats on phone
- 🔔 **Notification Sync** — Forward desktop notifications to Android
- 📱 **mDNS Discovery** — Automatic LAN discovery with rotating 6-digit PIN pairing
- 🪞 **Screen Mirror** — Stream desktop to Android via GStreamer (length-prefixed JPEG over TCP)

## Requirements

**Arch Linux packages:**

```bash
sudo pacman -S base-devel cmake nlohmann-json qrencode libpng dbus \
    avahi gstreamer gst-plugins-base gst-plugins-good x11
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
git clone https://github.com/GitGuru29/TitanShare.git
cd titanshare-daemon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Install & Run

```bash
# Install binary + service
sudo make install
sudo cp ../systemd/titanshare.service /etc/systemd/system/
sudo systemctl daemon-reload

# Start
sudo systemctl enable --now titanshare

# Check logs
journalctl -u titanshare -f
```

## Manual Run (Development)

```bash
./build/titanshare-daemon
```

The daemon advertises itself via mDNS as `_titanshare._tcp` on the LAN. The Android app discovers it automatically and prompts for the 6-digit PIN displayed in the GUI or logs.

## Protocol

Uses a TCP-based protocol on port 9999, compatible with the TitanShare Android app:

1. **Auth**: Client sends PIN → `AUTH_OK` / `AUTH_FAIL`
2. **Commands**: `CMD:<command>\n` → `CMD_OK\n` / `CMD_FAIL\n` / JSON response
3. **Files**: `FILE_START:<name>:<size>\n` → binary data → `FILE_OK\n`
4. **Mirror**: `CMD:START_MIRROR` → JSON with IP/port → Android connects on port 5001

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
| `MOUSE_SCROLL:dy` | Scroll wheel |
| `MOUSE_DOWN:LEFT/RIGHT` | Mouse button down (drag) |
| `MOUSE_UP:LEFT/RIGHT` | Mouse button up (drag) |
| `KEY_TYPE:text` | Type text |
| `KEY_PRESS:ENTER` | Press special key |
| `START_MIRROR` | Start screen mirror receiver on port 5001 |
| `STOP_MIRROR` | Stop screen mirror receiver |
| `push_file_list` | List files in send_to_android folder (JSON) |
| `push_file:<name>` | Stream a file from Linux to Android |

## Screen Mirroring

The mirror feature uses GStreamer to receive length-prefixed JPEG frames from the Android device:

- **Wire format**: `[uint32 BE length][JPEG payload]` per frame over TCP port 5001
- **Pipeline**: `appsrc ! queue ! jpegdec ! videoconvert ! autovideosink`
- **Backpressure**: Max 8 frames queued, excess frames dropped
- **Frame guard**: Rejects frames larger than 24MB

No firewall or Docker configuration is needed — the Android device connects inbound to the daemon's listener on port 5001.

## IPC (Daemon ↔ GUI)

The daemon communicates with the Qt6 GUI via JSON files in a runtime directory. The path is resolved at startup in this order:

1. `/run/titanshare/` (systemd or root)
2. `$XDG_RUNTIME_DIR/titanshare/` (user session)
3. `~/.local/share/titanshare/` (fallback)

Files written:
- `titanshare-pin.json` — Current pairing PIN and host info
- `transfer.json` — Active file transfer state (progress, filename, direction)

## Project Structure

```
titanshare-daemon/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # Entry point, signal handling
│   ├── config.hpp            # All constants and defaults
│   ├── server/               # Epoll TCP server + client sessions
│   ├── auth/                 # PIN generation, validation, persistence
│   ├── commands/             # Power, volume, lock, system info, mirror
│   ├── file_transfer/        # File receive/send protocol
│   ├── input/                # Virtual mouse + keyboard (uinput)
│   ├── mirror/               # GStreamer screen mirror receiver
│   ├── notifications/        # D-Bus notification bridge
│   ├── discovery/            # mDNS/Avahi LAN advertisement
│   ├── qr/                   # QR code PNG generator (legacy)
│   └── utils/                # Logger, network, process exec
├── src_gui/
│   ├── main.cpp              # Qt6 GUI application
│   └── qml/                  # QML UI components
├── systemd/
│   └── titanshare.service    # Systemd unit file
└── config/
    └── titanshare.conf       # Runtime configuration
```

## License

MIT
