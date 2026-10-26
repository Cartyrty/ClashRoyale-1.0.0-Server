#pragma once
// The player is looking for an opponent. Payload: Int estimatedSeconds.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class MatchmakeInfoMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24107;

    void encode() override { stream_.writeInt(estimatedSeconds_); }
    int getMessageType() const override { return kMessageType; }

    void setEstimatedSeconds(int estimatedSeconds) { estimatedSeconds_ = estimatedSeconds; }

private:
    int estimatedSeconds_ = 0;
};
} // namespace server
