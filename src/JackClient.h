#pragma once

#include <jack/jack.h>
#include <Messenger.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// libjack wrapper. JACK callbacks run on JACK's notification threads, never
// on a window thread: each one only posts a BMessage through the cached
// BMessenger (non-blocking, timeout 0) or bumps an atomic. The window thread
// re-reads full state via scan_ports()/get_connections() when notified, so a
// dropped message is harmless.
class JackClient {
public:
    JackClient();
    ~JackClient();

    bool connect(const std::string& client_name, const BMessenger& target);
    void disconnect();
    bool is_connected() const { return m_client != nullptr; }
    // True once JACK has reported the server gone. The handle is then an
    // orphan: calls on it may block or fail, so the wrappers refuse it.
    bool server_gone() const { return m_server_gone.load(std::memory_order_acquire); }
    // Bumped on every connect(); a shutdown message carries the generation it
    // was posted for, so a late one cannot tear down a newer client.
    uint32 generation() const { return m_generation.load(std::memory_order_acquire); }
    std::string get_actual_client_name() const;

    struct PortInfo {
        std::string name;
        std::string client;
        bool is_input;
        bool is_output;
        bool is_audio;
        bool is_midi;
    };

    struct ConnectionInfo {
        std::string source;
        std::string destination;
    };

    std::vector<PortInfo> get_ports() const;
    std::vector<ConnectionInfo> get_connections() const;

    bool connect_ports(const std::string& source, const std::string& dest);
    bool disconnect_ports(const std::string& source, const std::string& dest);

    jack_nframes_t get_buffer_size() const;
    jack_nframes_t get_sample_rate() const;
    uint64_t get_xrun_count() const { return m_xrun_count.load(std::memory_order_relaxed); }

    void scan_ports();

private:
    void post_message(uint32 what);
    bool usable() const { return m_client != nullptr && !server_gone(); }

    static void client_registration_callback(const char* name, int reg, void* arg);
    static void port_registration_callback(jack_port_id_t port_id, int reg, void* arg);
    static void port_connect_callback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
    static int sample_rate_callback(jack_nframes_t nframes, void* arg);
    static int buffer_size_callback(jack_nframes_t nframes, void* arg);
    static int xrun_callback(void* arg);
    static void info_shutdown_callback(jack_status_t code, const char* reason, void* arg);

    jack_client_t* m_client;
    BMessenger m_messenger;
    // Gates callback message posting during connect/teardown. Written on the
    // owning (window) thread only; read from JACK notification threads.
    std::atomic<bool> m_active;
    // Set from the JACK shutdown callback, cleared by connect().
    std::atomic<bool> m_server_gone;
    // Written by connect() on the owning thread; read from the shutdown callback.
    std::atomic<uint32> m_generation;
    std::atomic<uint64_t> m_xrun_count;
    std::vector<PortInfo> m_ports;
    // Guards m_ports (scanned on the window thread, copied out by callers).
    mutable std::mutex m_mutex;
};
