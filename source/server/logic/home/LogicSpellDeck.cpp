#include "logic/home/LogicSpellDeck.h"

#include "titan/logic/json/LogicJSON.h"

namespace server {
using library::call;
using library::Ptr;

namespace {
constexpr std::size_t kSpellDeckSize = 0x48;
// LogicSpell (0x30 bytes): spell data +0, level index +8.
constexpr std::size_t kSpellSize = 0x30;
constexpr int kSpellLevelIndexOffset = 8;
} // namespace

namespace LogicSpellDeck {
Ptr create() {
    const auto deck = library::allocate(kSpellDeckSize);
    call("__ZN14LogicSpellDeckC1Ev", {deck});
    return deck;
}

Ptr clone(Ptr deck) { return call("__ZNK14LogicSpellDeck5cloneEv", {deck}); }
void clear(Ptr deck) { call("__ZN14LogicSpellDeck5clearEv", {deck}); }

void putSpell(Ptr deck, int slot, Ptr spellData, int levelIndex) {
    const auto spell = library::allocate(kSpellSize);
    call("__ZN10LogicSpellC1Ev", {spell});
    call("__ZN10LogicSpell12setSpellDataEPK14LogicSpellData", {spell, spellData});
    library::writeInt(spell + kSpellLevelIndexOffset, levelIndex);
    call("__ZN14LogicSpellDeck19putSpellInEmptySlotEiP10LogicSpell", {deck, std::uint64_t(std::uint32_t(slot)), spell});
}

Ptr getSpell(Ptr deck, int slot) {
    return call("__ZNK14LogicSpellDeck8getSpellEi", {deck, std::uint64_t(std::uint32_t(slot))});
}
std::string save(Ptr deck) { return LogicJSON::writeArray(call("__ZNK14LogicSpellDeck4saveEv", {deck})); }
} // namespace LogicSpellDeck

} // namespace server
