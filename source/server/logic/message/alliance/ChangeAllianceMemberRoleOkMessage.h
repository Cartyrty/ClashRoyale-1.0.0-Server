#pragma once
// A role change went through. Payload: Long memberId, VInt role.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class ChangeAllianceMemberRoleOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24306;

    void encode() override {
        stream_.writeLong(memberId_);
        stream_.writeVInt(role_);
    }
    int getMessageType() const override { return kMessageType; }

    void setMemberId(std::int64_t memberId) { memberId_ = memberId; }
    void setRole(int role) { role_ = role; }

private:
    std::int64_t memberId_ = 0;
    int role_ = 0;
};
} // namespace server
