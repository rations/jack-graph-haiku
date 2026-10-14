#pragma once

#include <Window.h>

#include "Config.h"
#include "JackClient.h"
#include "JackServerControl.h"

class BMenuBar;
class BStringView;
class BMessageRunner;
class GraphCanvasView;

class MainWindow : public BWindow {
public:
    MainWindow();
    ~MainWindow() override;

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

private:
    void BuildMenuBar();
    void TryConnectJack();
    void RefreshPorts();
    void ScheduleRefresh();
    void UpdateStatusBar();
    void OpenSettingsWindow();
    void HandleStartRequest(BMessage* message);
    BPoint CanvasCenter() const;

    BMenuBar* m_menu_bar;
    GraphCanvasView* m_canvas;
    BStringView* m_status_view;
    BMessageRunner* m_status_runner;
    BMessageRunner* m_refresh_runner;
    bool m_refresh_pending;

    Config m_config;
    JackClient m_jack;
    JackServerControl m_server;
    bool m_jack_connected;

    BMessenger m_settings; /* valid while a SettingsWindow is open */
    bool m_settings_open;
};
