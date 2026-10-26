#pragma once
// The player's own open challenge withdrawn.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class CancelChallengeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14123;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
