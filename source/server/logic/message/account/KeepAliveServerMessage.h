#pragma once
// The answer to KeepAliveMessage.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class KeepAliveServerMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20108;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
