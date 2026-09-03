#pragma once
/*
 * TitanShare Daemon — Screen Mirror Receiver
 *
 * Receives a stream of length-prefixed JPEG frames over TCP and displays them
 * in a window using a GStreamer pipeline:
 *
 *     appsrc ! queue ! jpegdec ! videoconvert ! autovideosink
 *
 * Wire format (per frame over TCP):
 *     [uint32 BE : payload length][payload bytes (JPEG)]
 *
 * This class is deliberately decoupled from GStreamer types in its public
 * interface so the command dispatcher can start/stop it without pulling in
 * GStreamer headers.
 */

#include <atomic>
#include <memory>
#include <string>

namespace titanshare {

class MirrorReceiver {
public:
    MirrorReceiver();
    ~MirrorReceiver();

    // Non-copyable
    MirrorReceiver(const MirrorReceiver&) = delete;
    MirrorReceiver& operator=(const MirrorReceiver&) = delete;

    // Start listening for an incoming mirror stream on `port`.
    // Returns true if the listener bound successfully (async worker spawned).
    bool start(uint16_t port);

    // Stop the receiver and tear down the pipeline.
    void stop();

    bool isRunning() const { return m_running.load(); }

    // Number of frames displayed so far this session (thread-safe).
    uint64_t framesDisplayed() const { return m_frames.load(); }

private:
    // PIMPL — hides all GStreamer types from the header.
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_frames{0};
};

} // namespace titanshare
