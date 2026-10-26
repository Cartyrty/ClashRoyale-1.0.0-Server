#pragma once
// Creating a clan failed. Payload: VInt reason.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceCreateFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24332;

    void encode() override { stream_.writeVInt(reason_); }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) {
        reason_ = reason;
    } // 0 generic, 1 invalid name, 2 invalid description, 3 name too short

private:
    int reason_ = 0;
};
} // namespace server
