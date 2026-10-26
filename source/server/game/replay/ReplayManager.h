#pragma once
// Battle logs and replays (the battle_reports and replays tables of server.db). Each finished PvP battle leaves, for
// both players, a battle report and a replay - the JSON that LogicGameMode::loadReplay reads: {"rndSeed", "battle":
// {location, arena, NPC, deck0/1 (LogicSpellDeck::save), avatar0/1 (LogicClientAvatar::loadForReplay keys)}, "cmd":
// [LogicCommandManager::saveCommandToJSON], "evt": [LogicBattleEvent::saveJSON]}.
//   AskForBattleReplayStreamMessage (14406) -> BattleReportStreamMessage (24413)
//   HomeBattleReplayMessage (14114)         -> HomeBattleReplayDataMessage (24114): the replay in MySQL zlib format
#include <cstdint>
#include <string>
#include <vector>

namespace server {
class AskForBattleReplayStreamMessage;
class GameServer;
class HomeBattleReplayMessage;
class MessageManager;

class ReplayManager {
public:
    struct ReplayPlayer {
        std::int64_t accountId;
        std::string name;
        int expLevel;
    };

    explicit ReplayManager(GameServer &server);

    // Stores a finished battle; every human player gets a battle report.
    void recordBattle(const std::vector<ReplayPlayer> &players, const std::string &battleLogJSON,
                      const std::string &replayJSON, int arenaGlobalId);

    void onAskForBattleReplayStream(MessageManager &player, const AskForBattleReplayStreamMessage &message);
    void onHomeBattleReplay(MessageManager &player, const HomeBattleReplayMessage &message);

private:
    void importJsonState(); // battlelog.json and replays/*.json from before server.db

    GameServer &server_;
};
} // namespace server
