#pragma once
// Joining an open clan. Decoded by the library's own JoinAllianceMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class JoinAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14305;

    void decode() override {
        // JoinAllianceMessage: alliance id LogicLong* +0x48, then an Int +0x50 the server does not use.
        constexpr int kAllianceIdOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        allianceId_ = message.readLong(kAllianceIdOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAllianceId() const { return allianceId_; }

private:
    std::int64_t allianceId_ = 0;
};
} // namespace server
