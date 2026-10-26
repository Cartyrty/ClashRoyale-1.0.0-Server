#pragma once
// A member promoted or demoted. Payload: Long memberId, VInt role.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class ChangeAllianceMemberRoleMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14306;

    void decode() override {
        memberId_ = stream_.readLong();
        role_ = stream_.readVInt();
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getMemberId() const { return memberId_; }
    int getRole() const { return role_; }

private:
    std::int64_t memberId_ = 0;
    int role_ = 0;
};
} // namespace server
