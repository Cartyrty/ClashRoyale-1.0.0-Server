#include "logic/battle/LogicBattle.h"

namespace server {
using library::call;
using library::Ptr;

namespace {
// LogicBattle: NPC data +0x50, arena data +0x58, battle log +0x110.
constexpr int kNpcOffset = 0x50;
constexpr int kArenaOffset = 0x58;
constexpr int kBattleLogOffset = 0x110;
} // namespace

void LogicBattle::setType(int type) {
    call("__ZN11LogicBattle7setTypeEi", {battle_, std::uint64_t(std::uint32_t(type))});
}

void LogicBattle::setLocation(Ptr locationData) {
    call("__ZN11LogicBattle11setLocationEPK17LogicLocationDatabP17LogicGameListener", {battle_, locationData, 0, 0});
}

void LogicBattle::setNpc(Ptr npcData) { library::writePointer(battle_ + kNpcOffset, npcData); }
void LogicBattle::setArena(Ptr arenaData) { library::writePointer(battle_ + kArenaOffset, arenaData); }
Ptr LogicBattle::getArena() const { return library::readPointer(battle_ + kArenaOffset); }
void LogicBattle::setBattleLog(Ptr battleLog) { library::writePointer(battle_ + kBattleLogOffset, battleLog); }

void LogicBattle::setSpellDecks(Ptr deck0, Ptr deck1) {
    call("__ZN11LogicBattle13setSpellDecksEP14LogicSpellDeckS1_", {battle_, deck0, deck1});
}

bool LogicBattle::isFinished() const { return (call("__ZNK11LogicBattle10isFinishedEv", {battle_}) & 0xff) != 0; }
Ptr LogicBattle::getWinnerAvatar() const { return call("__ZNK11LogicBattle15getWinnerAvatarEv", {battle_}); }

Ptr LogicBattle::getLeaderCharacter(Ptr avatar) const {
    return call("__ZNK11LogicBattle18getLeaderCharacterEPK17LogicClientAvatar", {battle_, avatar});
}

int LogicBattle::getExpReward() const { return int(call("__ZNK11LogicBattle12getExpRewardEv", {battle_})); }

} // namespace server
