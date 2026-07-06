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
    kMsgMenuResetLayout = 'mrsl',
    kMsgMenuSettings = 'mset',
    kMsgMenuAbout = 'mabt',

    // Server supervisor -> MainWindow (relayed to SettingsWindow).
    // Failure/crash messages carry "log" (string): jackd output tail.
    kMsgServerStarted = 'svst',
    kMsgServerFailed = 'svfl',
    kMsgServerStopped = 'svsp',
    kMsgServerCrashed = 'svcr',

    // SettingsWindow -> MainWindow. Start carries the chosen settings:
    // "interface" (string, may be empty for default), "sample_rate",
    // "frames" and "periods" (int32), "realtime" and "midi" (bool).
    kMsgServerStartRequest = 'svrq',
    kMsgServerStopRequest = 'svsq',
    kMsgSettingsClosed = 'stcl'
};
