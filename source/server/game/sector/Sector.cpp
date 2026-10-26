#include "game/sector/Sector.h"

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/network/MessageManager.h"
#include "game/replay/ReplayManager.h"
#include "game/utils/Json.h"
#include "game/utils/TimeUtil.h"
#include "logic/battle/LogicBattleLog.h"
#include "logic/command/battle/LogicDoSpellCommand.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicResources.h"
#include "logic/gameobject/LogicSummoner.h"
#include "logic/home/LogicChest.h"
#include "logic/home/LogicSpell.h"
#include "logic/home/LogicSpellDeck.h"
#include "logic/message/battle/BattleEventMessage.h"
#include "logic/message/battle/SendBattleEventMessage.h"
#include "logic/message/sector/SectorHeartbeatMessage.h"
#include "logic/message/sector/SectorStateMessage.h"
#include "titan/logic/debug/Debugger.h"
#include "titan/logic/json/LogicJSON.h"

#include <algorithm>

namespace server {
namespace {
constexpr int kDeckSize = 8;
// The client leaves a command's execute tick to the server; its rollback window assumes tickWhenGiven + 20.
constexpr int kCommandDelayTicks = 20;
constexpr int kMaxTurnsPerUpdate = 10;
constexpr int kBehindRetryDivisor = 5;            // a turn held back for a slow client is retried after 100 ms
constexpr std::int64_t kStallMilliseconds = 5000; // the server was stalled: turns restart from now
} // namespace

Sector::Sector(GameServer &server, const std::vector<std::int64_t> &accountIds) : server_(server), mode_(true) {
    for (auto accountId : accountIds) {
        Player player;
        player.accountId = accountId;
        players_.push_back(std::move(player));
    }
}

// Logic changes allowed, random seed, type, location, NPC and arena, the two decks, then both avatars through
// LogicGameMode::addPlayer.
std::unique_ptr<Sector> Sector::create(GameServer &server, int battleType, library::Ptr locationData,
                                       library::Ptr arenaData, library::Ptr npcData,
                                       const std::vector<std::int64_t> &accountIds) {
    LogicResources::prepareLocation(locationData);
    std::unique_ptr<Sector> sector(new Sector(server, accountIds));
    auto battle = sector->mode_.getBattle();
    if (!battle) {
        Debugger::warning("LogicGameMode(true) has no LogicBattle");
        return nullptr;
    }
    const auto now = TimeUtil::getMilliseconds();
    sector->mode_.setLogicChangesAllowed(true);
    sector->randomSeed_ = int(now & 0x7fffffff);
    sector->mode_.setRandomSeed(sector->randomSeed_);
    battle.setType(battleType);
    battle.setLocation(locationData);
    battle.setNpc(npcData);
    battle.setArena(arenaData);
    sector->battleLog_ = LogicBattleLog::create(arenaData);
    battle.setBattleLog(sector->battleLog_);

    std::vector<library::Ptr> decks;
    std::vector<LogicClientAvatar> avatars;
    for (auto accountId : accountIds) {
        if (accountId <= 0) continue;
        const auto *account = server.getAccountManager().getAccount(accountId);
        auto &home = sector->homes_.emplace_back(LogicClientHome::decode(account->home));
        decks.push_back(LogicSpellDeck::clone(home.getSelectedDeck()));
        avatars.push_back(LogicClientAvatar::decode(account->avatar));
    }
    if (accountIds.size() == 2 && accountIds[1] == kNpcAccountId) {
        sector->npcBattle_ = true;
        // The NPC plays its PredefinedDeck, as the v6 server's NPC::load path does.
        library::Ptr npcDeck = 0;
        const auto predefinedDeck = LogicData::getNpcPredefinedDeck(npcData);
        const auto spells = LogicData::getPredefinedDeckSpells(predefinedDeck);
        const auto levels = LogicData::getPredefinedDeckLevels(predefinedDeck);
        const int count = library::arrayCount(spells);
        if (count > 0 && count <= kDeckSize) {
            npcDeck = LogicSpellDeck::create();
            for (int slot = 0; slot < count; ++slot) {
                const int level = slot < library::arrayCount(levels) ? library::arrayIntItem(levels, slot) : 1;
                LogicSpellDeck::putSpell(npcDeck, slot, library::arrayItem(spells, slot), std::max(0, level - 1));
            }
        }
        decks.push_back(npcDeck ? npcDeck : LogicSpellDeck::clone(decks[0]));
        auto npc = LogicClientAvatar::create();
        npc.setIds(kNpcAccountId); // LogicBattle::addPlayer needs distinct account ids
        npc.setArena(arenaData);
        npc.setExpLevel(avatars[0].getExpLevel());
        if (npcData) npc.setName(LogicData::getName(npcData));
        avatars.push_back(std::move(npc));
    }
    // The replay's "battle" object (LogicBattle::loadReplay keys).
    std::string battleJSON = "{\"location\":" + std::to_string(LogicData::getGlobalID(locationData)) +
                             ",\"arena\":" + std::to_string(LogicData::getGlobalID(arenaData));
    if (npcData) battleJSON += ",\"NPC\":" + std::to_string(LogicData::getGlobalID(npcData));
    for (std::size_t i = 0; i < 2; ++i)
        battleJSON += ",\"deck" + std::to_string(i) + "\":" + LogicSpellDeck::save(decks[i]);
    for (std::size_t i = 0; i < 2; ++i)
        battleJSON += ",\"avatar" + std::to_string(i) + "\":" + sector->saveReplayAvatarJSON(avatars[i].get());
    sector->replayBattleJSON_ = battleJSON + "}";

    battle.setSpellDecks(decks[0], decks[1]);
    for (auto &avatar : avatars) sector->mode_.addPlayer(std::move(avatar));
    sector->nextTurnTime_ = now + kTurnMilliseconds;
    sector->lastActiveTime_ = now;
    return sector;
}

// LogicClientAvatar::loadForReplay's keys.
std::string Sector::saveReplayAvatarJSON(library::Ptr avatarObject) const {
    const auto avatar = LogicClientAvatar::view(avatarObject);
    const auto accountId = avatar.getAccountId();
    auto saved = json::Value::object();
    saved["accountID.hi"] = std::int64_t(std::int32_t(accountId >> 32));
    saved["accountID.lo"] = std::int64_t(std::int32_t(accountId));
    saved["name"] = avatar.getName();
    saved["expLevel"] = avatar.getExpLevel();
    saved["expPoints"] = avatar.getExpPoints();
    saved["clan_name"] = avatar.getAllianceName();
    saved["arena"] = LogicData::getGlobalID(avatar.getArena());
    saved["badge"] = LogicData::getGlobalID(avatar.getAllianceBadge());
    return saved.dump();
}

Sector::Player *Sector::getPlayer(std::int64_t accountId) {
    for (auto &player : players_)
        if (player.accountId == accountId) return &player;
    return nullptr;
}

const Sector::Player *Sector::getPlayer(std::int64_t accountId) const {
    for (const auto &player : players_)
        if (player.accountId == accountId) return &player;
    return nullptr;
}

MessageManager *Sector::getOnlinePlayer(const Player &player) const {
    return player.accountId > 0 && !player.gone ? server_.getOnlinePlayer(player.accountId) : nullptr;
}

bool Sector::hasPlayer(std::int64_t accountId) const {
    const auto *player = getPlayer(accountId);
    return player && !player->gone;
}

bool Sector::hasOtherOnlinePlayers(std::int64_t accountId) const {
    return std::any_of(players_.begin(), players_.end(),
                       [&](const Player &player) { return player.accountId != accountId && getOnlinePlayer(player); });
}

bool Sector::hasActivePlayers() const {
    return std::any_of(players_.begin(), players_.end(),
                       [](const Player &player) { return player.accountId > 0 && !player.gone; });
}

// In PvP the player's view is rebuilt from exactly the bytes they get.
void Sector::sendSectorState(std::int64_t accountId) {
    auto *online = server_.getOnlinePlayer(accountId);
    auto *player = getPlayer(accountId);
    if (!online || !player) return;
    const auto state = SectorStateMessage::encodeState(mode_, mode_.getAvatarByAccountId(accountId));
    if (!npcBattle_) player->view = SectorStateMessage::decodeState(state);
    player->reportedTick = mode_.getTick();
    SectorStateMessage message;
    message.setState(state);
    online->sendUdpMessage(message);
}

void Sector::requestSectorState(MessageManager &online, int clientTick) {
    if (auto *player = getPlayer(online.getAccountId())) repair(*player, online, clientTick);
}

// The client waits for its full update, so a request inside the interval is deferred, never dropped.
void Sector::repair(Player &player, MessageManager &online, int clientTick) {
    if (finished_) return;
    const auto now = TimeUtil::getMilliseconds();
    if (now - player.lastRepairTime < kRepairIntervalMilliseconds) {
        player.repairPending = true;
        return;
    }
    player.lastRepairTime = now;
    player.repairPending = false;
    Debugger::print("battle", "%s is out of sync at client tick %d: full state at server tick %d",
                    online.getAvatarName().c_str(), clientTick, mode_.getTick());
    sendSectorState(player.accountId);
}

void Sector::reportClientTick(std::int64_t accountId, int clientTick) {
    if (auto *player = getPlayer(accountId)) player->reportedTick = std::max(player->reportedTick, clientTick);
}

bool Sector::addPendingCommand(LogicCommand command) {
    if (finished_ || pendingCommands_.size() >= kMaxPendingCommands) return false;
    pendingCommands_.push_back(std::move(command));
    return true;
}

// NPC battles run in the client's immediate mode (GameMode::updateLogic with isImmediateMessageExecution,
// EndClientTurn every 200 ticks): the server follows the client's ticks like the home, executing each command at its
// execute tick and comparing the checksum; a client that disagrees gets the server's state.
void Sector::endClientTurn(MessageManager &online, int clientTick, int clientChecksum,
                           std::vector<LogicCommand> commands) {
    if (!npcBattle_) {
        for (auto &command : commands) pendingCommands_.push_back(std::move(command));
        return;
    }
    if (auto *player = getPlayer(online.getAccountId())) player->reportedTick = clientTick;
    std::vector<bool> added(commands.size(), false);
    while (mode_.getTick() < clientTick && !isBattleFinished()) {
        const int tick = mode_.getTick();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            if (added[i] || commands[i].getExecuteTick() > tick) continue;
            mode_.addCommand(std::move(commands[i]));
            added[i] = true;
        }
        mode_.updateOneTick();
    }
    const int serverChecksum = mode_.calculateChecksum();
    if (serverChecksum != clientChecksum && mode_.getTick() == clientTick) {
        Debugger::print("battle", "%s: NPC battle out of sync at tick %d (server %d, client %d): full state",
                        online.getAvatarName().c_str(), clientTick, serverChecksum, clientChecksum);
        sendSectorState(online.getAccountId());
    }
    if (isBattleFinished()) finish();
}

// Emotes still reach the other player on the end screen; only those during the battle go into the replay ("evt", read
// back by LogicGameMode::loadReplay with LogicBattleEvent::loadJSON).
void Sector::relayBattleEvent(std::int64_t senderAccountId, SendBattleEventMessage &event) {
    if (!event.getBattleEvent()) return;
    event.setSenderAccountId(senderAccountId); // the authenticated account
    if (!finished_ && !npcBattle_ && !friendly_)
        replayEvents_.push_back(LogicJSON::writeObject(event.saveBattleEventJSON()));
    BattleEventMessage relay;
    relay.setBattleEvent(event.getBattleEvent().get());
    notifyOpponents(senderAccountId, relay);
}

void Sector::notifyOpponents(std::int64_t accountId, PiranhaMessage &message) {
    for (const auto &player : players_)
        if (player.accountId != accountId)
            if (auto *online = getOnlinePlayer(player)) online->sendMessage(message);
}

// One PvP turn: the commands received since the last turn go out in this heartbeat, 10 ticks run (the authoritative
// state and every player's view), and every player gets the heartbeat with the checksum of their own view.
void Sector::executeTurn() {
    const int startTick = mode_.getTick();
    auto battle = mode_.getBattle();
    std::vector<std::vector<std::uint8_t>> encoded;
    for (auto &command : pendingCommands_) {
        command.setExecuteTick(std::max(startTick, command.getTickWhenGiven() + kCommandDelayTicks));
        // The opponent's client has the deck hidden: the card comes from the player's summoner deck here.
        if (command.getCommandType() == LogicDoSpellCommand::kCommandType && !LogicDoSpellCommand::hasSpell(command)) {
            const auto avatar = mode_.getAvatarByAccountId(command.getExecutorAccountId());
            const auto summoner = avatar ? battle.getLeaderCharacter(avatar) : 0;
            const auto deck = summoner ? LogicSummoner::getDeck(summoner) : 0;
            const auto spell =
                deck ? LogicSpellDeck::getSpell(deck, LogicDoSpellCommand::getSpellDeckIndex(command)) : 0;
            if (spell) LogicDoSpellCommand::setSpell(command, LogicSpell::clone(spell));
        }
        encoded.push_back(command.encode());
        replayCommands_.push_back(encoded.back());
        mode_.addCommand(std::move(command));
    }
    pendingCommands_.clear();
    for (auto &player : players_)
        if (player.view)
            for (const auto &command : encoded) player.view->addCommand(LogicCommand::decode(command));
    for (int i = 0; i < kTicksPerTurn; ++i) {
        mode_.updateOneTick();
        if (!endTick_ && isBattleFinished()) endTick_ = mode_.getTick();
        for (auto &player : players_)
            if (player.view) player.view->updateOneTick();
    }
    ++turn_;
    for (auto &player : players_) {
        auto *online = getOnlinePlayer(player);
        if (!online) continue;
        SectorHeartbeatMessage heartbeat;
        heartbeat.setTurn(turn_);
        heartbeat.setChecksum(player.view ? player.view->calculateChecksum() : mode_.calculateChecksum());
        heartbeat.setCommands(encoded);
        online->sendUdpMessage(heartbeat);
    }
    if (isBattleFinished()) finish();
}

// The library's end (LogicBattle::isFinished; LogicGameMode::tick called battleEnded, which picked the winner,
// updated scores, win / loss counts and crowns).
void Sector::finish() {
    if (finished_) return;
    finished_ = true;
    finishTime_ = TimeUtil::getMilliseconds();
    if (!endTick_) endTick_ = mode_.getTick();
    for (auto &player : players_) player.reportedTickAtEnd = player.reportedTick;
    // A battle that never reached its end (an NPC battle left mid-match can run past kMaxTicks) has no winner: asking
    // for one asserts in the library. The players keep their saved homes and avatars.
    if (!isBattleFinished()) {
        Debugger::print("battle", "the battle did not reach its end by tick %d; no result", mode_.getTick());
        return;
    }
    const auto winner = mode_.getBattle().getWinnerAvatar();
    for (auto &player : players_)
        if (player.accountId > 0) applyResult(player, winner);
    if (!npcBattle_ && !friendly_) recordReplay();
}

// As LogicProgressSimulator does: the battle avatar replaces the saved avatar, and a winner gets
// getNextWinnerChest(arena) in a free chest slot. PvP players get a BattleResultMessage after the end animation.
void Sector::applyResult(Player &player, library::Ptr winner) {
    auto *account = server_.getAccountManager().getAccount(player.accountId);
    const auto avatarObject = mode_.getAvatarByAccountId(player.accountId);
    if (!account || !avatarObject) return;
    auto avatar = LogicClientAvatar::view(avatarObject);
    auto battle = mode_.getBattle();
    auto home = LogicClientHome::decode(account->home);
    const auto arena = avatar.getArena();
    library::Ptr chestData = 0;
    if (avatarObject == winner && !npcBattle_ && !friendly_ && arena && !LogicData::isTrainingCamp(arena) &&
        home.getChestCount() < home.getMaxChestCount()) {
        chestData = avatar.getNextWinnerChest(arena);
        if (chestData) {
            avatar.updateWinnerChestIndex();
            home.addChest(LogicChest::create(chestData, LogicChest::kSourceBattleWin));
        }
    }
    // Crowns: LogicSummoner::getStars for each side, as LogicBattle::battleEnded counts them.
    int ownCrowns = 0;
    int opponentCrowns = 0;
    library::Ptr opponentAvatar = 0;
    for (const auto &other : players_)
        if (other.accountId != player.accountId) opponentAvatar = mode_.getAvatarByAccountId(other.accountId);
    const auto ownSummoner = battle.getLeaderCharacter(avatarObject);
    const auto opponentSummoner = opponentAvatar ? battle.getLeaderCharacter(opponentAvatar) : 0;
    if (ownSummoner && opponentSummoner) {
        ownCrowns = LogicSummoner::getStars(ownSummoner, opponentSummoner);
        opponentCrowns = LogicSummoner::getStars(opponentSummoner, ownSummoner);
    }
    const int trophiesBefore = LogicClientAvatar::decode(account->avatar).getScore();
    if (!npcBattle_) {
        ByteStream finalState;
        mode_.encode(finalState, avatarObject);
        auto result = std::make_unique<BattleResultMessage>();
        result->setOutcome(avatarObject == winner ? BattleResultMessage::kOutcomeWin
                           : winner               ? BattleResultMessage::kOutcomeLoss
                                                  : BattleResultMessage::kOutcomeDraw);
        result->setTrophyChange(friendly_ ? 0 : avatar.getScore() - trophiesBefore);
        result->setExpReward(battle.getExpReward());
        result->setCrowns(ownCrowns, opponentCrowns);
        result->setChest(LogicData::getGlobalID(chestData));
        result->setFinalState(finalState.getByteArray());
        player.result = std::move(result);
    }
    if (!friendly_) { // a friendly battle changes nothing on the home or avatar
        account->avatar = avatar.encode();
        account->home = home.encode();
        account->savedAt = TimeUtil::getSeconds();
        server_.getAccountManager().saveAccount(*account);
    }
    Debugger::print("battle", "account %lld %s at tick %d, crowns %d-%d, trophies %d%s%s", (long long)player.accountId,
                    avatarObject == winner ? "won"
                    : winner               ? "lost"
                                           : "drew",
                    mode_.getTick(), ownCrowns, opponentCrowns, avatar.getScore(), chestData ? ", chest " : "",
                    chestData ? LogicData::getName(chestData).c_str() : "");
}

// The battle log (LogicBattleLog::getJSON) and the replay, for both players' battle logs: the JSON LogicGameMode::
// loadReplay reads. Commands scheduled at or after the end tick never ran; a replay that kept them would fail them
// after the battle ended (LogicCommandManager::tick: "Failed to execute command", an assert on the client).
void Sector::recordReplay() {
    std::string commands;
    for (const auto &encoded : replayCommands_) {
        const auto command = LogicCommand::decode(encoded);
        if (!command || command.getExecuteTick() >= endTick_) continue;
        commands += (commands.empty() ? "" : ",") + command.saveToJSON();
    }
    std::string events;
    for (const auto &event : replayEvents_) events += (events.empty() ? "" : ",") + event;
    const std::string replay = "{\"rndSeed\":" + std::to_string(randomSeed_) + ",\"battle\":" + replayBattleJSON_ +
                               ",\"cmd\":[" + commands + "],\"evt\":[" + events + "]}";
    std::vector<ReplayManager::ReplayPlayer> replayPlayers;
    for (const auto &player : players_) {
        const auto avatar = mode_.getAvatarByAccountId(player.accountId);
        replayPlayers.push_back({player.accountId, avatar ? LogicClientAvatar::view(avatar).getName() : std::string(),
                                 avatar ? LogicClientAvatar::view(avatar).getExpLevel() : 1});
    }
    server_.getReplayManager().recordBattle(replayPlayers, LogicBattleLog::getJSON(battleLog_), replay,
                                            LogicData::getGlobalID(mode_.getBattle().getArena()));
}

void Sector::runToEnd() {
    pendingCommands_.clear();
    for (int i = 0; i < kMaxTicks && !isBattleFinished(); ++i) mode_.updateOneTick();
    if (!endTick_) endTick_ = mode_.getTick();
    finish();
}

void Sector::leave(std::int64_t accountId) {
    if (auto *player = getPlayer(accountId)) player->gone = true;
}

void Sector::update(std::int64_t now) {
    const bool anyoneOnline =
        std::any_of(players_.begin(), players_.end(), [&](const Player &player) { return getOnlinePlayer(player); });
    if (anyoneOnline) lastActiveTime_ = now;
    if (!finished_ && now - lastActiveTime_ > kAbandonMilliseconds) {
        Debugger::print("battle", "nobody returned to the battle; it runs to its end");
        runToEnd();
    }
    // A player's result goes out when their client reaches the end tick (it reports its tick with every
    // SectorCommand), or when it should have by its lag at the end, at most kResultDelayMilliseconds after.
    if (finished_)
        for (auto &player : players_) {
            if (player.accountId <= 0 || player.gone || player.resultSent || !player.result) continue;
            const auto lag = std::int64_t(std::max(0, endTick_ - player.reportedTickAtEnd)) * kTickMilliseconds;
            const bool reached = player.reportedTick >= endTick_ ||
                                 now >= finishTime_ + std::min<std::int64_t>(lag, kResultDelayMilliseconds);
            if (!reached) continue;
            if (auto *online = server_.getOnlinePlayer(player.accountId)) {
                online->sendMessage(*player.result);
                online->clearUdpInfo(); // the client ends its UDP session with the result
                player.resultSent = true;
            }
        }
    for (auto &player : players_)
        if (player.repairPending && !finished_ && now - player.lastRepairTime >= kRepairIntervalMilliseconds)
            if (auto *online = server_.getOnlinePlayer(player.accountId)) repair(player, *online, player.reportedTick);
    if (finished_ || npcBattle_) return; // NPC battles follow the client's turns
    // Turns follow the wall clock like the original server, but never run more than kMaxLeadTicks ahead of the slowest
    // connected client (an emulated or slow device would otherwise fall out of the rollback window).
    auto isClientBehind = [&] {
        for (const auto &player : players_)
            if (getOnlinePlayer(player) && mode_.getTick() - player.reportedTick > kMaxLeadTicks) return true;
        return false;
    };
    for (int turns = 0; !finished_ && now >= nextTurnTime_ && turns < kMaxTurnsPerUpdate; ++turns) {
        if (isClientBehind()) {
            nextTurnTime_ = now + kTurnMilliseconds / kBehindRetryDivisor;
            break;
        }
        executeTurn();
        nextTurnTime_ += kTurnMilliseconds;
    }
    if (now > nextTurnTime_ + kStallMilliseconds) nextTurnTime_ = now + kTurnMilliseconds;
}
} // namespace server
