#include "MainWindow.h"

#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <StringView.h>

#include <jack/jack.h>

#include <string>

#include "GraphCanvasView.h"
#include "Messages.h"

// Probe whether a JACK server answers, without ever starting one. Replaced by
// the app's own persistent client connection once that lands; while no client
// is connected this is the only way to see a foreign jackd.
static bool IsJackServerRunning() {
    jack_status_t status;
    jack_client_t* probe = jack_client_open("jack-graph-probe", JackNoStartServer, &status);
    if (probe == NULL)
        return false;
    jack_client_close(probe);
    return true;
}

MainWindow::MainWindow()
    : BWindow(BRect(100, 100, 1300, 900), "Jack Graph", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
      m_menu_bar(NULL), m_canvas(NULL), m_status_view(NULL), m_status_runner(NULL) {
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

    // Periodic status refresh; also the safety net for any dropped
    // JACK-callback message (the window re-reads full state each tick).
    BMessage tick(kMsgStatusTick);
    m_status_runner = new BMessageRunner(BMessenger(this), &tick, 1000000);
    if (m_status_runner->InitCheck() != B_OK) {
        delete m_status_runner;
        m_status_runner = NULL;
    }

    UpdateStatusBar();
}

MainWindow::~MainWindow() {
    delete m_status_runner;
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
    m_menu_bar->AddItem(view_menu);

    BMenu* settings_menu = new BMenu("Settings");
    settings_menu->AddItem(
        new BMenuItem("JACK Settings" B_UTF8_ELLIPSIS, new BMessage(kMsgMenuSettings), ','));
    m_menu_bar->AddItem(settings_menu);

    BMenu* help_menu = new BMenu("Help");
    help_menu->AddItem(new BMenuItem("About Jack Graph", new BMessage(kMsgMenuAbout)));
    m_menu_bar->AddItem(help_menu);
}

void MainWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kMsgStatusTick:
            UpdateStatusBar();
            break;

        case kMsgMenuRefresh:
        case kMsgMenuZoomIn:
        case kMsgMenuZoomOut:
        case kMsgMenuZoomNormal:
        case kMsgMenuSettings:
        case kMsgMenuAbout:
            // Wired up as the corresponding features land.
            break;

        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool MainWindow::QuitRequested() {
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void MainWindow::UpdateStatusBar() {
    std::string status = "JACK: not connected";
    status += " | Server: ";
    status += IsJackServerRunning() ? "Running" : "Stopped";
    m_status_view->SetText(status.c_str());
}
