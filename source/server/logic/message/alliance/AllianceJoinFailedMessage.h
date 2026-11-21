#pragma once
// Joining a clan failed. Encoded by the library's own AllianceJoinFailedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceJoinFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24302;

    void encode() override {
        // AllianceJoinFailedMessage: reason +0x44 (VInt).
        constexpr int kReasonOffset = 0x44;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kReasonOffset, reason_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 1 full, 2 not open, 4 score too low

private:
    int reason_ = 0;
};
} // namespace server
