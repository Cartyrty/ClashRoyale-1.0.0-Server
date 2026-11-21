#pragma once
// A clan's page. Decoded by the library's own AskForAllianceDataMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AskForAllianceDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14302;

    void decode() override {
        // AskForAllianceDataMessage: alliance id LogicLong* +0x48.
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
