#include "logic/message/battle/SendBattleEventMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// SendBattleEventMessage: LogicBattleEvent* +0x48. LogicBattleEvent: sender LogicLong +8.
constexpr int kBattleEventOffset = 0x48;
constexpr int kSenderOffset = 8;
} // namespace

void SendBattleEventMessage::decode() {
    auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    battleEvent_.reset(message.takePointer(kBattleEventOffset));
}

void SendBattleEventMessage::setSenderAccountId(std::int64_t accountId) {
    library::writeLogicLong(battleEvent_.get() + kSenderOffset, accountId);
}

library::Ptr SendBattleEventMessage::saveBattleEventJSON() const {
    return library::call("__ZNK16LogicBattleEvent8saveJSONEv", {battleEvent_.get()});
}
} // namespace server
