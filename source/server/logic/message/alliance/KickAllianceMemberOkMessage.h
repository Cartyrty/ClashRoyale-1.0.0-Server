#pragma once
// A kick went through. Payload: Long memberId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class KickAllianceMemberOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24307;

    void encode() override { stream_.writeLong(memberId_); }
    int getMessageType() const override { return kMessageType; }

    void setMemberId(std::int64_t memberId) { memberId_ = memberId; }

private:
    std::int64_t memberId_ = 0;
};
} // namespace server
