#include "game/sector/SectorManager.h"

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/network/MessageManager.h"
#include "game/utils/TimeUtil.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/battle/LogicBattle.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicDataTables.h"
#include "logic/message/battle/SendBattleEventMessage.h"
#include "logic/message/home/CancelMatchmakeDoneMessage.h"
#include "logic/message/home/EndClientTurnMessage.h"
#include "logic/message/home/MatchmakeInfoMessage.h"
#include "logic/message/home/StartMissionMessage.h"
#include "logic/message/notification/OpponentLeftMatchNotificationMessage.h"
#include "logic/message/notification/OpponentRejoinsMatchNotificationMessage.h"
#include "logic/message/sector/RequestSectorStateMessage.h"
#include "logic/message/sector/SectorCommandMessage.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>

namespace server {
namespace {
constexpr int kEstimatedMatchmakingSeconds = 30;
} // namespace

SectorManager::SectorManager(GameServer &server) : server_(server) {}
SectorManager::~SectorManager() = default;

Sector *SectorManager::getSectorOf(std::int64_t accountId) {
    for (auto &sector : sectors_)
        if (sector->hasPlayer(accountId)) return sector.get();
    return nullptr;
}

bool SectorManager::isInBattle(std::int64_t accountId) const {
    for (const auto &sector : sectors_)
        if (sector->hasPlayer(accountId) && !sector->isFinished()) return true;
    return false;
}

bool SectorManager::removeFromQueue(std::int64_t accountId) {
    const auto before = matchmakingQueue_.size();
    matchmakingQueue_.erase(std::remove_if(matchmakingQueue_.begin(), matchmakingQueue_.end(),
                                           [&](const MatchmakingEntry &entry) { return entry.accountId == accountId; }),
                            matchmakingQueue_.end());
    return matchmakingQueue_.size() != before;
}

void SectorManager::destroy(Sector &sector) {
    sectors_.erase(
        std::remove_if(sectors_.begin(), sectors_.end(), [&](const auto &other) { return other.get() == &sector; }),
        sectors_.end());
}

// ---------------------------------------------------------------- messages
void SectorManager::onStartMission(MessageManager &player, const StartMissionMessage &message) {
    const auto npc = message.getNpcData();
    if (!npc || isInBattle(player.getAccountId())) return;
    leave(player); // an old, finished battle
    const auto location = LogicData::getNpcLocation(npc);
    player.leaveHome();
    const auto arena = LogicClientAvatar::decode(player.getAccount()->avatar).getArena();
    auto sector = Sector::create(server_, LogicBattle::kTypeNpc, location, arena, npc,
                                 {player.getAccountId(), Sector::kNpcAccountId});
    if (!sector) return;
    Debugger::print("battle", "%s starts mission %s at %s", player.getAvatarName().c_str(),
                    LogicData::getName(npc).c_str(), LogicData::getName(location).c_str());
    sector->sendSectorState(player.getAccountId());
    sectors_.push_back(std::move(sector));
}

void SectorManager::onCancelMatchmake(MessageManager &player) {
    if (!removeFromQueue(player.getAccountId())) return;
    CancelMatchmakeDoneMessage done;
    player.sendMessage(done);
}

// The sender is the authenticated account, whatever the command says.
void SectorManager::onSectorCommand(MessageManager &player, SectorCommandMessage &message) {
    auto *sector = getSectorOf(player.getAccountId());
    if (!sector) return;
    sector->reportClientTick(player.getAccountId(), message.getClientTick());
    auto &command = message.getCommand();
    if (!command) return;
    command.setExecutorAccountId(player.getAccountId());
    sector->addPendingCommand(std::move(command));
}

void SectorManager::onRequestSectorState(MessageManager &player, const RequestSectorStateMessage &message) {
    if (auto *sector = getSectorOf(player.getAccountId())) sector->requestSectorState(player, message.getClientTick());
}

void SectorManager::onSendBattleEvent(MessageManager &player, SendBattleEventMessage &message) {
    if (auto *sector = getSectorOf(player.getAccountId())) sector->relayBattleEvent(player.getAccountId(), message);
}

bool SectorManager::onEndClientTurn(MessageManager &player, EndClientTurnMessage &message) {
    auto *sector = getSectorOf(player.getAccountId());
    if (!sector || sector->isFinished()) return false;
    for (auto &command : message.getCommands()) command.setExecutorAccountId(player.getAccountId());
    sector->endClientTurn(player, message.getTick(), message.getChecksum(), std::move(message.getCommands()));
    return true;
}

// ---------------------------------------------------------------- starting and leaving
void SectorManager::startMatchmaking(MessageManager &player, bool /*useDiamonds*/) {
    const auto accountId = player.getAccountId();
    if (isInBattle(accountId)) return;
    for (const auto &entry : matchmakingQueue_)
        if (entry.accountId == accountId) return;
    matchmakingQueue_.push_back({accountId, TimeUtil::getMilliseconds()});
    MatchmakeInfoMessage info;
    info.setEstimatedSeconds(kEstimatedMatchmakingSeconds);
    player.sendMessage(info);
    Debugger::print("battle", "%s is looking for an opponent", player.getAvatarName().c_str());
}

bool SectorManager::startFriendlyBattle(std::int64_t challengerId, std::int64_t accepterId) {
    return startPvpBattle(challengerId, accepterId, true) != nullptr;
}

// Two players in the arena of the first one (arenas.csv PvpLocation, a locations.csv row).
Sector *SectorManager::startPvpBattle(std::int64_t accountId1, std::int64_t accountId2, bool friendly) {
    auto *player1 = server_.getOnlinePlayer(accountId1);
    auto *player2 = server_.getOnlinePlayer(accountId2);
    if (!player1 || !player2 || isInBattle(accountId1) || isInBattle(accountId2)) return nullptr;
    leave(*player1);
    leave(*player2);
    player1->leaveHome();
    player2->leaveHome();
    const auto arena = LogicClientAvatar::decode(player1->getAccount()->avatar).getArena();
    const auto locationName = LogicData::getValue(arena, "PvpLocation");
    const auto location = LogicDataTables::getLocationByName(locationName);
    if (!location) {
        Debugger::warning("arena %s has no PvP location %s", LogicData::getName(arena).c_str(), locationName.c_str());
        return nullptr;
    }
    auto sector = Sector::create(server_, LogicBattle::kTypePvp, location, arena, 0, {accountId1, accountId2});
    if (!sector) return nullptr;
    sector->setFriendly(friendly);
    Debugger::print("battle", "%s: %s vs %s at %s", friendly ? "friendly battle" : "PvP",
                    player1->getAvatarName().c_str(), player2->getAvatarName().c_str(),
                    LogicData::getName(location).c_str());
    // Each player's UDP session first: the battle's messages then go over UDP once the client has opened it.
    player1->sendUdpConnectionInfo();
    player2->sendUdpConnectionInfo();
    sector->sendSectorState(accountId1);
    sector->sendSectorState(accountId2);
    sectors_.push_back(std::move(sector));
    return sectors_.back().get();
}

// A finished battle is over for this player; leaving a running one ends it for them.
void SectorManager::leave(MessageManager &player) {
    const auto accountId = player.getAccountId();
    removeFromQueue(accountId);
    player.clearUdpInfo();
    auto *sector = getSectorOf(accountId);
    if (!sector) return;
    if (!sector->isFinished() && !sector->hasOtherOnlinePlayers(accountId)) sector->runToEnd();
    sector->leave(accountId);
    if (!sector->hasActivePlayers()) destroy(*sector);
}

// The battle keeps running (PvP) or waits (NPC battles follow the client); the opponent is told.
void SectorManager::onDisconnected(MessageManager &player) {
    removeFromQueue(player.getAccountId());
    auto *sector = getSectorOf(player.getAccountId());
    if (!sector) return;
    if (sector->isFinished()) return leave(player);
    Debugger::print("battle", "%s left the battle at tick %d; it waits for them", player.getAvatarName().c_str(),
                    sector->getTick());
    OpponentLeftMatchNotificationMessage notification;
    sector->notifyOpponents(player.getAccountId(), notification);
}

// SectorState instead of OwnHomeData; the opponent is told.
bool SectorManager::rejoin(MessageManager &player) {
    auto *sector = getSectorOf(player.getAccountId());
    if (!sector) return false;
    if (sector->isFinished()) {
        leave(player);
        return false;
    }
    Debugger::print("battle", "%s rejoins the battle at tick %d", player.getAvatarName().c_str(), sector->getTick());
    player.leaveHome();
    if (!sector->isNpcBattle()) player.sendUdpConnectionInfo(); // NPC battles follow the client's turns over TCP
    sector->sendSectorState(player.getAccountId());
    OpponentRejoinsMatchNotificationMessage notification;
    sector->notifyOpponents(player.getAccountId(), notification);
    return true;
}

void SectorManager::update() {
    const auto now = TimeUtil::getMilliseconds();
    // PvP: the first two waiting players meet.
    while (matchmakingQueue_.size() >= 2) {
        const auto accountId1 = matchmakingQueue_[0].accountId;
        const auto accountId2 = matchmakingQueue_[1].accountId;
        matchmakingQueue_.pop_front();
        matchmakingQueue_.pop_front();
        startPvpBattle(accountId1, accountId2, false);
    }
    for (std::size_t i = 0; i < sectors_.size(); ++i) sectors_[i]->update(now);
}
} // namespace server
