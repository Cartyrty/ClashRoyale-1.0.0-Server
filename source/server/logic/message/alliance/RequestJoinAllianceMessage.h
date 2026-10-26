#pragma once
// A join request to an invite-only clan. Payload: Long allianceId, String message.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <optional>
#include <string>

namespace server {
class RequestJoinAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14317;

    void decode() override {
        allianceId_ = stream_.readLong();
        message_ = stream_.readString();
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAllianceId() const { return allianceId_; }
    const std::optional<std::string> &getMessage() const { return message_; }

private:
    std::int64_t allianceId_ = 0;
    std::optional<std::string> message_;
};
} // namespace server
