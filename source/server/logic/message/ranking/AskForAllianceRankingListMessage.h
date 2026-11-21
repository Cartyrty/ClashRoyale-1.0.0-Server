#pragma once
// The clan leaderboard. Decoded by the library's own AskForAllianceRankingListMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAllianceRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14401;

    void decode() override {
        // AskForAllianceRankingListMessage: LogicLong* +0x48 (after a Boolean, when present), local bool +0x50.
        constexpr int kLocalOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        local_ = message.readBoolean(kLocalOffset);
    }
    int getMessageType() const override { return kMessageType; }

    bool getLocal() const { return local_; }

private:
    bool local_ = false;
};
} // namespace server
