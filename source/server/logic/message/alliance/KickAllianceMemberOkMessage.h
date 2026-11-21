#pragma once
// A kick went through. Encoded by the library's own KickAllianceMemberOkMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class KickAllianceMemberOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24307;

    void encode() override {
        // KickAllianceMemberOkMessage: member id LogicLong* +0x48.
        constexpr int kMemberIdOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeLong(kMemberIdOffset, memberId_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setMemberId(std::int64_t memberId) { memberId_ = memberId; }

private:
    std::int64_t memberId_ = 0;
};
} // namespace server
