# TitanShare Communication Architecture - Quick Reference

## System Overview Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         Android App                              │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Network Stack (TCP Client)                               │ │
│  │  - Discovers daemon via mDNS or manual IP entry          │ │
│  │  - Connects to TCP 9999                                   │ │
│  │  - Sends PIN for auth                                     │ │
│  │  - Sends/receives commands and files                      │ │
│  └────────────────────────────────────────────────────────────┘ │
└──────────────────────────┬──────────────────────────────────────┘
                           │ TCP 9999
                           │ (plaintext + binary frames)
                           ↓
┌──────────────────────────────────────────────────────────────────┐
│                     Linux Daemon (C++)                            │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ TCP Server (epoll)                                      │ │
│  │  - Accepts connections  → ClientSession per client      │ │
│  │  - Auth stage: Validates PIN                            │ │
│  │  - Header stage: Parses CMD: or FILE_START:            │ │
│  │  - Data stage: Receives file binary data                │ │
│  │  - Routes commands → CommandDispatcher                  │ │
│  └────────────────────────────────────────────────────────┘ │
│                           ↓                                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ CommandDispatcher                                       │ │
│  │  Power → systemctl (shutdown/reboot/suspend)           │ │
│  │  Volume → pactl (PulseAudio control)                   │ │
│  │  Input → VirtualInput (uinput mouse/keyboard)          │ │
│  │  System → SystemInfo (CPU/RAM/temp/storage)            │ │
│  │  Mirror → START_MIRROR returns IP:UDP5001              │ │
│  │  Files → pushFileList() / pushFile()                   │ │
│  └─────────────────────────────────────────────────────────┘ │
│           ↓              ↓              ↓                      │
│        Power          Volume          Lock         Input      │
│        Control        Control         /Unlock      (uinput)   │
│      (systemctl)      (pactl)        (loginctl)              │
└──────────────────────────────────────────────────────────────────┘
           ↓                                          ↓
      System State                           Desktop Input Layer
      Changes                            (mouse/keyboard events)
```

## Protocol Sequence: File Upload (Android → Linux)

```
Android                              Linux Daemon
  │                                      │
  ├─ FILE_START:photo.jpg:2097152 ──────>│
  │                                   (parse header)
  │<────────────── READY_FOR_FILE ───────│
  │                                      │
  ├─ [2MB binary data] ──────────────────>│ write() to disk
  │                                   (chunked writes)
  │                                      │
  │<────────────── FILE_OK ──────────────│
  │                                   (back to HEADER stage)
  │
  └─ Progress visible in ~/run/titanshare/transfer.json
```

## Protocol Sequence: System Info Query

```
Android                              Linux Daemon
  │                                      │
  ├─ CMD: get_info ─────────────────────>│
  │                                   (parse command)
  │                                      │ /proc/stat
  │                                      │ /proc/meminfo
  │                                      │ /sys/class/thermal
  │                                      ↓
  │                                  Collect all metrics
  │                                      │
  │<─ JSON(cpu,ram,temp,storage,...) ────│
  │                                      │
  └─ Back to ready for next command
```

## Command Types & Response Patterns

### Pattern 1: Simple ACK/NACK
```
Command:  CMD: shutdown
Response: CMD_OK\n     or  CMD_FAIL\n
```

### Pattern 2: JSON Response
```
Command:  CMD: get_info
Response: {"type":"system_info","data":{...}}\n
```

### Pattern 3: Large Data (Files)
```
Command:  CMD: push_file_list
Response: {"type":"push_file_list","files":[...]}\n

Command:  CMD: push_file:document.pdf
Response: FILE_PUSH:document.pdf:1048576\n
          [1MB binary data]
```

### Pattern 4: Bi-directional Streaming
```
Command:  CMD: START_MIRROR
Response: {"type":"MIRROR_READY","ip":"192.168.1.x","port":5001}\n
          (Android then connects to UDP 5001)
```

### Pattern 5: Android -> Linux Screen Mirror (No ADB)
```
Command:  CMD: START_PHONE_MIRROR
Response: MIRROR_READY\n | MIRROR_FAIL\n | MIRROR_ALREADY_RUNNING\n
Then per encoded frame:
Client -> Daemon: MIRROR_FRAME:<bytes>\n
Daemon -> Client: MIRROR_FRAME_READY\n
Client -> Daemon: [exactly <bytes> bytes of H.264 Annex-B payload]
Daemon -> Client: MIRROR_FRAME_OK\n | MIRROR_FAIL\n
Stop:
Command:  CMD: STOP_PHONE_MIRROR
Response: CMD_OK\n
```

## Authentication Flow

```
1. Client connects to TCP 9999
   ↓
2. Daemon waits for PIN
   ↓
3. Client sends: 123456\n
   ↓
4. Daemon validates against rolling window:
   - Current PIN
   - Previous PIN
   - PIN before that
   ↓
5. Response:
   ✓ AUTH_OK\n   → Transition to HEADER stage
   ✗ AUTH_FAIL\n → Close or allow retries

Note: PIN auto-rotates every 5 minutes
      3 recent PINs always valid
      Current PIN shown as QR code in GUI
```

## File Transfer State Machine

```
┌─────────────────────────────────────────────────────────────┐
│                    ClientSession FSM                        │
└─────────────────────────────────────────────────────────────┘

    ┌──────┐
    │  NEW │  (client just connected)
    └───┬──┘
        │ onData(pin_chars)
        ↓
    ┌────────┐
    │  AUTH  │  Waiting for "123456\n"
    └───┬──┬─┘
        │ │ AUTH_OK
        │ │
        ├─────────────────────────────────────┐
        │                                    │
        ↓                                    ↓
    ┌────────┐                          ┌─────────┐
    │INVALID │ (wrong PIN, reject)      │ HEADER  │ Ready for CMD: or FILE_START:
    └────────┘                          └────┬────┘
    Close connection          │          │          │
                             │          │          │
           ┌─────────────────┘          │          └──────────────┐
           │                            │                         │
           ↓ (simple command)           │ (file start)            ↓
    [Reply immediately]                 ↓            Transition to DATA
    Back to HEADER              ┌──────────┐
                                │   DATA   │
                                └────┬─────┘
                                     │ (read binary_data)
                                     │ write() to disk
                                     │
                                     ↓
                                (got all bytes?)
                                     │ Yes
                                     ↓
                                FILE_OK response
                                Back to HEADER
```

## Daemon Start Sequence

```
main()
  ↓
1. Initialize Logger
  ↓
2. Set Signal Handlers (SIGINT, SIGTERM → graceful shutdown)
  ↓
3. Create data directories (~/.local/share/titanshare/)
  ↓
4. Initialize SessionManager → Generate 6-digit PIN
  ↓
5. Log local IP, TCP port, PIN
  ↓
6. Start MdnsAdvertiser (background thread)
      └─ Advertises _titanshare._tcp service
      └─ TXT record includes PIN=123456
  ↓
7. Start PIN Refresh Timer (background thread)
      └─ Every 5 minutes: generate new PIN, update mDNS
  ↓
8. Initialize CommandDispatcher
      └─ Initialize VirtualInput (create /dev/uinput devices)
      └─ Initialize SystemInfo (ready to collect metrics)
  ↓
9. Start NotificationBridge
      └─ Optional D-Bus listener
  ↓
10. Enter TcpServer::run() [BLOCKING, main thread]
        └─ Listen on TCP 9999
        └─ Epoll loop: accept, recv, dispatch, send
        └─ Exit on SIGINT/SIGTERM
```

## Port Map

```
Port 9999 (TCP)
├─ Protocol: Text commands + binary file frames
├─ Encryption: None (plaintext)
├─ Dual-stack IPv6/IPv4
└─ Usage: Main command/file channel

Port 5001 (UDP)
├─ Protocol: GStreamer H.264 RTP (planned)
├─ Encryption: None
└─ Usage: Screen mirroring (not yet implemented)

Phone mirror transport note:
- Android -> Linux mirror uses the existing TCP 9999 connection.
- H.264 frames are sent as framed binary payloads after MIRROR_FRAME:<bytes> headers.
- Daemon pipes received H.264 to local ffplay (`ffplay -f h264 -i -`).

Port 7001 (TCP)
├─ Protocol: WebSocket + JSON
└─ Usage: Web UI (reserved, not implemented yet)
```

## Key Implementation Details

### TCP Data Flow Example
```
Client sends: "MOUSE_MOVE:10:20\n"
              (verbatim ASCII string)

Daemon:
  1. recv() into 64KB buffer
  2. onData() called with chunk
  3. processBuffer() scans for '\n'
  4. handleHeader() calls dispatch()
  5. CommandDispatcher::dispatch() :
     - Recognizes "MOUSE_MOVE:10:20"
     - Calls handleInputCommand()
     - Parses dx=10, dy=20
     - Calls m_input->moveMouse(10, 20)
     - Returns "" (no response for mouse moves, for performance)
  6. sendResponse() not called (empty string)
  7. Buffer offset advanced, stays in HEADER stage
  8. Ready for next command

NOTE: Mouse moves intentionally have no response to reduce latency
```

### File Push (Linux → Android) Performance
```
File: report.pdf (1 MB)

1. Android requests: CMD: push_file:report.pdf
2. Daemon reads file into memory...
   NO - streams 128KB chunks via send()
3. For each chunk:
   - Read 128 KB from disk
   - send() → may need retry loop (EAGAIN handling)
   - Poll socket if buffer full
4. Publish progress to /run/titanshare/transfer.json every 100ms
5. Android GUI displays progress bar in real-time
```

## Memory Layout (Per Client)

```
ClientSession object: ~1 KB
  - int fd
  - string remote_ip
  - SessionStage m_stage
  - string m_sessionKey
  - vector<char> m_buffer (64 KB allocated, grows as needed)
  - size_t m_bufferOffset (tracks consumed bytes, O(1) advance)
  - File transfer state (filename, bytes received, etc.)
  
Total per 64 clients: ~4-5 MB + TCP stack overhead
```

## Error Handling Paths

```
Scenario: Wrong PIN
  → AUTH_FAIL\n sent
  → Session stays in AUTH stage
  → Client can retry or close

Scenario: Disk full during file write
  → write() returns -1
  → CMD_FAIL\n sent
  → Session back to HEADER stage
  → File partially written (caller responsible for cleanup)

Scenario: Client disconnect
  → recv() returns 0 (EOF)
  → removeClient() called
  → File descriptor closed
  → Session object destroyed

Scenario: Invalid command
  → dispatch() returns "CMD_UNKNOWN\n"
  → sendResponse() sends response
  → Session stays in HEADER stage

Scenario: Mirror player not running
  → MIRROR_FRAME sent before START_PHONE_MIRROR
  → Daemon responds MIRROR_NOT_RUNNING\n

Scenario: Mirror pipe/player failure
  → write() to ffplay pipe fails
  → Daemon responds MIRROR_FAIL\n
  → Mirror session closes and returns to HEADER stage
```

## Security Boundaries

```
✓ Filename sanitization (no "/../" path traversal)
✓ PIN validation (rolling window of 3)
✓ File size validation (> 0 bytes required)

✗ Plaintext protocol (no TLS)
✗ Listens on all interfaces (0.0.0.0 + ::)
✗ Power commands require root (requires privilege)
✗ Input injection (can control mouse/keyboard without limits)

→ Design assumes trusted local network only
```

## Testing Checklist

```
Protocol Level:
[ ] AUTH handshake success/failure
[ ] Command dispatch with various payloads
[ ] FILE_START header parsing (edge cases)
[ ] Binary file transfer (verify byte-for-byte)
[ ] Progress update frequency
[ ] Buffer consumption O(1) property
[ ] Large files (>100 MB)
[ ] Concurrent clients (stress test)

Application Level:
[ ] Power command execution
[ ] Volume control with/without PulseAudio
[ ] Lock/unlock on different display servers
[ ] Mouse/keyboard injection race conditions
[ ] System info JSON completeness
[ ] mDNS discovery reliability
[ ] PIN rotation mechanism
[ ] File list enumeration from drop folder

Integration:
[ ] Android app ↔ Daemon compatibility
[ ] GUI real-time progress display
[ ] IPC file system watcher
[ ] Signal handling (SIGINT, SIGTERM)
[ ] Systemd service startup/stop
[ ] Phone mirror handshake: START_PHONE_MIRROR / STOP_PHONE_MIRROR
[ ] Frame protocol: MIRROR_FRAME:<bytes> + binary payload + MIRROR_FRAME_OK
[ ] Verify ffplay receives and displays H.264 stream in real time
```

---

**Note:** This quick reference is derived from static code analysis (June 2026).  
Actual screen mirroring implementation status unknown - handler exists but GStreamer pipeline not visible in codebase.
