#pragma once

#include <Messenger.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/types.h>

struct JackSettings {
    std::string interface; /* hmulti device path; empty = driver default */
    int sample_rate;
    int frames_per_period;
    int periods_per_buffer;
    bool realtime;
    bool midi; /* load the haikumidi slave */
};

struct AudioDevice {
    std::string path; /* /dev/audio/hmulti/... */
    std::string name; /* driver-reported friendly name, or the path */
};

// Spawns, supervises and stops jackd. start() and stop() return immediately;
// all waiting happens on a supervisor thread which reports through the target
// BMessenger (kMsgServerStarted/Failed/Stopped/Crashed). A window thread must
// never block on the server.
//
// Ownership/locking: m_pid, m_child and m_stop_requested are atomics shared
// with the supervisor thread; m_log is guarded by m_log_mutex. All public
// methods are called from the owning window's thread only.
class JackServerControl {
public:
    JackServerControl();
    ~JackServerControl();

    void set_target(const BMessenger& target);

    // True if any JACK server answers (ours or foreign).
    bool is_running() const;
    // True if we track a jackd we own (started by us, or recovered).
    bool is_ours() const { return m_pid.load() > 0; }

    void start(const JackSettings& settings);
    void stop();

    std::vector<AudioDevice> list_audio_devices() const;
    std::string get_log_tail() const;

private:
    void supervise(pid_t pid, int pipe_fd);
    void watch_adopted(pid_t pid);
    void append_log(const char* data, size_t len);
    void post_status(uint32 what, bool with_log);
    void join_thread();
    void recover_pid();
    std::string pid_file() const;
    void write_pid_file(pid_t pid);
    void clear_pid_file();

    BMessenger m_target;
    std::atomic<pid_t> m_pid;  /* jackd we own; 0 = none */
    std::atomic<bool> m_child; /* m_pid is our direct child (waitpid works) */
    std::atomic<bool> m_stop_requested;
    std::thread m_thread;

    mutable std::mutex m_log_mutex;
    std::string m_log;
};
