#pragma once
// The challenge was withdrawn.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class CancelChallengeDoneMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24124;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
