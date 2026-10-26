#pragma once
// The replay is not available any more.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class HomeBattleReplayFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24116;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
