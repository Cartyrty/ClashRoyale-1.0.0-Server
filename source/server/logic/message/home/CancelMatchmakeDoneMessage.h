#pragma once
// The player stopped looking for an opponent.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class CancelMatchmakeDoneMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24125;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
