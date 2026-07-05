#pragma once

#include <string>

enum class PortType { AUDIO, MIDI };
enum class PortDirection { INPUT, OUTPUT };

struct Node {
    std::string name;
    std::string client_name;
    PortType type;
    PortDirection direction;
    double x, y;
    double width, height;

    Node(const std::string& name, PortType type, PortDirection direction);
    std::string full_name() const;
    std::string display_name() const;
};
