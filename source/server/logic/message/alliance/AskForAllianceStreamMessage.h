#pragma once
// The clan chat (the original client gets it after login unasked).
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAllianceStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14304;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
