#pragma once
// A clan chat message. Payload: String(512) message.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChatToAllianceStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14315;

    void decode() override { message_ = stream_.readString(512); }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getMessage() const { return message_; }

private:
    std::optional<std::string> message_;
};
} // namespace server
