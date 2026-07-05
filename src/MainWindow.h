#pragma once

#include <Window.h>

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
    void UpdateStatusBar();

    BMenuBar* m_menu_bar;
    GraphCanvasView* m_canvas;
    BStringView* m_status_view;
    BMessageRunner* m_status_runner;
};
