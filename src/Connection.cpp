#include "Connection.h"

Connection::Connection(std::shared_ptr<Node> src, std::shared_ptr<Node> dst, PortType type)
    : source(std::move(src)), destination(std::move(dst)), type(type) {}
