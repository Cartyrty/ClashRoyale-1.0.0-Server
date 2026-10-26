#pragma once
// The library's LogicSummoner: a battle player's leader character (their deck, the crowns they took).
#include "library/Library.h"

namespace server {
namespace LogicSummoner {
library::Ptr getDeck(library::Ptr summoner);                // LogicSpellDeck*
int getStars(library::Ptr summoner, library::Ptr opponent); // crowns taken from the opponent
} // namespace LogicSummoner
} // namespace server
