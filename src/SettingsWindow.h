#pragma once

#include <Window.h>
#include <Messenger.h>

#include <string>
#include <vector>

#include "JackServerControl.h"

class BButton;
class BCheckBox;
class BMenuField;
class BPopUpMenu;
class BStringView;

// JACK server settings + start/stop. Runs on its own looper thread and never
// touches MainWindow state directly: Start/Stop are posted as request
// messages; MainWindow owns Config and JackServerControl and relays the
// supervisor's status messages back here.
class SettingsWindow : public BWindow {
public:
    struct InitialState {
        std::vector<AudioDevice> devices;
        std::string interface;
        int sample_rate;
        int frames_per_period;
        int periods_per_buffer;
        bool realtime;
        bool midi;
        bool server_running;
    };

    SettingsWindow(const BMessenger& main_window, const InitialState& state);

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

private:
    void BuildLayout(const InitialState& state);
    void SetServerState(bool running, const char* label);
    void SendStartRequest();

    BMessenger m_main_window;
    std::vector<AudioDevice> m_devices;

    BStringView* m_status_view;
    BButton* m_start_button;
    BButton* m_stop_button;
    BMenuField* m_interface_field;
    BPopUpMenu* m_interface_menu;
    BPopUpMenu* m_rate_menu;
    BPopUpMenu* m_frames_menu;
    BPopUpMenu* m_periods_menu;
    BCheckBox* m_midi_check;
    BCheckBox* m_realtime_check;
};
