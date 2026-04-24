/*
 * TitanShare — mDNS/Avahi Advertiser Implementation
 *
 * Publishes _titanshare._tcp on the local network with TXT records
 * containing host, port, pin, and daemon version so the Android app
 * can discover and pair automatically — no QR code needed.
 */

#include "discovery/mdns_advertiser.hpp"
#include "utils/logger.hpp"
#include "config.hpp"

// avahi headers are included via mdns_advertiser.hpp
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/alternative.h>

#include <cstring>
#include <unistd.h>  // gethostname

namespace titanshare {

// ─── Destructor ───────────────────────────────────────────────────────────────
MdnsAdvertiser::~MdnsAdvertiser() {
    stop();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void MdnsAdvertiser::start(const std::string& pin,
                           uint16_t port,
                           const std::string& hostname) {
    m_pin  = pin;
    m_port = port;

    if (hostname.empty()) {
        char buf[256]{};
        gethostname(buf, sizeof(buf) - 1);
        m_hostname = buf;
    } else {
        m_hostname = hostname;
    }

    m_running = true;
    m_thread = std::thread(&MdnsAdvertiser::runLoop, this);
}

void MdnsAdvertiser::updatePin(const std::string& newPin) {
    m_pin      = newPin;
    m_pinDirty = true;

    // Wake the poll loop so it can re-register the TXT record
    if (m_simplePoll) {
        avahi_simple_poll_quit(m_simplePoll);
    }
}

void MdnsAdvertiser::stop() {
    m_running = false;
    if (m_simplePoll) avahi_simple_poll_quit(m_simplePoll);
    if (m_thread.joinable()) m_thread.join();
    // Avahi objects are freed inside runLoop after the poll exits
}

// ─── Private: Poll Loop ───────────────────────────────────────────────────────

void MdnsAdvertiser::runLoop() {
    int avahiErr = 0;

    m_simplePoll = avahi_simple_poll_new();
    if (!m_simplePoll) {
        Logger::error("MDNS", "avahi_simple_poll_new() failed");
        m_running = false;
        return;
    }

    // Create Avahi client
    m_client = avahi_client_new(
        avahi_simple_poll_get(m_simplePoll),
        AVAHI_CLIENT_NO_FAIL,   // keep retrying if avahi-daemon not yet up
        &MdnsAdvertiser::clientCallback,
        this,
        &avahiErr
    );

    if (!m_client) {
        Logger::error("MDNS", "avahi_client_new() failed: " +
                      std::string(avahi_strerror(avahiErr)));
        avahi_simple_poll_free(m_simplePoll);
        m_simplePoll = nullptr;
        m_running = false;
        return;
    }

    Logger::info("MDNS", "🔍 mDNS advertiser started (" +
                 m_hostname + " port=" + std::to_string(m_port) + ")");

    // Run the poll loop — blocks here until avahi_simple_poll_quit() is called
    while (m_running) {
        int ret = avahi_simple_poll_iterate(m_simplePoll, 1000 /*ms*/);
        if (ret != 0) break;   // -1 = error, 1 = quit request

        // If pin changed between ticks, reset the group so createServices() reruns
        if (m_pinDirty && m_group) {
            m_pinDirty = false;
            avahi_entry_group_reset(m_group);
            createServices(m_client);
        }
    }

    // ─── Cleanup ──────────────────────────────────────────────────────────
    if (m_group)   { avahi_entry_group_free(m_group);  m_group  = nullptr; }
    if (m_client)  { avahi_client_free(m_client);      m_client = nullptr; }
    if (m_simplePoll) {
        avahi_simple_poll_free(m_simplePoll);
        m_simplePoll = nullptr;
    }

    Logger::info("MDNS", "mDNS advertiser stopped");
    m_running = false;
}

// ─── Private: Avahi Callbacks ─────────────────────────────────────────────────

void MdnsAdvertiser::clientCallback(AvahiClient* c, AvahiClientState state, void* userdata) {
    auto* self = static_cast<MdnsAdvertiser*>(userdata);

    if (state == AVAHI_CLIENT_S_RUNNING) {
        // Server is up — register our service
        self->createServices(c);
    } else if (state == AVAHI_CLIENT_FAILURE) {
        Logger::error("MDNS", "Avahi client failure: " +
                      std::string(avahi_strerror(avahi_client_errno(c))));
        avahi_simple_poll_quit(self->m_simplePoll);
    } else if (state == AVAHI_CLIENT_S_COLLISION ||
               state == AVAHI_CLIENT_S_REGISTERING) {
        // Name collision — reset group so it picks a new name
        if (self->m_group) avahi_entry_group_reset(self->m_group);
    }
}

void MdnsAdvertiser::groupCallback(AvahiEntryGroup* /*g*/, AvahiEntryGroupState state,
                                   void* userdata) {
    auto* self = static_cast<MdnsAdvertiser*>(userdata);

    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        Logger::info("MDNS", "✅ Service registered: _titanshare._tcp");
    } else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        Logger::warn("MDNS", "Name collision — trying alternative hostname");
        char* alt = avahi_alternative_service_name(self->m_hostname.c_str());
        self->m_hostname = alt;
        avahi_free(alt);
        self->createServices(avahi_entry_group_get_client(self->m_group));
    } else if (state == AVAHI_ENTRY_GROUP_FAILURE) {
        Logger::error("MDNS", "Entry group failure: " +
                      std::string(avahi_strerror(
                          avahi_client_errno(
                              avahi_entry_group_get_client(self->m_group)))));
        avahi_simple_poll_quit(self->m_simplePoll);
    }
}

void MdnsAdvertiser::createServices(AvahiClient* c) {
    if (!m_group) {
        m_group = avahi_entry_group_new(c,
                      &MdnsAdvertiser::groupCallback, this);
        if (!m_group) {
            Logger::error("MDNS", "avahi_entry_group_new() failed: " +
                          std::string(avahi_strerror(avahi_client_errno(c))));
            return;
        }
    }

    if (avahi_entry_group_is_empty(m_group)) {
        // Build TXT record: key=value pairs
        AvahiStringList* txt = nullptr;
        txt = avahi_string_list_add_pair(txt, "host",  m_hostname.c_str());
        txt = avahi_string_list_add_pair(txt, "pin",   m_pin.c_str());
        txt = avahi_string_list_add_pair(txt, "port",  std::to_string(m_port).c_str());
        txt = avahi_string_list_add_pair(txt, "ver",   config::DAEMON_VERSION.c_str());
        txt = avahi_string_list_add_pair(txt, "app",   "titanshare");

        int ret = avahi_entry_group_add_service_strlst(
            m_group,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            (AvahiPublishFlags)0,
            m_hostname.c_str(),         // service instance name
            config::MDNS_SERVICE_TYPE.c_str(),
            nullptr,                    // domain (auto)
            nullptr,                    // host (auto)
            m_port,
            txt
        );

        avahi_string_list_free(txt);

        if (ret < 0) {
            Logger::error("MDNS", "avahi_entry_group_add_service() failed: " +
                          std::string(avahi_strerror(ret)));
            return;
        }

        ret = avahi_entry_group_commit(m_group);
        if (ret < 0) {
            Logger::error("MDNS", "avahi_entry_group_commit() failed: " +
                          std::string(avahi_strerror(ret)));
        }
    }
}

} // namespace titanshare
