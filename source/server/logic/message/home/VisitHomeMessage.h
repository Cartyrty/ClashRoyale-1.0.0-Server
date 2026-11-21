#pragma once
// Another player's profile. Decoded by the library's own VisitHomeMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class VisitHomeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14113;

    void decode() override {
        // VisitHomeMessage: home id LogicLong* +0x48.
        constexpr int kHomeIdOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        homeId_ = message.readLong(kHomeIdOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getHomeId() const { return homeId_; }

private:
    std::int64_t homeId_ = 0;
};
} // namespace server
