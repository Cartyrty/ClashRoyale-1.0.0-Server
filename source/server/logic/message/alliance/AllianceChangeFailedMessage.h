#pragma once
// A clan action failed. Payload: VInt reason.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceChangeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24333;

    void encode() override { stream_.writeVInt(reason_); }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 2 invalid description

private:
    int reason_ = 0;
};
} // namespace server
