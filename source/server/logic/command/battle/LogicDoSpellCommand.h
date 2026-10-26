#pragma once
// A card placed in a battle. The client sends its deck index; the card itself (LogicSpell*) is filled in by the server
// for the opponent, whose client has the deck hidden.
#include "logic/command/LogicCommand.h"

namespace server {
namespace LogicDoSpellCommand {
constexpr int kCommandType = 1;
// LogicDoSpellCommand: deck index +0x1c, LogicSpell* +0x28 (encoded after a "has spell" Boolean).
constexpr int kSpellDeckIndexOffset = 0x1c;
constexpr int kSpellOffset = 0x28;

inline int getSpellDeckIndex(const LogicCommand &command) {
    return library::readInt(command.get() + kSpellDeckIndexOffset);
}
inline bool hasSpell(const LogicCommand &command) { return library::readPointer(command.get() + kSpellOffset) != 0; }
inline void setSpell(LogicCommand &command, library::Ptr spell) {
    library::writePointer(command.get() + kSpellOffset, spell);
}
} // namespace LogicDoSpellCommand
} // namespace server
