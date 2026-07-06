#include "SettingsWindow.h"

#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>

#include <cstdlib>

#include "Messages.h"

namespace {

enum { kMsgStartClicked = 'stck', kMsgStopClicked = 'spck' };

const int kSampleRates[] = {44100, 48000, 88200, 96000, 192000};
const int kFrames[] = {64, 128, 256, 512, 1024, 2048};
const int kPeriods[] = {2, 3, 4, 5, 6, 7, 8};

BPopUpMenu* make_int_menu(const char* name, const int* values, size_t count, int selected,
                          int fallback) {
    BPopUpMenu* menu = new BPopUpMenu(name);
    bool marked_any = false;
    for (size_t i = 0; i < count; i++) {
        BMenuItem* item = new BMenuItem(std::to_string(values[i]).c_str(), NULL);
        menu->AddItem(item);
        if (values[i] == selected) {
            item->SetMarked(true);
            marked_any = true;
        }
    }
    if (!marked_any) {
        for (int32 i = 0; i < menu->CountItems(); i++) {
            if (atoi(menu->ItemAt(i)->Label()) == fallback)
                menu->ItemAt(i)->SetMarked(true);
        }
    }
    return menu;
}

int marked_int(BPopUpMenu* menu, int fallback) {
    BMenuItem* item = menu->FindMarked();
    if (item == NULL)
        return fallback;
    int v = atoi(item->Label());
    return v > 0 ? v : fallback;
}

} // namespace

SettingsWindow::SettingsWindow(const BMessenger& main_window, const InitialState& state)
    : BWindow(BRect(150, 150, 550, 500), "JACK Settings", B_TITLED_WINDOW_LOOK,
              B_MODAL_APP_WINDOW_FEEL,
              B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
      m_main_window(main_window), m_devices(state.devices), m_status_view(NULL),
      m_start_button(NULL), m_stop_button(NULL), m_interface_field(NULL), m_interface_menu(NULL),
      m_rate_menu(NULL), m_frames_menu(NULL), m_periods_menu(NULL), m_midi_check(NULL),
      m_realtime_check(NULL) {
    BuildLayout(state);
    SetServerState(state.server_running, state.server_running ? "Running" : "Stopped");
}

void SettingsWindow::BuildLayout(const InitialState& state) {
    m_status_view = new BStringView("server_status", "Status: unknown");
    m_start_button = new BButton("Start", new BMessage(kMsgStartClicked));
    m_stop_button = new BButton("Stop", new BMessage(kMsgStopClicked));

    m_interface_menu = new BPopUpMenu("interface");
    BMenuItem* default_item = new BMenuItem("default (first device)", NULL);
    m_interface_menu->AddItem(default_item);
    default_item->SetMarked(true);
    for (const auto& device : m_devices) {
        BMenuItem* item = new BMenuItem(device.name.c_str(), NULL);
        m_interface_menu->AddItem(item);
        if (!state.interface.empty() && device.path == state.interface)
            item->SetMarked(true);
    }
    m_interface_field = new BMenuField("Interface:", m_interface_menu);

    m_rate_menu = make_int_menu("rate", kSampleRates, sizeof(kSampleRates) / sizeof(int),
                                state.sample_rate, 48000);
    m_frames_menu = make_int_menu("frames", kFrames, sizeof(kFrames) / sizeof(int),
                                  state.frames_per_period, 128);
    m_periods_menu = make_int_menu("periods", kPeriods, sizeof(kPeriods) / sizeof(int),
                                   state.periods_per_buffer, 3);

    m_midi_check = new BCheckBox("MIDI driver (haikumidi)", NULL);
    m_midi_check->SetValue(state.midi ? B_CONTROL_ON : B_CONTROL_OFF);
    m_realtime_check = new BCheckBox("Realtime", NULL);
    m_realtime_check->SetValue(state.realtime ? B_CONTROL_ON : B_CONTROL_OFF);

    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_WINDOW_INSETS)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
        .Add(m_status_view)
        .AddGlue()
        .Add(m_start_button)
        .Add(m_stop_button)
        .End()
        .AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
        .AddMenuField(m_interface_field, 0, 0)
        .AddMenuField(new BMenuField("Sample rate:", m_rate_menu), 0, 1)
        .AddMenuField(new BMenuField("Frames/period:", m_frames_menu), 0, 2)
        .AddMenuField(new BMenuField("Periods/buffer:", m_periods_menu), 0, 3)
        .End()
        .Add(m_midi_check)
        .Add(m_realtime_check)
        .AddGlue();
}

void SettingsWindow::SetServerState(bool running, const char* label) {
    std::string text = "Status: ";
    text += label;
    m_status_view->SetText(text.c_str());
    m_start_button->SetEnabled(!running);
    m_stop_button->SetEnabled(running);
}

void SettingsWindow::SendStartRequest() {
    BMessage request(kMsgServerStartRequest);

    std::string interface;
    BMenuItem* marked = m_interface_menu->FindMarked();
    if (marked != NULL) {
        int32 index = m_interface_menu->IndexOf(marked);
        if (index > 0 && (size_t)index <= m_devices.size())
            interface = m_devices[index - 1].path;
    }
    request.AddString("interface", interface.c_str());
    request.AddInt32("sample_rate", marked_int(m_rate_menu, 48000));
    request.AddInt32("frames", marked_int(m_frames_menu, 128));
    request.AddInt32("periods", marked_int(m_periods_menu, 3));
    request.AddBool("realtime", m_realtime_check->Value() == B_CONTROL_ON);
    request.AddBool("midi", m_midi_check->Value() == B_CONTROL_ON);

    m_main_window.SendMessage(&request, (BHandler*)NULL, 0);
}

void SettingsWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kMsgStartClicked:
            m_start_button->SetEnabled(false);
            m_stop_button->SetEnabled(false);
            m_status_view->SetText("Status: starting" B_UTF8_ELLIPSIS);
            SendStartRequest();
            break;

        case kMsgStopClicked:
            m_start_button->SetEnabled(false);
            m_stop_button->SetEnabled(false);
            m_status_view->SetText("Status: stopping" B_UTF8_ELLIPSIS);
            m_main_window.SendMessage(kMsgServerStopRequest);
            break;

        case kMsgMenuSettings:
            Activate();
            break;

        case kMsgServerStarted:
            SetServerState(true, "Running");
            break;

        case kMsgServerStopped:
            SetServerState(false, "Stopped");
            break;

        case kMsgServerFailed:
        case kMsgServerCrashed: {
            SetServerState(false,
                           message->what == kMsgServerFailed ? "failed to start" : "crashed");
            const char* log = NULL;
            message->FindString("log", &log);
            std::string text = (message->what == kMsgServerFailed) ? "jackd failed to start."
                                                                   : "jackd stopped unexpectedly.";
            if (log != NULL && log[0] != '\0') {
                text += "\n\njackd output:\n";
                text += log;
            }
            BAlert* alert = new BAlert("jackd error", text.c_str(), "OK", NULL, NULL,
                                       B_WIDTH_AS_USUAL, B_STOP_ALERT);
            alert->Go(NULL); /* asynchronous: never block this looper */
            break;
        }

        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool SettingsWindow::QuitRequested() {
    m_main_window.SendMessage(kMsgSettingsClosed);
    return true;
}
