#include <jack/jack.h>

#include "JackGraphApp.h"

// The status tick probes for a server by attempting to connect; without this
// every failed probe prints several lines to stderr.
static void silence_jack_errors(const char* message) {
    (void)message;
}

int main() {
    jack_set_error_function(silence_jack_errors);

    JackGraphApp app;
    app.Run();
    return 0;
}
