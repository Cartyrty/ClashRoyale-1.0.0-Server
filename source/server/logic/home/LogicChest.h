#pragma once
// The library's LogicChest: a chest in a home's slot.
#include "library/Library.h"

namespace server {
namespace LogicChest {
// LogicChest::setSource values.
constexpr int kSourceBattleWin = 1;

library::Ptr create(library::Ptr treasureChestData, int source);
library::Ptr getTreasureChestData(library::Ptr chest);
} // namespace LogicChest
} // namespace server
