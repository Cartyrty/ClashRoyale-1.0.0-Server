#pragma once
// The client's first UDP message, padded to a full-size datagram: it shows the UDP path works and gives the server the
// client's address. Only the UDP message factory makes it (TCP's 10108 is KeepAliveMessage).
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class UdpCheckConnectionMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10108;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
