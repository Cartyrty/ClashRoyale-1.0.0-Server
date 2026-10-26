#include "logic/message/battle/BattleResultMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// BattleResultMessage: VInts +0x44..+0x68 (outcome +0x44, trophy change +0x48, gold +0x5c, experience +0x60, own
// crowns +0x64, opponent crowns +0x68), chest global id +0x6c, final state bytes +0x78 (owned by the message),
// length +0x70.
constexpr int kOutcomeOffset = 0x44;
constexpr int kTrophyChangeOffset = 0x48;
constexpr int kExpRewardOffset = 0x60;
constexpr int kOwnCrownsOffset = 0x64;
constexpr int kOpponentCrownsOffset = 0x68;
constexpr int kChestOffset = 0x6c;
constexpr int kFinalStateLengthOffset = 0x70;
constexpr int kFinalStateOffset = 0x78;
} // namespace

void BattleResultMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writeInt(kOutcomeOffset, outcome_);
    message.writeInt(kTrophyChangeOffset, trophyChange_);
    message.writeInt(kExpRewardOffset, expReward_);
    message.writeInt(kOwnCrownsOffset, ownCrowns_);
    message.writeInt(kOpponentCrownsOffset, opponentCrowns_);
    message.writeInt(kChestOffset, chest_);
    message.writePointer(kFinalStateOffset, library::copyBytes(finalState_));
    message.writeInt(kFinalStateLengthOffset, std::int32_t(finalState_.size()));
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
