#pragma once
// TV Royale's replays.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForTVContentMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14402;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
