#pragma once
// The messages a client sends, by type (the server's half of the client's LogicScrollMessageFactory). Like the
// client's, it has a UDP mode: over UDP only the battle messages exist (UdpCheckConnection, RequestSectorState,
// SectorCommand) and another type is no message; over TCP a type the server does not know becomes an UnknownMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <memory>

namespace server {
class LogicMessageFactory {
public:
    explicit LogicMessageFactory(bool udp = false) : udp_(udp) {}

    std::unique_ptr<PiranhaMessage> createMessageByType(int messageType) const; // null: no UDP message of the type

private:
    bool udp_;
};
} // namespace server
