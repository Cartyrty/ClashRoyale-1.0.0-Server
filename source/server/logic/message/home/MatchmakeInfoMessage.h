#pragma once
// The player is looking for an opponent. Encoded by the library's own MatchmakeInfoMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class MatchmakeInfoMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24107;

    void encode() override {
        // MatchmakeInfoMessage: estimated seconds +0x44 (an Int; -1 by default).
        constexpr int kEstimatedSecondsOffset = 0x44;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kEstimatedSecondsOffset, estimatedSeconds_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setEstimatedSeconds(int estimatedSeconds) { estimatedSeconds_ = estimatedSeconds; }

private:
    int estimatedSeconds_ = 0;
};
} // namespace server
