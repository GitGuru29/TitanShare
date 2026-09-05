#include "mirror/mirror_receiver.hpp"
#include "utils/logger.hpp"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace titanshare {

namespace {
constexpr int    MAX_FRAME_BYTES = 24 * 1024 * 1024; // guard against corrupt sizes
constexpr size_t MAX_FRAMES_PENDING = 8;             // backpressure: drop if queue full
}

struct MirrorReceiver::Impl {
    // GStreamer elements
    GstElement* pipeline = nullptr;
    GstElement* appsrc   = nullptr;

    int  listenerFd = -1;
    int  clientFd   = -1;
    std::thread worker;

    // Shared counters
    std::atomic<bool>     running{false};
    std::atomic<uint64_t> frames{0};

    // ─── Pipeline helpers ────────────────────────────────────────────────
    bool buildPipeline();
    void cleanupPipeline();
};

// ─── GStreamer callbacks (C linkage) ──────────────────────────────────────────

namespace {

gboolean onBusMessage(GstBus* bus, GstMessage* msg, gpointer user_data) {
    (void)bus;
    (void)user_data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar*  dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            Logger::error("MIRROR", "GStreamer error: " + std::string(err ? err->message : "?") +
                          (dbg ? " (" + std::string(dbg) + ")" : ""));
            if (err) g_error_free(err);
            if (dbg) g_free(dbg);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar*  dbg = nullptr;
            gst_message_parse_warning(msg, &err, &dbg);
            Logger::warn("MIRROR", "GStreamer warning: " + std::string(err ? err->message : "?"));
            if (err) g_error_free(err);
            if (dbg) g_free(dbg);
            break;
        }
        case GST_MESSAGE_EOS:
            Logger::info("MIRROR", "GStreamer EOS");
            break;
        default:
            break;
    }
    return TRUE;
}

// Helper to set sink title and overlay callbacks
void setSinkPropertiesAndSignals(GstElement* element);

// Renames the X11 window created by the video sink from the GStreamer default
// to "TITANMIRROR" by setting WM_NAME, _NET_WM_NAME, and WM_CLASS on the sink's window.
void onPrepareXWindowId(GstElement* /*sink*/, guintptr xid, gpointer /*user_data*/) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        Logger::warn("MIRROR", "Could not open X display to rename sink window");
        return;
    }

    Window win = static_cast<Window>(xid);
    const char* title = "TITANMIRROR";

    // Legacy WM_NAME
    XStoreName(dpy, win, title);
    XSetIconName(dpy, win, title);

    // Modern UTF-8 _NET_WM_NAME recommended by the EWMH spec
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    XChangeProperty(dpy, win, netWmName, utf8, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title),
                    static_cast<int>(strlen(title)));

    Atom netWmIconName = XInternAtom(dpy, "_NET_WM_ICON_NAME", False);
    XChangeProperty(dpy, win, netWmIconName, utf8, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title),
                    static_cast<int>(strlen(title)));

    // Set WM_CLASS using standard XSetClassHint
    XClassHint classHint{};
    char resName[] = "TITANMIRROR";
    char resClass[] = "TITANMIRROR";
    classHint.res_name = resName;
    classHint.res_class = resClass;
    XSetClassHint(dpy, win, &classHint);

    // Also write property directly for XWayland window managers
    char wmClassBuf[128];
    int resNameLen  = snprintf(wmClassBuf, sizeof(wmClassBuf), "%s", title);
    int resClassLen = snprintf(wmClassBuf + resNameLen + 1, sizeof(wmClassBuf) - resNameLen - 1, "%s", title);
    int totalLen    = resNameLen + 1 + resClassLen + 1;

    Atom stringAtom = XInternAtom(dpy, "STRING", False);
    Atom wmClass    = XInternAtom(dpy, "WM_CLASS", False);
    XChangeProperty(dpy, win, wmClass, stringAtom, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(wmClassBuf), totalLen);

    // Set phone aspect ratio hint (9:19 portrait) for window managers
    XSizeHints hints{};
    hints.flags = PAspect;
    hints.min_aspect.x = 9;
    hints.min_aspect.y = 19;
    hints.max_aspect.x = 9;
    hints.max_aspect.y = 19;
    XSetWMNormalHints(dpy, win, &hints);

    XFlush(dpy);
    XCloseDisplay(dpy);
    Logger::info("MIRROR", "Renamed mirror sink window class & title to 'TITANMIRROR'");
}

void setSinkPropertiesAndSignals(GstElement* element) {
    if (!element) return;
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "title")) {
        g_object_set(element, "title", "TITANMIRROR", nullptr);
    }
    if (GST_IS_VIDEO_OVERLAY(element)) {
        g_signal_connect(element, "prepare-xwindow-id", G_CALLBACK(onPrepareXWindowId), nullptr);
        g_signal_connect(element, "prepare-window-handle", G_CALLBACK(onPrepareXWindowId), nullptr);
    }
}

void onDeepElementAdded(GstBin* /*bin*/, GstBin* /*sub_bin*/, GstElement* element, gpointer /*user_data*/) {
    setSinkPropertiesAndSignals(element);
}

GstBusSyncReply busSyncHandler(GstBus* bus, GstMessage* msg, gpointer user_data) {
    (void)bus;
    (void)user_data;
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT) {
        const GstStructure* s = gst_message_get_structure(msg);
        if (s) {
            const char* name = gst_structure_get_name(s);
            if (name && (strcmp(name, "prepare-window-handle") == 0 || strcmp(name, "prepare-xwindow-id") == 0)) {
                GstElement* sink = GST_ELEMENT_CAST(GST_MESSAGE_SRC(msg));
                if (sink) {
                    setSinkPropertiesAndSignals(sink);
                }
                guintptr xid = 0;
                if (!gst_structure_get(s, "xid", G_TYPE_ULONG, &xid, nullptr)) {
                    gst_structure_get(s, "window-handle", G_TYPE_POINTER, &xid, nullptr);
                }
                if (xid != 0) {
                    onPrepareXWindowId(sink, xid, nullptr);
                }
            }
        }
    }
    return GST_BUS_PASS;
}

} // namespace

// ─── Pipeline construction ───────────────────────────────────────────────────

bool MirrorReceiver::Impl::buildPipeline() {
    // Make sure the GStreamer core & plugins are initialised with program & app names.
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        gst_init(nullptr, nullptr);
        // gst_init() may reset the application name; re-apply ours after
        g_set_prgname("TITANMIRROR");
        g_set_application_name("TITANMIRROR");
    });

    // Ensure window manager rules (e.g. Hyprland) float and size TITANMIRROR like a modern mobile phone display with uniform 3mm bezels
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, float on\" >/dev/null 2>&1");
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, size 400 807\" >/dev/null 2>&1");
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, center on\" >/dev/null 2>&1");
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, rounding 24\" >/dev/null 2>&1");
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, border_size 2\" >/dev/null 2>&1");
        (void)system("hyprctl keyword windowrule \"match:class ^(TITANMIRROR)$, border_color rgb(313244)\" >/dev/null 2>&1");
    }

    // Reset before building
    pipeline = nullptr;
    appsrc   = nullptr;

    // Use native waylandsink with uniform 10px (3mm) screen bezels via videobox
    GError* error = nullptr;
    pipeline = gst_parse_launch(
        "appsrc name=src format=time is-live=true do-timestamp=false "
        " ! queue max-size-buffers=4 leaky=downstream "
        " ! jpegdec "
        " ! videobox top=-10 bottom=-10 left=-10 right=-10 fill=black "
        " ! videoconvert "
        " ! waylandsink name=vsink sync=false",
        &error);

    if (!pipeline) {
        Logger::warn("MIRROR", "waylandsink failed, falling back to autovideosink: " +
                      std::string(error ? error->message : "unknown"));
        if (error) { g_error_free(error); error = nullptr; }

        pipeline = gst_parse_launch(
            "appsrc name=src format=time is-live=true do-timestamp=false "
            " ! queue max-size-buffers=4 leaky=downstream "
            " ! jpegdec "
            " ! videobox top=-10 bottom=-10 left=-10 right=-10 fill=black "
            " ! videoconvert "
            " ! autovideosink name=vsink sync=false",
            &error);

        if (!pipeline) {
            Logger::error("MIRROR", "Failed to create GStreamer pipeline: " +
                          std::string(error ? error->message : "unknown"));
            if (error) g_error_free(error);
            return false;
        }
    }

    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    if (!appsrc) {
        Logger::error("MIRROR", "Failed to find appsrc in pipeline");
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return false;
    }

    // Attach deep element listener so any inner sink added by autovideosink gets configured
    g_signal_connect(pipeline, "deep-element-added", G_CALLBACK(onDeepElementAdded), nullptr);

    // Attach bus sync handler to intercept prepare-window-handle / prepare-xwindow-id immediately
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, busSyncHandler, nullptr, nullptr);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK(onBusMessage), this);
    gst_object_unref(bus);

    // Transition to READY so sink resolves
    gst_element_set_state(pipeline, GST_STATE_READY);

    // Iterate sinks and configure properties / signals on existing sink elements
    GstIterator* it = gst_bin_iterate_sinks(GST_BIN(pipeline));
    GValue item = G_VALUE_INIT;
    while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        GstElement* e = GST_ELEMENT_CAST(g_value_get_object(&item));
        setSinkPropertiesAndSignals(e);
        g_value_reset(&item);
    }
    gst_iterator_free(it);

    // Announce caps so jpegdec configures immediately
    GstCaps* caps = gst_caps_new_simple("image/jpeg", nullptr);
    g_object_set(appsrc, "caps", caps, nullptr);
    gst_caps_unref(caps);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        Logger::error("MIRROR", "Failed to set GStreamer pipeline to PLAYING");
        cleanupPipeline();
        return false;
    }

    Logger::info("MIRROR", "✅ GStreamer mirror pipeline is PLAYING");
    return true;
}

void MirrorReceiver::Impl::cleanupPipeline() {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    if (appsrc) {
        gst_object_unref(appsrc);
        appsrc = nullptr;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

MirrorReceiver::MirrorReceiver() : m_impl(std::make_unique<Impl>()) {}

MirrorReceiver::~MirrorReceiver() {
    stop();
}

bool MirrorReceiver::start(uint16_t port) {
    if (m_running.load()) {
        Logger::warn("MIRROR", "Mirror receiver already running");
        return false;
    }

    // ─── Join previous worker thread if finished ───────────────────────
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }

    // ─── Close any stale listener left over from a previous session ─────
    // If the previous stream ended normally, the worker closed the client
    // fd but left the listening socket bound; without this a fresh bind()
    // would fail with EADDRINUSE and START_MIRROR would report an error.
    if (m_impl->listenerFd >= 0) {
        ::close(m_impl->listenerFd);
        m_impl->listenerFd = -1;
    }

    // ─── Bind the TCP listener ───────────────────────────────────────────
    int sfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        Logger::error("MIRROR", "socket() failed: " + std::string(strerror(errno)));
        return false;
    }

    int one = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::error("MIRROR", "bind(port " + std::to_string(port) + ") failed: " +
                      std::string(strerror(errno)));
        ::close(sfd);
        return false;
    }

    if (::listen(sfd, 1) < 0) {
        Logger::error("MIRROR", "listen() failed: " + std::string(strerror(errno)));
        ::close(sfd);
        return false;
    }

    m_impl->listenerFd = sfd;
    m_frames.store(0);
    m_running.store(true);

    // ─── Spawn the worker thread ──────────────────────────────────────────
    m_impl->worker = std::thread([this, port]() {
        Logger::info("MIRROR", "📺 Mirror receiver listening on port " + std::to_string(port));

        // Accept a single client connection. Use poll() with a timeout so the
        // thread stays responsive to stop() (a blocking accept() would hang).
        sockaddr_in peer{};
        socklen_t   peerLen = sizeof(peer);
        int cfd = -1;
        while (m_running.load()) {
            struct pollfd pfd{};
            pfd.fd     = m_impl->listenerFd;
            pfd.events = POLLIN;
            int pr = ::poll(&pfd, 1, 200);
            if (pr < 0) {
                if (errno == EINTR) continue;
                Logger::error("MIRROR", "poll() failed: " + std::string(strerror(errno)));
                break;
            }
            if (pr == 0) continue; // timeout — re-check running flag

            cfd = ::accept(m_impl->listenerFd,
                           reinterpret_cast<sockaddr*>(&peer), &peerLen);
            if (cfd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                Logger::error("MIRROR", "accept() failed: " + std::string(strerror(errno)));
                break;
            }
            break;
        }

        if (cfd < 0) {
            m_running.store(false);
            return;
        }
        m_impl->clientFd = cfd;
        Logger::info("MIRROR", "🔗 Mirror stream connected from " +
                      std::string(inet_ntoa(peer.sin_addr)));

        if (!m_impl->buildPipeline()) {
            ::close(cfd);
            m_impl->clientFd = -1;
            m_running.store(false);
            return;
        }

        // ─── Read + push loop ─────────────────────────────────────────────
        uint8_t hdr[4];
        bool keepGoing = true;
        while (keepGoing && m_running.load()) {
            ssize_t r = ::recv(cfd, hdr, sizeof(hdr), MSG_WAITALL);
            if (r <= 0) break; // connection closed / error

            uint32_t len = (static_cast<uint32_t>(hdr[0]) << 24) |
                           (static_cast<uint32_t>(hdr[1]) << 16) |
                           (static_cast<uint32_t>(hdr[2]) << 8)  |
                           (static_cast<uint32_t>(hdr[3]));
            if (len == 0 || len > MAX_FRAME_BYTES) {
                Logger::warn("MIRROR", "Invalid frame length " + std::to_string(len) +
                              ", stopping");
                break;
            }

            std::vector<uint8_t> payload(len);
            size_t filled = 0;
            while (filled < len) {
                ssize_t n = ::recv(cfd, payload.data() + filled, len - filled, 0);
                if (n <= 0) { keepGoing = false; break; }
                filled += static_cast<size_t>(n);
            }
            if (!keepGoing) break;

            // ── Push into appsrc ─────────────────────────────────────────
            GstBuffer* buf = gst_buffer_new_allocate(nullptr, len, nullptr);
            if (!buf) continue;
            gst_buffer_fill(buf, 0, payload.data(), len);
            GST_BUFFER_PTS(buf)      = GST_CLOCK_TIME_NONE;
            GST_BUFFER_DURATION(buf) = GST_CLOCK_TIME_NONE;

            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(m_impl->appsrc), buf);
            if (ret != GST_FLOW_OK) {
                Logger::debug("MIRROR", "appsrc push flow " + std::to_string(ret));
            } else {
                m_frames.fetch_add(1);
            }
        }

        Logger::info("MIRROR", "📺 Mirror stream ended after " +
                      std::to_string(m_frames.load()) + " frames");
        m_impl->cleanupPipeline();
        ::close(cfd);
        m_impl->clientFd = -1;
        // Close the listening socket too so a subsequent START_MIRROR can
        // re-bind the port cleanly (see the stale-listener cleanup above).
        if (m_impl->listenerFd >= 0) {
            ::close(m_impl->listenerFd);
            m_impl->listenerFd = -1;
        }
        m_running.store(false);
    });

    return true;
}

void MirrorReceiver::stop() {
    m_running.store(false);

    // Close the listener so accept()/recv() unblocks on the worker thread
    if (m_impl->listenerFd >= 0) {
        ::shutdown(m_impl->listenerFd, SHUT_RDWR);
        ::close(m_impl->listenerFd);
        m_impl->listenerFd = -1;
    }
    if (m_impl->clientFd >= 0) {
        ::shutdown(m_impl->clientFd, SHUT_RDWR);
        ::close(m_impl->clientFd);
        m_impl->clientFd = -1;
    }

    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }

    m_impl->cleanupPipeline();
    Logger::info("MIRROR", "🛑 Mirror receiver stopped");
}

} // namespace titanshare
