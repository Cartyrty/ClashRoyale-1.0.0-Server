#pragma once
// The battle log. Decoded by the library's own AskForBattleReplayStreamMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AskForBattleReplayStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14406;

    void decode() override {
        // AskForBattleReplayStreamMessage: account id LogicLong* +0x48.
        constexpr int kAccountIdOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        accountId_ = message.readLong(kAccountIdOffset);
    }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAccountId() const { return accountId_; }

private:
    std::int64_t accountId_ = 0;
};
} // namespace server
