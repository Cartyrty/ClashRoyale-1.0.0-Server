#pragma once
// A name change was refused. Payload: VInt errorCode, String message.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameChangeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20205;

    void encode() override {
        stream_.writeVInt(errorCode_);
        stream_.writeString(message_);
    }
    int getMessageType() const override { return kMessageType; }

    void setErrorCode(int errorCode) { errorCode_ = errorCode; }                       // see AvatarNameRules
    void setMessage(const std::optional<std::string> &message) { message_ = message; } // null

private:
    int errorCode_ = 0;
    std::optional<std::string> message_;
};
} // namespace server
