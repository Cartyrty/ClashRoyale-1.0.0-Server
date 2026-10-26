#pragma once
// The client is still there (every few seconds); answered with KeepAliveServerMessage.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class KeepAliveMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10108;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
