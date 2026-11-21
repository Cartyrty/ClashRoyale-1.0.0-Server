#pragma once
// The join request was refused. Encoded by the library's own AllianceJoinRequestFailedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceJoinRequestFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24320;

    void encode() override {
        // AllianceJoinRequestFailedMessage: reason +0x44 (VInt).
        constexpr int kReasonOffset = 0x44;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kReasonOffset, reason_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 1 closed, 2 already sent, 3 score too low

private:
    int reason_ = 0;
};
} // namespace server
