#pragma once
// A friendly battle could not start. Payload: VInt reason.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class ChallengeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24121;

    void encode() override { stream_.writeVInt(reason_); }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; }

private:
    int reason_ = 0;
};
} // namespace server
