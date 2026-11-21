#pragma once
// A join request accepted or rejected. Decoded by the library's own RespondToAllianceJoinRequestMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class RespondToAllianceJoinRequestMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14321;

    void decode() override {
        // RespondToAllianceJoinRequestMessage: request stream entry id LogicLong* +0x48, accepted bool +0x50.
        constexpr int kStreamEntryIdOffset = 0x48;
        constexpr int kAcceptedOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        streamEntryId_ = message.readLong(kStreamEntryIdOffset);
        accepted_ = message.readBoolean(kAcceptedOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the request
    bool getAccepted() const { return accepted_; }

private:
    std::int64_t streamEntryId_ = 0;
    bool accepted_ = false;
};
} // namespace server
