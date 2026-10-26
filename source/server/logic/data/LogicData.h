#pragma once
// A row of the library's data tables (LogicData and its subclasses: arenas, NPCs, spells, chests, ...). A row is a
// library pointer; 0 is "no data".
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicData {
int getGlobalID(library::Ptr data); // 0 for no data
int getInstanceID(library::Ptr data);
std::string getName(library::Ptr data);
int getIntegerValue(library::Ptr data, const std::string &column);
std::string getValue(library::Ptr data, const std::string &column);

// Typed rows.
library::Ptr getNpcLocation(library::Ptr npcData);
library::Ptr getNpcPredefinedDeck(library::Ptr npcData);
library::Ptr getPredefinedDeckSpells(library::Ptr predefinedDeckData); // LogicArrayList<LogicSpellData*>
library::Ptr getPredefinedDeckLevels(library::Ptr predefinedDeckData); // LogicArrayList<int>
std::string getTilemapFileName(library::Ptr locationData);
bool isTrainingCamp(library::Ptr arenaData);
int getDonateCapacity(library::Ptr spellData); // its rarity's DonateCapacity, 1 without a rarity
} // namespace LogicData
} // namespace server
