#pragma once
/*
 * TitanShare — mDNS/Avahi Advertiser
 *
 * Publishes a _titanshare._tcp service on the local network so the
 * Android app can discover the daemon without any QR code or manual IP entry.
 *
 * Uses libavahi-client (avahi-client on Arch: sudo pacman -S avahi)
 */
// We include the Avahi headers here so the callback enum types are visible.
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>

#include <string>
#include <thread>
#include <atomic>
#include <functional>

// Avahi types are now pulled in via headers above

namespace titanshare {

class MdnsAdvertiser {
public:
    MdnsAdvertiser() = default;
    ~MdnsAdvertiser();

    // Non-copyable
    MdnsAdvertiser(const MdnsAdvertiser&) = delete;
    MdnsAdvertiser& operator=(const MdnsAdvertiser&) = delete;

    /**
     * Start advertising on the LAN.
     * @param pin      Current 6-digit pairing PIN (will appear in mDNS TXT)
     * @param port     TCP port the daemon listens on
     * @param hostname Human-readable device name shown to Android
     */
    void start(const std::string& pin, uint16_t port,
               const std::string& hostname = "");

    /** Update the advertised PIN and re-publish the TXT record. */
    void updatePin(const std::string& newPin);

    void stop();

    /** True once the Avahi poll loop is running. */
    bool isRunning() const { return m_running.load(); }

private:
    // Called from the Avahi thread
    void runLoop();

    static void clientCallback(AvahiClient* c, AvahiClientState state, void* userdata);
    static void groupCallback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata);

    void createServices(AvahiClient* c);

    // ─── State ───────────────────────────────────────────────────────
    AvahiSimplePoll* m_simplePoll{nullptr};
    AvahiClient*     m_client{nullptr};
    AvahiEntryGroup* m_group{nullptr};

    std::string      m_pin;
    uint16_t         m_port{9999};
    std::string      m_hostname;

    std::thread      m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_pinDirty{false};  ///< set true when pin changes mid-run
};

} // namespace titanshare
