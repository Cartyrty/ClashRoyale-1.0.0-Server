#pragma once
// A replay from the battle log to watch. Decoded by the library's own HomeBattleReplayMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class HomeBattleReplayMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14114;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    std::int64_t getReplayId() const { return replayId_; }

private:
    std::int64_t replayId_ = 0;
};
} // namespace server
