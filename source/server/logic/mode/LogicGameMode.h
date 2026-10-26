#pragma once
// The library's LogicGameMode: the simulation the client runs, for a home (commands, offline progress) or a battle
// (a LogicBattle with two players). The server runs the same one, so ticks and checksums match the client's.
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/battle/LogicBattle.h"
#include "logic/command/LogicCommand.h"
#include "logic/home/LogicClientHome.h"

#include <cstdint>

namespace server {
class ByteStream;

class LogicGameMode {
public:
    explicit LogicGameMode(bool battle);
    LogicGameMode(LogicGameMode &&other) noexcept;
    LogicGameMode &operator=(LogicGameMode &&other) noexcept;
    LogicGameMode(const LogicGameMode &) = delete;
    LogicGameMode &operator=(const LogicGameMode &) = delete;
    ~LogicGameMode();

    // Takes the home and the avatar; the seconds since the save are simulated as offline progress.
    void loadHomeState(LogicClientHome home, LogicClientAvatar avatar, int secondsSinceSave, int randomSeed);
    LogicClientHome getHome() const;           // a view
    LogicClientAvatar getPlayerAvatar() const; // a view

    int getTick() const;
    void updateOneTick();
    int calculateChecksum() const;
    void addCommand(LogicCommand command); // the command manager takes it

    // Battles.
    LogicBattle getBattle() const;
    void setLogicChangesAllowed(bool allowed);
    void setRandomSeed(int seed);
    void addPlayer(LogicClientAvatar avatar);                        // takes the avatar
    library::Ptr getAvatarByAccountId(std::int64_t accountId) const; // 0 when not a player
    // The full state as SectorStateMessage carries it (LogicGameMode::encode(stream, true, viewer)), and back.
    void encode(ByteStream &stream, library::Ptr viewerAvatar) const;
    void decode(ByteStream &stream);

    library::Ptr get() const { return mode_; }

private:
    void destroy();

    library::Ptr mode_ = 0;
};
} // namespace server
