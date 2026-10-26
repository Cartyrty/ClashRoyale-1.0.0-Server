#pragma once
// A battle event (an emote) relayed to the other player. Encoded by the library's own BattleEventMessage.
#include "library/Library.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class BattleEventMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 22952;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setBattleEvent(library::Ptr battleEvent) { battleEvent_ = battleEvent; } // not taken: one event, many relays

private:
    library::Ptr battleEvent_ = 0;
};
} // namespace server
