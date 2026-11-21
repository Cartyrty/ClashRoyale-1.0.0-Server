#pragma once
// A join request to an invite-only clan. Decoded by the library's own RequestJoinAllianceMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <optional>
#include <string>

namespace server {
class RequestJoinAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14317;

    void decode() override {
        // RequestJoinAllianceMessage: alliance id LogicLong* +0x48, message String* +0x50, then an Int +0x58 the
        // server does not use.
        constexpr int kAllianceIdOffset = 0x48;
        constexpr int kMessageOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        allianceId_ = message.readLong(kAllianceIdOffset);
        message_ = message.readString(kMessageOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAllianceId() const { return allianceId_; }
    const std::optional<std::string> &getMessage() const { return message_; }

private:
    std::int64_t allianceId_ = 0;
    std::optional<std::string> message_;
};
} // namespace server
