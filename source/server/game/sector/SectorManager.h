#pragma once
// Every running battle, and matchmaking. A player who disconnects keeps their battle: logging in again returns them to
// it (SectorState after LoginOk).
//   StartMission (14104)       an NPC battle
//   SectorState (21903)        a battle's full state
//   SectorHeartbeat (21902)    PvP, every 10-tick turn (500 ms)
//   SectorCommand (12904)      a PvP player's command (a card placement) or tick report
//   EndClientTurn (14102)      NPC battles: the server follows the client's turns
//   RequestSectorState (12903) a client that went out of sync gets the authoritative state again (rate-limited)
//   BattleResult (20225)       PvP end screen, sent once the client reaches the end
#include "game/sector/Sector.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace server {
class EndClientTurnMessage;
class GameServer;
class MessageManager;
class RequestSectorStateMessage;
class SectorCommandMessage;
class SendBattleEventMessage;
class StartMissionMessage;

class SectorManager {
public:
    explicit SectorManager(GameServer &server);
    ~SectorManager();

    void onStartMission(MessageManager &player, const StartMissionMessage &message);
    void onCancelMatchmake(MessageManager &player);
    void onSectorCommand(MessageManager &player, SectorCommandMessage &message);
    void onRequestSectorState(MessageManager &player, const RequestSectorStateMessage &message);
    void onSendBattleEvent(MessageManager &player, SendBattleEventMessage &message);
    // A turn from a battle; false when the player is not in a running one.
    bool onEndClientTurn(MessageManager &player, EndClientTurnMessage &message);

    // LogicStartMatchmakeCommand ran on the home: the player waits for an opponent.
    void startMatchmaking(MessageManager &player, bool useDiamonds);
    // A friendly battle (a clan challenge accepted): PvP without trophies, chests or stats.
    bool startFriendlyBattle(std::int64_t challengerId, std::int64_t accepterId);

    void leave(MessageManager &player);          // the player goes home (GoHome) or starts something else
    void onDisconnected(MessageManager &player); // the battle waits for the player
    bool rejoin(MessageManager &player);         // after login: back into a running battle
    bool isInBattle(std::int64_t accountId) const;
    void update();

private:
    struct MatchmakingEntry {
        std::int64_t accountId;
        std::int64_t since;
    };

    Sector *getSectorOf(std::int64_t accountId); // the battle the player is still in, finished or not
    Sector *startPvpBattle(std::int64_t accountId1, std::int64_t accountId2, bool friendly);
    bool removeFromQueue(std::int64_t accountId);
    void destroy(Sector &sector);

    GameServer &server_;
    std::vector<std::unique_ptr<Sector>> sectors_;
    std::deque<MatchmakingEntry> matchmakingQueue_;
};
} // namespace server
