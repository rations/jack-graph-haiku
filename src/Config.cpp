#include "Config.h"

#include <FindDirectory.h>
#include <Path.h>
#include <Directory.h>

#include <cstdlib>
#include <climits>
#include <fstream>

// The config file is untrusted input: every numeric value is parsed with
// strtol/strtod and range-checked; anything malformed keeps the default.
static bool parse_int(const std::string& value, int min, int max, int* out) {
    if (value.empty() || value.size() > 16)
        return false;
    char* end = NULL;
    long v = strtol(value.c_str(), &end, 10);
    if (end == NULL || *end != '\0' || v < min || v > max)
        return false;
    *out = (int)v;
    return true;
}

static bool parse_double(const std::string& value, double min, double max, double* out) {
    if (value.empty() || value.size() > 32)
        return false;
    char* end = NULL;
    double v = strtod(value.c_str(), &end);
    if (end == NULL || *end != '\0' || !(v >= min && v <= max))
        return false;
    *out = v;
    return true;
}

Config::Config()
    : m_window_width(1200), m_window_height(800), m_zoom(1.0), m_sample_rate(0), m_interface(""),
      m_frames_per_period(0), m_periods_per_buffer(0), m_realtime(true), m_midi_enabled(true) {}

Config::~Config() {}

std::string Config::settings_dir() const {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return "";
    path.Append("jack-graph");
    return path.Path();
}

std::string Config::config_file() const {
    std::string dir = settings_dir();
    if (dir.empty())
        return "";
    return dir + "/config";
}

void Config::load() {
    std::string file_path = config_file();
    if (file_path.empty())
        return;
    std::ifstream file(file_path);
    if (!file.is_open())
        return;

    std::string line;
    int line_count = 0;
    while (std::getline(file, line) && ++line_count < 200) {
        if (line.size() > 512)
            continue;
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "window_width")
            parse_int(value, 300, 16384, &m_window_width);
        else if (key == "window_height")
            parse_int(value, 200, 16384, &m_window_height);
        else if (key == "zoom")
            parse_double(value, 0.25, 4.0, &m_zoom);
        else if (key == "sample_rate")
            parse_int(value, 0, 384000, &m_sample_rate);
        else if (key == "interface")
            m_interface = value;
        else if (key == "frames_per_period")
            parse_int(value, 0, 16384, &m_frames_per_period);
        else if (key == "periods_per_buffer")
            parse_int(value, 0, 16, &m_periods_per_buffer);
        else if (key == "realtime")
            m_realtime = (value == "true");
        else if (key == "midi_enabled")
            m_midi_enabled = (value == "true");
        // Unknown keys are ignored.
    }
}

void Config::save() {
    std::string dir = settings_dir();
    if (dir.empty())
        return;
    if (create_directory(dir.c_str(), 0755) != B_OK)
        return;
    std::ofstream file(config_file());
    if (!file.is_open())
        return;
    file << "window_width=" << m_window_width << "\n";
    file << "window_height=" << m_window_height << "\n";
    file << "zoom=" << m_zoom << "\n";
    file << "sample_rate=" << m_sample_rate << "\n";
    file << "interface=" << m_interface << "\n";
    file << "frames_per_period=" << m_frames_per_period << "\n";
    file << "periods_per_buffer=" << m_periods_per_buffer << "\n";
    file << "realtime=" << (m_realtime ? "true" : "false") << "\n";
    file << "midi_enabled=" << (m_midi_enabled ? "true" : "false") << "\n";
}

int Config::get_window_width() const {
    return m_window_width;
}
int Config::get_window_height() const {
    return m_window_height;
}
void Config::set_window_size(int w, int h) {
    m_window_width = w;
    m_window_height = h;
}
double Config::get_zoom() const {
    return m_zoom;
}
void Config::set_zoom(double z) {
    m_zoom = z;
}
int Config::get_sample_rate() const {
    return m_sample_rate;
}
void Config::set_sample_rate(int sr) {
    m_sample_rate = sr;
}

std::string Config::get_interface() const {
    return m_interface;
}
void Config::set_interface(const std::string& i) {
    m_interface = i;
}

int Config::get_frames_per_period() const {
    return m_frames_per_period;
}
void Config::set_frames_per_period(int fpp) {
    m_frames_per_period = fpp;
}

int Config::get_periods_per_buffer() const {
    return m_periods_per_buffer;
}
void Config::set_periods_per_buffer(int ppb) {
    m_periods_per_buffer = ppb;
}

bool Config::get_realtime() const {
    return m_realtime;
}
void Config::set_realtime(bool rt) {
    m_realtime = rt;
}

bool Config::get_midi_enabled() const {
    return m_midi_enabled;
}
void Config::set_midi_enabled(bool midi) {
    m_midi_enabled = midi;
}
