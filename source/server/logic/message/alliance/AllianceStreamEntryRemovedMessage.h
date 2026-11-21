#pragma once
// A clan stream entry was removed. Encoded by the library's own AllianceStreamEntryRemovedMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AllianceStreamEntryRemovedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24318;

    void encode() override {
        // AllianceStreamEntryRemovedMessage: stream entry id LogicLong* +0x48.
        constexpr int kStreamEntryIdOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeLong(kStreamEntryIdOffset, streamEntryId_);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setStreamEntryId(std::int64_t streamEntryId) { streamEntryId_ = streamEntryId; }

private:
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
