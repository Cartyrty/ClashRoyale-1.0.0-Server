#pragma once
// A replay was watched; the server does not track it.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class HomeBattleReplayViewedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14117;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
