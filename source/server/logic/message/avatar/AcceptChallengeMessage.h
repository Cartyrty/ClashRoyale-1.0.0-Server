#pragma once
// A friendly battle challenge accepted. Decoded by the library's own AcceptChallengeMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AcceptChallengeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14120;

    void decode() override {
        // AcceptChallengeMessage: the challenge stream entry id LogicLong* +0x48 (LogicLong::decode).
        constexpr int kStreamEntryIdOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        streamEntryId_ = message.readLong(kStreamEntryIdOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the challenge

private:
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
