#pragma once
// The client stopped its home simulation.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class HomeLogicStoppedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14105;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
