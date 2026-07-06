#include "JackServerControl.h"

#include <FindDirectory.h>
#include <Path.h>
#include <Directory.h>
#include <OS.h>

#include <jack/jack.h>
#include <hmulti_audio.h>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "Messages.h"

static const char* kHmultiRoot = "/dev/audio/hmulti";
static const char* kJackdPath = "/boot/system/non-packaged/bin/jackd";
static const size_t kLogLimit = 32 * 1024;

JackServerControl::JackServerControl() : m_pid(0), m_child(false), m_stop_requested(false) {
    recover_pid();
}

JackServerControl::~JackServerControl() {
    // Do not stop jackd here: leaving the server running across app restarts
    // is intentional (the PID file lets the next session manage it).
    join_thread();
}

void JackServerControl::set_target(const BMessenger& target) {
    m_target = target;
}

void JackServerControl::join_thread() {
    if (m_thread.joinable())
        m_thread.join();
}

bool JackServerControl::is_running() const {
    jack_status_t status;
    jack_client_t* probe = jack_client_open("jack-graph-probe", JackNoStartServer, &status);
    if (probe == NULL)
        return false;
    jack_client_close(probe);
    return true;
}

std::string JackServerControl::pid_file() const {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return "";
    path.Append("jack-graph");
    create_directory(path.Path(), 0755);
    path.Append("jackd.pid");
    return path.Path();
}

void JackServerControl::write_pid_file(pid_t pid) {
    std::string file = pid_file();
    if (file.empty())
        return;
    std::ofstream pf(file);
    if (pf.is_open())
        pf << pid << "\n";
}

void JackServerControl::clear_pid_file() {
    std::string file = pid_file();
    if (!file.empty())
        unlink(file.c_str());
}

// Adopt a jackd from a previous session, but only after checking the team
// still exists AND is really jackd — a recycled team id must never be killed.
void JackServerControl::recover_pid() {
    std::string file = pid_file();
    if (file.empty())
        return;
    std::ifstream pf(file);
    if (!pf.is_open())
        return;
    long pid = 0;
    pf >> pid;
    pf.close();
    if (pid <= 0 || pid > INT32_MAX) {
        clear_pid_file();
        return;
    }

    team_info info;
    if (get_team_info((team_id)pid, &info) != B_OK || strstr(info.args, "jackd") == NULL) {
        clear_pid_file();
        return;
    }

    m_pid.store((pid_t)pid);
    m_child.store(false);
    fprintf(stderr, "jack-graph: recovered jackd team %ld from previous session\n", pid);
}

void JackServerControl::append_log(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    m_log.append(data, len);
    if (m_log.size() > kLogLimit)
        m_log.erase(0, m_log.size() - kLogLimit);
}

std::string JackServerControl::get_log_tail() const {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    /* Last ~2 KB is plenty for an error alert. */
    if (m_log.size() <= 2048)
        return m_log;
    return m_log.substr(m_log.size() - 2048);
}

// Runs on the supervisor thread.
void JackServerControl::post_status(uint32 what, bool with_log) {
    BMessage msg(what);
    if (with_log)
        msg.AddString("log", get_log_tail().c_str());
    m_target.SendMessage(&msg, (BHandler*)NULL, B_INFINITE_TIMEOUT);
}

void JackServerControl::start(const JackSettings& settings) {
    if (m_pid.load() > 0 || is_running()) {
        // Already running (ours or foreign): report success so the UI syncs.
        BMessage msg(kMsgServerStarted);
        m_target.SendMessage(&msg, (BHandler*)NULL, 0);
        return;
    }
    join_thread();
    m_stop_requested.store(false);
    {
        std::lock_guard<std::mutex> lock(m_log_mutex);
        m_log.clear();
    }

    // Global options first, then the -X slave, then the hmulti driver args
    // (after -d hmulti, -r means sample rate; before it, -r means
    // no-realtime).
    std::vector<std::string> args;
    args.push_back("jackd");
    args.push_back(settings.realtime ? "-R" : "-r");
    if (settings.midi) {
        args.push_back("-X");
        args.push_back("haikumidi");
    }
    args.push_back("-d");
    args.push_back("hmulti");
    if (!settings.interface.empty()) {
        args.push_back("-d");
        args.push_back(settings.interface);
    }
    args.push_back("-r");
    args.push_back(std::to_string(settings.sample_rate));
    args.push_back("-p");
    args.push_back(std::to_string(settings.frames_per_period));
    args.push_back("-n");
    args.push_back(std::to_string(settings.periods_per_buffer));

    std::vector<char*> argv;
    for (auto& a : args)
        argv.push_back(a.data());
    argv.push_back(nullptr);

    int fds[2];
    if (pipe(fds) != 0) {
        append_log("pipe() failed\n", 14);
        post_status(kMsgServerFailed, true);
        return;
    }

    fprintf(stderr, "jack-graph: launching:");
    for (auto& a : args)
        fprintf(stderr, " %s", a.c_str());
    fprintf(stderr, "\n");

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        append_log("fork() failed\n", 14);
        post_status(kMsgServerFailed, true);
        return;
    }

    if (pid == 0) {
        // Child: only async-signal-safe calls until exec.
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        execv(kJackdPath, argv.data());
        execvp("jackd", argv.data()); /* fallback: PATH search */
        const char* msg = "jack-graph: exec(jackd) failed\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(127);
    }

    close(fds[1]);
    m_pid.store(pid);
    m_child.store(true);
    write_pid_file(pid);

    m_thread = std::thread([this, pid, fd = fds[0]]() { supervise(pid, fd); });
}

// Supervisor thread: drain jackd output, poll readiness, detect exit.
void JackServerControl::supervise(pid_t pid, int pipe_fd) {
    fcntl(pipe_fd, F_SETFL, O_NONBLOCK);

    bool ready = false;
    bool exited = false;
    int probe_countdown = 0; /* readiness probe every ~500 ms */
    const int max_wait_ticks = 100;

    for (int tick = 0; !exited; tick++) {
        char buf[4096];
        ssize_t n;
        while ((n = read(pipe_fd, buf, sizeof(buf))) > 0)
            append_log(buf, (size_t)n);

        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }

        if (!ready) {
            if (--probe_countdown <= 0) {
                probe_countdown = 5;
                if (is_running()) {
                    ready = true;
                    post_status(kMsgServerStarted, false);
                }
            }
            if (!ready && tick >= max_wait_ticks) {
                // jackd never became ready: give up on it (SIGTERM) and
                // report failure with its output.
                kill(pid, SIGTERM);
                for (int i = 0; i < 50 && waitpid(pid, &status, WNOHANG) != pid; i++)
                    snooze(100000);
                while ((n = read(pipe_fd, buf, sizeof(buf))) > 0)
                    append_log(buf, (size_t)n);
                close(pipe_fd);
                m_pid.store(0);
                m_child.store(false);
                clear_pid_file();
                post_status(kMsgServerFailed, true);
                return;
            }
        }

        snooze(100000); /* 100 ms */
    }

    // Child exited: collect the rest of its output.
    char buf[4096];
    ssize_t n;
    while ((n = read(pipe_fd, buf, sizeof(buf))) > 0)
        append_log(buf, (size_t)n);
    close(pipe_fd);

    m_pid.store(0);
    m_child.store(false);
    clear_pid_file();

    if (!ready) {
        post_status(kMsgServerFailed, true);
    } else if (m_stop_requested.load()) {
        post_status(kMsgServerStopped, false);
    } else {
        post_status(kMsgServerCrashed, true);
    }
}

void JackServerControl::stop() {
    pid_t pid = m_pid.load();
    if (pid <= 0) {
        // Nothing we own. If a foreign server runs, the UI reports that.
        BMessage msg(kMsgServerStopped);
        m_target.SendMessage(&msg, (BHandler*)NULL, 0);
        return;
    }

    m_stop_requested.store(true);
    // SIGTERM only: jackd's clean close hands the device back to the media
    // services. The supervisor (or adopted-pid watcher) reports the result.
    if (kill(pid, SIGTERM) != 0 && errno == ESRCH) {
        m_pid.store(0);
        m_child.store(false);
        clear_pid_file();
        BMessage msg(kMsgServerStopped);
        m_target.SendMessage(&msg, (BHandler*)NULL, 0);
        return;
    }

    if (!m_child.load()) {
        // Recovered from the PID file: not our child, waitpid() cannot see
        // it — poll for its disappearance instead.
        join_thread();
        m_thread = std::thread([this, pid]() { watch_adopted(pid); });
    }
}

// Supervisor-thread variant for a jackd we did not spawn ourselves.
void JackServerControl::watch_adopted(pid_t pid) {
    for (int i = 0; i < 150; i++) { /* up to 15 s */
        if (kill(pid, 0) != 0 && errno == ESRCH) {
            m_pid.store(0);
            m_child.store(false);
            clear_pid_file();
            post_status(kMsgServerStopped, false);
            return;
        }
        snooze(100000);
    }
    append_log("jackd did not exit within 15 s of SIGTERM\n", 42);
    post_status(kMsgServerFailed, true);
}

// Enumerate /dev/audio/hmulti/<driver>/<index> device nodes. Opening
// read-only works while the media services hold the device; the ioctl gives
// the driver's friendly name. Any failure falls back to showing the path.
std::vector<AudioDevice> JackServerControl::list_audio_devices() const {
    std::vector<AudioDevice> devices;

    DIR* dir = opendir(kHmultiRoot);
    if (dir == NULL)
        return devices;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        std::string driver_dir = std::string(kHmultiRoot) + "/" + entry->d_name;

        DIR* sub = opendir(driver_dir.c_str());
        if (sub == NULL)
            continue;
        struct dirent* node;
        while ((node = readdir(sub)) != NULL) {
            if (strcmp(node->d_name, ".") == 0 || strcmp(node->d_name, "..") == 0)
                continue;

            AudioDevice device;
            device.path = driver_dir + "/" + node->d_name;
            device.name = std::string(entry->d_name) + "/" + node->d_name;

            int fd = open(device.path.c_str(), O_RDONLY);
            if (fd >= 0) {
                multi_channel_info channels[64];
                multi_description description;
                memset(&description, 0, sizeof(description));
                description.info_size = sizeof(description);
                description.request_channel_count = 64;
                description.channels = channels;
                if (ioctl(fd, B_MULTI_GET_DESCRIPTION, &description, sizeof(description)) == 0) {
                    description.friendly_name[sizeof(description.friendly_name) - 1] = '\0';
                    if (description.friendly_name[0] != '\0') {
                        device.name = std::string(description.friendly_name) + " (" +
                                      entry->d_name + "/" + node->d_name + ")";
                    }
                }
                close(fd);
            }

            devices.push_back(std::move(device));
        }
        closedir(sub);
    }

    closedir(dir);
    return devices;
}
