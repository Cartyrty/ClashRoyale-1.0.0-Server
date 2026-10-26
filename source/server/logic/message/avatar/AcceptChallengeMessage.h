#pragma once
// A friendly battle challenge accepted. Payload: Long streamEntryId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AcceptChallengeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14120;

    void decode() override { streamEntryId_ = stream_.readLong(); }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the challenge

private:
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
