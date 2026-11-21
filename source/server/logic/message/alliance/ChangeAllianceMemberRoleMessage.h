#pragma once
// A member promoted or demoted. Decoded by the library's own ChangeAllianceMemberRoleMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class ChangeAllianceMemberRoleMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14306;

    void decode() override {
        // ChangeAllianceMemberRoleMessage: member id LogicLong* +0x48, role +0x50.
        constexpr int kMemberIdOffset = 0x48;
        constexpr int kRoleOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        memberId_ = message.readLong(kMemberIdOffset);
        role_ = message.readInt(kRoleOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getMemberId() const { return memberId_; }
    int getRole() const { return role_; }

private:
    std::int64_t memberId_ = 0;
    int role_ = 0;
};
} // namespace server
