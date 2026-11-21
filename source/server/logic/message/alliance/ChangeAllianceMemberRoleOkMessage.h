#pragma once
// A role change went through. Encoded by the library's own ChangeAllianceMemberRoleOkMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class ChangeAllianceMemberRoleOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24306;

    void encode() override {
        // ChangeAllianceMemberRoleOkMessage: member id LogicLong* +0x48, role +0x50.
        constexpr int kMemberIdOffset = 0x48;
        constexpr int kRoleOffset = 0x50;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeLong(kMemberIdOffset, memberId_);
        message.writeInt(kRoleOffset, role_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setMemberId(std::int64_t memberId) { memberId_ = memberId; }
    void setRole(int role) { role_ = role; }

private:
    std::int64_t memberId_ = 0;
    int role_ = 0;
};
} // namespace server
