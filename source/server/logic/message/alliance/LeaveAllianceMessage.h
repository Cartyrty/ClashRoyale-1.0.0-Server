#pragma once
// Leaving the clan.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class LeaveAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14308;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
