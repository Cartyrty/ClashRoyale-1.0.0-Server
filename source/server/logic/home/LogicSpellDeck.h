#pragma once
// The library's LogicSpellDeck (eight LogicSpell slots: a card and its level).
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicSpellDeck {
library::Ptr create();
library::Ptr clone(library::Ptr deck);
void clear(library::Ptr deck);
// A new LogicSpell of the card at level index levelIndex (0 = level 1) in the slot.
void putSpell(library::Ptr deck, int slot, library::Ptr spellData, int levelIndex);
library::Ptr getSpell(library::Ptr deck, int slot); // LogicSpell*, 0 when empty
std::string save(library::Ptr deck);                // LogicSpellDeck::save as JSON text
} // namespace LogicSpellDeck

} // namespace server
