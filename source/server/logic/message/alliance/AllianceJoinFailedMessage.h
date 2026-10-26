#pragma once
// Joining a clan failed. Payload: VInt reason.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceJoinFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24302;

    void encode() override { stream_.writeVInt(reason_); }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 1 full, 2 not open, 4 score too low

private:
    int reason_ = 0;
};
} // namespace server
