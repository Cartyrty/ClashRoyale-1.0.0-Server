#pragma once
// A clan stream entry was removed. Payload: Long streamEntryId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AllianceStreamEntryRemovedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24318;

    void encode() override { stream_.writeLong(streamEntryId_); }
    int getMessageType() const override { return kMessageType; }

    void setStreamEntryId(std::int64_t streamEntryId) { streamEntryId_ = streamEntryId; }

private:
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
