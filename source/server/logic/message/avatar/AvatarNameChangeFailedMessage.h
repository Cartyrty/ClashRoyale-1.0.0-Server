#pragma once
// A name change was refused. Encoded by the library's own AvatarNameChangeFailedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameChangeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20205;

    void encode() override {
        // AvatarNameChangeFailedMessage: error code +0x44, message String* +0x48.
        constexpr int kErrorCodeOffset = 0x44;
        constexpr int kMessageOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kErrorCodeOffset, errorCode_);
        message.writeString(kMessageOffset, message_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setErrorCode(int errorCode) { errorCode_ = errorCode; }                       // see AvatarNameRules
    void setMessage(const std::optional<std::string> &message) { message_ = message; } // null

private:
    int errorCode_ = 0;
    std::optional<std::string> message_;
};
} // namespace server
