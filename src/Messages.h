#pragma once

// BMessage 'what' constants shared between windows, the JACK client bridge
// and the server supervisor. All cross-thread UI updates travel as these
// messages; no thread other than a window's own looper touches its views.

enum {
    // JACK callbacks -> MainWindow (posted via cached BMessenger, timeout 0)
    kMsgJackGraphChanged = 'jgch',
    kMsgJackShutdown = 'jsdn',

    // MainWindow internal
    kMsgRefreshPorts = 'rfsh',
    kMsgStatusTick = 'stat',

    // Menu / buttons
    kMsgMenuRefresh = 'mrfr',
    kMsgMenuZoomIn = 'mzin',
    kMsgMenuZoomOut = 'mzot',
    kMsgMenuZoomNormal = 'mznl',
    kMsgMenuSettings = 'mset',
    kMsgMenuAbout = 'mabt',

    // Server supervisor -> windows
    kMsgServerStarted = 'svst',
    kMsgServerFailed = 'svfl',
    kMsgServerStopped = 'svsp',
    kMsgServerCrashed = 'svcr',

    // SettingsWindow -> supervisor / MainWindow
    kMsgServerStartRequest = 'svrq',
    kMsgServerStopRequest = 'svsq'
};
