#pragma once
// A join request accepted or rejected. Payload: Long streamEntryId, Boolean accepted.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class RespondToAllianceJoinRequestMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14321;

    void decode() override {
        streamEntryId_ = stream_.readLong();
        accepted_ = stream_.readBoolean();
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the request
    bool getAccepted() const { return accepted_; }

private:
    std::int64_t streamEntryId_ = 0;
    bool accepted_ = false;
};
} // namespace server
