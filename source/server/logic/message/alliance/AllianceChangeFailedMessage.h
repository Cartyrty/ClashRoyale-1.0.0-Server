#pragma once
// A clan action failed. Encoded by the library's own AllianceChangeFailedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceChangeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24333;

    void encode() override {
        // AllianceChangeFailedMessage: reason +0x44 (VInt).
        constexpr int kReasonOffset = 0x44;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kReasonOffset, reason_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 2 invalid description

private:
    int reason_ = 0;
};
} // namespace server
