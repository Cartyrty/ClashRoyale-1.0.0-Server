#pragma once
// The local player leaderboard.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAvatarLocalRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14404;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
