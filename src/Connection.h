#pragma once

#include <memory>
#include "Node.h"

struct Connection {
    std::shared_ptr<Node> source;
    std::shared_ptr<Node> destination;
    PortType type;

    Connection(std::shared_ptr<Node> src, std::shared_ptr<Node> dst, PortType type);
};
