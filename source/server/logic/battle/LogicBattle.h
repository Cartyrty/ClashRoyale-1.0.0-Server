#pragma once
// The library's LogicBattle: the battle a battle LogicGameMode runs (a view of the game mode's object).
#include "library/Library.h"

namespace server {
class LogicBattle {
public:
    // LogicBattle::setType values.
    static constexpr int kTypePvp = 0;
    static constexpr int kTypeNpc = 1;

    explicit LogicBattle(library::Ptr battle) : battle_(battle) {}

    void setType(int type);
    void setLocation(library::Ptr locationData);
    void setNpc(library::Ptr npcData);
    void setArena(library::Ptr arenaData);
    library::Ptr getArena() const;
    void setBattleLog(library::Ptr battleLog);                  // the battle fills it (players, decks, crowns, scores)
    void setSpellDecks(library::Ptr deck0, library::Ptr deck1); // takes the decks
    bool isFinished() const;
    library::Ptr getWinnerAvatar() const;                       // only once finished; 0 for a draw
    library::Ptr getLeaderCharacter(library::Ptr avatar) const; // LogicSummoner*
    int getExpReward() const;

    library::Ptr get() const { return battle_; }
    explicit operator bool() const { return battle_ != 0; }

private:
    library::Ptr battle_;
};
} // namespace server
