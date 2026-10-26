#include "logic/message/battle/BattleEventMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// BattleEventMessage: LogicBattleEvent* +0x48 (the message's destructor would free it).
constexpr int kBattleEventOffset = 0x48;
} // namespace

void BattleEventMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writePointer(kBattleEventOffset, battleEvent_);
    stream_.writeBytesWithoutLength(message.encode());
    message.takePointer(kBattleEventOffset);
}
} // namespace server
