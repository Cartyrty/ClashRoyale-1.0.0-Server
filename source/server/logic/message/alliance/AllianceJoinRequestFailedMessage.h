#pragma once
// The join request was refused. Payload: VInt reason.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceJoinRequestFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24320;

    void encode() override { stream_.writeVInt(reason_); }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 1 closed, 2 already sent, 3 score too low

private:
    int reason_ = 0;
};
} // namespace server
