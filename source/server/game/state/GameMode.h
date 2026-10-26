#pragma once
// A player's home, running in the library's LogicGameMode exactly as on the client (the client's GameMode does the
// same on its side): the player's EndClientTurn commands execute at their ticks, the checksums are compared, and the
// encoded home and avatar are saved after every turn. Server commands go to the client and run here when the client
// echoes them back.
#include "logic/command/LogicCommand.h"
#include "logic/mode/LogicGameMode.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace server {
class EndClientTurnMessage;
class GameServer;
class MessageManager;

class GameMode {
public:
    // Offline time is applied to the saved home first (loadHomeState with the seconds since the save); then both sides
    // load that state with no time passed and the same random seed (OwnHomeDataMessage).
    GameMode(MessageManager &player, GameServer &server);
    GameMode(const GameMode &) = delete;
    GameMode &operator=(const GameMode &) = delete;

    void endClientTurn(EndClientTurnMessage &message);
    void save();
    // AvailableServerCommandMessage now; executed here when the client echoes it back in its next EndClientTurn.
    void sendServerCommand(LogicCommand command);
    // A new daily shop cycle reaches a player in their home (LogicShopSeedChangedCommand).
    void checkShop();

    LogicClientHome getHome() const { return mode_.getHome(); }
    LogicClientAvatar getAvatar() const { return mode_.getPlayerAvatar(); }

    // A server command for a saved account (the player is offline or in a battle).
    static void applyServerCommandOffline(GameServer &server, std::int64_t accountId, LogicCommand command);

private:
    void sendRewardClaims();

    MessageManager &player_;
    GameServer &server_;
    LogicGameMode mode_;
    std::deque<std::vector<std::uint8_t>> sentServerCommands_; // their encodings, in the order they were sent
    int shopSeedSent_ = -1;
};
} // namespace server
