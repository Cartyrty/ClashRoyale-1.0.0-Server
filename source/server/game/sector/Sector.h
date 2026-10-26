#pragma once
// One battle (an NPC mission or a PvP match), simulated by the library's battle LogicGameMode exactly as the client
// simulates it. NPC battles run in the client's immediate mode: the server follows the client's turns
// (EndClientTurnMessage). PvP battles follow the wall clock: every 10-tick turn the commands received since the last
// turn go out to both players in a SectorHeartbeatMessage. Results are applied to the homes as the library's
// LogicProgressSimulator does.
#include "logic/command/LogicCommand.h"
#include "logic/message/battle/BattleResultMessage.h"
#include "logic/mode/LogicGameMode.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace server {
class GameServer;
class MessageManager;
class PiranhaMessage;
class SendBattleEventMessage;

class Sector {
public:
    static constexpr std::int64_t kNpcAccountId = -1;
    static constexpr int kTicksPerTurn = 10;      // SectorManager::receiveSectorHeartbeat: turn * 10
    static constexpr int kTurnMilliseconds = 500; // 10 ticks of 50 ms (GameMode::updateLogic)
    static constexpr int kTickMilliseconds = 50;
    static constexpr int kMaxTicks = 20 * 60 * 20;           // the safety stop when an abandoned battle runs to its end
    static constexpr int kMaxLeadTicks = 60;                 // ticks the server may run ahead of the slowest client
    static constexpr int kResultDelayMilliseconds = 5000;    // the longest wait for a client to reach the end
    static constexpr int kRepairIntervalMilliseconds = 1000; // RequestSectorState rate limit per player
    static constexpr std::int64_t kAbandonMilliseconds = 10 * 60 * 1000; // nobody returned: the battle runs to its end
    static constexpr std::size_t kMaxPendingCommands = 64;

    // The setup order of LogicBattle::loadReplay. A battle against the NPC has kNpcAccountId as its second player.
    static std::unique_ptr<Sector> create(GameServer &server, int battleType, library::Ptr locationData,
                                          library::Ptr arenaData, library::Ptr npcData,
                                          const std::vector<std::int64_t> &accountIds);

    bool isNpcBattle() const { return npcBattle_; }
    bool isFriendly() const { return friendly_; }
    void setFriendly(bool friendly) { friendly_ = friendly; }
    bool isFinished() const { return finished_; }
    int getTick() const { return mode_.getTick(); }
    bool hasPlayer(std::int64_t accountId) const; // still in it (not gone home)
    bool hasOtherOnlinePlayers(std::int64_t accountId) const;
    bool hasActivePlayers() const;

    // Messages.
    void sendSectorState(std::int64_t accountId);
    // RequestSectorStateMessage: the client found a checksum mismatch; it gets the authoritative state again.
    void requestSectorState(MessageManager &player, int clientTick);
    void reportClientTick(std::int64_t accountId, int clientTick);
    // A PvP command for the next turn (false when the battle takes no more commands).
    bool addPendingCommand(LogicCommand command);
    // An NPC battle's client turn; a PvP turn's commands join the next heartbeat.
    void endClientTurn(MessageManager &player, int clientTick, int clientChecksum, std::vector<LogicCommand> commands);
    // An emote for the other player; those during a PvP battle go into the replay.
    void relayBattleEvent(std::int64_t senderAccountId, SendBattleEventMessage &message);
    void notifyOpponents(std::int64_t accountId, PiranhaMessage &message);

    void leave(std::int64_t accountId); // the player is gone for good (after the battle, or giving it up)
    void runToEnd();                    // nobody is left to play
    void update(std::int64_t nowMilliseconds);

private:
    struct Player {
        std::int64_t accountId = 0;
        bool gone = false;         // went home after the battle
        int reportedTick = 0;      // the client's last tick (SectorCommand reports, EndClientTurn)
        int reportedTickAtEnd = 0; // reportedTick when the battle ended
        std::int64_t lastRepairTime = 0;
        bool repairPending = false;        // a request arrived within the interval: answered when it passes
        std::optional<LogicGameMode> view; // PvP: the state as this client rebuilt it (opponent deck hidden)
        std::unique_ptr<BattleResultMessage> result;
        bool resultSent = false;
    };

    Sector(GameServer &server, const std::vector<std::int64_t> &accountIds);
    bool isBattleFinished() const { return mode_.getBattle().isFinished(); }
    Player *getPlayer(std::int64_t accountId);
    const Player *getPlayer(std::int64_t accountId) const;
    MessageManager *getOnlinePlayer(const Player &player) const; // null for the NPC, a player who left, or offline
    void repair(Player &player, MessageManager &online, int clientTick);
    void executeTurn();
    void finish();
    void applyResult(Player &player, library::Ptr winner);
    void recordReplay();
    std::string saveReplayAvatarJSON(library::Ptr avatar) const;

    GameServer &server_;
    std::vector<LogicClientHome> homes_; // the players' homes, their decks cloned from them
    LogicGameMode mode_;
    std::vector<Player> players_; // in addPlayer order
    bool npcBattle_ = false;
    bool friendly_ = false;
    int randomSeed_ = 0;
    int endTick_ = 0;                                       // the tick the battle ended at (0 while running)
    library::Ptr battleLog_ = 0;                            // LogicBattleLog, owned by the battle
    std::string replayBattleJSON_;                          // the replay's "battle" object
    std::vector<std::vector<std::uint8_t>> replayCommands_; // every command, as sent
    std::vector<std::string> replayEvents_;                 // emotes during the battle (LogicBattleEvent::saveJSON)
    int turn_ = 0;
    std::int64_t nextTurnTime_ = 0;
    bool finished_ = false;
    std::int64_t finishTime_ = 0;
    std::int64_t lastActiveTime_ = 0;           // the last time a player was online
    std::vector<LogicCommand> pendingCommands_; // received since the last turn
};
} // namespace server
