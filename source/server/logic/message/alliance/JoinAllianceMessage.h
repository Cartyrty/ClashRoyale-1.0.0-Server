#pragma once
// Joining an open clan. Payload: Long allianceId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class JoinAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14305;

    void decode() override { allianceId_ = stream_.readLong(); }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAllianceId() const { return allianceId_; }

private:
    std::int64_t allianceId_ = 0;
};
} // namespace server
