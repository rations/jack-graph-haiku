#include "JackGraphApp.h"

#include "MainWindow.h"

JackGraphApp::JackGraphApp() : BApplication("application/x-vnd.JackGraph"), m_main_window(NULL) {}

void JackGraphApp::ReadyToRun() {
    m_main_window = new MainWindow();
    m_main_window->Show();
}

bool JackGraphApp::QuitRequested() {
    // JACK client teardown belongs here (a normal application thread), never
    // in a JACK callback context; it is wired in once the client exists.
    return BApplication::QuitRequested();
}
