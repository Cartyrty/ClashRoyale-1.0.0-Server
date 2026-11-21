#pragma once
// A clan chat message. Decoded by the library's own ChatToAllianceStreamMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChatToAllianceStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14315;

    void decode() override {
        // ChatToAllianceStreamMessage: message String* +0x48.
        constexpr int kMessageOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        message_ = message.readString(kMessageOffset);
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getMessage() const { return message_; }

private:
    std::optional<std::string> message_;
};
} // namespace server
