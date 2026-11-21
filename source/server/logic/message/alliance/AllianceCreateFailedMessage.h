#pragma once
// Creating a clan failed. Encoded by the library's own AllianceCreateFailedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceCreateFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24332;

    void encode() override {
        // AllianceCreateFailedMessage: reason +0x44 (VInt).
        constexpr int kReasonOffset = 0x44;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kReasonOffset, reason_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setReason(int reason) { reason_ = reason; } // 0 generic, 1 invalid name, 2 invalid description, 3 name too short

private:
    int reason_ = 0;
};
} // namespace server
