#pragma once
// The clan leaderboard. Payload: Boolean local.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAllianceRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14401;

    void decode() override { local_ = stream_.readBoolean(); }
    int getMessageType() const override { return kMessageType; }

    bool getLocal() const { return local_; }

private:
    bool local_ = false;
};
} // namespace server
