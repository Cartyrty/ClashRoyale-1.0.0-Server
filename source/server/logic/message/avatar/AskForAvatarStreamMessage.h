#pragma once
// The player's inbox (avatar stream).
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForAvatarStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14405;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
