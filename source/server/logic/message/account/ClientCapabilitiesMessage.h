#pragma once
// The device's capabilities; the server does not use them.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class ClientCapabilitiesMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10107;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
