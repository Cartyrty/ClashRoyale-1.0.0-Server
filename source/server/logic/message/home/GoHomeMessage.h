#pragma once
// Back to the home (after a battle or a replay): the home is loaded again.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class GoHomeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14101;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
