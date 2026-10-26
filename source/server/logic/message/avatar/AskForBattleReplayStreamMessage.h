#pragma once
// The battle log. Payload: Long accountId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AskForBattleReplayStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14406;

    void decode() override { accountId_ = stream_.readLong(); }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAccountId() const { return accountId_; }

private:
    std::int64_t accountId_ = 0;
};
} // namespace server
