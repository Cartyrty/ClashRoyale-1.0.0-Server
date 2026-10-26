#pragma once
// The global player leaderboard.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAvatarRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14403;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
