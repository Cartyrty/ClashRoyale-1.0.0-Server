#pragma once
// Facebook friends who play; the server has none.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForPlayingFacebookFriendsMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10513;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
