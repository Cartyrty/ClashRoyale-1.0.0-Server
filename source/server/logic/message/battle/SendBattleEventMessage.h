#pragma once
// A battle event (an emote) for the other player. Decoded by the library's own SendBattleEventMessage; the message
// hands its LogicBattleEvent over.
#include "library/Library.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class SendBattleEventMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 12951;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    library::Object &getBattleEvent() { return battleEvent_; } // empty when none
    // LogicBattleEvent's sender (a LogicLong at +8): the server sets the authenticated account.
    void setSenderAccountId(std::int64_t accountId);
    library::Ptr saveBattleEventJSON() const; // LogicBattleEvent::saveJSON (a LogicJSONObject)

private:
    library::Object battleEvent_;
};
} // namespace server
