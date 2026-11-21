#pragma once
// A clan member on the clan's page, built as the library's own AllianceMemberEntry (operator new(96), its
// constructor) with the server's values put in its fields; AllianceMemberEntry::encode writes it.
#include "library/Library.h"
#include "logic/data/GlobalID.h"
#include "logic/data/LogicDataTables.h"

#include <cstdint>
#include <string>

namespace server {
struct AllianceMemberEntry {
    std::int64_t avatarId = 0;
    std::string name;
    int role = 1;
    int expLevel = 1;
    int score = 0;
    bool online = false;

    // A new library AllianceMemberEntry; its owner (the AllianceFullEntry's member list) frees it.
    library::Ptr createLibraryEntry() const {
        // AllianceMemberEntry (0x60 bytes): avatar id LogicLong* +0x10, home id LogicLong* +0x18 (written after a
        // Boolean when set), String* +0x20, name String* +0x28 (getName), role +0x30, exp level +0x34, score
        // +0x38, donations +0x3c, three VInts +0x40..+0x48, VInt +0x50, two Booleans +0x54 and +0x55 (online),
        // arena LogicArenaData* +0x58.
        constexpr std::size_t kSize = 0x60;
        constexpr int kAvatarIdOffset = 0x10;
        constexpr int kHomeIdOffset = 0x18;
        constexpr int kNameOffset = 0x28;
        constexpr int kRoleOffset = 0x30;
        constexpr int kExpLevelOffset = 0x34;
        constexpr int kScoreOffset = 0x38;
        constexpr int kOnlineOffset = 0x55;
        constexpr int kArenaOffset = 0x58;
        constexpr int kArenaInstance = 1;

        const auto entry = library::allocate(kSize);
        library::call("__ZN19AllianceMemberEntryC1Ev", {entry});
        library::writePointer(entry + kAvatarIdOffset, library::newLogicLong(avatarId));
        library::writePointer(entry + kHomeIdOffset, library::newLogicLong(avatarId));
        library::writePointer(entry + kNameOffset, library::newString(name));
        library::writeInt(entry + kRoleOffset, role);
        library::writeInt(entry + kExpLevelOffset, expLevel);
        library::writeInt(entry + kScoreOffset, score);
        library::writeByte(entry + kOnlineOffset, online ? 1 : 0);
        library::writePointer(entry + kArenaOffset, LogicDataTables::getDataById(GlobalID::createGlobalID(
                                                        LogicDataTables::kArenaClass, kArenaInstance)));
        return entry;
    }
};
} // namespace server
