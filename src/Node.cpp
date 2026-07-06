#include "Node.h"

Node::Node(const std::string& name, PortType type, PortDirection direction)
    : name(name), client_name(), type(type), direction(direction), x(0), y(0), width(180),
      height(24) {
    auto colon = name.find(':');
    if (colon != std::string::npos) {
        std::string client = name.substr(0, colon);
        std::string port_part = name.substr(colon + 1);
        /* Hardware clients are split into per-direction pseudo-clients
         * ("system:capture", "system_midi:playback", ...) so capture and
         * playback land in separate boxes. On Haiku the haikumidi slave
         * publishes MIDI under its own "system_midi" client, which needs the
         * same treatment as "system" (only the latter existed on Linux). */
        if (client == "system" || client == "system_midi") {
            auto last_underscore = port_part.rfind('_');
            if (last_underscore != std::string::npos) {
                std::string suffix = port_part.substr(last_underscore + 1);
                bool is_number = true;
                for (char c : suffix) {
                    if (c < '0' || c > '9') {
                        is_number = false;
                        break;
                    }
                }
                if (is_number) {
                    client_name = client + ":" + port_part.substr(0, last_underscore);
                } else {
                    client_name = client + ":" + port_part;
                }
            } else {
                client_name = client + ":" + port_part;
            }
        } else {
            client_name = client;
        }
    } else {
        client_name = name;
    }
}

std::string Node::full_name() const {
    return name;
}

std::string Node::display_name() const {
    auto colon = name.find(':');
    if (colon != std::string::npos) {
        return name.substr(colon + 1);
    }
    return name;
}
