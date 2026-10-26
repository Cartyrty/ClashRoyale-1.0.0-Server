#pragma once
// The library's LogicSpell: a card in a deck (its LogicSpellData and level).
#include "library/Library.h"

namespace server {
namespace LogicSpell {
library::Ptr clone(library::Ptr spell);
} // namespace LogicSpell
} // namespace server
