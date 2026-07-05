#pragma once

#include <Application.h>

class MainWindow;

class JackGraphApp : public BApplication {
public:
    JackGraphApp();

    void ReadyToRun() override;
    bool QuitRequested() override;

private:
    MainWindow* m_main_window;
};
