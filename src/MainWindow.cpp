#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <StringView.h>

#include <string>

#include "GraphCanvasView.h"
#include "Messages.h"
#include "Node.h"
#include "Connection.h"
#include "SettingsWindow.h"

MainWindow::MainWindow()
    : BWindow(BRect(100, 100, 1300, 900), "Jack Graph", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
      m_menu_bar(NULL), m_canvas(NULL), m_status_view(NULL), m_status_runner(NULL),
      m_refresh_runner(NULL), m_refresh_pending(false), m_jack_connected(false),
      m_settings_open(false) {
    m_config.load();
    ResizeTo((float)m_config.get_window_width(), (float)m_config.get_window_height());
    m_server.set_target(BMessenger(this));

    BuildMenuBar();

    m_canvas = new GraphCanvasView();
    m_status_view = new BStringView("status_bar", "JACK: not connected");

    BButton* refresh_button = new BButton("Refresh", new BMessage(kMsgMenuRefresh));
    BButton* settings_button = new BButton("JACK Settings", new BMessage(kMsgMenuSettings));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(m_menu_bar)
        .Add(m_canvas)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_SMALL_INSETS)
        .AddGlue()
        .Add(refresh_button)
        .Add(settings_button)
        .AddGlue()
        .End()
        .AddGroup(B_HORIZONTAL, 0)
        .SetInsets(B_USE_SMALL_INSETS, 2, B_USE_SMALL_INSETS, 2)
        .AddGlue()
        .Add(m_status_view)
        .AddGlue()
        .End();

    m_canvas->set_connect_callback([this](const std::string& src, const std::string& dst) {
        if (m_jack_connected)
            m_jack.connect_ports(src, dst);
    });
    m_canvas->set_disconnect_callback([this](const std::string& src, const std::string& dst) {
        if (m_jack_connected)
            m_jack.disconnect_ports(src, dst);
    });

    // Periodic status refresh. While disconnected it doubles as the probe
    // for a (re)appearing server: connecting is the probe.
    BMessage tick(kMsgStatusTick);
    m_status_runner = new BMessageRunner(BMessenger(this), &tick, 1000000);
    if (m_status_runner->InitCheck() != B_OK) {
        delete m_status_runner;
        m_status_runner = NULL;
    }

    TryConnectJack();
    RefreshPorts();
}

MainWindow::~MainWindow() {
    delete m_status_runner;
    delete m_refresh_runner;
}

void MainWindow::BuildMenuBar() {
    m_menu_bar = new BMenuBar("menu_bar");

    BMenu* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("Refresh", new BMessage(kMsgMenuRefresh), 'R'));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
    m_menu_bar->AddItem(file_menu);

    BMenu* view_menu = new BMenu("View");
    view_menu->AddItem(new BMenuItem("Zoom in", new BMessage(kMsgMenuZoomIn), '+'));
    view_menu->AddItem(new BMenuItem("Zoom out", new BMessage(kMsgMenuZoomOut), '-'));
    view_menu->AddItem(new BMenuItem("Zoom normal", new BMessage(kMsgMenuZoomNormal), '0'));
    view_menu->AddSeparatorItem();
    view_menu->AddItem(new BMenuItem("Reset layout", new BMessage(kMsgMenuResetLayout)));
    m_menu_bar->AddItem(view_menu);

    BMenu* settings_menu = new BMenu("Settings");
    settings_menu->AddItem(
        new BMenuItem("JACK Settings" B_UTF8_ELLIPSIS, new BMessage(kMsgMenuSettings), ','));
    m_menu_bar->AddItem(settings_menu);

    BMenu* help_menu = new BMenu("Help");
    help_menu->AddItem(new BMenuItem("About Jack Graph", new BMessage(kMsgMenuAbout)));
    m_menu_bar->AddItem(help_menu);
}

void MainWindow::TryConnectJack() {
    if (m_jack_connected)
        return;
    m_jack_connected = m_jack.connect("jack-graph", BMessenger(this));
}

void MainWindow::ScheduleRefresh() {
    // Coalesce bursts of JACK graph notifications into one refresh, 100 ms
    // after the first (state has settled by then). Window-thread only.
    if (m_refresh_pending)
        return;
    delete m_refresh_runner;
    BMessage refresh(kMsgRefreshPorts);
    m_refresh_runner = new BMessageRunner(BMessenger(this), &refresh, 100000, 1);
    if (m_refresh_runner->InitCheck() == B_OK) {
        m_refresh_pending = true;
    } else {
        delete m_refresh_runner;
        m_refresh_runner = NULL;
        RefreshPorts();
    }
}

void MainWindow::RefreshPorts() {
    m_jack.scan_ports();
    m_canvas->remove_all();

    if (m_jack_connected) {
        std::string our_client = m_jack.get_actual_client_name();
        auto ports = m_jack.get_ports();
        for (const auto& p : ports) {
            if (p.client == our_client)
                continue;
            auto node =
                std::make_shared<Node>(p.name, p.is_audio ? PortType::AUDIO : PortType::MIDI,
                                       p.is_output ? PortDirection::OUTPUT : PortDirection::INPUT);
            m_canvas->add_node(node);
        }

        auto conns = m_jack.get_connections();
        auto all_nodes = m_canvas->get_nodes();
        for (const auto& c : conns) {
            std::shared_ptr<Node> src_node, dst_node;
            for (auto& n : all_nodes) {
                if (n->full_name() == c.source)
                    src_node = n;
                if (n->full_name() == c.destination)
                    dst_node = n;
            }
            if (src_node && dst_node) {
                auto conn = std::make_shared<Connection>(src_node, dst_node, src_node->type);
                m_canvas->add_connection(conn);
            }
        }
    }

    m_canvas->layout(true);
    UpdateStatusBar();
}

void MainWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kMsgJackGraphChanged:
            ScheduleRefresh();
            break;

        case kMsgRefreshPorts:
            m_refresh_pending = false;
            RefreshPorts();
            break;

        case kMsgJackShutdown:
            // The server went away; close the client from this (window)
            // thread — never from the JACK callback that reported it.
            m_jack.disconnect();
            m_jack_connected = false;
            RefreshPorts();
            break;

        case kMsgStatusTick:
            if (!m_jack_connected) {
                TryConnectJack();
                if (m_jack_connected) {
                    RefreshPorts();
                    m_canvas->fit_to_window();
                }
            }
            UpdateStatusBar();
            break;

        case kMsgMenuRefresh:
            RefreshPorts();
            m_canvas->fit_to_window();
            break;

        case kMsgMenuZoomIn:
            m_canvas->set_zoom(m_canvas->get_zoom() * 1.2);
            break;

        case kMsgMenuZoomOut:
            m_canvas->set_zoom(m_canvas->get_zoom() / 1.2);
            break;

        case kMsgMenuZoomNormal:
            m_canvas->set_zoom(1.0);
            break;

        case kMsgMenuResetLayout:
            m_canvas->reset_layout();
            break;

        case kMsgMenuSettings:
            OpenSettingsWindow();
            break;

        case kMsgServerStartRequest:
            HandleStartRequest(message);
            break;

        case kMsgServerStopRequest:
            // Take our client out first so jackd shuts down cleanly, then
            // ask the supervisor to SIGTERM it (both non-blocking).
            m_jack.disconnect();
            m_jack_connected = false;
            RefreshPorts();
            m_server.stop();
            break;

        case kMsgServerStarted:
            if (m_settings_open)
                m_settings.SendMessage(message);
            TryConnectJack();
            if (m_jack_connected) {
                RefreshPorts();
                m_canvas->fit_to_window();
            }
            UpdateStatusBar();
            break;

        case kMsgServerStopped:
        case kMsgServerFailed:
        case kMsgServerCrashed:
            if (m_settings_open) {
                m_settings.SendMessage(message);
            } else if (message->what != kMsgServerStopped) {
                // No settings window to report through: alert from here so a
                // crash never passes silently.
                const char* log = NULL;
                message->FindString("log", &log);
                std::string text = (message->what == kMsgServerFailed)
                                       ? "jackd failed to start."
                                       : "jackd stopped unexpectedly.";
                if (log != NULL && log[0] != '\0') {
                    text += "\n\njackd output:\n";
                    text += log;
                }
                BAlert* alert = new BAlert("jackd error", text.c_str(), "OK", NULL, NULL,
                                           B_WIDTH_AS_USUAL, B_STOP_ALERT);
                alert->Go(NULL); /* asynchronous: never block this looper */
            }
            if (m_jack_connected) {
                m_jack.disconnect();
                m_jack_connected = false;
                RefreshPorts();
            }
            UpdateStatusBar();
            break;

        case kMsgSettingsClosed:
            m_settings_open = false;
            break;

        case kMsgMenuAbout: {
            BAlert* about = new BAlert("about",
                                       "Jack Graph 1.0\n\n"
                                       "JACK patchbay and server manager for Haiku.\n"
                                       "Native port of the Linux jack-graph.\n\n"
                                       "GPL-2.0",
                                       "OK");
            about->Go(NULL);
            break;
        }

        default:
            BWindow::MessageReceived(message);
            break;
    }
}

void MainWindow::OpenSettingsWindow() {
    if (m_settings_open && m_settings.IsValid()) {
        /* Already open: the window activates itself on this message. */
        m_settings.SendMessage(kMsgMenuSettings);
        return;
    }

    SettingsWindow::InitialState state;
    state.devices = m_server.list_audio_devices();
    state.interface = m_config.get_interface();
    state.sample_rate = m_config.get_sample_rate();
    state.frames_per_period = m_config.get_frames_per_period();
    state.periods_per_buffer = m_config.get_periods_per_buffer();
    state.realtime = m_config.get_realtime();
    state.midi = m_config.get_midi_enabled();
    state.server_running = m_jack_connected || m_server.is_running();

    SettingsWindow* window = new SettingsWindow(BMessenger(this), state);
    m_settings = BMessenger(window);
    m_settings_open = true;
    window->Show();
}

void MainWindow::HandleStartRequest(BMessage* message) {
    JackSettings settings;
    const char* interface = NULL;
    message->FindString("interface", &interface);
    settings.interface = interface != NULL ? interface : "";
    int32 v;
    settings.sample_rate = (message->FindInt32("sample_rate", &v) == B_OK) ? v : 48000;
    settings.frames_per_period = (message->FindInt32("frames", &v) == B_OK) ? v : 128;
    settings.periods_per_buffer = (message->FindInt32("periods", &v) == B_OK) ? v : 3;
    settings.realtime = message->GetBool("realtime", true);
    settings.midi = message->GetBool("midi", true);

    m_config.set_interface(settings.interface);
    m_config.set_sample_rate(settings.sample_rate);
    m_config.set_frames_per_period(settings.frames_per_period);
    m_config.set_periods_per_buffer(settings.periods_per_buffer);
    m_config.set_realtime(settings.realtime);
    m_config.set_midi_enabled(settings.midi);
    m_config.save();

    m_server.start(settings);
}

bool MainWindow::QuitRequested() {
    // Tear the JACK client down from the window thread while it still
    // exists; the config is saved afterwards.
    m_jack.disconnect();
    m_jack_connected = false;
    m_config.set_window_size((int)Bounds().Width(), (int)Bounds().Height());
    m_config.set_zoom(m_canvas->get_zoom());
    m_config.save();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void MainWindow::UpdateStatusBar() {
    std::string status;

    if (m_jack_connected) {
        status += "JACK: connected";
        status += " | Buffer: " + std::to_string(m_jack.get_buffer_size()) + " frames";
        status += " | Rate: " + std::to_string(m_jack.get_sample_rate()) + " Hz";
        status += " | Xruns: " + std::to_string(m_jack.get_xrun_count());
        status += " | Server: Running";
        /* The haikumidi slave fills the role the ALSA sequencer had on
         * Linux; it is active when JACK MIDI ports exist. */
        bool have_midi = false;
        for (const auto& p : m_jack.get_ports()) {
            if (p.is_midi) {
                have_midi = true;
                break;
            }
        }
        if (have_midi)
            status += " | MIDI: active";
    } else {
        status += "JACK: not connected | Server: Stopped";
    }

    m_status_view->SetText(status.c_str());
}
