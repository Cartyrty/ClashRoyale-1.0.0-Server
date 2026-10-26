#pragma once
// The library's LogicBattleLog: LogicBattle::addPlayer, setSpellDecks and battleEnded fill it (players, decks, crowns,
// scores); its JSON is the battle log entry of both players.
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicBattleLog {
library::Ptr create(library::Ptr arenaData);
std::string getJSON(library::Ptr battleLog);
} // namespace LogicBattleLog
} // namespace server
