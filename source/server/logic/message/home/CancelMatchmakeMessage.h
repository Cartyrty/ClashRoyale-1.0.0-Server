#pragma once
// The player stops looking for an opponent.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class CancelMatchmakeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14107;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
