#include "logic/data/LogicData.h"

namespace server {
using library::call;
using library::Ptr;

namespace LogicData {
namespace {
// LogicNpcData: PredefinedDeck +0x70. LogicPredefinedDeckData: spells +0x68, levels +0x70.
constexpr int kNpcPredefinedDeckOffset = 0x70;
constexpr int kPredefinedDeckSpellsOffset = 0x68;
constexpr int kPredefinedDeckLevelsOffset = 0x70;
} // namespace

int getGlobalID(Ptr data) { return data ? int(call("__ZNK9LogicData11getGlobalIDEv", {data})) : 0; }
int getInstanceID(Ptr data) { return int(call("__ZNK9LogicData13getInstanceIDEv", {data})); }
std::string getName(Ptr data) {
    return data ? library::readString(call("__ZNK9LogicData7getNameEv", {data})) : std::string();
}

int getIntegerValue(Ptr data, const std::string &column) {
    if (!data) return 0;
    return int(call("__ZNK9LogicData15getIntegerValueERK6String", {data, library::newString(column)}));
}

// CSVRow::getValueAt: a const String&.
std::string getValue(Ptr data, const std::string &column) {
    return library::readString(call("__ZNK9LogicData8getValueERK6String", {data, library::newString(column)}));
}

Ptr getNpcLocation(Ptr npcData) { return call("__ZNK12LogicNpcData11getLocationEv", {npcData}); }
Ptr getNpcPredefinedDeck(Ptr npcData) { return npcData ? library::readPointer(npcData + kNpcPredefinedDeckOffset) : 0; }
Ptr getPredefinedDeckSpells(Ptr predefinedDeckData) {
    return predefinedDeckData ? library::readPointer(predefinedDeckData + kPredefinedDeckSpellsOffset) : 0;
}
Ptr getPredefinedDeckLevels(Ptr predefinedDeckData) {
    return predefinedDeckData ? library::readPointer(predefinedDeckData + kPredefinedDeckLevelsOffset) : 0;
}

std::string getTilemapFileName(Ptr locationData) {
    return library::readString(call("__ZNK17LogicLocationData18getTilemapFileNameEv", {locationData, 0}));
}

bool isTrainingCamp(Ptr arenaData) {
    return (call("__ZNK14LogicArenaData14isTrainingCampEv", {arenaData}) & 0xff) != 0;
}

int getDonateCapacity(Ptr spellData) {
    const auto rarity = spellData ? call("__ZNK14LogicSpellData9getRarityEv", {spellData}) : 0;
    return rarity ? int(call("__ZNK15LogicRarityData17getDonateCapacityEv", {rarity})) : 1;
}
} // namespace LogicData
} // namespace server
