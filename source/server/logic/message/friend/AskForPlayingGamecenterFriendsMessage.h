#pragma once
// Game Center friends who play; the server has none.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForPlayingGamecenterFriendsMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10512;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
