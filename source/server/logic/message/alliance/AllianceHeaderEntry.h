#pragma once
// A clan as lists show it (search results, joinable clans, the top of a clan's page), built as the library's own
// AllianceHeaderEntry (operator new(80), its constructor) with the server's values put in its fields;
// AllianceHeaderEntry::encode writes it.
#include "library/Library.h"
#include "logic/data/LogicDataTables.h"

#include <cstdint>
#include <string>

namespace server {
struct AllianceHeaderEntry {
    std::int64_t id = 0;
    std::string name;
    int badge = 0;
    int type = 1;
    int memberCount = 0;
    int score = 0;
    int requiredScore = 0;
    int region = 0;

    // A new library AllianceHeaderEntry; its owner (a message's list, or an AllianceFullEntry) frees it.
    library::Ptr createLibraryEntry() const {
        // AllianceHeaderEntry (0x50 bytes): id LogicLong* +0x00, name String* +0x08, badge LogicAllianceBadgeData*
        // +0x10, type +0x18, member count +0x1c, score +0x20, required score +0x24, six VInts +0x28..+0x3c
        // (donations +0x38), region LogicRegionData* +0x40, locale LogicLocaleData* +0x48.
        constexpr std::size_t kSize = 0x50;
        constexpr int kIdOffset = 0x00;
        constexpr int kNameOffset = 0x08;
        constexpr int kBadgeOffset = 0x10;
        constexpr int kTypeOffset = 0x18;
        constexpr int kMemberCountOffset = 0x1c;
        constexpr int kScoreOffset = 0x20;
        constexpr int kRequiredScoreOffset = 0x24;
        constexpr int kRegionOffset = 0x40;

        const auto entry = library::allocate(kSize);
        library::call("__ZN19AllianceHeaderEntryC1Ev", {entry});
        library::writePointer(entry + kIdOffset, library::newLogicLong(id));
        library::writePointer(entry + kNameOffset, library::newString(name));
        library::writePointer(entry + kBadgeOffset, LogicDataTables::getDataById(badge));
        library::writeInt(entry + kTypeOffset, type);
        library::writeInt(entry + kMemberCountOffset, memberCount);
        library::writeInt(entry + kScoreOffset, score);
        library::writeInt(entry + kRequiredScoreOffset, requiredScore);
        library::writePointer(entry + kRegionOffset, LogicDataTables::getDataById(region));
        return entry;
    }
};
} // namespace server
