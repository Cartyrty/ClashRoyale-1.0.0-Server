#pragma once
// Clans the player can join.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AskForJoinableAlliancesListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14303;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
